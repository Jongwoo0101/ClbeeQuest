#!/bin/bash
# TUK-Shell BASH 파트 검증 스크립트 (04_검증및추적명세서 T01~T10 범위)
set -u

BIN="$1"
WORK="$2"

# BIN을 절대경로로 정규화 (아래에서 WORK로 cd하므로 상대경로가 깨지지 않게)
case "$BIN" in
    /*) ;;
    *) BIN="$(pwd)/$BIN" ;;
esac

mkdir -p "$WORK"
cd "$WORK" || exit 1
# WORK를 물리 경로로 정규화 (macOS /tmp -> /private/tmp 심볼릭 링크 대응:
# 쉘 getcwd() 출력과 프롬프트 검증 문자열이 일치하도록)
WORK="$(pwd -P)"
rm -f .tuk_history transcript_a.txt transcript_b.txt transcript_b2.txt transcript_c.txt raw_b.txt
rm -f in.fifo out.fifo

PASS=0
FAIL=0

strip_ansi() { sed $'s/\x1b\[[0-9;]*m//g'; }

check() {
    local name="$1"
    shift
    if "$@" > /dev/null 2>&1; then
        echo "PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $name"
        FAIL=$((FAIL + 1))
    fi
}

PARENT=$(dirname "$WORK")

# ---------------- Session A: 기본 내장/입력 예외 (T01, T02, T10 일부) ----------------
{
    printf 'pwd\n'
    printf 'cd ..\n'
    printf 'pwd\n'
    printf 'cd %s\n' "$WORK"
    printf 'help\n'
    printf 'a%.0s' $(seq 1 2000)
    printf '\n'
    printf 'pwd\n'
    printf 'say "hello"\n'
    printf 'sleep&\n'
    printf 'cd a b\n'
    printf 'pwd extra\n'
    printf '&\n'
    printf 'exit stuff\n'
    printf 'exit\n'
} | "$BIN" > transcript_a_raw.txt 2>&1
rc_a=$?
strip_ansi < transcript_a_raw.txt > transcript_a.txt

check "T01 pwd prints cwd"            grep -q "> $WORK\$" transcript_a.txt
check "T02 cd .. then pwd shows parent" grep -q "> $PARENT\$" transcript_a.txt
check "A help lists commands"          grep -q "TUK-Shell commands" transcript_a.txt
check "A input too long error"         grep -q "input too long" transcript_a.txt
check "A shell alive after overflow"   test "$(grep -c "> $WORK\$" transcript_a.txt)" -ge 2
check "A quote unsupported"            grep -q "unsupported syntax" transcript_a.txt
check "A attached & unsupported"       grep -q "unsupported '&' usage" transcript_a.txt
check "T10 cd extra args usage"        grep -q "usage: cd \[path\]" transcript_a.txt
check "T10 pwd extra args usage"       grep -q "usage: pwd" transcript_a.txt
check "A bare & error"                 grep -q "missing command before '&'" transcript_a.txt
check "A exit extra args usage"        grep -q "usage: exit" transcript_a.txt
check "A exit code 0"                  test "$rc_a" -eq 0

# ---------------- Session B: 인터랙티브 (T03~T07, T09, T10 일부) ----------------
RAW=raw_b.txt
: > "$RAW"
mkfifo in.fifo out.fifo
"$BIN" < in.fifo > out.fifo 2>&1 &
SHELL_PID=$!
exec 3> in.fifo 4< out.fifo

send() { printf '%s\n' "$1" >&3; }

drain_for() {
    local quiet="$1"
    local l
    while IFS= read -r -t "$quiet" l <&4; do
        printf '%s\n' "$l" >> "$RAW"
    done
}

send "sleep 7 &"
pid1=""
for i in 1 2 3 4 5 6 7 8 9 10; do
    drain_for 1
    pid1=$(strip_ansi < "$RAW" | sed -n 's/.*\[bg\] pid=\([0-9][0-9]*\) command="sleep 7".*/\1/p' | head -n 1)
    [ -n "$pid1" ] && break
done
sleep 1
send "sleep 6 &"
send "echo ===MARK-JOBS==="
send "jobs"
send "echo ===MARK-PID==="
send "jobs -pid ${pid1:-1}"
send "jobs -pid 999999"
send "jobs -pid abc"
send "jobs -foo"
send "echo ===MARK-NAME==="
send "jobs -name slee"
send "jobs -name zzznomatch"
send "echo ===MARK-TOP==="
send "top -time"
send "top -cpu"
send "top -mem"
send "top"
send "top -x"
send "echo ===MARK-CAMPUS==="
send "notice -a -n 3"
send "notice -a -g"
send "notice -n"
send "schedule"
send "schedule extra"
send "bus -1"
send "bus"
send "bob -E"
send "bob -x"
send "map -find B101"
send "map -find"
send "map -B"
send "weather -c"
send "weather -q"
send "contact -p Kim"
send "contact -p"
send "contact -e"
send "sleep 9 &"
send "exit"
drain_for 3
exec 3>&- 4<&-
wait "$SHELL_PID" 2>/dev/null
rc_b=$?
strip_ansi < "$RAW" > transcript_b.txt

seg() { awk -v m="===MARK-$1===" '$0 ~ m {f=1; next} /===MARK-/ {if (f) exit} f' transcript_b.txt; }

check "T03 background register msg"    grep -q "\[bg\] pid=$pid1 command=\"sleep 7\"" transcript_b.txt
check "T04 jobs shows table header"    sh -c "$(declare -f seg); seg JOBS | grep -q 'PID.*STATUS.*START_TIME.*CPU%.*MEM(KB).*COMMAND'"
check "T04 jobs shows running sleep"   sh -c "$(declare -f seg); seg JOBS | grep -q 'RUNNING.*sleep 7'"
check "T04 jobs shows second job [2]"  sh -c "$(declare -f seg); seg JOBS | grep -q '\[2\].*sleep 6'"
check "T05 jobs -pid finds one row"    sh -c "$(declare -f seg); seg PID | grep -q '\[1\].*$pid1.*RUNNING.*sleep 7'"
check "T05 jobs -pid no match msg"     sh -c "$(declare -f seg); seg PID | grep -q 'No matching job found.'"
check "T05 jobs -pid abc rejected"     sh -c "$(declare -f seg); seg PID | grep -q 'PID must be a positive number'"
check "T10 invalid jobs option"        sh -c "$(declare -f seg); seg PID | grep -q 'invalid jobs option'"
check "T06 jobs -name substring"       sh -c "$(declare -f seg); seg NAME | grep -q 'sleep 7'"
check "T06 jobs -name no match"        sh -c "$(declare -f seg); seg NAME | grep -q 'No matching job found.'"
check "T07 top -time row1 older job"   sh -c "$(declare -f seg); seg TOP | grep -q '\[1\].*sleep 7'"
check "T07 top -time row2 newer job"   sh -c "$(declare -f seg); seg TOP | grep -q '\[2\].*sleep 6'"
check "T07 top runs 3 sort modes"      sh -c "$(declare -f seg); test \"\$(seg TOP | grep -c 'START_TIME')\" -eq 3"
check "T10 top w/o option usage"       sh -c "$(declare -f seg); seg TOP | grep -q 'usage: top -cpu | -mem | -time'"
check "T10 invalid top option"         sh -c "$(declare -f seg); seg TOP | grep -q 'invalid top option'"
check "T09 notice -a -n 3 parsed"      sh -c "$(declare -f seg); seg CAMPUS | grep -q '\[mock\] notice: -a -n 3'"
check "T09 notice two categories usage" sh -c "$(declare -f seg); test \"\$(seg CAMPUS | grep -c 'usage: notice')\" -ge 2"
check "B schedule mock output"         sh -c "$(declare -f seg); seg CAMPUS | grep -q '\[mock\] schedule:'"
check "B schedule extra arg usage"     sh -c "$(declare -f seg); seg CAMPUS | grep -q 'usage: schedule'"
check "B bus -1 dispatched"            sh -c "$(declare -f seg); seg CAMPUS | grep -q '\[mock\] bus: -1'"
check "B bus w/o option usage"         sh -c "$(declare -f seg); seg CAMPUS | grep -q 'usage: bus -1 | -2'"
check "B bob -E dispatched"            sh -c "$(declare -f seg); seg CAMPUS | grep -q '\[mock\] bob: -E'"
check "B bob invalid option usage"     sh -c "$(declare -f seg); test \"\$(seg CAMPUS | grep -c 'usage: bob')\" -ge 1"
check "B map -find B101 dispatched"    sh -c "$(declare -f seg); seg CAMPUS | grep -q '\[mock\] map: -find B101'"
check "B map -find w/o value usage"    sh -c "$(declare -f seg); test \"\$(seg CAMPUS | grep -c 'usage: map')\" -ge 1"
check "B map -B dispatched"            sh -c "$(declare -f seg); seg CAMPUS | grep -q '\[mock\] map: -B'"
check "B weather -c dispatched"        sh -c "$(declare -f seg); seg CAMPUS | grep -q '\[mock\] weather: -c'"
check "B weather invalid usage"        sh -c "$(declare -f seg); test \"\$(seg CAMPUS | grep -c 'usage: weather')\" -ge 1"
check "B contact -p Kim dispatched"    sh -c "$(declare -f seg); seg CAMPUS | grep -q '\[mock\] contact: -p Kim'"
check "B contact -p w/o value usage"   sh -c "$(declare -f seg); test \"\$(seg CAMPUS | grep -c 'usage: contact')\" -ge 1"
check "B contact -e dispatched"        sh -c "$(declare -f seg); seg CAMPUS | grep -q '\[mock\] contact: -e'"
check "B exit code 0"                  test "$rc_b" -eq 0

# ---------------- Session B2: 백그라운드 종료 감지/[done] (T03 후속) ----------------
{
    printf 'sleep 1 &\n'
    sleep 2
    printf 'jobs\n'
    printf 'exit\n'
} | "$BIN" 2>&1 | strip_ansi > transcript_b2.txt

check "B2 [done] reap message"         grep -q '\[done\] pid=[0-9][0-9]* exit=0 command="sleep 1"' transcript_b2.txt
check "B2 jobs empty after reap"       grep -q 'No background jobs.' transcript_b2.txt

# ---------------- Session C: 히스토리 (T08) ----------------
check "T08 history file created"       test -f .tuk_history
check "T08 history keeps raw bg line"  grep -qx 'sleep 7 &' .tuk_history
check "T08 history keeps usage-error cmd" grep -qx 'jobs -pid abc' .tuk_history
check "T08 parse-error line not saved" sh -c "! grep -q 'sleep&' .tuk_history"

lines_before=$(wc -l < .tuk_history)
printf 'pwd\nexit\n' | "$BIN" > transcript_c.txt 2>&1
lines_after=$(wc -l < .tuk_history)

check "T08 restart appends (load ok)"  test "$lines_after" -eq $((lines_before + 2))
check "T08 old entries preserved"      grep -qx 'sleep 7 &' .tuk_history

rm -f in.fifo out.fifo

echo "----------------------------------------"
echo "PASS=$PASS FAIL=$FAIL"
test "$FAIL" -eq 0
