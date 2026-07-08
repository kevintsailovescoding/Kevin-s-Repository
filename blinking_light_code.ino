#define ledPin 8
#define interval 75
int ledState = LOW;
unsigned long time = millis();
unsigned long previous = 0;

void setup() {
  //put your setup code here, to run once:
  pinMode(ledPin, OUTPUT);
}
void loop() {
  time = millis();
  if (time - previous >= interval) {
    previous = time;
    if (ledState == LOW){
      ledState = HIGH;
    }
    else{
      ledState = LOW;
    }
digitalWrite(ledPin, ledState);
  }
}
