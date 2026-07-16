#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <sys/types.h>

typedef struct {
    pid_t pid;
    long mem_usage_kb;
    double cpu_usage; // 퍼센트 단위 (예: 12.5)
} ProcessInfo;

// 프로세스 통계 업데이트 함수 (성공 시 0, 실패 시 -1 반환)
int update_process_stats(ProcessInfo *proc);

#endif // SYSTEM_INFO_H