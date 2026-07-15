# 🧑‍💻 이우진 (MacBook / PM) — Shell Core & REPL 파트

> 마감: 7월 17일(목) 오후 2시
> 환경: MacBook (POSIX 호환, 개발/테스트 무리 없음)

## 담당 모듈
- `main.c`
- `parser.c`
- `builtin.c`
- `Makefile` (전체 빌드 관리)

## ⚠️ 최우선 작업 (오늘, 7/15 즉시)
다른 두 팀원(원종우, 이동현)의 개발이 이우진의 파싱 결과물에 의존하므로,
**본격 구현 전에 인터페이스(헤더/구조체)부터 확정해서 GitHub에 올려야 합니다.**

```c
// parser.h (예시 - 오늘 가장 먼저 push할 것)
typedef struct {
    char *args[MAX_ARGS];
    int   argc;
    int   is_background;   // '&' 여부
    // 파이프(|) 지원 시 관련 필드 추가
} Command;

Command parse_input(char *line);
```

Makefile 뼈대도 함께 올려서 팀원들이 로컬에서 바로 빌드할 수 있게 할 것.

## 세부 업무

1. **REPL 루프**: `TUK-OS >` 프롬프트 출력 및 사용자 입력(문자열) 처리
2. **명령어 파싱**: 입력 문자열을 파싱해 인자(arguments) 분리, 파이프(`|`)·백그라운드 기호(`&`) 식별
3. **기본 내장 명령어**: `cd`, `pwd`, `exit`, `help` 등 프로세스 생성 없이 쉘 내부에서 처리되는 명령어 구현
4. **히스토리 관리**: 입력 명령어 히스토리 저장 및 `.tuk_history` 파일에서 불러오기
5. **PM 역할**:
   - 전체 `Makefile` 작성
   - GitHub에서 팀원 코드 취합 및 병합(Merge)
   - 결과보고서 초안 작성 리드

## 타임라인

| 시점 | 작업 |
|---|---|
| 7/15 (오늘) | `parser.h`/`Makefile` 우선 push → parser 구현 → builtin(cd, pwd, exit) |
| 7/16 오전 | REPL 루프 + 히스토리 기능 완성 |
| 7/16 오후 | **PM으로서 코드 병합 시작**, 충돌 해결 |
| 7/16 저녁 | 발표자료 — **전체 구조** 파트 작성 |
| 7/17 오전 | 최종 통합 테스트, 제출본 정리 |

## 장점
POSIX 호환 환경인 Mac에서 무리 없이 개발 및 테스트 가능.
