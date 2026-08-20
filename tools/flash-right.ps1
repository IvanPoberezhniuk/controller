param([switch]$NoBuild)

& (Join-Path $PSScriptRoot "flash.ps1") -Node Right -NoBuild:$NoBuild
exit $LASTEXITCODE
