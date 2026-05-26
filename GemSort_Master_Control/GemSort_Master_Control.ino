#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <ESP32Servo.h>

// ==========================================
// PIN DEFINITIONS
// ==========================================
// --- MOTOR 1: Main Belt (TA6586) ---
const int belt_in1 = 12; 
const int belt_in2 = 14; 

// --- MOTOR 2: Separator Disk (DRV8833) ---
const int disk_in1 = 27; 
const int disk_in2 = 26; 

// --- MOTOR 3: Sorting Servo (SG90) ---
const int servoPin = 15;

// ==========================================
// OBJECTS & GLOBALS
// ==========================================
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_24MS, TCS34725_GAIN_4X);
Servo myServo;
unsigned long lastColorCheck = 0;

int wood_R = 225, wood_G = 150, wood_B = 110;
bool holeIsUnderSensor = false; 
float hue, sat, val;

// Save current motor speeds so they don't get overwritten
int currentBeltSpeed = 0;
int currentDiskSpeed = 0;

// ==========================================
// MATH SUBROUTINES
// ==========================================
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

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // 1. ALLOCATE TIMERS FOR SERVO FIRST (Prevents conflict with DC Motors)
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  myServo.setPeriodHertz(50); 
  myServo.attach(servoPin, 500, 2400); 
  myServo.write(90); 

  // 2. NOW SETUP DC MOTORS
  pinMode(belt_in1, OUTPUT); pinMode(belt_in2, OUTPUT);
  pinMode(disk_in1, OUTPUT); pinMode(disk_in2, OUTPUT);
  
  // Start motors automatically at safe testing speeds
  currentBeltSpeed = 55;
  currentDiskSpeed = 35;
  analogWrite(belt_in1, map(currentBeltSpeed, 0, 100, 0, 255)); 
  digitalWrite(belt_in2, LOW);
  analogWrite(disk_in1, map(currentDiskSpeed, 0, 100, 0, 255)); 
  digitalWrite(disk_in2, LOW);

  Serial.println("\n--- Initializing Color Sensor ---");
  if (tcs.begin()) Serial.println("TCS34725 Found successfully!");
  else Serial.println("ERROR: No TCS34725 found.");

  Serial.println("\n=== GEMSORT MASTER AUTONOMOUS CONTROL ===");
  Serial.println("Motors have started automatically at 70% and 35%.");
  Serial.println("To change speeds, type two numbers (e.g., 30 20).");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  
  // ------------------------------------------
  // 1. DUAL MOTOR CONTROL (Manual Override)
  // ------------------------------------------
  if (Serial.available() > 0) {
    int newBelt = Serial.parseInt();
    int newDisk = Serial.parseInt();
    
    // Clear any trailing noise (like hidden \n or \r)
    delay(10);
    while(Serial.available() > 0) { Serial.read(); } 

    // Only update if valid numbers were entered (ignores serial noise timeouts which return 0)
    if (newBelt > 0 && newDisk > 0) {
      currentBeltSpeed = constrain(newBelt, 0, 100);
      currentDiskSpeed = constrain(newDisk, 0, 100);

      int pwmBelt = map(currentBeltSpeed, 0, 100, 0, 255);
      int pwmDisk = map(currentDiskSpeed, 0, 100, 0, 255);

      Serial.print(">>> BELT: "); Serial.print(currentBeltSpeed);
      Serial.print("%  |  DISK: "); Serial.print(currentDiskSpeed);
      Serial.println("%");

      analogWrite(belt_in1, pwmBelt);
      digitalWrite(belt_in2, LOW);
      analogWrite(disk_in1, pwmDisk);
      digitalWrite(disk_in2, LOW);
    }
  }

  // ------------------------------------------
  // 2. AUTONOMOUS COLOR SORTING LOGIC
  // ------------------------------------------
  if (millis() - lastColorCheck >= 100) {
    lastColorCheck = millis(); 
    
    uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);

    if (c > 430) { 
      if (holeIsUnderSensor == false) {
        holeIsUnderSensor = true; 
        
        // --- PEAK SATURATION TRACKER ---
        // A fixed delay doesn't work because dark gems and bright gems trigger at different times.
        // Instead, we sample the hole rapidly and save the RGB values that have the HIGHEST saturation.
        // Gems ALWAYS have higher saturation than the disk, wood, or white spots!
        
        float r_norm_init = r / 65535.0;
        float g_norm_init = g / 65535.0;
        float b_norm_init = b / 65535.0;
        float cmax_init = max(r_norm_init, max(g_norm_init, b_norm_init));
        float cmin_init = min(r_norm_init, min(g_norm_init, b_norm_init));
        float max_sat = (cmax_init == 0) ? 0 : ((cmax_init - cmin_init) / cmax_init) * 100.0;
        
        uint16_t best_r = r, best_g = g, best_b = b, best_c = c;

        for (int i = 0; i < 6; i++) {
           delay(20);
           tcs.getRawData(&r, &g, &b, &c);
           
           float r_norm = r / 65535.0;
           float g_norm = g / 65535.0;
           float b_norm = b / 65535.0;
           float cmax = max(r_norm, max(g_norm, b_norm));
           float cmin = min(r_norm, min(g_norm, b_norm));
           float current_sat = (cmax == 0) ? 0 : ((cmax - cmin) / cmax) * 100.0;
           
           if (current_sat > max_sat) {
               max_sat = current_sat;
               best_r = r;
               best_g = g;
               best_b = b;
               best_c = c;
           }
        }
        
        // Official reading is the point of maximum saturation!
        r = best_r; g = best_g; b = best_b; c = best_c;
        // -------------------------------
        
        float diff_R = r - wood_R;
        float diff_G = g - wood_G;
        float diff_B = b - wood_B;
        float distance = sqrt((diff_R * diff_R) + (diff_G * diff_G) + (diff_B * diff_B));

        if (distance < 45) {
           Serial.println("🕳️ Empty Hole Detected (Wood)");
        } 
        else {
           calculateHSV(r, g, b);
           Serial.print("💎 GEM: [Hue: "); Serial.print(hue, 1);
           Serial.print("°  Sat: "); Serial.print(sat, 1);
           Serial.print("%]");

           String detectedColor = "Unknown";

           // 0. TRAP THE WHITE SPOT ANOMALY (Hue 15-30, Sat < 55)
           if (hue >= 15 && hue <= 30 && sat < 55) {
               Serial.println("  ⚪ White Spot Anomaly (Ignored)");
           }
           // 1. PURPLE (Hue ~ 343°)
           else if (hue >= 300 && hue <= 350) {
               detectedColor = "Purple";
               myServo.write(170);
           }
           // 5. RED vs PINK (Hue < 8 or > 350)
           else if (hue > 350 || hue < 8) {
               if (sat >= 64) {
                   detectedColor = "Red";
                   myServo.write(10);
               } else {
                   detectedColor = "Pink";
                   myServo.write(35);
               }
           }
           else if (hue >= 8 && hue < 20) {
               detectedColor = "Orange";
               myServo.write(60);
           }
           else if (hue >= 20 && hue < 55) {
               detectedColor = "Yellow";
               myServo.write(85);
           }
           else if (hue >= 55 && hue < 140) {
               detectedColor = "Green";
               myServo.write(110);
           }
           else if (hue >= 140 && hue < 260) {
               detectedColor = "Blue";
               myServo.write(140);
           }

           if (detectedColor != "Unknown") {
               Serial.print("  ✅ Dropping in: ");
               Serial.println(detectedColor);
           }
        }
      }
    } 
    else if (c < 200) {
      holeIsUnderSensor = false; 
    }
  }
}
