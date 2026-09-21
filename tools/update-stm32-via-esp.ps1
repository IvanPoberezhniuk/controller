param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Left", "Right")]
    [string]$Node,

    [string]$Port = "COM3",

    [string]$Image,

    [switch]$UpdaterAlreadyRunning,

    [switch]$ResetForcedUpdater
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$role = $Node.ToLowerInvariant()
if (-not $Image) {
    $Image = Join-Path $repoRoot "build\stm32-$role-ota-release\UGV_STM32_$($Node.ToUpperInvariant()).bin"
}
$Image = (Resolve-Path $Image).Path
$imageBytes = [System.IO.File]::ReadAllBytes($Image)

if (-not ("Ugv.Crc32" -as [type])) {
    Add-Type -TypeDefinition @'
namespace Ugv {
    public static class Crc32 {
        public static uint Compute(byte[] data) {
            uint crc = 0xffffffffu;
            foreach (byte value in data) {
                crc ^= value;
                for (int bit = 0; bit < 8; ++bit) {
                    crc = (crc >> 1) ^ ((crc & 1u) != 0u ? 0xedb88320u : 0u);
                }
            }
            return ~crc;
        }
    }
}
'@
}

$crc = [Ugv.Crc32]::Compute($imageBytes)
$targetRole = if ($Node -eq "Left") { 1 } else { 2 }

$headerStream = [System.IO.MemoryStream]::new()
$writer = [System.IO.BinaryWriter]::new($headerStream)
$writer.Write([uint32]0x46564755)
$writer.Write([byte]1)
$writer.Write([byte]$targetRole)
$writer.Write([uint16]0)
$writer.Write([uint32]$imageBytes.Length)
$writer.Write([uint32]$crc)
$writer.Flush()
$header = $headerStream.ToArray()
$writer.Dispose()
$headerStream.Dispose()

$serial = [System.IO.Ports.SerialPort]::new($Port, 115200,
    [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.Handshake = [System.IO.Ports.Handshake]::None
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.NewLine = "`n"
$serial.ReadTimeout = 250
$serial.WriteTimeout = 5000

function Wait-UpdaterLine {
    param(
        [Parameter(Mandatory = $true)][string[]]$Expected,
        [int]$TimeoutMs = 60000
    )
    $deadline = [Environment]::TickCount64 + $TimeoutMs
    while ([Environment]::TickCount64 -lt $deadline) {
        try {
            $line = $serial.ReadLine().Trim()
            if ($line) {
                Write-Host "ESP: $line"
                foreach ($prefix in $Expected) {
                    if ($line.StartsWith($prefix, [StringComparison]::Ordinal)) {
                        return $line
                    }
                }
                if ($line.StartsWith("ERROR", [StringComparison]::Ordinal)) {
                    throw "ESP updater reported: $line"
                }
            }
        } catch [System.TimeoutException] {
            continue
        }
    }
    throw "Timed out waiting for ESP updater: $($Expected -join ', ')"
}

Write-Host ("Image: {0} ({1} bytes, CRC32 0x{2:X8})" -f $Image, $imageBytes.Length, $crc)
if ($ResetForcedUpdater) {
    Write-Host "Resetting the ESP into the forced updater on $Port."
} elseif ($UpdaterAlreadyRunning) {
    Write-Host "Using the updater already running on $Port."
} else {
    Write-Host "Waiting on $Port. Hold the ESP32 BOOT button for about 2 seconds while it is running."
}

try {
    $serial.Open()
    # Do not discard input here: the operator may already be holding BOOT and
    # the ESP can emit READY just before SerialPort.Open() completes. The USB
    # bridge preserves that line for us.
    if ($ResetForcedUpdater) {
        # Reset ESP while the COM handle is already open, then allow the
        # temporary forced-updater firmware to initialize UART0.
        $serial.DtrEnable = $false
        $serial.RtsEnable = $true
        Start-Sleep -Milliseconds 150
        $serial.RtsEnable = $false
        Start-Sleep -Milliseconds 2500
    } elseif (-not $UpdaterAlreadyRunning) {
        [void](Wait-UpdaterLine -Expected @("UGV-UPDATER READY") -TimeoutMs 60000)
    }

    $serial.Write($header, 0, $header.Length)
    $sendLine = Wait-UpdaterLine -Expected @("SEND ") -TimeoutMs 5000
    $requestedSize = [int]($sendLine.Substring(5).Trim())
    if ($requestedSize -ne $imageBytes.Length) {
        throw "ESP requested $requestedSize bytes, image has $($imageBytes.Length)"
    }

    $serial.Write($imageBytes, 0, $imageBytes.Length)
    [void](Wait-UpdaterLine -Expected @("BUFFERED") -TimeoutMs 35000)
    [void](Wait-UpdaterLine -Expected @("OK STM32 updated") -TimeoutMs 600000)
    Write-Host "$Node STM32 update completed successfully."
} finally {
    if ($serial.IsOpen) {
        $serial.Close()
    }
    $serial.Dispose()
}
