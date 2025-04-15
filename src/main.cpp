#include <Arduino.h>
#include "HX711.h"

HX711 scale;

// Use the pins you wired
#define DT 21
#define SCK 22

void setup()
{
  Serial.begin(115200);
  pinMode(2, OUTPUT);

  scale.begin(DT, SCK);

  Serial.println("Taring...");
  scale.set_scale(); // default scale value
  scale.tare();      // resets the scale to 0

  for (int i = 0; i < 3; i++)
  {
    digitalWrite(2, HIGH);
    delay(100);
    digitalWrite(2, LOW);
    delay(100);
  }

  Serial.println("Ready!");
}

void loop()
{
  Serial.print("Raw: ");
  Serial.print(scale.read());
  Serial.print(" | Calibrated: ");
  Serial.println(scale.get_units(), 2);
  delay(500);
}
