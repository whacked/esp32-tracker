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
#define SAMPLING_RATE_MS 10     // sampling period

#define EMA_ALPHA 0.60f     // Smoothing factor (0 to 1), higher = more responsive
#define STABILITY_WINDOW 10 // window for stability check

// Secondary window for stability detection
float stableReadings[STABILITY_WINDOW];
int stableIndex = 0;
bool isStable = false;

// Event detection settings
#define DELTA_THRESHOLD 1.0             // Threshold for detecting rises/drops
#define CHANGE_DETECTION_THRESHOLD 2.0f // Threshold for confirming sips/refills
#define DIRECTION_WINDOW 3              // Number of samples to average for direction detection

#define ZERO_THRESHOLD 1.0f // ≤ this == “nothing on scale”

// State machine
// Replaces the old enum
enum EventState
{
  WAITING,        // Scale has never seen a cup (or was just tared)
  CUP_ON_STABLE,  // Cup present, weight is stable
  CUP_OFF_STABLE, // Cup removed, weight ~ 0 g and stable
  TRANSITION      // Any unstable period between plateaus
};

const char *getStateStr(EventState state)
{
  switch (state)
  {
  case WAITING:
    return "waiting";
  case CUP_ON_STABLE:
    return "plateau A";
  case TRANSITION:
    return "transitioning";
  case CUP_OFF_STABLE:
    return "cup off stable";
  default:
    return "unknown";
  }
}

float baselineWeight = 0;
float eventStartWeight = 0;
time_t eventStartTime = 0;
bool wasStable = false; // Moved here from loop()
float lastStableWeight = 0.0f;
float dropStartWeight = 0.0f;
float postDropWeight = 0.0f;
unsigned long dropStartTime = 0;
float preDropWeight = 0;
float directionBuffer[DIRECTION_WINDOW] = {0};
int directionIndex = 0;

float emaValue = 0; // Current EMA value

EventState eventState = WAITING;
EventState prevState = WAITING;
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

void processStateDetection(float grams, bool isStable, bool wasStable)
{
  static float lastCupWeight = 0.0f;    // plateaus with cup on
  static unsigned long lastCupTime = 0; // when cup was lifted

  switch (eventState)
  {
  /* ────────────────────────────────────────────────────
     Never saw a cup yet.  First stable reading > 1 g
     becomes our reference weight, but does *not* fire
     any event. */
  case WAITING:
    if (isStable && grams > ZERO_THRESHOLD)
    {
      lastCupWeight = grams;
      eventState = CUP_ON_STABLE;
      eventPrinter.printfLevel(2, "Cup placed: %.1fg", grams);
    }
    break;

  /* ────────────────────────────────────────────────────
     We’re on a plateau with a cup.  Leaving the plateau
     (unstable) switches to TRANSITION.  Landing at ~0 g
     and stable sets CUP_OFF_STABLE. */
  case CUP_ON_STABLE:
    if (!isStable && wasStable)
    {
      eventState = TRANSITION;
    }
    break;

  /* ────────────────────────────────────────────────────
     Cup was lifted and is off the scale (stable ≤ 1 g).
     Any unstable reading kicks us to TRANSITION; the
     *next* stable >1 g is a new cup‑on plateau, where we
     classify the Δ. */
  case CUP_OFF_STABLE:
    if (!isStable && wasStable)
    {
      eventState = TRANSITION;
    }
    break;

  /* ────────────────────────────────────────────────────
     Transitional noise (cup being moved).  We only care
     once stability returns. */
  case TRANSITION:
    if (isStable)
    {
      if (grams <= ZERO_THRESHOLD)
      {
        // Cup just left the scale → CUP_OFF plateau
        eventState = CUP_OFF_STABLE;
        lastCupTime = getDataLogger().getCorrectedTime();
        eventPrinter.printfLevel(2, "Cup removed (%.1fg → 0g)", lastCupWeight);
      }
      else
      {                                      // cup put back
        float delta = lastCupWeight - grams; // +ve = sip
        if (fabs(delta) < CHANGE_DETECTION_THRESHOLD)
        {
          eventPrinter.printfLevel(1, "No‑op Δ=%.1fg", delta);
        }
        else if (delta > 0)
        {
          eventPrinter.printfLevel(0, "Sip  %.1fg  (%.1fg → %.1fg)",
                                   delta, lastCupWeight, grams);
          getDataLogger().addSip(lastCupTime, delta);
        }
        else
        {
          eventPrinter.printfLevel(0, "Refill +%.1fg  (%.1fg → %.1fg)",
                                   -delta, lastCupWeight, grams);
          getDataLogger().addRefill(lastCupTime, -delta);
        }

        lastCupWeight = grams; // new baseline
        eventState = CUP_ON_STABLE;
      }
    }
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(2, OUTPUT);

  scale.begin(DT, SCK);

  statusPrinter.printf("Taring...");
  scale.set_scale();
  scale.tare();

  // startup indicator
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(2, HIGH);
    delay(100);
    digitalWrite(2, LOW);
    delay(100);
  }

  // Initialize BtServer
  int samplingRateHz = 1000 / SAMPLING_RATE_MS; // Calculate Hz from ms
  statusPrinter.printf("starting server");
  btServer = new BtServer(samplingRateHz);
  btServer->setup();
  statusPrinter.printf("Ready!");

  // // DEBUG: add 20 fake measurements
  // for (int i = 0; i < 20; i++)
  // {
  //   if (i % 2 == 0)
  //   {
  //     getDataLogger().addMeasurement(100 + i, true);
  //   }
  //   else
  //   {
  //     getDataLogger().addSip(100 + i, i);
  //   }
  // }
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
  statusPrinter.printfLevel(
      2, "value=%6.1f window=[%6.1f %6.1f] diff=%6.1f -> %s\t|\t%s",
      newValue, minVal, maxVal, diff,
      stable ? "stable" : "unstable",
      getStateStr(eventState)); // ← updated

  return stable;
}

void loop()
{
  getBtServer().processCommands();

  float rawValue = scale.get_units();
  // rawPrinter.printf("raw=%.1f", rawValue);

  // Calculate weight with exponential moving average
  if (emaValue == 0)
  {
    // Initialize EMA with first reading
    emaValue = rawValue;
  }
  else
  {
    // EMA formula: EMAt = α * Xt + (1 - α) * EMAt-1
    emaValue = EMA_ALPHA * rawValue + (1 - EMA_ALPHA) * emaValue;
  }

  // Original
  // o3: map(long, … ) in the Arduino core truncates to long, throwing away all sub‑gram precision.
  // float grams = map(emaValue, CALIBRATION_AT_NO_LOAD, CALIBRATION_AT_LOAD_1, 0, WEIGHT_AT_LOAD_1);
  float grams =
      (emaValue - CALIBRATION_AT_NO_LOAD) *
      (float)WEIGHT_AT_LOAD_1 /
      (CALIBRATION_AT_LOAD_1 - CALIBRATION_AT_NO_LOAD);

  grams = max(0.0f, grams);

  // Round very small values to 0 to prevent noise
  if (abs(grams) < 0.1)
  {
    grams = 0;
  }

  // Check stability on grams value after rounding
  isStable = checkStability(grams);

  // Process the state machine
  processStateDetection(grams, isStable, wasStable);

  // Update stability tracking
  wasStable = isStable;

  if (prevState != eventState)
  {
    statusPrinter.printfLevel(2, "*** %s\t→\t%s", getStateStr(prevState), getStateStr(eventState));
    prevState = eventState;
  }

  // Record the measurement
  // getDataLogger().addMeasurement(grams, isStable);

  // Use task delay instead of blocking delay
  vTaskDelay(pdMS_TO_TICKS(SAMPLING_RATE_MS));
}
