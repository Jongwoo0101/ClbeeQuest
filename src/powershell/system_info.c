#include "system_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int update_process_stats(ProcessInfo *proc) {
    // 유효성 검증
    if (!proc || proc->pid <= 0) {
        if (proc) {
            proc->cpu_usage = 0.0;
            proc->mem_usage_kb = 0;
        }
        return -1;
    }

    char path[256];
    char line[512];
    FILE *fp;

    // 실패를 대비한 기본값 초기화
    proc->cpu_usage = 0.0;
    proc->mem_usage_kb = 0;

    // ==========================================
    // 1. 메모리 사용량 (VmRSS) 파싱
    // ==========================================
    snprintf(path, sizeof(path), "/proc/%d/status", proc->pid);
    fp = fopen(path, "r");
    if (!fp) {
        // 이미 종료되었거나 권한 부족 (exit 호출 금지)
        return -1; 
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %ld kB", &proc->mem_usage_kb);
            break; // 값을 찾으면 즉시 루프 탈출
        }
    }
    fclose(fp); // 파일 디스크립터 반환

    // ==========================================
    // 2. CPU 사용량 계산을 위한 uptime 읽기
    // ==========================================
    double system_uptime = 0.0;
    fp = fopen("/proc/uptime", "r");
    if (!fp) return -1;
    
    if (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%lf", &system_uptime);
    }
    fclose(fp);

    // ==========================================
    // 3. 프로세스 CPU 통계 (stat) 파싱
    // ==========================================
    snprintf(path, sizeof(path), "/proc/%d/stat", proc->pid);
    fp = fopen(path, "r");
    if (!fp) return -1;

    if (fgets(line, sizeof(line), fp)) {
        // 프로세스명(comm)에 공백이나 ')'가 있을 수 있으므로, 
        // 마지막 ')' 문자를 찾아 그 이후부터 파싱하는 것이 가장 안전합니다.
        char *p = strrchr(line, ')');
        if (p) {
            p += 2; // ')'와 그 뒤의 공백 건너뛰기 (이제 p는 3번째 필드인 상태코드부터 시작)

            unsigned long utime = 0, stime = 0;
            unsigned long long starttime = 0;

            // 3번(state)부터 22번(starttime) 필드까지 차례대로 파싱
            // 필요한 utime(14), stime(15), starttime(22)만 변수에 담고 나머지는 무시(%* 처리)
            sscanf(p,
                   "%*c "       // 3: state
                   "%*d "       // 4: ppid
                   "%*d "       // 5: pgrp
                   "%*d "       // 6: session
                   "%*d "       // 7: tty_nr
                   "%*d "       // 8: tpgid
                   "%*u "       // 9: flags
                   "%*lu "      // 10: minflt
                   "%*lu "      // 11: cminflt
                   "%*lu "      // 12: majflt
                   "%*lu "      // 13: cmajflt
                   "%lu "       // 14: utime
                   "%lu "       // 15: stime
                   "%*ld "      // 16: cutime
                   "%*ld "      // 17: cstime
                   "%*ld "      // 18: priority
                   "%*ld "      // 19: nice
                   "%*ld "      // 20: num_threads
                   "%*ld "      // 21: itrealvalue
                   "%llu",      // 22: starttime
                   &utime, &stime, &starttime);

            long clk_tck = sysconf(_SC_CLK_TCK);
            if (clk_tck > 0) {
                // HZ(clk_tck) 단위의 시간을 초 단위로 변환
                double total_time_sec = (double)(utime + stime) / clk_tck;
                double start_time_sec = (double)starttime / clk_tck;
                
                // 프로세스가 실행된 총 시간(초)
                double elapsed_seconds = system_uptime - start_time_sec;

                if (elapsed_seconds > 0.0) {
                    proc->cpu_usage = (total_time_sec / elapsed_seconds) * 100.0;
                }
            }
        }
    }
    fclose(fp); // 파일 디스크립터 반환

    return 0; // 정상 완료
}