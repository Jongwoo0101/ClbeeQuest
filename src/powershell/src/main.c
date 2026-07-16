/*
 * main.c - TUK-Shell 진입점 및 REPL 흐름 제어 [우진]
 *
 * 05_개발철칙명세서 1-1: 본 파일은 초기화 -> REPL 루프 -> 종료 정리의
 * 구조적 흐름만 제어하며 100줄을 초과하지 않는다. 실제 파싱/실행/자원
 * 관리는 parser, commands, process, history, campus, system_info로 분리.
 */
#include <stdio.h>

#include "commands.h"
#include "history.h"
#include "parser.h"
#include "process.h"
#include "tuk_shell.h" /* TUK_COLOR_SKY: 종료 알림 색상 */
#include "welcome.h"

/* 02_인터페이스명세서 4-1: REPL 루프. 쉘 종료 코드를 반환한다. */
int shell_loop(void)
{
    char line[MAX_INPUT_LENGTH];
    char raw_line[MAX_INPUT_LENGTH];
    char *argv[MAX_ARGS + 1];
    int running = 1;

    while (running) {
        int background_flag = 0;

        /* 03 5-1: REPL 사이클 시작 직전 백그라운드 상태 갱신/회수 */
        refresh_all_processes(&g_process_list);

        print_prompt();
        int read_result = read_input_line(line, sizeof(line));
        if (read_result == READ_EOF) { /* EOF -> 종료 파이프라인 (03 3-1) */
            putchar('\n');
            break;
        }
        if (read_result == READ_OVERFLOW) {
            continue; /* 길이 초과: 오류 출력 완료, 실행 취소 (03 3-2) */
        }

        /* 01 4-2 [4]: 파싱(strtok) 전에 히스토리 저장용 원본 보관 */
        snprintf(raw_line, sizeof(raw_line), "%s", line);

        int argc = parse_command(line, argv, &background_flag);
        if (argc <= 0) {
            continue; /* 빈 입력(0) 또는 파싱 오류(-1, 메시지 출력됨) */
        }

        /* 03 3-1 [7]: 명령 확정 직후 히스토리 파일/버퍼 동기화 */
        history_add(raw_line);

        commands_set_raw_line(raw_line);
        if (execute_command(argc, argv, background_flag) == TUK_SHELL_EXIT) {
            running = 0; /* exit 내장 명령 정상 수리 -> 루프 종료 */
        }
    }
    return 0;
}

int main(void)
{
    /* 가상 OS 부팅 시퀀스 및 로고 출력 */
    print_welcome_screen();

    /* 03 2-1 초기화: 리스트 헤드는 process.c에서 NULL로 시작.
     * 히스토리 실패 시 내부에서 경고만 출력하고 기능을 비활성화한다. */
    history_init();

    int exit_code = shell_loop();

    /* 02 7장 크로스쉘 정합: ZSH 파트와 동일한 종료 알림.
     * exit/EOF 양쪽 경로를 모두 커버하며 색상은 종료 알림용 TU SKY BLUE (05 4-1) */
    printf(TUK_COLOR_SKY "TUK-Shell을 종료합니다." TUK_COLOR_RESET "\n");

    /* 03 9장 종료 파이프라인: 리스트 노드 해제 -> 히스토리 flush/close */
    free_all_processes(&g_process_list);
    history_close();

    return exit_code;
}
