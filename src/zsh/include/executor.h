#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "process.h"

/*
 * execute_command
 *  - parse_command로 얻은 argc/argv/background_flag를 받아
 *    내장/외부 명령을 분기 실행한다 (03_파이프라인명세서.md 4-1).
 *  - raw_command  : 사용자가 입력한 원본 명령 문자열(& 제거 전). 백그라운드 등록 시
 *                   ProcessInfo->command 필드 채우는 용도로 사용된다.
 *  - should_exit  : exit 명령이 실행되면 1로 설정.
 *  - job_list     : 백그라운드 작업 연결 리스트의 헤드 포인터 주소. 백그라운드 실행 시
 *                   create_process_node() + append_process()로 등록된다.
 *
 * 반환값: 0 정상, 1 사용법(또는 백그라운드 금지) 오류, -1 치명적 오류
 */
int execute_command(int argc, char **argv, int background_flag, const char *raw_command,
                     int *should_exit, ProcessInfo **job_list);

#endif /* EXECUTOR_H */
