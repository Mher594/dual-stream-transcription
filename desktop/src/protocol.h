#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace krisp {

constexpr uint8_t kStreamMic = 0;
constexpr uint8_t kStreamSpeaker = 1;
constexpr int kSampleRate = 16000;
constexpr int kChannels = 1;
constexpr int kBytesPerSample = 2;
constexpr int kDefaultPort = 8765;
constexpr const char* kPcmFormat = "pcm_s16le";

// Audio frame layout: [u8 stream_id][pcm s16le…]. Named because the payload
// offset and the payload length are derived from it in four places, and a bare
// `1` there is indistinguishable from kChannels or kStreamSpeaker.
constexpr std::size_t kStreamIdBytes = 1;

enum class MessageType {
  Hello,
  HelloAck,
  CaptureStarted,
  CaptureStopped,
  Error,
  Unknown,
};

struct ControlMessage {
  MessageType type = MessageType::Unknown;
  int sampleRate = 0;
  std::string format;
  int channels = 0;
  int port = 0;
  std::string message;
};

struct AudioFrame {
  uint8_t streamId = 0;
  std::vector<std::int16_t> samples;
};

std::optional<ControlMessage> parseControlJson(std::string_view json);
std::string makeHelloAckJson(int port);
std::string makeErrorJson(std::string_view message);

// Binary: [u8 stream_id][pcm s16le...]
std::optional<AudioFrame> parseAudioFrame(const uint8_t* data, size_t size);

bool isValidHello(const ControlMessage& msg);

}  // namespace krisp
