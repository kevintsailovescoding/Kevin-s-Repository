#define LED1 6
#define LED2 9
#define LED3 10
#define LED4 11
#define joystickX A0
#define joystickY A1
#define joystick 7
#define SW 2
#define centerX 512
#define centerY 512
#define deadzone 100

void setup() {
  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);
  pinMode(6, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(joystick, INPUT_PULLUP);
  pinMode(SW, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop() {

  if (digitalRead(SW) == LOW) {
    flashAllLeds();
    return;
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

void flashAllLeds() {
  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, HIGH);
  digitalWrite(LED3, HIGH);
  digitalWrite(LED4, HIGH);
  delay(43);

  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
  delay(43);
}