#pragma once

#include "transcript_model.h"
#include "ws_server.h"

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

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
  // One in-flight row per stream: both can be mid-utterance at the same time.
  struct InterimRow {
    QWidget* row = nullptr;
    QLabel* text = nullptr;
  };

  QWidget* buildHeader(QWidget* parent);
  QWidget* buildCard(QWidget* parent);
  QWidget* buildFooter(QWidget* parent);
  QWidget* makeRow(const QString& time, StreamKind stream, const QString& text, bool inFlight);
  InterimRow& interimFor(StreamKind stream);

  void appendLine(StreamKind stream, const QString& line);
  void setInterim(StreamKind stream, const QString& text);
  void clearTimeline();
  void refreshEmptyState();
  void renderStatus();
  void onStatus(const QString& status);
  void onStats(quint64 micBytes, quint64 speakerBytes);

  TranscriptModel* model_ = nullptr;
  WsServer* server_ = nullptr;

  QLabel* dot_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QLabel* statsLabel_ = nullptr;
  QLabel* errorLabel_ = nullptr;

  QScrollArea* scroll_ = nullptr;
  QVBoxLayout* timeline_ = nullptr;
  QWidget* empty_ = nullptr;
  QLabel* emptyTitle_ = nullptr;
  QLabel* emptyBody_ = nullptr;
  InterimRow micInterim_;
  InterimRow speakerInterim_;
  int lineCount_ = 0;

  QLabel* hint_ = nullptr;
  QPushButton* clearBtn_ = nullptr;

  QString baseStatus_;
  bool micSttConnected_ = false;
  bool speakerSttConnected_ = false;
  bool capturing_ = false;
  bool cleared_ = false;
};

}  // namespace krisp
