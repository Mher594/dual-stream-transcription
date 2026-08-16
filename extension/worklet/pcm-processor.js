/**
 * AudioWorkletProcessor: accumulates a chunk of mono float audio and posts it as
 * Int16 PCM. The target rate and stream id come from processorOptions, so this
 * file holds no copy of either.
 */
const CHUNK_SECONDS = 0.05;
const INT16_MAX = 0x7fff;

class PcmProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const { sampleRate: targetRate, streamId } = options.processorOptions || {};
    // Neither has a safe default: guessing the rate resamples wrong, and
    // guessing the stream id would label speaker audio as microphone.
    if (!targetRate || streamId === undefined) {
      throw new Error("pcm-processor needs processorOptions {sampleRate, streamId}");
    }

    this._targetRate = targetRate;
    this._streamId = streamId;
    this._ratio = sampleRate / targetRate;
    this._phase = 0;
    this._buffer = [];
    this._chunkSamples = Math.max(1, Math.round(targetRate * CHUNK_SECONDS));
  }

  process(inputs) {
    const input = inputs[0];
    if (!input || !input[0]) return true;
    const channel = input[0];

    // Simple decimation / linear resample toward target rate.
    for (let i = 0; i < channel.length; i++) {
      this._phase += 1;
      if (this._phase >= this._ratio) {
        this._phase -= this._ratio;
        const s = Math.max(-1, Math.min(1, channel[i]));
        this._buffer.push((s * INT16_MAX) | 0);
        if (this._buffer.length >= this._chunkSamples) {
          const pcm = new Int16Array(this._buffer.splice(0, this._chunkSamples));
          this.port.postMessage(
            { type: "pcm", streamId: this._streamId, pcm },
            [pcm.buffer]
          );
        }
      }
    }
    return true;
  }
}

registerProcessor("pcm-processor", PcmProcessor);
