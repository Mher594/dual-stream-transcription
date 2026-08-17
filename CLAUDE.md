# Krisp — Dual-Stream Capture

Chrome extension captures **mic** and **Meet tab audio** separately → C++ Qt desktop app transcribes both → live transcripts with the two streams kept distinct.

Source of truth for requirements: `task.md`. Don't invent requirements beyond it.

## Prime directive: simple, straightforward, elegant

This is an interview deliverable read by a human. Optimize for a reviewer who reads the diff top to bottom and understands it.

- Fewest moving parts that satisfy `task.md`. If a feature isn't in the task, it doesn't ship.
- No abstraction without a second caller. No interfaces, factories, or layers added "for later."
- Prefer editing an existing file over adding a new one. Prefer deleting over adding.
- Straight-line code over clever code. A reviewer should not have to reverse-engineer intent.
- Comments explain *why*, never *what*. Most code needs none.
- If a change makes you write a paragraph to justify it, it's probably the wrong change.

## Locked architecture

Change these only when the user explicitly asks. Never silently swap stacks.

```
Extension (mic + tab audio) ──ws://127.0.0.1:8765──► Qt desktop
                                                      ├─ 2× Deepgram STT (mic | speaker)
                                                      └─ live transcript timeline, labelled per line
```

| Concern | Choice |
|---|---|
| Transport | one WebSocket, localhost only (Qt WebSockets) |
| Wire format | text JSON control (`hello`, `hello_ack`, `capture_started`, `capture_stopped`, `error`); binary audio `[u8 stream_id][pcm s16le…]`, `0=mic`, `1=speaker` |
| Audio | PCM s16le, 16 kHz, mono |
| STT | Deepgram streaming; key lives on the **desktop only** |
| UI | Qt **6.8.3** via Conan — never system Qt, never a hardcoded `C:/Qt` |
| Port | `8765`, overridable via `KRISP_WS_PORT` |
| Config | Environment variables only: `DEEPGRAM_API_KEY` (required), `KRISP_WS_PORT`, `KRISP_STT_MODEL`. No config file, no dotfile search |
| Tests | GoogleTest + gMock `1.17.0` |
| JSON | `QJsonDocument` — no extra JSON library |

**Invariant:** `task.md` asks for the streams "kept separate so the conversation can be followed in a natural order". Both halves matter. *Separate* is structural and non-negotiable: separate capture, separate stream ids on the wire, separate STT sessions, never a mixed audio track. *Natural order* is what the UI is for — one timeline, every line naming the stream it came from. Interleaving is the point; dropping the label is the violation.

## Code

**Desktop (`desktop/`)** — C++17, CMake-only build, Conan for deps. Single-threaded on the Qt event loop; async I/O only, no worker threads. RAII everywhere, no naked `new`/`delete` outside Qt parent ownership. Fail loudly: surface transport and STT errors in both the log and the UI. Targets stay small: `krisp_core` (testable logic), `krisp_desktop` (app), `krisp_tests`.

**Extension (`extension/`)** — MV3, vanilla JS, **no npm/node tooling**. Minimum permissions for capture + localhost. Never call an STT API from the extension. Offscreen document owns capture; keep the popup to start/stop/status.

## Dependencies

Pin exact versions — never ranges, never `latest`. Prefer current LTS, else latest stable. New dependency needs a one-line justification, and prefer Conan over vendoring. Don't add a second WebSocket stack or a second test framework.

## Secrets

The key reaches the app through the environment, set by whoever launches it — the app never reads a config file. Document every variable in `README.md`. Never commit keys; never print them to logs, UI, or README. Don't commit recorded PCM, wav dumps, or transcripts containing real meeting audio. Log counters and stream labels, not payloads.

## Workflow

- Work in vertical slices: capture → transport → receive → STT → UI. After each, say what changed, how to verify, and what's still missing.
- **Never `git commit` or `git push` unless explicitly asked.**
- No drive-by refactors; don't mix formatting churn with behavior changes.
- Any change to ports, env vars, build flags, or run order updates `README.md` in the same change.
- Call out risky or irreversible choices before applying them.

## Definition of done

Builds via the documented Conan/CMake steps, `krisp_tests` pass, README still sufficient for a fresh clone, no secrets or build artifacts staged. Anything that parses or accumulates lives in `krisp_core` and gets unit tests — protocol framing, stream routing, Deepgram message assembly, transcript behaviour. Meet capture and live STT stay a manual checklist in `docs/verification.md`. Don't call something done without stating the verification path.

## Build (details in `README.md`)

PowerShell at the repo root; `build.ps1` loads the MSVC environment itself.

```powershell
.\scripts\build.ps1
.\scripts\test.ps1
.\scripts\run.ps1
```

These scripts are the documented path — keep them and `README.md` in step. Run the desktop app **before** starting extension capture.
