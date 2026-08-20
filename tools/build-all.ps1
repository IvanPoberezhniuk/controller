param(
    [switch]$SkipEsp32,
    [switch]$SkipHostTests
)

. (Join-Path $PSScriptRoot "stm32-env.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Push-Location $repoRoot
try {
    foreach ($preset in @("stm32-left-debug", "stm32-right-debug")) {
        cmake --preset $preset
        if ($LASTEXITCODE -ne 0) { throw "Configure failed: $preset" }
        cmake --build --preset $preset
        if ($LASTEXITCODE -ne 0) { throw "Build failed: $preset" }
    }

    if (-not $SkipHostTests) {
        & (Join-Path $PSScriptRoot "test-host.ps1")
    }

    if (-not $SkipEsp32) {
        if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
            throw "idf.py is not available. Run from an ESP-IDF shell or pass -SkipEsp32."
        }
        idf.py -C firmware\esp32 build
        if ($LASTEXITCODE -ne 0) { throw "ESP32 build failed." }
    }
} finally {
    Pop-Location
}
