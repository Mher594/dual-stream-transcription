#include "deepgram_protocol.h"

#include "protocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace krisp {
namespace {

const QString kListenUrl = QStringLiteral("wss://api.deepgram.com/v1/listen");

// Pause that means "end of sentence". Deepgram's default (10 ms) is shorter
// than a gap between words and shatters a sentence across several lines.
constexpr int kEndpointingMs = 300;
// Flush if speech never pauses, otherwise an utterance would accumulate forever.
constexpr int kUtteranceEndMs = 1000;

DeepgramEvent transcriptEvent(const QString& text, bool isFinal) {
  return DeepgramEvent{DeepgramEvent::Kind::Transcript, text, isFinal};
}

}  // namespace

QUrl deepgramListenUrl(const QString& model) {
  QUrl url(kListenUrl);
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("encoding"), QStringLiteral("linear16"));
  q.addQueryItem(QStringLiteral("sample_rate"), QString::number(kSampleRate));
  q.addQueryItem(QStringLiteral("channels"), QString::number(kChannels));
  q.addQueryItem(QStringLiteral("model"), model);
  // interim_results also gates UtteranceEnd: without it Deepgram sends neither,
  // and the backstop that settles a line during non-stop speech goes silent.
  q.addQueryItem(QStringLiteral("interim_results"), QStringLiteral("true"));
  q.addQueryItem(QStringLiteral("punctuate"), QStringLiteral("true"));
  q.addQueryItem(QStringLiteral("smart_format"), QStringLiteral("true"));
  q.addQueryItem(QStringLiteral("endpointing"), QString::number(kEndpointingMs));
  q.addQueryItem(QStringLiteral("utterance_end_ms"), QString::number(kUtteranceEndMs));
  url.setQuery(q);
  return url;
}

DeepgramEvent UtteranceAssembler::flush() {
  if (utterance_.isEmpty()) return {};
  const QString sentence = utterance_;
  utterance_.clear();
  return transcriptEvent(sentence, true);
}

DeepgramEvent UtteranceAssembler::consume(const QString& json) {
  const auto doc = QJsonDocument::fromJson(json.toUtf8());
  if (!doc.isObject()) return {};
  const QJsonObject root = doc.object();
  const QString type = root.value(QStringLiteral("type")).toString();

  if (type == QStringLiteral("Error")) {
    return DeepgramEvent{DeepgramEvent::Kind::Error,
                         root.value(QStringLiteral("message"))
                             .toString(QStringLiteral("error")),
                         false};
  }

  // Continuous speech may never produce speech_final; UtteranceEnd is the
  // backstop that still settles the line.
  if (type == QStringLiteral("UtteranceEnd")) return flush();

  if (type != QStringLiteral("Results") && !root.contains(QStringLiteral("channel"))) {
    return {};
  }

  const auto alts = root.value(QStringLiteral("channel"))
                        .toObject()
                        .value(QStringLiteral("alternatives"))
                        .toArray();
  if (alts.isEmpty()) return {};
  const QString text = alts.at(0).toObject().value(QStringLiteral("transcript")).toString();

  const bool segmentFinal = root.value(QStringLiteral("is_final")).toBool(false);
  const bool speechFinal = root.value(QStringLiteral("speech_final")).toBool(false);

  if (segmentFinal && !text.isEmpty()) {
    if (!utterance_.isEmpty()) utterance_ += QLatin1Char(' ');
    utterance_ += text;
  }
  if (speechFinal) return flush();

  // No pause yet: show the settled part plus whatever is still in flight.
  QString live = utterance_;
  if (!segmentFinal && !text.isEmpty()) {
    if (!live.isEmpty()) live += QLatin1Char(' ');
    live += text;
  }
  if (live.isEmpty()) return {};
  return transcriptEvent(live, false);
}

}  // namespace krisp
