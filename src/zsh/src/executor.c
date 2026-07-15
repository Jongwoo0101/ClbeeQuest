#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "executor.h"
#include "commands.h"
#include "process.h"
#include "tuk_shell.h"

/*
 * execute_external
 *  - fork() 후 자식은 execvp()로 프로그램 교체.
 *  - execvp 실패 시 자식은 perror + exit(1)만 하고, 부모 자원(리스트/히스토리 등)에는
 *    절대 접근하지 않는다 (05_개발철칙명세서.md 3-2 "자식 프로세스 에러 전파 차단").
 *  - 백그라운드 실행 시 ProcessInfo 노드를 생성해 job_list에 등록한다 (2단계).
 */
static int execute_external(char **argv, int background_flag, const char *raw_command,
                             ProcessInfo **job_list)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        perror("fork"); /* 05문서 3-1 */
        return -1;
    }

    if (pid == 0) {
        /* --- 자식 프로세스 --- */
        if (execvp(argv[0], argv) == -1) {
            perror(argv[0]);
            exit(1); /* 부모가 waitpid로 종료코드 회수 가능하도록 즉시 종료 */
        }
        /* execvp 성공 시 이 지점에 도달하지 않음 */
    }

    /* --- 부모 프로세스 --- */
    if (background_flag) {
        ProcessInfo *node = create_process_node(pid, argv, raw_command);

        if (node == NULL) {
            /* 노드 생성 실패해도 자식 프로세스 자체는 이미 실행 중 - 등록만 실패 처리 */
            fprintf(stderr, "warning: failed to register background job (pid=%d)\n", (int)pid);
        } else if (append_process(job_list, node) != 0) {
            fprintf(stderr, "warning: failed to append background job (pid=%d)\n", (int)pid);
            free(node); /* 리스트에 들어가지 못한 노드는 즉시 해제 (누수 방지) */
        }

        /* 시작 메시지 형식 - 03문서 4-4, 색상 TU SKY BLUE */
        printf(TU_SKY_BLUE "[bg] pid=%d command=\"%s\"\n" COLOR_RESET,
               (int)pid, raw_command);
        return 0;
    }

    /* 포그라운드: 자식 종료까지 대기 후 REPL 복귀 (03문서 4-3) */
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }

    return 0;
}

int execute_command(int argc, char **argv, int background_flag, const char *raw_command,
                     int *should_exit, ProcessInfo **job_list)
{
    *should_exit = 0;

    if (argc <= 0 || argv[0] == NULL) {
        return 0;
    }

    if (is_builtin(argv[0])) {
        /*
         * 백그라운드 금지 내장 명령어 (01문서 5-3): 현재 인식되는 cd/pwd/help/exit
         * 전부 금지 목록에 포함된다. jobs/top이 추가되어도 is_builtin()만
         * 확장하면 되므로 이 로직은 변경할 필요가 없다.
         */
        if (background_flag) {
            fprintf(stderr, "built-in command cannot run in background\n");
            return 1;
        }
        return execute_builtin(argc, argv, should_exit, job_list);
    }

    return execute_external(argv, background_flag, raw_command, job_list);
}
