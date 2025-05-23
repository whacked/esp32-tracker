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

    // Helper method to serialize a single record
    String recordToJson(const Record &r) const
    {
        return "{\"start_time\":" + String(r.start_time) +
               ",\"end_time\":" + String(r.end_time) +
               ",\"grams\":" + String(r.grams, 2) +
               ",\"type\":\"" + String(r.type == SIP ? "sip" : r.type == REFILL ? "refill"
                                                                                : "measurement") +
               "\"}";
    }

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

    // Get a paginated subset of records as JSON
    String getBufferJsonPaginated(size_t offset, size_t length) const
    {
        String json = "{";

        // Add metadata
        json += "\"total\":" + String(recordBuffer.size()) + ",";
        json += "\"offset\":" + String(offset) + ",";

        // Calculate actual length (handle bounds)
        size_t available = recordBuffer.size() > offset ? recordBuffer.size() - offset : 0;
        size_t actualLength = min(length, available);
        json += "\"length\":" + String(actualLength) + ",";

        // Add records array
        json += "\"records\":[";

        for (size_t i = 0; i < actualLength; i++)
        {
            if (i > 0)
                json += ",";
            json += recordToJson(recordBuffer[offset + i]);
        }

        json += "]}";
        return json;
    }

    // Simplified version that gets all records
    String getBufferJson() const
    {
        return getBufferJsonPaginated(0, recordBuffer.size());
    }

    // Drop a range of records from the buffer
    bool dropRecords(size_t offset, size_t length)
    {
        if (offset >= recordBuffer.size())
        {
            return false;
        }

        // Calculate actual number of records we can drop
        size_t available = recordBuffer.size() - offset;
        size_t actualLength = min(length, available);

        // Create iterators for the range to remove
        auto start = recordBuffer.begin() + offset;
        auto end = start + actualLength;

        // Remove the records
        recordBuffer.erase(start, end);
        return true;
    }
};

// Global instance
static DataLogger dataLogger;
inline DataLogger &getDataLogger() { return dataLogger; }
