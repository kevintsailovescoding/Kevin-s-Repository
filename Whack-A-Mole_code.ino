// Whack-a-Mole Game with Buzzer + 60 Second Timer
// Light stays on until correctly hit, random pattern (no repeats)

const int ledPins[3]    = {2, 3, 4};
const int buttonPins[3] = {8, 9, 10};
const int buzzerPin     = 7;

int score = 0;
int activeMole = -1;
int lastMole = -1;

unsigned long gameStartTime = 0;
const unsigned long gameDuration = 60000; // 60 seconds
bool gameOver = false;

void setup() {
  Serial.begin(9600);

  for (int i = 0; i < 3; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  pinMode(buzzerPin, OUTPUT);

  randomSeed(analogRead(A0));
  Serial.println("Whack-a-Mole! Get ready...");
  delay(1000);

  startGame();
}

void loop() {
  if (gameOver) {
    checkForRestart();
    return;
  }

  // Check if 60 seconds are up
  if (millis() - gameStartTime > gameDuration) {
    endGame();
    return;
  }

  // Check buttons - LED only turns off when the CORRECT button is pressed
  for (int i = 0; i < 3; i++) {
    if (digitalRead(buttonPins[i]) == LOW) {
      if (i == activeMole) {
        score++;
        Serial.print("Hit! Score: ");
        Serial.println(score);
        hitEffect(i);
        delay(150); // debounce
        spawnMole(); // new random LED
      } else {
        Serial.println("Wrong button!");
        wrongButtonEffect(); // buzzer sounds, but LED stays lit
        delay(150); // debounce
      }
      return;
    }
  }
}

void startGame() {
  score = 0;
  gameOver = false;
  lastMole = -1;
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
  tone(buzzerPin, 200, 150);
}

void endGame() {
  gameOver = true;

  for (int i = 0; i < 3; i++) digitalWrite(ledPins[i], LOW);

  Serial.println("=====================");
  Serial.println("Time's up!");
  Serial.print("Final Score: ");
  Serial.println(score);
  Serial.println("Press any button to play again");
  Serial.println("=====================");

  tone(buzzerPin, 523, 150); delay(180);
  tone(buzzerPin, 659, 150); delay(180);
  tone(buzzerPin, 784, 250); delay(280);
  noTone(buzzerPin);

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) digitalWrite(ledPins[j], HIGH);
    delay(150);
    for (int j = 0; j < 3; j++) digitalWrite(ledPins[j], LOW);
    delay(150);
  }
}

void checkForRestart() {
  for (int i = 0; i < 3; i++) {
    if (digitalRead(buttonPins[i]) == LOW) {
      delay(200);
      Serial.println("Restarting game...");
      startGame();
      return;
    }
  }
}