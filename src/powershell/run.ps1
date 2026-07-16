$ErrorActionPreference = "Stop"
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDirectory
try {
    if (-not (Get-Command "mingw32-make.exe" -ErrorAction SilentlyContinue)) {
        throw "mingw32-make.exe was not found. Install the MinGW-w64 toolchain first."
    }

    mingw32-make.exe -f Makefile.win
    & (Join-Path $scriptDirectory "tuk_shell.exe")
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
