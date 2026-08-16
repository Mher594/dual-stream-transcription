import {
  STREAM_MIC,
  STREAM_SPEAKER,
  helloMessage,
  captureStartedMessage,
  captureStoppedMessage,
  errorMessage,
  encodeAudioFrame,
  desktopWsUrl,
  SAMPLE_RATE,
  CHANNELS,
  DEFAULT_PORT,
} from "./protocol.js";

// Long enough to cover a slow desktop start, short enough that "is the app
// running?" arrives while the user is still looking at the popup.
const CONNECT_TIMEOUT_MS = 5000;

let ws = null;
let micStream = null;
let tabStream = null;
let micCtx = null;
let tabCtx = null;
let tabMonitor = null;
let micNode = null;
let tabNode = null;
let capturing = false;

function reportStatus(status, detail = "") {
  chrome.runtime.sendMessage({ type: "status", status, detail }).catch(() => {});
}

function sendJson(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(typeof obj === "string" ? obj : JSON.stringify(obj));
  }
}

function connectWs(port) {
  return new Promise((resolve, reject) => {
    if (ws && ws.readyState === WebSocket.OPEN) {
      resolve(ws);
      return;
    }
    const socket = new WebSocket(desktopWsUrl(port));
    socket.binaryType = "arraybuffer";
    const timer = setTimeout(() => {
      socket.close();
      reject(new Error("Desktop WebSocket connect timeout — is the app running?"));
    }, CONNECT_TIMEOUT_MS);

    socket.onopen = () => {
      clearTimeout(timer);
      ws = socket;
      socket.send(helloMessage());
      resolve(socket);
    };
    socket.onerror = () => {
      clearTimeout(timer);
      reject(new Error("WebSocket error — start the desktop app first"));
    };
    socket.onclose = async () => {
      ws = null;
      if (!capturing) return;
      // Losing the desktop ends the capture. Leaving `capturing` set would drop
      // every frame silently and make Start a no-op, because it early-returns on
      // that flag — the extension would look busy while sending nothing.
      await stopCapture();
      reportStatus("error", "Desktop connection closed — press Start to capture again");
    };
    socket.onmessage = (ev) => {
      if (typeof ev.data !== "string") return;
      try {
        const msg = JSON.parse(ev.data);
        if (msg.type === "error") {
          reportStatus("error", msg.message || "Desktop error");
        }
      } catch (_) {
        /* ignore */
      }
    };
  });
}

async function attachPcmPipeline(stream, streamId) {
  const ctx = new AudioContext({ sampleRate: SAMPLE_RATE });
  // If the browser ignores requested rate, worklet still resamples via ratio.
  await ctx.audioWorklet.addModule(chrome.runtime.getURL("worklet/pcm-processor.js"));
  const source = ctx.createMediaStreamSource(stream);
  const node = new AudioWorkletNode(ctx, "pcm-processor", {
    numberOfInputs: 1,
    numberOfOutputs: 1,
    channelCount: CHANNELS,
    processorOptions: { sampleRate: SAMPLE_RATE, streamId },
  });
  // The processor refuses to guess its rate or stream id, and a constructor
  // failure on the audio thread is otherwise invisible.
  node.onprocessorerror = () => {
    reportStatus("error", `Audio processor failed for stream ${streamId}`);
  };
  node.port.onmessage = (ev) => {
    if (!capturing || !ws || ws.readyState !== WebSocket.OPEN) return;
    const { streamId: sid, pcm } = ev.data || {};
    if (!(pcm instanceof Int16Array)) return;
    ws.send(encodeAudioFrame(sid, pcm));
  };
  // Keep graph alive without playing loud audio to speakers.
  const mute = ctx.createGain();
  mute.gain.value = 0;
  source.connect(node);
  node.connect(mute);
  mute.connect(ctx.destination);
  return { ctx, node };
}

async function startCapture({ streamId, port }) {
  if (capturing) return { ok: true };

  await connectWs(port);

  // Tab audio first: it needs no permission prompt, so a microphone problem
  // degrades to "speaker pane only" instead of killing the whole capture.
  tabStream = await navigator.mediaDevices.getUserMedia({
    audio: {
      mandatory: {
        chromeMediaSource: "tab",
        chromeMediaSourceId: streamId,
      },
    },
    video: false,
  });

  const tabPipe = await attachPcmPipeline(tabStream, STREAM_SPEAKER);
  tabCtx = tabPipe.ctx;
  tabNode = tabPipe.node;

  // tabCapture takes the tab's audio out of normal playback, so without this the
  // user hears nothing from the call. A media element plays it at full quality
  // (the capture context runs at 16 kHz) and is far less machinery than a second
  // audio graph. The mic's echoCancellation stops it looping back into the mic.
  tabMonitor = new Audio();
  tabMonitor.srcObject = tabStream;
  await tabMonitor.play();

  let micError = null;
  try {
    micStream = await navigator.mediaDevices.getUserMedia({
      audio: {
        channelCount: CHANNELS,
        echoCancellation: true,
        noiseSuppression: true,
      },
      video: false,
    });
    const micPipe = await attachPcmPipeline(micStream, STREAM_MIC);
    micCtx = micPipe.ctx;
    micNode = micPipe.node;
  } catch (err) {
    micError = err?.message || String(err);
  }

  capturing = true;
  sendJson(captureStartedMessage());

  const detail = micError
    ? `Tab audio only — microphone unavailable: ${micError}`
    : "Streaming mic + tab audio";
  reportStatus("capturing", detail);
  return { ok: true, detail, micError };
}

async function stopCapture() {
  capturing = false;
  sendJson(captureStoppedMessage());

  for (const node of [micNode, tabNode]) {
    try {
      node?.port?.close?.();
      node?.disconnect?.();
    } catch (_) {
      /* ignore */
    }
  }
  micNode = null;
  tabNode = null;

  if (tabMonitor) {
    tabMonitor.pause();
    tabMonitor.srcObject = null;
    tabMonitor = null;
  }

  for (const ctx of [micCtx, tabCtx]) {
    try {
      await ctx?.close();
    } catch (_) {
      /* ignore */
    }
  }
  micCtx = null;
  tabCtx = null;

  for (const stream of [micStream, tabStream]) {
    stream?.getTracks?.().forEach((t) => t.stop());
  }
  micStream = null;
  tabStream = null;

  if (ws) {
    try {
      ws.close();
    } catch (_) {
      /* ignore */
    }
    ws = null;
  }
  return { ok: true };
}

chrome.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  // Only claim the reply channel for messages this document owns. Replying to
  // anything else races the service worker — answering its getStatus with an
  // {ok:false} is what made the popup show "Status: unknown".
  if (message?.type !== "offscreenStart" && message?.type !== "offscreenStop") {
    return false;
  }

  (async () => {
    if (message?.type === "offscreenStart") {
      try {
        const result = await startCapture({
          streamId: message.streamId,
          port: message.port || DEFAULT_PORT,
        });
        sendResponse(result);
      } catch (err) {
        const detail = err?.message || String(err);
        // Tell the desktop before tearing the socket down: the user is watching
        // that window during a call, not this extension's popup.
        sendJson(errorMessage(detail));
        await stopCapture();
        reportStatus("error", detail);
        sendResponse({ error: detail });
      }
      return;
    }
    if (message?.type === "offscreenStop") {
      await stopCapture();
      sendResponse({ ok: true });
    }
  })();
  return true;
});
