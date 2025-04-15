#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <deque>
#include "HX711.h"

HX711 scale;

// Use the pins you wired
#define DT 21
#define SCK 22

// Calibration values based on your measurements
#define CALIBRATION_LOW -400    // reading at no load  -331 is the average
#define CALIBRATION_HIGH 998000 // reading at 950g
#define WEIGHT_HIGH 950         // actual weight in grams

// BLE UUIDs
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_RX   "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_TX   "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// Rolling average window
#define WINDOW_SIZE 10
float readings[WINDOW_SIZE];
int readIndex = 0;
float total = 0;
float average = 0;

String getTimestamp() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char timestamp[25];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S%z", &timeinfo);
  return String(timestamp);
}


// BLE characteristics
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;

// BLE callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

struct Record {
  time_t timestamp;
  float grams;
};

std::deque<Record> recordBuffer;
bool loggingEnabled = true;
int samplingRateHz = 20;  // default from original delay(25)

void handleCommand(const String &cmd) {
  Serial.print("Received command: ");
  Serial.println(cmd);

  int spaceIdx = cmd.indexOf(' ');
  String command = (spaceIdx == -1) ? cmd : cmd.substring(0, spaceIdx);
  String args = (spaceIdx == -1) ? ""  : cmd.substring(spaceIdx + 1);

  if (command == "setTime") {
    // Example: 2025-04-15 17:01:00+0800
    Serial.print("Setting time to: ");
    Serial.println(args);
    // TODO: parse and convert to epoch offset

  } else if (command == "clearBuffer") {
    recordBuffer.clear();
    Serial.println("Cleared buffer");

  } else if (command == "readBuffer") {
    Serial.println("Sending buffer...");
    String json = "[";
    for (size_t i = 0; i < recordBuffer.size(); ++i) {
      const auto &r = recordBuffer[i];
      json += "{\"timestamp\":" + String(r.timestamp) + ",\"grams\":" + String(r.grams, 2) + "}";
      if (i < recordBuffer.size() - 1) json += ",";
    }
    json += "]";
    if (deviceConnected) {
      pTxCharacteristic->setValue(json.c_str());
      pTxCharacteristic->notify();
    }

  } else if (command == "startLogging") {
    loggingEnabled = true;
    Serial.println("Logging enabled");

  } else if (command == "stopLogging") {
    loggingEnabled = false;
    Serial.println("Logging disabled");

  } else if (command == "getNow") {
    time_t now = time(nullptr);
    String response = "{\"epoch\":" + String(now) + ",\"local\":\"" + getTimestamp() + "\"}";
    if (deviceConnected) {
      pTxCharacteristic->setValue(response.c_str());
      pTxCharacteristic->notify();
    }

  } else if (command == "getStatus") {
    String status = "{";
    status += "\"logging\":" + String(loggingEnabled ? "true" : "false");
    status += ",\"bufferSize\":" + String(recordBuffer.size());
    status += ",\"rateHz\":" + String(samplingRateHz);
    status += "}";
    if (deviceConnected) {
      pTxCharacteristic->setValue(status.c_str());
      pTxCharacteristic->notify();
    }

  } else if (command == "setSamplingRate") {
    int rate = args.toInt();
    if (rate > 0) {
      samplingRateHz = rate;
      Serial.print("Sampling rate set to ");
      Serial.println(rate);
    }

  } else if (command == "calibrate") {
    // Example: -400 998000 950
    int a, b, c;
    int parsed = sscanf(args.c_str(), "%d %d %d", &a, &b, &c);
    if (parsed == 3) {
      Serial.printf("Calibration set: low=%d, high=%d, weight=%d\n", a, b, c);
      // TODO: store these and use for grams conversion
    } else {
      Serial.println("Invalid calibration args");
    }

  } else if (command == "reset") {
    Serial.println("Resetting...");
    ESP.restart();

  } else {
    if (deviceConnected) {
      pTxCharacteristic->setValue("Unknown command");
      pTxCharacteristic->notify();
    }
  }
}

// RX write handler
// Command buffer
String incomingBuffer = "";
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    if (!rxValue.empty()) {
      for (char c : rxValue) {
        if (c == '\n') {
          handleCommand(incomingBuffer);
          incomingBuffer = "";
        } else {
          incomingBuffer += c;
        }
      }
    }
  }
};

void setupBLE() {
  BLEDevice::init("ESP32-Scale");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("BLE UART started, waiting for connections...");
}

void setup()
{
  Serial.begin(115200);
  pinMode(2, OUTPUT);

  scale.begin(DT, SCK);

  Serial.println("Taring...");
  scale.set_scale();
  scale.tare();

  // Initialize readings array
  for (int i = 0; i < WINDOW_SIZE; i++) {
    readings[i] = 0;
  }

  // startup indicator
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(2, HIGH);
    delay(100);
    digitalWrite(2, LOW);
    delay(100);
  }

  setupBLE();
  Serial.println("Ready!");
}

void loop()
{
  // Subtract the last reading
  total = total - readings[readIndex];
  // Read from the sensor
  readings[readIndex] = scale.get_units();
  // Add the reading to the total
  total = total + readings[readIndex];
  // Advance to the next position in the array
  readIndex = (readIndex + 1) % WINDOW_SIZE;

  // Calculate the average
  average = total / WINDOW_SIZE;
  
  // Convert to grams using map
  float grams = map(average, CALIBRATION_LOW, CALIBRATION_HIGH, 0, WEIGHT_HIGH);
  // Clamp negative values to 0
  grams = max(0.0f, grams);

  Serial.print("Time: ");
  Serial.print(getTimestamp());
  Serial.print(" | ");

  Serial.print("Raw: ");
  Serial.print(scale.read());
  Serial.print(" | Calibrated: ");
  Serial.print(average, 2);
  Serial.print(" | Grams: ");
  Serial.println(grams, 1);
  
  delay(25); // Increased sampling rate (20 samples per second)
}
