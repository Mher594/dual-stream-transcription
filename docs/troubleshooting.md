# Troubleshooting

| Symptom | Fix |
|---------|-----|
| Extension: WebSocket timeout | Start desktop first; check `KRISP_WS_PORT` matches the extension popup |
| No tab audio / byte counter stuck on speaker | Meet tab must be active on Start; reload the extension |
| Mic counter stuck / popup says "microphone unavailable" | Chrome cannot prompt from the invisible offscreen document, so the extension opens a tab to ask on first Start. If it was denied earlier, clear the block at `chrome://settings/content/microphone` and press Start again. Tab audio keeps working meanwhile |
| No transcripts | Read the red error line in the desktop window — it keeps the *first* (root) cause, e.g. `API key rejected (HTTP 401)`. It clears when both STT streams reconnect. The terminal running the app has the full timestamped sequence |
| Status says `STT: mic down, speaker down` | Audio is arriving from the extension but Deepgram is not connected — the red line and the terminal log say why |
| Can't hear the call while capturing | Reload the extension; tab audio is monitored back to speakers from the offscreen document |
| `DEEPGRAM_API_KEY` not picked up | Copy `.env.example` → `.env` at repo root; real env vars override the file. Restart the desktop app |
| `conan install` / CMake “Visual Studio …” generator error | The build passes `-c:a tools.cmake.cmaketoolchain:generator=Ninja`; use `.\scripts\build.ps1` rather than a bare `conan install` |
| Transcription accuracy is poor | Try another model: set `KRISP_STT_MODEL` (e.g. `nova-3`) in `.env` and restart the desktop app. The log line `connecting with model …` confirms which one is live |
| Missing Qt DLLs at runtime | Run via `build/Release/generators/conanrun.bat` before the exe, or add Qt `bin` to `PATH` |
| First `conan install` takes hours | Conan Center ships a prebuilt Qt for msvc `193` (VS 2022 ≤ 17.9) only; newer MSVC builds it from source. One-time cost, then cached |
