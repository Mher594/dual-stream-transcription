#include "env_loader.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QtGlobal>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::string writeTempEnv(const std::string& name, const std::string& contents) {
  const auto path = (std::filesystem::temp_directory_path() / name).string();
  std::ofstream out(path, std::ios::trunc);
  out << contents;
  return path;
}

}  // namespace

TEST(EnvLoaderTest, ParsesPairsAndIgnoresCommentsAndJunk) {
  const auto path = writeTempEnv("krisp_parse.env",
                                 "# a comment\n"
                                 "\n"
                                 "DEEPGRAM_API_KEY = abc123 \n"
                                 "KRISP_WS_PORT=9000\n"
                                 "QUOTED=\"quoted value\"\n"
                                 "line without an equals sign\n");
  const auto env = krisp::parseEnvFile(path);

  EXPECT_EQ(env.at("DEEPGRAM_API_KEY"), "abc123");
  EXPECT_EQ(env.at("KRISP_WS_PORT"), "9000");
  EXPECT_EQ(env.at("QUOTED"), "quoted value");
  EXPECT_EQ(env.size(), 3u);
}

TEST(EnvLoaderTest, MissingFileYieldsEmptyMap) {
  EXPECT_TRUE(krisp::parseEnvFile("no_such_file_here.env").empty());
}

// Regression guard. The previous loader pushed values into the Windows
// environment block with SetEnvironmentVariableA, which the MSVC CRT's getenv
// never sees — so DEEPGRAM_API_KEY silently arrived empty and nothing was ever
// transcribed. The value must reach the caller directly.
TEST(EnvLoaderTest, FileValueReachesCallerWhenEnvironmentIsUnset) {
  qunsetenv("KRISP_TEST_FILE_ONLY");
  const auto path = writeTempEnv("krisp_fileonly.env", "KRISP_TEST_FILE_ONLY=from_file\n");
  const auto env = krisp::parseEnvFile(path);

  EXPECT_EQ(krisp::envOrFile(env, "KRISP_TEST_FILE_ONLY"), "from_file");
}

TEST(EnvLoaderTest, ProcessEnvironmentWinsOverFile) {
  qputenv("KRISP_TEST_OVERRIDE", QByteArray("from_env"));
  const auto path = writeTempEnv("krisp_override.env", "KRISP_TEST_OVERRIDE=from_file\n");
  const auto env = krisp::parseEnvFile(path);

  EXPECT_EQ(krisp::envOrFile(env, "KRISP_TEST_OVERRIDE"), "from_env");
  qunsetenv("KRISP_TEST_OVERRIDE");
}

TEST(EnvLoaderTest, UnknownKeyYieldsEmptyString) {
  EXPECT_TRUE(krisp::envOrFile({}, "KRISP_TEST_ABSENT").empty());
}
