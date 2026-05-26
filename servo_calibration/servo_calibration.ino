#include <ESP32Servo.h>

Servo myServo;

// CHANGE THIS to the pin your servo is connected to
const int servoPin = 15; 

void setup() {
  Serial.begin(115200);
  
  // Initialize ESP32 Servo
  // Standard SG90 pulse widths are usually 500us to 2400us
  myServo.setPeriodHertz(50); 
  myServo.attach(servoPin, 500, 2400); 

  Serial.println("\n=== SG90 SERVO CALIBRATOR ===");
  Serial.println("Type an angle between 0 and 180 and hit enter.");
  Serial.println("Listen closely. If the servo buzzes loudly, you have hit the physical limit!");
  
  // Start at exactly 90 degrees (center)
  myServo.write(90);
  Serial.println("Starting at 90 degrees (Center).");
}

void loop() {
  if (Serial.available() > 0) {
    int targetAngle = Serial.parseInt();
    
    // Clear the serial buffer
    delay(10);
    while(Serial.available() > 0) { Serial.read(); }

    if (targetAngle >= 0 && targetAngle <= 180) {
      Serial.print("Moving to: ");
      Serial.print(targetAngle);
      Serial.println(" degrees");
      
      myServo.write(targetAngle);
    } else {
      Serial.println("Please enter a valid angle between 0 and 180.");
    }
  }
}
