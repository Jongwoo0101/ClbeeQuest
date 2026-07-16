# tuk_shell_launcher.ps1
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "🚀 TUK-Shell Launcher를 초기화합니다..." -ForegroundColor Cyan

# [1] wsl.exe 설치 및 접근성 검증
if (-not (Get-Command "wsl.exe" -ErrorAction SilentlyContinue)) {
    Write-Host "[ERROR] 시스템에서 wsl.exe를 찾을 수 없습니다. WSL이 설치되어 있는지 확인해 주세요." -ForegroundColor Red
    Exit 1
}

# [2] Windows 경로를 WSL 경로로 자동 변환 (예: C:\Users\Donghyun -> /mnt/c/Users/Donghyun)
$winPath = (Get-Location).Path
$driveLetter = $winPath.Substring(0, 1).ToLower()
# 드라이브 문자를 제외한 나머지 경로의 역슬래시를 슬래시로 치환
$wslPath = "/mnt/$driveLetter" + $winPath.Substring(2).Replace('\', '/')

Write-Host "[INFO] 작업 경로 변환 완료: $wslPath" -ForegroundColor DarkGray

# [3] WSL 내부에 실행 파일(./tuk_shell)이 존재하는지 사전 검증
# bash의 test -x 명령어로 파일 존재 및 실행 권한 확인
wsl.exe -e bash -c "test -x `"$wslPath/tuk_shell`""
if ($LASTEXITCODE -ne 0) {
    Write-Host "`n[ERROR] 해당 경로('$wslPath')에서 'tuk_shell' 실행 파일을 찾을 수 없거나 실행 권한이 없습니다." -ForegroundColor Red
    Write-Host "[HINT] 1. gcc를 사용해 프로젝트가 정상적으로 컴파일되었는지 확인하세요." -ForegroundColor Yellow
    Write-Host "[HINT] 2. 'chmod +x tuk_shell' 명령으로 실행 권한을 부여했는지 확인하세요.`n" -ForegroundColor Yellow
    Exit 1
}

# [4] 포그라운드(대화형) 실행 연결
Write-Host "[INFO] TUK-Shell 환경으로 진입합니다..." -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor DarkGray

# bash를 통해 변환된 디렉토리로 이동 후 쉘 실행
wsl.exe -e bash -c "cd `"$wslPath`" && ./tuk_shell"

# [5] 쉘 종료 후 예외 상황 처리
if ($LASTEXITCODE -ne 0) {
    Write-Host "`n[WARN] TUK-Shell이 비정상 종료되었거나 에러 코드를 반환했습니다. (Exit Code: $LASTEXITCODE)" -ForegroundColor Yellow
} else {
    Write-Host "`n[INFO] TUK-Shell이 정상적으로 종료되었습니다." -ForegroundColor Green
}