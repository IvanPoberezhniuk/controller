# Install the persistent UART bootloader, relocated application, and the
# metadata that makes the initial application bootable. This is the one-time
# ST-Link operation; later application updates use ESP32 -> USART2.
param(
    [ValidateSet("Left", "Right")]
    [Parameter(Mandatory)]
    [string]$Node,
    [switch]$NoBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "stm32-env.ps1")
if (-not $Stm32ProgrammerCli) {
    throw "STM32 Programmer bundle is not installed."
}

Set-Location (Join-Path $PSScriptRoot "..")

$nodeLower = $Node.ToLowerInvariant()
$nodeUpper = $Node.ToUpperInvariant()
$bootPreset = "stm32-$nodeLower-bootloader-release"
$appPreset = "stm32-$nodeLower-ota-release"
$bootElf = "build\$bootPreset\UGV_BOOTLOADER_$nodeUpper.elf"
$appElf = "build\$appPreset\UGV_STM32_$nodeUpper.elf"
$appBin = "build\$appPreset\UGV_STM32_$nodeUpper.bin"
$metadataBin = "build\$appPreset\UGV_STM32_$nodeUpper.metadata.bin"

if (-not $NoBuild) {
    foreach ($preset in @($bootPreset, $appPreset)) {
        cmake --preset $preset
        if ($LASTEXITCODE -ne 0) { exit 1 }
        cmake --build --preset $preset
        if ($LASTEXITCODE -ne 0) { exit 1 }
    }
}

foreach ($path in @($bootElf, $appElf, $appBin)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Firmware image not found: $path"
    }
}

function Get-UgvCrc32 {
    param([byte[]]$Bytes)

    [uint32]$crc = [uint32]::MaxValue
    foreach ($byte in $Bytes) {
        $crc = [uint32]($crc -bxor [uint32]$byte)
        for ($bit = 0; $bit -lt 8; $bit++) {
            [uint32]$polynomial = if (($crc -band 1u) -ne 0u) {
                0xedb88320u
            } else {
                0u
            }
            $crc = [uint32](($crc -shr 1) -bxor $polynomial)
        }
    }
    return [uint32]($crc -bxor [uint32]::MaxValue)
}

$crcSelfTest = Get-UgvCrc32 ([Text.Encoding]::ASCII.GetBytes("123456789"))
if ($crcSelfTest -ne 0xcbf43926u) {
    throw ('CRC-32 self-test failed: 0x{0:X8}' -f $crcSelfTest)
}

$application = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $appBin))
if ($application.Length -lt 8 -or $application.Length -gt (102 * 1024)) {
    throw "Invalid OTA application size: $($application.Length) bytes"
}

[uint32]$initialSp = [BitConverter]::ToUInt32($application, 0)
[uint32]$resetHandler = [BitConverter]::ToUInt32($application, 4)
[uint32]$resetAddress = $resetHandler -band 0xfffffffeu
if ($initialSp -lt 0x20000000u -or $initialSp -gt 0x20007ff8u -or
    ($initialSp -band 7u) -ne 0u) {
    throw ('Invalid application stack pointer: 0x{0:X8}' -f $initialSp)
}
if (($resetHandler -band 1u) -eq 0u -or
    $resetAddress -lt 0x08006000u -or $resetAddress -ge 0x0801f800u) {
    throw ('Application is not linked at 0x08006000 (reset=0x{0:X8})' -f
           $resetHandler)
}

[uint32]$imageCrc = Get-UgvCrc32 $application
[byte]$nodeId = if ($Node -eq "Left") { 0x10 } else { 0x11 }

$stream = [IO.MemoryStream]::new()
$writer = [IO.BinaryWriter]::new($stream)
$writer.Write([uint32]0x5547564d) # UGVM
$writer.Write([uint16]1)
$writer.Write($nodeId)
$writer.Write([byte]0)
$writer.Write([uint32]$application.Length)
$writer.Write($imageCrc)
$writer.Write([uint32]1)          # generation
$writer.Write([uint32]1)          # VALID
$writer.Write([uint32]0)
$writer.Flush()
[byte[]]$metadataPrefix = $stream.ToArray()
if ($metadataPrefix.Length -ne 28) {
    throw "Internal metadata layout error"
}
[uint32]$metadataCrc = Get-UgvCrc32 $metadataPrefix
$writer.Write($metadataCrc)
$writer.Flush()
[byte[]]$metadata = $stream.ToArray()
$writer.Dispose()
$stream.Dispose()
[IO.File]::WriteAllBytes((Join-Path (Get-Location) $metadataBin), $metadata)

Write-Host ('Application size={0} CRC32=0x{1:X8}, metadata CRC32=0x{2:X8}' -f
            $application.Length, $imageCrc, $metadataCrc)

& $Stm32ProgrammerCli -c port=SWD -w $bootElf -v
if ($LASTEXITCODE -ne 0) { exit 1 }
& $Stm32ProgrammerCli -c port=SWD -w $appElf -v
if ($LASTEXITCODE -ne 0) { exit 1 }
& $Stm32ProgrammerCli -c port=SWD -w $metadataBin 0x0801F800 -v -rst
exit $LASTEXITCODE
