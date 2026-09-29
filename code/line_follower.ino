/* 
=========================================================== 
   SOP'26 WEEK 1 - IR Line Follower (ESP32-S3-CAM, single board) 
=========================================================== 
 
Board   : ESP32-S3-CAM  (camera unused in this version) 
Sensors : 2x IR line sensor (LM393-based, digital output) 
Driver  : L298N motor driver 
 
This is the SOP26_WEEK1_Line_Following_BuildManual wiring, coded up. 
No serial link to any other board needed -- one board does everything. 
 
----------------------------------------------------------- 
WIRING (must match exactly -- see manual Section 5 for why) 
----------------------------------------------------------- 
  ESP32-S3-CAM GPIO1  -> L298N ENA  (left motor speed, PWM) 
  ESP32-S3-CAM GPIO2  -> L298N IN1  (left motor direction A) 
  ESP32-S3-CAM GPIO21 -> L298N IN2  (left motor direction B) 
  ESP32-S3-CAM GPIO47 -> L298N ENB  (right motor speed, PWM) 
  ESP32-S3-CAM GPIO48 -> L298N IN3  (right motor direction A) 
  ESP32-S3-CAM GPIO41 -> L298N IN4  (right motor direction B) 
 
  ESP32-S3-CAM GPIO42 -> Left IR sensor OUT 
  ESP32-S3-CAM GPIO40 -> Right IR sensor OUT 
  5V                  -> both sensors' VCC 
  Common GND          -> both sensors' GND 
 
Do NOT use GPIO43/44 (Serial), 19/20 (USB), 35/36/37 (PSRAM), 
0 (boot), 45/46 (strapping), 38/39 (SD) -- see manual Section 4. 
 
----------------------------------------------------------- 
DECISION LOGIC (manual Section 3.2) 
----------------------------------------------------------- 
  Left sensor | Right sensor | Meaning                  | Action 
  ------------|--------------|---------------------------|----------- 
     OFF      |     OFF      | line centered, in the gap | straight 
     ON       |     OFF      | drifted right -> line now  | turn left 
              |              | under left sensor          | 
     OFF      |     ON       | drifted left -> line now   | turn right 
              |              | under right sensor         | 
     ON       |     ON       | both on line at once       | stop 
              |              | (marker/intersection)      | (safe default) 
 
----------------------------------------------------------- 
Reference: this logic follows the same well-tested 2-sensor 
pattern used in un0038998/LineFollowerRobot (github.com), adapted 
here for the ESP32-S3 pinout above. That project also raises PWM 
frequency via a raw AVR timer register (TCCR0B) to stop TT motors 
from buzzing at low speed -- that register is Arduino Uno/Nano 
specific and doesn't exist on the ESP32-S3, so here we use the 
MIN_MOTOR_PWM floor from your build manual instead (Section 2.2 / 
Section 9), which solves the same stiction problem on this chip. 
=========================================================== 
*/ 
 
// ---------------- PIN DEFINITIONS (do not change) ---------------- 
const int ENA = 1;   // left motor speed (PWM) 
const int IN1 = 2;   // left motor direction A 
const int IN2 = 21;  // left motor direction B 
const int ENB = 47;  // right motor speed (PWM) 
const int IN3 = 48;  // right motor direction A 
const int IN4 = 41;  // right motor direction B 
 
const int LEFT_IR_PIN  = 42; 
const int RIGHT_IR_PIN = 40; 
 
// ---------------- TUNABLE VALUES (edit these) ---------------- 
 
// Both wheels, driving straight 
const int FORWARD_SPEED = 50; 
 
// While correcting back onto the line: 
// OUTER wheel = the one speeding up, INNER wheel = the one slowing down 
const int TURN_OUTER_SPEED = 80; 
const int TURN_INNER_SPEED = 15; 
 
// Cheap TT gear motors often just buzz and don't spin below a certain 
// PWM value (internal friction beats the weak driving force). If the 
// robot's wheel buzzes but doesn't turn at low speed, raise this. 
const int MIN_MOTOR_PWM = 27.5; 
 
// LM393 IR sensor modules can be wired/tuned to output HIGH or LOW 
// when the line is detected, depending on the board and trimmer. 
// If your sensor readings look backwards in Serial Monitor (Step 6 
// of the manual), flip this single value and re-upload -- don't 
// rewire anything. 
const int SENSOR_ACTIVE_STATE = HIGH; // set to LOW if it reads backwards 
 
// ---------------- INTERNAL (don't touch) ---------------- 
unsigned long lastPrintTime = 0; 
 
void setup() { 
  Serial.begin(115200); 
 
  pinMode(ENA, OUTPUT); 
  pinMode(IN1, OUTPUT); 
  pinMode(IN2, OUTPUT); 
  pinMode(ENB, OUTPUT); 
  pinMode(IN3, OUTPUT); 
  pinMode(IN4, OUTPUT); 
 
  pinMode(LEFT_IR_PIN, INPUT); 
  pinMode(RIGHT_IR_PIN, INPUT); 
 
  stopMotors(); // start safely stopped 
 
  Serial.println("SOP26 IR Line Follower - ESP32-S3-CAM"); 
  Serial.println("Reading sensors... LEFT / RIGHT, ON_LINE or off"); 
} 
 
void loop() { 
  bool leftOnLine  = (digitalRead(LEFT_IR_PIN)  == SENSOR_ACTIVE_STATE); 
  bool rightOnLine = (digitalRead(RIGHT_IR_PIN) == SENSOR_ACTIVE_STATE); 
 
  // Print live readings a few times a second (not every loop -- that 
  // would flood Serial Monitor and slow everything down). 
  if (millis() - lastPrintTime > 200) { 
    Serial.print("LEFT="); 
    Serial.print(leftOnLine ? "ON_LINE" : "off"); 
    Serial.print("   RIGHT="); 
    Serial.println(rightOnLine ? "ON_LINE" : "off"); 
    lastPrintTime = millis(); 
  } 
 
  // ---- Decision table (manual Section 3.2) ---- 
  if (!leftOnLine && !rightOnLine) { 
    driveStraight(); 
  } else if (leftOnLine && !rightOnLine) { 
    turnLeft();   // drifted right -> correct left 
  } else if (!leftOnLine && rightOnLine) { 
    turnRight();  // drifted left -> correct right 
  } else { 
    stopMotors(); // both sensors on line at once -- safest default 
  } 
} 
 
//========================================================== 
// Movement functions 
//========================================================== 
 
void driveStraight() { 
  setMotors(FORWARD_SPEED, FORWARD_SPEED); 
} 
 
void turnLeft() { 
  // left wheel = inner (slower), right wheel = outer (faster) 
  setMotors(TURN_INNER_SPEED, TURN_OUTER_SPEED); 
} 
 
void turnRight() { 
  // right wheel = inner (slower), left wheel = outer (faster) 
  setMotors(TURN_OUTER_SPEED, TURN_INNER_SPEED); 
} 
 
void stopMotors() { 
  digitalWrite(IN1, LOW); 
  digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW); 
  digitalWrite(IN4, LOW); 
  analogWrite(ENA, 0); 
  analogWrite(ENB, 0); 
} 
 
//========================================================== 
// setMotors: both speeds are 0-255, always FORWARD direction. 
// Applies the MIN_MOTOR_PWM floor so a low turn speed never just 
// buzzes -- if a nonzero speed is below the floor, it's raised to 
// the floor instead of being silently too weak to move. 
//========================================================== 
 
void setMotors(int leftSpeed, int rightSpeed) { 
  leftSpeed  = applyMinPWM(leftSpeed); 
  rightSpeed = applyMinPWM(rightSpeed); 
 
  digitalWrite(IN1, HIGH); 
  digitalWrite(IN2, LOW); 
  analogWrite(ENA, leftSpeed); 
 
  digitalWrite(IN3, HIGH); 
  digitalWrite(IN4, LOW); 
  analogWrite(ENB, rightSpeed); 
} 
 
int applyMinPWM(int speed) { 
  if (speed <= 0) return 0; 
  if (speed < MIN_MOTOR_PWM) return MIN_MOTOR_PWM; 
  return speed; 
} where do i add this code to week 1 repo
