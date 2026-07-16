#include <stdio.h>
#include <string.h>
#include "tuk_shell.h"
#include "parser.h"
#include "executor.h"
#include "process.h"

int main(void)
{
    char line[MAX_INPUT_LENGTH];
    char raw_line[MAX_INPUT_LENGTH];
    char *argv[MAX_ARGS + 1];
    int argc;
    int background_flag;
    int should_exit;
    size_t len;
    ProcessInfo *job_list = NULL; /* 2단계: 백그라운드 작업 연결 리스트 헤드 */

    printf(TU_BLUE "TUK-Shell (ZSH Part) - 5단계: /proc 연동 (실제 CPU/메모리 정보)\n" COLOR_RESET);

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

        /* [3]/[4] 개행 제거 */
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        /* 원본 문자열 보관: parse_command가 line을 strtok으로 파괴하기 전에 복사
           (01_상세기능명세서.md 4-2 순서 4, ProcessInfo->command에 사용) */
        strncpy(raw_line, line, sizeof(raw_line) - 1);
        raw_line[sizeof(raw_line) - 1] = '\0';

        /* [5],[6] 토큰화 + '&' 검사 */
        argc = parse_command(line, argv, &background_flag);
        if (argc == 0) {
            continue; /* 빈 줄 */
        }
        if (argc < 0) {
            continue; /* 파싱 오류: 메시지는 parse_command에서 이미 출력됨 */
        }

        /* 03문서 4-4 메시지 포맷과 일치시키기 위해 raw_line에서 '&' 제거 */
        strip_background_marker(raw_line, background_flag);

        execute_command(argc, argv, background_flag, raw_line, &should_exit, &job_list);

        if (should_exit) {
            break;
        }
    }

    /* 종료 파이프라인: 리스트 노드 -> (히스토리는 6단계 예정) -> 기타 버퍼 순 (03문서 9장) */
    free_process_list(&job_list);

    printf(TU_BLUE "TUK-Shell을 종료합니다.\n" COLOR_RESET);
    return 0;
}
