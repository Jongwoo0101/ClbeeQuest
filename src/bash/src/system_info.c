/*
 * system_info.c - /proc 파싱 연동 Mock 구현 [타 파트 - 동현]
 *
 * [MOCK] 05_개발철칙명세서 5-1: 동현 파트의 소스 병합 전까지 더미 반환으로
 * 대체하여 BASH 파트의 독립 개발/검증(1~7단계)을 보장한다.
 *
 * 실제 구현 시 /proc/[pid]/stat, /proc/[pid]/status를 파싱해
 * CPU 사용률과 메모리 사용량(KB)을 채우게 된다. (01 3-4, 02 6-1)
 *
 * Mock 정책: PID 기반의 결정적(deterministic) 더미 값을 부여하여
 * top -cpu / -mem 정렬 검증(04 T07)이 Mock 상태에서도 가능하게 한다.
 */
#include "system_info.h"

#include <stddef.h>

int update_process_stats(ProcessInfo *proc)
{
    if (proc == NULL) {
        return -1;
    }
    proc->cpu_usage = (double)(proc->pid % 100) / 10.0;
    proc->mem_usage_kb = 1024L + (long)(proc->pid % 8) * 512L;
    return 0; /* 실패(-1) 시 0.0/0 복원 규약은 호출부 refresh_all_processes()에 구현됨 */
}
