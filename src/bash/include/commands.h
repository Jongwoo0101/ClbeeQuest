#ifndef TUK_COMMANDS_H
#define TUK_COMMANDS_H

#include "tuk_shell.h" /* TUK_SHELL_EXIT */

/*
 * commands.h - 내장 명령어 핸들러 및 명령 분기 인터페이스 [우진]
 *
 * 근거 문서:
 *  - 01_상세기능명세서 5~7장 (내장 명령어, jobs 검색, top 정렬)
 *  - 02_인터페이스명세서 4-1, 4-2 (함수 시그니처 고정)
 *
 * execute_command() 반환 규약 (02 4-1):
 *  - 정상 처리 0 / 사용법 오류 1 / 치명적 오류 -1
 *  - TUK_SHELL_EXIT(2)는 본 파트의 확장 규약(tuk_shell.h)으로, exit 내장
 *    명령이 정상 수리되어 REPL 루프를 종료해야 함을 main.c에 알린다.
 */

/*
 * 백그라운드 등록 시 ProcessInfo.command에 보관할 "사용자 입력 원본
 * 문자열"(01 3-1)을 전달한다. execute_command()의 고정 시그니처에는
 * 원본 문자열 인자가 없으므로 REPL 루프가 매 명령마다 설정한다.
 */
void commands_set_raw_line(const char *raw_line);

/* 02 4-1: 내장 테이블 -> 캠퍼스 명령 -> 외부 명령 순으로 분기 (02 2-3) */
int execute_command(int argc, char **argv, int background_flag);

/* 02 4-2: 내장 명령어 핸들러 (시그니처 고정) */
int handle_cd(int argc, char **argv);
int handle_pwd(int argc, char **argv);
int handle_help(int argc, char **argv);
int handle_exit(int argc, char **argv);
int handle_jobs(int argc, char **argv);
int handle_top(int argc, char **argv);

#endif /* TUK_COMMANDS_H */
