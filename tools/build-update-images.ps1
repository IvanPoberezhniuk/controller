param(
    [switch]$FinalPinout
)

. (Join-Path $PSScriptRoot "stm32-env.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$pinoutValue = if ($FinalPinout) { "ON" } else { "OFF" }

Push-Location $repoRoot
try {
    foreach ($role in @("left", "right")) {
        $bootPreset = "stm32-$role-bootloader-release"
        cmake --preset $bootPreset
        if ($LASTEXITCODE -ne 0) { throw "Configure failed: $bootPreset" }
        cmake --build --preset $bootPreset
        if ($LASTEXITCODE -ne 0) { throw "Build failed: $bootPreset" }

        $appPreset = "stm32-$role-ota-release"
        cmake --preset $appPreset "-DUGV_FINAL_OTA_PINOUT=$pinoutValue"
        if ($LASTEXITCODE -ne 0) { throw "Configure failed: $appPreset" }
        cmake --build --preset $appPreset
        if ($LASTEXITCODE -ne 0) { throw "Build failed: $appPreset" }

        $image = Join-Path $repoRoot "build\$appPreset\UGV_STM32_$($role.ToUpper()).bin"
        python tools\ugv_can_update.py --node $role --image $image --dry-run
        if ($LASTEXITCODE -ne 0) { throw "OTA image validation failed: $image" }
    }

    Write-Host "Bootloader and OTA application images are ready under build/."
} finally {
    Pop-Location
}
