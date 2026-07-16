#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "system_info.h"

/* 리눅스 기본값(HZ=100)을 폴백으로 사용, sysconf 실패 시에만 적용 */
#define DEFAULT_CLK_TCK 100

static long get_clock_ticks_per_sec(void)
{
    long ticks = sysconf(_SC_CLK_TCK);

    if (ticks <= 0) {
        ticks = DEFAULT_CLK_TCK;
    }
    return ticks;
}

/*
 * read_proc_stat_times
 *  - /proc/[pid]/stat에서 utime(14번째 필드), stime(15번째 필드)을 파싱한다.
 *  - comm(2번째 필드)은 괄호로 감싸져 있고 내부에 공백/괄호가 포함될 수 있으므로,
 *    마지막 ')' 위치를 기준으로 그 뒤부터 상대 위치로 안전하게 파싱한다.
 *  - 성공 0, 실패(파일 없음/파싱 실패) -1.
 */
static int read_proc_stat_times(pid_t pid, unsigned long *utime, unsigned long *stime)
{
    char path[64];
    char buf[512];
    FILE *fp;
    char *comm_end;
    unsigned long ut, st;
    int n;

    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);

    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1; /* 프로세스가 이미 종료되어 접근 불가능한 경우 등 */
    }

    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    comm_end = strrchr(buf, ')');
    if (comm_end == NULL) {
        return -1;
    }

    /*
     * comm_end 다음 필드부터: state(1) ppid(2) pgrp(3) session(4) tty_nr(5)
     * tpgid(6) flags(7) minflt(8) cminflt(9) majflt(10) cmajflt(11)
     * utime(12) stime(13) ...  (comm 제외 상대 인덱스 기준)
     */
    n = sscanf(comm_end + 1,
               " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
               &ut, &st);
    if (n != 2) {
        return -1;
    }

    *utime = ut;
    *stime = st;
    return 0;
}

/*
 * read_proc_status_rss
 *  - /proc/[pid]/status에서 VmRSS(KB) 값을 파싱한다.
 *  - 성공 0, 실패(파일 없음/필드 없음) -1.
 */
static int read_proc_status_rss(pid_t pid, long *rss_kb)
{
    char path[64];
    char line[256];
    FILE *fp;
    int found = 0;

    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);

    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            long value;
            if (sscanf(line + 6, "%ld", &value) == 1) {
                *rss_kb = value;
                found = 1;
            }
            break;
        }
    }
    fclose(fp);

    return found ? 0 : -1;
}

int update_process_stats(ProcessInfo *proc)
{
    unsigned long utime, stime;
    long rss_kb;
    long ticks_per_sec;
    double total_cpu_seconds;
    double elapsed_seconds;
    time_t now;

    if (proc == NULL) {
        return -1;
    }

    if (read_proc_stat_times(proc->pid, &utime, &stime) != 0) {
        return -1; /* 호출부가 cpu=0.0, mem=0으로 처리 (01문서 3-4) */
    }

    if (read_proc_status_rss(proc->pid, &rss_kb) != 0) {
        return -1;
    }

    ticks_per_sec = get_clock_ticks_per_sec();
    total_cpu_seconds = (double)(utime + stime) / (double)ticks_per_sec;

    now = time(NULL);
    elapsed_seconds = difftime(now, proc->start_time);
    if (elapsed_seconds <= 0.0) {
        elapsed_seconds = 1.0; /* 등록 직후 0으로 나누기 방지 */
    }

    /* [설계 노트] system_info.h 주석 참고: "시작 이후 누적 평균 CPU 점유율" */
    proc->cpu_usage = (total_cpu_seconds / elapsed_seconds) * 100.0;
    proc->mem_usage_kb = rss_kb;

    return 0;
}
