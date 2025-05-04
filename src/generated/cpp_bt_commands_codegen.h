// AUTO-GENERATED FILE. DO NOT EDIT.

#pragma once
#include <string>
#include <vector>
#include <time.h>

enum RecordType {
    MEASUREMENT,
    SIP,
    REFILL,
};

inline const char* RecordTypeToString(RecordType t) {
    switch (t) {
        case MEASUREMENT: return "measurement";
        case SIP: return "sip";
        case REFILL: return "refill";
        default: return "unknown";
    }
}

struct Record {
    time_t start_time;
    time_t end_time;
    float grams;
    RecordType type;
};

inline std::string RecordToJson(const Record& r) {
    return std::string("{") +
        "\"start_time\":" + std::to_string(r.start_time) +
        ",\"end_time\":" + std::to_string(r.end_time) +
        ",\"grams\":" + std::to_string(r.grams) +
        ",\"type\":" + RecordTypeToString(r.type) + "}";
}

#include <string>
#include <cstdio>

inline std::string json_escape(const std::string &s)
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

inline std::string SetTimeResponseToJson(const SetTimeResponse& r) {
    return std::string("{") +
        "\"status\":" + json_escape(r.status) +
        ",\"offset\":" + std::to_string(r.offset) +
        ",\"time\":" + json_escape(r.time) + "}";
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
        ",\"type\":" + RecordTypeToString(r.type) + "}";
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


struct GetNowResponse {
    int epoch;
    std::string local;
};

inline std::string GetNowResponseToJson(const GetNowResponse& r) {
    return std::string("{") +
        "\"epoch\":" + std::to_string(r.epoch) +
        ",\"local\":" + json_escape(r.local) + "}";
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

