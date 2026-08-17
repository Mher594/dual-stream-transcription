#pragma once

#include "transcript_model.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QWebSocket>
#include <QWebSocketServer>

namespace krisp {

class WsServer : public QObject {
  Q_OBJECT
 public:
  explicit WsServer(QObject* parent = nullptr);

  bool listen(quint16 port);
  quint16 port() const;

  // The two things the extension sends. Public so the wire behaviour can be
  // driven from a test without standing up a socket.
  void handleControlMessage(const QString& message);
  void handleAudioFrame(const QByteArray& message);

 signals:
  void statusChanged(const QString& status);
  void statsChanged(quint64 micBytes, quint64 speakerBytes);
  void capturingChanged(bool capturing);
  void errorRaised(const QString& message);
  // Audio, already split by stream. Announced rather than pushed into an STT
  // client, so this class never learns which engine transcribes it.
  void audioReceived(StreamKind stream, const QByteArray& pcm);

 private:
  void setStatus(const QString& status);
  void raiseError(const QString& message);
  void onNewConnection();
  void onSocketDisconnected();

  QWebSocketServer server_;
  QWebSocket* client_ = nullptr;
  quint64 micBytes_ = 0;
  quint64 speakerBytes_ = 0;
  bool helloOk_ = false;
};

}  // namespace krisp
