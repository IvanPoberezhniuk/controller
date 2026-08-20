function Find-Stm32BundleExecutable {
    param(
        [Parameter(Mandatory)] [string]$BundleName,
        [Parameter(Mandatory)] [string]$RelativeExecutable,
        [switch]$Optional
    )

    $bundleBase = Join-Path $env:LOCALAPPDATA "stm32cube\bundles\$BundleName"
    $candidates = @()
    if (Test-Path -LiteralPath $bundleBase) {
        $candidates = Get-ChildItem -LiteralPath $bundleBase -Directory | Sort-Object {
            [version](($_.Name -split '\+')[0])
        } -Descending
    }

    foreach ($candidate in $candidates) {
        $executable = Join-Path $candidate.FullName $RelativeExecutable
        if (Test-Path -LiteralPath $executable) {
            return $executable
        }
    }

    if ($Optional) {
        return $null
    }
    throw "STM32Cube bundle executable not found: $BundleName/$RelativeExecutable"
}

$stm32Gcc = Find-Stm32BundleExecutable "gnu-tools-for-stm32" "bin\arm-none-eabi-gcc.exe"
$stm32Cmake = Find-Stm32BundleExecutable "cmake" "bin\cmake.exe"
$stm32Ninja = Find-Stm32BundleExecutable "ninja" "bin\ninja.exe"

$stm32ToolDirs = @(
    (Split-Path -Parent $stm32Gcc),
    (Split-Path -Parent $stm32Cmake),
    (Split-Path -Parent $stm32Ninja)
)
$env:Path = ($stm32ToolDirs + $env:Path) -join [IO.Path]::PathSeparator

$script:Stm32ProgrammerCli = Find-Stm32BundleExecutable `
    "programmer" "bin\STM32_Programmer_CLI.exe" -Optional
