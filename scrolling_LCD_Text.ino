#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Most I2C backpacks use address 0x27 or 0x3F - if the display stays blank,
// run an I2C scanner sketch to find yours
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define LED1 6
#define LED2 9
#define LED3 10
#define LED4 11
#define joystickX A0
#define joystickY A1
#define joystick 7

#define centerX 512
#define centerY 512
#define deadzone 100

const char message[] = "MY NAME IS DANAN, I LIKE MEN";
int scrollPos = 0;
unsigned long lastScrollTime = 0;
const unsigned long scrollInterval = 300; 

void setup(){
  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);
  pinMode(6, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(joystick, INPUT_PULLUP);
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
}

void scrollText() {
  if (millis() - lastScrollTime >= scrollInterval) {
    lastScrollTime = millis();

    lcd.setCursor(0, 0);

    int msgLen = strlen(message);
    for (int i = 0; i < 16; i++) {
      int charIndex = (scrollPos + i) % (msgLen + 4);
      if (charIndex < msgLen) {
        lcd.print(message[charIndex]);
      } else {
        lcd.print(' ');
      }
    }

    scrollPos++;
    if (scrollPos >= msgLen + 4) {
      scrollPos = 0;
    }
  }
}

void loop(){
  int xVal = analogRead(joystickX);
  int yVal = analogRead(joystickY);

  int xOffset = map(xVal, 0, 1023, -centerX, centerX);
  int yOffset = map(yVal, 0, 1023, -centerY, centerY);

  bool right = xOffset > deadzone;
  bool left  = xOffset < -deadzone;
  bool up    = yOffset > deadzone;
  bool down  = yOffset < -deadzone;

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

  scrollText();

  delay(20);
}