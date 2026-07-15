#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "commands.h"
#include "process.h"
#include "tuk_shell.h"

/* 3단계 기준 내장 명령어 목록. top은 4단계에서 추가된다. */
static const char *BUILTIN_NAMES[] = { "cd", "pwd", "help", "exit", "jobs" };
static const int BUILTIN_COUNT = 5;

int is_builtin(const char *cmd)
{
    int i;

    if (cmd == NULL) {
        return 0;
    }
    for (i = 0; i < BUILTIN_COUNT; i++) {
        if (strcmp(cmd, BUILTIN_NAMES[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int handle_cd(int argc, char **argv)
{
    const char *target;

    /* 인자 2개 이상 -> usage 출력 (01문서 5-2) */
    if (argc >= 3) {
        fprintf(stderr, "usage: cd [path]\n");
        return 1;
    }

    if (argc == 1) {
        /*
         * 팀 합의 사항(01문서 5-2 "인자 0개" 항목): 인자 없을 때는 HOME으로 이동.
         * 필요 시 이 부분을 미지원(오류 처리)으로 바꾸는 것도 가능하나,
         * 일반 쉘 관례를 따라 HOME 이동을 기본값으로 채택.
         */
        target = getenv("HOME");
        if (target == NULL) {
            fprintf(stderr, "cd: HOME environment variable not set\n");
            return 1;
        }
    } else {
        target = argv[1];
    }

    if (chdir(target) != 0) {
        perror("cd"); /* 05문서 3-1: 반환값 검증 + perror */
        return 0;     /* 실패해도 쉘 종료 아님, 현재 디렉토리 유지 */
    }

    return 0;
}

int handle_pwd(int argc, char **argv)
{
    char cwd[MAX_INPUT_LENGTH];
    (void)argv; /* 시그니처 통일을 위해 유지, 실제로는 사용하지 않음 */

    if (argc > 1) {
        fprintf(stderr, "usage: pwd\n");
        return 1;
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return 0;
    }

    printf("%s\n", cwd);
    return 0;
}

int handle_help(int argc, char **argv)
{
    (void)argv;

    if (argc > 1) {
        fprintf(stderr, "usage: help\n");
        return 1;
    }

    printf(TU_BLUE "TUK-Shell (ZSH Part) - 사용 가능한 명령어 [3단계]\n" COLOR_RESET);
    printf("  cd [path]           작업 디렉토리 변경 (인자 없으면 HOME 이동)\n");
    printf("  pwd                 현재 작업 디렉토리 출력\n");
    printf("  help                이 도움말 출력\n");
    printf("  exit                쉘 종료\n");
    printf("  jobs                전체 백그라운드 작업 목록 출력\n");
    printf("  jobs -pid [PID]     PID로 작업 검색\n");
    printf("  jobs -name [NAME]   이름(부분 문자열)으로 작업 검색\n");
    printf("  [cmd] [args]        외부 명령어 실행 (fork + execvp)\n");
    printf("  [cmd] [args] &      백그라운드 실행 및 작업 등록\n");
    printf("\n"
           "※ top, schedule, bus, bob, notice, map, weather, contact 명령어는\n"
           "  plan.md 개발 순서에 따라 이후 단계에서 순차적으로 추가됩니다.\n");

    return 0;
}

int handle_exit(int argc, char **argv)
{
    (void)argv;

    if (argc > 1) {
        fprintf(stderr, "usage: exit\n");
        return 1;
    }
    /*
     * 실제 자원 정리(백그라운드 리스트 free, 히스토리 flush/close 등)는
     * main.c의 종료 파이프라인에서 수행한다. 여기서는 종료 신호 역할만 한다.
     */
    return 0;
}

/* jobs 출력 컬럼 헤더 (01문서 6-4, 02문서 1-2 "표 형식 출력은 컬럼 제목을 포함한다") */
static void print_jobs_header(void)
{
    printf("%-6s %-8s %-9s %-10s %-6s %-9s %s\n",
           "JOB#", "PID", "STATUS", "START_TIME", "CPU%", "MEM(KB)", "COMMAND");
}

/* 노드 한 건을 01문서 6-4 포맷에 맞춰 출력 */
static void print_job_row(int job_number, const ProcessInfo *proc)
{
    char job_col[16];
    char time_buf[16];
    struct tm *tm_info;

    snprintf(job_col, sizeof(job_col), "[%d]", job_number);

    tm_info = localtime(&proc->start_time);
    if (tm_info != NULL) {
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
    } else {
        snprintf(time_buf, sizeof(time_buf), "--:--:--");
    }

    printf("%-6s %-8d %-9s %-10s %-6.1f %-9ld %s\n",
           job_col, (int)proc->pid, process_status_to_string(proc->status),
           time_buf, proc->cpu_usage, proc->mem_usage_kb, proc->command);
}

/* argv[2]가 순수 숫자(PID)인지 검사 - "jobs -pid abc" 방어 (01문서 6-3) */
static int is_all_digits(const char *s)
{
    if (s == NULL || *s == '\0') {
        return 0;
    }
    for (; *s != '\0'; s++) {
        if (*s < '0' || *s > '9') {
            return 0;
        }
    }
    return 1;
}

/* jobs (인자 없음): 전체 목록 출력 */
static int handle_jobs_all(ProcessInfo *head)
{
    ProcessInfo *cur;
    int job_number;

    if (head == NULL) {
        printf("No background jobs.\n");
        return 0;
    }

    print_jobs_header();
    job_number = 1;
    for (cur = head; cur != NULL; cur = cur->next, job_number++) {
        print_job_row(job_number, cur);
    }
    return 0;
}

/* jobs -pid [PID]: PID 완전 일치 검색 (01문서 6-2) */
static int handle_jobs_by_pid(int argc, char **argv, ProcessInfo *head)
{
    ProcessInfo *cur;
    ProcessInfo *found = NULL;
    int job_number;
    int found_index = 0;
    pid_t target_pid;

    if (argc != 3) {
        fprintf(stderr, "usage: jobs -pid [PID]\n");
        return 1;
    }
    if (!is_all_digits(argv[2])) {
        fprintf(stderr, "invalid jobs option: PID must be numeric\n");
        return 1;
    }
    target_pid = (pid_t)atol(argv[2]);

    job_number = 1;
    for (cur = head; cur != NULL; cur = cur->next, job_number++) {
        if (cur->pid == target_pid) {
            found = cur;
            found_index = job_number;
            break; /* PID는 유일하므로 첫 매칭에서 종료 */
        }
    }

    if (found == NULL) {
        printf("No matching job found.\n");
        return 0;
    }

    print_jobs_header();
    print_job_row(found_index, found);
    return 0;
}

/* jobs -name [KEYWORD]: name 필드 부분 문자열 검색 (01문서 6-2) */
static int handle_jobs_by_name(int argc, char **argv, ProcessInfo *head)
{
    ProcessInfo *cur;
    int job_number;
    int matched = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: jobs -name [KEYWORD]\n");
        return 1;
    }

    for (cur = head; cur != NULL; cur = cur->next) {
        if (strstr(cur->name, argv[2]) != NULL) {
            matched = 1;
            break;
        }
    }

    if (!matched) {
        printf("No matching job found.\n");
        return 0;
    }

    print_jobs_header();
    job_number = 1;
    for (cur = head; cur != NULL; cur = cur->next, job_number++) {
        if (strstr(cur->name, argv[2]) != NULL) {
            print_job_row(job_number, cur);
        }
    }
    return 0;
}

int handle_jobs(int argc, char **argv, ProcessInfo **job_list)
{
    /* 03문서 6-1: jobs 호출 -> 백그라운드 상태 갱신을 먼저 수행 */
    refresh_all_processes(job_list);

    if (argc == 1) {
        return handle_jobs_all(*job_list);
    }

    if (strcmp(argv[1], "-pid") == 0) {
        return handle_jobs_by_pid(argc, argv, *job_list);
    }

    if (strcmp(argv[1], "-name") == 0) {
        return handle_jobs_by_name(argc, argv, *job_list);
    }

    fprintf(stderr, "invalid jobs option\n");
    return 1;
}

int execute_builtin(int argc, char **argv, int *should_exit, ProcessInfo **job_list)
{
    *should_exit = 0;

    if (strcmp(argv[0], "cd") == 0) {
        return handle_cd(argc, argv);
    } else if (strcmp(argv[0], "pwd") == 0) {
        return handle_pwd(argc, argv);
    } else if (strcmp(argv[0], "help") == 0) {
        return handle_help(argc, argv);
    } else if (strcmp(argv[0], "jobs") == 0) {
        return handle_jobs(argc, argv, job_list);
    } else if (strcmp(argv[0], "exit") == 0) {
        int result = handle_exit(argc, argv);
        if (result == 0) {
            *should_exit = 1;
        }
        return result;
    }

    /* is_builtin() 확인 없이 호출된 방어적 상황 */
    fprintf(stderr, "error: unknown builtin command '%s'\n", argv[0]);
    return -1;
}
