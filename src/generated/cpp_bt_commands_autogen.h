// AUTO-GENERATED FILE. DO NOT EDIT.

// Header guard
#pragma once
#include "util.h"
// Standard includes
#include <string>
#include <vector>
#include <time.h>

// RecordType enum definition
enum RecordType {
    MEASUREMENT,
    SIP,
    REFILL,
};

// RecordType toString function
inline const char* RecordTypeToString(RecordType t) {
    switch (t) {
        case MEASUREMENT: return "measurement";
        case SIP: return "sip";
        case REFILL: return "refill";
        default: return "unknown";
    }
}

// Record struct definition
struct Record {
    time_t start_time;
    time_t end_time;
    float grams;
    RecordType type;
};

// Record toJson function
inline std::string RecordToJson(const Record& r) {
    return std::string("{") +
        "\"start_time\":" + std::to_string(r.start_time) +
        ",\"end_time\":" + std::to_string(r.end_time) +
        ",\"grams\":" + std::to_string(r.grams) +
        ",\"type\":" + "\"" + RecordTypeToString(r.type) + "\"" + "}";
}

// Command enum definition
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

// Command string constants
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

// Command structs and serialization
struct GetNowResponse;
struct SetLogLevelArgs;
struct DropRecordsResponse;
struct ReadBufferArgs;
struct SetLogLevelResponse;
struct GetStatusResponse;
struct SetTimeArgs;
struct SetTimeResponse;
struct CalibrateArgs;
struct ReadBufferResponse;
struct ReadBufferResponseRecordsItem;
struct SetSamplingRateArgs;

inline std::string GetNowResponseToJson(const GetNowResponse& r);
inline std::string SetLogLevelArgsToJson(const SetLogLevelArgs& r);
inline std::string DropRecordsResponseToJson(const DropRecordsResponse& r);
inline std::string ReadBufferArgsToJson(const ReadBufferArgs& r);
inline std::string SetLogLevelResponseToJson(const SetLogLevelResponse& r);
inline std::string GetStatusResponseToJson(const GetStatusResponse& r);
inline std::string SetTimeArgsToJson(const SetTimeArgs& r);
inline std::string SetTimeResponseToJson(const SetTimeResponse& r);
inline std::string CalibrateArgsToJson(const CalibrateArgs& r);
inline std::string ReadBufferResponseToJson(const ReadBufferResponse& r);
inline std::string ReadBufferResponseRecordsItemToJson(const ReadBufferResponseRecordsItem& r);
inline std::string SetSamplingRateArgsToJson(const SetSamplingRateArgs& r);

struct GetNowResponse {
    int epoch;
    std::string local;
};

inline std::string GetNowResponseToJson(const GetNowResponse& r) {
    return std::string("{") +
        "\"epoch\":" + std::to_string(r.epoch) +
        ",\"local\":" + json_escape(r.local) + "}";
}


struct SetLogLevelArgs {
    std::string printer;
    int level;
};

SetLogLevelArgs parseSetLogLevelArgs(const std::string &args) {
    auto tokens = splitBySpace(args);
    if (tokens.size() < 2) {
        throw std::runtime_error("Invalid arguments");
    }
    SetLogLevelArgs result;
    result.printer = tokens[0];
    result.level = std::strtol(tokens[1].c_str(), nullptr, 10);
    return result;
}

inline std::string SetLogLevelArgsToJson(const SetLogLevelArgs& r) {
    return std::string("{") +
        "\"printer\":" + json_escape(r.printer) +
        ",\"level\":" + std::to_string(r.level) + "}";
}


struct DropRecordsResponse {
    std::string status;
    int offset;
    int length;
};

inline std::string DropRecordsResponseToJson(const DropRecordsResponse& r) {
    return std::string("{") +
        "\"status\":" + json_escape(r.status) +
        ",\"offset\":" + std::to_string(r.offset) +
        ",\"length\":" + std::to_string(r.length) + "}";
}


struct ReadBufferArgs {
    int offset;
    int length;
};

ReadBufferArgs parseReadBufferArgs(const std::string &args) {
    auto tokens = splitBySpace(args);
    if (tokens.size() < 2) {
        throw std::runtime_error("Invalid arguments");
    }
    ReadBufferArgs result;
    result.offset = std::strtol(tokens[0].c_str(), nullptr, 10);
    result.length = std::strtol(tokens[1].c_str(), nullptr, 10);
    return result;
}

inline std::string ReadBufferArgsToJson(const ReadBufferArgs& r) {
    return std::string("{") +
        "\"offset\":" + std::to_string(r.offset) +
        ",\"length\":" + std::to_string(r.length) + "}";
}


struct SetLogLevelResponse {
    std::string status;
    std::string printer;
    int level;
};

inline std::string SetLogLevelResponseToJson(const SetLogLevelResponse& r) {
    return std::string("{") +
        "\"status\":" + json_escape(r.status) +
        ",\"printer\":" + json_escape(r.printer) +
        ",\"level\":" + std::to_string(r.level) + "}";
}


struct GetStatusResponse {
    bool logging;
    int bufferSize;
    int rateHz;
};

inline std::string GetStatusResponseToJson(const GetStatusResponse& r) {
    return std::string("{") +
        "\"logging\":" + (r.logging ? "true" : "false") +
        ",\"bufferSize\":" + std::to_string(r.bufferSize) +
        ",\"rateHz\":" + std::to_string(r.rateHz) + "}";
}


struct SetTimeArgs {
    int epoch;
};

SetTimeArgs parseSetTimeArgs(const std::string &args) {
    auto tokens = splitBySpace(args);
    if (tokens.size() < 1) {
        throw std::runtime_error("Invalid arguments");
    }
    SetTimeArgs result;
    result.epoch = std::strtol(tokens[0].c_str(), nullptr, 10);
    return result;
}

inline std::string SetTimeArgsToJson(const SetTimeArgs& r) {
    return std::string("{") +
        "\"epoch\":" + std::to_string(r.epoch) + "}";
}


struct SetTimeResponse {
    std::string status;
    int offset;
    std::string time;
};

inline std::string SetTimeResponseToJson(const SetTimeResponse& r) {
    return std::string("{") +
        "\"status\":" + json_escape(r.status) +
        ",\"offset\":" + std::to_string(r.offset) +
        ",\"time\":" + json_escape(r.time) + "}";
}


struct CalibrateArgs {
    int low;
    int high;
    int weight;
};

CalibrateArgs parseCalibrateArgs(const std::string &args) {
    auto tokens = splitBySpace(args);
    if (tokens.size() < 3) {
        throw std::runtime_error("Invalid arguments");
    }
    CalibrateArgs result;
    result.low = std::strtol(tokens[0].c_str(), nullptr, 10);
    result.high = std::strtol(tokens[1].c_str(), nullptr, 10);
    result.weight = std::strtol(tokens[2].c_str(), nullptr, 10);
    return result;
}

inline std::string CalibrateArgsToJson(const CalibrateArgs& r) {
    return std::string("{") +
        "\"low\":" + std::to_string(r.low) +
        ",\"high\":" + std::to_string(r.high) +
        ",\"weight\":" + std::to_string(r.weight) + "}";
}


struct ReadBufferResponse {
    std::vector<ReadBufferResponseRecordsItem> records;
    int length;
};

inline std::string ReadBufferResponseToJson(const ReadBufferResponse& r) {
    return std::string("{") +
        "\"records\":" + "[" +
        [&](){
            std::string arr;
            for (size_t i = 0; i < r.records.size(); ++i) {
                if (i > 0) arr += ",";
                arr += ReadBufferResponseRecordsItemToJson(r.records[i]);
            }
            return arr;
        }() + "]" +
        ",\"length\":" + std::to_string(r.length) + "}";
}


struct ReadBufferResponseRecordsItem {
    time_t start_time;
    time_t end_time;
    float grams;
    RecordType type;
};

inline std::string ReadBufferResponseRecordsItemToJson(const ReadBufferResponseRecordsItem& r) {
    return std::string("{") +
        "\"start_time\":" + std::to_string(r.start_time) +
        ",\"end_time\":" + std::to_string(r.end_time) +
        ",\"grams\":" + std::to_string(r.grams) +
        ",\"type\":" + "\"" + RecordTypeToString(r.type) + "\"" + "}";
}


struct SetSamplingRateArgs {
    int rate;
};

SetSamplingRateArgs parseSetSamplingRateArgs(const std::string &args) {
    auto tokens = splitBySpace(args);
    if (tokens.size() < 1) {
        throw std::runtime_error("Invalid arguments");
    }
    SetSamplingRateArgs result;
    result.rate = std::strtol(tokens[0].c_str(), nullptr, 10);
    return result;
}

inline std::string SetSamplingRateArgsToJson(const SetSamplingRateArgs& r) {
    return std::string("{") +
        "\"rate\":" + std::to_string(r.rate) + "}";
}

