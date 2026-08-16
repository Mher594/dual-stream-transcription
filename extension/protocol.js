export const STREAM_MIC = 0;
export const STREAM_SPEAKER = 1;
export const DEFAULT_PORT = 8765;
export const SAMPLE_RATE = 16000;
export const CHANNELS = 1;
export const FORMAT = "pcm_s16le";

// Audio frame layout: [u8 stream_id][pcm s16le…]. The desktop derives the
// payload offset and length from this too (kStreamIdBytes in protocol.h).
export const STREAM_ID_BYTES = 1;

export function helloMessage() {
  return JSON.stringify({
    type: "hello",
    sampleRate: SAMPLE_RATE,
    format: FORMAT,
    channels: CHANNELS,
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
  const bytes = new Uint8Array(STREAM_ID_BYTES + pcm.byteLength);
  bytes[0] = streamId & 0xff;
  bytes.set(new Uint8Array(pcm.buffer, pcm.byteOffset, pcm.byteLength), STREAM_ID_BYTES);
  return bytes.buffer;
}

export function desktopWsUrl(port = DEFAULT_PORT) {
  return `ws://127.0.0.1:${port}`;
}
