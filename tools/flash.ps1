# Build + flash one motor node over ST-LINK.
param(
    [ValidateSet("Left", "Right")]
    [string]$Node,
    [switch]$NoBuild
)

if (-not $Node) {
    throw "Select -Node Left or -Node Right."
}

. (Join-Path $PSScriptRoot "stm32-env.ps1")
if (-not $Stm32ProgrammerCli) {
    throw "STM32 Programmer bundle is not installed."
}

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

& $Stm32ProgrammerCli -c port=SWD -w $elf -v -rst
