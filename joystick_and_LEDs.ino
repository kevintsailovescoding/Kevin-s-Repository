#define LED1 6
#define LED2 9
#define LED3 10
#define LED4 11
#define joystickX A0
#define joystickY A1
#define joystick 7
#define SW 2
#define BUZZER 4
#define centerX 512
#define centerY 512
#define deadzone 100
#define startInterval 400 
#define minInterval 30 
#define speedUpTime 3000    
#define beepFreq 2500       
#define boomStartFreq 1200  
#define boomEndFreq 400       
#define boomDuration 300     
#define boomStepDelay 10     

unsigned long pressStartTime = 0;
bool isPressing = false;
bool ledState = false;
unsigned long lastToggleTime = 0;

void setup() {
  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);
  pinMode(6, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(joystick, INPUT_PULLUP);
  pinMode(SW, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  Serial.begin(9600);
}

void loop() {

  if (digitalRead(SW) == LOW) {
    if (!isPressing) {
      isPressing = true;
      pressStartTime = millis();
      lastToggleTime = millis();
      ledState = true;
      setAllLeds(HIGH);
      tone(BUZZER, beepFreq);
    }
    bombFlash();
    return;
  } else {
    if (isPressing) {
      isPressing = false;
      explode(); // boom sound + LEDs held on for the duration of the sound
      setAllLeds(LOW);
      noTone(BUZZER);
    }
  }

  int xVal = analogRead(joystickX);
  int yVal = analogRead(joystickY);


  int xOffset = map(xVal, 0, 1023, -centerX, centerX);
  int yOffset = map(yVal, 0, 1023, -centerY, centerY);

  bool right = xOffset > deadzone;
  bool left = xOffset < -deadzone;
  bool up = yOffset > deadzone;
  bool down = yOffset < -deadzone;

  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);

  if (right && up) {
    digitalWrite(LED1, HIGH);
  } else if (left && up) {
    digitalWrite(LED2, HIGH);
  } else if (left && down) {
    digitalWrite(LED3, HIGH);
  } else if (right && down) {
    digitalWrite(LED4, HIGH);
  }


  delay(20);
}

void bombFlash() {
  unsigned long heldTime = millis() - pressStartTime;
  float progress = (float)heldTime / (float)speedUpTime;
  if (progress > 1.0) progress = 1.0;
  unsigned long currentInterval = startInterval - (unsigned long)(progress * (startInterval - minInterval));

  if (millis() - lastToggleTime >= currentInterval) {
    lastToggleTime = millis();
    ledState = !ledState;
    setAllLeds(ledState ? HIGH : LOW);

    if (ledState) {
      tone(BUZZER, beepFreq); 
    } else {
      noTone(BUZZER);         
    }
  }
}

// Explosion effect: all LEDs snap on and stay solidly lit while a
// descending "boom" tone plays, then both cut off together at the end.
void explode() {
  setAllLeds(HIGH); // LEDs stay on for the entire boom

  int numSteps = boomDuration / boomStepDelay;
  for (int i = 0; i < numSteps; i++) {
    // Linearly interpolate frequency from boomStartFreq down to boomEndFreq
    float progress = (float)i / (float)(numSteps - 1);
    int currentFreq = boomStartFreq - (int)(progress * (boomStartFreq - boomEndFreq));

    tone(BUZZER, currentFreq);
    delay(boomStepDelay);
  }

  noTone(BUZZER);
  // setAllLeds(LOW) and noTone are also called right after this returns,
  // in the release-handling block in loop()
}

void setAllLeds(int state) {
  digitalWrite(LED1, state);
  digitalWrite(LED2, state);
  digitalWrite(LED3, state);
  digitalWrite(LED4, state);
}