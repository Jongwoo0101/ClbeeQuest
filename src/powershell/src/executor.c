/*
 * executor.c - 플랫폼별 외부 명령 실행 [우진]
 *
 * 근거 문서:
 *  - 02_인터페이스명세서 3-4 (외부 명령 실행 규칙)
 *  - 03_파이프라인명세서 4장 (포그라운드/백그라운드 분기)
 *  - 05_개발철칙명세서 2-1 (free 시점 주석), 3-2 (자식 에러 전파 차단)
 */
#include "executor.h"

#include <stdio.h>
#include <stdlib.h>
#include "process.h"
#include "platform.h"
#include "tuk_shell.h"

int run_external_command(char **argv, int background_flag,
                         const char *raw_command)
{
    tuk_pid_t pid = 0;
    tuk_process_handle_t handle = 0;
    if (tuk_spawn(argv, &pid, &handle) != 0) {
        perror("spawn"); /* 03 10장: 자식 생성 실패 - 오류 출력 후 루프 복귀 */
        return -1;
    }

    if (!background_flag) {
        /* 03 4-3 포그라운드: 자식 종료 코드 수집 후 REPL 복귀 */
        TukWaitState state = TUK_WAIT_RUNNING;
        int exit_code = -1;
        if (tuk_wait_process(pid, handle, 0, &state, &exit_code) != 0) {
            perror("process wait");
        }
        tuk_close_process_handle(handle);
        return 0;
    }

    /* 03 4-3 백그라운드: ProcessInfo 생성 -> 리스트 등록 -> 즉시 복귀 */
    ProcessInfo *node = create_process_node(pid, handle, argv, raw_command);
    if (node == NULL) {
        tuk_close_process_handle(handle);
        return -1; /* 등록 실패: 자식은 실행 중이나 추적 목록에서만 제외 */
    }
    if (append_process(&g_process_list, node) != 0) {
        /* free 시점: 등록 실패 시 보관처가 없으므로 즉시 해제 */
        free(node);
        tuk_close_process_handle(handle);
        return -1;
    }
    /* 03 4-4: 시작 메시지 - TU SKY BLUE, 단색 터미널 대비 [bg] 표기 병행 */
    printf(TUK_COLOR_SKY "[bg] pid=%ld command=\"%s\"" TUK_COLOR_RESET "\n",
           (long)pid, node->command);
    return 0;
}
