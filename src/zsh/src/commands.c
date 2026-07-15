#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "commands.h"
#include "tuk_shell.h"

/* 1단계 기준 내장 명령어 목록. jobs/top은 이후 단계에서 추가된다. */
static const char *BUILTIN_NAMES[] = { "cd", "pwd", "help", "exit" };
static const int BUILTIN_COUNT = 4;

int is_builtin(const char *cmd)
{
    int i;

    if (cmd == NULL) {
        return 0;
    }
    for (i = 0; i < BUILTIN_COUNT; i++) {
        if (strcmp(cmd, BUILTIN_NAMES[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int handle_cd(int argc, char **argv)
{
    const char *target;

    /* 인자 2개 이상 -> usage 출력 (01문서 5-2) */
    if (argc >= 3) {
        fprintf(stderr, "usage: cd [path]\n");
        return 1;
    }

    if (argc == 1) {
        /*
         * 팀 합의 사항(01문서 5-2 "인자 0개" 항목): 인자 없을 때는 HOME으로 이동.
         * 필요 시 이 부분을 미지원(오류 처리)으로 바꾸는 것도 가능하나,
         * 일반 쉘 관례를 따라 HOME 이동을 기본값으로 채택.
         */
        target = getenv("HOME");
        if (target == NULL) {
            fprintf(stderr, "cd: HOME environment variable not set\n");
            return 1;
        }
    } else {
        target = argv[1];
    }

    if (chdir(target) != 0) {
        perror("cd"); /* 05문서 3-1: 반환값 검증 + perror */
        return 0;     /* 실패해도 쉘 종료 아님, 현재 디렉토리 유지 */
    }

    return 0;
}

int handle_pwd(int argc, char **argv)
{
    char cwd[MAX_INPUT_LENGTH];
    (void)argv; /* 시그니처 통일을 위해 유지, 실제로는 사용하지 않음 */

    if (argc > 1) {
        fprintf(stderr, "usage: pwd\n");
        return 1;
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return 0;
    }

    printf("%s\n", cwd);
    return 0;
}

int handle_help(int argc, char **argv)
{
    (void)argv;

    if (argc > 1) {
        fprintf(stderr, "usage: help\n");
        return 1;
    }

    printf(TU_BLUE "TUK-Shell (ZSH Part) - 사용 가능한 명령어 [1단계]\n" COLOR_RESET);
    printf("  cd [path]      작업 디렉토리 변경 (인자 없으면 HOME 이동)\n");
    printf("  pwd            현재 작업 디렉토리 출력\n");
    printf("  help           이 도움말 출력\n");
    printf("  exit           쉘 종료\n");
    printf("  [cmd] [args]   외부 명령어 실행 (fork + execvp)\n");
    printf("  [cmd] [args] & 백그라운드 실행 시도 (완전한 작업 관리는 2단계에서 추가)\n");
    printf("\n"
           "※ jobs, top, schedule, bus, bob, notice, map, weather, contact 명령어는\n"
           "  plan.md 개발 순서에 따라 이후 단계에서 순차적으로 추가됩니다.\n");

    return 0;
}

int handle_exit(int argc, char **argv)
{
    (void)argv;

    if (argc > 1) {
        fprintf(stderr, "usage: exit\n");
        return 1;
    }
    /*
     * 실제 자원 정리(백그라운드 리스트 free, 히스토리 flush/close 등)는
     * 아직 도입되지 않은 자료구조(2단계)/기능(6단계)이므로,
     * 현재는 main.c의 REPL 루프 종료 신호 역할만 수행한다.
     */
    return 0;
}

int execute_builtin(int argc, char **argv, int *should_exit)
{
    *should_exit = 0;

    if (strcmp(argv[0], "cd") == 0) {
        return handle_cd(argc, argv);
    } else if (strcmp(argv[0], "pwd") == 0) {
        return handle_pwd(argc, argv);
    } else if (strcmp(argv[0], "help") == 0) {
        return handle_help(argc, argv);
    } else if (strcmp(argv[0], "exit") == 0) {
        int result = handle_exit(argc, argv);
        if (result == 0) {
            *should_exit = 1;
        }
        return result;
    }

    /* is_builtin() 확인 없이 호출된 방어적 상황 */
    fprintf(stderr, "error: unknown builtin command '%s'\n", argv[0]);
    return -1;
}
