# tuk_shell_main.ps1

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# ==========================================================
# 1. TUK-Shell Command Handlers
# ==========================================================

function Get-TukSchedule {
    Write-Host "📅 [AI소프트웨어학과 시간표]" -ForegroundColor Cyan
    Write-Host "--------------------------------" -ForegroundColor Cyan
    Write-Host "월: 운영체제 (09:00 - 12:00)" -ForegroundColor Cyan
    Write-Host "화: 데이터구조 (13:30 - 15:30)" -ForegroundColor Cyan
}

function Get-TukBus {
    param($ArgsList) 

    if ($null -eq $ArgsList -or $ArgsList.Count -eq 0) {
        Write-Host "usage: bus [-1 | -2]" -ForegroundColor Yellow
        return 1
    }

    $option = $ArgsList[0]
    switch ($option) {
        "-1" { Write-Host "🚌 [1캠퍼스 셔틀] 다음 정류장 도착: 5분 후" -ForegroundColor Cyan }
        "-2" { Write-Host "🚌 [2캠퍼스 셔틀] 다음 정류장 도착: 12분 후" -ForegroundColor Cyan }
        default {
            Write-Host "invalid bus option: $option" -ForegroundColor Red
            Write-Host "usage: bus [-1 | -2]" -ForegroundColor Yellow
        }
    }
}

function Get-TukNotice {
    param($ArgsList)
    
    $category = "전체"
    $count = 5 

    if ($null -ne $ArgsList) {
        for ($i = 0; $i -lt $ArgsList.Count; $i++) {
            switch ($ArgsList[$i]) {
                "-g" { $category = "일반" }
                "-a" { $category = "학사" }
                "-s" { $category = "장학" }
                "-n" {
                    if (($i + 1) -lt $ArgsList.Count) {
                        $count = $ArgsList[$i + 1]
                        $i++ 
                    } else {
                        Write-Host "notice: '-n' 옵션은 숫자가 필요합니다." -ForegroundColor Red
                        return 1
                    }
                }
                default {
                    Write-Host "invalid notice option: $($ArgsList[$i])" -ForegroundColor Red
                    Write-Host "usage: notice [-g|-a|-s] [-n N]" -ForegroundColor Yellow
                    return 1
                }
            }
        }
    }

    Write-Host "📢 [공지사항 - $category] 최근 $count 건" -ForegroundColor Cyan
    Write-Host "1. 2026학년도 수강신청 안내" -ForegroundColor Cyan
}

# ==========================================================
# 2. Main REPL Loop
# ==========================================================

$promptString = "TUK-OS > "
$promptColor = "Cyan"

Write-Host "🚀 TUK-Shell (PowerShell Wrapper) 테스트 모드 시작" -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor DarkGray

while ($true) {
    Write-Host $promptString -ForegroundColor $promptColor -NoNewline
    $inputLine = Read-Host

    if ([string]::IsNullOrWhiteSpace($inputLine)) { continue }

    $argv = $inputLine.Trim() -split '\s+'
    $command = $argv[0]

    $isBackground = $false
    if ($argv[-1] -eq '&') {
        $isBackground = $true
        if ($argv.Length -ge 2) {
            $argv = $argv[0..($argv.Length - 2)]
        }
    }

    switch ($command) {
        "exit" {
            if ($isBackground) { Write-Host "built-in command cannot run in background" -ForegroundColor Red; break }
            Write-Host "TUK-Shell을 종료합니다." -ForegroundColor DarkGray
            exit 0
        }
        "help" {
            Write-Host "==== TUK-Shell 지원 명령어 ===="
            Write-Host "exit, schedule, bus, notice"
            break
        }
        "schedule" { Get-TukSchedule; break }
        "bus"      { 
            if ($argv.Length -gt 1) { Get-TukBus -ArgsList $argv[1..($argv.Length - 1)] }
            else { Get-TukBus -ArgsList $null }
            break 
        }
        "notice"   { 
            if ($argv.Length -gt 1) { Get-TukNotice -ArgsList $argv[1..($argv.Length - 1)] }
            else { Get-TukNotice -ArgsList $null }
            break 
        }
        default {
            Write-Host "$($command): command not found" -ForegroundColor Red
        }
    }
}