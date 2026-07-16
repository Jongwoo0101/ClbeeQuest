#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>
#include <time.h>
#include "tuk_shell.h"

#define PROC_NAME_MAX 64
#define PROC_COMMAND_MAX MAX_INPUT_LENGTH

/* 01_상세기능명세서.md 3-2 상태값 규칙 */
typedef enum {
    PROC_RUNNING, /* 백그라운드 실행 중, waitpid(WNOHANG) 미종료 */
    PROC_STOPPED, /* 시그널 등으로 일시 정지 */
    PROC_DONE     /* 종료 확인, 정리 직전 */
} ProcessStatus;

/* 01_상세기능명세서.md 3-1 필드 명세 */
typedef struct ProcessInfo {
    pid_t pid;
    char name[PROC_NAME_MAX];
    char command[PROC_COMMAND_MAX];
    time_t start_time;
    double cpu_usage;
    long mem_usage_kb;
    ProcessStatus status;
    int exit_code; /* 실행 중이면 -1 */
    struct ProcessInfo *next;
} ProcessInfo;

/*
 * create_process_node
 *  - malloc(sizeof(ProcessInfo))로 노드 생성.
 *  - 실패 시 NULL 반환하고 호출부는 리스트에 넣지 않는다 (01문서 3-3).
 */
ProcessInfo *create_process_node(pid_t pid, char **argv, const char *raw_command);

/* 리스트 끝에 노드 추가. 성공 0, 실패 -1 */
int append_process(ProcessInfo **head, ProcessInfo *node);

/*
 * remove_process
 *  - pid에 해당하는 노드를 리스트에서 분리한 뒤 즉시 free() (01문서 3-3, 05문서 2-1).
 *  - 성공 0, 대상 없음 -1.
 */
int remove_process(ProcessInfo **head, pid_t pid);

/* pid로 노드 검색 (리스트 순회, 노드 소유권은 그대로 리스트에 있음) */
ProcessInfo *find_process_by_pid(ProcessInfo *head, pid_t pid);

/*
 * refresh_all_processes
 *  - 03_파이프라인명세서.md 5장 순서를 그대로 구현:
 *    waitpid(WNOHANG) 종료 확인 -> 미종료면 update_process_stats() 갱신
 *    -> 종료 확인되면 [done] 메시지 출력 후 노드 제거/free.
 *  - 항상 0을 반환한다(리스트 자체가 없을 때만 -1).
 */
int refresh_all_processes(ProcessInfo **head);

/* exit/EOF 시 남은 모든 노드를 순회하며 free (01문서 3-3, 05문서 2-1) */
void free_process_list(ProcessInfo **head);

/* 상태 enum -> 문자열 (이후 jobs/top 출력에서 재사용) */
const char *process_status_to_string(ProcessStatus status);

#endif /* PROCESS_H */
