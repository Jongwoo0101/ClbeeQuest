#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "history.h"
#include "tuk_shell.h"

#define HISTORY_INITIAL_CAPACITY 64

/*
 * dup_string
 *  - strdup()은 표준 C11에 없고 POSIX 확장이라 -std=c11 빌드에서 선언 경고를 피하기 위해
 *    직접 구현한다.
 *  - 해제 시점: 호출부(history_append_to_buffer)가 실패 시 즉시 free하거나,
 *    성공 시 ctx->entries에 보관했다가 history_close()에서 일괄 free한다.
 */
static char *dup_string(const char *src)
{
    size_t len = strlen(src) + 1;
    char *copy = (char *)malloc(len);

    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, src, len);
    return copy;
}

/*
 * history_append_to_buffer
 *  - line을 복제하여 ctx->entries에 추가한다. 필요 시 realloc으로 배열을 2배 확장한다.
 *  - 성공 0, 실패 -1 (malloc/realloc 실패).
 *
 * [malloc 지점] dup_string()의 결과와 realloc된 entries 배열은
 * history_close()에서 세션 종료 시 일괄 free된다 (05문서 2-1).
 */
static int history_append_to_buffer(HistoryContext *ctx, const char *line)
{
    char *copy;

    if (ctx->count >= ctx->capacity) {
        int new_capacity = (ctx->capacity == 0) ? HISTORY_INITIAL_CAPACITY : ctx->capacity * 2;
        char **new_entries = (char **)realloc(ctx->entries, sizeof(char *) * (size_t)new_capacity);

        if (new_entries == NULL) {
            perror("malloc"); /* 03문서 7-3: 메모리 버퍼 추가 실패, 파일 저장은 이미 완료된 상태 */
            return -1;
        }
        ctx->entries = new_entries;
        ctx->capacity = new_capacity;
    }

    copy = dup_string(line);
    if (copy == NULL) {
        perror("malloc");
        return -1;
    }

    ctx->entries[ctx->count] = copy;
    ctx->count++;
    return 0;
}

void history_init(HistoryContext *ctx)
{
    char line_buf[MAX_INPUT_LENGTH];

    ctx->entries = NULL;
    ctx->count = 0;
    ctx->capacity = 0;
    ctx->fp = NULL;
    ctx->enabled = 0;

    /* "a+": 없으면 생성, 있으면 유지. 쓰기는 항상 파일 끝에, 읽기는 위치 지정 필요 */
    ctx->fp = fopen(HISTORY_FILE_NAME, "a+");
    if (ctx->fp == NULL) {
        perror("history"); /* 01문서 8-3: 실패해도 쉘 전체 종료로 이어지지 않음 */
        return; /* enabled=0 유지 -> 이후 history_add는 아무 동작 안 함 */
    }

    ctx->enabled = 1;

    /* 기존 히스토리 로드 - 01문서 8-2, 03문서 7-2 */
    rewind(ctx->fp);
    while (fgets(line_buf, sizeof(line_buf), ctx->fp) != NULL) {
        size_t len = strlen(line_buf);

        if (len > 0 && line_buf[len - 1] == '\n') {
            line_buf[len - 1] = '\0';
        }
        if (line_buf[0] != '\0') {
            /* 로드 단계에서 메모리 실패해도 파일 자체는 이미 존재하므로 쉘은 계속 진행 */
            history_append_to_buffer(ctx, line_buf);
        }
    }
}

void history_add(HistoryContext *ctx, const char *line)
{
    if (line == NULL || line[0] == '\0') {
        return; /* 빈 줄은 저장하지 않음 - 01문서 8-1 */
    }

    if (!ctx->enabled || ctx->fp == NULL) {
        return; /* 히스토리 비활성화 상태 - 01문서 8-3 */
    }

    if (fprintf(ctx->fp, "%s\n", line) < 0) {
        perror("history"); /* 03문서 7-3: 파일 쓰기 실패해도 현재 명령 실행은 계속 진행 */
    } else if (fflush(ctx->fp) != 0) {
        perror("history");
    }

    /* 메모리 버퍼 추가 실패는 파일 저장에 영향 주지 않음 (이미 위에서 완료됨) */
    history_append_to_buffer(ctx, line);
}

void history_close(HistoryContext *ctx)
{
    int i;

    if (ctx->fp != NULL) {
        fflush(ctx->fp); /* 05문서 2-2: 디스크에 물리적으로 기록되도록 보장 */
        fclose(ctx->fp);
        ctx->fp = NULL;
    }

    /* [free] 세션 종료 시 메모리 버퍼에 남아있던 모든 문자열과 배열 자체 해제 (05문서 2-1) */
    for (i = 0; i < ctx->count; i++) {
        free(ctx->entries[i]);
    }
    free(ctx->entries);
    ctx->entries = NULL;
    ctx->count = 0;
    ctx->capacity = 0;
}
