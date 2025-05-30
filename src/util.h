#pragma once
#include <string>
#include <utility>

std::string json_escape(const std::string &s);
std::pair<std::string, std::string> parseCommand(const std::string &fullCommand);