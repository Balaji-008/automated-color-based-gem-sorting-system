#include <Wire.h>
#include "Adafruit_TCS34725.h"

// --- MOTOR 1: Main Belt (TA6586) ---
const int belt_in1 = 12; 
const int belt_in2 = 14; 

// --- MOTOR 2: Separator Disk (DRV8833) ---
const int disk_in1 = 27; 
const int disk_in2 = 26; 

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_24MS, TCS34725_GAIN_4X);
unsigned long lastColorCheck = 0;

float hue, sat, val;

void calculateHSV(float r, float g, float b) {
  float max_val = max(max(r, g), b);
  float min_val = min(min(r, g), b);
  float diff = max_val - min_val;

  if (max_val == 0) { sat = 0; val = 0; hue = 0; return; }
  sat = (diff / max_val) * 100.0;
  val = (max_val / 65535.0) * 100.0; 

  if (diff == 0) { hue = 0; } 
  else if (max_val == r) { hue = (60 * ((g - b) / diff) + 360); while (hue >= 360) hue -= 360; } 
  else if (max_val == g) { hue = (60 * ((b - r) / diff) + 120); } 
  else if (max_val == b) { hue = (60 * ((r - g) / diff) + 240); }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(belt_in1, OUTPUT); pinMode(belt_in2, OUTPUT);
  pinMode(disk_in1, OUTPUT); pinMode(disk_in2, OUTPUT);
  
  // Start the disk so we can see the anomalies
  analogWrite(belt_in1, map(0, 0, 100, 0, 255)); // Belt OFF
  digitalWrite(belt_in2, LOW);
  analogWrite(disk_in1, map(35, 0, 100, 0, 255)); // Disk ON at 35%
  digitalWrite(disk_in2, LOW);

  Serial.println("\n--- Initializing Color Sensor ---");
  if (tcs.begin()) {
    Serial.println("TCS34725 Found successfully!");
  } else {
    Serial.println("ERROR: No TCS34725 found.");
  }
  Serial.println("=== SPINNING CALIBRATION MODE ===");
  Serial.println("Let the disk spin. Do NOT drop any gems.");
}

void loop() {
  // Read extremely fast to catch the white spot
  if (millis() - lastColorCheck >= 50) {
    lastColorCheck = millis(); 
    
    uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);

    calculateHSV(r, g, b);

    Serial.print("RAW -> [C:"); Serial.print(c);
    Serial.print(" R:"); Serial.print(r);
    Serial.print(" G:"); Serial.print(g);
    Serial.print(" B:"); Serial.print(b);
    Serial.print("]   |   HSV -> [Hue:"); Serial.print(hue, 1);
    Serial.print("° Sat:"); Serial.print(sat, 1);
    Serial.println("%]");
  }
}
