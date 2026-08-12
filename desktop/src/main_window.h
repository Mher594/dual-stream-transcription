#pragma once

#include "transcript_model.h"
#include "ws_server.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>

namespace krisp {

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  MainWindow(TranscriptModel* model, WsServer* server, QWidget* parent = nullptr);

  // Errors stay on screen until both STT streams are healthy again.
  void showError(const QString& message);
  void setSttConnected(StreamKind stream, bool connected);
  void setCapturing(bool capturing);

 private:
  struct Pane {
    QPlainTextEdit* lines = nullptr;
    QLabel* interim = nullptr;
  };

  Pane buildPane(const QString& title, QWidget* parent, QHBoxLayout* row);
  Pane& paneFor(StreamKind stream);
  void appendLine(StreamKind stream, const QString& line);
  void setInterim(StreamKind stream, const QString& text);
  void clearPanes();
  void renderStatus();
  void onStatus(const QString& status);
  void onStats(quint64 micBytes, quint64 speakerBytes);

  TranscriptModel* model_ = nullptr;
  WsServer* server_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QLabel* statsLabel_ = nullptr;
  QLabel* errorLabel_ = nullptr;
  Pane mic_;
  Pane speaker_;
  QString baseStatus_;
  bool micSttConnected_ = false;
  bool speakerSttConnected_ = false;
  bool capturing_ = false;
};

}  // namespace krisp
