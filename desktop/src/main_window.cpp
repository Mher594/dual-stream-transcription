#include "main_window.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWidget>

namespace krisp {
namespace {

// Settled lines live in the widget, so the widget owns the cap too.
constexpr int kMaxLines = 500;

}  // namespace

MainWindow::MainWindow(TranscriptModel* model, WsServer* server, QWidget* parent)
    : QMainWindow(parent), model_(model), server_(server) {
  setWindowTitle(QStringLiteral("Krisp Dual-Stream Transcripts"));
  resize(960, 640);

  auto* central = new QWidget(this);
  setCentralWidget(central);
  auto* root = new QVBoxLayout(central);

  statusLabel_ = new QLabel(QStringLiteral("Starting…"), central);
  statsLabel_ = new QLabel(QStringLiteral("Mic: 0 B | Speaker: 0 B"), central);
  errorLabel_ = new QLabel(central);
  errorLabel_->setStyleSheet(QStringLiteral("color: #b00020; font-weight: bold;"));
  errorLabel_->setWordWrap(true);
  errorLabel_->setVisible(false);
  root->addWidget(statusLabel_);
  root->addWidget(statsLabel_);
  root->addWidget(errorLabel_);

  auto* panes = new QHBoxLayout();
  root->addLayout(panes, 1);
  mic_ = buildPane(QStringLiteral("Microphone (you)"), central, panes);
  speaker_ = buildPane(QStringLiteral("Speaker (tab / others)"), central, panes);

  auto* clearBtn = new QPushButton(QStringLiteral("Clear transcripts"), central);
  root->addWidget(clearBtn);
  connect(clearBtn, &QPushButton::clicked, model_, &TranscriptModel::clear);

  connect(model_, &TranscriptModel::lineCommitted, this, &MainWindow::appendLine);
  connect(model_, &TranscriptModel::interimChanged, this, &MainWindow::setInterim);
  connect(model_, &TranscriptModel::cleared, this, &MainWindow::clearPanes);
  connect(server_, &WsServer::statusChanged, this, &MainWindow::onStatus);
  connect(server_, &WsServer::statsChanged, this, &MainWindow::onStats);
}

MainWindow::Pane MainWindow::buildPane(const QString& title, QWidget* parent, QHBoxLayout* row) {
  auto* column = new QVBoxLayout();
  column->addWidget(new QLabel(title, parent));

  Pane pane;
  pane.lines = new QPlainTextEdit(parent);
  pane.lines->setReadOnly(true);
  pane.lines->setMaximumBlockCount(kMaxLines);
  column->addWidget(pane.lines, 1);

  // In-flight text sits below the settled lines, unstamped and greyed, so it
  // reads as "not committed yet" and never disturbs the scrollback above it.
  pane.interim = new QLabel(parent);
  pane.interim->setWordWrap(true);
  pane.interim->setStyleSheet(QStringLiteral("color: #666; font-style: italic;"));
  column->addWidget(pane.interim);

  row->addLayout(column, 1);
  return pane;
}

MainWindow::Pane& MainWindow::paneFor(StreamKind stream) {
  return stream == StreamKind::Mic ? mic_ : speaker_;
}

void MainWindow::appendLine(StreamKind stream, const QString& line) {
  QPlainTextEdit* view = paneFor(stream).lines;
  // Only follow the tail if the reader is already there, so scrolling back
  // through the transcript during a call is not yanked away.
  QScrollBar* bar = view->verticalScrollBar();
  const bool atBottom = bar->value() >= bar->maximum() - 4;
  view->appendPlainText(line);
  if (atBottom) bar->setValue(bar->maximum());
}

void MainWindow::setInterim(StreamKind stream, const QString& text) {
  paneFor(stream).interim->setText(text);
}

void MainWindow::clearPanes() {
  for (Pane* pane : {&mic_, &speaker_}) {
    pane->lines->clear();
    pane->interim->clear();
  }
}

void MainWindow::showError(const QString& message) {
  // Keep the first error: it is the root cause. Retry notices and the eventual
  // "gave up" message would otherwise bury it. The log keeps the full sequence.
  if (errorLabel_->isVisible()) return;
  errorLabel_->setText(message);
  errorLabel_->setVisible(true);
}

void MainWindow::setSttConnected(StreamKind stream, bool connected) {
  if (stream == StreamKind::Mic) {
    micSttConnected_ = connected;
  } else {
    speakerSttConnected_ = connected;
  }
  if (micSttConnected_ && speakerSttConnected_) {
    errorLabel_->clear();
    errorLabel_->setVisible(false);
  }
  renderStatus();
}

void MainWindow::setCapturing(bool capturing) {
  capturing_ = capturing;
  renderStatus();
}

void MainWindow::onStatus(const QString& status) {
  baseStatus_ = status;
  renderStatus();
}

void MainWindow::renderStatus() {
  QString text = baseStatus_;
  // While capturing, audio arriving is not the same as audio being transcribed —
  // say both so the line cannot claim success while Deepgram is down.
  if (capturing_) {
    const auto state = [](bool up) {
      return up ? QStringLiteral("connected") : QStringLiteral("down");
    };
    text += QStringLiteral("  |  STT: mic %1, speaker %2")
                .arg(state(micSttConnected_), state(speakerSttConnected_));
  }
  statusLabel_->setText(text);
}

void MainWindow::onStats(quint64 micBytes, quint64 speakerBytes) {
  statsLabel_->setText(QStringLiteral("Mic: %1 B | Speaker: %2 B").arg(micBytes).arg(speakerBytes));
}

}  // namespace krisp
