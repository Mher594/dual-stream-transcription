#pragma once

#include "deepgram_protocol.h"
#include "transcript_model.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QWebSocket>

namespace krisp {

class DeepgramClient : public QObject {
  Q_OBJECT
 public:
  explicit DeepgramClient(StreamKind stream, QObject* parent = nullptr);

  void setApiKey(const QString& apiKey);
  // Empty keeps the default model.
  void setModel(const QString& model);
  void start();
  void stop();
  void sendPcm(const char* data, qsizetype size);

 signals:
  void transcript(StreamKind stream, const QString& text, bool isFinal);
  void errorOccurred(const QString& message);
  void connectedChanged(StreamKind stream, bool connected);

 private:
  void openSocket();
  void scheduleReconnect();
  void raiseError(const QString& message);
  QString describeSocketError() const;
  void onConnected();
  void onDisconnected();
  void onTextMessage(const QString& message);
  QUrl listenUrl() const;
  QString label() const;

  void dispatch(const DeepgramEvent& event);

  StreamKind stream_;
  QString apiKey_;
  QString model_;
  QWebSocket socket_;
  UtteranceAssembler assembler_;
  QByteArray pending_;
  QTimer reconnectTimer_;
  int reconnectAttempts_ = 0;
  bool started_ = false;
};

}  // namespace krisp
