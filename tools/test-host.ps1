$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildDir = Join-Path $repoRoot "build\host-tests"
$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gcc) {
    throw "Host gcc is required to run the C unit tests."
}

New-Item -ItemType Directory -Force $buildDir | Out-Null
Push-Location $repoRoot
try {
    function Build-And-Run {
        param(
            [Parameter(Mandatory)] [string]$Name,
            [Parameter(Mandatory)] [string[]]$Arguments
        )

        $executable = Join-Path $buildDir "$Name.exe"
        & $gcc.Source @Arguments -o $executable
        if ($LASTEXITCODE -ne 0) {
            throw "Host test compilation failed: $Name"
        }
        & $executable
        if ($LASTEXITCODE -ne 0) {
            throw "Host test failed: $Name"
        }
    }

    Build-And-Run "test_motor_math" @(
        "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-Ifirmware/stm32-common/Application/Inc",
        "Tests/stm32/test_motor_math.c",
        "firmware/stm32-common/Application/Src/motor_math.c", "-lm"
    )

    Build-And-Run "test_can_codec" @(
        "-std=c11", "-Wall", "-Wextra", "-Werror", "-Ishared/can",
        "Tests/can/test_can_codec.c", "shared/can/ugv_can_codec.c"
    )

    Build-And-Run "test_fw_update_protocol" @(
        "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-Ishared/can", "-Ishared/update",
        "Tests/update/test_fw_update_protocol.c",
        "shared/update/ugv_fw_update_protocol.c",
        "shared/update/ugv_crc32.c"
    )

    $stm32TestIncludes = @(
        "-std=c11", "-Wall", "-Wextra", "-Werror", "-DUGV_NODE_ROLE_LEFT",
        "-ITests/stm32/fakes", "-Ifirmware/stm32-common/Application/Inc",
        "-Ifirmware/stm32-common/Platform/Inc", "-Ifirmware/stm32-left",
        "-Ishared/can"
    )
    Build-And-Run "test_safety" ($stm32TestIncludes + @(
        "Tests/stm32/test_safety.c",
        "firmware/stm32-common/Application/Src/safety.c"
    ))
    Build-And-Run "test_fault_manager" ($stm32TestIncludes + @(
        "Tests/stm32/test_fault_manager.c",
        "firmware/stm32-common/Application/Src/fault_manager.c"
    ))

    & (Join-Path $repoRoot "Tests\can\test_dbc_sync.ps1")
    Write-Host "All host tests passed." -ForegroundColor Green
} finally {
    Pop-Location
}
