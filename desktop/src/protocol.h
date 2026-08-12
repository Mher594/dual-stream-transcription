#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace krisp {

constexpr uint8_t kStreamMic = 0;
constexpr uint8_t kStreamSpeaker = 1;
constexpr int kSampleRate = 16000;

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
