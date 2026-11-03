param()
$ErrorActionPreference = 'Stop'
$veilPublicationCheck = Join-Path $PSScriptRoot 'check-publication.py'
& python $veilPublicationCheck
exit $LASTEXITCODE
