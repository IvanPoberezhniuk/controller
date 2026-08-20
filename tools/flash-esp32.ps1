param([string]$Port)

if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    throw "idf.py is not available. Open an ESP-IDF shell first."
}

$projectDir = Resolve-Path (Join-Path $PSScriptRoot "..\firmware\esp32")
$arguments = @("-C", $projectDir, "flash")
if ($Port) {
    $arguments = @("-C", $projectDir, "-p", $Port, "flash")
}

& idf.py @arguments
exit $LASTEXITCODE
