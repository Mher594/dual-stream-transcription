/**
 * AudioWorkletProcessor: accumulates ~50ms of mono float audio and posts Int16 PCM.
 * Target rate handling is done upstream via OfflineAudioContext / AudioContext rate.
 */
class PcmProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this._targetRate = (options.processorOptions && options.processorOptions.sampleRate) || 16000;
    this._streamId = (options.processorOptions && options.processorOptions.streamId) || 0;
    this._ratio = sampleRate / this._targetRate;
    this._phase = 0;
    this._buffer = [];
    this._chunkSamples = Math.max(1, Math.round(this._targetRate * 0.05)); // ~50ms
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
        this._buffer.push((s * 0x7fff) | 0);
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
