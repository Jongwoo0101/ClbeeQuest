#include <stdio.h>
#include <string.h>
#include "parser.h"
#include "tuk_shell.h"

/*
 * 이번 단계(1단계) 구현 범위:
 *  - 공백/탭 기준 토큰화
 *  - 마지막 독립 토큰 "&" 검사 (붙어있는 "cmd&" 형태는 02_인터페이스명세서.md 2-2 기준
 *    미지원으로 간주 -> 하나의 토큰으로 취급되어 & 판정에서 제외됨)
 *  - MAX_ARGS 초과 시 오류 처리
 *  - 리다이렉션('>', '<'), 파이프('|'), 따옴표('"', '\'')는 01_상세기능명세서.md 4-3에 따라
 *    본 단계 범위 밖이므로 명시적 오류로 거부한다 (execvp에 리터럴 인자로 전달되어
 *    의도치 않은 동작을 일으키는 것을 방지 - 5단계 테스트에서 실제로 발견된 문제:
 *    "yes > /dev/null &" 를 그냥 통과시키면 yes가 ">"와 "/dev/null"을 인자로 받아
 *    무한 반복 출력하며 터미널을 뒤덮는 사고로 이어짐)
 *
 * 이 함수는 동적 메모리를 사용하지 않는다(strtok은 line 버퍼를 직접 사용).
 * 따라서 별도의 free() 지점이 없다.
 */

/* 토큰 안에 리다이렉션/파이프/따옴표 문자가 있는지 검사 */
static int has_unsupported_syntax(const char *token)
{
    for (; *token != '\0'; token++) {
        if (*token == '>' || *token == '<' || *token == '|' ||
            *token == '"' || *token == '\'') {
            return 1;
        }
    }
    return 0;
}

int parse_command(char *line, char **argv, int *background_flag)
{
    int argc = 0;
    int i;
    char *token;
    const char *delim = " \t";

    *background_flag = 0;

    /* 빈 줄(공백/탭만 있는 경우 포함) 처리 - 4-2 순서 2번 */
    token = strtok(line, delim);
    if (token == NULL) {
        return 0;
    }

    while (token != NULL) {
        if (argc >= MAX_ARGS) {
            /* 토큰 수 초과 시 잘라서 실행하지 않고 오류만 출력 - 01문서 4-3 */
            fprintf(stderr, "error: too many arguments (max %d)\n", MAX_ARGS);
            return -1;
        }
        argv[argc++] = token;
        token = strtok(NULL, delim);
    }
    argv[argc] = NULL; /* argv[argc] = NULL 규약 - 02문서 2-2 */

    /* 마지막 토큰이 "&"이면 백그라운드 플래그 설정 후 토큰 목록에서 제거 - 4-2 순서 6번 */
    if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
        *background_flag = 1;
        argv[argc - 1] = NULL;
        argc--;
    }

    /* 리다이렉션/파이프/따옴표 거부 - 01문서 4-3 */
    for (i = 0; i < argc; i++) {
        if (has_unsupported_syntax(argv[i])) {
            fprintf(stderr,
                    "error: redirection('>','<'), pipe('|'), quotes are not supported in this stage\n");
            return -1;
        }
    }

    return argc;
}

void strip_background_marker(char *raw_line, int background_flag)
{
    size_t len;

    if (!background_flag || raw_line == NULL) {
        return;
    }

    len = strlen(raw_line);
    while (len > 0 && (raw_line[len - 1] == ' ' || raw_line[len - 1] == '\t')) {
        raw_line[--len] = '\0';
    }
    if (len > 0 && raw_line[len - 1] == '&') {
        raw_line[--len] = '\0';
        while (len > 0 && (raw_line[len - 1] == ' ' || raw_line[len - 1] == '\t')) {
            raw_line[--len] = '\0';
        }
    }
}
