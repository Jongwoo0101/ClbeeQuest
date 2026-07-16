#ifndef TUK_SHELL_H
#define TUK_SHELL_H

/* 입력/토큰 제약 (02_인터페이스명세서.md 2-1) */
#define MAX_INPUT_LENGTH 1024
#define MAX_ARGS 64

/* TU Color (ANSI True Color) - 05_개발철칙명세서.md 4-1 */
#define TU_BLUE      "\x1b[38;2;23;88;168m"   /* #1758A8 : 프롬프트/강조 */
#define TU_SKY_BLUE  "\x1b[38;2;6;143;211m"   /* #068FD3 : 시스템/작업 메시지 */
#define TU_MINT      "\x1b[38;2;1;179;205m"   /* #01B3CD : 캠퍼스 명령어 결과 */
#define COLOR_RESET  "\x1b[0m"

#endif /* TUK_SHELL_H */
