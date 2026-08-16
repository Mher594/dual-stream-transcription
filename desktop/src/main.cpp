#include "deepgram_client.h"
#include "main_window.h"
#include "protocol.h"
#include "transcript_model.h"
#include "ws_server.h"

#include <QApplication>

#include <limits>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  // Timestamped log on stderr. The UI shows only the first error of a failure;
  // the console keeps the whole sequence, which is what you need to diagnose it.
  qSetMessagePattern(QStringLiteral("%{time hh:mm:ss.zzz} [%{type}] %{message}"));

  // Configuration comes from the environment, set by whoever launches the app.
  const QString apiKey = qEnvironmentVariable("DEEPGRAM_API_KEY");
  // Empty keeps the client's default model.
  const QString sttModel = qEnvironmentVariable("KRISP_STT_MODEL");

  quint16 port = krisp::kDefaultPort;
  if (const QString portStr = qEnvironmentVariable("KRISP_WS_PORT"); !portStr.isEmpty()) {
    bool ok = false;
    const int p = portStr.toInt(&ok);
    // Upper bound is "fits in a port number", so take it from the type itself.
    if (ok && p > 0 && p <= std::numeric_limits<quint16>::max()) {
      port = static_cast<quint16>(p);
    }
  }

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
    const QString message = QStringLiteral(
        "DEEPGRAM_API_KEY is not set — set it in the shell that launches the app, "
        "then restart. See README.md.");
    qWarning("%s", qUtf8Printable(message));
    window.showError(message);
  }

  if (!server.listen(port)) {
    return 1;
  }

  window.show();
  return app.exec();
}
