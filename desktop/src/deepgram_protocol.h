#pragma once

#include <QString>
#include <QUrl>

namespace krisp {

// Model used unless KRISP_STT_MODEL overrides it.
constexpr const char* kDefaultSttModel = "nova-2";

// The streaming request as a URL. Here rather than in the client because these
// parameters are the Deepgram contract: drop one and the transcript quietly
// changes shape without anything being raised.
QUrl deepgramListenUrl(const QString& model);

// What a Deepgram streaming message means to the rest of the app.
struct DeepgramEvent {
  enum class Kind { None, Transcript, Error };

  Kind kind = Kind::None;
  QString text;  // transcript text, or the error message
  bool isFinal = false;
};

// Turns Deepgram's streaming messages into whole utterances.
//
// Deepgram finalises a fragment (`is_final`) far more often than the speaker
// actually pauses (`speech_final`). Committing a line per fragment splits one
// sentence across several, so fragments are collected here and only committed
// on a pause. Everything before that is reported as interim text.
class UtteranceAssembler {
 public:
  DeepgramEvent consume(const QString& json);

  // Commits a half-built sentence, e.g. when the socket closes mid-speech.
  DeepgramEvent flush();

 private:
  QString utterance_;
};

}  // namespace krisp
