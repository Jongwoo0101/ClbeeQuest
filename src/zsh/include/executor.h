#ifndef EXECUTOR_H
#define EXECUTOR_H

/*
 * execute_command
 *  - parse_command로 얻은 argc/argv/background_flag를 받아
 *    내장/외부 명령을 분기 실행한다 (03_파이프라인명세서.md 4-1).
 *  - should_exit : exit 명령이 실행되면 1로 설정.
 *
 * 반환값: 0 정상, 1 사용법(또는 백그라운드 금지) 오류, -1 치명적 오류
 */
int execute_command(int argc, char **argv, int background_flag, int *should_exit);

#endif /* EXECUTOR_H */
