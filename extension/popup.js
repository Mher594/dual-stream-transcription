import { DEFAULT_PORT } from "./protocol.js";

const el = {
  status: document.getElementById("status"),
  dot: document.getElementById("dot"),
  streams: document.getElementById("streams"),
  alert: document.getElementById("alert"),
  start: document.getElementById("start"),
  stop: document.getElementById("stop"),
  port: document.getElementById("port"),
  portField: document.getElementById("portField"),
  hint: document.getElementById("hint"),
};

function setUi(state) {
  const status = state.status || "unknown";
  const capturing = status === "capturing";
  const busy = capturing || status === "connecting";
  const failed = status === "error";

  // The red panel carries the error text, so the status line stays short rather
  // than saying the same thing twice.
  el.status.textContent = failed
    ? "Status: error"
    : `Status: ${status}${state.detail ? ` — ${state.detail}` : ""}`;
  el.dot.className = `dot${capturing ? " live" : failed ? " bad" : ""}`;

  el.streams.hidden = !capturing;
  el.alert.hidden = !failed;
  el.alert.textContent = failed ? state.detail || "Capture failed." : "";

  el.start.disabled = busy;
  el.stop.disabled = !busy;
  el.port.disabled = busy;
  el.portField.classList.toggle("invalid", failed);
  el.hint.hidden = busy;
}

async function refresh() {
  const state = await chrome.runtime.sendMessage({ type: "getStatus" });
  if (state) setUi(state);
}

el.start.addEventListener("click", async () => {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  if (!tab?.id) {
    setUi({ status: "error", detail: "No active tab to capture." });
    return;
  }
  const port = Number(el.port.value) || DEFAULT_PORT;
  await chrome.storage.local.set({ wsPort: port });
  const result = await chrome.runtime.sendMessage({
    type: "startCapture",
    tabId: tab.id,
    port,
  });
  if (result?.error) {
    setUi({ status: "error", detail: result.error });
  } else {
    await refresh();
  }
});

el.stop.addEventListener("click", async () => {
  await chrome.runtime.sendMessage({ type: "stopCapture" });
  await refresh();
});

el.port.value = String(DEFAULT_PORT);
chrome.storage.local.get(["wsPort"]).then((v) => {
  if (v.wsPort) el.port.value = String(v.wsPort);
});

chrome.runtime.onMessage.addListener((msg) => {
  if (msg?.type === "status") setUi(msg);
});

refresh();
