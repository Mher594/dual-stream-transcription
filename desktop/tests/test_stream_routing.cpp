#include "protocol.h"

#include <gtest/gtest.h>

#include <vector>

using krisp::kStreamMic;
using krisp::kStreamSpeaker;
using krisp::parseAudioFrame;

TEST(StreamRoutingTest, LabelsStaySeparate) {
  std::vector<uint8_t> mic = {kStreamMic, 0x01, 0x00};
  std::vector<uint8_t> spk = {kStreamSpeaker, 0x02, 0x00};

  const auto micFrame = parseAudioFrame(mic.data(), mic.size());
  const auto spkFrame = parseAudioFrame(spk.data(), spk.size());
  ASSERT_TRUE(micFrame);
  ASSERT_TRUE(spkFrame);
  EXPECT_EQ(micFrame->streamId, kStreamMic);
  EXPECT_EQ(spkFrame->streamId, kStreamSpeaker);
  EXPECT_NE(micFrame->streamId, spkFrame->streamId);
  EXPECT_EQ(micFrame->samples[0], 1);
  EXPECT_EQ(spkFrame->samples[0], 2);
}
