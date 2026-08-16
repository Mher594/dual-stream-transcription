#include "ws_server.h"

#include "protocol.h"

#include <QHostAddress>

namespace krisp {

WsServer::WsServer(DeepgramClient* micStt, DeepgramClient* speakerStt, QObject* parent)
    : QObject(parent),
      micStt_(micStt),
      speakerStt_(speakerStt),
      server_(QStringLiteral("KrispDesktop"), QWebSocketServer::NonSecureMode, this) {
  connect(&server_, &QWebSocketServer::newConnection, this, &WsServer::onNewConnection);
}

bool WsServer::listen(quint16 port) {
  const bool ok = server_.listen(QHostAddress::LocalHost, port);
  if (ok) {
    setStatus(QStringLiteral("Listening on ws://127.0.0.1:%1").arg(server_.serverPort()));
  } else {
    setStatus(QStringLiteral("Listen failed: %1").arg(server_.errorString()));
  }
  return ok;
}

quint16 WsServer::port() const { return server_.serverPort(); }

void WsServer::setStatus(const QString& status) {
  qInfo("%s", qUtf8Printable(status));
  emit statusChanged(status);
}

void WsServer::onNewConnection() {
  while (server_.hasPendingConnections()) {
    QWebSocket* socket = server_.nextPendingConnection();
    if (client_) {
      socket->sendTextMessage(QString::fromStdString(makeErrorJson("Only one extension client supported")));
      socket->close();
      socket->deleteLater();
      continue;
    }
    client_ = socket;
    helloOk_ = false;
    micBytes_ = 0;
    speakerBytes_ = 0;
    emit statsChanged(micBytes_, speakerBytes_);
    connect(client_, &QWebSocket::textMessageReceived, this, &WsServer::onTextMessage);
    connect(client_, &QWebSocket::binaryMessageReceived, this, &WsServer::onBinaryMessage);
    connect(client_, &QWebSocket::disconnected, this, &WsServer::onSocketDisconnected);
    setStatus(QStringLiteral("Extension connected — waiting for hello"));
  }
}

void WsServer::onTextMessage(const QString& message) {
  const auto parsed = parseControlJson(message.toStdString());
  if (!parsed) {
    if (client_) {
      client_->sendTextMessage(QString::fromStdString(makeErrorJson("Invalid JSON control message")));
    }
    return;
  }

  switch (parsed->type) {
    case MessageType::Hello:
      if (!isValidHello(*parsed)) {
        if (client_) {
          client_->sendTextMessage(QString::fromStdString(makeErrorJson(
              "hello must be " + std::string(kPcmFormat) + " " +
              std::to_string(kSampleRate) + " Hz mono")));
        }
        setStatus(QStringLiteral("Rejected hello (bad audio format)"));
        return;
      }
      helloOk_ = true;
      if (client_) {
        client_->sendTextMessage(QString::fromStdString(makeHelloAckJson(port())));
      }
      setStatus(QStringLiteral("Hello OK — ready for audio"));
      break;
    case MessageType::CaptureStarted:
      ensureSttStarted();
      // Says only what this class can vouch for. Whether Deepgram is actually
      // reachable is the STT clients' business, and the UI appends that.
      setStatus(QStringLiteral("Capturing — receiving audio"));
      emit capturingChanged(true);
      break;
    case MessageType::CaptureStopped:
      stopStt();
      setStatus(QStringLiteral("Capture stopped"));
      emit capturingChanged(false);
      break;
    case MessageType::Error:
      setStatus(QStringLiteral("Extension error: %1")
                    .arg(QString::fromStdString(parsed->message)));
      break;
    default:
      break;
  }
}

void WsServer::onBinaryMessage(const QByteArray& message) {
  if (!helloOk_) return;
  const auto frame = parseAudioFrame(reinterpret_cast<const uint8_t*>(message.constData()),
                                     static_cast<size_t>(message.size()));
  if (!frame) return;

  const char* pcm = message.constData() + kStreamIdBytes;
  const qsizetype pcmSize = message.size() - static_cast<qsizetype>(kStreamIdBytes);
  if (pcmSize <= 0) return;

  if (frame->streamId == kStreamMic) {
    micBytes_ += static_cast<quint64>(pcmSize);
    micStt_->sendPcm(pcm, pcmSize);
  } else {
    speakerBytes_ += static_cast<quint64>(pcmSize);
    speakerStt_->sendPcm(pcm, pcmSize);
  }
  emit statsChanged(micBytes_, speakerBytes_);
}

void WsServer::onSocketDisconnected() {
  if (sender() == client_) {
    client_->deleteLater();
    client_ = nullptr;
    helloOk_ = false;
    stopStt();
    setStatus(QStringLiteral("Extension disconnected"));
    emit capturingChanged(false);
  }
}

void WsServer::ensureSttStarted() {
  // A missing key surfaces as a persistent error from the client itself.
  micStt_->start();
  speakerStt_->start();
}

void WsServer::stopStt() {
  micStt_->stop();
  speakerStt_->stop();
}

}  // namespace krisp
