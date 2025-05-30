#pragma once
#include <string>
#include "DataLogger.h"
#include <build_metadata.h>
#include "util.h"

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
                SetTimeResponse response{
                    .status = "ok",
                    .offset = dataLogger.getTimeOffset(),
                    .time = dataLogger.getTimestamp()};
                return SetTimeResponseToJson(response);
            }
            return errorJsonResponse("Invalid timestamp");
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
            GetNowResponse response{
                .epoch = dataLogger.getCorrectedTime(),
                .local = dataLogger.getTimestamp()};
            return GetNowResponseToJson(response);
        }
        else if (command == CMD_GETSTATUS)
        {
            GetStatusResponse response{
                .logging = dataLogger.isLoggingEnabled(),
                .bufferSize = dataLogger.getBufferSize(),
                .rateHz = samplingRateHz};
            return GetStatusResponseToJson(response);
        }
        else if (command == CMD_SETSAMPLINGRATE)
        {
            int rate = std::stoi(args);
            if (rate > 0)
            {
                samplingRateHz = rate;
                return "{\"status\":\"ok\",\"rate\":" + std::to_string(rate) + "}";
            }
            return errorJsonResponse("Invalid rate");
        }
        else if (command == CMD_DROPRECORDS)
        {
            ReadBufferArgs dropArgs = parseReadBufferArgs(args);
            if (dropArgs.offset < 0 || dropArgs.length < 0)
            {
                return errorJsonResponse("Invalid arguments");
            }

            bool success = dataLogger.dropRecords(dropArgs.offset, dropArgs.length);
            DropRecordsResponse response{
                .status = success ? "ok" : "error",
                .offset = dropArgs.offset,
                .length = dropArgs.length};
            return DropRecordsResponseToJson(response);
        }
        else if (command == CMD_SETLOGLEVEL)
        {
            SetLogLevelArgs llArgs = parseSetLogLevelArgsArgs(args);

            if (llArgs.level >= 0 && llArgs.level <= 3)
            {
                if (llArgs.printer == "raw")
                {
                    rawPrinter.logLevel = llArgs.level;
                }
                else if (llArgs.printer == "event")
                {
                    eventPrinter.logLevel = llArgs.level;
                }
                else if (llArgs.printer == "status")
                {
                    statusPrinter.logLevel = llArgs.level;
                }
                else if (llArgs.printer == "all")
                {
                    rawPrinter.logLevel = llArgs.level;
                    eventPrinter.logLevel = llArgs.level;
                    statusPrinter.logLevel = llArgs.level;
                }
                else
                {
                    return errorJsonResponse("Invalid printer name");
                }

                SetLogLevelResponse response{
                    "ok",
                    llArgs.printer,
                    llArgs.level};
                return SetLogLevelResponseToJson(response);
            }
            return errorJsonResponse("Invalid level");
        }
        return errorJsonResponse("Unknown command: '" + command + "'");
    }
};