const int ledPins[3]    = {2, 3, 4};
const int buttonPins[3] = {8, 9, 10};
const int buzzerPin     = 7;

// Score meter LEDs (3 red + 3 white), in order
const int scoreLedPins[6] = {13, 12, 11, 5, 6, A5};

// Meter level: 0-12. 0-6 = LEDs filling in at half brightness,
// 7-12 = those same LEDs upgrading to full brightness.
int meterLevel = 0;
const int maxMeterLevel = 12;

// Timing for the software "half brightness" blink (no analogWrite needed,
// works on every pin including non-PWM pins like 13, 12, A5)
const unsigned long halfBlinkPeriod = 4; // ms - full on/off cycle

int score = 0;
int activeMole = -1;
int lastMole = -1;

unsigned long gameStartTime = 0;
const unsigned long gameDuration = 17500; 
bool gameOver = false;

// Per-mole reaction timer
unsigned long moleSpawnTime = 0;
const unsigned long moleTimeLimit = 355; // reaction window per mole

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

  randomSeed(analogRead(A0));
  Serial.println("Whack-a-Mole! Press any button to start.");

  attractAndStart();
}

void loop() {
  // Keep the half-lit LEDs blinking every cycle, regardless of game state
  refreshScoreLeds();

  if (gameOver) {
    checkForRestart();
    return;
  }

  // Check if overall game time is up
  if (millis() - gameStartTime > gameDuration) {
    loseGame("Time's up! You didn't fill the meter.");
    return;
  }

  // Check if the player took too long to hit the current mole
  if (millis() - moleSpawnTime > moleTimeLimit) {
    loseGame("Too slow! You didn't react in time.");
    return;
  }

  // Check buttons - any wrong press, or any press after the window,
  // ends the game immediately. Only a correct first-try hit continues.
  for (int i = 0; i < 3; i++) {
    if (digitalRead(buttonPins[i]) == LOW) {
      if (i == activeMole) {
        score++;
        Serial.print("Hit! Score: ");
        Serial.println(score);
        hitEffect(i);
        addMeterLevel();

        if (meterLevel >= maxMeterLevel) {
          winGame();
          return;
        }

        delay(150); // debounce
        spawnMole(); // new random LED
      } else {
        Serial.println("Wrong button! No second chances.");
        wrongButtonEffect(); // buzzer sounds, but LED stays lit
        loseGame("Wrong button - you only get one try!");
        return;
      }
      return;
    }
  }
}

// --- Attract mode (idle screen) + countdown ---

// Rotates all 9 LEDs (3 mole + 6 score) with a looping arcade jingle
// until any button is pressed, then runs a 3-2-1 countdown, then starts.
void attractAndStart() {
  const int attractPins[9] = {
    ledPins[0], ledPins[1], ledPins[2],
    scoreLedPins[0], scoreLedPins[1], scoreLedPins[2],
    scoreLedPins[3], scoreLedPins[4], scoreLedPins[5]
  };
  const int jingleNotes[9] = {659, 784, 880, 784, 659, 587, 659, 784, 1047};
  const unsigned long stepInterval = 150; // ms per rotation step
  const int jingleNoteDuration = 120;

  setAllLeds(LOW);

  int current = 0;
  digitalWrite(attractPins[current], HIGH);
  tone(buzzerPin, jingleNotes[current], jingleNoteDuration);
  unsigned long lastStepTime = millis();

  while (true) {
    // Exit attract mode the instant any button is pressed
    for (int i = 0; i < 3; i++) {
      if (digitalRead(buttonPins[i]) == LOW) {
        noTone(buzzerPin);
        setAllLeds(LOW);
        delay(200); // debounce
        countdownSequence();
        startGame();
        return;
      }
    }

    if (millis() - lastStepTime >= stepInterval) {
      lastStepTime = millis();
      digitalWrite(attractPins[current], LOW);
      current = (current + 1) % 9;
      digitalWrite(attractPins[current], HIGH);
      tone(buzzerPin, jingleNotes[current], jingleNoteDuration);
    }
  }
}

// 3-2-1 countdown with rising beep pitch and a flash on every LED,
// giving the player a fair, predictable moment before the first mole appears.
void countdownSequence() {
  const int beepFreqs[3] = {392, 523, 659}; // G4, C5, E5 - rising pitch
  for (int i = 0; i < 3; i++) {
    int count = 3 - i;
    Serial.println(count);
    setAllLeds(HIGH);
    tone(buzzerPin, beepFreqs[i], 200);
    delay(400);
    setAllLeds(LOW);
    delay(400);
  }
  Serial.println("GO!");
  tone(buzzerPin, 988, 250); // B5 - "go" tone
  delay(300);
  noTone(buzzerPin);
}

void startGame() {
  score = 0;
  gameOver = false;
  lastMole = -1;
  meterLevel = 0;
  gameStartTime = millis();
  spawnMole();
}

void spawnMole() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(ledPins[i], LOW);
  }

  // Pick a random LED that's different from the last one
  int newMole;
  do {
    newMole = random(0, 3);
  } while (newMole == lastMole);

  activeMole = newMole;
  lastMole = newMole;
  digitalWrite(ledPins[activeMole], HIGH);

  moleSpawnTime = millis(); // reset reaction timer for this mole
}

void hitEffect(int pin) {
  for (int i = 0; i < 2; i++) {
    digitalWrite(ledPins[pin], LOW);
    delay(80);
    digitalWrite(ledPins[pin], HIGH);
    delay(80);
  }
  digitalWrite(ledPins[pin], LOW); // make sure it's off before next mole
}

void wrongButtonEffect() {
  // Short buzzer tone for pressing the wrong button - LED stays on
  tone(buzzerPin, 400, 150);
}

// --- Score meter helpers ---

void addMeterLevel() {
  if (meterLevel < maxMeterLevel) meterLevel++;
}

// Called every loop iteration. For each LED:
//   meterLevel >= i+7  -> fully lit
//   meterLevel >= i+1  -> half lit (blinked fast to look dim)
//   otherwise          -> off
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

// --- End-game states ---

void winGame() {
  gameOver = true;

  for (int i = 0; i < 3; i++) digitalWrite(ledPins[i], LOW);

  Serial.println("=====================");
  Serial.println("YOU WIN! All 6 lit up fully!");
  Serial.print("Final Score: ");
  Serial.println(score);
  Serial.println("Press any button to play again");
  Serial.println("=====================");

  // Happy ascending chime
  tone(buzzerPin, 523, 150); delay(160); // C5
  tone(buzzerPin, 659, 150); delay(160); // E5
  tone(buzzerPin, 784, 150); delay(160); // G5
  tone(buzzerPin, 1047, 300); delay(320); // C6
  noTone(buzzerPin);

  // Victory light show - all score LEDs + mole LEDs flash together
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

  // Sad descending tone
  tone(buzzerPin, 392, 250); delay(270); // G4
  tone(buzzerPin, 330, 250); delay(270); // E4
  tone(buzzerPin, 262, 400); delay(420); // C4
  noTone(buzzerPin);

  // Slow, somber blink of whatever was lit on the meter
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
      Serial.println("Restarting game...");
      attractAndStart();
      return;
    }
  }
}