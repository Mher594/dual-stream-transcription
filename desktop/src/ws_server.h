#pragma once

#include "deepgram_client.h"
#include "transcript_model.h"

#include <QObject>
#include <QWebSocket>
#include <QWebSocketServer>
#include <cstdint>

namespace krisp {

class WsServer : public QObject {
  Q_OBJECT
 public:
  WsServer(TranscriptModel* model,
           DeepgramClient* micStt,
           DeepgramClient* speakerStt,
           QObject* parent = nullptr);

  bool listen(quint16 port);
  quint16 port() const;

  quint64 micBytes() const { return micBytes_; }
  quint64 speakerBytes() const { return speakerBytes_; }

 signals:
  void statusChanged(const QString& status);
  void statsChanged(quint64 micBytes, quint64 speakerBytes);
  void capturingChanged(bool capturing);

 private:
  void setStatus(const QString& status);
  void onNewConnection();
  void onTextMessage(const QString& message);
  void onBinaryMessage(const QByteArray& message);
  void onSocketDisconnected();
  void ensureSttStarted();
  void stopStt();

  TranscriptModel* model_ = nullptr;
  DeepgramClient* micStt_ = nullptr;
  DeepgramClient* speakerStt_ = nullptr;
  QWebSocketServer server_;
  QWebSocket* client_ = nullptr;
  quint16 port_ = 0;
  quint64 micBytes_ = 0;
  quint64 speakerBytes_ = 0;
  bool helloOk_ = false;
};

}  // namespace krisp
