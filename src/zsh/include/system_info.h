#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include "process.h"

/*
 * update_process_stats
 *  - /proc/[pid]/stat, /proc/[pid]/status 파싱 기반 CPU/메모리 갱신.
 *  - 성공 시 0, 실패 시 -1.
 *  - 실패 시 호출부(refresh_all_processes)는 cpu_usage=0.0, mem_usage_kb=0으로 처리하고
 *    status는 기존 값을 유지한다.
 *
 * [5단계 구현 노트]
 *  - mem_usage_kb: /proc/[pid]/status의 VmRSS 값을 그대로 사용한다.
 *  - cpu_usage: /proc/[pid]/stat의 utime+stime(틱)을 sysconf(_SC_CLK_TCK)로 초 단위 변환한 뒤,
 *    "프로세스 시작 이후 경과 시간(start_time 기준)" 대비 비율(%)로 계산한다.
 *    즉 top(1)의 순간 CPU%(직전 샘플링 구간 델타)가 아니라 "시작 이후 누적 평균 CPU 점유율"이다.
 *    순간 사용률을 계산하려면 이전 측정 시점의 utime+stime과 timestamp를 별도로 저장해야 하는데,
 *    01_상세기능명세서.md 3-1의 ProcessInfo 필드 명세에는 그런 필드가 없어 구조체를 임의로
 *    늘리지 않기 위해 누적 평균 방식으로 단순화했다(팀 합의 시 델타 방식으로 교체 가능).
 */
int update_process_stats(ProcessInfo *proc);

#endif /* SYSTEM_INFO_H */
