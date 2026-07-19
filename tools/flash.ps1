# Build + flash over ST-LINK. Usage (from repo root or anywhere):
#   powershell -ExecutionPolicy Bypass -File tools\flash.ps1
#   ... -NoBuild     # flash the existing build\Debug\UGV_Controllers.elf as-is
param([switch]$NoBuild)

$bundles = "$env:LOCALAPPDATA\stm32cube\bundles"
$env:Path = "$bundles\gnu-tools-for-stm32\14.3.1+st.2\bin;" +
            "$bundles\cmake\4.3.1+st.1\bin;" +
            "$bundles\ninja\1.13.2+st.1\bin;" + $env:Path

Set-Location (Join-Path $PSScriptRoot "..")

if (-not $NoBuild) {
    cmake --build build/Debug
    if ($LASTEXITCODE -ne 0) {
        Write-Host "BUILD FAILED" -ForegroundColor Red
        exit 1
    }
}

& "$bundles\programmer\2.22.0+st.1\bin\STM32_Programmer_CLI.exe" `
    -c port=SWD -w "build\Debug\UGV_Controllers.elf" -v -rst
