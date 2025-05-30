#include <string>
#include <cstdio>
#include <cstdlib> // for std::strtol
#include <vector>
#include <cctype>

std::string json_escape(const std::string &s)
{
    std::string out;
    for (char c : s)
    {
        switch (c)
        {
        case '\"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                // Control character, encode as \u00XX
                char buf[7];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            }
            else
            {
                out += c;
            }
        }
    }
    return out;
}

std::vector<std::string> splitBySpace(const std::string &input)
{
    std::vector<std::string> result;
    size_t pos = 0;
    while (pos < input.length())
    {
        while (pos < input.length() && std::isspace(input[pos]))
            ++pos; // skip spaces
        size_t start = pos;
        while (pos < input.length() && !std::isspace(input[pos]))
            ++pos;
        if (start < pos)
            result.emplace_back(input.substr(start, pos - start));
    }
    return result;
}

std::pair<std::string, std::string> parseCommand(const std::string &fullCommand)
{
    // Skip leading whitespace
    size_t start = fullCommand.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
    {
        return {"", ""};
    }

    // Find end of command (first whitespace after command)
    size_t cmdEnd = fullCommand.find_first_of(" \t\n\r", start);
    if (cmdEnd == std::string::npos)
    {
        return {fullCommand.substr(start), ""};
    }

    // Find start of args (first non-whitespace after command)
    size_t argsStart = fullCommand.find_first_not_of(" \t\n\r", cmdEnd);
    if (argsStart == std::string::npos)
    {
        return {fullCommand.substr(start, cmdEnd - start), ""};
    }

    return {
        fullCommand.substr(start, cmdEnd - start),
        fullCommand.substr(argsStart)};
}
