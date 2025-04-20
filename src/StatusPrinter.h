#pragma once
#include <Arduino.h>
#include <ctime>

class StatusPrinter
{
private:
    String label;
    String lastMessage;

    static String getTimestampMs()
    {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        struct tm timeinfo;
        localtime_r(&tv.tv_sec, &timeinfo);
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d",
                 timeinfo.tm_hour,
                 timeinfo.tm_min,
                 timeinfo.tm_sec,
                 (int)(tv.tv_usec / 1000));
        return String(buffer);
    }

public:
    int logLevel = 1; // Default to 1

    StatusPrinter(const String &printerLabel, int logLevel = 1) : label(printerLabel), lastMessage(""), logLevel(logLevel)
    {
        logLevel = logLevel;
    }

    void print(const String &message)
    {
        if (message == lastMessage)
        {
            return;
        }
        Serial.printf("[%s] <%s> %s\n",
                      getTimestampMs().c_str(),
                      label.c_str(),
                      message.c_str());
        lastMessage = message;
    }

    void printf(const char *format, ...)
    {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        print(String(buffer));
    }

    void printfLevel(int level, const char *format, ...)
    {
        if (level <= this->logLevel)
        {
            va_list args;
            va_start(args, format);
            char buf[256];
            vsnprintf(buf, sizeof(buf), format, args);
            va_end(args);
            print(buf);
        }
    }
};

static StatusPrinter rawPrinter("RAW");
static StatusPrinter eventPrinter("EVENT");
static StatusPrinter statusPrinter("STATUS");

inline StatusPrinter &getRawPrinter() { return rawPrinter; }
inline StatusPrinter &getEventPrinter() { return eventPrinter; }
inline StatusPrinter &getStatusPrinter() { return statusPrinter; }