#ifndef HISTORY_H
#define HISTORY_H

#include <stdio.h>

#define HISTORY_FILE_NAME ".tuk_history"

/*
 * HistoryContext
 *  - entries : malloc/realloc로 관리되는 문자열 포인터 배열 (세션 내 히스토리 메모리 버퍼)
 *  - count   : 현재 저장된 항목 수
 *  - capacity: entries 배열의 할당 크기
 *  - fp      : .tuk_history 파일 스트림. 열기 실패 시 NULL.
 *  - enabled : 파일 열기에 성공했을 때만 1. 실패하면 0 (히스토리 기능 전체 비활성화).
 */
typedef struct {
    char **entries;
    int count;
    int capacity;
    FILE *fp;
    int enabled;
} HistoryContext;

/*
 * history_init
 *  - 현재 디렉토리의 .tuk_history 파일을 "a+" 모드로 연다(없으면 생성).
 *  - 파일이 이미 존재하면 전체 내용을 한 줄씩 읽어 메모리 버퍼(ctx->entries)에 로드한다
 *    (01문서 8-2, 03문서 7-2).
 *  - 파일 열기 실패 시: perror("history") 출력 후 ctx->enabled=0으로 남기고 반환한다.
 *    이 실패가 쉘 전체 종료로 이어지지 않는다 (01문서 8-3, 05문서 3-2).
 */
void history_init(HistoryContext *ctx);

/*
 * history_add
 *  - line이 빈 문자열이면 아무 것도 하지 않는다 (01문서 8-1 "빈 줄은 저장하지 않는다").
 *  - 히스토리가 비활성화 상태(ctx->enabled==0)면 아무 것도 하지 않는다.
 *  - 파일에 append 후 fflush(), 그리고 메모리 버퍼에도 추가한다 (03문서 7-1).
 *  - 파일 쓰기 실패: perror 출력 후에도 현재 명령 실행 자체는 계속 진행되어야 하므로
 *    이 함수는 실패해도 쉘을 중단시키지 않는다 (03문서 7-3).
 *  - 메모리 버퍼 확장(realloc) 실패 시에도 파일 저장은 이미 끝난 상태이므로 경고만 출력한다
 *    (03문서 7-3 "메모리 버퍼 추가 실패: 파일 저장만 유지").
 */
void history_add(HistoryContext *ctx, const char *line);

/*
 * history_close
 *  - 파일 스트림을 fflush 후 fclose (05_개발철칙명세서.md 2-2).
 *  - 메모리 버퍼에 malloc된 모든 문자열과 배열 자체를 free (05문서 2-1).
 */
void history_close(HistoryContext *ctx);

#endif /* HISTORY_H */
