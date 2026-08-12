# Shared helpers: load MSVC (via vcvars64) and Conan run env into the current shell.

function Get-DesktopDir {
    (Resolve-Path (Join-Path $PSScriptRoot '..\desktop')).Path
}

function Import-CmdEnvironment {
    param([string]$BatchFile)

    if (-not (Test-Path $BatchFile)) {
        throw "Not found: $BatchFile"
    }

    cmd /c "`"$BatchFile`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^(?<key>[^=]+)=(?<val>.*)$') {
            Set-Item -Path "env:$($matches.key)" -Value $matches.val
        }
    }
}

function Import-VsDevEnvironment {
    if ((Get-Command cl -ErrorAction SilentlyContinue) -and (Get-Command ninja -ErrorAction SilentlyContinue)) {
        return
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw 'vswhere not found. Install Visual Studio with the Desktop development with C++ workload.'
    }

    $vsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsPath) {
        throw 'Visual Studio C++ tools not found.'
    }

    Import-CmdEnvironment -BatchFile (Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat')

    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        throw 'Failed to load MSVC (cl.exe not on PATH).'
    }
    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        throw 'ninja not found after loading vcvars64. Reinstall the VS C++ workload.'
    }
}

# Runs a build step and stops the script if the tool reports failure. Native
# commands do not raise on non-zero, so $LASTEXITCODE has to be checked by hand.
function Invoke-Step {
    param(
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][scriptblock]$Action
    )

    Write-Host $Label -ForegroundColor Cyan
    & $Action
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed (exit $LASTEXITCODE)"
    }
}

# Checks one of the built executables exists and makes Qt's DLLs resolvable via
# Conan, then hands back its path. The caller invokes it, so the program's output
# streams to the console instead of becoming this function's return value.
function Resolve-DesktopExe {
    param([Parameter(Mandatory)][string]$Name)

    $desktop = Get-DesktopDir
    $exe = Join-Path $desktop "build\Release\$Name"
    if (-not (Test-Path $exe)) {
        throw "$Name not found. Run scripts\build.ps1 first."
    }

    $conanrun = Join-Path $desktop 'build\Release\generators\conanrun.bat'
    if (-not (Test-Path $conanrun)) {
        throw 'conanrun.bat not found. Run scripts\build.ps1 first.'
    }
    Import-CmdEnvironment -BatchFile $conanrun

    return $exe
}
