#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include "process.h"

/*
 * update_process_stats
 *  - /proc/[pid]/stat, /proc/[pid]/status 파싱 기반 CPU/메모리 갱신.
 *  - plan.md 개발 순서상 5단계에서 실제 구현되며, 동현 파트와 인터페이스를 맞춘다
 *    (01_상세기능명세서.md 3-4 "[타 파트 - 동현]").
 *  - 성공 시 0, 실패 시 -1.
 *  - 실패 시 호출부(refresh_all_processes)는 cpu_usage=0.0, mem_usage_kb=0으로 처리하고
 *    status는 기존 값을 유지한다.
 *
 * [2단계 현재 상태]
 *  실제 /proc 파싱은 아직 구현하지 않았고, 항상 -1(미구현)을 반환하는 스텁이다.
 *  인터페이스 시그니처만 고정해 두어 이후 executor/process 쪽 코드를 바꾸지 않고
 *  5단계에서 이 파일의 구현부만 교체하면 되도록 설계했다.
 */
int update_process_stats(ProcessInfo *proc);

#endif /* SYSTEM_INFO_H */
