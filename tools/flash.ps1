# Build + flash one motor node over ST-LINK.
param(
    [ValidateSet("Left", "Right")]
    [string]$Node,
    [switch]$NoBuild
)

if (-not $Node) {
    throw "Select -Node Left or -Node Right."
}

$bundles = "$env:LOCALAPPDATA\stm32cube\bundles"
$env:Path = "$bundles\gnu-tools-for-stm32\14.3.1+st.2\bin;" +
            "$bundles\cmake\4.3.1+st.1\bin;" +
            "$bundles\ninja\1.13.2+st.1\bin;" + $env:Path

Set-Location (Join-Path $PSScriptRoot "..")

$nodeLower = $Node.ToLowerInvariant()
$nodeUpper = $Node.ToUpperInvariant()
$preset = "stm32-$nodeLower-debug"
$elf = "build\$preset\UGV_STM32_$nodeUpper.elf"

if (-not $NoBuild) {
    cmake --preset $preset
    if ($LASTEXITCODE -ne 0) {
        Write-Host "CONFIGURE FAILED" -ForegroundColor Red
        exit 1
    }

    cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) {
        Write-Host "BUILD FAILED" -ForegroundColor Red
        exit 1
    }
}

if (-not (Test-Path -LiteralPath $elf)) {
    throw "Firmware image not found: $elf"
}

& "$bundles\programmer\2.22.0+st.1\bin\STM32_Programmer_CLI.exe" `
    -c port=SWD -w $elf -v -rst
