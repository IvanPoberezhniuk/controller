# Live debug-UART console: prints telemetry and sends typed commands.
# The firmware's safety layer requires a fresh command at least every 300 ms
# (command_timeout_ms) while armed, so the last typed command is auto-repeated
# every 150 ms -- type a new one to replace it. Commands: arm, estop, clear,
# m<idx> <rpm> (e.g. "m0 100"). Ctrl+C to exit.
# Usage: powershell -ExecutionPolicy Bypass -File tools\console.ps1 [-Port COM8]
param(
    [string]$Port = "COM8",
    [int]$Baud = 115200
)

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
$sp.ReadTimeout = 50
$sp.Open()
Write-Host "Connected to $Port @ $Baud. Type a command and press Enter (arm / m0 100 / m0 0 / estop / clear). Ctrl+C to exit." -ForegroundColor Green

$typed = ""
$lastCmd = $null
$nextSend = Get-Date

try {
    while ($true) {
        while ($sp.BytesToRead -gt 0) {
            try { Write-Host $sp.ReadLine().TrimEnd() } catch { break }
        }

        while ([Console]::KeyAvailable) {
            $k = [Console]::ReadKey($true)
            if ($k.Key -eq [ConsoleKey]::Enter) {
                if ($typed.Length -gt 0) {
                    $lastCmd = $typed
                    $typed = ""
                    $nextSend = Get-Date
                    Write-Host ">> $lastCmd (repeating every 150 ms)" -ForegroundColor Cyan
                }
            } elseif ($k.Key -eq [ConsoleKey]::Backspace) {
                if ($typed.Length -gt 0) { $typed = $typed.Substring(0, $typed.Length - 1) }
            } else {
                $typed += $k.KeyChar
            }
        }

        if ($lastCmd -and (Get-Date) -ge $nextSend) {
            $sp.Write("$lastCmd`r`n")
            $nextSend = (Get-Date).AddMilliseconds(150)
        }

        Start-Sleep -Milliseconds 20
    }
} finally {
    $sp.Close()
}
