#include "system_info.h"

int update_process_stats(ProcessInfo *proc)
{
    (void)proc;

    /*
     * [2단계 스텁]
     * /proc/[pid]/stat, /proc/[pid]/status 파싱은 5단계(plan.md 개발 순서)에서
     * 동현 파트 인터페이스에 맞춰 구현한다. 지금은 항상 실패를 반환하여
     * refresh_all_processes()가 cpu_usage=0.0, mem_usage_kb=0으로 안전하게
     * 대체하도록 한다 (01_상세기능명세서.md 3-4).
     */
    return -1;
}
