/*
 * commands.c - 내장 명령어 핸들러 및 명령 분기 [우진]
 *
 * 근거 문서:
 *  - 01_상세기능명세서 5장 (기본 내장 명령어), 6장 (jobs), 7장 (top)
 *  - 02_인터페이스명세서 3장 (CLI 규격), 4장 (함수 인터페이스)
 *  - 03_파이프라인명세서 4장 (분기), 6장 (jobs/top 조회 파이프라인)
 */
#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "campus.h"
#include "executor.h"
#include "parser.h"
#include "process.h"

#define CWD_BUFFER_SIZE 4096

/* 백그라운드 등록용 원본 명령 문자열 (commands.h 주석 참고) */
static const char *g_raw_line = NULL;

void commands_set_raw_line(const char *raw_line)
{
    g_raw_line = raw_line;
}

/* ------------------------------------------------------------
 * 기본 내장 명령어 (01 5장)
 * ------------------------------------------------------------ */

int handle_cd(int argc, char **argv)
{
    const char *target = NULL;

    if (argc == 1) {
        /* 01 5-2의 팀 합의 항목: plan.md의 "기존 쉘 명령어 완벽 호환"에
         * 따라 인자 없으면 홈 디렉토리 이동안을 채택 (팀 확정 필요 표기) */
        target = getenv("HOME");
        if (target == NULL || target[0] == '\0') {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    } else if (argc == 2) {
        target = argv[1];
    } else {
        fprintf(stderr, "usage: cd [path]\n"); /* 01 5-2: 인자 2개 이상 */
        return 1;
    }

    if (chdir(target) != 0) {
        perror("cd"); /* 01 5-1: 실패 시 perror, 현재 디렉토리 유지 */
        return 1;
    }
    return 0;
}

int handle_pwd(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        fprintf(stderr, "usage: pwd\n"); /* 01 5-2: 추가 인자 시 사용법만 */
        return 1;
    }

    char cwd[CWD_BUFFER_SIZE];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return 1;
    }
    printf("%s\n", cwd);
    return 0;
}

int handle_help(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        fprintf(stderr, "usage: help\n");
        return 1;
    }

    /* 01 5-1: 사용 가능한 명령어와 옵션 요약. 제목만 TU BLUE 강조하고
     * 내용은 색상 없이도 식별 가능하게 유지한다. (02 1-2) */
    printf(TUK_COLOR_BLUE "TUK-Shell commands" TUK_COLOR_RESET "\n");
    printf("  built-in:\n");
    printf("    cd [path]                     change directory\n");
    printf("    pwd                           print working directory\n");
    printf("    help                          show this message\n");
    printf("    exit                          quit TUK-Shell\n");
    printf("    jobs [-pid PID | -name KEY]   list/search background jobs\n");
    printf("    top -cpu | -mem | -time       sort background jobs\n");
    printf("  campus:\n");
    printf("    schedule                      AI-SW department timetable\n");
    printf("    bus -1 | -2                   campus shuttle timetable\n");
    printf("    bob -t | -E | -d              cafeteria menu\n");
    printf("    notice [-g | -a | -s] [-n N]  school notices\n");
    printf("    map -A..-G | -f | -find ROOM  campus buildings\n");
    printf("    weather -c | -w | -d          Siheung campus weather\n");
    printf("    contact -p NAME | -d DEPT | -e  contacts\n");
    printf("  external:\n");
    printf("    [cmd] [args...] [&]           run via fork/execvp\n");
    return 0;
}

int handle_exit(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) {
        fprintf(stderr, "usage: exit\n"); /* 01 5-2: 인자 있으면 실행 안 함 */
        return 1;
    }
    /* 실제 정리(리스트 해제, 히스토리 close)는 main.c의 종료 파이프라인이
     * 수행한다. (03 9장: 흐름 제어는 main 담당 - 05 1-1) */
    return TUK_SHELL_EXIT;
}

/* ------------------------------------------------------------
 * jobs / top 공용 표 출력 (01 6-4 출력 컬럼 규격)
 * ------------------------------------------------------------ */

static void print_job_header(void)
{
    /* 02 1-2: 표 형식 출력은 컬럼 제목을 포함한다 */
    printf("%-7s %-8s %-9s %-11s %-6s %-8s %s\n", "[JOB#]", "PID", "STATUS",
           "START_TIME", "CPU%", "MEM(KB)", "COMMAND");
}

static void print_job_row(int index, const ProcessInfo *proc)
{
    char job_label[16];
    char time_text[9]; /* "HH:MM:SS" + NUL (01 6-4) */
    struct tm *time_info = localtime(&proc->start_time);

    if (time_info == NULL ||
        strftime(time_text, sizeof(time_text), "%H:%M:%S", time_info) == 0) {
        snprintf(time_text, sizeof(time_text), "??:??:??");
    }
    snprintf(job_label, sizeof(job_label), "[%d]", index);

    /* 02 1-1: 작업 상태/PID/시스템 정보는 TU SKY BLUE */
    printf(TUK_COLOR_SKY "%-7s %-8ld %-9s %-11s %-6.1f %-8ld %s"
           TUK_COLOR_RESET "\n",
           job_label, (long)proc->pid, process_status_string(proc->status),
           time_text, proc->cpu_usage, proc->mem_usage_kb, proc->command);
}

/* ------------------------------------------------------------
 * jobs: 작업 검색 (01 6장)
 * ------------------------------------------------------------ */

int handle_jobs(int argc, char **argv)
{
    /* 03 6-1: jobs 호출 -> 백그라운드 상태 갱신 -> 옵션 해석 -> 출력 */
    refresh_all_processes(&g_process_list);

    if (argc == 1) {
        if (g_process_list == NULL) {
            printf("No background jobs.\n"); /* 02 3-2: 빈 결과 메시지 */
            return 0;
        }
        print_job_header();
        int index = 1;
        for (ProcessInfo *cur = g_process_list; cur != NULL;
             cur = cur->next) {
            print_job_row(index++, cur);
        }
        return 0;
    }

    if (strcmp(argv[1], "-pid") == 0) {
        if (argc != 3) { /* 01 6-3: 옵션 뒤 인자가 없으면 사용법 출력 */
            fprintf(stderr, "usage: jobs -pid PID\n");
            return 1;
        }
        long pid_value = 0;
        if (parse_positive_long(argv[2], &pid_value) != 0) {
            fprintf(stderr, "jobs: PID must be a positive number\n");
            return 1;
        }
        /* 01 6-2: 숫자 PID 완전 일치 검색 */
        ProcessInfo *found =
            find_process_by_pid(g_process_list, (pid_t)pid_value);
        if (found == NULL) {
            printf("No matching job found.\n"); /* 01 6-2 */
            return 0;
        }
        print_job_header();
        print_job_row(1, found);
        return 0;
    }

    if (strcmp(argv[1], "-name") == 0) {
        if (argc != 3) {
            fprintf(stderr, "usage: jobs -name KEYWORD\n");
            return 1;
        }
        int matched = 0;
        int index = 1;
        for (ProcessInfo *cur = g_process_list; cur != NULL;
             cur = cur->next) {
            /* 01 6-2: name 필드 기준 부분 문자열 검색 */
            if (strstr(cur->name, argv[2]) != NULL) {
                if (!matched) {
                    print_job_header();
                }
                matched = 1;
                print_job_row(index++, cur);
            }
        }
        if (!matched) {
            printf("No matching job found.\n");
        }
        return 0;
    }

    /* 01 6-3: 지원하지 않는 옵션 */
    fprintf(stderr, "jobs: invalid jobs option '%s'\n", argv[1]);
    fprintf(stderr, "usage: jobs [-pid PID | -name KEYWORD]\n");
    return 1;
}

/* ------------------------------------------------------------
 * top: 작업 정렬 (01 7장) - qsort 콜백 비교 함수 직접 구현 (plan.md [4])
 * ------------------------------------------------------------ */

static int compare_by_cpu_desc(const void *lhs, const void *rhs)
{
    const ProcessInfo *a = *(ProcessInfo *const *)lhs;
    const ProcessInfo *b = *(ProcessInfo *const *)rhs;
    if (a->cpu_usage < b->cpu_usage) {
        return 1;
    }
    if (a->cpu_usage > b->cpu_usage) {
        return -1;
    }
    return 0;
}

static int compare_by_mem_desc(const void *lhs, const void *rhs)
{
    const ProcessInfo *a = *(ProcessInfo *const *)lhs;
    const ProcessInfo *b = *(ProcessInfo *const *)rhs;
    if (a->mem_usage_kb < b->mem_usage_kb) {
        return 1;
    }
    if (a->mem_usage_kb > b->mem_usage_kb) {
        return -1;
    }
    return 0;
}

/* 01 7-2: (현재 시각 - start_time) 내림차순 == start_time 오름차순 */
static int compare_by_time_desc(const void *lhs, const void *rhs)
{
    const ProcessInfo *a = *(ProcessInfo *const *)lhs;
    const ProcessInfo *b = *(ProcessInfo *const *)rhs;
    if (a->start_time < b->start_time) {
        return -1;
    }
    if (a->start_time > b->start_time) {
        return 1;
    }
    return 0;
}

int handle_top(int argc, char **argv)
{
    /* 03 6-2: top 호출 -> 상태 갱신 -> 배열 복사 -> qsort -> 출력 -> 해제 */
    refresh_all_processes(&g_process_list);

    if (argc != 2) { /* 01 7-4: 옵션이 없거나 2개 이상이면 사용법 출력 */
        fprintf(stderr, "usage: top -cpu | -mem | -time\n");
        return 1;
    }

    int (*compare)(const void *, const void *) = NULL;
    if (strcmp(argv[1], "-cpu") == 0) {
        compare = compare_by_cpu_desc;
    } else if (strcmp(argv[1], "-mem") == 0) {
        compare = compare_by_mem_desc;
    } else if (strcmp(argv[1], "-time") == 0) {
        compare = compare_by_time_desc;
    } else {
        fprintf(stderr, "top: invalid top option '%s'\n", argv[1]);
        fprintf(stderr, "usage: top -cpu | -mem | -time\n");
        return 1;
    }

    int count = process_count(g_process_list);
    if (count == 0) {
        printf("No background jobs.\n"); /* 01 7-3 */
        return 0;
    }

    /* 01 7-3: 원본 리스트를 깨뜨리지 않도록 출력용 포인터 배열 복사 후 정렬.
     * free 시점: 아래 표 출력 직후 즉시 해제 (03 6-2) */
    ProcessInfo **items = malloc(sizeof(ProcessInfo *) * (size_t)count);
    if (items == NULL) {
        perror("malloc"); /* 03 10장: 할당 실패 시 현재 작업 중단 */
        return -1;
    }
    int index = 0;
    for (ProcessInfo *cur = g_process_list; cur != NULL; cur = cur->next) {
        items[index++] = cur;
    }
    qsort(items, (size_t)count, sizeof(ProcessInfo *), compare);

    print_job_header();
    for (int i = 0; i < count; i++) {
        print_job_row(i + 1, items[i]);
    }
    free(items); /* 출력용 임시 배열 해제 (03 6-2) */
    return 0;
}

/* ------------------------------------------------------------
 * 명령 분기 (02 2-3: 내장 테이블 매칭 -> 외부 명령 실행)
 * ------------------------------------------------------------ */

struct builtin_entry {
    const char *name;
    int (*handler)(int argc, char **argv);
};

/* 01 5-3의 백그라운드 금지 목록과 동일한 여섯 개 내장 명령 */
static const struct builtin_entry BUILTINS[] = {
    { "cd", handle_cd },     { "pwd", handle_pwd },
    { "help", handle_help }, { "exit", handle_exit },
    { "jobs", handle_jobs }, { "top", handle_top },
};

int execute_command(int argc, char **argv, int background_flag)
{
    if (argc <= 0 || argv == NULL || argv[0] == NULL) {
        return 0;
    }

    for (size_t i = 0; i < sizeof(BUILTINS) / sizeof(BUILTINS[0]); i++) {
        if (strcmp(argv[0], BUILTINS[i].name) == 0) {
            if (background_flag) {
                /* 01 5-3: 금지된 조합 - 오류 출력 후 루프 복귀 */
                fprintf(stderr,
                        "built-in command cannot run in background\n");
                return 1;
            }
            return BUILTINS[i].handler(argc, argv);
        }
    }

    if (is_campus_command(argv[0])) {
        /* 03 4-2: 캠퍼스 명령도 쉘 프로세스 내부에서 직접 실행되는
         * 내장 명령이므로 동일하게 백그라운드 조합을 금지한다. */
        if (background_flag) {
            fprintf(stderr, "built-in command cannot run in background\n");
            return 1;
        }
        return dispatch_campus_command(argc, argv);
    }

    /* 02 3-4: 외부 명령 - fork/execvp (백그라운드면 등록 후 즉시 복귀) */
    return run_external_command(argv, background_flag,
                                g_raw_line != NULL ? g_raw_line : argv[0]);
}
