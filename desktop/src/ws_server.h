#pragma once

#include "deepgram_client.h"

#include <QObject>
#include <QWebSocket>
#include <QWebSocketServer>

namespace krisp {

class WsServer : public QObject {
  Q_OBJECT
 public:
  WsServer(DeepgramClient* micStt, DeepgramClient* speakerStt, QObject* parent = nullptr);

  bool listen(quint16 port);
  quint16 port() const;

 signals:
  void statusChanged(const QString& status);
  void statsChanged(quint64 micBytes, quint64 speakerBytes);
  void capturingChanged(bool capturing);
  void errorRaised(const QString& message);

 private:
  void setStatus(const QString& status);
  void raiseError(const QString& message);
  void onNewConnection();
  void onTextMessage(const QString& message);
  void onBinaryMessage(const QByteArray& message);
  void onSocketDisconnected();
  void ensureSttStarted();
  void stopStt();

  DeepgramClient* micStt_ = nullptr;
  DeepgramClient* speakerStt_ = nullptr;
  QWebSocketServer server_;
  QWebSocket* client_ = nullptr;
  quint64 micBytes_ = 0;
  quint64 speakerBytes_ = 0;
  bool helloOk_ = false;
};

}  // namespace krisp
