#ifndef CAMPUS_H
#define CAMPUS_H

/*
 * campus.h - 캠퍼스 특화 커스텀 명령어 (7단계)
 *
 * 근거 문서:
 *  - 01_상세기능명세서.md 9장 (책임 범위, 옵션 파싱 규칙, 핸들러 인터페이스)
 *  - 02_인터페이스명세서.md 3-3 (명령어별 사용 형식/인자 규칙), 6-2 (데이터 파일)
 *  - 03_파이프라인명세서.md 8장 (캠퍼스 명령어 파이프라인)
 *
 * 핸들러 반환 규약 (01문서 9-4):
 *  - 성공 0 / 사용법 오류 1 / 데이터 로드 실패 -1
 *  - 출력 색상은 TU MINT (#01B3CD) 기본
 *
 * 데이터 소스: plan.md 3-[6] "로컬 텍스트/JSON 파일을 파싱해서 출력"에 따라
 * data/campus/ 아래의 '|' 구분 텍스트 파일을 파싱한다. 추후 API 연동으로
 * 교체할 수 있도록 파일 접근은 campus.c 내부 static 함수로 캡슐화하고,
 * 아래 핸들러 인터페이스는 고정한다.
 */

/*
 * is_campus_command
 *  - cmd가 캠퍼스 명령어 이름이면 1, 아니면 0.
 *  - schedule/bus/bob/notice/map/weather/contact 인식.
 *  - is_builtin()과 별도로 두어 내장 명령어 목록(01문서 5-3 백그라운드 금지 목록)과
 *    캠퍼스 명령어 목록을 구분해서 관리한다.
 */
int is_campus_command(const char *cmd);

/*
 * dispatch_campus_command
 *  - argv[0]에 해당하는 캠퍼스 명령의 옵션을 검증한 뒤 핸들러를 호출한다.
 *    호출 전 is_campus_command() 확인 전제.
 *  - 반환값: 0 정상, 1 사용법/옵션 오류, -1 데이터 로드 실패
 */
int dispatch_campus_command(int argc, char **argv);

/* 01문서 9-4 핸들러 인터페이스 (시그니처 고정) */
int handle_schedule_command(int argc, char **argv);
int handle_bus_command(int argc, char **argv);
int handle_bob_command(int argc, char **argv);
int handle_notice_command(int argc, char **argv);
int handle_map_command(int argc, char **argv);
int handle_weather_command(int argc, char **argv);
int handle_contact_command(int argc, char **argv);

#endif /* CAMPUS_H */
