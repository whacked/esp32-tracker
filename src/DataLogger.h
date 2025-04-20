#pragma once
#include <deque>
#include <time.h>
#include <Arduino.h>

enum RecordType
{
    MEASUREMENT,
    SIP,
    REFILL
};

struct Record
{
    time_t start_time;
    time_t end_time; // 0 if not stabilized
    float grams;
    RecordType type;
};

class DataLogger
{
private:
    std::deque<Record> recordBuffer;
    bool loggingEnabled;
    time_t timeOffset; // Moved from main.cpp

public:
    DataLogger() : loggingEnabled(true), timeOffset(0) {}

    // Core buffer operations
    void addRecord(time_t start_time, time_t end_time, float grams, RecordType type)
    {
        if (!loggingEnabled)
            return;
        recordBuffer.push_back({start_time, end_time, grams, type});
    }

    void clearBuffer()
    {
        recordBuffer.clear();
    }

    // Logging control
    bool isLoggingEnabled() const { return loggingEnabled; }
    void setLoggingEnabled(bool enabled) { loggingEnabled = enabled; }

    // Buffer access
    const std::deque<Record> &getBuffer() const { return recordBuffer; }
    size_t getBufferSize() const { return recordBuffer.size(); }

    // Time management (moved from main.cpp)
    void setTimeOffset(time_t offset) { timeOffset = offset; }
    time_t getTimeOffset() const { return timeOffset; }

    time_t getCorrectedTime() const
    {
        return time(nullptr) + timeOffset;
    }

    String getTimestamp() const
    {
        time_t now = getCorrectedTime();
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char timestamp[25];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S%z", &timeinfo);
        return String(timestamp);
    }

    // Specialized record additions (previously scattered in main.cpp)
    void addMeasurement(float grams, bool stable)
    {
        if (!loggingEnabled)
            return;

        time_t now = getCorrectedTime();
        if (stable)
        {
            // If stable, update the last record's end time if it exists and matches
            if (!recordBuffer.empty() &&
                recordBuffer.back().end_time == 0 &&
                abs(recordBuffer.back().grams - grams) < 1.0)
            { // TODO: Make tolerance configurable
                recordBuffer.back().end_time = now;
            }
            else
            {
                // New stable reading
                addRecord(now, now, grams, MEASUREMENT);
            }
        }
        else
        {
            // Unstable reading, just record the start time
            addRecord(now, 0, grams, MEASUREMENT);
        }
    }

    void addSip(time_t start_time, float amount)
    {
        if (!loggingEnabled)
            return;
        addRecord(start_time, getCorrectedTime(), amount, SIP);
    }

    void addRefill(time_t start_time, float amount)
    {
        if (!loggingEnabled)
            return;
        addRecord(start_time, getCorrectedTime(), amount, REFILL);
    }

    // Utility function for JSON serialization (used by BtServer)
    String getBufferJson() const
    {
        String json = "[";
        for (size_t i = 0; i < recordBuffer.size(); ++i)
        {
            const auto &r = recordBuffer[i];
            json += "{\"start_time\":" + String(r.start_time) +
                    ",\"end_time\":" + String(r.end_time) +
                    ",\"grams\":" + String(r.grams, 2) +
                    ",\"type\":\"" + String(r.type == SIP ? "sip" : r.type == REFILL ? "refill"
                                                                                     : "measurement") +
                    "\"}";
            if (i < recordBuffer.size() - 1)
                json += ",";
        }
        json += "]";
        return json;
    }
};

// Global instance
static DataLogger dataLogger;
inline DataLogger &getDataLogger() { return dataLogger; }
