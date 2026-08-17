#include "protocol.h"
#include "transcript_model.h"
#include "ws_server.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>

using krisp::kStreamMic;
using krisp::kStreamSpeaker;
using krisp::StreamKind;
using krisp::WsServer;

namespace {

// Records what the server announced. Standing in for the STT clients is the
// whole point: routing is what we want to check, not transcription.
struct Routed {
  QList<QPair<StreamKind, QByteArray>> frames;

  explicit Routed(WsServer& server) {
    QObject::connect(&server, &WsServer::audioReceived,
                     [this](StreamKind stream, const QByteArray& pcm) {
                       frames.append({stream, pcm});
                     });
  }
};

QByteArray frame(uint8_t streamId, const QByteArray& pcm) {
  return QByteArray(1, static_cast<char>(streamId)) + pcm;
}

const QString kHello = QStringLiteral(
    R"({"type":"hello","sampleRate":16000,"format":"pcm_s16le","channels":1})");

}  // namespace

TEST(StreamRoutingTest, EachStreamKeepsItsOwnLabel) {
  WsServer server;
  Routed routed(server);
  server.handleControlMessage(kHello);

  server.handleAudioFrame(frame(kStreamMic, QByteArray("\x01\x00", 2)));
  server.handleAudioFrame(frame(kStreamSpeaker, QByteArray("\x02\x00", 2)));

  ASSERT_EQ(routed.frames.size(), 2);
  EXPECT_EQ(routed.frames[0].first, StreamKind::Mic);
  EXPECT_EQ(routed.frames[1].first, StreamKind::Speaker);
}

TEST(StreamRoutingTest, IdenticalPayloadsStillRouteApart) {
  // The id byte is the only thing telling these apart, so the same audio on
  // both streams must still come out labelled differently.
  const QByteArray pcm("\x07\x00", 2);
  WsServer server;
  Routed routed(server);
  server.handleControlMessage(kHello);

  server.handleAudioFrame(frame(kStreamMic, pcm));
  server.handleAudioFrame(frame(kStreamSpeaker, pcm));

  ASSERT_EQ(routed.frames.size(), 2);
  EXPECT_NE(routed.frames[0].first, routed.frames[1].first);
}

TEST(StreamRoutingTest, StripsTheStreamIdFromThePayload) {
  const QByteArray pcm("\x11\x22\x33\x44", 4);
  WsServer server;
  Routed routed(server);
  server.handleControlMessage(kHello);

  server.handleAudioFrame(frame(kStreamMic, pcm));

  ASSERT_EQ(routed.frames.size(), 1);
  EXPECT_EQ(routed.frames[0].second, pcm);
}

TEST(StreamRoutingTest, IgnoresAudioBeforeHello) {
  WsServer server;
  Routed routed(server);

  server.handleAudioFrame(frame(kStreamMic, QByteArray("\x01\x00", 2)));

  EXPECT_TRUE(routed.frames.isEmpty());
}

TEST(StreamRoutingTest, DropsMalformedFrames) {
  WsServer server;
  Routed routed(server);
  server.handleControlMessage(kHello);

  server.handleAudioFrame(frame(9, QByteArray("\x01\x00", 2)));       // unknown stream
  server.handleAudioFrame(frame(kStreamMic, QByteArray("\x01", 1)));  // half a sample
  server.handleAudioFrame(frame(kStreamMic, QByteArray()));           // header only
  server.handleAudioFrame(QByteArray());                              // nothing at all

  EXPECT_TRUE(routed.frames.isEmpty());
}
