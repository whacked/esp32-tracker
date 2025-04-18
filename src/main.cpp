#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <deque>
#include <mutex>
#include "HX711.h"

HX711 scale;

// Use the pins you wired
#define DT 21
#define SCK 22

// Calibration values based on your measurements
#define CALIBRATION_LOW -400    // reading at no load  -331 is the average
// #define HOME_SET 1
#ifdef HOME_SET
  #define CALIBRATION_HIGH -473500 // reading at 950g
  #define WEIGHT_HIGH 550         // actual weight in grams
#else // OFFICE_SET
  #define CALIBRATION_HIGH 998000 // reading at 950g
  #define WEIGHT_HIGH 950         // actual weight in grams
#endif

// BLE UUIDs
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_RX   "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_TX   "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// timings
#define SAMPLING_RATE_MS 25

// Stabilization settings
#define STABILITY_COUNT 3
#define STABILITY_TOLERANCE 1.0  // in grams
#define STABILITY_WINDOW 5       // window for stability check

// Primary moving average (existing)
#define WINDOW_SIZE 10
float readings[WINDOW_SIZE];
int readIndex = 0;
float total = 0;
float average = 0;

// Secondary window for stability detection
float stableReadings[STABILITY_WINDOW];
int stableIndex = 0;
bool isStable = false;

// Add near the top with other globals
time_t timeOffset = 0;  // Offset in seconds from ESP32's boot time

// Update enum to better reflect states
enum EventState {
  STABLE,         // Weight is stable, waiting for changes
  DROP_DETECTED,  // Weight dropped, waiting for restabilization
};

// Add with other globals
EventState eventState = STABLE;
float baselineWeight = 0;
float eventStartWeight = 0;
time_t eventStartTime = 0;

// Update Record struct to include event type
enum RecordType {
  MEASUREMENT,
  SIP,
  REFILL
};

struct Record {
  time_t start_time;
  time_t end_time;    // 0 if not stabilized
  float grams;
  RecordType type;    // Added field
};

std::deque<Record> recordBuffer;
bool loggingEnabled = true;
int samplingRateHz = 20;  // default from original delay(25)

// Update getTimestamp function
String getTimestamp() {
  time_t now = time(nullptr) + timeOffset;  // Add offset to current time
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char timestamp[25];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S%z", &timeinfo);
  return String(timestamp);
}

// Add helper function to get corrected time
time_t getCorrectedTime() {
  return time(nullptr) + timeOffset;
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

void handleCommand(const String &cmd) {
  Serial.print("Received command: ");
  Serial.println(cmd);

  int spaceIdx = cmd.indexOf(' ');
  String command = (spaceIdx == -1) ? cmd : cmd.substring(0, spaceIdx);
  String args = (spaceIdx == -1) ? ""  : cmd.substring(spaceIdx + 1);

  if (command == "getVersion") {
    String version = "1.0.0";
    if (deviceConnected) {
      pTxCharacteristic->setValue(version.c_str());
      pTxCharacteristic->notify();
    }
  } else if (command == "setTime") {
    // Example: 1234567890 (epoch timestamp)
    time_t targetTime = args.toInt();
    if (targetTime > 0) {
      timeOffset = targetTime - time(nullptr);
      String response = "{\"status\":\"ok\",\"offset\":" + String(timeOffset) + 
                       ",\"time\":\"" + getTimestamp() + "\"}";
      if (deviceConnected) {
        pTxCharacteristic->setValue(response.c_str());
        pTxCharacteristic->notify();
      }
    } else {
      if (deviceConnected) {
        pTxCharacteristic->setValue("{\"status\":\"error\",\"message\":\"Invalid timestamp\"}");
        pTxCharacteristic->notify();
      }
    }
  } else if (command == "clearBuffer") {
    recordBuffer.clear();
    Serial.println("Cleared buffer");

  } else if (command == "readBuffer") {
    Serial.println("Sending buffer...");
    String json = "[";
    for (size_t i = 0; i < recordBuffer.size(); ++i) {
      const auto &r = recordBuffer[i];
      json += "{\"start_time\":" + String(r.start_time) + 
              ",\"end_time\":" + String(r.end_time) + 
              ",\"grams\":" + String(r.grams, 2) + 
              ",\"type\":\"" + String(r.type == SIP ? "sip" : 
                                    r.type == REFILL ? "refill" : 
                                    "measurement") + "\"}";
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
    time_t now = getCorrectedTime();
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

// Command queue structure to hold both command and its timestamp
struct QueuedCommand {
    String command;
    unsigned long timestamp;
    QueuedCommand(const String& cmd) : command(cmd), timestamp(millis()) {}
};

std::deque<QueuedCommand> commandQueue;
std::mutex commandMutex;

class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        std::string rxValue = pCharacteristic->getValue();
        if (!rxValue.empty()) {
            for (char c : rxValue) {
                if (c == '\n') {
                    commandMutex.lock();
                    commandQueue.push_back(QueuedCommand(incomingBuffer));
                    commandMutex.unlock();
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

int STABILITY_CHECK_PRINT_COUNT = 0;
bool checkStability(float newValue) {

  if (STABILITY_CHECK_PRINT_COUNT++ % 10 == 0) {
    // Add debug prints to see what values we're checking
    Serial.printf("Stability check - new value: %.1f, window: ", newValue);
    for (int i = 0; i < STABILITY_WINDOW; i++) {
      Serial.printf("%.1f ", stableReadings[i]);
    }
    Serial.println();
  }

  // Update stability window
  stableReadings[stableIndex] = newValue;
  stableIndex = (stableIndex + 1) % STABILITY_WINDOW;
  
  // Need minimum number of readings
  if (stableIndex < STABILITY_COUNT - 1) {
    return false;
  }
  
  // Check if last STABILITY_COUNT readings are within tolerance
  float minVal = newValue;
  float maxVal = newValue;
  
  // Only check the last STABILITY_COUNT values
  for (int i = 0; i < STABILITY_COUNT; i++) {
    int idx = (stableIndex - 1 - i + STABILITY_WINDOW) % STABILITY_WINDOW;
    float val = stableReadings[idx];
    minVal = min(minVal, val);
    maxVal = max(maxVal, val);
  }
  
  bool stable = (maxVal - minVal) <= STABILITY_TOLERANCE;
  Serial.printf("Stability range: %.1f to %.1f (diff: %.1f) -> %s\n", 
    minVal, maxVal, maxVal - minVal, stable ? "stable" : "unstable");
  return stable;
}

void printStatus(float raw, float calibrated, float grams, bool stable, const char* event = nullptr) {
  char timestamp[25];
  time_t now = getCorrectedTime();
  strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&now));
  
  Serial.print("Raw: ");
  Serial.print(raw);
  Serial.print(" | Calibrated: ");
  Serial.print(calibrated, 2);
  Serial.print(" | Grams: ");
  Serial.print(grams, 1);
  Serial.print(" | Status: ");
  Serial.println(stable ? "STABLE" : "settling");
  if (event) {
    Serial.printf("[%s] %s: %.1fg\n", timestamp, event, grams);
  } else {
    Serial.printf("[%s] Weight %s: %.1fg\n", 
      timestamp,
      stable ? "stabilized" : "changing",
      grams
    );
  }
}

void addToBuffer(float grams, bool stable = false) {
  if (loggingEnabled) {
    if (stable) {
      // If stable, update the last record's end time if it exists and matches
      if (!recordBuffer.empty() && 
          recordBuffer.back().end_time == 0 && 
          abs(recordBuffer.back().grams - grams) < STABILITY_TOLERANCE) {
        recordBuffer.back().end_time = getCorrectedTime();
      } else {
        // New stable reading
        recordBuffer.push_back({getCorrectedTime(), getCorrectedTime(), grams, MEASUREMENT});
      }
    } else {
      // Unstable reading, just record the start time
      recordBuffer.push_back({getCorrectedTime(), 0, grams, MEASUREMENT});
    }
  }
}

void loop()
{
  // Process any pending commands first
  while (!commandQueue.empty()) {
    commandMutex.lock();
    QueuedCommand cmd = commandQueue.front();
    commandQueue.pop_front();
    commandMutex.unlock();

    // If command is too old, skip it (optional, for debugging)
    if (millis() - cmd.timestamp > 1000) {
        Serial.println("Warning: Dropped old command");
        continue;
    }

    handleCommand(cmd.command);
    
    // Force a small delay after each command to ensure BLE stack processes the notification
    vTaskDelay(1); // Yield to BLE stack
  }

  // Calculate weight with moving average
  total = total - readings[readIndex];
  readings[readIndex] = scale.get_units();
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % WINDOW_SIZE;
  average = total / WINDOW_SIZE;
  
  float grams = map(average, CALIBRATION_LOW, CALIBRATION_HIGH, 0, WEIGHT_HIGH);
  grams = max(0.0f, grams);
  
  // Round very small values to 0 to prevent noise
  if (abs(grams) < 0.1) {
    grams = 0;
  }
  
  // Check stability on grams value after rounding
  isStable = checkStability(grams);
  
  // State machine logic
  static bool wasStable = false;
  char timestamp[25];
  time_t now = getCorrectedTime();
  strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&now));

  // printStatus(scale.read(), average, grams, isStable);

  switch (eventState) {
    case STABLE:
      if (wasStable && !isStable) {
        // Transition: STABLE → UNSTABLE
        float delta = grams - baselineWeight;
        if (delta < -STABILITY_TOLERANCE) {
          // Significant drop detected
          eventStartTime = getCorrectedTime();
          eventStartWeight = baselineWeight;  // Store original weight
          eventState = DROP_DETECTED;
          Serial.printf("[%s] Drop detected: %.1fg → %.1fg\n", timestamp, baselineWeight, grams);
        }
      } else if (isStable) {
        // Update baseline when stable
        if (!wasStable) {
          baselineWeight = grams;
          Serial.printf("[%s] New baseline: %.1fg\n", timestamp, grams);
        }
      }
      break;

    case DROP_DETECTED:
      if (!wasStable && isStable) {
        // Weight has restabilized after drop
        float delta = grams - eventStartWeight;
        time_t endTime = getCorrectedTime();
        
        if (delta < -STABILITY_TOLERANCE) {
          // SIP: weight is lower than original
          float sipAmount = eventStartWeight - grams;
          Serial.printf("[%s] Sip detected: %.1fg (%.1fg → %.1fg)\n", 
            timestamp, sipAmount, eventStartWeight, grams);
          if (loggingEnabled) {
            recordBuffer.push_back({eventStartTime, endTime, sipAmount, SIP});
          }
          baselineWeight = grams;  // Update baseline to new weight
        } else if (delta > STABILITY_TOLERANCE) {
          // REFILL: weight is higher than original
          Serial.printf("[%s] Refill detected: %.1fg → %.1fg\n", 
            timestamp, eventStartWeight, grams);
          if (loggingEnabled) {
            recordBuffer.push_back({eventStartTime, endTime, grams - eventStartWeight, REFILL});
          }
          baselineWeight = grams;  // Update baseline to new weight
        } else {
          // Weight returned to original ±tolerance
          Serial.printf("[%s] False alarm, weight returned to baseline\n", timestamp);
          baselineWeight = grams;
        }
        eventState = STABLE;
      }
      break;
  }

  // Update stability tracking
  wasStable = isStable;

  // Use task delay instead of blocking delay
  vTaskDelay(pdMS_TO_TICKS(SAMPLING_RATE_MS));
}
