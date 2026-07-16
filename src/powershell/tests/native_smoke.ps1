param(
    [string]$Binary = (Join-Path $PSScriptRoot "..\tuk_shell.exe")
)

$ErrorActionPreference = "Stop"
$binaryPath = (Resolve-Path -LiteralPath $Binary).Path

$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $binaryPath
$startInfo.WorkingDirectory = (Split-Path -Parent $binaryPath)
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardInput = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true

$process = New-Object System.Diagnostics.Process
$process.StartInfo = $startInfo
[void]$process.Start()

$outputTask = $process.StandardOutput.ReadToEndAsync()
$errorTask = $process.StandardError.ReadToEndAsync()
$commands = @(
    "echo native-ok",
    "Start-Sleep 5 &",
    "jobs",
    "top -cpu",
    "exit"
)
foreach ($command in $commands) {
    $process.StandardInput.WriteLine($command)
}
$process.StandardInput.Close()
$process.WaitForExit()
$output = $outputTask.Result
$errors = $errorTask.Result

if ($process.ExitCode -ne 0) {
    throw "tuk_shell.exe exited with code $($process.ExitCode): $errors"
}
if ($output -notmatch "native-ok") {
    throw "Native command output was not found."
}
if ($output -notmatch "RUNNING|Start-Sleep") {
    throw "Native background job was not registered."
}
if ($output -match "CreateProcess failed|fork failed|process wait failed") {
    throw "Native process execution reported an error: $errors"
}

Write-Host "PASS: native Windows smoke test"
