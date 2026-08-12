#include "env_loader.h"

#include <cstdlib>
#include <fstream>

namespace krisp {
namespace {

std::string trim(std::string s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
  return s;
}

}  // namespace

std::map<std::string, std::string> parseEnvFile(const std::string& path) {
  std::map<std::string, std::string> env;
  std::ifstream in(path);
  if (!in) return env;

  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    const auto eq = line.find('=');
    if (eq == std::string::npos) continue;

    auto key = trim(line.substr(0, eq));
    auto value = trim(line.substr(eq + 1));
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
      value = value.substr(1, value.size() - 2);
    }
    if (!key.empty()) env.emplace(std::move(key), std::move(value));
  }
  return env;
}

std::string envOrFile(const std::map<std::string, std::string>& fileEnv, const char* key) {
  if (const char* fromEnv = std::getenv(key); fromEnv != nullptr && *fromEnv != '\0') {
    return fromEnv;
  }
  const auto it = fileEnv.find(key);
  return it == fileEnv.end() ? std::string() : it->second;
}

}  // namespace krisp
