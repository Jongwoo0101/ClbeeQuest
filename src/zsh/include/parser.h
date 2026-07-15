#ifndef PARSER_H
#define PARSER_H

/*
 * parse_command
 *  - line : 원본 입력 문자열 (개행이 이미 제거된 상태). 내부에서 strtok으로
 *           직접 토큰화하므로 line 버퍼가 변형된다.
 *  - argv : 호출자가 크기 MAX_ARGS+1로 준비한 배열. 토큰 포인터(line 내부를 가리킴)로 채워지고
 *           마지막에 argv[argc] = NULL 규약을 지킨다.
 *  - background_flag : 출력 파라미터. 마지막 독립 토큰이 "&"이면 1, 아니면 0으로 설정.
 *                       "&"가 background_flag에 반영된 뒤에는 argv에서 제거된다.
 *
 * 반환값 (01_상세기능명세서.md 4-2, 02_인터페이스명세서.md 4-1):
 *    0 : 빈 입력(공백만 있는 줄 포함) - 실행하지 않고 다음 루프로
 *   >0 : 정상 파싱된 토큰 개수(argc)
 *   -1 : 파싱 오류 (예: MAX_ARGS 초과)
 */
int parse_command(char *line, char **argv, int *background_flag);

#endif /* PARSER_H */
