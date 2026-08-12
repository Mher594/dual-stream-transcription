N T E RV I E W TA S K
Extension + Desktop Dual-Stream Pipeline
Goal
Imagine a Google Meet call happening in the browser. Build a Chrome extension and a C++
desktop app that work together.
Constraints
The Chrome extension captures two audio streams separately in the browser and sends
them to the desktop app:
Microphone — what the local user says
Tab audio — what the user hears from the Google Meet tab (remote participants)
The desktop app does the rest:
Receive the streams from the extension
Transcribe both streams (e.g. with a well-known transcription engine)
Show live or near-live transcripts in the desktop app, with microphone and speaker
streams kept separate so the conversation can be followed in a natural order
Chrome extension, loadable unpacked.
Desktop app in C++.
Must work on at least Windows or macOS.
Feel free to use any open-source libraries.
Use any STT and LLM service you like. (e.g. Deepgram's STT trial provides enough tokens
for this task)
Use CMake as the build system.
Use Conan for dependency management.
The project must be fully buildable and runnable from the README (no missing steps or
broken setup).
Ship a README: how to build/run it (extension + desktop).
AI usage
Any AI usage is allowed as long as you are the orchestrator of it. If you use AI, it is your
responsibility to manage it wisely to create a reasonable solution for the task that covers all
requirements.