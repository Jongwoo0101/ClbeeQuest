# 🧑‍💻 이우진 (MacBook / PM) — Shell Core & REPL + UI Launcher 파트

> 마감: 7월 17일(목) 오후 2시
> 환경: MacBook (POSIX 호환, 개발/테스트 무리 없음)

## 담당 모듈
- `main.c`
- `parser.c`
- `builtin.c`
- `launcher.sh` (8단계, UI 진입점)
- `Makefile` (전체 빌드 관리)

## ⚠️ 최우선 작업 (오늘, 7/15 즉시)
원종우·이동현의 코드가 결국 파싱 결과(구조체)를 입력으로 받으므로,
**본격 구현 전에 인터페이스(헤더/구조체)부터 확정해서 GitHub에 올려야 합니다.**

```c
// parser.h (오늘 가장 먼저 push할 것)
typedef struct {
    char *args[MAX_ARGS];
    int   argc;
    int   is_background;   // '&' 여부
    // 파이프(|) 지원 시 관련 필드 추가
} Command;

Command parse_input(char *line);
```

Makefile 뼈대도 함께 올려서 팀원들이 로컬에서 바로 빌드할 수 있게 할 것.

## 세부 업무 (명세 1단계 기준)

1. **REPL 루프**: `TUK-OS >` 프롬프트 출력 → 입력 파싱 → fork/exec로 외부 명령어 실행 → 결과 대기
2. **명령어 파싱**: 입력 문자열을 파싱해 인자(arguments) 분리, 파이프(`|`)·백그라운드 기호(`&`) 식별
3. **기본 내장 명령어**: `cd`, `pwd`, `exit`, `help` 구현
4. **jobs 명령어 파싱**: `jobs -pid [PID]`, `jobs -name [이름]` — 인자 파싱은 A가 담당하되, 실제 검색 로직은 B의 `list.c` 함수를 호출 (인터페이스 협의 필요)
5. **히스토리 관리**: 입력 명령어를 `.tuk_history` 파일에 저장/불러오기 (방향키 탐색은 termios 활용 선택사항)
6. **PM 역할**:
   - 전체 `Makefile` 작성
   - GitHub에서 팀원 코드 취합 및 병합(Merge)
   - 결과보고서 초안 작성 리드
   - 시스템콜 사용 시 에러 처리(반환값 체크) 및 free() 시점 주석 규칙을 팀 전체에 공지

## 8단계: 바탕화면 통합 투명 터미널 UI (Launcher) — A 담당

- `launcher.sh` 쉘 스크립트 작성 (C 런타임 바깥, 리눅스 GUI 제어)
- `wmctrl`, `xprop`, `gsettings` 등으로 터미널 실행 시 타이틀 바/창 테두리 제거
- 터미널 배경 투명 설정 → 바탕화면 위젯처럼 자연스럽게 융화
- TUK-Shell의 **진입점(Entry point)** 역할 → `main.c`를 소유한 A가 담당하는 것이 자연스러움
- 다른 모든 기능(B, C 파트)이 안정화된 마지막 단계에 적용 (7/16 오후~저녁 권장)

## 타임라인

| 시점 | 작업 |
|---|---|
| 7/15 (오늘) | `parser.h`/`Makefile` 우선 push → parser 구현 → builtin(cd, pwd, exit) |
| 7/16 오전 | REPL 루프 완성 + jobs 명령어 파싱(B와 연동) + 히스토리 기능 |
| 7/16 오후 | **PM으로서 코드 병합**, 충돌 해결 + `launcher.sh` 작성/적용(8단계) |
| 7/16 저녁 | 발표자료 — **전체 구조** 파트 작성 |
| 7/17 오전 | 최종 통합 테스트, 제출본 정리 |

## 장점
POSIX 호환 환경인 Mac에서 무리 없이 개발 및 테스트 가능.

## 다른 팀원과의 접점
- **원종우(B)**: `jobs -pid/-name` 검색은 A(파싱)+B(list.c 검색 로직) 협업 필요 → 함수 시그니처 사전 합의 권장
- **이동현(C)**: `&` 식별은 A가 parser에서 처리하지만, 실제 백그라운드 실행/좀비 프로세스 처리는 C의 `executor.c`가 담당 → 이 경계도 사전 합의 필요
