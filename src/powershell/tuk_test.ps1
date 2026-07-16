# tuk_test.ps1

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# ==========================================================
# 0. 전역 자료구조 및 히스토리 초기화
# ==========================================================
$global:BackgroundJobs = @()
$global:JobIdCounter = 1

$global:HistoryFile = Join-Path -Path $HOME -ChildPath ".tuk_history"
try {
    if (-not (Test-Path $global:HistoryFile)) {
        New-Item -Path $global:HistoryFile -ItemType File -Force | Out-Null
    }
} catch {
    Write-Host "history file init error" -ForegroundColor Red
}

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
            Write-Host "invalid bus option: $($option)" -ForegroundColor Red
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
    Write-Host "📢 [공지사항 - $($category)] 최근 $($count) 건" -ForegroundColor Cyan
    Write-Host "1. 2026학년도 수강신청 안내" -ForegroundColor Cyan
}

function Get-TukJobs {
    param($ArgsList)
    
    foreach ($jobObj in $global:BackgroundJobs) {
        if ($jobObj.Status -ne "DONE") {
            if ($jobObj.PSJob.State -in @("Completed", "Failed", "Stopped")) {
                $jobObj.Status = "DONE"
            }
        }
    }
    
    $targetJobs = $global:BackgroundJobs
    if ($null -ne $ArgsList -and $ArgsList.Count -gt 0) {
        $option = $ArgsList[0]
        if ($option -eq "-pid") {
            if ($ArgsList.Count -lt 2) { Write-Host "usage: jobs -pid [PID]" -ForegroundColor Yellow; return }
            $targetPid = $ArgsList[1]
            $targetJobs = $global:BackgroundJobs | Where-Object { $_.PSJob.Id -eq $targetPid }
        } elseif ($option -eq "-name") {
            if ($ArgsList.Count -lt 2) { Write-Host "usage: jobs -name [KEYWORD]" -ForegroundColor Yellow; return }
            $keyword = $ArgsList[1]
            $targetJobs = $global:BackgroundJobs | Where-Object { $_.Command -match $keyword }
        } else {
            Write-Host "invalid jobs option: $($option)" -ForegroundColor Red
            return
        }
    }
    
    if ($targetJobs.Count -eq 0) {
        Write-Host "No matching job found." -ForegroundColor DarkGray
    } else {
        Write-Host "[JOB#]`tPID`tSTATUS`tSTART_TIME`tCPU%`tMEM(KB)`tCOMMAND" -ForegroundColor Cyan
        foreach ($jobObj in $targetJobs) {
            $line = "[$($jobObj.JobNumber)]`t$($jobObj.PSJob.Id)`t$($jobObj.Status)`t$($jobObj.StartTime)`t$($jobObj.CpuUsage)`t$($jobObj.MemUsage)`t$($jobObj.Command)"
            Write-Host $line -ForegroundColor Cyan
        }
    }
    
    $doneJobs = $global:BackgroundJobs | Where-Object { $_.Status -eq "DONE" }
    foreach ($done in $doneJobs) {
        Remove-Job -Job $done.PSJob -Force -ErrorAction SilentlyContinue
    }
    $global:BackgroundJobs = @($global:BackgroundJobs | Where-Object { $_.Status -ne "DONE" })
}

# ==========================================================
# C 모듈 연동 브릿지 함수
# ==========================================================
function Update-JobStats {
    foreach ($jobObj in $global:BackgroundJobs) {
        if ($jobObj.Status -eq "RUNNING") {
            try {
                # WSL 내부의 C 실행 파일 호출 (예외 발생 시 catch 블록으로 이동)
                # (주의: 실제 실행 파일 이름이 system_info 가 아닐 경우 수정 필요)
                $c_output = wsl.exe -e bash -c "./system_info 2>/dev/null"
                
                if ($null -ne $c_output) {
                    $outputStr = $c_output -join "`n"
                    
                    # 정규표현식 파싱: 숫자와 소수점만 추출
                    if ($outputStr -match "CPU\s*.*?:\s*([0-9.]+)") {
                        $jobObj.CpuUsage = [Math]::Round([double]$matches[1], 2)
                    }
                    if ($outputStr -match "메모리\s*.*?:\s*([0-9]+)") {
                        $jobObj.MemUsage = [int]$matches[1]
                    }
                }
            } catch {
                # C 실행 파일이 없거나 오류 시 기존 값을 유지하여 쉘 안정성 확보
            }
        }
    }
}

# ==========================================================
# top 명령어 핸들러
# ==========================================================
function Get-TukTop {
    param($ArgsList)

    # 1. 상태 최신화 및 가비지 컬렉션 (DONE 처리)
    foreach ($jobObj in $global:BackgroundJobs) {
        if ($jobObj.Status -ne "DONE" -and $jobObj.PSJob.State -in @("Completed", "Failed", "Stopped")) {
            $jobObj.Status = "DONE"
        }
    }
    
    # 2. C 모듈 브릿지 가동하여 최신 CPU/MEM 확보
    Update-JobStats

    # 3. 정렬 기준 파싱
    $sortProp = "CpuUsage" # 기본값: cpu
    $isDescending = $true

    if ($null -ne $ArgsList -and $ArgsList.Count -gt 0) {
        $option = $ArgsList[0]
        switch ($option) {
            "-cpu"  { $sortProp = "CpuUsage" }
            "-mem"  { $sortProp = "MemUsage" }
            "-time" { 
                $sortProp = "StartTime" 
                # 가장 오래된 작업(현재 시각 - 시작 시각이 가장 큰 작업)이 먼저 나오도록 오름차순 정렬
                $isDescending = $false 
            }
            default {
                Write-Host "invalid top option: $($option)" -ForegroundColor Red
                Write-Host "usage: top [-cpu | -mem | -time]" -ForegroundColor Yellow
                return
            }
        }
    }

    $activeJobs = @($global:BackgroundJobs | Where-Object { $_.Status -ne "DONE" })

    if ($activeJobs.Count -eq 0) {
        Write-Host "No background jobs." -ForegroundColor DarkGray
        return
    }

    # 4. 파이프라인 객체 정렬
    if ($isDescending) {
        $sortedJobs = $activeJobs | Sort-Object -Property $sortProp -Descending
    } else {
        $sortedJobs = $activeJobs | Sort-Object -Property $sortProp
    }

    # 5. 출력
    Write-Host "[JOB#]`tPID`tSTATUS`tSTART_TIME`tCPU%`tMEM(KB)`tCOMMAND" -ForegroundColor Cyan
    foreach ($jobObj in $sortedJobs) {
        $line = "[$($jobObj.JobNumber)]`t$($jobObj.PSJob.Id)`t$($jobObj.Status)`t$($jobObj.StartTime)`t$($jobObj.CpuUsage)`t$($jobObj.MemUsage)`t$($jobObj.Command)"
        Write-Host $line -ForegroundColor Cyan
    }
}

# ==========================================================
# history 명령어 핸들러
# ==========================================================
function Get-TukHistory {
    try {
        $histories = Get-Content -Path $global:HistoryFile -Encoding UTF8
        $startIndex = 1
        foreach ($h in $histories) {
            Write-Host "  $($startIndex)  $($h)" -ForegroundColor Cyan
            $startIndex++
        }
    } catch {
        Write-Host "history read error" -ForegroundColor Red
    }
}

function Get-TukBob {
    param($ArgsList)
    $isToday = $false
    $isEdong = $false

    if ($null -ne $ArgsList) {
        for ($i = 0; $i -lt $ArgsList.Count; $i++) {
            switch ($ArgsList[$i]) {
                "-t" { $isToday = $true }
                "-e" { $isEdong = $true }
                default {
                    Write-Host "invalid bob option: $($ArgsList[$i])" -ForegroundColor Red
                    Write-Host "usage: bob [-t] [-e]" -ForegroundColor Yellow
                    return
                }
            }
        }
    }

    if ($isEdong) { 
        Write-Host "[E동 레스토랑] 2026-07-15 (Wed)  7000원" -ForegroundColor Cyan
        Write-Host "메뉴: 등심돈까스, 제육덮밥" -ForegroundColor Cyan
        Write-Host "🔗 E동 레스토랑 메뉴: https://ibook.tukorea.ac.kr/Viewer/menu01" -ForegroundColor Cyan
    } elseif ($isToday) { 
        Write-Host "[TIP 지하 식당] 2026-07-15 (Wed)  5500원" -ForegroundColor Cyan
        Write-Host "메뉴: 뚝배기불고기, 해물짬뽕" -ForegroundColor Cyan
        Write-Host "🔗 TIP 지하 식당 메뉴: https://ibook.tukorea.ac.kr/viewer/menu02" -ForegroundColor Cyan
    } else { 
        # -d 또는 기본값 (대신식당)
        Write-Host "[대신식당] 2026-07-15 (Wed)  6000원" -ForegroundColor Cyan
        Write-Host "메뉴: 백반(제육), 계란찜, 잡곡밥, 김치" -ForegroundColor Cyan
    }
}

function Get-TukMap {
    param($ArgsList)
    $building = "전체 안내"
    $findKeyword = ""

    if ($null -ne $ArgsList) {
        for ($i = 0; $i -lt $ArgsList.Count; $i++) {
            switch ($ArgsList[$i]) {
                "-A" { $building = "A동 (새천년관)" }
                "-E" { $building = "E동 (엔지니어링센터)" }
                "-G" { $building = "G동 (종합교육관)" }
                "-f" { $building = "편의시설" }
                "-find" {
                    if (($i + 1) -lt $ArgsList.Count) {
                        $findKeyword = $ArgsList[$i + 1]
                        $i++ # 값 파싱 완료 후 인덱스 건너뛰기
                    } else {
                        Write-Host "map: '-find' 옵션은 검색어가 필요합니다." -ForegroundColor Red
                        return
                    }
                }
                default {
                    Write-Host "invalid map option: $($ArgsList[$i])" -ForegroundColor Red
                    Write-Host "usage: map [-A|-E|-G|-f] [-find KEYWORD]" -ForegroundColor Yellow
                    return
                }
            }
        }
    }

    Write-Host "🗺️ [캠퍼스 맵 - $($building)]" -ForegroundColor Cyan
    if ($findKeyword -ne "") {
        Write-Host "▶ 검색 결과: '$($findKeyword)' (해당 위치 좌표 출력)" -ForegroundColor Cyan
    }
}

function Get-TukWeather {
    param($ArgsList)
    $mode = "현재 날씨"

    if ($null -ne $ArgsList) {
        for ($i = 0; $i -lt $ArgsList.Count; $i++) {
            switch ($ArgsList[$i]) {
                "-c" { $mode = "현재 기상" }
                "-w" { $mode = "주간 예보" }
                "-d" { $mode = "미세먼지" }
                default {
                    Write-Host "invalid weather option: $($ArgsList[$i])" -ForegroundColor Red
                    Write-Host "usage: weather [-c | -w | -d]" -ForegroundColor Yellow
                    return
                }
            }
        }
    }
    Write-Host "🌤️ [정왕동 날씨 정보 - $($mode)]" -ForegroundColor Cyan
    Write-Host "온도: 22도, 맑음 (※ API 연동 대기중)" -ForegroundColor DarkGray
}

function Get-TukContact {
    param($ArgsList)
    $category = "통합 연락망"

    if ($null -ne $ArgsList) {
        for ($i = 0; $i -lt $ArgsList.Count; $i++) {
            switch ($ArgsList[$i]) {
                "-p" { $category = "교수진 연구실" }
                "-d" { $category = "학과 사무실" }
                "-e" { $category = "긴급 및 보안" }
                default {
                    Write-Host "invalid contact option: $($ArgsList[$i])" -ForegroundColor Red
                    Write-Host "usage: contact [-p | -d | -e]" -ForegroundColor Yellow
                    return
                }
            }
        }
    }
    Write-Host "📞 [교내 연락처 - $($category)]" -ForegroundColor Cyan
    Write-Host "AI소프트웨어학과: 031-8041-XXXX" -ForegroundColor Cyan
}

# ==========================================================
# 2. Main REPL Loop
# ==========================================================
$promptString = "TUK-OS > "
$promptColor = "Cyan"

Write-Host "🚀 TUK-Shell (PowerShell Wrapper) 가동" -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor DarkGray

while ($true) {
    Write-Host $promptString -ForegroundColor $promptColor -NoNewline
    $inputLine = Read-Host

    if ([string]::IsNullOrWhiteSpace($inputLine)) { continue }

    # [핵심] 유효한 명령어는 즉시 히스토리 파일에 저장 (안전한 예외 처리 포함)
    try {
        Add-Content -Path $global:HistoryFile -Value $inputLine -Encoding UTF8 -ErrorAction Stop
    } catch {
        # 파일 저장 실패 시 쉘이 멈추지 않도록 무시
    }

    $argv = $inputLine.Trim() -split '\s+'
    $command = $argv[0]

    $isBackground = $false
    if ($argv[-1] -eq '&') {
        $isBackground = $true
        if ($argv.Length -ge 2) { $argv = $argv[0..($argv.Length - 2)] }
    }

    switch ($command) {
        "exit" {
            if ($isBackground) { Write-Host "built-in command cannot run in background" -ForegroundColor Red; break }
            Write-Host "TUK-Shell을 종료합니다." -ForegroundColor DarkGray
            exit 0
        }
        "help" {
            Write-Host "==== TUK-Shell 지원 명령어 ===="
            Write-Host "시스템: exit, jobs, top, history"
            Write-Host "캠퍼스: schedule, bus, notice, bob, map, weather, contact"
            break
        }
        "history" {
            if ($isBackground) { Write-Host "built-in command cannot run in background" -ForegroundColor Red; break }
            Get-TukHistory
            break
        }
        "jobs" {
            if ($isBackground) { Write-Host "built-in command cannot run in background" -ForegroundColor Red; break }
            if ($argv.Length -gt 1) { Get-TukJobs -ArgsList $argv[1..($argv.Length - 1)] }
            else { Get-TukJobs -ArgsList $null }
            break
        }
        "schedule" { Get-TukSchedule; break }
        "bus"      { 
            if ($argv.Length -gt 1) { Get-TukBus -ArgsList $argv[1..($argv.Length - 1)] }
            else { he -ArgsList $null }
            break 
        }
        "notice"   { 
            if ($argv.Length -gt 1) { Get-TukNotice -ArgsList $argv[1..($argv.Length - 1)] }
            else { Get-TukNotice -ArgsList $null }
            break 
        }
        "top" {
            if ($isBackground) { Write-Host "built-in command cannot run in background" -ForegroundColor Red; break }
            if ($argv.Length -gt 1) { Get-TukTop -ArgsList $argv[1..($argv.Length - 1)] }
            else { Get-TukTop -ArgsList $null }
            break
        }
        "bob" {
            if ($argv.Length -gt 1) { Get-TukBob -ArgsList $argv[1..($argv.Length - 1)] }
            else { Get-TukBob -ArgsList $null }
            break
        }
        "map" {
            if ($argv.Length -gt 1) { Get-TukMap -ArgsList $argv[1..($argv.Length - 1)] }
            else { Get-TukMap -ArgsList $null }
            break
        }
        "weather" {
            if ($argv.Length -gt 1) { Get-TukWeather -ArgsList $argv[1..($argv.Length - 1)] }
            else { Get-TukWeather -ArgsList $null }
            break
        }
        "contact" {
            if ($argv.Length -gt 1) { Get-TukContact -ArgsList $argv[1..($argv.Length - 1)] }
            else { Get-TukContact -ArgsList $null }
            break
        }
        default {
            try {
                $fullCommand = $argv -join ' '
                if ($isBackground) {
                    $psJob = Start-Job -ScriptBlock { param($cmd); wsl.exe -e bash -c $cmd } -ArgumentList $fullCommand
                    $jobObj = [PSCustomObject]@{
                        JobNumber = $global:JobIdCounter
                        PSJob     = $psJob
                        Command   = $fullCommand
                        StartTime = (Get-Date).ToString("HH:mm:ss")
                        Status    = "RUNNING"
                        CpuUsage  = 0.0
                        MemUsage  = 0
                    }
                    $global:BackgroundJobs += $jobObj
                    Write-Host "[$($global:JobIdCounter)] $($psJob.Id)" -ForegroundColor Cyan
                    $global:JobIdCounter++
                } else {
                    wsl.exe -e bash -c $fullCommand
                }
            } catch {
                Write-Host "wsl execution error: $($_.Exception.Message)" -ForegroundColor Red
            }
        }
    }
}