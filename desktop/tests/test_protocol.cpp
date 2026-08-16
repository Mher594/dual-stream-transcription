#include "protocol.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

using krisp::parseAudioFrame;
using krisp::parseControlJson;
using krisp::isValidHello;
using krisp::makeHelloAckJson;
using krisp::makeErrorJson;
using krisp::kStreamMic;
using krisp::kStreamSpeaker;
using krisp::kSampleRate;
using krisp::kChannels;
using krisp::kPcmFormat;
using krisp::kDefaultPort;

namespace {

std::string helloJson(int sampleRate) {
  return std::string(R"({"type":"hello","sampleRate":)") + std::to_string(sampleRate) +
         R"(,"format":")" + kPcmFormat + R"(","channels":)" + std::to_string(kChannels) + "}";
}

}  // namespace

TEST(ProtocolTest, ParsesValidHello) {
  const auto msg = parseControlJson(helloJson(kSampleRate));
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->type, krisp::MessageType::Hello);
  EXPECT_TRUE(isValidHello(*msg));
}

TEST(ProtocolTest, RejectsBadHelloSampleRate) {
  const auto msg = parseControlJson(helloJson(48000));
  ASSERT_TRUE(msg.has_value());
  EXPECT_FALSE(isValidHello(*msg));
}

TEST(ProtocolTest, HelloAckAndErrorJsonRoundTripShape) {
  const auto ack = parseControlJson(makeHelloAckJson(kDefaultPort));
  ASSERT_TRUE(ack.has_value());
  EXPECT_EQ(ack->type, krisp::MessageType::HelloAck);
  EXPECT_EQ(ack->port, kDefaultPort);

  const auto err = parseControlJson(makeErrorJson("boom"));
  ASSERT_TRUE(err.has_value());
  EXPECT_EQ(err->type, krisp::MessageType::Error);
  EXPECT_EQ(err->message, "boom");
}

TEST(ProtocolTest, ParsesBinaryAudioFrame) {
  // One header byte then two samples' worth of PCM. The payload is forwarded
  // untouched, so only its length is checked, never its contents.
  const std::vector<uint8_t> bytes = {kStreamMic, 0x01, 0x00, 0xfe, 0xff};

  const auto streamId = parseAudioFrame(bytes.data(), bytes.size());
  ASSERT_TRUE(streamId.has_value());
  EXPECT_EQ(*streamId, kStreamMic);
}

TEST(ProtocolTest, RejectsFrameWithNoPayload) {
  const uint8_t bytes[] = {kStreamMic};
  EXPECT_TRUE(parseAudioFrame(bytes, sizeof(bytes)).has_value());  // header alone is valid
  EXPECT_FALSE(parseAudioFrame(nullptr, 0).has_value());
  EXPECT_FALSE(parseAudioFrame(bytes, 0).has_value());
}

TEST(ProtocolTest, RejectsUnknownStreamId) {
  uint8_t bytes[] = {9, 0x00, 0x00};
  EXPECT_FALSE(parseAudioFrame(bytes, sizeof(bytes)).has_value());
}

TEST(ProtocolTest, RejectsOddPcmLength) {
  uint8_t bytes[] = {kStreamSpeaker, 0x00};
  EXPECT_FALSE(parseAudioFrame(bytes, sizeof(bytes)).has_value());
}
