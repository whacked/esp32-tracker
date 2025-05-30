#pragma once
#include <string>
#include <vector>
#include <utility>

std::string json_escape(const std::string &s);
std::vector<std::string> splitBySpace(const std::string &input);
std::pair<std::string, std::string> parseCommand(const std::string &fullCommand);