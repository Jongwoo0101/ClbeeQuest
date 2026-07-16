#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "commands.h"
#include "process.h"
#include "tuk_shell.h"

/* 4단계 기준 내장 명령어 목록. 캠퍼스 명령어(7단계)는 campus.c에서 별도 관리한다. */
static const char *BUILTIN_NAMES[] = { "cd", "pwd", "help", "exit", "jobs", "top" };
static const int BUILTIN_COUNT = 6;

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

    printf("\n");
    printf(TU_MINT " 📖 [TUK-OS 커맨드 매뉴얼]\n" COLOR_RESET);
    printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n" COLOR_RESET);
    
    printf(TU_BLUE " 📌 시스템 기본 명령어\n" COLOR_RESET);
    printf("  cd [path]                 작업 디렉토리 변경 (인자 없으면 HOME 이동)\n");
    printf("  pwd                       현재 작업 디렉토리 출력\n");
    printf("  help                      이 도움말 출력\n");
    printf("  exit                      쉘 종료\n");
    printf("  jobs                      전체 백그라운드 작업 목록 출력\n");
    printf("  jobs -pid [PID]           PID로 작업 검색\n");
    printf("  jobs -name [NAME]         이름(부분 문자열)으로 작업 검색\n");
    printf("  top -cpu                  CPU 사용량 내림차순 정렬 출력\n");
    printf("  top -mem                  메모리 사용량 내림차순 정렬 출력\n");
    printf("  top -time                 실행시간 내림차순 정렬 출력\n");
    printf("  [cmd] [args]              외부 명령어 실행 (fork + execvp)\n");
    printf("  [cmd] [args] &            백그라운드 실행 및 작업 등록\n");
    
    printf(TU_SKY_BLUE " ────────────────────────────────────────────────────────────────────────────\n" COLOR_RESET);
    
    printf(TU_BLUE " 🎓 캠퍼스 특화 명령어\n" COLOR_RESET);
    printf("  schedule                  AI소프트웨어학과 시간표 출력\n");
    printf("  bus -1 | -2               1캠퍼스 / 2캠퍼스 셔틀 시간표\n");
    printf("  bob -t | -e | -d          TIP 지하 / E동 레스토랑 / 대신식당 메뉴\n");
    printf("  notice [-g|-a|-s] [-n N]  공지사항 (일반/학사/장학, 최근 N개 출력)\n");
    printf("  map -A ~ -G               건물별 층 안내\n");
    printf("  map -f                    교내 편의시설 안내\n");
    printf("  map -find [강의실]        강의실 위치 검색 (예: map -find E동 402)\n");
    printf("  weather -c | -w | -d      현재 날씨 / 주간 예보 / 미세먼지 확인\n");
    printf("  contact -p [이름 | -l]    교수 연락처 검색 (-l: 전체 목록)\n");
    printf("  contact -d [부서 | -l]    행정부서 연락처 검색 (-l: 전체 목록)\n");
    printf("  contact -e                긴급 연락처 모아보기\n");
    
    printf(TU_SKY_BLUE " ════════════════════════════════════════════════════════════════════════════\n\n" COLOR_RESET);

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

/*
 * TopEntry
 *  - 정렬 기준(-cpu/-mem/-time)에 따라 미리 계산해 둔 metric 값과 원본 노드
 *    포인터를 함께 담는다. qsort 콜백은 이 metric만 비교하면 되므로
 *    옵션별 분기를 컴페어 함수 밖에서 한 번만 처리할 수 있다.
 */
typedef struct {
    ProcessInfo *proc;
    double metric;
} TopEntry;

/* 내림차순 비교 콜백 (01문서 7-3 "비교 함수는 직접 구현") */
static int compare_top_entry_desc(const void *a, const void *b)
{
    const TopEntry *ea = (const TopEntry *)a;
    const TopEntry *eb = (const TopEntry *)b;

    if (ea->metric < eb->metric) {
        return 1;
    }
    if (ea->metric > eb->metric) {
        return -1;
    }
    return 0;
}

int handle_top(int argc, char **argv, ProcessInfo **job_list)
{
    ProcessInfo *cur;
    TopEntry *entries;
    int count;
    int i;
    time_t now;

    /* 03문서 6-2: top 호출 -> 백그라운드 상태 갱신 먼저 */
    refresh_all_processes(job_list);

    /* 옵션이 없거나 2개 이상이면 사용법 출력 (01문서 7-4) */
    if (argc != 2) {
        fprintf(stderr, "usage: top -cpu | top -mem | top -time\n");
        return 1;
    }

    if (strcmp(argv[1], "-cpu") != 0 &&
        strcmp(argv[1], "-mem") != 0 &&
        strcmp(argv[1], "-time") != 0) {
        fprintf(stderr, "invalid top option\n");
        return 1;
    }

    count = 0;
    for (cur = *job_list; cur != NULL; cur = cur->next) {
        count++;
    }

    if (count == 0) {
        printf("No background jobs.\n"); /* 01문서 7-3 */
        return 0;
    }

    /* 리스트를 출력용 배열로 복사 - 원본 연결 리스트는 정렬하지 않는다 (01문서 7-3) */
    entries = (TopEntry *)malloc(sizeof(TopEntry) * (size_t)count);
    if (entries == NULL) {
        perror("malloc");
        return -1;
    }

    now = time(NULL);
    i = 0;
    for (cur = *job_list; cur != NULL; cur = cur->next, i++) {
        entries[i].proc = cur;
        if (strcmp(argv[1], "-cpu") == 0) {
            entries[i].metric = cur->cpu_usage;
        } else if (strcmp(argv[1], "-mem") == 0) {
            entries[i].metric = (double)cur->mem_usage_kb;
        } else { /* -time: 현재 시각 - start_time 내림차순 (01문서 7-2) */
            entries[i].metric = difftime(now, cur->start_time);
        }
    }

    qsort(entries, (size_t)count, sizeof(TopEntry), compare_top_entry_desc);

    print_jobs_header();
    for (i = 0; i < count; i++) {
        print_job_row(i + 1, entries[i].proc);
    }

    /* [free] 정렬용 임시 배열 해제 - 원본 ProcessInfo 노드는 그대로 리스트 소유 (03문서 6-2) */
    free(entries);

    return 0;
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
    } else if (strcmp(argv[0], "top") == 0) {
        return handle_top(argc, argv, job_list);
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
