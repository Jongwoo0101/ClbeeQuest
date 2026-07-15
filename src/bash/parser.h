#ifndef TUK_PARSER_H
#define TUK_PARSER_H

#include <stddef.h>

/*
 * parser.h - 입력 파싱 인터페이스 및 프로젝트 공용 입출력 규격 상수 [우진]
 *
 * 05_개발철칙명세서 1-2의 파일 구성에는 별도 공용 헤더가 없으므로,
 * 입력 제약 상수(05 4-2)와 TUK 색상 규격(plan.md UI/UX)을 입력 규격의
 * 소유자인 parser.h에 정의하여 모든 모듈이 공유한다.
 */

/* 02_인터페이스명세서 2-1: 입력 버퍼 및 토큰 배열 제약 상수 */
#define MAX_INPUT_LENGTH 1024
#define MAX_ARGS 64

/* plan.md UI/UX: 한국공학대학교 핵심 색상 (ANSI True Color) */
#define TUK_COLOR_BLUE  "\x1b[38;2;23;88;168m"  /* TU BLUE     #1758A8 */
#define TUK_COLOR_SKY   "\x1b[38;2;6;143;211m"  /* TU SKY BLUE #068FD3 */
#define TUK_COLOR_MINT  "\x1b[38;2;1;179;205m"  /* TU MINT     #01B3CD */
#define TUK_COLOR_RESET "\x1b[0m"

/* read_input_line() 반환값 */
#define READ_OK 0
#define READ_EOF (-1)
#define READ_OVERFLOW 1

/* 01_상세기능명세서 4-1: TU BLUE 고정 프롬프트 "TUK-OS > " 출력 */
void print_prompt(void);

/*
 * 한 줄 입력을 읽는다.
 * 반환: READ_OK 정상 / READ_EOF 입력 종료 /
 *       READ_OVERFLOW 길이 초과(잔여 버퍼 폐기 및 오류 출력 완료, 03 3-2)
 */
int read_input_line(char *buffer, size_t size);

/*
 * 02_인터페이스명세서 4-1: 성공 시 argc 반환, 빈 입력 0, 파싱 오류 -1.
 * line을 공백/탭 기준으로 토큰화해 argv를 채우고(argv[argc]=NULL),
 * 마지막 독립 토큰 '&'만 background_flag로 인정한 뒤 제거한다. (02 2-2)
 */
int parse_command(char *line, char **argv, int *background_flag);

/*
 * 10진수 양의 정수 문자열 파싱 헬퍼 (jobs -pid, notice -n 공용).
 * 성공 시 0 반환 및 *out 설정, 형식 오류 시 -1 반환.
 */
int parse_positive_long(const char *text, long *out);

#endif /* TUK_PARSER_H */
