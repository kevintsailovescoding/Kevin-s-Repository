const int ledPins[3]    = {2, 3, 4};
const int buttonPins[3] = {8, 9, 10};
const int buzzerPin     = 7;

const int scoreLedPins[6] = {13, 12, 11, 5, 6, A5};

int meterLevel = 0;
const int maxMeterLevel = 12;

const unsigned long halfBlinkPeriod = 4;

int score = 0;
int activeMole = -1;
int lastMole = -1;

unsigned long gameStartTime = 0;
const unsigned long gameDuration = 17500;
bool gameOver = false;

unsigned long moleSpawnTime = 0;

enum Difficulty { EASY, MEDIUM, HARD };
Difficulty difficulty = MEDIUM;

unsigned long moleTimeLimit = 500;

int middleClickCount = 0;
unsigned long middleFirstClickTime = 0;
bool middleButtonWasPressed = false;

void setMoleTimeLimitForDifficulty() {
  switch (difficulty) {
    case EASY:   moleTimeLimit = 667; break;
    case MEDIUM: moleTimeLimit = 500; break;
    case HARD:   moleTimeLimit = 333; break;
  }
}

String difficultyName() {
  switch (difficulty) {
    case EASY:   return "EASY";
    case MEDIUM: return "MEDIUM";
    case HARD:   return "HARD";
  }
  return "";
}

void cycleDifficulty() {
  difficulty = (Difficulty)((difficulty + 1) % 3);
  setMoleTimeLimitForDifficulty();

  Serial.print("Difficulty: ");
  Serial.println(difficultyName());

  tone(buzzerPin, 660, 100);
  delay(120);
  noTone(buzzerPin);

  showDifficultyOnLcd();
}

void showDifficultyOnLcd() {
  lcdSetCursor(0, 1);
  lcdPrintPadded("Mode: " + difficultyName(), 16);
}

int updateMiddleButtonClicks() {
  bool pressed = digitalRead(buttonPins[1]) == LOW;

  if (pressed && !middleButtonWasPressed) {
    unsigned long now = millis();
    if (middleClickCount == 0 || now - middleFirstClickTime > 1000) {
      middleClickCount = 1;
      middleFirstClickTime = now;
    } else {
      middleClickCount++;
    }
    middleButtonWasPressed = true;
    delay(30);

    if (middleClickCount >= 3) {
      middleClickCount = 0;
      cycleDifficulty();
      return 1;
    }
    return 0;
  }

  if (!pressed) {
    middleButtonWasPressed = false;
  }

  if (middleClickCount == 1 && millis() - middleFirstClickTime > 1000) {
    middleClickCount = 0;
    return 2;
  }

  if (middleClickCount == 2 && millis() - middleFirstClickTime > 1000) {
    middleClickCount = 0;
  }

  return 0;
}

#define I2C_SDA A4
#define I2C_SCL A3
#define LCD_ADDR 0x27

void i2c_sw_init() {
  digitalWrite(I2C_SDA, HIGH);
  digitalWrite(I2C_SCL, HIGH);
  pinMode(I2C_SDA, OUTPUT);
  pinMode(I2C_SCL, OUTPUT);
}

void i2c_delay() {
  delayMicroseconds(5);
}

void i2c_start() {
  digitalWrite(I2C_SDA, HIGH);
  digitalWrite(I2C_SCL, HIGH);
  i2c_delay();
  digitalWrite(I2C_SDA, LOW);
  i2c_delay();
  digitalWrite(I2C_SCL, LOW);
  i2c_delay();
}

void i2c_stop() {
  digitalWrite(I2C_SDA, LOW);
  i2c_delay();
  digitalWrite(I2C_SCL, HIGH);
  i2c_delay();
  digitalWrite(I2C_SDA, HIGH);
  i2c_delay();
}

bool i2c_write_byte(uint8_t b) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(I2C_SDA, (b & 0x80) ? HIGH : LOW);
    b <<= 1;
    i2c_delay();
    digitalWrite(I2C_SCL, HIGH);
    i2c_delay();
    digitalWrite(I2C_SCL, LOW);
    i2c_delay();
  }

  pinMode(I2C_SDA, INPUT_PULLUP);
  i2c_delay();
  digitalWrite(I2C_SCL, HIGH);
  i2c_delay();
  bool ack = (digitalRead(I2C_SDA) == LOW);
  digitalWrite(I2C_SCL, LOW);
  pinMode(I2C_SDA, OUTPUT);
  i2c_delay();
  return ack;
}

void lcdWrite4(uint8_t nibble, bool rs) {
  uint8_t data = (nibble & 0xF0) | (rs ? 0x01 : 0x00) | 0x08;
  i2c_start();
  i2c_write_byte(LCD_ADDR << 1);
  i2c_write_byte(data | 0x04);
  i2c_write_byte(data);
  i2c_stop();
  delayMicroseconds(50);
}

void lcdSend(uint8_t value, bool rs) {
  lcdWrite4(value & 0xF0, rs);
  lcdWrite4((value << 4) & 0xF0, rs);
}

void lcdCommand(uint8_t cmd) {
  lcdSend(cmd, false);
}

void lcdData(uint8_t data) {
  lcdSend(data, true);
}

void lcdClear() {
  lcdCommand(0x01);
  delay(2);
}

void lcdSetCursor(uint8_t col, uint8_t row) {
  uint8_t rowOffsets[] = {0x00, 0x40};
  lcdCommand(0x80 | (col + rowOffsets[row]));
}

void lcdPrint(const String &s) {
  for (unsigned int i = 0; i < s.length(); i++) {
    lcdData((uint8_t)s[i]);
  }
}

void lcdPrintPadded(const String &s, uint8_t width) {
  String out = s;
  if (out.length() > width) out = out.substring(0, width);
  while (out.length() < width) out += ' ';
  lcdPrint(out);
}

void lcdInit() {
  i2c_sw_init();
  delay(50);

  lcdWrite4(0x30, false); delay(5);
  lcdWrite4(0x30, false); delayMicroseconds(150);
  lcdWrite4(0x30, false); delayMicroseconds(150);
  lcdWrite4(0x20, false); delayMicroseconds(150);

  lcdCommand(0x28);
  lcdCommand(0x0C);
  lcdCommand(0x06);
  lcdClear();
}

void lcdShowScore() {
  lcdSetCursor(0, 0);
  lcdPrintPadded("Score: " + String(score), 16);
  lcdSetCursor(0, 1);
  lcdPrintPadded("Meter: " + String(meterLevel) + "/" + String(maxMeterLevel), 16);
}

void setup() {
  Serial.begin(9600);

  for (int i = 0; i < 3; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  pinMode(buzzerPin, OUTPUT);

  for (int i = 0; i < 6; i++) {
    pinMode(scoreLedPins[i], OUTPUT);
    digitalWrite(scoreLedPins[i], LOW);
  }

  setMoleTimeLimitForDifficulty();

  lcdInit();
  lcdSetCursor(0, 0);
  lcdPrintPadded("WHACK-A-MOLE!", 16);
  showDifficultyOnLcd();

  randomSeed(analogRead(A0));
  Serial.println("Whack-a-Mole! Press any button to start.");
  Serial.println("Click the middle button 3x within 1 second to change difficulty.");

  attractAndStart();
}

void loop() {
  refreshScoreLeds();

  if (gameOver) {
    checkForRestart();
    return;
  }

  if (millis() - gameStartTime > gameDuration) {
    loseGame("Time's up! You didn't fill the meter.");
    return;
  }

  if (millis() - moleSpawnTime > moleTimeLimit) {
    loseGame("Too slow! You didn't react in time.");
    return;
  }

  for (int i = 0; i < 3; i++) {
    if (digitalRead(buttonPins[i]) == LOW) {
      if (i == activeMole) {
        score++;
        Serial.print("Hit! Score: ");
        Serial.println(score);
        hitEffect(i);
        addMeterLevel();
        lcdShowScore();

        if (meterLevel >= maxMeterLevel) {
          winGame();
          return;
        }

        delay(150);
        spawnMole();
      } else {
        Serial.println("Wrong button! No second chances.");
        wrongButtonEffect();
        loseGame("Wrong button - you only get one try!");
        return;
      }
      return;
    }
  }
}

#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define REST     0

const int melodyNotes[] = {
  NOTE_E5, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_C5, NOTE_B4, NOTE_A4, NOTE_A4,
  NOTE_C5, NOTE_E5, NOTE_D5, NOTE_C5, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_E5,
  NOTE_C5, NOTE_A4, NOTE_A4, REST,
  NOTE_D5, NOTE_F5, NOTE_A5, NOTE_G5, NOTE_F5, NOTE_E5, NOTE_C5, NOTE_E5,
  NOTE_D5, NOTE_C5, NOTE_B4, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_E5, NOTE_C5,
  NOTE_A4, NOTE_A4, REST
};
const int melodyDurations[] = {
  400, 200, 200, 400, 200, 200, 400, 200,
  200, 400, 200, 200, 600, 200, 400, 400,
  400, 400, 400, 400,
  400, 200, 400, 200, 200, 600, 200, 400,
  200, 200, 400, 200, 200, 400, 400, 400,
  400, 400, 400
};
const int melodyLength = sizeof(melodyNotes) / sizeof(melodyNotes[0]);

void attractAndStart() {
  const int attractPins[9] = {
    ledPins[0], ledPins[1], ledPins[2],
    scoreLedPins[0], scoreLedPins[1], scoreLedPins[2],
    scoreLedPins[3], scoreLedPins[4], scoreLedPins[5]
  };
  const unsigned long ledStepInterval = 130;

  setAllLeds(LOW);

  int ledIndex = 0;
  digitalWrite(attractPins[ledIndex], HIGH);
  unsigned long lastLedStep = millis();

  int melodyIndex = -1;
  unsigned long noteStartTime = 0;
  advanceMelody(melodyIndex, noteStartTime);

  middleClickCount = 0;
  middleButtonWasPressed = false;

  while (true) {
    bool b0 = digitalRead(buttonPins[0]) == LOW;
    bool b2 = digitalRead(buttonPins[2]) == LOW;

    int middleResult = updateMiddleButtonClicks();

    if (middleResult == 2 || b0 || b2) {
      noTone(buzzerPin);
      setAllLeds(LOW);
      delay(200);
      countdownSequence();
      startGame();
      return;
    }

    if (millis() - lastLedStep >= ledStepInterval) {
      lastLedStep = millis();
      digitalWrite(attractPins[ledIndex], LOW);
      ledIndex = (ledIndex + 1) % 9;
      digitalWrite(attractPins[ledIndex], HIGH);
    }

    if (millis() - noteStartTime >= (unsigned long)melodyDurations[melodyIndex]) {
      advanceMelody(melodyIndex, noteStartTime);
    }
  }
}

void advanceMelody(int &melodyIndex, unsigned long &noteStartTime) {
  melodyIndex = (melodyIndex + 1) % melodyLength;
  noteStartTime = millis();
  if (melodyNotes[melodyIndex] == REST) {
    noTone(buzzerPin);
  } else {
    tone(buzzerPin, melodyNotes[melodyIndex]);
  }
}

void countdownSequence() {
  const int beepFreqs[3] = {392, 523, 659};
  lcdClear();
  lcdSetCursor(0, 0);
  lcdPrintPadded("Get ready!", 16);
  for (int i = 0; i < 3; i++) {
    int count = 3 - i;
    Serial.println(count);
    lcdSetCursor(0, 1);
    lcdPrintPadded(String(count) + "...", 16);
    setAllLeds(HIGH);
    tone(buzzerPin, beepFreqs[i], 200);
    delay(400);
    setAllLeds(LOW);
    delay(400);
  }
  Serial.println("GO!");
  lcdSetCursor(0, 1);
  lcdPrintPadded("GO!", 16);
  tone(buzzerPin, 988, 250);
  delay(300);
  noTone(buzzerPin);
}

void startGame() {
  score = 0;
  gameOver = false;
  lastMole = -1;
  meterLevel = 0;
  gameStartTime = millis();
  lcdClear();
  lcdShowScore();
  spawnMole();
}

void spawnMole() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(ledPins[i], LOW);
  }

  int newMole;
  do {
    newMole = random(0, 3);
  } while (newMole == lastMole);

  activeMole = newMole;
  lastMole = newMole;
  digitalWrite(ledPins[activeMole], HIGH);

  moleSpawnTime = millis();
}

void hitEffect(int pin) {
  for (int i = 0; i < 2; i++) {
    digitalWrite(ledPins[pin], LOW);
    delay(80);
    digitalWrite(ledPins[pin], HIGH);
    delay(80);
  }
  digitalWrite(ledPins[pin], LOW);
}

void wrongButtonEffect() {
  tone(buzzerPin, 400, 150);
}

void addMeterLevel() {
  if (meterLevel < maxMeterLevel) meterLevel++;
}

void refreshScoreLeds() {
  unsigned long phase = millis() % halfBlinkPeriod;
  bool halfOn = phase < (halfBlinkPeriod / 2);

  for (int i = 0; i < 6; i++) {
    if (meterLevel >= i + 7) {
      digitalWrite(scoreLedPins[i], HIGH);
    } else if (meterLevel >= i + 1) {
      digitalWrite(scoreLedPins[i], halfOn ? HIGH : LOW);
    } else {
      digitalWrite(scoreLedPins[i], LOW);
    }
  }
}

void winGame() {
  gameOver = true;

  for (int i = 0; i < 3; i++) digitalWrite(ledPins[i], LOW);

  Serial.println("=====================");
  Serial.println("YOU WIN! All 6 lit up fully!");
  Serial.print("Final Score: ");
  Serial.println(score);
  Serial.println("Press any button to play again");
  Serial.println("=====================");

  lcdClear();
  lcdSetCursor(0, 0);
  lcdPrintPadded("YOU WIN!", 16);
  lcdSetCursor(0, 1);
  lcdPrintPadded("Score: " + String(score), 16);

  tone(buzzerPin, 523, 150); delay(160);
  tone(buzzerPin, 659, 150); delay(160);
  tone(buzzerPin, 784, 150); delay(160);
  tone(buzzerPin, 1047, 300); delay(320);
  noTone(buzzerPin);

  for (int i = 0; i < 4; i++) {
    setAllLeds(HIGH);
    delay(120);
    setAllLeds(LOW);
    delay(120);
  }
}

void loseGame(String reason) {
  gameOver = true;

  for (int i = 0; i < 3; i++) digitalWrite(ledPins[i], LOW);

  Serial.println("=====================");
  Serial.println(reason);
  Serial.print("Final Score: ");
  Serial.println(score);
  Serial.println("Press any button to play again");
  Serial.println("=====================");

  lcdClear();
  lcdSetCursor(0, 0);
  lcdPrintPadded("GAME OVER", 16);
  lcdSetCursor(0, 1);
  lcdPrintPadded("Score: " + String(score), 16);

  tone(buzzerPin, 392, 250); delay(270);
  tone(buzzerPin, 330, 250); delay(270);
  tone(buzzerPin, 262, 400); delay(420);
  noTone(buzzerPin);

  for (int i = 0; i < 3; i++) {
    refreshScoreLeds();
    delay(200);
    setScoreLeds(LOW);
    delay(200);
  }
}

void setAllLeds(int state) {
  for (int i = 0; i < 3; i++) digitalWrite(ledPins[i], state);
  setScoreLeds(state);
}

void setScoreLeds(int state) {
  for (int i = 0; i < 6; i++) digitalWrite(scoreLedPins[i], state);
}

void checkForRestart() {
  bool b0 = digitalRead(buttonPins[0]) == LOW;
  bool b2 = digitalRead(buttonPins[2]) == LOW;

  int middleResult = updateMiddleButtonClicks();

  if (middleResult == 2 || b0 || b2) {
    delay(200);
    Serial.println("Restarting game...");
    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrintPadded("WHACK-A-MOLE!", 16);
    showDifficultyOnLcd();
    attractAndStart();
    return;
  }
}