/*
#define greenPin 7
void greenPhase(){
  tone(8,2000,100);
  delay(200);
}

void setup() {
  // put your setup code here, to run once:
pinMode(8, OUTPUT);
pinMode(7, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
digitalWrite(greenPin, HIGH);
delay(200);
digitalWrite(greenPin, LOW);
greenPhase();
*/

 
#define greenPin 7
#define buzzerPin 8

void beepBlink(int speed) {
  digitalWrite(greenPin, HIGH);
  tone(buzzerPin, 2000, speed);
  delay(speed);
  digitalWrite(greenPin, LOW);
  delay(speed);
}

void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
}

void loop() {
  // Phase 1: steady beeping/blinking for 5 seconds
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    beepBlink(200); // fixed speed
  }

  for (int speed = 200; speed >= 20; speed -= 10) {
    beepBlink(speed);
  }
}
 

