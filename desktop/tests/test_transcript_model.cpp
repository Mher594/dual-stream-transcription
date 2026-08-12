#include "transcript_model.h"

#include <gtest/gtest.h>

#include <QRegularExpression>
#include <QString>
#include <QVector>

using krisp::StreamKind;
using krisp::TranscriptModel;

namespace {

// Records what a view would have been told to do.
struct Recorder {
  struct Line {
    StreamKind stream;
    QString text;
  };

  QVector<Line> committed;
  QVector<Line> interims;
  int clears = 0;

  explicit Recorder(TranscriptModel& model) {
    QObject::connect(&model, &TranscriptModel::lineCommitted,
                     [this](StreamKind s, const QString& l) { committed.append({s, l}); });
    QObject::connect(&model, &TranscriptModel::interimChanged,
                     [this](StreamKind s, const QString& t) { interims.append({s, t}); });
    QObject::connect(&model, &TranscriptModel::cleared, [this]() { ++clears; });
  }

  QVector<Line> committedFor(StreamKind stream) const {
    QVector<Line> out;
    for (const auto& line : committed) {
      if (line.stream == stream) out.append(line);
    }
    return out;
  }
};

bool isStamped(const QString& line, const QString& text) {
  return QRegularExpression(QStringLiteral("^\\[\\d{2}:\\d{2}:\\d{2}\\] %1$")
                                .arg(QRegularExpression::escape(text)))
      .match(line)
      .hasMatch();
}

}  // namespace

TEST(TranscriptModelTest, KeepsStreamsSeparate) {
  TranscriptModel model;
  Recorder rec(model);

  model.applyUpdate(StreamKind::Mic, QStringLiteral("hello"), true);
  model.applyUpdate(StreamKind::Speaker, QStringLiteral("hi there"), true);

  ASSERT_EQ(rec.committedFor(StreamKind::Mic).size(), 1);
  ASSERT_EQ(rec.committedFor(StreamKind::Speaker).size(), 1);
  EXPECT_TRUE(isStamped(rec.committedFor(StreamKind::Mic)[0].text, QStringLiteral("hello")));
  EXPECT_TRUE(isStamped(rec.committedFor(StreamKind::Speaker)[0].text, QStringLiteral("hi there")));
}

TEST(TranscriptModelTest, StampsCommittedLinesButNotInterim) {
  TranscriptModel model;
  Recorder rec(model);

  model.applyUpdate(StreamKind::Mic, QStringLiteral("still going"), false);
  ASSERT_EQ(rec.interims.size(), 1);
  EXPECT_EQ(rec.interims[0].text, QStringLiteral("still going"));
  EXPECT_TRUE(rec.committed.isEmpty());

  model.applyUpdate(StreamKind::Mic, QStringLiteral("still going now"), true);
  ASSERT_EQ(rec.committed.size(), 1);
  EXPECT_TRUE(isStamped(rec.committed[0].text, QStringLiteral("still going now")));
}

TEST(TranscriptModelTest, CommittingClearsTheInterimItReplaces) {
  TranscriptModel model;
  Recorder rec(model);

  model.applyUpdate(StreamKind::Mic, QStringLiteral("partial"), false);
  model.applyUpdate(StreamKind::Mic, QStringLiteral("partial sentence"), true);

  // The last thing the view is told about interim must be "nothing", or the
  // in-flight text would sit under the committed line forever.
  ASSERT_FALSE(rec.interims.isEmpty());
  EXPECT_TRUE(rec.interims.last().text.isEmpty());
}

TEST(TranscriptModelTest, SilenceCommitsNoLine) {
  TranscriptModel model;
  Recorder rec(model);

  model.applyUpdate(StreamKind::Mic, QString(), true);
  model.applyUpdate(StreamKind::Mic, QStringLiteral("   "), true);

  EXPECT_TRUE(rec.committed.isEmpty());
}

TEST(TranscriptModelTest, UnchangedInterimDoesNotChurnTheView) {
  TranscriptModel model;
  Recorder rec(model);

  model.applyUpdate(StreamKind::Mic, QStringLiteral("same"), false);
  model.applyUpdate(StreamKind::Mic, QStringLiteral("same"), false);
  model.applyUpdate(StreamKind::Mic, QStringLiteral("same"), false);

  EXPECT_EQ(rec.interims.size(), 1);
}

TEST(TranscriptModelTest, ClearResetsBothStreams) {
  TranscriptModel model;
  Recorder rec(model);

  model.applyUpdate(StreamKind::Mic, QStringLiteral("mic text"), false);
  model.applyUpdate(StreamKind::Speaker, QStringLiteral("speaker text"), false);
  model.clear();
  EXPECT_EQ(rec.clears, 1);

  // Interim state is gone, so the same text is news again.
  rec.interims.clear();
  model.applyUpdate(StreamKind::Mic, QStringLiteral("mic text"), false);
  EXPECT_EQ(rec.interims.size(), 1);
}
