#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define trigPin 7
#define echoPin 8

#define redLED 13
#define yellowLED 4
#define greenLED 2

#define button1Pin 11 
#define button2Pin 5  
#define buzzerPin 3

LiquidCrystal_I2C lcd(0x27, 16, 2);

enum GameState {
  IDLE,
  SHOW_TARGET,
  P1_GET_READY,
  P1_MEASURING,
  P1_REVEAL,
  P2_GET_READY,
  P2_MEASURING,
  P2_REVEAL,
  FINAL_RESULT
};
GameState gameState = IDLE;

float targetDistance;

float p1Distance;
float p2Distance;
bool p1TimedOut;
bool p2TimedOut;

unsigned long stateStartTime;
unsigned long roundStartTime;

const unsigned long TIME_LIMIT = 7500;        
const unsigned long READY_PAUSE = 2000;         
const unsigned long REVEAL_DISPLAY_TIME = 2200; 
const unsigned long RESULT_DISPLAY_TIME = 3500; 

const unsigned long PING_INTERVAL = 60; 
unsigned long lastPingTime = 0;
float latestDistance = 999; 

struct ButtonDebouncer {
  int pin;
  bool lastReading;
  bool state;
  unsigned long lastDebounceTime;
};

ButtonDebouncer btn1 = {button1Pin, HIGH, HIGH, 0};
ButtonDebouncer btn2 = {button2Pin, HIGH, HIGH, 0};
const unsigned long debounceDelay = 40;

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(redLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(greenLED, OUTPUT);

  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  randomSeed(analogRead(A0)); 

  lcd.init();
  lcd.backlight();

  showIdleScreen();
}

void loop() {
  switch (gameState) {
    case IDLE:
      if (checkButtonPressed(btn1) || checkButtonPressed(btn2)) {
        gameState = SHOW_TARGET;
      }
      break;

    case SHOW_TARGET:
      startNewRound();
      break;

    case P1_GET_READY:
      if (millis() - stateStartTime >= READY_PAUSE) {
        beginMeasuringPhase(true);
      }
      break;

    case P1_MEASURING:
      handleMeasuring(true);
      break;

    case P1_REVEAL:
      if (millis() - stateStartTime >= REVEAL_DISPLAY_TIME) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Target: ");
        lcd.print(targetDistance, 0);
        lcd.print(" cm");
        lcd.setCursor(0, 1);
        lcd.print("P2 get ready...");
        stateStartTime = millis();
        gameState = P2_GET_READY;
      }
      break;

    case P2_GET_READY:
      if (millis() - stateStartTime >= READY_PAUSE) {
        beginMeasuringPhase(false);
      }
      break;

    case P2_MEASURING:
      handleMeasuring(false);
      break;

    case P2_REVEAL:
      if (millis() - stateStartTime >= REVEAL_DISPLAY_TIME) {
        showFinalResult();
      }
      break;

    case FINAL_RESULT:
      if (millis() - stateStartTime >= RESULT_DISPLAY_TIME) {
        showIdleScreen();
      }
      break;

    default:
      break;
  }

  if (gameState != IDLE && gameState != P1_MEASURING && gameState != P2_MEASURING) {
    checkButtonPressed(btn1);
    checkButtonPressed(btn2);
  }
}

void showIdleScreen() {
  allLEDsOff();
  noTone(buzzerPin);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Distance Game!");
  lcd.setCursor(0, 1);
  lcd.print("Press a button");
  gameState = IDLE;
}

void startNewRound() {
  targetDistance = random(5, 26); 
  p1TimedOut = false;
  p2TimedOut = false;

  allLEDsOff();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Target: ");
  lcd.print(targetDistance, 0);
  lcd.print(" cm");
  lcd.setCursor(0, 1);
  lcd.print("P1 get ready...");

  stateStartTime = millis();
  gameState = P1_GET_READY;
}

void beginMeasuringPhase(bool isPlayer1) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Target: ");
  lcd.print(targetDistance, 0);
  lcd.print(" cm");
  lcd.setCursor(0, 1);
  lcd.print(isPlayer1 ? "P1: Move + press" : "P2: Move + press");

  roundStartTime = millis();
  lastPingTime = 0;             
  latestDistance = getDistanceCM(); 
  gameState = isPlayer1 ? P1_MEASURING : P2_MEASURING;
}

void handleMeasuring(bool isPlayer1) {
  if (millis() - lastPingTime >= PING_INTERVAL) {
    latestDistance = getDistanceCM();
    lastPingTime = millis();
  }

  bool pressed = isPlayer1 ? checkButtonPressed(btn1) : checkButtonPressed(btn2);
  bool timedOut = (millis() - roundStartTime >= TIME_LIMIT);

  if (pressed || timedOut) {
    if (isPlayer1) {
      p1TimedOut = timedOut && !pressed;
      p1Distance = latestDistance;
      revealPlayer(true);
    } else {
      p2TimedOut = timedOut && !pressed;
      p2Distance = latestDistance;
      revealPlayer(false);
    }
  }

}

void revealPlayer(bool isPlayer1) {
  allLEDsOff();

  bool timedOut = isPlayer1 ? p1TimedOut : p2TimedOut;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(isPlayer1 ? "P1: " : "P2: ");

  if (timedOut) {
    lcd.print("Too slow!");
  } else {
    lcd.print("Locked in!");
  }
  lcd.setCursor(0, 1);
  lcd.print("Guess sealed");

  playDecentSound();

  stateStartTime = millis();
  gameState = isPlayer1 ? P1_REVEAL : P2_REVEAL;
}

void showFinalResult() {
  allLEDsOff();

  float p1Diff = p1TimedOut ? 999999 : abs(p1Distance - targetDistance);
  float p2Diff = p2TimedOut ? 999999 : abs(p2Distance - targetDistance);

  lcd.clear();

  if (p1TimedOut && p2TimedOut) {
    lcd.setCursor(0, 0);
    lcd.print("Both too slow!");
    lcd.setCursor(0, 1);
    lcd.print("No winner");
    blinkYellow();
    playLoseSound();
  } else if (p1Diff < p2Diff) {
    lcd.setCursor(0, 0);
    lcd.print("Player 1 Wins!");
    lcd.setCursor(0, 1);
    lcd.print("P1:");
    lcd.print(p1Distance, 1);
    lcd.print(" P2:");
    lcd.print(p2TimedOut ? 0 : p2Distance, 1);
    digitalWrite(redLED, HIGH);   
    playWinSound();
  } else if (p2Diff < p1Diff) {
    lcd.setCursor(0, 0);
    lcd.print("Player 2 Wins!");
    lcd.setCursor(0, 1);
    lcd.print("P1:");
    lcd.print(p1TimedOut ? 0 : p1Distance, 1);
    lcd.print(" P2:");
    lcd.print(p2Distance, 1);
    digitalWrite(greenLED, HIGH); 
    playWinSound();
  } else {
    lcd.setCursor(0, 0);
    lcd.print("It's a tie!");
    lcd.setCursor(0, 1);
    lcd.print("P1:");
    lcd.print(p1Distance, 1);
    lcd.print(" P2:");
    lcd.print(p2Distance, 1);
    digitalWrite(yellowLED, HIGH); 
    playDecentSound();
  }

  stateStartTime = millis();
  gameState = FINAL_RESULT;
}

float getDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);

  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); 
  if (duration == 0) return 999; 

  return duration * 0.034 / 2;
}

void allLEDsOff() {
  digitalWrite(redLED, LOW);
  digitalWrite(yellowLED, LOW);
  digitalWrite(greenLED, LOW);
}

void blinkYellow() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(yellowLED, HIGH);
    delay(150);
    digitalWrite(yellowLED, LOW);
    delay(150);
  }
  digitalWrite(yellowLED, HIGH);
}

bool checkButtonPressed(ButtonDebouncer &btn) {
  bool reading = digitalRead(btn.pin);

  if (reading != btn.lastReading) {
    btn.lastDebounceTime = millis();
  }

  bool pressed = false;

  if ((millis() - btn.lastDebounceTime) > debounceDelay) {
    if (reading != btn.state) {
      btn.state = reading;
      if (btn.state == LOW) {
        pressed = true;
      }
    }
  }

  btn.lastReading = reading;
  return pressed;
}

void playWinSound() {
  int melody[] = {523, 659, 784, 1047}; 
  int durations[] = {150, 150, 150, 300};
  for (int i = 0; i < 4; i++) {
    tone(buzzerPin, melody[i], durations[i]);
    delay(durations[i] + 30);
  }
  noTone(buzzerPin);
}

void playLoseSound() {
  int melody[] = {400, 350, 300, 200};
  int durations[] = {150, 150, 150, 400};
  for (int i = 0; i < 4; i++) {
    tone(buzzerPin, melody[i], durations[i]);
    delay(durations[i] + 30);
  }
  noTone(buzzerPin);
}

void playDecentSound() {
  tone(buzzerPin, 500, 200);
  delay(230);
  noTone(buzzerPin);
}