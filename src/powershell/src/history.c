/*
 * history.c - .tuk_history 파일 입출력 및 탐색용 메모리 버퍼 [우진]
 *
 * 근거 문서:
 *  - 01_상세기능명세서 8장, 03_파이프라인명세서 7장, 05_개발철칙명세서 2-2/3-2
 *  - termios 기반 방향키 탐색은 선택 기능(01 8-4)으로 미구현.
 *    파일 저장/로드 및 메모리 버퍼 적재는 필수 구현으로 유지한다.
 */
#include "history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "platform.h"

#define HISTORY_FILE ".tuk_history"
#define HISTORY_INITIAL_CAPACITY 16

/* fclose 시점: history_close() - 쉘 종료 파이프라인 (05 2-2) */
static FILE *g_history_fp = NULL;

/* 각 원소는 strdup으로 할당. free 시점: history_close() (05 2-1) */
static char **g_history_entries = NULL;
static size_t g_history_count = 0;
static size_t g_history_capacity = 0;

/*
 * 메모리 버퍼에 한 줄 추가. 실패해도 파일 저장은 유지된다. (03 7-3)
 */
static void history_buffer_add(const char *line)
{
    if (g_history_count == g_history_capacity) {
        size_t new_capacity = (g_history_capacity == 0)
                                  ? HISTORY_INITIAL_CAPACITY
                                  : g_history_capacity * 2;
        /* free 시점: history_close()에서 배열 자체를 해제 */
        char **grown =
            realloc(g_history_entries, new_capacity * sizeof(char *));
        if (grown == NULL) {
            perror("history");
            return; /* 버퍼 추가 실패: 파일 저장만 유지 (03 7-3) */
        }
        g_history_entries = grown;
        g_history_capacity = new_capacity;
    }

    /* free 시점: history_close()에서 원소 단위로 해제 */
    char *copy = tuk_strdup(line);
    if (copy == NULL) {
        perror("history");
        return;
    }
    g_history_entries[g_history_count++] = copy;
}

int history_init(void)
{
    /* 01 8-2: 파일이 없으면 생성 후 계속 진행 ("a+"가 생성을 보장) */
    g_history_fp = fopen(HISTORY_FILE, "a+");
    if (g_history_fp == NULL) {
        /* 01 8-3: 열기 실패 - 히스토리 기능만 비활성화, 쉘은 계속 */
        perror("history");
        return -1;
    }

    /* 03 7-2: 시작 시 전체 이력을 한 줄씩 메모리 버퍼에 적재 */
    rewind(g_history_fp);
    char line[MAX_INPUT_LENGTH];
    while (fgets(line, sizeof(line), g_history_fp) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] != '\0') {
            history_buffer_add(line);
        }
    }
    if (ferror(g_history_fp)) {
        perror("history");
    }

    /* a+ 스트림에서 읽기 -> 쓰기 전환 전 위치 재설정 (C 표준 요구사항) */
    if (fseek(g_history_fp, 0L, SEEK_END) != 0) {
        perror("history");
    }
    return 0;
}

void history_add(const char *line)
{
    if (g_history_fp == NULL) {
        return; /* 초기화 실패로 비활성화된 상태 (05 3-2 장애 격리) */
    }
    if (line == NULL || line[0] == '\0') {
        return; /* 01 8-1: 빈 줄은 저장하지 않는다 */
    }

    /* 03 7-1: append -> fflush -> 메모리 버퍼 추가 순서 */
    if (fprintf(g_history_fp, "%s\n", line) < 0) {
        perror("history"); /* 쓰기 실패해도 현재 명령 실행은 계속 (03 7-3) */
    } else if (fflush(g_history_fp) == EOF) {
        perror("history");
    }
    history_buffer_add(line);
}

void history_close(void)
{
    if (g_history_fp != NULL) {
        /* 05 2-2: 쓰기 버퍼의 물리적 기록(fflush 포함) 보장 후 fclose */
        if (fflush(g_history_fp) == EOF) {
            perror("history");
        }
        if (fclose(g_history_fp) == EOF) {
            perror("history");
        }
        g_history_fp = NULL;
    }

    /* free 시점: history_init()/history_add()가 할당한 버퍼 전체 해제 (03 9장) */
    for (size_t i = 0; i < g_history_count; i++) {
        free(g_history_entries[i]);
    }
    free(g_history_entries);
    g_history_entries = NULL;
    g_history_count = 0;
    g_history_capacity = 0;
}
