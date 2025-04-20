#pragma once
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <mutex>
#include <deque>
#include "DataLogger.h"

// BLE UUIDs
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

class BtServer
{
private:
    BLEServer *pServer;
    BLECharacteristic *pTxCharacteristic;
    bool deviceConnected = false;
    std::mutex commandMutex;
    String incomingBuffer;
    int &samplingRateHz;

    struct QueuedCommand
    {
        String command;
        unsigned long timestamp;
        QueuedCommand(const String &cmd) : command(cmd), timestamp(millis()) {}
    };
    std::deque<QueuedCommand> commandQueue;

    class ServerCallbacks : public BLEServerCallbacks
    {
        BtServer &server;

    public:
        ServerCallbacks(BtServer &s) : server(s) {}
        void onConnect(BLEServer *pServer) override { server.deviceConnected = true; }
        void onDisconnect(BLEServer *pServer) override
        {
            server.deviceConnected = false;
            delay(100);                         // brief delay helps stack clean up
            pServer->getAdvertising()->start(); // RESTART ADVERTISING
            Serial.println("Disconnected, advertising restarted");
        }
    };

    class CharCallbacks : public BLECharacteristicCallbacks
    {
        BtServer &server;

    public:
        CharCallbacks(BtServer &s) : server(s) {}
        void onWrite(BLECharacteristic *pCharacteristic) override
        {
            std::string rxValue = pCharacteristic->getValue();
            if (!rxValue.empty())
            {
                for (char c : rxValue)
                {
                    if (c == '\n')
                    {
                        server.commandMutex.lock();
                        server.commandQueue.push_back(QueuedCommand(server.incomingBuffer));
                        server.commandMutex.unlock();
                        server.incomingBuffer = "";
                    }
                    else
                    {
                        server.incomingBuffer += c;
                    }
                }
            }
        }
    };

    void notify(const char *value)
    {
        if (deviceConnected)
        {
            pTxCharacteristic->setValue(value);
            pTxCharacteristic->notify();
        }
    }

    void handleCommand(const String &cmd)
    {
        Serial.print("Received command: ");
        Serial.println(cmd);

        int spaceIdx = cmd.indexOf(' ');
        String command = (spaceIdx == -1) ? cmd : cmd.substring(0, spaceIdx);
        String args = (spaceIdx == -1) ? "" : cmd.substring(spaceIdx + 1);

        if (command == "getVersion")
        {
            notify("1.0.0");
        }
        else if (command == "setTime")
        {
            time_t targetTime = args.toInt();
            if (targetTime > 0)
            {
                getDataLogger().setTimeOffset(targetTime - time(nullptr));
                String response = "{\"status\":\"ok\",\"offset\":" +
                                  String(getDataLogger().getTimeOffset()) +
                                  ",\"time\":\"" + getDataLogger().getTimestamp() + "\"}";
                notify(response.c_str());
            }
            else
            {
                notify("{\"status\":\"error\",\"message\":\"Invalid timestamp\"}");
            }
        }
        else if (command == "clearBuffer")
        {
            getDataLogger().clearBuffer();
            Serial.println("Cleared buffer");
        }
        else if (command == "readBuffer")
        {
            Serial.println("Sending buffer...");
            notify(getDataLogger().getBufferJson().c_str());
        }
        else if (command == "startLogging")
        {
            getDataLogger().setLoggingEnabled(true);
            Serial.println("Logging enabled");
        }
        else if (command == "stopLogging")
        {
            getDataLogger().setLoggingEnabled(false);
            Serial.println("Logging disabled");
        }
        else if (command == "getNow")
        {
            String response = "{\"epoch\":" + String(getDataLogger().getCorrectedTime()) +
                              ",\"local\":\"" + getDataLogger().getTimestamp() + "\"}";
            notify(response.c_str());
        }
        else if (command == "getStatus")
        {
            String status = "{";
            status += "\"logging\":" + String(getDataLogger().isLoggingEnabled() ? "true" : "false");
            status += ",\"bufferSize\":" + String(getDataLogger().getBufferSize());
            status += ",\"rateHz\":" + String(samplingRateHz);
            status += "}";
            notify(status.c_str());
        }
        else if (command == "setSamplingRate")
        {
            int rate = args.toInt();
            if (rate > 0)
            {
                samplingRateHz = rate;
                Serial.print("Sampling rate set to ");
                Serial.println(rate);
            }
        }
        else if (command == "calibrate")
        {
            int a, b, c;
            int parsed = sscanf(args.c_str(), "%d %d %d", &a, &b, &c);
            if (parsed == 3)
            {
                Serial.printf("Calibration set: low=%d, high=%d, weight=%d\n", a, b, c);
                // TODO: store these and use for grams conversion
            }
            else
            {
                Serial.println("Invalid calibration args");
            }
        }
        else if (command == "reset")
        {
            Serial.println("Resetting...");
            ESP.restart();
        }
        else
        {
            notify("Unknown command");
        }
    }

public:
    BtServer(int &samplingRate) : samplingRateHz(samplingRate) {}

    void setup()
    {
        BLEDevice::init("ESP32-Scale");
        pServer = BLEDevice::createServer();
        pServer->setCallbacks(new ServerCallbacks(*this));

        BLEService *pService = pServer->createService(SERVICE_UUID);

        pTxCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_TX,
            BLECharacteristic::PROPERTY_NOTIFY);
        pTxCharacteristic->addDescriptor(new BLE2902());

        BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_RX,
            BLECharacteristic::PROPERTY_WRITE);
        pRxCharacteristic->setCallbacks(new CharCallbacks(*this));

        pService->start();
        pServer->getAdvertising()->start();
        Serial.println("BLE UART started, waiting for connections...");
    }

    void processCommands()
    {
        while (!commandQueue.empty())
        {
            commandMutex.lock();
            QueuedCommand cmd = commandQueue.front();
            commandQueue.pop_front();
            commandMutex.unlock();

            if (millis() - cmd.timestamp > 1000)
            {
                Serial.println("Warning: Dropped old command");
                continue;
            }

            handleCommand(cmd.command);
            vTaskDelay(1); // Yield to BLE stack
        }
    }

    bool isConnected() const { return deviceConnected; }
};

// Global instance
static BtServer *btServer = nullptr;
inline BtServer &getBtServer() { return *btServer; }
