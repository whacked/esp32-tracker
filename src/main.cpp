#include <Arduino.h>
#include <deque>
#include <mutex>
#include "HX711.h"
#include "StatusPrinter.h"
#include "DataLogger.h"
#include "BtServer.h"

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

// Stabilization settings
#define STABILITY_TOLERANCE 1.0 // in grams
#define STABILITY_WINDOW 5      // window for stability check
#define SAMPLING_RATE_MS 25     // sampling period

// Primary moving average
#define WINDOW_SIZE 10
float readings[WINDOW_SIZE];
int readIndex = 0;
float total = 0;
float average = 0;

// Secondary window for stability detection
float stableReadings[STABILITY_WINDOW];
int stableIndex = 0;
bool isStable = false;

// Event detection settings
#define DELTA_THRESHOLD 1.0            // Threshold for detecting rises/drops
#define CHANGE_DETECTION_THRESHOLD 2.0 // Threshold for confirming sips/refills
#define DIRECTION_WINDOW 3             // Number of samples to average for direction detection

// State machine
enum EventState
{
  STABLE,          // Weight is stable, waiting for changes
  DROP_DETECTED,   // Initial drop detected, waiting for stabilization
  DROP_STABILIZED, // Drop has stabilized, waiting for increase
  RISE_DETECTED,   // Rise detected, waiting for stabilization
  RISE_STABILIZED  // Rise has stabilized, evaluate event type
};

EventState eventState = STABLE;
float baselineWeight = 0;
float eventStartWeight = 0;
time_t eventStartTime = 0;

// Direction detection
float preDropWeight = 0;
float postDropWeight = 0;
float directionBuffer[DIRECTION_WINDOW] = {0};
int directionIndex = 0;

// Sampling rate (shared with BtServer)
int samplingRateHz = 20;

// Status printers
StatusPrinter rawPrinter("RAW");
StatusPrinter eventPrinter("EVENT");
StatusPrinter statusPrinter("STATUS");

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

  // Initialize BtServer
  btServer = new BtServer(samplingRateHz);
  btServer->setup();
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
  getBtServer().processCommands();

  float rawValue = scale.get_units();
  rawPrinter.printf("raw=%.1f", rawValue);

  // Calculate weight with moving average
  total = total - readings[readIndex];
  readings[readIndex] = rawValue;
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % WINDOW_SIZE;
  average = total / WINDOW_SIZE;

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
        eventStartTime = getDataLogger().getCorrectedTime();
        eventState = DROP_DETECTED;
        eventPrinter.printf("Drop detected: %.1fg → %.1fg", baselineWeight, grams);
      }
    }
    else if (isStable)
    {
      baselineWeight = grams;
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
