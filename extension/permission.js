// Runs in a real tab purely so Chrome has somewhere to anchor the microphone
// prompt. The offscreen document that does the actual capture is invisible, so
// Chrome refuses to prompt there — but the grant is stored against the whole
// extension origin, so once it is given here the offscreen document inherits it.

const msg = document.getElementById("msg");
const note = document.getElementById("note");

try {
  const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
  stream.getTracks().forEach((track) => track.stop());
  chrome.runtime.sendMessage({ type: "micPermissionResult", granted: true }).catch(() => {});
} catch (err) {
  msg.textContent =
    `Microphone access was not granted (${err?.message || err}). ` +
    "Allow it for this extension at chrome://settings/content/microphone, then press Start again.";
  msg.classList.add("error");
  note.textContent =
    "Capture carries on with tab audio only, so the speaker pane keeps working while this is unresolved.";
  chrome.runtime.sendMessage({ type: "micPermissionResult", granted: false }).catch(() => {});
}
