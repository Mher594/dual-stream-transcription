#include "transcript_model.h"

#include <QTime>

namespace krisp {

TranscriptModel::TranscriptModel(QObject* parent) : QObject(parent) {}

void TranscriptModel::clear() {
  micInterim_.clear();
  speakerInterim_.clear();
  emit cleared();
}

void TranscriptModel::applyUpdate(StreamKind stream, const QString& text, bool isFinal) {
  const QString trimmed = text.trimmed();
  QString& interim = (stream == StreamKind::Mic) ? micInterim_ : speakerInterim_;

  if (!isFinal) {
    if (interim == trimmed) return;  // nothing new to show
    interim = trimmed;
    emit interimChanged(stream, interim);
    return;
  }

  if (!interim.isEmpty()) {
    interim.clear();
    emit interimChanged(stream, interim);
  }
  if (trimmed.isEmpty()) return;  // silence must not commit a blank line

  // The timestamp is what lets a reader interleave the two panes back into one
  // conversation. In-flight text stays unstamped until it settles.
  emit lineCommitted(stream, QStringLiteral("[%1] %2")
                                 .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                                      trimmed));
}

}  // namespace krisp
