#pragma once
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <mutex>
#include <deque>
#include <memory>
#include "DataLogger.h"
#include "StatusPrinter.h"
#include "CommandHandler.h"
#include "generated/cpp_bt_commands_autogen.h"
#include <build_metadata.h>

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
    CommandHandler *commandHandler;

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

        auto [command, args] = parseCommand(cmd.c_str());
        std::string response = commandHandler->handleCommand(command, args);
        notify(response.c_str());
    }

public:
    BtServer(int &samplingRate)
        : samplingRateHz(samplingRate),
          commandHandler(new DataLoggerCommandHandler(getDataLogger(), samplingRate)) {}

    ~BtServer()
    {
        delete commandHandler;
    }

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
