# Architecture

Dual-stream capture from Google Meet: microphone and tab audio stay separate end-to-end, transcribed on the desktop, and shown as one timeline where every line is labelled with the stream it came from.

## Pipeline

```
Chrome extension                         C++ desktop app (Qt 6.8.3 via Conan)
────────────────                         ────────────────────────────────────
Mic capture  ──┐                         ┌─ WebSocket server (:8765)
               ├──► WS localhost ───────►├─ Deepgram STT (mic + speaker)
Tab capture  ──┘    PCM s16le 16 kHz     └─ labelled transcript timeline
```

Extension → localhost WebSocket → desktop receiver → two Deepgram sessions → Qt UI.

## Keeping the streams separate

`task.md` asks for the transcripts to be shown "with microphone and speaker streams
kept separate so the conversation can be followed in a natural order". Both halves
shape the design.

*Separate* is structural: the extension captures two `MediaStream`s and never mixes
them, each frame is tagged `0=mic` / `1=speaker` on the wire, and the desktop runs an
independent Deepgram session per stream. Nothing downstream can confuse them.

*Natural order* is what the window is for. Two side-by-side panes keep the streams
apart but make the conversation harder to follow — a reader has to re-interleave them
by timestamp. So the transcripts share one timeline in the order they were spoken,
and every line carries its stream: a green **You** chip or a blue **Others** chip,
with the legend naming them in full. Separation is preserved as identity on each
line, which is what makes the order readable rather than working against it.

## Stack

Qt **6.8.3** via Conan (never system Qt) — Qt WebSockets serves the extension *and* drives the two Deepgram clients, and `QJsonDocument` handles the control messages, so there is no second WebSocket or JSON library. Tests are GoogleTest **1.17.0**. Audio is PCM s16le, 16 kHz, mono throughout.

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
- `WsServer` announces audio per stream rather than pushing it into an STT client; `main.cpp` is the only place that knows Deepgram transcribes it. That is what lets stream routing be unit-tested without a socket or a key.
- Deepgram finalises fragments far more often than a speaker pauses, so fragments are accumulated and committed as one line on `speech_final` (see `UtteranceAssembler`).
- Microphone access is granted once from a visible tab (`permission.html`); the offscreen document has no window for Chrome to prompt in, and the grant is stored per extension origin.

## Repository layout

```
scripts/                   # build.ps1 / test.ps1 / run.ps1 — the documented entry point
extension/                 # Chrome MV3 (load unpacked)
desktop/src/               # C++ Qt app (CMake + Conan)
desktop/tests/             # GoogleTest suites
docs/                      # Task brief, architecture, troubleshooting
CLAUDE.md                  # Working conventions and the locked-decision table
```

The desktop splits into three CMake targets: `krisp_core` holds the logic worth
testing (wire protocol, stream routing, Deepgram message assembly, transcript
model), `krisp_desktop` adds the Deepgram clients and the Qt window on top, and
`krisp_tests` covers `krisp_core`. Anything that parses, routes or accumulates
belongs in `krisp_core`, so it can be tested without a network.
