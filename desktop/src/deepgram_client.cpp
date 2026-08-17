#include "deepgram_client.h"

#include "protocol.h"

#include <QNetworkRequest>

namespace krisp {
namespace {

// Wait while a previous socket finishes closing before opening a new one.
constexpr int kSocketSettleMs = 250;
constexpr int kReconnectBaseMs = 500;  // backoff: 0.5 s … 8 s
constexpr int kPendingSeconds = 2;

// Deepgram's socket needs a few hundred ms to open, so without this the
// opening words of every session are lost.
constexpr qsizetype kMaxPendingBytes =
    static_cast<qsizetype>(kSampleRate) * kBytesPerSample * kPendingSeconds;

// A rejected key makes Deepgram close immediately, so unbounded retries would
// hammer the API. Give up after a few tries and say so instead.
constexpr int kMaxReconnectAttempts = 5;

}  // namespace

DeepgramClient::DeepgramClient(StreamKind stream, QObject* parent)
    : QObject(parent), stream_(stream), model_(QString::fromUtf8(kDefaultSttModel)) {
  reconnectTimer_.setSingleShot(true);
  connect(&reconnectTimer_, &QTimer::timeout, this, [this]() {
    if (started_) openSocket();
  });

  connect(&socket_, &QWebSocket::connected, this, &DeepgramClient::onConnected);
  connect(&socket_, &QWebSocket::disconnected, this, &DeepgramClient::onDisconnected);
  connect(&socket_, &QWebSocket::textMessageReceived, this, &DeepgramClient::onTextMessage);
  connect(&socket_, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
    raiseError(QStringLiteral("Deepgram (%1): %2").arg(label(), describeSocketError()));
  });
}

QString DeepgramClient::describeSocketError() const {
  const QString detail = socket_.errorString();
  // Qt renders Deepgram's 401 as "Unsupported WWW-Authenticate challenges",
  // which reads like a protocol bug rather than a rejected key.
  if (detail.contains(QStringLiteral("WWW-Authenticate"), Qt::CaseInsensitive)) {
    return QStringLiteral("API key rejected (HTTP 401) — check DEEPGRAM_API_KEY");
  }
  return detail;
}

// Every error reaches the log; the UI keeps only the first one, so the log is
// where the full sequence of a failure lives.
void DeepgramClient::raiseError(const QString& message) {
  qWarning("%s", qUtf8Printable(message));
  emit errorOccurred(message);
}

QString DeepgramClient::label() const {
  return stream_ == StreamKind::Mic ? QStringLiteral("mic") : QStringLiteral("speaker");
}

void DeepgramClient::setApiKey(const QString& apiKey) { apiKey_ = apiKey; }

void DeepgramClient::setModel(const QString& model) {
  if (!model.isEmpty()) model_ = model;
}

void DeepgramClient::start() {
  if (started_) return;
  started_ = true;
  reconnectAttempts_ = 0;
  openSocket();
}

void DeepgramClient::openSocket() {
  if (apiKey_.isEmpty()) {
    // Nothing to retry until the app is restarted with a key.
    started_ = false;
    raiseError(QStringLiteral(
        "DEEPGRAM_API_KEY is missing — %1 audio is received but not transcribed").arg(label()));
    return;
  }
  if (socket_.state() != QAbstractSocket::UnconnectedState) {
    // Still closing from a previous capture. Wait for the socket to settle
    // rather than returning silently and leaving this stream dead.
    reconnectTimer_.start(kSocketSettleMs);
    return;
  }

  qInfo("Deepgram (%s): connecting with model %s", qUtf8Printable(label()),
        qUtf8Printable(model_));
  QNetworkRequest req(deepgramListenUrl(model_));
  req.setRawHeader("Authorization", QByteArray("Token ") + apiKey_.toUtf8());
  socket_.open(req);
}

void DeepgramClient::stop() {
  started_ = false;
  reconnectTimer_.stop();
  reconnectAttempts_ = 0;
  pending_.clear();
  if (socket_.state() == QAbstractSocket::ConnectedState) {
    // Deepgram flushes any buffered audio and returns a final result on this.
    socket_.sendTextMessage(QStringLiteral("{\"type\":\"CloseStream\"}"));
  }
  if (socket_.state() != QAbstractSocket::UnconnectedState) {
    socket_.close();
  }
}

void DeepgramClient::onConnected() {
  reconnectAttempts_ = 0;
  qInfo("Deepgram (%s): connected, flushing %lld buffered bytes",
        qUtf8Printable(label()), static_cast<long long>(pending_.size()));
  emit connectedChanged(stream_, true);
  if (!pending_.isEmpty()) {
    socket_.sendBinaryMessage(pending_);
    pending_.clear();
  }
}

void DeepgramClient::onDisconnected() {
  qInfo("Deepgram (%s): socket closed (code %d)", qUtf8Printable(label()),
        static_cast<int>(socket_.closeCode()));
  // Don't lose a half-spoken sentence when the socket drops or capture stops.
  dispatch(assembler_.flush());
  emit connectedChanged(stream_, false);
  if (started_) scheduleReconnect();
}

void DeepgramClient::scheduleReconnect() {
  if (reconnectAttempts_ >= kMaxReconnectAttempts) {
    started_ = false;
    pending_.clear();
    raiseError(
        QStringLiteral("Deepgram (%1): gave up after %2 reconnect attempts — check "
                       "DEEPGRAM_API_KEY and network, then restart capture")
            .arg(label())
            .arg(kMaxReconnectAttempts));
    return;
  }

  ++reconnectAttempts_;
  const int delayMs = kReconnectBaseMs * (1 << (reconnectAttempts_ - 1));
  raiseError(QStringLiteral("Deepgram (%1): disconnected — retrying in %2 ms (%3/%4)")
                         .arg(label())
                         .arg(delayMs)
                         .arg(reconnectAttempts_)
                         .arg(kMaxReconnectAttempts));
  reconnectTimer_.start(delayMs);
}

void DeepgramClient::sendPcm(const char* data, qsizetype size) {
  if (!started_ || size <= 0) return;

  if (socket_.state() == QAbstractSocket::ConnectedState) {
    socket_.sendBinaryMessage(QByteArray(data, size));
    return;
  }

  // Connecting or reconnecting: hold the most recent audio so speech that lands
  // during the handshake still gets transcribed. Oldest bytes drop first.
  pending_.append(data, size);
  if (pending_.size() > kMaxPendingBytes) {
    pending_.remove(0, pending_.size() - kMaxPendingBytes);
  }
}

void DeepgramClient::onTextMessage(const QString& message) {
  dispatch(assembler_.consume(message));
}

void DeepgramClient::dispatch(const DeepgramEvent& event) {
  switch (event.kind) {
    case DeepgramEvent::Kind::Transcript:
      emit transcript(stream_, event.text, event.isFinal);
      break;
    case DeepgramEvent::Kind::Error:
      raiseError(QStringLiteral("Deepgram (%1): %2").arg(label(), event.text));
      break;
    case DeepgramEvent::Kind::None:
      break;
  }
}

}  // namespace krisp
