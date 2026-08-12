#Requires -Version 5.1
# Starts the desktop app. Run this before starting capture in the extension.
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '_env.ps1')

& (Resolve-DesktopExe -Name 'krisp_desktop.exe')
exit $LASTEXITCODE
