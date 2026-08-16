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
  std::vector<uint8_t> bytes;
  bytes.push_back(kStreamMic);
  // two samples: 1, -2
  bytes.push_back(0x01);
  bytes.push_back(0x00);
  bytes.push_back(0xfe);
  bytes.push_back(0xff);

  const auto frame = parseAudioFrame(bytes.data(), bytes.size());
  ASSERT_TRUE(frame.has_value());
  EXPECT_EQ(frame->streamId, kStreamMic);
  ASSERT_EQ(frame->samples.size(), 2u);
  EXPECT_EQ(frame->samples[0], 1);
  EXPECT_EQ(frame->samples[1], -2);
}

TEST(ProtocolTest, RejectsUnknownStreamId) {
  uint8_t bytes[] = {9, 0x00, 0x00};
  EXPECT_FALSE(parseAudioFrame(bytes, sizeof(bytes)).has_value());
}

TEST(ProtocolTest, RejectsOddPcmLength) {
  uint8_t bytes[] = {kStreamSpeaker, 0x00};
  EXPECT_FALSE(parseAudioFrame(bytes, sizeof(bytes)).has_value());
}
