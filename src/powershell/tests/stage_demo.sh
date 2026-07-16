#!/bin/bash
# TUK-Shell BASH 파트 단계별 시연 스크립트 [우진]
#
# plan.md 1~7단계를 순서대로 실제 실행하여 출력을 보여준다.
# 검증 근거: docs/40_verification/BASH_단계별_검증결과.md
#
# 사용법:
#   cd src/bash && make && ./tests/stage_demo.sh
#   (색상 없이 보려면)  ./tests/stage_demo.sh --no-color

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASH_DIR="$(dirname "$SCRIPT_DIR")"
BIN="$BASH_DIR/tuk_shell"
WORK="$(mktemp -d)"

if [ ! -x "$BIN" ]; then
    echo "[stage_demo] 빌드가 필요합니다. 'make -C $BASH_DIR' 실행 후 다시 시도하세요." >&2
    exit 1
fi

# 캠퍼스 명령은 임시 작업 디렉터리에서 실행되므로 데이터 경로를 명시한다.
# notice는 고정 HTML을 주입해 시연을 오프라인·결정론적으로 유지(라이브로 보려면
# 아래 TUK_NOTICE_FIXTURE 줄을 주석 처리하고 인터넷 연결 상태로 실행).
export TUK_CAMPUS_DATA="$(cd "$BASH_DIR/../../data/campus" && pwd)"
export TUK_NOTICE_FIXTURE="$SCRIPT_DIR/fixtures/notice_sample.html"

if [ "${1:-}" = "--no-color" ]; then
    FILTER='sed s/\x1b\[[0-9;]*m//g'
else
    FILTER='cat'
fi

banner() {
    echo ""
    echo "==================================================================="
    echo "  $1"
    echo "==================================================================="
}

# 파이프 입력을 쉘에 흘려보내고 출력을 그대로 보여준다.
# 타이밍이 필요한 단계는 FIFO로 명령 사이에 지연을 준다.
feed() { printf '%b' "$1" | (cd "$WORK" && "$BIN") 2>&1 | $FILTER; }

feed_timed() {  # stdin: 지연 포함 명령 스트림, 백그라운드 job 대기용
    rm -f "$WORK/in.fifo"
    mkfifo "$WORK/in.fifo"
    ( cd "$WORK" && "$BIN" < in.fifo ) 2>&1 | $FILTER &
    local pid=$!
    exec 3> "$WORK/in.fifo"
    cat >&3
    exec 3>&-
    wait "$pid"
    rm -f "$WORK/in.fifo"
}

banner "STAGE 1: REPL + 외부명령 (fork/execvp)"
feed 'pwd\necho hello TUK-Shell\nsle\nexit\n'

banner "STAGE 2: 백그라운드 등록 + [done] 회수 (연결 리스트/즉시 free)"
{
    printf 'sleep 1 &\n'; sleep 0.3
    printf 'jobs\n';       sleep 1.2
    printf 'jobs\n'
    printf 'exit\n'
} | feed_timed

banner "STAGE 3 & 4: jobs 검색 + top 정렬"
{
    printf 'sleep 30 &\n'; sleep 0.3
    printf 'sleep 40 &\n'; sleep 0.3
    printf 'jobs\n';           sleep 0.3
    printf 'jobs -pid 999999\n'; sleep 0.3
    printf 'jobs -name sleep\n';  sleep 0.3
    printf 'top -cpu\n';   sleep 0.3
    printf 'top -mem\n';   sleep 0.3
    printf 'top -time\n';  sleep 0.3
    printf 'top -bad\n';   sleep 0.3
    printf 'exit\n'
} | feed_timed
pkill -f 'sleep 30' 2>/dev/null
pkill -f 'sleep 40' 2>/dev/null

banner "STAGE 6: 히스토리 파일 입출력 (.tuk_history)"
rm -f "$WORK/.tuk_history"
feed 'pwd\nschedule\nexit\n' >/dev/null
echo "[1회차 후 .tuk_history]"; cat -n "$WORK/.tuk_history"
feed 'help\nexit\n' >/dev/null
echo "[2회차(재시작) 후 .tuk_history — 이력 보존 + append]"; cat -n "$WORK/.tuk_history"

banner "STAGE 7: 캠퍼스 명령 옵션검증 + 실데이터 출력"
feed 'schedule\nbus -1\nbus\nbob -t\nnotice -a -n 3\nnotice -a -g\nmap -A\nmap -f\nmap -s\nmap -find B101\nmap -find\nweather -c\ncontact -p 김민석\ncontact -e\nexit\n'

banner "내장 예외처리 + 백그라운드 금지 (05 5-3)"
feed 'cd a b\npwd extra\ncd &\njobs &\nexit\n'

rm -rf "$WORK"
echo ""
echo "[stage_demo] 완료. 상세 판정은 docs/40_verification/BASH_단계별_검증결과.md 참고."
