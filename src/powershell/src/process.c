/*
 * process.c - ProcessInfo 연결 리스트 관리 [공통]
 *
 * 근거 문서:
 *  - 01_상세기능명세서 3장 (구조체/리스트/메모리 규칙)
 *  - 03_파이프라인명세서 5장 (상태 갱신), 9장 (종료 정리)
 *  - 05_개발철칙명세서 2-1 (free 시점 주석 필수, 종료 감지 즉시 해제)
 *
 * fork/execvp 기반 외부 명령 실행은 executor.c로 분리했다.
 */
#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system_info.h"
#include "tuk_shell.h"

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

ProcessInfo *create_process_node(tuk_pid_t pid, tuk_process_handle_t handle,
                                 char **argv,
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
    node->handle = handle;
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

int remove_process(ProcessInfo **head, tuk_pid_t pid)
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
            tuk_close_process_handle(cur->handle);
            /* free 시점: 리스트에서 분리 직후 즉시 해제 (01 3-3, 05 2-1) */
            free(cur);
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    return -1;
}

ProcessInfo *find_process_by_pid(ProcessInfo *head, tuk_pid_t pid)
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
        TukWaitState wait_state = TUK_WAIT_RUNNING;
        int exit_code = -1;
        if (tuk_wait_process(cur->pid, cur->handle, 1, &wait_state,
                             &exit_code) != 0) {
            perror("process wait"); /* 03 10장: 회수 실패 시 오류 출력 후 계속 */
        } else if (wait_state == TUK_WAIT_RUNNING) {
            /* 01 3-2: 미종료 - 통계만 갱신. 실패 시 0.0/0, 상태는 유지 (01 3-4) */
            if (update_process_stats(cur) != 0) {
                cur->cpu_usage = 0.0;
                cur->mem_usage_kb = 0;
            }
        } else if (wait_state == TUK_WAIT_EXITED) {
            /* 03 5-4: RUNNING -> DONE, 종료 메시지 출력 후 정리 */
            cur->status = DONE;
            cur->exit_code = exit_code;
            printf(TUK_COLOR_SKY "[done] pid=%ld exit=%d command=\"%s\""
                   TUK_COLOR_RESET "\n",
                   (long)cur->pid, cur->exit_code, cur->command);
            /* 05 2-1: 종료 감지 즉시 리스트 분리 + free */
            remove_process(head, cur->pid);
        } else if (wait_state == TUK_WAIT_STOPPED) {
            cur->status = STOPPED;
        } else if (wait_state == TUK_WAIT_CONTINUED) {
            cur->status = RUNNING;
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
        tuk_close_process_handle(cur->handle);
        /* free 시점: exit/EOF 종료 파이프라인에서 전체 해제 (03 9장, 05 2-1) */
        free(cur);
        cur = next;
    }
    *head = NULL;
}
