#include "deepgram_protocol.h"

#include <gtest/gtest.h>

#include <QUrlQuery>

using krisp::deepgramListenUrl;
using krisp::DeepgramEvent;
using krisp::UtteranceAssembler;

namespace {

// Shaped like a real Deepgram streaming Results message.
QString results(const QString& text, bool isFinal, bool speechFinal) {
  const auto flag = [](bool b) {
    return b ? QStringLiteral("true") : QStringLiteral("false");
  };
  return QStringLiteral(
             R"({"type":"Results","channel":{"alternatives":[{"transcript":"%1"}]},)"
             R"("is_final":%2,"speech_final":%3})")
      .arg(text, flag(isFinal), flag(speechFinal));
}

}  // namespace

TEST(UtteranceAssemblerTest, CollectsFragmentsIntoOneLineOnPause) {
  UtteranceAssembler a;

  // Deepgram closes several fragments before the speaker actually pauses.
  EXPECT_EQ(a.consume(results(QStringLiteral("so what I"), true, false)).isFinal, false);
  EXPECT_EQ(a.consume(results(QStringLiteral("was saying is"), true, false)).isFinal, false);

  const auto done = a.consume(results(QStringLiteral("we should ship it"), true, true));
  EXPECT_EQ(done.kind, DeepgramEvent::Kind::Transcript);
  EXPECT_TRUE(done.isFinal);
  EXPECT_EQ(done.text, QStringLiteral("so what I was saying is we should ship it"));
}

TEST(UtteranceAssemblerTest, InterimShowsSettledTextPlusLiveTail) {
  UtteranceAssembler a;
  a.consume(results(QStringLiteral("hello"), true, false));

  const auto live = a.consume(results(QStringLiteral("there fri"), false, false));
  EXPECT_EQ(live.kind, DeepgramEvent::Kind::Transcript);
  EXPECT_FALSE(live.isFinal);
  EXPECT_EQ(live.text, QStringLiteral("hello there fri"));
}

// Continuous speech may never produce speech_final, and without this backstop
// the line would accumulate forever and never settle.
TEST(UtteranceAssemblerTest, UtteranceEndSettlesTheLine) {
  UtteranceAssembler a;
  a.consume(results(QStringLiteral("no pause here"), true, false));

  const auto done = a.consume(QStringLiteral(R"({"type":"UtteranceEnd","last_word_end":3.2})"));
  EXPECT_EQ(done.kind, DeepgramEvent::Kind::Transcript);
  EXPECT_TRUE(done.isFinal);
  EXPECT_EQ(done.text, QStringLiteral("no pause here"));
}

TEST(UtteranceAssemblerTest, FlushCommitsHalfBuiltSentenceThenNothing) {
  UtteranceAssembler a;
  a.consume(results(QStringLiteral("cut off mid"), true, false));

  const auto flushed = a.flush();
  EXPECT_TRUE(flushed.isFinal);
  EXPECT_EQ(flushed.text, QStringLiteral("cut off mid"));

  // Nothing buffered any more, so a second flush must not repeat the line.
  EXPECT_EQ(a.flush().kind, DeepgramEvent::Kind::None);
}

TEST(UtteranceAssemblerTest, SilenceProducesNoBlankLines) {
  UtteranceAssembler a;
  EXPECT_EQ(a.consume(results(QString(), false, false)).kind, DeepgramEvent::Kind::None);
  EXPECT_EQ(a.consume(results(QString(), true, false)).kind, DeepgramEvent::Kind::None);
  EXPECT_EQ(a.consume(results(QString(), true, true)).kind, DeepgramEvent::Kind::None);
}

TEST(UtteranceAssemblerTest, ReportsErrorsAndIgnoresJunk) {
  UtteranceAssembler a;

  const auto err = a.consume(QStringLiteral(R"({"type":"Error","message":"bad request"})"));
  EXPECT_EQ(err.kind, DeepgramEvent::Kind::Error);
  EXPECT_EQ(err.text, QStringLiteral("bad request"));

  EXPECT_EQ(a.consume(QStringLiteral("not json at all")).kind, DeepgramEvent::Kind::None);
  EXPECT_EQ(a.consume(QStringLiteral(R"({"type":"Metadata"})")).kind, DeepgramEvent::Kind::None);
}

// A new sentence must not inherit the previous one.
TEST(UtteranceAssemblerTest, ResetsBetweenUtterances) {
  UtteranceAssembler a;
  a.consume(results(QStringLiteral("first sentence"), true, true));

  const auto second = a.consume(results(QStringLiteral("second sentence"), true, true));
  EXPECT_EQ(second.text, QStringLiteral("second sentence"));
}

TEST(DeepgramListenUrlTest, TargetsTheStreamingEndpoint) {
  const QUrl url = deepgramListenUrl(QStringLiteral("nova-2"));
  EXPECT_EQ(url.scheme(), QStringLiteral("wss"));
  EXPECT_EQ(url.host(), QStringLiteral("api.deepgram.com"));
  EXPECT_EQ(url.path(), QStringLiteral("/v1/listen"));
}

TEST(DeepgramListenUrlTest, CarriesEveryParameterTheTranscriptDependsOn) {
  // Dropping one of these does not fail the connection — it silently changes
  // what comes back, which is why they are pinned here rather than eyeballed.
  const QUrlQuery q(deepgramListenUrl(QStringLiteral("nova-2")).query());

  EXPECT_EQ(q.queryItemValue(QStringLiteral("encoding")), QStringLiteral("linear16"));
  EXPECT_EQ(q.queryItemValue(QStringLiteral("sample_rate")), QStringLiteral("16000"));
  EXPECT_EQ(q.queryItemValue(QStringLiteral("channels")), QStringLiteral("1"));
  EXPECT_EQ(q.queryItemValue(QStringLiteral("interim_results")), QStringLiteral("true"));
  EXPECT_EQ(q.queryItemValue(QStringLiteral("punctuate")), QStringLiteral("true"));
  EXPECT_EQ(q.queryItemValue(QStringLiteral("smart_format")), QStringLiteral("true"));
  EXPECT_EQ(q.queryItemValue(QStringLiteral("endpointing")), QStringLiteral("300"));
  EXPECT_EQ(q.queryItemValue(QStringLiteral("utterance_end_ms")), QStringLiteral("1000"));
}

TEST(DeepgramListenUrlTest, SendsTheModelItIsGiven) {
  const QUrlQuery def(deepgramListenUrl(QString::fromUtf8(krisp::kDefaultSttModel)).query());
  EXPECT_EQ(def.queryItemValue(QStringLiteral("model")), QStringLiteral("nova-2"));

  const QUrlQuery custom(deepgramListenUrl(QStringLiteral("nova-3")).query());
  EXPECT_EQ(custom.queryItemValue(QStringLiteral("model")), QStringLiteral("nova-3"));
}
