#pragma once
#include <string>
#include "DataLogger.h"
#include <build_metadata.h>

const std::string STATUS_OK_JSON = "{\"status\":\"ok\"}";

inline std::string errorJsonResponse(const std::string &message)
{
    return "{\"status\":\"error\",\"message\":\"" + json_escape(message) + "\"}";
}

class CommandHandler
{
public:
    virtual ~CommandHandler() = default;

    // Process a command and return the response
    virtual std::string handleCommand(const std::string &command, const std::string &args) = 0;

    // Helper to parse command and args from a full command string
    static std::pair<std::string, std::string> parseCommand(const std::string &fullCommand)
    {
        size_t spaceIdx = fullCommand.find(' ');
        if (spaceIdx == std::string::npos)
        {
            return {fullCommand, ""};
        }
        return {
            fullCommand.substr(0, spaceIdx),
            fullCommand.substr(spaceIdx + 1)};
    }
};

// Concrete implementation that uses DataLogger
class DataLoggerCommandHandler : public CommandHandler
{
private:
    DataLogger &dataLogger;
    int &samplingRateHz;

public:
    DataLoggerCommandHandler(DataLogger &logger, int &rateHz)
        : dataLogger(logger), samplingRateHz(rateHz) {}

    std::string handleCommand(const std::string &command, const std::string &args) override
    {
        if (command == CMD_GETVERSION)
        {
            return "0.0." + std::to_string(BUILD_NUMBER);
        }
        else if (command == CMD_SETTIME)
        {
            time_t targetTime = std::stol(args);
            if (targetTime > 0)
            {
                dataLogger.setTimeOffset(targetTime - time(nullptr));
                return "{\"status\":\"ok\",\"offset\":" +
                       std::to_string(dataLogger.getTimeOffset()) +
                       ",\"time\":\"" + dataLogger.getTimestamp() + "\"}";
            }
            return "{\"status\":\"error\",\"message\":\"Invalid timestamp\"}";
        }
        else if (command == CMD_CLEARBUFFER)
        {
            dataLogger.clearBuffer();
            return STATUS_OK_JSON;
        }
        else if (command == CMD_READBUFFER)
        {
            size_t offset = 0;
            size_t length = 20; // Default page size

            if (!args.empty())
            {
                size_t spaceIdx = args.find(' ');
                if (spaceIdx == std::string::npos)
                {
                    offset = std::stoul(args);
                }
                else
                {
                    offset = std::stoul(args.substr(0, spaceIdx));
                    length = std::stoul(args.substr(spaceIdx + 1));
                }
            }
            return dataLogger.getBufferJsonPaginated(offset, length);
        }
        else if (command == CMD_STARTLOGGING)
        {
            dataLogger.setLoggingEnabled(true);
            return STATUS_OK_JSON;
        }
        else if (command == CMD_STOPLOGGING)
        {
            dataLogger.setLoggingEnabled(false);
            return STATUS_OK_JSON;
        }
        else if (command == CMD_GETNOW)
        {
            return "{\"epoch\":" + std::to_string(dataLogger.getCorrectedTime()) +
                   ",\"local\":\"" + dataLogger.getTimestamp() + "\"}";
        }
        else if (command == CMD_GETSTATUS)
        {
            return "{\"logging\":" + std::string(dataLogger.isLoggingEnabled() ? "true" : "false") +
                   ",\"bufferSize\":" + std::to_string(dataLogger.getBufferSize()) +
                   ",\"rateHz\":" + std::to_string(samplingRateHz) + "}";
        }
        else if (command == CMD_SETSAMPLINGRATE)
        {
            int rate = std::stoi(args);
            if (rate > 0)
            {
                samplingRateHz = rate;
                return "{\"status\":\"ok\",\"rate\":" + std::to_string(rate) + "}";
            }
            return "{\"status\":\"error\",\"message\":\"Invalid rate\"}";
        }
        else if (command == CMD_DROPRECORDS)
        {
            size_t spaceIdx = args.find(' ');
            if (spaceIdx == std::string::npos)
            {
                return "{\"status\":\"error\",\"message\":\"Invalid format\"}";
            }

            size_t offset = std::stoul(args.substr(0, spaceIdx));
            size_t length = std::stoul(args.substr(spaceIdx + 1));

            bool success = dataLogger.dropRecords(offset, length);
            return "{\"status\":\"" + std::string(success ? "ok" : "error") +
                   "\",\"offset\":" + std::to_string(offset) +
                   ",\"length\":" + std::to_string(length) + "}";
        }
        else if (command == CMD_SETLOGLEVEL)
        {
            size_t spaceIdx = args.find(' ');
            if (spaceIdx == std::string::npos)
            {
                return errorJsonResponse("Invalid format");
            }
                return errorJsonResponse("Invalid format");

            bool success = dataLogger.dropRecords(offset, length);
            return "{\"status\":\"" + std::string(success ? "ok" : "error") +
                   "\",\"offset\":" + std::to_string(offset) +
                   ",\"length\":" + std::to_string(length) + "}";
        }
        else if (command == CMD_SETLOGLEVEL)
        {
            size_t spaceIdx = args.find(' ');
            if (spaceIdx == std::string::npos)
            {
                return errorResponse("Invalid format");
            }
            size_t length = std::stoul(args.substr(spaceIdx + 1));

            bool success = dataLogger.dropRecords(offset, length);
            return "{\"status\":\"" + std::string(success ? "ok" : "error") +
                   "\",\"offset\":" + std::to_string(offset) +
                   ",\"length\":" + std::to_string(length) + "}";
        }

        return "{\"status\":\"error\",\"message\":\"Unknown command: '" + json_escape(command) + "'\"}";
    }
};