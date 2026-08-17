# Krisp — Extension + Desktop Dual-Stream Pipeline

Chrome extension + C++ desktop app: capture Meet as **mic** and **tab audio** separately, transcribe with Deepgram, and show a live transcript timeline where every line is labelled with the stream it came from.

More detail: [Architecture](docs/architecture.md) · [Full verification checklist](docs/verification.md) · [Troubleshooting](docs/troubleshooting.md)

## Prerequisites

| Tool | Notes |
|------|--------|
| Windows 10/11 x64 | Primary target |
| Visual Studio 2022+ with C++ workload | MSVC + Ninja |
| CMake ≥ 3.16 | Conan builds use **Ninja** |
| Python 3.10+ | For Conan |
| Conan 2 | `pip install "conan>=2,<3"` |
| Google Chrome | Load unpacked extension |
| Deepgram API key | https://console.deepgram.com/ |

The first `conan install` downloads a prebuilt Qt when Conan Center has one for your toolchain — currently msvc `193` (VS 2022 up to 17.9). On newer MSVC there is no prebuilt and Qt compiles from source, which is a long one-time cost. The build uses your own detected Conan profile, so you get whichever applies.

## Configuration

The desktop app reads its configuration from environment variables — nothing else, no config file.

| Variable | Required | Default | Purpose |
|---|---|---|---|
| `DEEPGRAM_API_KEY` | yes | — | Deepgram streaming STT key ([console](https://console.deepgram.com/)) |
| `KRISP_WS_PORT` | no | `8765` | Port the desktop listens on for the extension |
| `KRISP_STT_MODEL` | no | `nova-2` | Deepgram model, e.g. `nova-3` — for comparing accuracy without rebuilding |

Set the key in the shell you launch from:

```powershell
$env:DEEPGRAM_API_KEY = "your_key_here"
```

That lasts for the current shell only. To persist it for future shells, run `setx DEEPGRAM_API_KEY "your_key_here"` once and open a new terminal.

For a real deployment the key belongs in the OS credential store (Windows Credential Manager, macOS Keychain). That is deliberately not wired up here: it is platform-specific code, and it would replace one shell line with a manual credential-store step.

## Build

From **PowerShell** at the repo root — no Developer Command Prompt needed, the script loads the VS tools itself:

```powershell
.\scripts\build.ps1     # conan install + cmake configure + build
.\scripts\test.ps1      # run krisp_tests
.\scripts\run.ps1       # start the desktop app
```

<details>
<summary>Equivalent manual steps</summary>

From a **Visual Studio Developer PowerShell** (these steps do not load the MSVC environment for you):

```powershell
cd desktop
conan profile detect --exist-ok
conan install . --build=missing -s:a compiler.cppstd=17 -c:a tools.cmake.cmaketoolchain:generator=Ninja

# Quote the -D arguments — PowerShell splits an unquoted one at ".cmake".
cmake -S . -B build/Release -G Ninja "-DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake" "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"

cmake --build build/Release
```

Presets (if generated): `cmake --preset conan-release` then `cmake --build --preset conan-release`.

Tests and app — `conanrun.bat` puts Qt's DLLs on `PATH`:

```powershell
cd desktop
build\Release\generators\conanrun.bat
build\Release\krisp_tests.exe      # or: ctest --test-dir build/Release --output-on-failure

$env:DEEPGRAM_API_KEY = "your_key_here"
build\Release\krisp_desktop.exe
```

</details>

## Run

**Desktop first**, then the extension.

```powershell
$env:DEEPGRAM_API_KEY = "your_key_here"
.\scripts\run.ps1
```

Expect: `Listening on ws://127.0.0.1:8765`. The terminal keeps a timestamped log of connection and STT events; the window shows the current status and the first error of any failure.

**Extension:**

1. `chrome://extensions` → Developer mode → **Load unpacked** → `extension/`
2. Open Meet (keep tab active) → extension icon → **Start**
3. First run only: a tab opens asking for microphone access → **Allow**. It closes itself, and capture continues. Chrome remembers the grant.
4. Watch the conversation timeline in the desktop app → **Stop** when done

## Quick verify

- [ ] Build + `krisp_tests` pass
- [ ] Desktop listening; extension connects; byte counters rise for both streams
- [ ] Live transcript lines labelled **You** / **Others**; you can still hear the call

Full checklist: [docs/verification.md](docs/verification.md). Problems: [docs/troubleshooting.md](docs/troubleshooting.md).

## License

Interview assignment / private use unless otherwise specified.
