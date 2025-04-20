#pragma once
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <mutex>
#include "DataLogger.h"

class BtServer
{
private:
    BLEServer *pServer;
    BLECharacteristic *pTxCharacteristic;
    bool deviceConnected;
    std::mutex commandMutex;
    std::deque<std::pair<String, unsigned long>> commandQueue;

    void handleCommand(const String &cmd);
    static void notifyValue(const char *value);

public:
    BtServer();
    void setup();
    void processCommands();
    bool isConnected() const { return deviceConnected; }

    // Command handlers
    void sendBuffer();
    void setTime(time_t timestamp);
    void getStatus();
    // ... other command handlers ...
};

// Singleton instance
extern BtServer &getBtServer();
