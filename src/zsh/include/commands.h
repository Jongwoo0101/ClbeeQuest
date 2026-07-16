#ifndef COMMANDS_H
#define COMMANDS_H

#include "process.h"

/* 개별 핸들러: 성공 0, 사용법 오류 1 (02_인터페이스명세서.md 4-2) */
int handle_cd(int argc, char **argv);
int handle_pwd(int argc, char **argv);
int handle_help(int argc, char **argv);
int handle_exit(int argc, char **argv);

/*
 * handle_jobs
 *  - jobs / jobs -pid [PID] / jobs -name [KEYWORD] 처리 (01_상세기능명세서.md 6장).
 *  - job_list: 백그라운드 작업 리스트의 헤드 포인터 주소. 호출 시 먼저
 *    refresh_all_processes()로 상태를 갱신한 뒤 조회한다 (03_파이프라인명세서.md 6-1).
 *  - 반환값: 0 정상(검색 결과 없음도 정상 0), 1 사용법/옵션 오류
 */
int handle_jobs(int argc, char **argv, ProcessInfo **job_list);

/*
 * handle_top
 *  - top -cpu / top -mem / top -time 처리 (01_상세기능명세서.md 7장).
 *  - 원본 연결 리스트는 훼손하지 않고, 출력용 배열로 복사한 뒤 qsort()로 정렬한다
 *    (01문서 7-3, 03_파이프라인명세서.md 6-2).
 *  - 반환값: 0 정상(활성 작업 0개도 정상), 1 사용법/옵션 오류, -1 치명적 오류(malloc 실패)
 */
int handle_top(int argc, char **argv, ProcessInfo **job_list);

/*
 * is_builtin
 *  - cmd가 내장 명령어 이름이면 1, 아니면 0.
 *  - 현재 cd/pwd/help/exit/jobs/top 인식 (4단계 기준).
 */
int is_builtin(const char *cmd);

/*
 * execute_builtin
 *  - argv[0]에 해당하는 내장 명령어 핸들러를 호출한다. 호출 전 is_builtin() 확인 전제.
 *  - should_exit : exit 명령이 정상 처리되면 1로 설정 (main 루프 종료 신호).
 *  - job_list    : jobs 명령이 리스트 조회를 위해 필요로 함.
 *  - 반환값: 0 정상, 1 사용법 오류, -1 치명적 오류
 */
int execute_builtin(int argc, char **argv, int *should_exit, ProcessInfo **job_list);

#endif /* COMMANDS_H */
