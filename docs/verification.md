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
- [ ] Live transcripts appear in **Microphone** and **Speaker** panes separately
- [ ] Final lines carry an `[HH:mm:ss]` stamp, so the two panes can be read as one conversation
- [ ] A sentence settles into one line when the speaker pauses, not several fragments mid-sentence
- [ ] In-flight speech shows greyed and unstamped *below* each pane, then moves up as a stamped line
- [ ] Scrolling back through a pane mid-call stays put; it only follows new lines when already at the bottom
- [ ] Status line reads `STT: mic connected, speaker connected` while capturing
- [ ] **Stop** cleans up; status returns to idle/listening

## Notes

- Meet tab must be **active** when pressing Start (tab capture targets that tab).
- Start the **desktop app before** the extension.
- Mic = local user; Speaker = tab audio (remote participants).
