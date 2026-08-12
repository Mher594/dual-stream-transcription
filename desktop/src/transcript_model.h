#pragma once

#include <QObject>
#include <QString>
#include <cstdint>

namespace krisp {

enum class StreamKind : std::uint8_t { Mic = 0, Speaker = 1 };

// Turns STT updates into the two things a view needs: settled lines, emitted
// one at a time so they can be appended, and the in-flight text per stream.
//
// Committed lines are not stored here. The view owns its own history (and its
// own cap), which keeps a growing transcript from being rebuilt on every tick.
class TranscriptModel : public QObject {
  Q_OBJECT
 public:
  explicit TranscriptModel(QObject* parent = nullptr);

  void clear();
  void applyUpdate(StreamKind stream, const QString& text, bool isFinal);

 signals:
  void lineCommitted(StreamKind stream, const QString& line);  // "[HH:mm:ss] text"
  void interimChanged(StreamKind stream, const QString& text);
  void cleared();

 private:
  QString micInterim_;
  QString speakerInterim_;
};

}  // namespace krisp
