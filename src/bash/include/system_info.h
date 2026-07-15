#ifndef TUK_SYSTEM_INFO_H
#define TUK_SYSTEM_INFO_H

#include "process.h"

/*
 * system_info.h - /proc 기반 시스템 정보 수집 인터페이스 [타 파트 - 동현]
 *
 * 01_상세기능명세서 3-4: BASH 파트는 /proc 파싱을 직접 구현하지 않고
 * 아래 인터페이스 호출을 전제로 한다. 시그니처는 02 4-4와 동일하게 고정.
 *
 * 규약:
 *  - 성공 시 0 반환, 실패 시 -1 반환
 *  - 실패 시 호출부(refresh_all_processes)가 cpu_usage=0.0,
 *    mem_usage_kb=0으로 되돌리고 상태는 기존 값을 유지한다.
 *  - /proc/[pid] 접근 불가 시 호출부는 waitpid() 결과를 우선 신뢰한다.
 */
int update_process_stats(ProcessInfo *proc);

#endif /* TUK_SYSTEM_INFO_H */
