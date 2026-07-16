#ifndef TUK_CAMPUS_H
#define TUK_CAMPUS_H

/*
 * campus.h - 캠퍼스 특화 커스텀 명령어 인터페이스
 *
 * 역할 분담 (01_상세기능명세서 9-1):
 *  - [우진]  명령어 진입점 식별, 옵션 파싱, 사용법/오류 처리, 핸들러 호출
 *  - [우진] handle_*_command 실제 구현 (notice.c / campus_data.c 실데이터)
 *
 * 핸들러 반환 규약 (01 9-4):
 *  - 성공 0 / 사용법 오류 1 / 데이터 로드 실패 -1
 *  - 출력 색상은 TU MINT (#01B3CD) 기본
 */

/* [우진] argv[0]이 캠퍼스 명령어이면 1, 아니면 0 */
int is_campus_command(const char *name);

/* [우진] 옵션 검증 후 대응 핸들러 호출. 반환 규약은 핸들러와 동일 */
int dispatch_campus_command(int argc, char **argv);

/* 캠퍼스 명령 핸들러 (01 9-4 인터페이스 고정, 모두 [우진] 실데이터 구현)
 * - notice   : notice.c      (학사공지 게시판 라이브 HTML)
 * - 그 외 6개: campus_data.c  (data/campus 디렉터리의 .txt) */
int handle_schedule_command(int argc, char **argv);
int handle_bus_command(int argc, char **argv);
int handle_bob_command(int argc, char **argv);
int handle_notice_command(int argc, char **argv);
int handle_map_command(int argc, char **argv);
int handle_weather_command(int argc, char **argv);
int handle_contact_command(int argc, char **argv);

#endif /* TUK_CAMPUS_H */
