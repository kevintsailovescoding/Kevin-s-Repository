#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int SERVO_PIN = 9;
const int TRIG_PIN = 7;
const int ECHO_PIN = 8;
const int BUZZER_PIN = 13;
const int BUTTON_PIN = 10;
const int GREEN_LED_PIN = 11;
const int POT_PIN = A0;
const int LASER_PIN = 2;

Servo sweepServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

bool manualMode = false;

int lastButtonReading = HIGH;
int stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

int currentAngle = 0;
int sweepDirection = 1;
unsigned long lastStepTime = 0;
const int stepDelay = 15;

const float ENTER_THRESHOLD_CM = 10.0;
const float EXIT_THRESHOLD_CM = 12.0;

const int REQUIRED_CONSISTENT_READINGS = 1;
int enterCount = 0;
int exitCount = 0;
bool inContactZone = false;

bool beepedThisLeg = false;

bool showingContact = false;
float snapshotDistance = 0;
int snapshotAngle = 0;

unsigned long lastSensorRead = 0;
const int sensorReadInterval = 40;

bool buzzerActive = false;
unsigned long buzzerStartTime = 0;
const int buzzerDuration = 90;
const int buzzerFreq = 2000;

const int manualBuzzerFreq = 2000;
const unsigned long manualBeepOnTime = 30;
const unsigned long manualBeepOffTime = 30;
bool manualBuzzerOn = false;
unsigned long manualBuzzerToggleTime = 0;
bool lastObjectInRange = false;

void setup() {
  Serial.begin(9600);

  sweepServo.attach(SERVO_PIN);
  sweepServo.write(currentAngle);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);

  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);

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

void handleButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != stableButtonState) {
      stableButtonState = reading;

      if (stableButtonState == LOW) {
        manualMode = !manualMode;
        digitalWrite(GREEN_LED_PIN, manualMode ? HIGH : LOW);

        noTone(BUZZER_PIN);
        buzzerActive = false;
        manualBuzzerOn = false;
        lastObjectInRange = false;
        inContactZone = false;
        enterCount = 0;
        exitCount = 0;
        beepedThisLeg = false;
        showingContact = false;
        digitalWrite(LASER_PIN, LOW);

        lcd.clear();
        if (manualMode) {
          lcd.setCursor(0, 0);
          lcd.print("Manual Mode");
        } else {
          lcd.setCursor(0, 0);
          lcd.print("Scanning...");
        }
      }
    }
  }

  lastButtonReading = reading;
}

void triggerPing() {
  buzzerActive = true;
  buzzerStartTime = millis();
  tone(BUZZER_PIN, buzzerFreq);
}

void updateSweepBuzzer() {
  if (!buzzerActive) return;
  if (millis() - buzzerStartTime >= buzzerDuration) {
    noTone(BUZZER_PIN);
    buzzerActive = false;
  }
}

void updateManualBuzzer(bool objectInRange) {
  if (!objectInRange) {
    if (manualBuzzerOn) {
      noTone(BUZZER_PIN);
      manualBuzzerOn = false;
    }
    return;
  }

  unsigned long now = millis();
  if (manualBuzzerOn) {
    if (now - manualBuzzerToggleTime >= manualBeepOnTime) {
      noTone(BUZZER_PIN);
      manualBuzzerOn = false;
      manualBuzzerToggleTime = now;
    }
  } else {
    if (now - manualBuzzerToggleTime >= manualBeepOffTime) {
      tone(BUZZER_PIN, manualBuzzerFreq);
      manualBuzzerOn = true;
      manualBuzzerToggleTime = now;
    }
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

void runSweepMode() {
  unsigned long nowStep = millis();
  unsigned long elapsed = nowStep - lastStepTime;
  if (elapsed >= (unsigned long)stepDelay) {
    int stepsToMove = elapsed / stepDelay;
    lastStepTime += (unsigned long)stepsToMove * stepDelay;

    currentAngle += sweepDirection * stepsToMove;

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

    if (distance <= 0) {
      distance = 999.0;
    }

    bool justEnteredZone = false;

    if (!inContactZone) {
      if (distance <= ENTER_THRESHOLD_CM) {
        enterCount++;
        if (enterCount >= REQUIRED_CONSISTENT_READINGS) {
          inContactZone = true;
          justEnteredZone = true;
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

          beepedThisLeg = false;

          if (showingContact) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Scanning...");
            showingContact = false;
          }
        }
      } else {
        exitCount = 0;
      }
    }

    if (justEnteredZone) {
      digitalWrite(LASER_PIN, HIGH);

      snapshotDistance = distance;
      snapshotAngle = currentAngle;

      if (!showingContact) {
        lcd.clear();
        showingContact = true;
      }
      showContactScreen(snapshotDistance, snapshotAngle);

      beepedThisLeg = true;
      triggerPing();

      delay(100);

      digitalWrite(LASER_PIN, LOW);

      lastStepTime = millis();
      lastSensorRead = millis();

    } else if (inContactZone && !beepedThisLeg) {
      triggerPing();
      beepedThisLeg = true;
    }
  }

  updateSweepBuzzer();
}

void runManualMode() {
  int potValue = analogRead(POT_PIN);
  int manualAngle = map(potValue, 0, 1023, 0, 180);
  sweepServo.write(manualAngle);

  if (millis() - lastSensorRead >= sensorReadInterval) {
    lastSensorRead = millis();
    float distance = readDistanceCM();

    if (distance <= 0) {
      distance = 999.0;
    }

    if (distance <= ENTER_THRESHOLD_CM) {
      digitalWrite(LASER_PIN, HIGH);

      lcd.setCursor(0, 0);
      lcd.print("Dist: ");
      lcd.print(distance, 1);
      lcd.print(" cm   ");

      lcd.setCursor(0, 1);
      lcd.print("Angle: ");
      lcd.print(manualAngle);
      lcd.print((char)223);
      lcd.print("   ");

      lastObjectInRange = true;
    } else {
      digitalWrite(LASER_PIN, LOW);

      lastObjectInRange = false;

      lcd.setCursor(0, 0);
      lcd.print("Manual Mode   ");
      lcd.setCursor(0, 1);
      lcd.print("               ");
    }
  }

  updateManualBuzzer(lastObjectInRange);
}

void loop() {
  handleButton();

  if (manualMode) {
    runManualMode();
  } else {
    runSweepMode();
  }
}