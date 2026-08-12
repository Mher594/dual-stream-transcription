#pragma once

#include <map>
#include <string>

namespace krisp {

// Parses KEY=VALUE lines from a .env file. A missing or unreadable file yields
// an empty map — a .env is optional when the variables are already exported.
std::map<std::string, std::string> parseEnvFile(const std::string& path);

// Looks up `key`, preferring the real process environment over the .env file.
// Returns an empty string when neither has it.
std::string envOrFile(const std::map<std::string, std::string>& fileEnv, const char* key);

}  // namespace krisp
