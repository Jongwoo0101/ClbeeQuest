#ifndef TUK_SYSTEM_INFO_H
#define TUK_SYSTEM_INFO_H

#include "process.h"

/*
 * system_info.h - /proc 기반 시스템 정보 수집 인터페이스 [우진]
 *
 * 01_상세기능명세서 3-4의 인터페이스 지점. 원래 동현(PowerShell 파트)
 * 소스 병합 대기(Mock)였으나, 동현의 /proc 파서를 기반으로 BASH 파트
 * (우진)가 system_info.c에 직접 이식·실구현했다.
 * 시그니처는 02 4-4와 동일하게 고정.
 *
 * 규약:
 *  - 성공 시 0 반환, 실패 시 -1 반환
 *  - 실패 시 호출부(refresh_all_processes)가 cpu_usage=0.0,
 *    mem_usage_kb=0으로 되돌리고 상태는 기존 값을 유지한다.
 *  - /proc/[pid] 접근 불가 시 호출부는 waitpid() 결과를 우선 신뢰한다.
 *  - TUK_PROC_ROOT 지정 시 /proc 대신 그 디렉터리만 사용(폴백 없음),
 *    미지정 && /proc 부재(비Linux 개발환경)면 결정적 더미 값으로 성공.
 */
int update_process_stats(ProcessInfo *proc);

#endif /* TUK_SYSTEM_INFO_H */
