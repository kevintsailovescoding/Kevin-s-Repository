#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define LCD_ADDR   0x27
#define BUTTON_PIN 2
#define BUZZER_PIN 13
#define GREEN_LED  7
#define RED_LED    8

const unsigned long DEBOUNCE_MS        = 30;

const unsigned long INITIAL_CACTUS_STEP = 260;
const unsigned long MIN_CACTUS_STEP     = 110;
const unsigned long SPEEDUP_INTERVAL    = 6000;
const unsigned long SPEEDUP_AMOUNT      = 10;

const unsigned long PTERO_START_TIME    = 15000;
const unsigned long INITIAL_PTERO_STEP  = 220;
const unsigned long MIN_PTERO_STEP      = 90;

const unsigned long SUCCESS_TONE_FREQ   = 2000;
const unsigned long SUCCESS_TONE_MS     = 100;
const unsigned long LOSE_LED_MS         = 2000;

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

byte dinoChar[8] = {
  0b00110,
  0b00110,
  0b00100,
  0b01110,
  0b11111,
  0b00100,
  0b01010,
  0b00000
};

byte cactusChar[8] = {
  0b00100,
  0b10101,
  0b10101,
  0b11111,
  0b00100,
  0b00100,
  0b00100,
  0b00000
};

byte pteroChar[8] = {
  0b00000,
  0b10001,
  0b11011,
  0b01110,
  0b00100,
  0b00000,
  0b00000,
  0b00000
};

#define DINO_CHAR    byte(0)
#define CACTUS_CHAR  byte(1)
#define PTERO_CHAR   byte(2)

enum GameState { WAITING, RUNNING, GAMEOVER };
volatile GameState state = WAITING;

const int DINO_COL = 1;
int dinoRow = 1;

int cactusCol = 16;
unsigned long lastCactusMove = 0;
unsigned long currentCactusStep = INITIAL_CACTUS_STEP;

bool pteroActive = false;
int pteroCol = 16;
int pteroRow = 0;
unsigned long lastPteroMove = 0;
unsigned long currentPteroStep = INITIAL_PTERO_STEP;

unsigned long runStartTime = 0;

bool isJumping = false;

int score = 0;

unsigned long greenLedOffAt = 0;

volatile bool buttonHeld = false;
volatile bool buttonPressedEvent = false;
volatile unsigned long lastInterruptTime = 0;

void buttonISR() {
  unsigned long now = millis();
  if (now - lastInterruptTime > DEBOUNCE_MS) {
    bool pressed = (digitalRead(BUTTON_PIN) == LOW);
    if (pressed && !buttonHeld) {
      buttonPressedEvent = true;
    }
    buttonHeld = pressed;
    lastInterruptTime = now;
  }
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, CHANGE);

  randomSeed(analogRead(A0));

  lcd.init();
  lcd.backlight();
  lcd.createChar(DINO_CHAR, dinoChar);
  lcd.createChar(CACTUS_CHAR, cactusChar);
  lcd.createChar(PTERO_CHAR, pteroChar);

  resetToStart();
}

void loop() {
  switch (state) {
    case WAITING:
      if (buttonPressedEvent) {
        buttonPressedEvent = false;
        startGame();
      }
      break;

    case RUNNING:
      updateJump();
      updateCactus();
      updatePtero();
      checkCollision();
      updateLeds();
      drawGame();
      break;

    case GAMEOVER:
      break;
  }
}

void updateLeds() {
  if (greenLedOffAt != 0 && millis() >= greenLedOffAt) {
    digitalWrite(GREEN_LED, LOW);
    greenLedOffAt = 0;
  }
}

void triggerSuccess() {
  digitalWrite(GREEN_LED, HIGH);
  tone(BUZZER_PIN, SUCCESS_TONE_FREQ, SUCCESS_TONE_MS);
  greenLedOffAt = millis() + SUCCESS_TONE_MS;
}

void resetToStart() {
  dinoRow = 1;
  isJumping = false;
  cactusCol = 16;
  pteroActive = false;
  pteroCol = 16;
  pteroRow = 0;
  score = 0;
  currentCactusStep = INITIAL_CACTUS_STEP;
  currentPteroStep = INITIAL_PTERO_STEP;
  state = WAITING;

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  greenLedOffAt = 0;

  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Dino Game");
  lcd.setCursor(0, 1);
  lcd.write(DINO_CHAR);
  lcd.print(" Hold to jump!");
}

void startGame() {
  dinoRow = 1;
  isJumping = false;
  cactusCol = 16;
  pteroActive = false;
  pteroCol = 16;
  pteroRow = 0;
  score = 0;
  currentCactusStep = INITIAL_CACTUS_STEP;
  currentPteroStep = INITIAL_PTERO_STEP;
  runStartTime = millis();
  lastCactusMove = millis();
  lastPteroMove = millis();
  state = RUNNING;
  lcd.clear();
}

void updateJump() {
  if (buttonHeld) {
    if (!isJumping) {
      isJumping = true;
      dinoRow = 0;
      tone(BUZZER_PIN, 1500, 80);
    }
  } else {
    if (isJumping) {
      isJumping = false;
      dinoRow = 1;
    }
  }
}

void updateCactus() {
  unsigned long now = millis();

  unsigned long elapsed = now - runStartTime;
  unsigned long reduction = (elapsed / SPEEDUP_INTERVAL) * SPEEDUP_AMOUNT;
  if (reduction > (INITIAL_CACTUS_STEP - MIN_CACTUS_STEP)) {
    reduction = INITIAL_CACTUS_STEP - MIN_CACTUS_STEP;
  }
  currentCactusStep = INITIAL_CACTUS_STEP - reduction;

  if (now - lastCactusMove >= currentCactusStep) {
    lastCactusMove = now;

    if (cactusCol == DINO_COL) {
      score++;
      triggerSuccess();
    }

    cactusCol--;
    if (cactusCol < 0) {
      cactusCol = 16;
    }
  }
}

void updatePtero() {
  unsigned long now = millis();
  unsigned long elapsed = now - runStartTime;

  if (!pteroActive) {
    if (elapsed >= PTERO_START_TIME) {
      pteroActive = true;
      pteroCol = 16;
      pteroRow = random(0, 2);
      lastPteroMove = now;
    }
    return;
  }

  unsigned long sinceStart = elapsed - PTERO_START_TIME;
  unsigned long reduction = (sinceStart / SPEEDUP_INTERVAL) * SPEEDUP_AMOUNT;
  if (reduction > (INITIAL_PTERO_STEP - MIN_PTERO_STEP)) {
    reduction = INITIAL_PTERO_STEP - MIN_PTERO_STEP;
  }
  currentPteroStep = INITIAL_PTERO_STEP - reduction;

  if (now - lastPteroMove >= currentPteroStep) {
    lastPteroMove = now;

    if (pteroCol == DINO_COL) {
      score++;
      triggerSuccess();
    }

    pteroCol--;
    if (pteroCol < 0) {
      pteroCol = 16;
      pteroRow = random(0, 2);
    }
  }
}

void checkCollision() {
  if (cactusCol == DINO_COL && dinoRow == 1) {
    loseGame();
    return;
  }
  if (pteroActive && pteroCol == DINO_COL && dinoRow == pteroRow) {
    loseGame();
  }
}

void drawGame() {
  char row0[17];
  char row1[17];
  for (int i = 0; i < 16; i++) {
    row0[i] = ' ';
    row1[i] = '_';
  }
  row0[16] = '\0';
  row1[16] = '\0';

  if (dinoRow == 0) row0[DINO_COL] = 3;
  else               row1[DINO_COL] = 3;

  if (cactusCol >= 0 && cactusCol < 16) {
    row1[cactusCol] = 4;
  }

  if (pteroActive && pteroCol >= 0 && pteroCol < 16) {
    if (pteroRow == 0) row0[pteroCol] = 5;
    else               row1[pteroCol] = 5;
  }

  char scoreBuf[6];
  snprintf(scoreBuf, sizeof(scoreBuf), "%d", score);
  int len = strlen(scoreBuf);
  int startCol = 16 - len;
  if (startCol < 0) startCol = 0;
  for (int i = 0; i < len && (startCol + i) < 16; i++) {
    if (row0[startCol + i] == ' ') {
      row0[startCol + i] = scoreBuf[i];
    }
  }

  lcd.setCursor(0, 0);
  for (int i = 0; i < 16; i++) {
    if (row0[i] == 3)      lcd.write(DINO_CHAR);
    else if (row0[i] == 5) lcd.write(PTERO_CHAR);
    else                    lcd.print(row0[i]);
  }

  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; i++) {
    if (row1[i] == 3)      lcd.write(DINO_CHAR);
    else if (row1[i] == 4) lcd.write(CACTUS_CHAR);
    else if (row1[i] == 5) lcd.write(PTERO_CHAR);
    else                    lcd.print(row1[i]);
  }
}

void loseGame() {
  state = GAMEOVER;

  digitalWrite(GREEN_LED, LOW);
  greenLedOffAt = 0;
  digitalWrite(RED_LED, HIGH);

  tone(BUZZER_PIN, 400, 150);
  delay(150);
  tone(BUZZER_PIN, 300, 150);
  delay(150);
  tone(BUZZER_PIN, 150, 300);
  delay(300);
  noTone(BUZZER_PIN);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   GAME OVER!   ");
  lcd.setCursor(0, 1);
  char buf[17];
  snprintf(buf, sizeof(buf), "   Score: %d", score);
  lcd.print(buf);

  delay(LOSE_LED_MS);
  digitalWrite(RED_LED, LOW);

  buttonPressedEvent = false;
  resetToStart();
}