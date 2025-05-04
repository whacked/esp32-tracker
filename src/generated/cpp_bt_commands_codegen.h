// AUTO-GENERATED FILE. DO NOT EDIT.

#pragma once
#include <string>
#include <vector>

enum class Command {
    GetVersion,
    SetTime,
    ClearBuffer,
    ReadBuffer,
    StartLogging,
    StopLogging,
    GetNow,
    GetStatus,
    SetSamplingRate,
    Calibrate,
    Reset,
    SetLogLevel,
    DropRecords,
    Unknown,
};

constexpr const char* CMD_GETVERSION = "getVersion";
constexpr const char* CMD_SETTIME = "setTime";
constexpr const char* CMD_CLEARBUFFER = "clearBuffer";
constexpr const char* CMD_READBUFFER = "readBuffer";
constexpr const char* CMD_STARTLOGGING = "startLogging";
constexpr const char* CMD_STOPLOGGING = "stopLogging";
constexpr const char* CMD_GETNOW = "getNow";
constexpr const char* CMD_GETSTATUS = "getStatus";
constexpr const char* CMD_SETSAMPLINGRATE = "setSamplingRate";
constexpr const char* CMD_CALIBRATE = "calibrate";
constexpr const char* CMD_RESET = "reset";
constexpr const char* CMD_SETLOGLEVEL = "setLogLevel";
constexpr const char* CMD_DROPRECORDS = "dropRecords";
constexpr const char* CMD_UNKNOWN = "unknown";

struct SetTimeResponse {
    std::string status;
    int offset;
    std::string time;
};


struct ReadBufferResponse {
    std::vector</*UnknownType*/> records;
    int length;
};


struct GetNowResponse {
    int epoch;
    std::string local;
};


struct GetStatusResponse {
    bool logging;
    int bufferSize;
    int rateHz;
};


struct SetLogLevelResponse {
    std::string status;
    std::string printer;
    int level;
};


struct DropRecordsResponse {
    std::string status;
    int offset;
    int length;
};

