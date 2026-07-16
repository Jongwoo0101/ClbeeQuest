#ifndef TUK_SHELL_H
#define TUK_SHELL_H

/*
 * tuk_shell.h - 프로젝트 공용 우산(umbrella) 헤더 [우진]
 *
 * 05_개발철칙명세서 1-2 파일 구성에는 공용 헤더가 명시되어 있지 않으나,
 * zsh 파트 구조(include/tuk_shell.h)와 일관성을 맞추기 위해 도입한다.
 * 입력 규격 상수(02 2-1)와 TUK 색상 규격(plan.md UI/UX)처럼 여러 모듈이
 * 공유하는 매크로만 이곳에 모으고, 각 모듈의 함수 선언은 해당 모듈
 * 헤더(parser.h, process.h 등)에 그대로 둔다.
 */

/* 02_인터페이스명세서 2-1: 입력 버퍼 및 토큰 배열 제약 상수 */
#define MAX_INPUT_LENGTH 1024
#define MAX_ARGS 64

/* plan.md UI/UX: 한국공학대학교 핵심 색상 (ANSI True Color) */
#define TUK_COLOR_BLUE  "\x1b[38;2;23;88;168m"  /* TU BLUE     #1758A8 */
#define TUK_COLOR_SKY   "\x1b[38;2;6;143;211m"  /* TU SKY BLUE #068FD3 */
#define TUK_COLOR_MINT  "\x1b[38;2;1;179;205m"  /* TU MINT     #01B3CD */
#define TUK_COLOR_RESET "\x1b[0m"

/* read_input_line() 반환값 (parser.c) */
#define READ_OK 0
#define READ_EOF (-1)
#define READ_OVERFLOW 1

/* execute_command() 확장 반환값 - exit 내장 명령이 정상 수리되어
 * REPL 루프를 종료해야 함을 main.c에 알린다 (commands.c) */
#define TUK_SHELL_EXIT 2

#endif /* TUK_SHELL_H */
