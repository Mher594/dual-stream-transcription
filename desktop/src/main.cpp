#include "deepgram_client.h"
#include "env_loader.h"
#include "main_window.h"
#include "transcript_model.h"
#include "ws_server.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>

namespace {

// Search a few locations so the app works whether it is launched from the repo
// root, from `desktop/`, or from the build tree.
std::map<std::string, std::string> loadEnvFromDisk() {
  const QStringList candidates = {
      QDir::current().absoluteFilePath(QStringLiteral(".env")),
      QDir::current().absoluteFilePath(QStringLiteral("../.env")),
      QDir::current().absoluteFilePath(QStringLiteral("../../.env")),
      QFileInfo(QCoreApplication::applicationDirPath() + QStringLiteral("/../../../.env"))
          .absoluteFilePath(),
      QFileInfo(QCoreApplication::applicationDirPath() + QStringLiteral("/../../.env"))
          .absoluteFilePath(),
  };
  for (const QString& path : candidates) {
    if (QFileInfo::exists(path)) return krisp::parseEnvFile(path.toStdString());
  }
  return {};
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  // Timestamped log on stderr. The UI shows only the first error of a failure;
  // the console keeps the whole sequence, which is what you need to diagnose it.
  qSetMessagePattern(QStringLiteral("%{time hh:mm:ss.zzz} [%{type}] %{message}"));

  const auto fileEnv = loadEnvFromDisk();
  const QString apiKey =
      QString::fromStdString(krisp::envOrFile(fileEnv, "DEEPGRAM_API_KEY"));

  quint16 port = 8765;
  if (const std::string portStr = krisp::envOrFile(fileEnv, "KRISP_WS_PORT"); !portStr.empty()) {
    bool ok = false;
    const int p = QString::fromStdString(portStr).toInt(&ok);
    if (ok && p > 0 && p < 65536) port = static_cast<quint16>(p);
  }

  // Empty keeps the client's default model.
  const QString sttModel =
      QString::fromStdString(krisp::envOrFile(fileEnv, "KRISP_STT_MODEL"));

  krisp::TranscriptModel model;
  krisp::DeepgramClient micStt(krisp::StreamKind::Mic);
  krisp::DeepgramClient speakerStt(krisp::StreamKind::Speaker);
  krisp::WsServer server(&model, &micStt, &speakerStt);

  QObject::connect(&micStt, &krisp::DeepgramClient::transcript, &model,
                   &krisp::TranscriptModel::applyUpdate);
  QObject::connect(&speakerStt, &krisp::DeepgramClient::transcript, &model,
                   &krisp::TranscriptModel::applyUpdate);

  krisp::MainWindow window(&model, &server);
  QObject::connect(&server, &krisp::WsServer::capturingChanged, &window,
                   &krisp::MainWindow::setCapturing);
  for (auto* stt : {&micStt, &speakerStt}) {
    stt->setApiKey(apiKey);
    stt->setModel(sttModel);
    QObject::connect(stt, &krisp::DeepgramClient::errorOccurred, &window,
                     &krisp::MainWindow::showError);
    QObject::connect(stt, &krisp::DeepgramClient::connectedChanged, &window,
                     &krisp::MainWindow::setSttConnected);
  }

  if (apiKey.isEmpty()) {
    window.showError(QStringLiteral(
        "DEEPGRAM_API_KEY is not set — copy .env.example to .env at the repo root "
        "and add your key, then restart."));
  }

  if (!server.listen(port)) {
    return 1;
  }

  window.show();
  return app.exec();
}
