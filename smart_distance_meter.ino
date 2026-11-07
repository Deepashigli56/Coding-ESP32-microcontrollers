/*
  Smart Distance Meter with Proportional Motor Feedback
  Board: ESP32 Dev Board
  Display: 16x2 I2C LCD
  Feedback: DC motor speed scales with proximity (closer = faster spin)

  Library required: LiquidCrystal_I2C (by Frank de Brabander)
  Install via Arduino IDE: Sketch > Include Library > Manage Libraries
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- Pin definitions ----------
#define TRIG_PIN      5
#define ECHO_PIN      18
#define MOTOR_PWM_PIN 25
#define SWITCH_PIN    27
#define POT_PIN       34   // ADC1 channel, input-only pin

// ---------- LCD setup ----------
// Common I2C address is 0x27 or 0x3F - run an I2C scanner sketch if unsure
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- PWM config for motor ----------
const int PWM_FREQ      = 5000;   // 5kHz, inaudible whine, smooth for small DC motors
const int PWM_CHANNEL   = 0;
const int PWM_RESOLUTION = 8;     // 0-255 duty range

// ---------- Distance thresholds (cm) ----------
const float MIN_DISTANCE = 5.0;     // motor at full speed at/below this distance
const float MAX_DISTANCE = 100.0;   // default max range to react to (overridden by pot)

// ---------- Variables ----------
long duration;
float distanceCm;
int motorSpeed;
bool systemEnabled = true;
unsigned long lastSwitchPress = 0;
const unsigned long DEBOUNCE_MS = 250;

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Configure PWM for motor control
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_PWM_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0);  // motor off initially

  // LCD init
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Distance");
  lcd.setCursor(0, 1);
  lcd.print("Meter Ready");
  delay(1500);
  lcd.clear();
}

void loop() {
  handleSwitch();

  if (systemEnabled) {
    distanceCm = measureDistanceCm();

    // Read potentiometer to let user set max sensitivity range live
    int potRaw = analogRead(POT_PIN);                  // 0-4095 on ESP32 ADC
    float maxRangeCm = map(potRaw, 0, 4095, 10, 200);   // adjustable 10-200 cm window

    motorSpeed = computeMotorSpeed(distanceCm, MIN_DISTANCE, maxRangeCm);
    ledcWrite(PWM_CHANNEL, motorSpeed);

    updateDisplay(distanceCm, motorSpeed, maxRangeCm);

    Serial.print("Distance: ");
    Serial.print(distanceCm);
    Serial.print(" cm | Motor PWM: ");
    Serial.print(motorSpeed);
    Serial.print(" | Max range: ");
    Serial.println(maxRangeCm);
  } else {
    ledcWrite(PWM_CHANNEL, 0);
    lcd.setCursor(0, 0);
    lcd.print("System Paused  ");
    lcd.setCursor(0, 1);
    lcd.print("Press switch    ");
  }

  delay(150);  // small delay for sensor + display stability
}

// Reads the HC-SR04 and returns distance in cm
float measureDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // pulseIn timeout of 30000us avoids permanent hang if echo never returns
  duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1;  // no echo received, out of range or sensor issue
  }

  float distance = (duration * 0.0343) / 2.0;  // speed of sound 343 m/s -> 0.0343 cm/us
  return distance;
}

// Maps distance to motor PWM duty cycle - closer object = faster motor
int computeMotorSpeed(float distance, float minDist, float maxDist) {
  if (distance < 0 || distance > maxDist) {
    return 0;  // out of range or invalid reading - motor off
  }
  if (distance <= minDist) {
    return 255;  // closest range - full speed
  }

  // Linear interpolation: closer distance -> higher PWM value
  float ratio = (maxDist - distance) / (maxDist - minDist);
  int pwmValue = (int)(ratio * 255);
  return constrain(pwmValue, 0, 255);
}

// Updates the LCD with current readings
void updateDisplay(float distance, int speed, float maxRange) {
  lcd.setCursor(0, 0);
  if (distance < 0) {
    lcd.print("Dist: ---   cm ");
  } else {
    lcd.print("Dist: ");
    lcd.print(distance, 1);
    lcd.print(" cm   ");
  }

  lcd.setCursor(0, 1);
  int speedPercent = map(speed, 0, 255, 0, 100);
  lcd.print("Spd:");
  lcd.print(speedPercent);
  lcd.print("% Rng:");
  lcd.print((int)maxRange);
  lcd.print("cm ");
}

// Handles the push switch to enable/disable the system with debounce
void handleSwitch() {
  if (digitalRead(SWITCH_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastSwitchPress > DEBOUNCE_MS) {
      systemEnabled = !systemEnabled;
      lastSwitchPress = now;
      lcd.clear();
    }
  }
}
