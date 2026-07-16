/*
 * system_info.c - 플랫폼별 CPU/메모리 통계 실구현 [우진]
 *
 * 근거 문서:
 *  - 01_상세기능명세서 3-4 (실패 시 0.0/0 복원은 호출부 담당), 02 4-4 (시그니처 고정)
 *  - plan.md 5단계 (/proc 연동), 05_개발철칙명세서 5-1 (독립 개발/검증 보장)
 *
 * 원래 동현(PowerShell 파트) 소스 병합 대기 지점(Mock)이었으나, 동현의
 * /proc 파서(src/powershell/system_info.c)를 기반으로 BASH 파트(우진)가
 * 직접 이식·실구현했다. 파싱 골격은 동현 구현을 따른다:
 *  - stat의 comm(2번 필드)은 공백/괄호를 포함할 수 있으므로 strrchr(')')로
 *    마지막 ')'를 찾아 그 뒤에서 3번 필드(state)부터 파싱
 *  - utime(14)·stime(15)·starttime(22)만 취하고 나머지는 %* 로 건너뜀
 *  - status의 "VmRSS:" 라인 스캔 → MEM(KB)
 *  - CPU% = 100 * ((utime+stime)/CLK_TCK) / (uptime - starttime/CLK_TCK)
 *
 * Linux 경로 정책:
 *  - TUK_PROC_ROOT 지정 시 그 디렉터리를 /proc 대신 사용(fixture 단위검증용).
 *    명시 지정이 잘못됐으면 조용히 폴백하지 않고 실패 — campus_data.c의
 *    TUK_CAMPUS_DATA와 동일 정책.
 *  - 미지정 && /proc 부재(macOS 등 비Linux 개발환경)면 기존 Mock과 동일한
 *    결정적 더미 값을 반환해 개발/회귀의 결정론을 유지한다(05 5-1).
 *    Ubuntu/WSL 등 Linux에서는 아래 실 파싱 경로가 동작한다.
 */
#include "system_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif

int update_process_stats(ProcessInfo *proc)
{
    if (proc == NULL || proc->pid <= 0) {
        return -1;
    }

#ifdef _WIN32
    if (proc->handle == NULL) {
        return -1;
    }

    FILETIME created;
    FILETIME exited;
    FILETIME kernel;
    FILETIME user;
    if (!GetProcessTimes(proc->handle, &created, &exited, &kernel, &user)) {
        return -1;
    }

    ULARGE_INTEGER created_ticks;
    ULARGE_INTEGER kernel_ticks;
    ULARGE_INTEGER user_ticks;
    created_ticks.LowPart = created.dwLowDateTime;
    created_ticks.HighPart = created.dwHighDateTime;
    kernel_ticks.LowPart = kernel.dwLowDateTime;
    kernel_ticks.HighPart = kernel.dwHighDateTime;
    user_ticks.LowPart = user.dwLowDateTime;
    user_ticks.HighPart = user.dwHighDateTime;

    FILETIME now_filetime;
    GetSystemTimeAsFileTime(&now_filetime);
    ULARGE_INTEGER now_ticks;
    now_ticks.LowPart = now_filetime.dwLowDateTime;
    now_ticks.HighPart = now_filetime.dwHighDateTime;
    if (now_ticks.QuadPart <= created_ticks.QuadPart) {
        return -1;
    }

    ULONGLONG process_ticks = kernel_ticks.QuadPart + user_ticks.QuadPart;
    ULONGLONG elapsed_ticks = now_ticks.QuadPart - created_ticks.QuadPart;
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    DWORD cpu_count = system_info.dwNumberOfProcessors;
    if (cpu_count == 0) {
        cpu_count = 1;
    }
    proc->cpu_usage = 100.0 * (double)process_ticks /
                      (double)elapsed_ticks / (double)cpu_count;

    PROCESS_MEMORY_COUNTERS memory;
    if (!GetProcessMemoryInfo(proc->handle, &memory, sizeof(memory))) {
        return -1;
    }
    proc->mem_usage_kb = (long)(memory.WorkingSetSize / 1024);
    return 0;
#else

    const char *env = getenv("TUK_PROC_ROOT");
    int explicit_root = (env != NULL && env[0] != '\0');
    const char *root = explicit_root ? env : "/proc";

    char path[512];
    char line[512];
    FILE *fp;

    /* 1) <root>/uptime: 가용성 프로브 겸 시스템 가동 시간(초, 첫 필드) */
    if (snprintf(path, sizeof(path), "%s/uptime", root) >= (int)sizeof(path)) {
        return -1;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        if (explicit_root) {
            return -1; /* 명시적 재정의 존중: 잘못된 경로면 폴백 없이 실패 */
        }
        /* 비Linux 개발 폴백: /proc가 없는 macOS 등에서는 기존 Mock과 동일한
         * PID 기반 결정적 더미 값을 유지해 정렬 검증(04 T07)과 회귀의
         * 결정론을 보장한다(05 5-1). Linux에서는 이 분기에 오지 않는다. */
        proc->cpu_usage = (double)(proc->pid % 100) / 10.0;
        proc->mem_usage_kb = 1024L + (long)(proc->pid % 8) * 512L;
        return 0;
    }
    double system_uptime = 0.0;
    int ok = (fgets(line, sizeof(line), fp) != NULL &&
              sscanf(line, "%lf", &system_uptime) == 1);
    fclose(fp);
    if (!ok) {
        return -1;
    }

    /* 이하 <pid> 파일들의 열기/파싱 실패는 perror 없이 조용히 -1 처리한다.
     * /proc/<pid> 소멸은 프로세스 종료 직후의 정상적인 경합이라 에러 출력
     * 대상이 아니며(05 3-1 perror 규칙의 의도적 예외), 호출부 규약
     * (0.0/0 복원)과 waitpid() 회수가 후속 처리한다(system_info.h). */

    /* 2) <root>/<pid>/stat: utime(14)·stime(15)·starttime(22), proc(5) 참조 */
    if (snprintf(path, sizeof(path), "%s/%ld/stat", root,
                 (long)proc->pid) >= (int)sizeof(path)) {
        return -1;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    unsigned long utime = 0, stime = 0;
    unsigned long long starttime = 0;
    ok = 0;
    if (fgets(line, sizeof(line), fp) != NULL) {
        char *p = strrchr(line, ')'); /* comm 안전 건너뛰기 (동현 골격) */
        if (p != NULL && p[1] == ' ') {
            p += 2; /* ')' + 공백 → 3번 필드(state)부터 시작 */
            if (sscanf(p,
                       "%*c "         /* 3: state */
                       "%*d %*d %*d " /* 4-6: ppid pgrp session */
                       "%*d %*d "     /* 7-8: tty_nr tpgid */
                       "%*u "         /* 9: flags */
                       "%*u %*u "     /* 10-11: minflt cminflt */
                       "%*u %*u "     /* 12-13: majflt cmajflt */
                       "%lu %lu "     /* 14-15: utime stime */
                       "%*d %*d "     /* 16-17: cutime cstime */
                       "%*d %*d "     /* 18-19: priority nice */
                       "%*d %*d "     /* 20-21: num_threads itrealvalue */
                       "%llu",        /* 22: starttime */
                       &utime, &stime, &starttime) == 3) {
                ok = 1;
            }
        }
    }
    fclose(fp);
    if (!ok) {
        return -1;
    }

    /* 3) <root>/<pid>/status: "VmRSS:" 라인 → 메모리(KB).
     * 좀비/커널 스레드는 VmRSS가 없으므로 0KB로 두고 성공 처리한다. */
    if (snprintf(path, sizeof(path), "%s/%ld/status", root,
                 (long)proc->pid) >= (int)sizeof(path)) {
        return -1;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    long rss_kb = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %ld", &rss_kb);
            break;
        }
    }
    fclose(fp);

    /* 4) CPU% 산출: 프로세스 생존 기간 대비 CPU 점유 시간 비율 */
    long clk = sysconf(_SC_CLK_TCK);
    if (clk <= 0) {
        clk = 100; /* USER_HZ 관례값 폴백 */
    }
    double elapsed = system_uptime - (double)starttime / (double)clk;
    proc->cpu_usage =
        (elapsed <= 0.0)
            ? 0.0
            : 100.0 * ((double)(utime + stime) / (double)clk) / elapsed;
    proc->mem_usage_kb = rss_kb;
    return 0; /* 실패(-1) 시 0.0/0 복원 규약은 호출부 refresh_all_processes()에 구현됨 */
#endif
}
