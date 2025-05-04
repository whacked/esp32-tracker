#include <Arduino.h>
#include <deque>
#include <mutex>
#include "HX711.h"
#include "StatusPrinter.h"
#include "DataLogger.h"
#include "BtServer.h"

#define DEBUG_CALIBRATION 0

HX711 scale;

// Use the pins you wired
#define DT 21
#define SCK 22

// Calibration values based on your measurements
#ifdef HOME_SET
#define CALIBRATION_AT_NO_LOAD 46      // reading at no load  -331 is the average
#define CALIBRATION_AT_LOAD_1 -1263440 // reading at 950g
#define WEIGHT_AT_LOAD_1 1199          // actual weight in grams
#else                                  // OFFICE_SET
#define CALIBRATION_AT_NO_LOAD -400    // reading at no load  -331 is the average
#define CALIBRATION_AT_LOAD_1 998000   // reading at 950g
#define WEIGHT_AT_LOAD_1 950           // actual weight in grams
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

#define ZERO_THRESHOLD 1.0f // ≤ this == "nothing on scale"

// State machine

// ─────────── Plateau-based state machine ────────────
enum PlateauState
{
  EMPTY,
  PLATEAU
};

const char *stateName(PlateauState s)
{
  return s == EMPTY ? "empty" : "plateau";
}

PlateauState plateauState = EMPTY;
PlateauState prevPlateauState = EMPTY;
float prevPlateauWeight = 0.0f;
unsigned long plateauStartTime = 0; // first sample in this plateau

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

void processPlateauDetection(float grams, bool isStable)
{
  /* -------- persistent state -------- */
  static PlateauState state = EMPTY;
  static float prevPlateau = 0.0f;      // last stable weight (>0 g)
  static float weightBeforeLift = 0.0f; // plateau right before cup removal
  static unsigned long liftTime = 0;    // when the cup was lifted
  static bool waitingReplace = false;   // TRUE after a lift, until cup back
  static bool plateauAnnounce = false;  // verbose once per plateau

  unsigned long now = getDataLogger().getCorrectedTime();

  /* Ignore during instability ----------------------------------- */
  if (!isStable)
  {
    plateauAnnounce = false;
    return;
  }

  /* Optional verbose trace of plateaus (statusPrinter, not events) */
  if (!plateauAnnounce)
  {
    statusPrinter.printfLevel(3, "plateau %.1fg (%s)",
                              grams,
                              state == EMPTY ? "empty" : "cup on");
    plateauAnnounce = true;
  }

  /* Insignificant jitter that still passes stability test? */
  if (fabs(grams - prevPlateau) < CHANGE_DETECTION_THRESHOLD &&
      !(state == EMPTY && grams > ZERO_THRESHOLD))
  { // allow EMPTY→cup placed
    return;
  }

  /* ─── Cup REMOVED  (stable ~0 g) ─────────────────────────────── */
  if (grams <= ZERO_THRESHOLD)
  {
    if (state == PLATEAU)
    { // cup was present and lifted just now
      eventPrinter.printfLevel(2, "Cup removed (%.1fg → 0g)", prevPlateau);
      weightBeforeLift = prevPlateau;
      liftTime = now;
      waitingReplace = true;
    }
    state = EMPTY;
    prevPlateau = 0.0f;
    return;
  }

  /* ─── Cup PLACED / plateau >0 g  ─────────────────────────────── */
  if (state == EMPTY)
  {
    if (waitingReplace)
    {                                         // we just returned after a lift
      float delta = weightBeforeLift - grams; // +ve = sip
      if (fabs(delta) >= CHANGE_DETECTION_THRESHOLD)
      {
        if (delta > 0)
        {
          eventPrinter.printfLevel(0, "Sip    %.1fg  (%.1fg → %.1fg)",
                                   delta, weightBeforeLift, grams);
          getDataLogger().addSip(liftTime, delta);
        }
        else
        {
          eventPrinter.printfLevel(0, "Refill +%.1fg (%.1fg → %.1fg)",
                                   -delta, weightBeforeLift, grams);
          getDataLogger().addRefill(liftTime, -delta);
        }
      }
      else
      {
        eventPrinter.printfLevel(1, "No‑op Δ=%.1fg", delta);
      }
      waitingReplace = false;
    }
    else
    {
      /* first‑ever cup placement */
      eventPrinter.printfLevel(2, "Cup placed: %.1fg", grams);
    }

    state = PLATEAU;
    prevPlateau = grams;
    return;
  }

  /* ─── Weight changed while cup stayed on the scale (straw etc.) */
  float delta = prevPlateau - grams; // +ve = sip
  if (fabs(delta) >= CHANGE_DETECTION_THRESHOLD)
  {
    if (delta > 0)
    {
      eventPrinter.printfLevel(0, "Sip    %.1fg (%.1fg → %.1fg)",
                               delta, prevPlateau, grams);
      getDataLogger().addSip(now, delta);
    }
    else
    {
      eventPrinter.printfLevel(0, "Refill +%.1fg (%.1fg → %.1fg)",
                               -delta, prevPlateau, grams);
      getDataLogger().addRefill(now, -delta);
    }
    prevPlateau = grams;
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
      stateName(plateauState));

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
  // o3: map(long, … ) in the Arduino core truncates to long, throwing away all sub-gram precision.
  // float grams = map(long, … ) in the Arduino core truncates to long, throwing away all sub‑gram precision.
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

#if DEBUG_CALIBRATION
  // Print raw and EMA values for calibration
  Serial.printf("RAW: %8.1f | EMA: %8.1f | grams: %8.2f\n", rawValue, emaValue, grams);

  // If user sends a newline, print current calibration info
  if (Serial.available())
  {
    char c = Serial.read();
    if (c == '\n' || c == '\r')
    {
      Serial.println("---- Calibration Debug ----");
      Serial.printf("RAW: %8.1f\n", rawValue);
      Serial.printf("EMA: %8.1f\n", emaValue);
      Serial.printf("grams: %8.2f\n", grams);
      Serial.printf("CALIBRATION_AT_NO_LOAD: %ld\n", (long)CALIBRATION_AT_NO_LOAD);
      Serial.printf("CALIBRATION_AT_LOAD_1: %ld\n", (long)CALIBRATION_AT_LOAD_1);
      Serial.printf("WEIGHT_AT_LOAD_1: %ld\n", (long)WEIGHT_AT_LOAD_1);
      Serial.println("--------------------------");
    }
  }
#endif

  // Check stability on grams value after rounding
  isStable = checkStability(grams);

  // Process the state machine
  processPlateauDetection(grams, isStable);

  // Update stability tracking
  wasStable = isStable;

  // Fix state transition logging
  if (prevPlateauState != plateauState)
  {
    statusPrinter.printfLevel(2, "*** %s\t→\t%s", stateName(prevPlateauState), stateName(plateauState));
    prevPlateauState = plateauState;
  }

  // Record the measurement
  // getDataLogger().addMeasurement(grams, isStable);

  // Use task delay instead of blocking delay
  vTaskDelay(pdMS_TO_TICKS(SAMPLING_RATE_MS));
}
