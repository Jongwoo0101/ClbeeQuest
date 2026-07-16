#ifndef TUK_PROCESS_H
#define TUK_PROCESS_H

#include <time.h>

#include "platform.h"
#include "tuk_shell.h" /* MAX_INPUT_LENGTH: 원본 명령 문자열 보관 용량 */

/*
 * process.h - ProcessInfo 구조체 및 연결 리스트 관리 [공통]
 *
 * 근거 문서:
 *  - 01_상세기능명세서 3장 (구조체 필드, 상태값, 메모리 관리 규칙)
 *  - 02_인터페이스명세서 4-3, 4-4 (함수 시그니처 고정)
 *  - 03_파이프라인명세서 5장 (상태 갱신 파이프라인)
 *
 * 외부 명령 실행은 executor.h와 platform.h로 분리했다.
 */

#define PROCESS_NAME_MAX 64

/* 01 3-2: 백그라운드 작업 상태값 */
typedef enum {
    RUNNING,
    STOPPED,
    DONE
} ProcessStatus;

/* 01 3-1: ProcessInfo 구조체 필드 명세 */
typedef struct ProcessInfo {
    tuk_pid_t pid;                   /* 백그라운드 자식 프로세스 ID */
    tuk_process_handle_t handle;     /* wait/stat용 OS 핸들 */
    char name[PROCESS_NAME_MAX];     /* 실행 파일명(argv[0]) */
    char command[MAX_INPUT_LENGTH];  /* 사용자가 입력한 원본 명령 문자열 */
    time_t start_time;               /* 부모 쉘이 작업 등록한 시각 */
    double cpu_usage;                /* 최신 CPU 사용률(%) */
    long mem_usage_kb;               /* 최신 메모리 사용량(KB) */
    ProcessStatus status;            /* RUNNING / STOPPED / DONE */
    int exit_code;                   /* 종료 코드, 실행 중이면 -1 */
    struct ProcessInfo *next;        /* 단일 연결 리스트 포인터 */
} ProcessInfo;

/* 03 2-1: 쉘 런타임 상태 - 백그라운드 작업 리스트 헤드 (초기값 NULL) */
extern ProcessInfo *g_process_list;

/* 02 4-3: 프로세스 리스트 관리 인터페이스 (시그니처 고정) */
ProcessInfo *create_process_node(tuk_pid_t pid, tuk_process_handle_t handle,
                                 char **argv,
                                 const char *raw_command);
int append_process(ProcessInfo **head, ProcessInfo *node);
int remove_process(ProcessInfo **head, tuk_pid_t pid);
ProcessInfo *find_process_by_pid(ProcessInfo *head, tuk_pid_t pid);

/* 02 4-4: 시스템 정보 갱신 - 플랫폼별 non-blocking 폴링 및 통계 갱신 */
int refresh_all_processes(ProcessInfo **head);

/* 03 9장: exit/EOF 시 남은 노드 전체 해제 */
void free_all_processes(ProcessInfo **head);

/* 상태 enum -> 출력용 문자열 ("RUNNING" 등) */
const char *process_status_string(ProcessStatus status);

/* 리스트 노드 개수 (top 출력용 배열 크기 산정) */
int process_count(ProcessInfo *head);

#endif /* TUK_PROCESS_H */
