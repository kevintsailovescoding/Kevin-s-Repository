#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

byte testChar[8] = {
  0b11101,
  0b00101,
  0b11111,
  0b10100,
  0b10111,
  0b00000,
  0b00000,
  0b00000
};

#define flashInterval 400 // ms between on/off toggles - adjust for speed

bool charVisible = true;
unsigned long lastFlashTime = 0;

void setup() {
  lcd.init();
  lcd.backlight();

  lcd.createChar(0, testChar);

  lcd.setCursor(0, 0);
  lcd.print("HI  ");
  lcd.print(" I'm gae");

  lastFlashTime = millis();
}

void loop() {
  if (millis() - lastFlashTime >= flashInterval) {
    lastFlashTime = millis();
    charVisible = !charVisible;

    lcd.setCursor(3, 0); // same position as before - move cursor back here each time
    if (charVisible) {
      lcd.write(byte(0)); // draw the custom character
    } else {
      lcd.print(" ");     // overwrite it with a blank space
    }
  }
}