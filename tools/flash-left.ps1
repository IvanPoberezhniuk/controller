param([switch]$NoBuild)

& (Join-Path $PSScriptRoot "flash.ps1") -Node Left -NoBuild:$NoBuild
exit $LASTEXITCODE
