#include "protocol.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace krisp {

namespace {

MessageType typeFromString(std::string_view t) {
  if (t == "hello") return MessageType::Hello;
  if (t == "hello_ack") return MessageType::HelloAck;
  if (t == "capture_started") return MessageType::CaptureStarted;
  if (t == "capture_stopped") return MessageType::CaptureStopped;
  if (t == "error") return MessageType::Error;
  return MessageType::Unknown;
}

}  // namespace

std::optional<ControlMessage> parseControlJson(std::string_view json) {
  QJsonParseError err;
  const auto doc = QJsonDocument::fromJson(
      QByteArray(json.data(), static_cast<int>(json.size())), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    return std::nullopt;
  }
  const QJsonObject obj = doc.object();
  ControlMessage msg;
  msg.type = typeFromString(obj.value(QStringLiteral("type")).toString().toStdString());
  msg.sampleRate = obj.value(QStringLiteral("sampleRate")).toInt();
  msg.format = obj.value(QStringLiteral("format")).toString().toStdString();
  msg.channels = obj.value(QStringLiteral("channels")).toInt();
  msg.port = obj.value(QStringLiteral("port")).toInt();
  msg.message = obj.value(QStringLiteral("message")).toString().toStdString();
  return msg;
}

std::string makeHelloAckJson(int port) {
  QJsonObject obj;
  obj.insert(QStringLiteral("type"), QStringLiteral("hello_ack"));
  obj.insert(QStringLiteral("port"), port);
  return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

std::string makeErrorJson(std::string_view message) {
  QJsonObject obj;
  obj.insert(QStringLiteral("type"), QStringLiteral("error"));
  obj.insert(QStringLiteral("message"), QString::fromUtf8(message.data(), static_cast<int>(message.size())));
  return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

std::optional<uint8_t> parseAudioFrame(const uint8_t* data, size_t size) {
  if (data == nullptr || size < kStreamIdBytes) return std::nullopt;
  const uint8_t streamId = data[0];
  if (streamId != kStreamMic && streamId != kStreamSpeaker) return std::nullopt;
  const size_t pcmBytes = size - kStreamIdBytes;
  if (pcmBytes % kBytesPerSample != 0) return std::nullopt;
  return streamId;
}

bool isValidHello(const ControlMessage& msg) {
  return msg.type == MessageType::Hello && msg.sampleRate == kSampleRate &&
         msg.channels == kChannels && msg.format == kPcmFormat;
}

}  // namespace krisp
