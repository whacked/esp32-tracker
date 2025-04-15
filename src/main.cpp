#include <Arduino.h>
#include "HX711.h"

HX711 scale;

// Use the pins you wired
#define DT 21
#define SCK 22

// Calibration values based on your measurements
#define CALIBRATION_LOW -400    // reading at no load  -331 is the average
#define CALIBRATION_HIGH 998000 // reading at 950g
#define WEIGHT_HIGH 950         // actual weight in grams

// Rolling average window
#define WINDOW_SIZE 10
float readings[WINDOW_SIZE];
int readIndex = 0;
float total = 0;
float average = 0;

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

  for (int i = 0; i < 3; i++)
  {
    digitalWrite(2, HIGH);
    delay(100);
    digitalWrite(2, LOW);
    delay(100);
  }

  Serial.println("Ready!");
}

String getTimestamp() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char timestamp[25];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S%z", &timeinfo);
  return String(timestamp);
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