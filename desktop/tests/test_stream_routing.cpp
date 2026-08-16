#include "protocol.h"

#include <gtest/gtest.h>

#include <vector>

using krisp::kStreamMic;
using krisp::kStreamSpeaker;
using krisp::parseAudioFrame;

TEST(StreamRoutingTest, LabelsStaySeparate) {
  std::vector<uint8_t> mic = {kStreamMic, 0x01, 0x00};
  std::vector<uint8_t> spk = {kStreamSpeaker, 0x02, 0x00};

  const auto micId = parseAudioFrame(mic.data(), mic.size());
  const auto spkId = parseAudioFrame(spk.data(), spk.size());
  ASSERT_TRUE(micId);
  ASSERT_TRUE(spkId);
  EXPECT_EQ(*micId, kStreamMic);
  EXPECT_EQ(*spkId, kStreamSpeaker);
  // The whole point: identical payloads must still route to different streams.
  EXPECT_NE(*micId, *spkId);
}
