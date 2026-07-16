#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---- Pin definitions ----
const int SERVO_PIN = 9;
const int TRIG_PIN = 7;
const int ECHO_PIN = 8;
const int BUZZER_PIN = 13;

// ---- Objects ----
Servo sweepServo;
LiquidCrystal_I2C lcd(0x27, 16, 2); // change to 0x3F if needed

// ---- Sweep state (servo NEVER stops) ----
int currentAngle = 0;
int sweepDirection = 1;
unsigned long lastStepTime = 0;
const int stepDelay = 15; // ms per 1-degree step

// ---- Detection thresholds (hysteresis) ----
const float ENTER_THRESHOLD_CM = 10.0;
const float EXIT_THRESHOLD_CM = 12.0;
const int REQUIRED_CONSISTENT_READINGS = 2;
int enterCount = 0;
int exitCount = 0;
bool inContactZone = false;

// ---- "Contact" display hold ----
const unsigned long HOLD_DURATION = 2000;   // how long text stays on screen
const unsigned long PING_INTERVAL = 700;    // min gap between repeat pings while tracking
bool showingContact = false;
float snapshotDistance = 0;
int snapshotAngle = 0;
unsigned long contactHoldUntil = 0;
unsigned long lastPingTime = 0;

// ---- Sensor polling ----
unsigned long lastSensorRead = 0;
const int sensorReadInterval = 560; // HC-SR04 needs ~60ms min between pings

// ---- Buzzer: non-blocking descending "radar ping" sweep ----
bool buzzerActive = false;
unsigned long buzzerStartTime = 0;
const int buzzerDuration = 150;   // total ping length, ms
const int buzzerStartFreq = 1500;
const int buzzerEndFreq = 1500;

void setup() {
  Serial.begin(9600);

  sweepServo.attach(SERVO_PIN);
  sweepServo.write(currentAngle);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  pinMode(BUZZER_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Scanning...");
}

float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return -1;

  return duration * 0.0343 / 2.0; // cm
}

void triggerPing() {
  buzzerActive = true;
  buzzerStartTime = millis();
  tone(BUZZER_PIN, buzzerStartFreq); // starts playing immediately, updated below
}

void updateBuzzer() {
  if (!buzzerActive) return;

  unsigned long elapsed = millis() - buzzerStartTime;
  if (elapsed >= buzzerDuration) {
    noTone(BUZZER_PIN);
    buzzerActive = false;
  } else {
    int freq = map(elapsed, 0, buzzerDuration, buzzerStartFreq, buzzerEndFreq);
    tone(BUZZER_PIN, freq);
  }
}

void showContactScreen(float distance, int angle) {
  lcd.setCursor(0, 0);
  lcd.print("Dist: ");
  lcd.print(distance, 1);
  lcd.print(" cm   ");

  lcd.setCursor(0, 1);
  lcd.print("Angle: ");
  lcd.print(angle);
  lcd.print((char)223);
  lcd.print("   ");
}

void loop() {
  // ---- Servo sweeps continuously, no matter what ----
  if (millis() - lastStepTime >= stepDelay) {
    lastStepTime = millis();
    currentAngle += sweepDirection;
    if (currentAngle >= 180) {
      currentAngle = 180;
      sweepDirection = -1;
    } else if (currentAngle <= 0) {
      currentAngle = 0;
      sweepDirection = 1;
    }
    sweepServo.write(currentAngle);
  }

  // ---- Sensor polling ----
  if (millis() - lastSensorRead >= sensorReadInterval) {
    lastSensorRead = millis();
    float distance = readDistanceCM();

    if (distance > 0) {
      if (!inContactZone) {
        if (distance <= ENTER_THRESHOLD_CM) {
          enterCount++;
          if (enterCount >= REQUIRED_CONSISTENT_READINGS) {
            inContactZone = true;
            enterCount = 0;
            exitCount = 0;
          }
        } else {
          enterCount = 0;
        }
      } else {
        if (distance > EXIT_THRESHOLD_CM) {
          exitCount++;
          if (exitCount >= REQUIRED_CONSISTENT_READINGS) {
            inContactZone = false;
            enterCount = 0;
            exitCount = 0;
          }
        } else {
          exitCount = 0;
        }
      }

      // While in the contact zone, refresh the snapshot + ping,
      // but no faster than PING_INTERVAL so it doesn't turn into one solid tone
      if (inContactZone && (millis() - lastPingTime >= PING_INTERVAL)) {
        lastPingTime = millis();

        snapshotDistance = distance;
        snapshotAngle = currentAngle;
        contactHoldUntil = millis() + HOLD_DURATION;

        if (!showingContact) {
          lcd.clear();
          showingContact = true;
        }
        showContactScreen(snapshotDistance, snapshotAngle);
        triggerPing();
      }
    }
  }

  // ---- Revert to "Scanning..." once hold time expires with no new contact ----
  if (showingContact && millis() > contactHoldUntil) {
    showingContact = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scanning...");
  }

  // ---- Keep the buzzer sweep updating (non-blocking) ----
  updateBuzzer();
}