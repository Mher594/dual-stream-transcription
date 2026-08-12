#include "deepgram_protocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace krisp {
namespace {

DeepgramEvent transcriptEvent(const QString& text, bool isFinal) {
  return DeepgramEvent{DeepgramEvent::Kind::Transcript, text, isFinal};
}

}  // namespace

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
