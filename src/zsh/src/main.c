#include <stdio.h>
#include <string.h>
#include "tuk_shell.h"
#include "parser.h"
#include "executor.h"

int main(void)
{
    char line[MAX_INPUT_LENGTH];
    char *argv[MAX_ARGS + 1];
    int argc;
    int background_flag;
    int should_exit;
    size_t len;

    printf(TU_BLUE "TUK-Shell (ZSH Part) - 1단계: 기본 REPL 루프\n" COLOR_RESET);

    while (1) {
        /* [1] 프롬프트 출력 - TU BLUE */
        printf(TU_BLUE "TUK-OS > " COLOR_RESET);
        fflush(stdout);

        /* [2] 한 줄 입력 수신 */
        if (fgets(line, sizeof(line), stdin) == NULL) {
            /* EOF(Ctrl+D) -> 종료 파이프라인 */
            printf("\n");
            break;
        }

        /* 입력 길이 초과 검사: 개행을 못 읽었다면 버퍼가 가득 찬 것 */
        len = strlen(line);
        if (len > 0 && line[len - 1] != '\n') {
            int c;
            fprintf(stderr, "error: input too long (max %d)\n", MAX_INPUT_LENGTH - 1);
            while ((c = getchar()) != '\n' && c != EOF) {
                /* 남은 입력 버퍼 비우기 */
            }
            continue;
        }

        /* [3]/[4] 개행 문자 제거 (원본 히스토리 보관은 6단계에서 추가) */
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        /* [5],[6] 토큰화 + '&' 검사 */
        argc = parse_command(line, argv, &background_flag);
        if (argc == 0) {
            continue; /* 빈 줄: 실행 없이 다음 루프 */
        }
        if (argc < 0) {
            continue; /* 파싱 오류: 메시지는 parse_command에서 이미 출력됨 */
        }

        /* 내장/외부 명령 분기 실행 */
        execute_command(argc, argv, background_flag, &should_exit);

        if (should_exit) {
            break;
        }
    }

    printf(TU_BLUE "TUK-Shell을 종료합니다.\n" COLOR_RESET);
    return 0;
}
