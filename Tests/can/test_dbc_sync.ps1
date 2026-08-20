$headerPath = Resolve-Path (Join-Path $PSScriptRoot "..\..\shared\can\ugv_can_protocol.h")
$dbcPath = Resolve-Path (Join-Path $PSScriptRoot "..\..\shared\can\ugv.dbc")

$expected = @{}
foreach ($line in Get-Content -LiteralPath $headerPath) {
    if ($line -match 'X\([A-Z0-9_]+,\s*([A-Za-z0-9_]+),\s*(0x[0-9A-Fa-f]+),\s*([0-8])u\)') {
        $expected[$Matches[1]] = @{
            Id = [Convert]::ToInt32($Matches[2].Substring(2), 16)
            Dlc = [int]$Matches[3]
        }
    }
}

$actual = @{}
foreach ($line in Get-Content -LiteralPath $dbcPath) {
    if ($line -match '^BO_\s+(\d+)\s+([A-Za-z0-9_]+):\s+([0-8])\s+') {
        $actual[$Matches[2]] = @{
            Id = [int]$Matches[1]
            Dlc = [int]$Matches[3]
        }
    }
}

if ($expected.Count -eq 0) {
    throw "No CAN messages parsed from $headerPath"
}
if ($actual.Count -ne $expected.Count) {
    throw "DBC/header message-count mismatch: $($actual.Count) != $($expected.Count)"
}

foreach ($name in $expected.Keys) {
    if (-not $actual.ContainsKey($name)) {
        throw "DBC is missing message $name"
    }
    if ($actual[$name].Id -ne $expected[$name].Id -or
        $actual[$name].Dlc -ne $expected[$name].Dlc) {
        throw "DBC mismatch for $name"
    }
}

Write-Host "DBC message IDs and DLCs match ugv_can_protocol.h"
