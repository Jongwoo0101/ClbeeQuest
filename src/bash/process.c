/*
 * process.c - ProcessInfo 연결 리스트 관리 및 fork/execvp 실행 [공통]
 *
 * 근거 문서:
 *  - 01_상세기능명세서 3장 (구조체/리스트/메모리 규칙)
 *  - 03_파이프라인명세서 4장 (명령 실행), 5장 (상태 갱신), 9장 (종료 정리)
 *  - 05_개발철칙명세서 2-1 (free 시점 주석 필수, 종료 감지 즉시 해제)
 */
#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "system_info.h"

/* 03 2-1 [2]: ProcessInfo 리스트 헤드는 NULL로 초기화 */
ProcessInfo *g_process_list = NULL;

const char *process_status_string(ProcessStatus status)
{
    switch (status) {
    case RUNNING:
        return "RUNNING";
    case STOPPED:
        return "STOPPED";
    case DONE:
        return "DONE";
    default:
        return "UNKNOWN";
    }
}

int process_count(ProcessInfo *head)
{
    int count = 0;
    for (ProcessInfo *cur = head; cur != NULL; cur = cur->next) {
        count++;
    }
    return count;
}

/*
 * 03 4-4/5-3의 메시지 예시(command="sleep 30")에 맞춰, 보관용 명령 문자열은
 * 후행 공백과 백그라운드 지시자 '&'를 제거한 형태로 정규화한다.
 * (히스토리에는 main.c에서 원본 그대로 저장되므로 손실 없음)
 */
static void copy_normalized_command(char *dest, size_t dest_size,
                                    const char *raw_command)
{
    snprintf(dest, dest_size, "%s", raw_command == NULL ? "" : raw_command);
    size_t len = strlen(dest);
    while (len > 0 && (dest[len - 1] == ' ' || dest[len - 1] == '\t')) {
        dest[--len] = '\0';
    }
    if (len > 0 && dest[len - 1] == '&') {
        dest[--len] = '\0';
        while (len > 0 && (dest[len - 1] == ' ' || dest[len - 1] == '\t')) {
            dest[--len] = '\0';
        }
    }
}

ProcessInfo *create_process_node(pid_t pid, char **argv,
                                 const char *raw_command)
{
    /* free 시점: 종료 감지 시 refresh_all_processes() -> remove_process()에서
     * 즉시 해제되며, 쉘 종료 시 남은 노드는 free_all_processes()에서 해제된다.
     * (05 2-1) */
    ProcessInfo *node = malloc(sizeof(ProcessInfo));
    if (node == NULL) {
        perror("malloc"); /* 01 3-3: 등록 실패 시 리스트에 넣지 않고 오류만 */
        return NULL;
    }

    node->pid = pid;
    snprintf(node->name, sizeof(node->name), "%s",
             (argv != NULL && argv[0] != NULL) ? argv[0] : "");
    copy_normalized_command(node->command, sizeof(node->command), raw_command);
    node->start_time = time(NULL);
    if (node->start_time == (time_t)-1) {
        perror("time");
    }
    node->cpu_usage = 0.0;
    node->mem_usage_kb = 0;
    node->status = RUNNING;
    node->exit_code = -1; /* 01 3-1: 실행 중이면 -1 */
    node->next = NULL;
    return node;
}

int append_process(ProcessInfo **head, ProcessInfo *node)
{
    if (head == NULL || node == NULL) {
        return -1;
    }
    if (*head == NULL) {
        *head = node;
        return 0;
    }
    ProcessInfo *cur = *head;
    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = node;
    return 0;
}

int remove_process(ProcessInfo **head, pid_t pid)
{
    if (head == NULL) {
        return -1;
    }
    ProcessInfo *cur = *head;
    ProcessInfo *prev = NULL;
    while (cur != NULL) {
        if (cur->pid == pid) {
            if (prev == NULL) {
                *head = cur->next;
            } else {
                prev->next = cur->next;
            }
            /* free 시점: 리스트에서 분리 직후 즉시 해제 (01 3-3, 05 2-1) */
            free(cur);
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    return -1;
}

ProcessInfo *find_process_by_pid(ProcessInfo *head, pid_t pid)
{
    for (ProcessInfo *cur = head; cur != NULL; cur = cur->next) {
        if (cur->pid == pid) {
            return cur;
        }
    }
    return NULL;
}

int refresh_all_processes(ProcessInfo **head)
{
    if (head == NULL) {
        return -1;
    }
    ProcessInfo *cur = *head;
    while (cur != NULL) {
        ProcessInfo *next = cur->next; /* 노드 제거 대비 다음 포인터 선보관 */
        int status = 0;
        pid_t result = waitpid(cur->pid, &status,
                               WNOHANG | WUNTRACED | WCONTINUED);
        if (result < 0) {
            perror("waitpid"); /* 03 10장: 회수 실패 시 오류 출력 후 계속 */
        } else if (result == 0) {
            /* 01 3-2: 미종료 - 통계만 갱신. 실패 시 0.0/0, 상태는 유지 (01 3-4) */
            if (update_process_stats(cur) != 0) {
                cur->cpu_usage = 0.0;
                cur->mem_usage_kb = 0;
            }
        } else {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                /* 03 5-4: RUNNING -> DONE, 종료 메시지 출력 후 정리 */
                cur->status = DONE;
                cur->exit_code = WIFEXITED(status)
                                     ? WEXITSTATUS(status)
                                     : 128 + WTERMSIG(status);
                printf(TUK_COLOR_SKY "[done] pid=%ld exit=%d command=\"%s\""
                       TUK_COLOR_RESET "\n",
                       (long)cur->pid, cur->exit_code, cur->command);
                /* 05 2-1: 종료 감지 즉시 리스트 분리 + free */
                remove_process(head, cur->pid);
            } else if (WIFSTOPPED(status)) {
                cur->status = STOPPED; /* 03 5-4: RUNNING -> STOPPED */
            } else if (WIFCONTINUED(status)) {
                cur->status = RUNNING; /* 03 5-4: STOPPED -> RUNNING */
            }
        }
        cur = next;
    }
    return 0;
}

void free_all_processes(ProcessInfo **head)
{
    if (head == NULL) {
        return;
    }
    ProcessInfo *cur = *head;
    while (cur != NULL) {
        ProcessInfo *next = cur->next;
        /* free 시점: exit/EOF 종료 파이프라인에서 전체 해제 (03 9장, 05 2-1) */
        free(cur);
        cur = next;
    }
    *head = NULL;
}

int run_external_command(char **argv, int background_flag,
                         const char *raw_command)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork"); /* 03 10장: 자식 생성 실패 - 오류 출력 후 루프 복귀 */
        return -1;
    }

    if (pid == 0) {
        /* 자식 프로세스: 프로그램 교체. 실패 시 부모 자원(리스트/히스토리)을
         * 건드리지 않고 즉시 종료해 부모가 상태를 회수하게 한다. (05 3-2) */
        execvp(argv[0], argv);
        perror(argv[0]);
        exit(1);
    }

    if (!background_flag) {
        /* 03 4-3 포그라운드: 자식 종료 코드 수집 후 REPL 복귀 */
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
        }
        return 0;
    }

    /* 03 4-3 백그라운드: ProcessInfo 생성 -> 리스트 등록 -> 즉시 복귀 */
    ProcessInfo *node = create_process_node(pid, argv, raw_command);
    if (node == NULL) {
        return -1; /* 등록 실패: 자식은 실행 중이나 추적 목록에서만 제외 */
    }
    if (append_process(&g_process_list, node) != 0) {
        /* free 시점: 등록 실패 시 보관처가 없으므로 즉시 해제 */
        free(node);
        return -1;
    }
    /* 03 4-4: 시작 메시지 - TU SKY BLUE, 단색 터미널 대비 [bg] 표기 병행 */
    printf(TUK_COLOR_SKY "[bg] pid=%ld command=\"%s\"" TUK_COLOR_RESET "\n",
           (long)pid, node->command);
    return 0;
}
