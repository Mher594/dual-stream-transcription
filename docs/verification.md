# Manual verification checklist

Use after a fresh clone on Windows. Automated tests cover the wire protocol, stream routing, Deepgram message assembly and transcript behaviour; Meet capture and live STT stay manual.

## Build and tests

- [ ] `.\scripts\build.ps1` succeeds (see [README](../README.md))
- [ ] `.\scripts\test.ps1` reports all tests passing

## Desktop

- [ ] App starts and shows `Listening on ws://127.0.0.1:8765` (or your `KRISP_WS_PORT`)
- [ ] Red error line on startup when `DEEPGRAM_API_KEY` is unset, and none when it is set

## Extension and capture

- [ ] Extension loads unpacked from `extension/`
- [ ] With desktop running, **Start** capture on an active Meet tab
- [ ] First run: the microphone permission tab appears, **Allow** closes it and capture continues
- [ ] Mic and speaker byte counters increase in the desktop window
- [ ] You can still hear remote participants while capturing
- [ ] Live transcripts appear in the conversation timeline, each line stamped and labelled **You** (green) or **Others** (blue)
- [ ] A sentence settles into one line when the speaker pauses, not several fragments mid-sentence
- [ ] In-flight speech shows greyed and italic with a `···` stamp, and is replaced by the settled line
- [ ] Scrolling back mid-call stays put; the timeline only follows new lines when already at the bottom
- [ ] **Clear transcripts** empties the timeline and disables itself; capture keeps running
- [ ] Status line reads `STT: mic connected, speaker connected` while capturing
- [ ] Popup lists **Microphone** (stream 0) and **Meet tab audio** (stream 1) while capturing, with a pulsing green dot
- [ ] Toolbar badge shows a green dot while capturing and clears on stop
- [ ] **Stop** cleans up; status returns to idle/listening

## Notes

- Meet tab must be **active** when pressing Start (tab capture targets that tab).
- Start the **desktop app before** the extension.
- Mic = local user; Speaker = tab audio (remote participants).
