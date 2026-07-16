/*
 * unit_system_info.c - update_process_stats() /proc 파싱 단위 회귀
 * (plan.md 5단계, 04_검증및추적명세서 자동 회귀 보강)
 *
 * TUK_PROC_ROOT로 tests/fixtures/proc를 주입해 /proc가 없는 비Linux(macOS)
 * 개발환경에서도 실제 Linux 파싱 경로를 결정론적으로 검증한다.
 * stat fixture의 comm("my (weird) proc")에 공백/괄호를 넣어 strrchr(')')
 * 골격(동현 파서 이식)의 안전성을 함께 증명한다.
 *
 * fixture 기대값 (CLK_TCK=100 기준; macOS/Linux USER_HZ 관례값):
 *  cpu = 100 * ((utime 500 + stime 300)/100) / (uptime 1000 - 20000/100)
 *      = 100 * 8 / 800 = 1.00%
 *  mem = VmRSS 5432 KB
 *
 * 빌드/실행: tests/run_tests.sh가 Makefile과 동일 플래그로 컴파일해 실행하고,
 * PASS/FAIL 라인을 전체 집계에 합산한다. 실패 시 종료 코드 1.
 */
#define _POSIX_C_SOURCE 200809L /* glibc -std=c11에서 setenv 노출용 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "process.h"
#include "system_info.h"

static int g_fail = 0;

static void report(const char *name, int ok)
{
    printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) {
        g_fail = 1;
    }
}

int main(int argc, char **argv)
{
    /* fixture 루트: 인자로 받거나(run_tests.sh), src/bash에서 직접 실행 시
     * 상대경로 기본값 사용 */
    const char *root = (argc > 1) ? argv[1] : "tests/fixtures/proc";

    /* U1: 정상 파싱 - comm에 공백/괄호가 있어도 stat 필드가 정확해야 한다 */
    if (setenv("TUK_PROC_ROOT", root, 1) != 0) {
        fprintf(stderr, "setenv failed\n");
        return 1;
    }
    ProcessInfo proc;
    memset(&proc, 0, sizeof(proc));
    proc.pid = 4242;
    int rc = update_process_stats(&proc);
    report("U1 fixture pid=4242 parsed (rc==0)", rc == 0);
    report("U1 cpu_usage == 1.00% (stat utime/stime/starttime)",
           fabs(proc.cpu_usage - 1.0) < 0.005);
    report("U1 mem_usage_kb == 5432 (status VmRSS)",
           proc.mem_usage_kb == 5432L);

    /* U2: <root>/<pid> 부재 = 프로세스 종료 경합 경로 → 조용히 -1 */
    memset(&proc, 0, sizeof(proc));
    proc.pid = 4243;
    report("U2 missing <pid> dir returns -1", update_process_stats(&proc) == -1);

    /* U3/U4: 방어적 인자 검증 */
    report("U3 NULL proc returns -1", update_process_stats(NULL) == -1);
    memset(&proc, 0, sizeof(proc));
    proc.pid = 0;
    report("U4 pid<=0 returns -1", update_process_stats(&proc) == -1);

    /* U5: 명시적 TUK_PROC_ROOT가 잘못된 경로 → 더미 폴백 없이 -1
     * (TUK_CAMPUS_DATA와 동일한 '명시 시 폴백 없음' 정책) */
    if (setenv("TUK_PROC_ROOT", "/nonexistent/tuk_proc_root", 1) != 0) {
        fprintf(stderr, "setenv failed\n");
        return 1;
    }
    memset(&proc, 0, sizeof(proc));
    proc.pid = 4242;
    report("U5 explicit bad TUK_PROC_ROOT returns -1 (no fallback)",
           update_process_stats(&proc) == -1);

    return g_fail ? 1 : 0;
}
