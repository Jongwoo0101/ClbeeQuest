#include <stdio.h>
#include <string.h>
#include "tuk_shell.h"
#include "parser.h"
#include "executor.h"
#include "process.h"
#include "history.h"
#include "welcome.h"

int main(void)
{
    char line[MAX_INPUT_LENGTH];
    char raw_line[MAX_INPUT_LENGTH];     /* 히스토리 저장용: '&' 포함 원본 그대로 */
    char command_line[MAX_INPUT_LENGTH]; /* 실행/메시지용: '&' 제거본 (5단계와 동일) */
    char *argv[MAX_ARGS + 1];
    int argc;
    int background_flag;
    int should_exit;
    size_t len;
    ProcessInfo *job_list = NULL; /* 2단계: 백그라운드 작업 연결 리스트 헤드 */
    HistoryContext history;       /* 6단계: 히스토리 파일/메모리 버퍼 컨텍스트 */

    /* 가상 OS 부팅 시퀀스 및 로고 출력 */
    print_welcome_screen();

    history_init(&history); /* 03문서 2-1 [3]: .tuk_history 열기 및 기존 이력 로드 */

    while (1) {
        /* 백그라운드 상태 갱신: REPL 한 사이클 시작 직전 (03문서 5-1) */
        refresh_all_processes(&job_list);

        /* [1] 프롬프트 출력 - TU BLUE */
        printf(TU_BLUE "TUK-OS > " COLOR_RESET);
        fflush(stdout);

        /* [2] 한 줄 입력 수신 */
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break; /* EOF -> 종료 파이프라인 */
        }

        len = strlen(line);
        if (len > 0 && line[len - 1] != '\n') {
            int c;
            fprintf(stderr, "error: input too long (max %d)\n", MAX_INPUT_LENGTH - 1);
            while ((c = getchar()) != '\n' && c != EOF) {
                /* 남은 입력 버퍼 비우기 */
            }
            continue;
        }

        /* [3]/[4] 개행 제거 후 원본 문자열 보관 (히스토리용, 이후 수정하지 않음) */
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        strncpy(raw_line, line, sizeof(raw_line) - 1);
        raw_line[sizeof(raw_line) - 1] = '\0';
        strncpy(command_line, line, sizeof(command_line) - 1);
        command_line[sizeof(command_line) - 1] = '\0';

        /* [5],[6] 토큰화 + '&' 검사 (line 버퍼는 여기서 파괴됨) */
        argc = parse_command(line, argv, &background_flag);

        /* [7] 히스토리 동기화: 빈 줄이 아니면 파싱 성공 여부와 무관하게 원본 그대로 저장 */
        if (argc != 0) {
            history_add(&history, raw_line);
        }

        if (argc == 0) {
            continue; /* 빈 줄 */
        }
        if (argc < 0) {
            continue; /* 파싱 오류: 메시지는 parse_command에서 이미 출력됨 */
        }

        /* 03문서 4-4 메시지 포맷과 일치시키기 위해 command_line에서 '&' 제거 */
        strip_background_marker(command_line, background_flag);

        execute_command(argc, argv, background_flag, command_line, &should_exit, &job_list);

        if (should_exit) {
            break;
        }
    }

    /* 종료 파이프라인: 리스트 노드 -> 히스토리 -> 기타 버퍼 순 (03문서 9장) */
    free_process_list(&job_list);
    history_close(&history);

    printf(TU_BLUE "TUK-Shell을 종료합니다.\n" COLOR_RESET);
    return 0;
}