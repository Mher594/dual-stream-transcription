#Requires -Version 5.1
# Runs the unit tests (krisp_tests). Build first with .\scripts\build.ps1.
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '_env.ps1')

& (Resolve-DesktopExe -Name 'krisp_tests.exe')
exit $LASTEXITCODE
