/*
 * executor.c - fork/execvp 기반 외부 명령 실행 [우진]
 *
 * 근거 문서:
 *  - 02_인터페이스명세서 3-4 (외부 명령 실행 규칙)
 *  - 03_파이프라인명세서 4장 (포그라운드/백그라운드 분기)
 *  - 05_개발철칙명세서 2-1 (free 시점 주석), 3-2 (자식 에러 전파 차단)
 */
#include "executor.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "process.h"
#include "tuk_shell.h"

int run_external_command(char **argv, int background_flag,
                         const char *raw_command)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork"); /* 03 10장: 자식 생성 실패 - 오류 출력 후 루프 복귀 */
        return -1;
    }

    if (pid == 0) {
        /* 자식 프로세스: 프로그램 교체. 실패 시 부모 자원(리스트/히스토리)을
         * 건드리지 않고 즉시 종료해 부모가 상태를 회수하게 한다. (05 3-2) */
        execvp(argv[0], argv);
        perror(argv[0]);
        exit(1);
    }

    if (!background_flag) {
        /* 03 4-3 포그라운드: 자식 종료 코드 수집 후 REPL 복귀 */
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
        }
        return 0;
    }

    /* 03 4-3 백그라운드: ProcessInfo 생성 -> 리스트 등록 -> 즉시 복귀 */
    ProcessInfo *node = create_process_node(pid, argv, raw_command);
    if (node == NULL) {
        return -1; /* 등록 실패: 자식은 실행 중이나 추적 목록에서만 제외 */
    }
    if (append_process(&g_process_list, node) != 0) {
        /* free 시점: 등록 실패 시 보관처가 없으므로 즉시 해제 */
        free(node);
        return -1;
    }
    /* 03 4-4: 시작 메시지 - TU SKY BLUE, 단색 터미널 대비 [bg] 표기 병행 */
    printf(TUK_COLOR_SKY "[bg] pid=%ld command=\"%s\"" TUK_COLOR_RESET "\n",
           (long)pid, node->command);
    return 0;
}
