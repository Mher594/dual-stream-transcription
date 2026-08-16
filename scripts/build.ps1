#Requires -Version 5.1
# Builds the desktop app and tests. Loads MSVC automatically — plain PowerShell is enough.
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '_env.ps1')

Push-Location (Get-DesktopDir)
try {
    Import-VsDevEnvironment

    # Detect the profile rather than shipping one. A pinned compiler version
    # sends every other machine into an hours-long Qt source build instead of
    # downloading the prebuilt that matches its own toolchain.
    Invoke-Step '[1/4] Conan profile...' {
        conan profile detect --exist-ok
    }
    Invoke-Step '[2/4] Conan install...' {
        conan install . --build=missing `
            -s:a compiler.cppstd=17 `
            -c:a tools.cmake.cmaketoolchain:generator=Ninja
    }
    Invoke-Step '[3/4] CMake configure...' {
        # Quote the -D arguments. PowerShell splits an unquoted one at ".cmake",
        # so cmake would receive a toolchain path with the extension missing —
        # invisible on a warm CMakeCache, fatal on a fresh clone.
        cmake -S . -B build/Release -G Ninja `
            "-DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake" `
            "-DCMAKE_BUILD_TYPE=Release" `
            "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"
    }
    Invoke-Step '[4/4] Build...' {
        cmake --build build/Release
    }

    Write-Host 'Build OK. Next: .\scripts\test.ps1' -ForegroundColor Green
}
catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
finally {
    Pop-Location
}
