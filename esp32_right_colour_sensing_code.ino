//
// MSE 2202 TCS34725 colour sensor example
// Updated to:
// - average multiple readings
// - normalize RGB values
// - detect color names using thresholds
// - control SG90 sorter servo
//
// Language: Arduino (C++)
// Target:   ESP32
//

#define PRINT_COLOUR

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <SPI.h>
#include "Adafruit_TCS34725.h"
#include <ESP32Servo.h>

// Function declarations
void doHeartbeat();
String detectColor(float rn, float gn, float bn, uint16_t c);
void updateSorter(const String& detectedColor);

// Constants
const int cHeartbeatInterval = 75;
const int cSmartLED          = 23;
const int cSmartLEDCount     = 1;
const int cSDA               = 18;
const int cSCL               = 19;
const int cTCSLED            = 14;
const int cLEDSwitch         = 34;

// Servo constants
const int cServoPin          = 22;

// Set this to your known center value
const int cServoCenterUS     = 1600;

// Approximate microsecond change for 90 degrees from center
// Tune this value if needed: try 450, 500, 550, etc.
const int cServo90OffsetUS   = 1050;

// Left/right positions are symmetric around center
const int cServoLeftUS       = cServoCenterUS + cServo90OffsetUS;
const int cServoRightUS      = cServoCenterUS - cServo90OffsetUS;

const int cServoMoveDelayMs  = 400;

// Averaging settings
const int cNumSamples        = 10;
const int cSampleDelayMs     = 5;

// Variables
boolean heartbeatState       = true;
unsigned long lastHeartbeat  = 0;
unsigned long curMillis      = 0;
unsigned long prevMillis     = 0;

// Sorter state
String lastSorterDirection   = "CENTER";

// Declare SK6812 SMART LED object
Adafruit_NeoPixel SmartLEDs(cSmartLEDCount, cSmartLED, NEO_RGB + NEO_KHZ800);

// Servo object
Servo sorterServo;

// Smart LED brightness for heartbeat
unsigned char LEDBrightnessIndex = 0;
unsigned char LEDBrightnessLevels[] = {
  0, 0, 0, 5, 15, 30, 45, 60, 75, 90, 105, 120, 135,
  150, 135, 120, 105, 90, 75, 60, 45, 30, 15, 5, 0
};

// TCS34725 colour sensor
Adafruit_TCS34725 tcs =
  Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_2_4MS, TCS34725_GAIN_4X);

bool tcsFlag = 0;

void setup() {
  Serial.begin(115200);

  // Set up SmartLED
  SmartLEDs.begin();
  SmartLEDs.clear();
  SmartLEDs.setPixelColor(0, SmartLEDs.Color(0, 0, 0));
  SmartLEDs.setBrightness(0);
  SmartLEDs.show();

  Wire.setPins(cSDA, cSCL);
  pinMode(cTCSLED, OUTPUT);
  pinMode(cLEDSwitch, INPUT_PULLUP);

  // Set up sorter servo
  sorterServo.setPeriodHertz(50);
  sorterServo.attach(cServoPin, 500, 2400);
  sorterServo.writeMicroseconds(cServoCenterUS);
  delay(500);
  sorterServo.detach();

  // Connect to TCS34725 colour sensor
  if (tcs.begin()) {
    Serial.println("Found TCS34725 colour sensor");
    tcsFlag = true;
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    tcsFlag = false;
  }
}

void loop() {
  uint16_t r = 0, g = 0, b = 0, c = 0;

  digitalWrite(cTCSLED, !digitalRead(cLEDSwitch));

  if (tcsFlag) {
    // Average multiple samples
    uint32_t rSum = 0, gSum = 0, bSum = 0, cSum = 0;

    for (int i = 0; i < cNumSamples; i++) {
      uint16_t rt, gt, bt, ct;
      tcs.getRawData(&rt, &gt, &bt, &ct);
      rSum += rt;
      gSum += gt;
      bSum += bt;
      cSum += ct;
      delay(cSampleDelayMs);
    }

    r = rSum / cNumSamples;
    g = gSum / cNumSamples;
    b = bSum / cNumSamples;
    c = cSum / cNumSamples;

    float sum = r + g + b;

    if (sum > 0) {
      float rn = r / sum;
      float gn = g / sum;
      float bn = b / sum;

      String detectedColor = detectColor(rn, gn, bn, c);

      updateSorter(detectedColor);

#ifdef PRINT_COLOUR
      Serial.printf("Raw  R:%4d G:%4d B:%4d C:%4d   ", r, g, b, c);
      Serial.printf("Norm R:%.3f G:%.3f B:%.3f   ", rn, gn, bn);
      Serial.print("Detected: ");
      Serial.print(detectedColor);
      Serial.print("   Sorter: ");
      Serial.println(lastSorterDirection);
#endif
    } else {
      Serial.println("No valid colour reading");
    }
  }

  doHeartbeat();
}

// Detect color from normalized RGB + clear channel
String detectColor(float rn, float gn, float bn, uint16_t c) {

  // BLACK
  if (c < 40) {
    return "BLACK";
  }

  // WHITE
  if (c > 300 && abs(rn - gn) < 0.08 && abs(gn - bn) < 0.08) {
    return "WHITE";
  }

  // GREEN
  if (gn > 0.45) {
    return "GREEN";
  }

  // BLUE
  if (bn > 0.45 && c > 60) {
    return "BLUE";
  }

  // YELLOW
  if (rn > 0.32 && gn > 0.35 && bn < 0.28) {
    return "YELLOW";
  }

  // ORANGE
  if (rn > 0.52 && gn > 0.18 && bn < 0.25) {
    return "ORANGE";
  }

  // PURPLE
  if (bn > 0.35 && rn > 0.28 && gn < 0.32) {
    return "PURPLE";
  }

  // PINK
  if (rn > 0.50 && bn > 0.22 && gn < 0.30) {
    return "P