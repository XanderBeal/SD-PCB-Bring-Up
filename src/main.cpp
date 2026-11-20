#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "funcs.h"


void setup() {
    pinMode(PC7, OUTPUT);
}

void loop() {
    digitalWrite(PC7, HIGH);
    delay(10);
    digitalWrite(PC7, LOW);
    delay(10);
}
