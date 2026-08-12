export const STREAM_MIC = 0;
export const STREAM_SPEAKER = 1;
export const DEFAULT_PORT = 8765;
export const SAMPLE_RATE = 16000;

export function helloMessage() {
  return JSON.stringify({
    type: "hello",
    sampleRate: SAMPLE_RATE,
    format: "pcm_s16le",
    channels: 1,
  });
}

export function captureStartedMessage() {
  return JSON.stringify({ type: "capture_started" });
}

export function captureStoppedMessage() {
  return JSON.stringify({ type: "capture_stopped" });
}

export function errorMessage(message) {
  return JSON.stringify({ type: "error", message });
}

/** @param {number} streamId @param {Int16Array} pcm */
export function encodeAudioFrame(streamId, pcm) {
  const bytes = new Uint8Array(1 + pcm.byteLength);
  bytes[0] = streamId & 0xff;
  bytes.set(new Uint8Array(pcm.buffer, pcm.byteOffset, pcm.byteLength), 1);
  return bytes.buffer;
}

export function desktopWsUrl(port = DEFAULT_PORT) {
  return `ws://127.0.0.1:${port}`;
}
