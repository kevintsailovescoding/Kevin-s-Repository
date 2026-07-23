const int ledPins[3] = {2, 3, 4};
const int buttonPins[3] = {8, 9, 10};
const int buzzerPin = 7;
 
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
 
int selectedButton = -1;
bool awaitingConfirm = false;
unsigned long selectFirstClickTime = 0;
bool selectButtonWasPressed[3] = {false, false, false};
const unsigned long confirmWindow = 1000;
 
const String titleTopMsg = "Kevin and Danan's Whack-a-Mole";
const String titleBottomMsg = "joe";
const unsigned long titleTopStep = 675; 
const unsigned long titleBottomStep = 675; 
 
void setMoleTimeLimitForDifficulty() {
  switch (difficulty) {
    case EASY: moleTimeLimit = 667; break;
    case MEDIUM: moleTimeLimit = 500; break;
    case HARD: moleTimeLimit = 333; break;
  }
}
 
String difficultyName() {
  switch (difficulty) {
    case EASY: return "EASY";
    case MEDIUM: return "MEDIUM";
    case HARD: return "HARD";
  }
  return "";
}
 
Difficulty difficultyForButton(int i) {
  switch (i) {
    case 0: return EASY;
    case 1: return MEDIUM;
    case 2: return HARD;
  }
  return MEDIUM;
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
  lcdPrintPadded("Level: " + difficultyName(), 16);
}
 
// Returns a 16-char window into a message that wraps around with a gap,
// creating a continuous scrolling marquee effect.
String marqueeWindow(const String &msg, int pos) {
  String s = msg + " "; // gap between end and restart
  int len = s.length();
  String out = "";
  for (int i = 0; i < 16; i++) {
    out += s[(pos + i) % len];
  }
  return out;
}
 
// Title / start screen: two independently-scrolling rows.
// Top row shows the game title; bottom row shows the how-to-play manual.
// Returns as soon as the player presses any button.
void titleScreen() {
  setAllLeds(LOW);
  lcdClear();
 
  int topPos = 0;
  int bottomPos = 0;
  int topLen = titleTopMsg.length() + 4;
  int bottomLen = titleBottomMsg.length() + 4;
 
  unsigned long lastTopStep = millis();
  unsigned long lastBottomStep = millis();
 
  // Gentle LED chase across the mole LEDs for flair.
  int chaseIndex = 0;
  unsigned long lastChase = millis();
  const unsigned long chaseInterval = 200;
 
  // Draw initial frame.
  lcdSetCursor(0, 0);
  lcdPrint(marqueeWindow(titleTopMsg, topPos));
  lcdSetCursor(0, 1);
  lcdPrint(marqueeWindow(titleBottomMsg, bottomPos));
  digitalWrite(ledPins[chaseIndex], HIGH);
 
  while (true) {
    // Any button press begins the game -> go to difficulty selection.
    for (int i = 0; i < 3; i++) {
      if (digitalRead(buttonPins[i]) == LOW) {
        tone(buzzerPin, 660, 120);
        setAllLeds(LOW);
        // Wait for release so this press isn't read as a level selection.
        while (digitalRead(buttonPins[i]) == LOW) { /* hold */ }
        delay(50);
        noTone(buzzerPin);
        return;
      }
    }
 
    if (millis() - lastTopStep >= titleTopStep) {
      lastTopStep = millis();
      topPos = (topPos + 1) % topLen;
      lcdSetCursor(0, 0);
      lcdPrint(marqueeWindow(titleTopMsg, topPos));
    }
 
    if (millis() - lastBottomStep >= titleBottomStep) {
      lastBottomStep = millis();
      bottomPos = (bottomPos + 1) % bottomLen;
      lcdSetCursor(0, 1);
      lcdPrint(marqueeWindow(titleBottomMsg, bottomPos));
    }
 
    if (millis() - lastChase >= chaseInterval) {
      lastChase = millis();
      digitalWrite(ledPins[chaseIndex], LOW);
      chaseIndex = (chaseIndex + 1) % 3;
      digitalWrite(ledPins[chaseIndex], HIGH);
    }
  }
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
 
  randomSeed(analogRead(A0));
  Serial.println("Whack-a-Mole!");
  Serial.println("Press any button at the title screen to begin.");
  Serial.println("Blue=Easy Yellow=Medium Green=Hard - press twice within 1s to confirm & start.");
 
  titleScreen();
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
 
#define NOTE_A4 440
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_D5 587
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_G5 784
#define NOTE_A5 880
#define REST 0
 
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
 
void showSelectPrompt() {
  if (!awaitingConfirm) {
    lcdSetCursor(0, 0);
    lcdPrintPadded("Pick your level", 16);
    lcdSetCursor(0, 1);
    lcdPrintPadded("Easy, Med, Hard", 16);
  } else {
    lcdSetCursor(0, 0);
    lcdPrintPadded(difficultyName() + " selected", 16);
    lcdSetCursor(0, 1);
    lcdPrintPadded("Press again: GO!", 16);
  }
}
 
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
 
  selectedButton = -1;
  awaitingConfirm = false;
  for (int i = 0; i < 3; i++) selectButtonWasPressed[i] = false;
  lcdClear();
  showSelectPrompt();
 
  while (true) {
    if (awaitingConfirm && millis() - selectFirstClickTime > confirmWindow) {
      awaitingConfirm = false;
      selectedButton = -1;
      showSelectPrompt();
    }
 
    for (int i = 0; i < 3; i++) {
      bool pressed = digitalRead(buttonPins[i]) == LOW;
 
      if (pressed && !selectButtonWasPressed[i]) {
        selectButtonWasPressed[i] = true;
        delay(30); // debounce
 
        if (awaitingConfirm && selectedButton == i) {
          noTone(buzzerPin);
          setAllLeds(LOW);
          delay(200);
          countdownSequence();
          startGame();
          return;
        } else {
          selectedButton = i;
          difficulty = difficultyForButton(i);
          setMoleTimeLimitForDifficulty();
          awaitingConfirm = true;
          selectFirstClickTime = millis();
 
          tone(buzzerPin, 660, 100);
 
          Serial.print("Level selected: ");
          Serial.println(difficultyName());
          Serial.println("Press the same button again within 1s to start.");
 
          showSelectPrompt();
        }
      } else if (!pressed) {
        selectButtonWasPressed[i] = false;
      }
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
  for (int i = 0; i < 3; i++) {
    if (digitalRead(buttonPins[i]) == LOW) {
      delay(200);
      Serial.println("Restarting...");
      titleScreen();
      attractAndStart();
      return;
    }
  }
}