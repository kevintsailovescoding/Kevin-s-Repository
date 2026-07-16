#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int SERVO_PIN = 9;
const int TRIG_PIN = 7;
const int ECHO_PIN = 8;
const int BUZZER_PIN = 13;

Servo sweepServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

int currentAngle = 0;
int sweepDirection = 1;
unsigned long lastStepTime = 0;
const int stepDelay = 15;

const float ENTER_THRESHOLD_CM = 10.0;
const float EXIT_THRESHOLD_CM = 12.0;
const int REQUIRED_CONSISTENT_READINGS = 2;
int enterCount = 0;
int exitCount = 0;
bool inContactZone = false;

bool beepedThisLeg = false;

const unsigned long HOLD_DURATION = 2000;
bool showingContact = false;
float snapshotDistance = 0;
int snapshotAngle = 0;
unsigned long contactHoldUntil = 0;

unsigned long lastSensorRead = 0;
const int sensorReadInterval = 60;

bool buzzerActive = false;
unsigned long buzzerStartTime = 0;
const int buzzerDuration = 90;
const int buzzerFreq = 1800;

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

  return duration * 0.0343 / 2.0;
}

void triggerPing() {
  buzzerActive = true;
  buzzerStartTime = millis();
  tone(BUZZER_PIN, buzzerFreq);
}

void updateBuzzer() {
  if (!buzzerActive) return;

  if (millis() - buzzerStartTime >= buzzerDuration) {
    noTone(BUZZER_PIN);
    buzzerActive = false;
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
  if (millis() - lastStepTime >= stepDelay) {
    lastStepTime = millis();
    currentAngle += sweepDirection;

    if (currentAngle >= 180) {
      currentAngle = 180;
      sweepDirection = -1;
      beepedThisLeg = false;
    } else if (currentAngle <= 0) {
      currentAngle = 0;
      sweepDirection = 1;
      beepedThisLeg = false;
    }

    sweepServo.write(currentAngle);
  }

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

      if (inContactZone) {
        snapshotDistance = distance;
        snapshotAngle = currentAngle;
        contactHoldUntil = millis() + HOLD_DURATION;

        if (!showingContact) {
          lcd.clear();
          showingContact = true;
        }
        showContactScreen(snapshotDistance, snapshotAngle);

        if (!beepedThisLeg) {
          triggerPing();
          beepedThisLeg = true;
        }
      }
    }
  }

  if (showingContact && millis() > contactHoldUntil) {
    showingContact = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scanning...");
  }

  updateBuzzer();
}