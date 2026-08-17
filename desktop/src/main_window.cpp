#include "main_window.h"

#include <QHBoxLayout>
#include <QLocale>
#include <QRegularExpression>
#include <QScrollBar>
#include <QWidget>

namespace krisp {
namespace {

// Settled lines live in the widget tree, so the widget owns the cap too.
constexpr int kMaxLines = 500;

// The scrollbar rarely lands exactly on maximum(), so "already at the bottom"
// needs a little slack or following the tail would stop working.
constexpr int kAtBottomSlack = 4;

// Column widths from the design: timestamp, then the stream chip.
constexpr int kTimeColumn = 76;
constexpr int kChipColumn = 68;

// This is a light design, so every surface names its own colour. Inheriting the
// system palette would leave the scroll area's viewport dark on a dark-mode
// machine, with the line text painted dark on top of it.
const char* kStyleSheet = R"(
  QMainWindow, #root { background: #f3f3f3; }
  #status { font-size: 13px; color: #1a1a1a; }
  #stats { font-family: "Cascadia Mono", Consolas, monospace; font-size: 12px; color: #6b6b6b; }
  #error {
    background: #fdf2f3; border: 1px solid #f0c4c9; border-radius: 4px;
    padding: 12px 14px; font-size: 13px; font-weight: bold; color: #b00020;
  }
  #card { background: #ffffff; border: 1px solid #e0e0e0; border-radius: 6px; }
  #cardHeader { background: #ffffff; border-bottom: 1px solid #ededed; }
  #timeline, #empty { background: #ffffff; }
  #cardTitle { font-size: 12px; font-weight: bold; color: #6b6b6b; }
  #legendMic {
    font-size: 11px; font-weight: bold; color: #14603b;
    background: #eaf5ef; border: 1px solid #cfe6da; border-radius: 3px; padding: 4px 7px;
  }
  #legendSpeaker {
    font-size: 11px; font-weight: bold; color: #0b4f8f;
    background: #eaf2fb; border: 1px solid #cfe0f3; border-radius: 3px; padding: 4px 7px;
  }
  #row { background: #ffffff; border-top: 1px solid #f4f4f4; }
  #rowFirst { background: #ffffff; border-top: none; }
  #time { font-family: "Cascadia Mono", Consolas, monospace; font-size: 12px; color: #a0a0a0; }
  #chipMic {
    font-size: 11px; font-weight: bold; color: #14603b;
    background: #eaf5ef; border-radius: 3px; padding: 4px 6px;
  }
  #chipSpeaker {
    font-size: 11px; font-weight: bold; color: #0b4f8f;
    background: #eaf2fb; border-radius: 3px; padding: 4px 6px;
  }
  #lineText { font-size: 14px; color: #1a1a1a; }
  #lineTextPending { font-size: 14px; color: #666666; font-style: italic; }
  #emptyTitle { font-size: 14px; color: #6b6b6b; }
  #emptyBody { font-size: 13px; color: #8a8a8a; }
  #hint { font-size: 12px; color: #8a8a8a; }
  QPushButton {
    font-size: 13px; color: #1a1a1a; background: #fdfdfd;
    border: 1px solid #d1d1d1; border-radius: 4px; padding: 0 16px; min-height: 32px;
  }
  QPushButton:disabled { color: #9a9a9a; background: #fafafa; border-color: #e0e0e0; }
  QScrollArea { border: none; background: #ffffff; }
  QScrollArea > QWidget > QWidget { background: #ffffff; }
)";

QLabel* dotLabel(QWidget* parent) {
  auto* dot = new QLabel(parent);
  dot->setFixedSize(8, 8);
  return dot;
}

}  // namespace

MainWindow::MainWindow(TranscriptModel* model, WsServer* server, QWidget* parent)
    : QMainWindow(parent), model_(model), server_(server) {
  setWindowTitle(QStringLiteral("Krisp Dual-Stream Transcripts"));
  resize(960, 640);
  setStyleSheet(QString::fromLatin1(kStyleSheet));

  auto* root = new QWidget(this);
  root->setObjectName(QStringLiteral("root"));
  setCentralWidget(root);

  auto* column = new QVBoxLayout(root);
  column->setContentsMargins(16, 16, 16, 16);
  column->setSpacing(12);
  column->addWidget(buildHeader(root));

  errorLabel_ = new QLabel(root);
  errorLabel_->setObjectName(QStringLiteral("error"));
  errorLabel_->setWordWrap(true);
  errorLabel_->setVisible(false);
  column->addWidget(errorLabel_);

  column->addWidget(buildCard(root), 1);
  column->addWidget(buildFooter(root));

  connect(model_, &TranscriptModel::lineCommitted, this, &MainWindow::appendLine);
  connect(model_, &TranscriptModel::interimChanged, this, &MainWindow::setInterim);
  connect(model_, &TranscriptModel::cleared, this, &MainWindow::clearTimeline);
  connect(server_, &WsServer::statusChanged, this, &MainWindow::onStatus);
  connect(server_, &WsServer::statsChanged, this, &MainWindow::onStats);

  onStats(0, 0);
  renderStatus();
  refreshEmptyState();
}

QWidget* MainWindow::buildHeader(QWidget* parent) {
  auto* header = new QWidget(parent);
  auto* row = new QHBoxLayout(header);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(8);

  dot_ = dotLabel(header);
  statusLabel_ = new QLabel(QStringLiteral("Starting…"), header);
  statusLabel_->setObjectName(QStringLiteral("status"));
  statsLabel_ = new QLabel(header);
  statsLabel_->setObjectName(QStringLiteral("stats"));

  row->addWidget(dot_);
  row->addWidget(statusLabel_, 1);
  row->addWidget(statsLabel_);
  return header;
}

QWidget* MainWindow::buildCard(QWidget* parent) {
  auto* card = new QWidget(parent);
  card->setObjectName(QStringLiteral("card"));
  card->setAttribute(Qt::WA_StyledBackground, true);
  auto* cardColumn = new QVBoxLayout(card);
  cardColumn->setContentsMargins(0, 0, 0, 0);
  cardColumn->setSpacing(0);

  auto* head = new QWidget(card);
  head->setObjectName(QStringLiteral("cardHeader"));
  head->setAttribute(Qt::WA_StyledBackground, true);
  auto* headRow = new QHBoxLayout(head);
  headRow->setContentsMargins(16, 12, 16, 12);
  headRow->setSpacing(8);

  auto* title = new QLabel(QStringLiteral("CONVERSATION"), head);
  title->setObjectName(QStringLiteral("cardTitle"));
  QFont titleFont = title->font();
  titleFont.setLetterSpacing(QFont::PercentageSpacing, 106);  // 0.06em
  title->setFont(titleFont);

  auto* legendMic = new QLabel(QStringLiteral("Microphone (you)"), head);
  legendMic->setObjectName(QStringLiteral("legendMic"));
  auto* legendSpeaker = new QLabel(QStringLiteral("Speaker (tab / others)"), head);
  legendSpeaker->setObjectName(QStringLiteral("legendSpeaker"));

  headRow->addWidget(title);
  headRow->addStretch(1);
  headRow->addWidget(legendMic);
  headRow->addWidget(legendSpeaker);
  cardColumn->addWidget(head);

  // The timeline: settled rows, then one in-flight row per stream.
  auto* content = new QWidget(card);
  content->setObjectName(QStringLiteral("timeline"));
  timeline_ = new QVBoxLayout(content);
  timeline_->setContentsMargins(0, 0, 0, 0);
  timeline_->setSpacing(0);

  for (auto* interim : {&micInterim_, &speakerInterim_}) {
    const StreamKind stream = (interim == &micInterim_) ? StreamKind::Mic : StreamKind::Speaker;
    interim->row = makeRow(QStringLiteral("···"), stream, QString(), true);
    interim->text = interim->row->findChild<QLabel*>(QStringLiteral("lineTextPending"));
    interim->row->setVisible(false);
    timeline_->addWidget(interim->row);
  }
  timeline_->addStretch(1);

  scroll_ = new QScrollArea(card);
  scroll_->setWidget(content);
  scroll_->setWidgetResizable(true);
  scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  cardColumn->addWidget(scroll_, 1);

  empty_ = new QWidget(card);
  empty_->setObjectName(QStringLiteral("empty"));
  auto* emptyColumn = new QVBoxLayout(empty_);
  emptyColumn->setContentsMargins(24, 24, 24, 24);
  emptyColumn->setSpacing(6);
  emptyTitle_ = new QLabel(empty_);
  emptyTitle_->setObjectName(QStringLiteral("emptyTitle"));
  emptyTitle_->setAlignment(Qt::AlignCenter);
  emptyBody_ = new QLabel(empty_);
  emptyBody_->setObjectName(QStringLiteral("emptyBody"));
  emptyBody_->setAlignment(Qt::AlignCenter);
  emptyBody_->setWordWrap(true);
  emptyColumn->addStretch(1);
  emptyColumn->addWidget(emptyTitle_);
  emptyColumn->addWidget(emptyBody_);
  emptyColumn->addStretch(1);
  cardColumn->addWidget(empty_, 1);

  return card;
}

QWidget* MainWindow::buildFooter(QWidget* parent) {
  auto* footer = new QWidget(parent);
  auto* row = new QHBoxLayout(footer);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(8);

  hint_ = new QLabel(
      QStringLiteral("Greyed italic text is still in flight — it commits with a "
                     "timestamp when the utterance settles."),
      footer);
  hint_->setObjectName(QStringLiteral("hint"));

  clearBtn_ = new QPushButton(QStringLiteral("Clear transcripts"), footer);
  connect(clearBtn_, &QPushButton::clicked, model_, &TranscriptModel::clear);

  row->addWidget(hint_);
  row->addStretch(1);
  row->addWidget(clearBtn_);
  return footer;
}

QWidget* MainWindow::makeRow(const QString& time, StreamKind stream, const QString& text,
                             bool inFlight) {
  const bool mic = stream == StreamKind::Mic;

  auto* row = new QWidget;
  row->setObjectName(QStringLiteral("row"));
  row->setAttribute(Qt::WA_StyledBackground, true);
  auto* layout = new QHBoxLayout(row);
  layout->setContentsMargins(16, 11, 16, 11);
  layout->setSpacing(14);
  layout->setAlignment(Qt::AlignTop);

  auto* timeLabel = new QLabel(time, row);
  timeLabel->setObjectName(QStringLiteral("time"));
  timeLabel->setFixedWidth(kTimeColumn);

  auto* chip = new QLabel(mic ? QStringLiteral("You") : QStringLiteral("Others"), row);
  chip->setObjectName(mic ? QStringLiteral("chipMic") : QStringLiteral("chipSpeaker"));
  chip->setAlignment(Qt::AlignCenter);
  chip->setFixedWidth(kChipColumn);

  auto* body = new QLabel(text, row);
  body->setObjectName(inFlight ? QStringLiteral("lineTextPending")
                               : QStringLiteral("lineText"));
  body->setWordWrap(true);
  body->setTextInteractionFlags(Qt::TextSelectableByMouse);

  layout->addWidget(timeLabel);
  layout->addWidget(chip);
  layout->addWidget(body, 1);
  return row;
}

MainWindow::InterimRow& MainWindow::interimFor(StreamKind stream) {
  return stream == StreamKind::Mic ? micInterim_ : speakerInterim_;
}

void MainWindow::appendLine(StreamKind stream, const QString& line) {
  // TranscriptModel emits "[HH:mm:ss] text"; the design gives the stamp its own
  // column. Split it rather than re-stamping, so the time shown is the one the
  // model recorded. An unrecognised shape falls back to rendering the line whole.
  static const QRegularExpression stamped(
      QStringLiteral("^\\[(\\d{2}:\\d{2}:\\d{2})\\] (.*)$"));
  const auto match = stamped.match(line);
  const QString time = match.hasMatch() ? match.captured(1) : QString();
  const QString text = match.hasMatch() ? match.captured(2) : line;

  QScrollBar* bar = scroll_->verticalScrollBar();
  const bool atBottom = bar->value() >= bar->maximum() - kAtBottomSlack;

  // Settled rows go above the two in-flight rows and the trailing stretch.
  auto* row = makeRow(time, stream, text, false);
  if (lineCount_ == 0) row->setObjectName(QStringLiteral("rowFirst"));
  timeline_->insertWidget(lineCount_, row);
  ++lineCount_;

  while (lineCount_ > kMaxLines) {
    delete timeline_->takeAt(0)->widget();
    --lineCount_;
  }

  cleared_ = false;
  refreshEmptyState();
  if (atBottom) bar->setValue(bar->maximum());
}

void MainWindow::setInterim(StreamKind stream, const QString& text) {
  InterimRow& interim = interimFor(stream);
  interim.text->setText(text);
  interim.row->setVisible(!text.isEmpty());
  if (!text.isEmpty()) cleared_ = false;
  refreshEmptyState();
}

void MainWindow::clearTimeline() {
  while (lineCount_ > 0) {
    delete timeline_->takeAt(0)->widget();
    --lineCount_;
  }
  for (auto* interim : {&micInterim_, &speakerInterim_}) {
    interim->text->clear();
    interim->row->setVisible(false);
  }
  cleared_ = true;
  refreshEmptyState();
}

void MainWindow::refreshEmptyState() {
  const bool hasContent = lineCount_ > 0 || micInterim_.row->isVisible() ||
                          speakerInterim_.row->isVisible();

  if (!hasContent) {
    if (errorLabel_->isVisible()) {
      emptyTitle_->setText(QStringLiteral("Audio is arriving, but no transcript can be produced."));
      emptyBody_->setText(QStringLiteral(
          "The byte counters above keep rising, so capture and transport are fine. "
          "The error clears once both streams reconnect."));
    } else if (cleared_) {
      emptyTitle_->setText(QStringLiteral("Transcript cleared."));
      emptyBody_->setText(QStringLiteral(
          "Capture never stopped — the next settled utterance starts the timeline again."));
    } else {
      emptyTitle_->setText(QStringLiteral("Waiting for audio."));
      emptyBody_->setText(QStringLiteral(
          "Load the unpacked extension, open the Meet tab, then press Start in the popup."));
    }
  }

  empty_->setVisible(!hasContent);
  scroll_->setVisible(hasContent);
  hint_->setVisible(hasContent);
  clearBtn_->setEnabled(hasContent);
}

void MainWindow::showError(const QString& message) {
  // Keep the first error: it is the root cause. Retry notices and the eventual
  // "gave up" message would otherwise bury it. The log keeps the full sequence.
  if (errorLabel_->isVisible()) return;
  errorLabel_->setText(message);
  errorLabel_->setVisible(true);
  renderStatus();
  refreshEmptyState();
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
    refreshEmptyState();
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

  const QString colour = errorLabel_->isVisible() ? QStringLiteral("#b00020")
                         : capturing_             ? QStringLiteral("#1b7f4e")
                                                  : QStringLiteral("#8a8a8a");
  dot_->setStyleSheet(
      QStringLiteral("background: %1; border-radius: 4px;").arg(colour));
}

void MainWindow::onStats(quint64 micBytes, quint64 speakerBytes) {
  const QLocale locale;
  statsLabel_->setText(QStringLiteral("Mic: %1 B  |  Speaker: %2 B")
                           .arg(locale.toString(static_cast<qulonglong>(micBytes)),
                                locale.toString(static_cast<qulonglong>(speakerBytes))));
}

}  // namespace krisp
