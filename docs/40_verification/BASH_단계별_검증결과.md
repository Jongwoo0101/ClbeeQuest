# BASH 파트 단계별 검증 결과 (이우진)

> **기준 문서**: `docs/10_planning/plan.md` (개발 순서 1~7단계), `docs/40_verification/04_검증및추적명세서.md` (T01~T10)
> **대상 커밋 기준일**: 2026-07-15
> **빌드**: `make` (`gcc -std=c11 -Wall -Wextra -Werror -Iinclude`) — 경고 0, 링크 성공
> **자동 회귀**: T01~T10 포함 52개 항목 전부 PASS (`tests/run_tests.sh`)
> **메모리**: AddressSanitizer 실행 검증에서 use-after-free/overflow 없음

본 문서는 plan.md의 1~7단계와 04번 명세서의 최소 테스트 시나리오(T01~T10)를,
실제 TUK-Shell(BASH 빌드) 실행 출력으로 증빙한다. 아래 출력은 모두 실제
실행 결과에서 ANSI 색상 코드만 제거해 옮긴 것이다.

---

## 1단계: 기본 REPL 루프 (fork/execvp 외부 명령)

**검증 항목**: 프롬프트 반복 출력, 외부 명령 실행, 포그라운드 복귀, 없는 명령 오류 처리

```text
TUK-OS > pwd
/private/tmp
TUK-OS > echo hello TUK-Shell
hello TUK-Shell
TUK-OS > sle
sle: No such file or directory
TUK-OS > exit
TUK-Shell을 종료합니다.
```

- `pwd`, `echo` → 외부 명령이 `fork()`+`execvp()`로 실행되고 포그라운드 대기 후 복귀
- `sle`(오타) → 자식 `execvp` 실패 시 `perror(argv[0])` 출력 후 `exit(1)` (05 3-2)
- **판정**: ✅ 통과 (T01 `pwd` 포함)

---

## 2단계: 구조체 + 연결 리스트 기반 백그라운드 작업 관리

**검증 항목**: `&` 백그라운드 등록, `ProcessInfo` 리스트 적재, 종료 감지 시 `[done]` 회수 + 즉시 `free`

```text
TUK-OS > sleep 1 &
[bg] pid=67043 command="sleep 1"
TUK-OS > jobs
[JOB#]  PID      STATUS    START_TIME  CPU%   MEM(KB)  COMMAND
[1]     67043    RUNNING   18:47:45    4.3    2560     sleep 1
TUK-OS > jobs
[done] pid=67043 exit=0 command="sleep 1"
No background jobs.
```

- `sleep 1 &` → `[bg]` 등록 메시지(TU SKY BLUE), PID·명령어·시작시각 저장
- 1초 후 REPL 사이클 진입 시 `waitpid(WNOHANG)`로 종료 감지 → `[done]` 출력 후 리스트에서 노드 분리·`free`
- **판정**: ✅ 통과 (T03 백그라운드 등록)

---

## 3단계: `jobs` 검색

**검증 항목**: 전체 목록, `-pid` 완전일치, `-name` 부분일치, 미존재/오류 처리

```text
TUK-OS > jobs
[JOB#]  PID      STATUS    START_TIME  CPU%   MEM(KB)  COMMAND
[1]     66948    RUNNING   18:47:25    4.8    3072     sleep 30
[2]     66950    RUNNING   18:47:25    5.0    4096     sleep 40
TUK-OS > jobs -pid 999999
No matching job found.
TUK-OS > jobs -name sleep
[JOB#]  PID      STATUS    START_TIME  CPU%   MEM(KB)  COMMAND
[1]     66948    RUNNING   18:47:25    4.8    3072     sleep 30
[2]     66950    RUNNING   18:47:25    5.0    4096     sleep 40
```

- `jobs` → 컬럼 제목 포함 표 출력 (01 6-4 규격)
- `jobs -pid 999999` → 미존재 시 `No matching job found.`
- `jobs -name sleep` → `name` 필드 부분 문자열 매칭
- 추가 확인: `jobs -pid abc`(비숫자) 거부, `jobs -foo`(미정의) → `invalid jobs option`
- **판정**: ✅ 통과 (T04 `jobs`, T05 `jobs -pid`, T06 `jobs -name`)

---

## 4단계: `top` 정렬 (qsort 콜백 직접 구현)

**검증 항목**: `-cpu`/`-mem`/`-time` 각기 다른 기준 내림차순 정렬, 원본 리스트 불변, 오류 옵션 처리

```text
TUK-OS > top -cpu
[1]     66950    RUNNING   18:47:25    5.0    4096     sleep 40
[2]     66948    RUNNING   18:47:25    4.8    3072     sleep 30
TUK-OS > top -mem
[1]     66950    RUNNING   18:47:25    5.0    4096     sleep 40
[2]     66948    RUNNING   18:47:25    4.8    3072     sleep 30
TUK-OS > top -time
[1]     66948    RUNNING   18:47:25    4.8    3072     sleep 30
[2]     66950    RUNNING   18:47:25    5.0    4096     sleep 40
TUK-OS > top -bad
top: invalid top option '-bad'
usage: top -cpu | -mem | -time
```

- `-cpu`/`-mem` → 값이 큰 job(pid 66950)이 `[1]`
- `-time` → 먼저 등록된 job(pid 66948, 이른 `start_time`)이 `[1]` → 실행시간 기준 내림차순
- 출력용 배열을 `qsort()`로 정렬하므로 원본 연결 리스트 순서는 보존
- `top -bad` → `invalid top option`
- **판정**: ✅ 통과 (T07 `top -time`)

---

## 5단계: `/proc` 연동 (동현 파트 인터페이스, 현재 Mock)

**검증 항목**: 실행 중 프로세스의 CPU/메모리 값 갱신, 접근 실패가 쉘 종료로 이어지지 않음

- 위 3·4단계 표의 `CPU%`, `MEM(KB)` 값은 `update_process_stats()`가 채운 결과다.
- 현재 이 함수는 `system_info.c`의 **Mock**(동현 파트 병합 전 결정적 더미 값)이며,
  BASH 호출부(`refresh_all_processes`)는 실패 시 `0.0`/`0`으로 되돌리고 상태는 유지하는
  규약(01 3-4)까지 구현되어 있어, 실제 `/proc` 파서로 교체해도 호출부 변경이 불필요하다.
- **판정**: ✅ 인터페이스/호출부 통과, 실데이터 파싱은 동현 파트 병합 대기

---

## 6단계: 히스토리 파일 입출력 (`.tuk_history`)

**검증 항목**: 명령 확정 시 원본 저장, 재시작 시 이력 로드 후 append, 빈 줄 미저장

```text
[1회차] pwd / jobs / schedule / exit 입력 후 .tuk_history:
     1  pwd
     2  jobs
     3  schedule
     4  exit

[2회차] 재시작 후 help / exit 입력 → 기존 이력 보존 + append:
     1  pwd
     2  jobs
     3  schedule
     4  exit
     5  help
     6  exit
```

- 명령 확정 직후 append + `fflush` (03 7-1)
- 재시작 시 기존 4줄 로드 후 새 2줄이 5~6번에 이어 붙음 → 로드/저장 정상
- **판정**: ✅ 통과 (T08 재시작 후 히스토리 로드)

---

## 7단계: 캠퍼스 특화 명령어 (BASH 파서/디스패치 + 원종우 Mock 핸들러)

**검증 항목**: 진입점 식별, 옵션 검증, 잘못된 옵션 사용법 출력, 핸들러 디스패치

```text
TUK-OS > schedule
[mock] schedule: (원종우 파트 더미 데이터 출력 지점)
TUK-OS > bus -1
[mock] bus: -1 (원종우 파트 더미 데이터 출력 지점)
TUK-OS > bus
usage: bus -1 | -2
TUK-OS > notice -a -n 3
[mock] notice: -a -n 3 (원종우 파트 더미 데이터 출력 지점)
TUK-OS > notice -a -g
usage: notice [-g | -a | -s] [-n N]
TUK-OS > map -find B101
[mock] map: -find B101 (원종우 파트 더미 데이터 출력 지점)
TUK-OS > map -find
usage: map -A | -B | -C | -D | -E | -F | -G | -f | -find ROOM
TUK-OS > contact -e
[mock] contact: -e (원종우 파트 더미 데이터 출력 지점)
```

- `bus`(옵션 누락), `notice -a -g`(분류 옵션 2개), `map -find`(값 누락) → 각 명령어별 사용법 출력
- `notice -a -n 3` → 허용 조합 파싱 성공 후 핸들러 디스패치
- 핸들러는 현재 원종우 파트 병합 전 **Mock**(검증된 옵션을 되돌려 출력, TU MINT)
- **판정**: ✅ 통과 (T09 `notice -a -n 3`, T10 잘못된 옵션)

---

## 내장 예외처리 및 백그라운드 금지 (05 5-3)

```text
TUK-OS > cd a b
usage: cd [path]
TUK-OS > pwd extra
usage: pwd
TUK-OS > exit now
usage: exit
TUK-OS > cd &
built-in command cannot run in background
TUK-OS > jobs &
built-in command cannot run in background
```

- 인자 과다 → 사용법 출력, 내장 명령 + `&` 조합 → 금지 메시지
- **판정**: ✅ 통과 (T10 잘못된 옵션/조합)

---

## 크로스쉘 정합성 (02 7장)

원종우 파트(ZSH) 출력과 TUK-Shell 내부 명령 형식을 대조한 결과 아래가 일치한다.

| 항목 | 형식 |
|------|------|
| 프롬프트 | `TUK-OS > ` |
| 백그라운드 등록 | `[bg] pid=N command="..."` |
| 백그라운드 종료 | `[done] pid=N exit=N command="..."` |
| 없는 명령 | `<cmd>: No such file or directory` |
| 내장+`&` 금지 | `built-in command cannot run in background` |
| 종료 알림 | `TUK-Shell을 종료합니다.` |

> 종료 알림은 스펙에 명시되지 않았으나 ZSH 파트와의 정합성을 위해 추가했다.
> `exit` 명령과 EOF(Ctrl+D) 양쪽 종료 경로에서 동일하게 출력된다.

---

## T01~T10 요약

| ID | 시나리오 | 결과 |
|----|----------|------|
| T01 | `pwd` | ✅ |
| T02 | `cd ..` 후 `pwd` | ✅ |
| T03 | `sleep N &` 백그라운드 등록 | ✅ |
| T04 | `jobs` 표 출력 | ✅ |
| T05 | `jobs -pid <pid>` | ✅ |
| T06 | `jobs -name sleep` | ✅ |
| T07 | `top -time` 정렬 | ✅ |
| T08 | 재시작 후 `.tuk_history` 로드 | ✅ |
| T09 | `notice -a -n 3` 옵션 조합 파싱 | ✅ |
| T10 | 잘못된 옵션 → 사용법/오류 | ✅ |

---

## 재현 방법

```bash
cd src/bash
make                       # 빌드
./tests/stage_demo.sh      # 단계별 시연 출력 재생
./tests/run_tests.sh ./tuk_shell /tmp/tuk_verify   # 자동 회귀(T01~T10, 52항목)
```

## 남은 Mock 연동 지점

| 지점 | 파일 | 담당 |
|------|------|------|
| `/proc` CPU/메모리 파싱 | `src/system_info.c` `update_process_stats()` | 동현 |
| 캠퍼스 명령 7종 실데이터 | `src/campus.c` `handle_*_command()` | 원종우 |

두 지점 모두 헤더 시그니처와 반환 규약이 고정되어 있어, 실제 구현으로 교체 시
BASH 호출부 수정 없이 링크된다.
