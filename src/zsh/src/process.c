#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "process.h"
#include "system_info.h"
#include "tuk_shell.h"

ProcessInfo *create_process_node(pid_t pid, char **argv, const char *raw_command)
{
    ProcessInfo *node;

    /*
     * [malloc 지점 1] 노드 생성.
     * - 해제 시점: refresh_all_processes()에서 종료가 감지되어 remove_process()가
     *   호출될 때, 또는 쉘 종료 시 free_process_list()가 호출될 때 free()된다.
     */
    node = (ProcessInfo *)malloc(sizeof(ProcessInfo));
    if (node == NULL) {
        perror("malloc"); /* 05문서 3-1 */
        return NULL;      /* 등록 실패: 리스트에 넣지 않고 오류만 출력 (01문서 3-3) */
    }

    node->pid = pid;

    if (argv != NULL && argv[0] != NULL) {
        strncpy(node->name, argv[0], PROC_NAME_MAX - 1);
        node->name[PROC_NAME_MAX - 1] = '\0';
    } else {
        node->name[0] = '\0';
    }

    if (raw_command != NULL) {
        strncpy(node->command, raw_command, PROC_COMMAND_MAX - 1);
        node->command[PROC_COMMAND_MAX - 1] = '\0';
    } else {
        node->command[0] = '\0';
    }

    node->start_time = time(NULL);
    node->cpu_usage = 0.0;
    node->mem_usage_kb = 0;
    node->status = PROC_RUNNING;
    node->exit_code = -1; /* 실행 중 표시 (01문서 3-1) */
    node->next = NULL;

    return node;
}

int append_process(ProcessInfo **head, ProcessInfo *node)
{
    ProcessInfo *cur;

    if (head == NULL || node == NULL) {
        return -1;
    }

    if (*head == NULL) {
        *head = node;
        return 0;
    }

    cur = *head;
    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = node;

    return 0;
}

int remove_process(ProcessInfo **head, pid_t pid)
{
    ProcessInfo *cur;
    ProcessInfo *prev;

    if (head == NULL) {
        return -1;
    }

    cur = *head;
    prev = NULL;

    while (cur != NULL) {
        if (cur->pid == pid) {
            if (prev == NULL) {
                *head = cur->next;
            } else {
                prev->next = cur->next;
            }
            /*
             * [free 지점 1] 리스트에서 분리한 직후 즉시 해제.
             * - 종료가 확인된 백그라운드 작업의 정상적인 회수 경로.
             */
            free(cur);
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }

    return -1; /* 대상 PID 없음 */
}

ProcessInfo *find_process_by_pid(ProcessInfo *head, pid_t pid)
{
    ProcessInfo *cur = head;

    while (cur != NULL) {
        if (cur->pid == pid) {
            return cur;
        }
        cur = cur->next;
    }

    return NULL;
}

const char *process_status_to_string(ProcessStatus status)
{
    switch (status) {
        case PROC_RUNNING:
            return "RUNNING";
        case PROC_STOPPED:
            return "STOPPED";
        case PROC_DONE:
            return "DONE";
        default:
            return "UNKNOWN";
    }
}

int refresh_all_processes(ProcessInfo **head)
{
    ProcessInfo *cur;
    ProcessInfo *next_node;
    int wstatus;
    pid_t result;

    if (head == NULL) {
        return -1;
    }

    cur = *head;

    while (cur != NULL) {
        /* 노드가 이번 반복에서 제거될 수 있으므로 다음 포인터를 미리 저장 */
        next_node = cur->next;

        result = waitpid(cur->pid, &wstatus, WNOHANG);

        if (result == 0) {
            /* 아직 종료되지 않음: 통계 갱신 시도 (03문서 5-2 [2]) */
            if (update_process_stats(cur) != 0) {
                /* 실패 시 규칙: cpu/mem 0으로, status는 기존 값 유지 (01문서 3-4) */
                cur->cpu_usage = 0.0;
                cur->mem_usage_kb = 0;
            }
        } else if (result == cur->pid) {
            /* 종료 확인됨 (03문서 5-2 [4], 5-4 상태 전이) */
            if (WIFEXITED(wstatus)) {
                cur->exit_code = WEXITSTATUS(wstatus);
            } else if (WIFSIGNALED(wstatus)) {
                cur->exit_code = 128 + WTERMSIG(wstatus);
            } else {
                cur->exit_code = -1;
            }
            cur->status = PROC_DONE;

            /* 종료 메시지 형식 - 03문서 5-3 */
            printf(TU_SKY_BLUE "[done] pid=%d exit=%d command=\"%s\"\n" COLOR_RESET,
                   (int)cur->pid, cur->exit_code, cur->command);

            /* 리스트 제거 + free는 remove_process 내부에서 수행 */
            remove_process(head, cur->pid);
        } else {
            /*
             * result < 0 (예: ECHILD): 이미 다른 경로로 회수되었거나
             * 자식이 아닌 경우. 쉘 전체를 중단시키지 않고 다음 노드로 진행한다.
             */
        }

        cur = next_node;
    }

    return 0;
}

void free_process_list(ProcessInfo **head)
{
    ProcessInfo *cur;
    ProcessInfo *tmp;

    if (head == NULL) {
        return;
    }

    cur = *head;
    while (cur != NULL) {
        tmp = cur;
        cur = cur->next;
        /*
         * [free 지점 2] 쉘 종료(exit/EOF) 시 잔여 노드 전체 해제.
         * - 정상적으로 종료를 감지하지 못한 채 쉘이 끝나는 경우의 최종 안전망.
         */
        free(tmp);
    }
    *head = NULL;
}
