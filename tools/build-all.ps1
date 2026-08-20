param([switch]$SkipEsp32)

$bundles = "$env:LOCALAPPDATA\stm32cube\bundles"
$env:Path = "$bundles\gnu-tools-for-stm32\14.3.1+st.2\bin;" +
            "$bundles\cmake\4.3.1+st.1\bin;" +
            "$bundles\ninja\1.13.2+st.1\bin;" + $env:Path

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Push-Location $repoRoot
try {
    foreach ($preset in @("stm32-left-debug", "stm32-right-debug")) {
        cmake --preset $preset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        cmake --build --preset $preset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    if (-not $SkipEsp32) {
        if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
            throw "idf.py is not available. Run from an ESP-IDF shell or pass -SkipEsp32."
        }
        idf.py -C firmware\esp32 build
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
} finally {
    Pop-Location
}
