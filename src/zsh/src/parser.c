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
 *
 * 이 함수는 동적 메모리를 사용하지 않는다(strtok은 line 버퍼를 직접 사용).
 * 따라서 별도의 free() 지점이 없다.
 */
int parse_command(char *line, char **argv, int *background_flag)
{
    int argc = 0;
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
