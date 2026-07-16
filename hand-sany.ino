#include <Servo.h>

Servo myServo;

const int trigPin = 7;
const int echoPin = 8;
const int ledPin = 11;
const int buttonPin = 10;
const int servoPin = 9;
const int potPin = A5;

int lastPotAngle = -1;

bool buttonHeldLast = false;
unsigned long pressStartTime = 0;
const unsigned long RAMP_DURATION = 150;

enum ServoState { IDLE, SWEEPING, WAITING, COOLDOWN };
ServoState servoState = IDLE;
unsigned long sweepStartTime = 0;
unsigned long waitStartTime = 0;
const unsigned long SWEEP_DURATION = 150;
const unsigned long WAIT_DURATION = 1250;
const unsigned long COOLDOWN_DURATION = 1000;
unsigned long cooldownStartTime = 0;

void setup() {
  myServo.attach(servoPin);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.begin(9600);

  myServo.write(0);
}

long getDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 25000);
  if (duration == 0) return -1;

  long distance = duration * 0.0343 / 2;
  return distance;
}

void loop() {
  bool buttonHeld = (digitalRead(buttonPin) == LOW);

  if (servoState == IDLE) {
    long distance = getDistanceCM();

    if (distance > 0 && distance <= 3) {
      sweepStartTime = millis();
      servoState = SWEEPING;
    } else {
      if (buttonHeld && !buttonHeldLast) {
        pressStartTime = millis();
      }
      buttonHeldLast = buttonHeld;

      digitalWrite(ledPin, buttonHeld ? HIGH : LOW);

      if (buttonHeld) {
        unsigned long heldTime = millis() - pressStartTime;
        int rampAngle = map(heldTime, 0, RAMP_DURATION, 0, 90);
        rampAngle = constrain(rampAngle, 0, 90);

        if (rampAngle != lastPotAngle) {
          myServo.write(rampAngle);
          lastPotAngle = rampAngle;
        }
      } else {
        if (lastPotAngle != 0) {
          myServo.write(0);
          lastPotAngle = 0;
        }
      }
    }
  }
  else if (servoState == SWEEPING) {
    unsigned long elapsed = millis() - sweepStartTime;
    int angle = map(elapsed, 0, SWEEP_DURATION, 0, 90);
    angle = constrain(angle, 0, 90);

    myServo.write(angle);
    lastPotAngle = angle;
    digitalWrite(ledPin, HIGH);

    if (elapsed >= SWEEP_DURATION) {
      myServo.write(90);
      lastPotAngle = 90;
      waitStartTime = millis();
      servoState = WAITING;
    }
  }
  else if (servoState == WAITING) {
    digitalWrite(ledPin, HIGH);

    long distance = getDistanceCM();

    if (distance > 3 || distance == -1 || millis() - waitStartTime >= WAIT_DURATION) {
      myServo.write(0);
      lastPotAngle = 0;
      digitalWrite(ledPin, LOW);
      cooldownStartTime = millis();
      servoState = COOLDOWN;
    }
  }
  else if (servoState == COOLDOWN) {
    if (millis() - cooldownStartTime >= COOLDOWN_DURATION) {
      servoState = IDLE;
    }
  }

  delay(50);
}