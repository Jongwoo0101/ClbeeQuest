#ifndef TUK_PARSER_H
#define TUK_PARSER_H

#include <stddef.h>

#include "tuk_shell.h" /* MAX_INPUT_LENGTH, TUK_COLOR_*, READ_* 등 공용 상수 */

/*
 * parser.h - 입력 파싱 인터페이스 [우진]
 *
 * 근거 문서: 01_상세기능명세서 4장, 02_인터페이스명세서 2장,
 *           03_파이프라인명세서 3장
 * 공용 상수(입력 제약, TUK 색상, 반환 코드)는 tuk_shell.h로 이전했다.
 */

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
