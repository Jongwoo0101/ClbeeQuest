#ifndef TUK_HISTORY_H
#define TUK_HISTORY_H

/*
 * history.h - .tuk_history 파일 입출력 및 메모리 버퍼 관리 [우진]
 *
 * 근거 문서:
 *  - 01_상세기능명세서 8장 (저장/로드 규칙, 장애 허용 범위)
 *  - 03_파이프라인명세서 7장 (쓰기/읽기 파이프라인)
 *  - 05_개발철칙명세서 2-2 (종료 시 fclose/fflush 보장), 3-2 (장애 격리)
 */

/*
 * 쉘 시작 시 .tuk_history를 열고 기존 이력을 메모리 버퍼로 로드한다.
 * 파일이 없으면 생성한다. 반환: 성공 0 / 실패 -1.
 * 실패 시 perror("history") 출력 후 히스토리 기능만 비활성화되며,
 * 쉘 전체 종료 사유가 되어서는 안 된다. (01 8-3)
 */
int history_init(void);

/*
 * 명령 확정 직후 원본 문자열 한 줄을 파일에 append + fflush 하고
 * 메모리 버퍼에도 추가한다. 빈 줄은 저장하지 않는다. (01 8-1, 03 7-1)
 * 쓰기/버퍼 실패는 내부에서 경고만 출력하고 명령 실행은 계속된다. (03 7-3)
 */
void history_add(const char *line);

/*
 * 쉘 종료 시 파일 스트림 fflush/fclose 및 메모리 버퍼 전체 해제. (03 9장)
 */
void history_close(void);

#endif /* TUK_HISTORY_H */
