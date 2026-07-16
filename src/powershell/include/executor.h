#ifndef TUK_EXECUTOR_H
#define TUK_EXECUTOR_H

/*
 * executor.h - 외부 명령어 실행 (fork/execvp/waitpid) [우진]
 *
 * ProcessInfo 자료구조 정의/리스트 관리는 process.h가 담당하고,
 * 이 모듈은 그 리스트를 사용해 외부 명령을 실행/등록하는 책임만 분리했다.
 * (zsh 파트 구조와 일관성을 맞추기 위해 process.c에서 분리)
 *
 * 근거 문서: 02_인터페이스명세서 3-4, 03_파이프라인명세서 4장
 */

/*
 * 외부 명령 실행: fork() 후 자식은 execvp(), 포그라운드는
 * waitpid(...,0) 대기, 백그라운드는 리스트 등록 후 즉시 복귀.
 * 반환: 정상 0 / 치명적 오류(fork·등록 실패) -1
 */
int run_external_command(char **argv, int background_flag,
                         const char *raw_command);

#endif /* TUK_EXECUTOR_H */
