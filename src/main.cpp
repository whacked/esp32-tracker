#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <deque>
#include <mutex>
#include "HX711.h"
#include "StatusPrinter.h"
#include "DataLogger.h"

HX711 scale;

// Use the pins you wired
#define DT 21
#define SCK 22

// Calibration values based on your measurements
#ifdef HOME_SET
#define CALIBRATION_AT_NO_LOAD 46     // reading at no load  -331 is the average
#define CALIBRATION_AT_LOAD_1 -299539 // reading at 950g
#define WEIGHT_AT_LOAD_1 285          // actual weight in grams
#else                                 // OFFICE_SET
#define CALIBRATION_AT_NO_LOAD -400   // reading at no load  -331 is the average
#define CALIBRATION_AT_LOAD_1 998000  // reading at 950g
#define WEIGHT_AT_LOAD_1 950          // actual weight in grams
#endif

// BLE UUIDs
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// timings
#define SAMPLING_RATE_MS 25

// Stabilization settings
#define STABILITY_COUNT 3
#define STABILITY_TOLERANCE 1.0 // in grams
#define STABILITY_WINDOW 5      // window for stability check

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
time_t timeOffset = 0; // Offset in seconds from ESP32's boot time

// Add these with other #defines
#define DELTA_THRESHOLD 1.0            // Threshold for detecting rises/drops
#define CHANGE_DETECTION_THRESHOLD 2.0 // Threshold for confirming sips/refills
#define DIRECTION_WINDOW 3             // Number of samples to average for direction detection

// Update enum to include new state
enum EventState
{
  STABLE,          // Weight is stable, waiting for changes
  DROP_DETECTED,   // Initial drop detected, waiting for stabilization
  DROP_STABILIZED, // Drop has stabilized, waiting for increase
  RISE_DETECTED,   // Rise detected, waiting for stabilization
  RISE_STABILIZED  // Rise has stabilized, evaluate event type
};

// Add with other globals
EventState eventState = STABLE;
float baselineWeight = 0;
float eventStartWeight = 0;
time_t eventStartTime = 0;

int samplingRateHz = 20; // default from original delay(25)

// Add these helper variables
float preDropWeight = 0;
float postDropWeight = 0;
float directionBuffer[DIRECTION_WINDOW] = {0};
int directionIndex = 0;

// Helper function to detect direction of change
float getAverageDirection(float currentValue, float baselineValue)
{
  float currentDelta = currentValue - baselineValue;
  directionBuffer[directionIndex] = currentDelta;
  directionIndex = (directionIndex + 1) % DIRECTION_WINDOW;

  float sum = 0;
  for (int i = 0; i < DIRECTION_WINDOW; i++)
  {
    sum += directionBuffer[i];
  }
  return sum / DIRECTION_WINDOW;
}

// Update getTimestamp function
String getTimestamp()
{
  time_t now = time(nullptr) + timeOffset; // Add offset to current time
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char timestamp[25];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S%z", &timeinfo);
  return String(timestamp);
}

// Add helper function to get corrected time
time_t getCorrectedTime()
{
  return time(nullptr) + timeOffset;
}

// BLE characteristics
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;

// BLE callbacks
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
  }

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
  }
};

void handleCommand(const String &cmd)
{
  Serial.print("Received command: ");
  Serial.println(cmd);

  int spaceIdx = cmd.indexOf(' ');
  String command = (spaceIdx == -1) ? cmd : cmd.substring(0, spaceIdx);
  String args = (spaceIdx == -1) ? "" : cmd.substring(spaceIdx + 1);

  if (command == "getVersion")
  {
    String version = "1.0.0";
    if (deviceConnected)
    {
      pTxCharacteristic->setValue(version.c_str());
      pTxCharacteristic->notify();
    }
  }
  else if (command == "setTime")
  {
    // Example: 1234567890 (epoch timestamp)
    time_t targetTime = args.toInt();
    if (targetTime > 0)
    {
      getDataLogger().setTimeOffset(targetTime - time(nullptr));
      String response = "{\"status\":\"ok\",\"offset\":" + String(getDataLogger().getTimeOffset()) +
                        ",\"time\":\"" + getDataLogger().getTimestamp() + "\"}";
      if (deviceConnected)
      {
        pTxCharacteristic->setValue(response.c_str());
        pTxCharacteristic->notify();
      }
    }
    else
    {
      if (deviceConnected)
      {
        pTxCharacteristic->setValue("{\"status\":\"error\",\"message\":\"Invalid timestamp\"}");
        pTxCharacteristic->notify();
      }
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
    String json = getDataLogger().getBufferJson();
    if (deviceConnected)
    {
      pTxCharacteristic->setValue(json.c_str());
      pTxCharacteristic->notify();
    }
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
    time_t now = getCorrectedTime();
    String response = "{\"epoch\":" + String(now) + ",\"local\":\"" + getTimestamp() + "\"}";
    if (deviceConnected)
    {
      pTxCharacteristic->setValue(response.c_str());
      pTxCharacteristic->notify();
    }
  }
  else if (command == "getStatus")
  {
    String status = "{";
    status += "\"logging\":" + String(getDataLogger().isLoggingEnabled() ? "true" : "false");
    status += ",\"bufferSize\":" + String(getDataLogger().getBufferSize());
    status += ",\"rateHz\":" + String(samplingRateHz);
    status += "}";
    if (deviceConnected)
    {
      pTxCharacteristic->setValue(status.c_str());
      pTxCharacteristic->notify();
    }
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
    // Example: -400 998000 950
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
    if (deviceConnected)
    {
      pTxCharacteristic->setValue("Unknown command");
      pTxCharacteristic->notify();
    }
  }
}

// RX write handler
// Command buffer
String incomingBuffer = "";

// Command queue structure to hold both command and its timestamp
struct QueuedCommand
{
  String command;
  unsigned long timestamp;
  QueuedCommand(const String &cmd) : command(cmd), timestamp(millis()) {}
};

std::deque<QueuedCommand> commandQueue;
std::mutex commandMutex;

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    std::string rxValue = pCharacteristic->getValue();
    if (!rxValue.empty())
    {
      for (char c : rxValue)
      {
        if (c == '\n')
        {
          commandMutex.lock();
          commandQueue.push_back(QueuedCommand(incomingBuffer));
          commandMutex.unlock();
          incomingBuffer = "";
        }
        else
        {
          incomingBuffer += c;
        }
      }
    }
  }
};

// Add with other globals
StatusPrinter rawPrinter("RAW");
StatusPrinter eventPrinter("EVENT");
StatusPrinter statusPrinter("STATUS");

void setupBLE()
{
  BLEDevice::init("ESP32-Scale");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_TX,
      BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_RX,
      BLECharacteristic::PROPERTY_WRITE);
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
  for (int i = 0; i < WINDOW_SIZE; i++)
  {
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

bool checkStability(float newValue)
{
  // Update stability window
  stableReadings[stableIndex] = newValue;
  stableIndex = (stableIndex + 1) % STABILITY_WINDOW;

  // Wait for window to fill up
  static bool windowFilled = false;
  if (!windowFilled && stableIndex == 0)
  {
    windowFilled = true;
  }
  if (!windowFilled)
  {
    return false;
  }

  // Check all values in window
  float minVal = stableReadings[0];
  float maxVal = stableReadings[0];

  for (int i = 1; i < STABILITY_WINDOW; i++)
  {
    float val = stableReadings[i];
    minVal = min(minVal, val);
    maxVal = max(maxVal, val);
  }

  float diff = maxVal - minVal;
  bool stable = diff <= STABILITY_TOLERANCE;

  statusPrinter.printf("value=%.1f window=[%.1f %.1f] diff=%.1f -> %s",
                       newValue, minVal, maxVal, diff, stable ? "stable" : "unstable");

  return stable;
}

void loop()
{
  // Process any pending commands first
  while (!commandQueue.empty())
  {
    commandMutex.lock();
    QueuedCommand cmd = commandQueue.front();
    commandQueue.pop_front();
    commandMutex.unlock();

    // If command is too old, skip it (optional, for debugging)
    if (millis() - cmd.timestamp > 1000)
    {
      Serial.println("Warning: Dropped old command");
      continue;
    }

    handleCommand(cmd.command);

    // Force a small delay after each command to ensure BLE stack processes the notification
    vTaskDelay(1); // Yield to BLE stack
  }

  float rawValue = scale.get_units();
  rawPrinter.printf("raw=%.1f", rawValue);

  // Calculate weight with moving average
  total = total - readings[readIndex];
  readings[readIndex] = rawValue;
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % WINDOW_SIZE;
  average = total / WINDOW_SIZE;

  // statusPrinter.printf("average=%.1f", average);

  float grams = map(average, CALIBRATION_AT_NO_LOAD, CALIBRATION_AT_LOAD_1, 0, WEIGHT_AT_LOAD_1);
  grams = max(0.0f, grams);

  // Round very small values to 0 to prevent noise
  if (abs(grams) < 0.1)
  {
    grams = 0;
  }

  // Check stability on grams value after rounding
  isStable = checkStability(grams);

  // State machine logic
  static bool wasStable = false;

  eventPrinter.printf("grams=%.1f stable=%s state=%d",
                      grams, isStable ? "true" : "false", eventState);

  switch (eventState)
  {
  case STABLE:
    if (wasStable && !isStable)
    {
      float direction = getAverageDirection(grams, baselineWeight);
      if (direction < -DELTA_THRESHOLD)
      {
        preDropWeight = baselineWeight;
        eventStartTime = getCorrectedTime();
        eventState = DROP_DETECTED;
        eventPrinter.printf("Drop detected: %.1fg → %.1fg", baselineWeight, grams);
      }
    }
    else if (isStable)
    {
      baselineWeight = grams; // Update baseline while stable
    }
    break;

  case DROP_DETECTED:
    if (!wasStable && isStable)
    {
      postDropWeight = grams;
      eventState = DROP_STABILIZED;
      eventPrinter.printf("Drop stabilized at: %.1fg", grams);
    }
    break;

  case DROP_STABILIZED:
    if (wasStable && !isStable)
    {
      float direction = getAverageDirection(grams, postDropWeight);
      if (direction > DELTA_THRESHOLD)
      {
        eventState = RISE_DETECTED;
        eventPrinter.printf("Rise detected: %.1fg → %.1fg", postDropWeight, grams);
      }
    }
    break;

  case RISE_DETECTED:
    if (!wasStable && isStable)
    {
      eventState = RISE_STABILIZED;
      float postRiseWeight = grams;
      float weightChange = preDropWeight - postRiseWeight;

      if (abs(weightChange) > CHANGE_DETECTION_THRESHOLD)
      {
        if (weightChange > 0)
        {
          eventPrinter.printf("Sip confirmed: %.1fg (%.1fg → %.1fg)",
                              weightChange, preDropWeight, postRiseWeight);
          getDataLogger().addSip(eventStartTime, weightChange);
        }
        else
        {
          eventPrinter.printf("Refill detected: %.1fg → %.1fg",
                              preDropWeight, postRiseWeight);
          getDataLogger().addRefill(eventStartTime, -weightChange);
        }
      }
      else
      {
        eventPrinter.print("False alarm, weight change below threshold");
      }

      baselineWeight = grams;
      eventState = STABLE;
    }
    break;
  }

  // Update stability tracking
  wasStable = isStable;

  // Record the measurement
  getDataLogger().addMeasurement(grams, isStable);

  // Use task delay instead of blocking delay
  vTaskDelay(pdMS_TO_TICKS(SAMPLING_RATE_MS));
}
