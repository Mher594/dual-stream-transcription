const OFFSCREEN_URL = "offscreen.html";
const PERMISSION_URL = "permission.html";

let status = { status: "idle", detail: "" };

// Chrome recycles the service worker freely, which resets the variable above
// while capture carries on in the offscreen document. Session storage survives
// that; it is cleared when the browser closes, which is exactly the lifetime we
// want.
function broadcastStatus(next) {
  status = next;
  chrome.storage.session.set({ status: next }).catch(() => {});
  chrome.runtime.sendMessage({ type: "status", ...status }).catch(() => {});
}

async function currentStatus() {
  const stored = await chrome.storage.session.get("status");
  if (stored.status) status = stored.status;
  // Stored state can outlive the thing it describes: capture only exists while
  // the offscreen document does.
  if (status.status === "capturing" && !(await hasOffscreen())) {
    broadcastStatus({ status: "stopped", detail: "Capture stopped" });
  }
  return status;
}

// Chrome will not show a microphone prompt for the offscreen document because it
// has no visible window, so ask once from a real tab. The grant belongs to the
// extension origin, so the offscreen document inherits it from then on.
async function ensureMicPermission() {
  if ((await chrome.storage.local.get("micGranted")).micGranted) return;

  try {
    const micPermission = await navigator.permissions.query({ name: "microphone" });
    if (micPermission.state === "granted") {
      await chrome.storage.local.set({ micGranted: true });
      return;
    }
  } catch (_) {
    // Permissions API unavailable in this context — just ask.
  }

  const tab = await chrome.tabs.create({ url: PERMISSION_URL, active: true });
  if (await waitForMicPermission()) {
    await chrome.storage.local.set({ micGranted: true });
    try {
      await chrome.tabs.remove(tab.id);
    } catch (_) {
      /* already closed */
    }
  }
  // On denial the tab stays open showing what to do; capture continues with tab
  // audio only rather than failing outright.
}

function waitForMicPermission() {
  return new Promise((resolve) => {
    const done = (granted) => {
      clearTimeout(timer);
      chrome.runtime.onMessage.removeListener(onMessage);
      resolve(granted);
    };
    const onMessage = (message) => {
      if (message?.type === "micPermissionResult") done(Boolean(message.granted));
    };
    const timer = setTimeout(() => done(false), 60000);
    chrome.runtime.onMessage.addListener(onMessage);
  });
}

async function hasOffscreen() {
  if (!chrome.offscreen?.hasDocument) {
    const contexts = await chrome.runtime.getContexts({
      contextTypes: ["OFFSCREEN_DOCUMENT"],
    });
    return contexts.length > 0;
  }
  return chrome.offscreen.hasDocument();
}

async function ensureOffscreen() {
  if (await hasOffscreen()) return;
  await chrome.offscreen.createDocument({
    url: OFFSCREEN_URL,
    reasons: ["USER_MEDIA", "AUDIO_PLAYBACK"],
    justification: "Capture microphone and tab audio and stream PCM to the desktop app.",
  });
}

async function closeOffscreen() {
  if (await hasOffscreen()) {
    await chrome.offscreen.closeDocument();
  }
}

const HANDLED = new Set(["getStatus", "status", "startCapture", "stopCapture"]);

chrome.runtime.onMessage.addListener((message, _sender, sendResponse) => {
  // Only claim the reply channel for messages this context owns. sendMessage
  // reaches every listener in the extension and the first reply wins, so
  // answering anything else races the offscreen document.
  if (!HANDLED.has(message?.type)) return false;

  (async () => {
    if (message?.type === "getStatus") {
      sendResponse(await currentStatus());
      return;
    }

    if (message?.type === "status") {
      broadcastStatus({ status: message.status, detail: message.detail || "" });
      sendResponse({ ok: true });
      return;
    }

    if (message?.type === "startCapture") {
      try {
        broadcastStatus({ status: "connecting", detail: "Preparing capture…" });
        await ensureMicPermission();
        await ensureOffscreen();

        const streamId = await chrome.tabCapture.getMediaStreamId({
          targetTabId: message.tabId,
        });

        const response = await chrome.runtime.sendMessage({
          type: "offscreenStart",
          streamId,
          port: message.port || 8765,
        });

        if (response?.error) {
          broadcastStatus({ status: "error", detail: response.error });
          sendResponse({ error: response.error });
          return;
        }

        // A stale grant (revoked since we cached it) shows up here — forget it so
        // the next Start asks again instead of silently capturing tab audio only.
        if (response?.micError) await chrome.storage.local.remove("micGranted");

        broadcastStatus({
          status: "capturing",
          detail: response?.detail || "Streaming mic + tab audio",
        });
        sendResponse({ ok: true });
      } catch (err) {
        const detail = err?.message || String(err);
        broadcastStatus({ status: "error", detail });
        sendResponse({ error: detail });
      }
      return;
    }

    if (message?.type === "stopCapture") {
      try {
        await chrome.runtime.sendMessage({ type: "offscreenStop" });
      } catch (_) {
        /* offscreen may already be gone */
      }
      await closeOffscreen();
      broadcastStatus({ status: "stopped", detail: "Capture stopped" });
      sendResponse({ ok: true });
    }
  })();
  return true;
});
