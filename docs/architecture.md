# Architecture

Dual-stream capture from Google Meet: microphone and tab audio stay separate end-to-end, transcribed on the desktop, shown in two panes.

## Pipeline

```
Chrome extension                         C++ desktop app (Qt 6.8.3 via Conan)
────────────────                         ────────────────────────────────────
Mic capture  ──┐                         ┌─ WebSocket server (:8765)
               ├──► WS localhost ───────►├─ Deepgram STT (mic + speaker)
Tab capture  ──┘    PCM s16le 16 kHz     └─ dual transcript UI
```

Extension → localhost WebSocket → desktop receiver → two Deepgram sessions → Qt UI.

## Stack

Qt **6.8.3** via Conan (never system Qt) — Qt WebSockets serves the extension *and* drives the two Deepgram clients, and `QJsonDocument` handles the control messages, so there is no second WebSocket or JSON library. Tests are GoogleTest + gMock **1.17.0**. Audio is PCM s16le, 16 kHz, mono throughout.

These choices are locked; [CLAUDE.md](../CLAUDE.md) has the full table and the conventions that go with them.

## Wire protocol

**Control (JSON text frames):** `hello`, `hello_ack`, `capture_started`, `capture_stopped`, `error`

**Audio (binary):** `[uint8 stream_id][pcm s16le…]` — `0=mic`, `1=speaker`

Extension connects to `ws://127.0.0.1:${KRISP_WS_PORT}`.

## Runtime model

- Desktop app logic runs on the **Qt main event loop** (async WebSockets only; no worker threads).
- Configuration is read from environment variables only — no config file, no dotfile search. Whoever launches the app sets them, the same contract the AWS SDK and most C++ services use.
- Extension uses MV3 service worker + **offscreen document** for capture and WebSocket I/O.
- Tab audio is played back through a media element in the offscreen document; tab capture otherwise removes it from normal playback and the call goes silent.
- Deepgram finalises fragments far more often than a speaker pauses, so fragments are accumulated and committed as one line on `speech_final` (see `UtteranceAssembler`).
- Microphone access is granted once from a visible tab (`permission.html`); the offscreen document has no window for Chrome to prompt in, and the grant is stored per extension origin.

## Repository layout

```
scripts/                   # build.ps1 / test.ps1 / run.ps1 — the documented entry point
extension/                 # Chrome MV3 (load unpacked)
desktop/src/               # C++ Qt app (CMake + Conan)
desktop/tests/             # GoogleTest suites
docs/                      # Architecture, verification, troubleshooting
CLAUDE.md                  # Working conventions and the locked-decision table
task.md
```

The desktop splits into three CMake targets: `krisp_core` holds the logic worth
testing (wire protocol, Deepgram message assembly, transcript model),
`krisp_desktop` adds the sockets and the Qt window on top, and
`krisp_tests` covers `krisp_core`. Anything that parses or accumulates belongs
in `krisp_core`, so it can be tested without a network.
