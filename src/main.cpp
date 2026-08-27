#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1000); // give USB serial time to connect
  Serial.println("Wireless Tracker booted OK");
}

void loop() {
  Serial.println("alive...");
  delay(1000);
}