const statusEl = document.getElementById("status");
const startBtn = document.getElementById("start");
const stopBtn = document.getElementById("stop");
const portInput = document.getElementById("port");

function setUi(state) {
  statusEl.textContent = `Status: ${state.status || "unknown"}`;
  if (state.detail) {
    statusEl.textContent += ` — ${state.detail}`;
  }
  const capturing = state.status === "capturing" || state.status === "connecting";
  startBtn.disabled = capturing;
  stopBtn.disabled = !capturing && state.status !== "connected";
  if (state.status === "capturing") {
    stopBtn.disabled = false;
    startBtn.disabled = true;
  }
  if (state.status === "idle" || state.status === "error" || state.status === "stopped") {
    startBtn.disabled = false;
    stopBtn.disabled = true;
  }
}

async function refresh() {
  const state = await chrome.runtime.sendMessage({ type: "getStatus" });
  if (state) setUi(state);
}

startBtn.addEventListener("click", async () => {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  if (!tab?.id) {
    setUi({ status: "error", detail: "No active tab" });
    return;
  }
  const port = Number(portInput.value) || 8765;
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

stopBtn.addEventListener("click", async () => {
  await chrome.runtime.sendMessage({ type: "stopCapture" });
  await refresh();
});

chrome.storage.local.get(["wsPort"]).then((v) => {
  if (v.wsPort) portInput.value = String(v.wsPort);
});

chrome.runtime.onMessage.addListener((msg) => {
  if (msg?.type === "status") setUi(msg);
});

refresh();
