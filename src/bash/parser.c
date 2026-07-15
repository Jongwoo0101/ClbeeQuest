/*
 * parser.c - REPL 입력 수신 및 명령어 파싱 [우진]
 *
 * 근거 문서:
 *  - 01_상세기능명세서 4장 (입력 처리 순서, 입력 제약)
 *  - 02_인터페이스명세서 2장 (토큰화 규칙, '&' 규약)
 *  - 03_파이프라인명세서 3장 (입력 예외 처리)
 */
#include "parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_prompt(void)
{
    /* 01 4-1: 고정 프롬프트 + TU BLUE */
    printf(TUK_COLOR_BLUE "TUK-OS > " TUK_COLOR_RESET);
    if (fflush(stdout) == EOF) {
        perror("prompt");
    }
}

int read_input_line(char *buffer, size_t size)
{
    if (fgets(buffer, (int)size, stdin) == NULL) {
        if (ferror(stdin)) {
            perror("read");
        }
        return READ_EOF; /* EOF -> 종료 파이프라인으로 이동 (03 3-1) */
    }

    char *newline = strchr(buffer, '\n');
    if (newline != NULL) {
        *newline = '\0'; /* 01 4-2 [3]: 개행 문자 제거 */
        return READ_OK;
    }
    if (feof(stdin)) {
        return READ_OK; /* 개행 없이 끝난 마지막 줄은 정상 처리 */
    }

    /* 03 3-2: 입력 길이 초과 - 남은 버퍼를 비우고 오류 출력 후 실행 취소 */
    int ch = 0;
    while ((ch = getchar()) != '\n' && ch != EOF) {
        /* 잔여 입력 폐기 */
    }
    fprintf(stderr, "tuk-shell: input too long (max %d characters)\n",
            MAX_INPUT_LENGTH - 1);
    return READ_OVERFLOW;
}

int parse_command(char *line, char **argv, int *background_flag)
{
    *background_flag = 0;

    /* 01 4-3: 따옴표/리다이렉션/파이프는 미지원 - 명시적 오류 반환 */
    for (const char *p = line; *p != '\0'; p++) {
        if (strchr("\"'|<>", *p) != NULL) {
            fprintf(stderr, "tuk-shell: unsupported syntax '%c'\n", *p);
            return -1;
        }
    }

    /* 02 2-2: 공백/탭 구분 토큰화, argv[argc] = NULL 규약 준수 */
    int argc = 0;
    char *token = strtok(line, " \t");
    while (token != NULL) {
        if (argc >= MAX_ARGS) {
            /* 01 4-3: 토큰 수 초과 시 잘라서 실행하지 않고 오류 출력 */
            fprintf(stderr, "tuk-shell: too many arguments (max %d)\n",
                    MAX_ARGS);
            return -1;
        }
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    argv[argc] = NULL;

    if (argc == 0) {
        return 0; /* 빈 줄 또는 공백만 있는 줄은 실행 없이 복귀 (03 3-2) */
    }

    /* 02 2-2: 마지막 독립 토큰 '&'만 백그라운드 플래그로 인정.
     * "sleep&"처럼 붙은 형태나 중간 위치 '&'는 미지원 오류로 처리한다. */
    for (int i = 0; i < argc; i++) {
        if (strchr(argv[i], '&') == NULL) {
            continue;
        }
        if (strcmp(argv[i], "&") != 0 || i != argc - 1) {
            fprintf(stderr,
                    "tuk-shell: unsupported '&' usage (use: command args &)\n");
            return -1;
        }
        *background_flag = 1;
        argv[--argc] = NULL; /* 01 4-2 [6]: '&' 토큰 제거 */
    }

    if (argc == 0) {
        fprintf(stderr, "tuk-shell: missing command before '&'\n");
        return -1;
    }
    return argc;
}

int parse_positive_long(const char *text, long *out)
{
    if (text == NULL || out == NULL || *text == '\0') {
        return -1;
    }
    errno = 0;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0) {
        return -1; /* 01 6-3: "jobs -pid abc"처럼 숫자가 아니면 오류 처리 */
    }
    *out = value;
    return 0;
}
