#include <ESP32Servo.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PIN_SV_THUMB 13
#define PIN_SV_INDEX 14
#define PIN_SV_MIDDLE 27
#define PIN_SV_RING 26
#define PIN_SV_PINKY 25

#define PIN_SV_BAND1 18
#define PIN_SV_BAND2 19

#define PIN_FSR_THUMB 32
#define PIN_FSR_INDEX 33
#define PIN_FSR_MIDDLE 34

#define PIN_FMG1 35
#define PIN_FMG2 36
#define PIN_FMG3 39
#define PIN_FMG4 4
#define PIN_FMG5 2

#define PIN_SDA 21
#define PIN_SCL 22

#define PIN_TRIG 5
#define PIN_ECHO 23

#define PIN_DHT 17
#define PIN_ESTOP 16

#define NUM_FINGERS 5
const int SERVO_PIN[NUM_FINGERS] =
{ PIN_SV_THUMB, PIN_SV_INDEX, PIN_SV_MIDDLE, PIN_SV_RING, PIN_SV_PINKY };
const char* FNAME[NUM_FINGERS] = { "Thumb", "Index", "Middle", "Ring", "Pinky" };

#define NUM_FSR 3
const int FSR_PIN[NUM_FSR] = { PIN_FSR_THUMB, PIN_FSR_INDEX, PIN_FSR_MIDDLE };
const int FSR_FINGER[NUM_FSR] = { 0, 1, 2 };
int fsrOfFinger[NUM_FINGERS] = { 0, 1, 2, -1, -1 };

#define NUM_FMG 5
const int FMG_PIN[NUM_FMG] = { PIN_FMG1, PIN_FMG2, PIN_FMG3, PIN_FMG4, PIN_FMG5 };

int OPEN_US[NUM_FINGERS] = { 1000, 1000, 1000, 1000, 1000 };
int CLOSED_US[NUM_FINGERS] = { 2000, 2000, 2000, 2000, 2000 };
const int MIN_PULSE_US = 900, MAX_PULSE_US = 2100;

const bool CONTINUOUS_ROTATION_SERVOS = true;

const int CR_CLOSE_DEG = 270;
const int CR_OPEN_DEG = 270;

const float CR_SPEED_DEG_PER_S = 300.0f;

const int CR_STOP_US = 1500;
const int CR_MAX_OFFSET = 280;

int CR_STOP_TRIM_US[NUM_FINGERS] = { 0, 0, 0, 0, 0 };

const uint32_t MOVE_DURATION_MS = 1400;

const uint32_t FINGER_STAGGER_MS = 45;

const uint32_t TICK_MS = 20;

const int PULSE_DEADBAND_US = 2;

const uint32_t MOVE_TIMEOUT_MS = 6000;

const bool USE_FSR_FORCE = false;

const int FSR_CONTACT = 300;
const int FSR_HOLD_MIN = 700;
const int FSR_STOP_TARGET = 1800;
const int FSR_CRUSH_LIMIT = 3200;
const uint32_t FORCE_CHECK_MS = 40;
const uint32_t FORCE_SETTLE_MS = 250;
const int RETIGHTEN_STEP_PCT = 2;
const int GRIP_MAX_PCT = 95;

const bool USE_FMG_FOR_FIT = false;

const int BAND_EMPTY_DEG = 90;
const int BAND_CLOSED_DEG = 160;
const int BAND_FIXED_DEG = BAND_CLOSED_DEG;

const int BAND_MAX_DEG = 170;
const int BAND_STEP_DEG = 2;
const uint32_t BAND_STEP_MS = 60;

const int FIT_TARGET_MIN = 900;

#define DIST_NO_ECHO (-1)
const int ARM_MIN_VALID_CM = 2;
const int ARM_PRESENT_CM = 10;
const uint32_t ARM_CONFIRM_MS = 800, ARM_GONE_MS = 1500;

const uint32_t PING_INTERVAL_MS = 20;
const unsigned long PING_TIMEOUT_US = 12000UL;

const uint32_t ARM_DROPOUT_GRACE_MS = 400;

const int FLEX_ON_DELTA = 350, FLEX_OFF_DELTA = 180, FLEX_FULL_DELTA = 1200;
const uint32_t FLEX_DEBOUNCE_MS = 120;

#define DHT_TYPE DHT11
const float HUMIDITY_ALERT = 70.0, TEMP_ALERT_C = 35.0;

const int GRIP_CLOSE_AT_CM = 3;
const int GRIP_OPEN_AT_CM = 4;

const int GRIP_MEDIAN_SAMPLES = 3;
const int GRIP_STABLE_READS = 1;

const uint32_t GRIP_MIN_INTERVAL_MS = 750;
const int NOECHO_HOLD_READS = 10;

const uint32_t SWEEP_TOTAL_MS = 1500;

const bool RELEASE_SERVOS_WHEN_IDLE = false;
const uint32_t HOLD_AFTER_MOVE_MS = 500;

const uint32_t RELEASE_LOCKOUT_MS = 10000;

const uint32_t ESTOP_CLEAR_HOLD_MS = 1500;

#define OLED_ADDR 0x3C

enum Phase { P_EMPTY, P_CLOSING, P_OCCUPIED, P_LOCKOUT, P_FAULT, P_SHUTDOWN };
Phase phase = P_EMPTY;

uint32_t fingersArrivedAt = 0;
uint32_t bandsArrivedAt = 0;
bool bandsAttached = true;

bool sweepActive = false;
uint32_t sweepStart = 0;
uint32_t lockoutUntil = 0;

enum StopWhy : uint8_t { W_NONE=0, W_TARGET=1, W_FORCE=2, W_CRUSH=3,
W_INFERRED=4, W_TIMEOUT=5, W_STOPPED=6 };
const char* WHY[] = { "-", "target", "FORCE", "CRUSH", "infer", "t/out", "stop" };

Servo finger[NUM_FINGERS], band1, band2;
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
DHT dht(PIN_DHT, DHT_TYPE);

float posPct[NUM_FINGERS] = {0}, targetPct[NUM_FINGERS] = {0};
float moveFromPct[NUM_FINGERS] = {0};
uint32_t moveStartAt[NUM_FINGERS] = {0};
uint32_t moveDurMs[NUM_FINGERS] = {0};
int lastPulseUs[NUM_FINGERS] = {0};
bool forceStopped[NUM_FINGERS] = {false};
uint8_t stopWhy[NUM_FINGERS] = {W_NONE};
bool fingersAttached = false;

int band1Deg = BAND_EMPTY_DEG, band2Deg = BAND_EMPTY_DEG;
int bandTargetDeg = BAND_EMPTY_DEG;

int fmgBaseline[NUM_FMG] = {0};
bool baselineValid = false;

bool gripping = false, handClosed = false, shutdownLatched = false;
uint8_t gripTarget = 0, slipEvents = 0;
float hum = 0, tempC = 0;
bool sweatAlert = false;
long dist = -1;
const char* faultMsg = "";

uint32_t lastTick=0, lastForce=0, lastMoveStart=0, lastBandStep=0;
uint32_t lastDht=0, lastOled=0, lastFlexEdge=0, lastPrint=0;
uint32_t armSeenAt=0, armLastSeen=0, btnDownAt=0, lastPing=0;
uint32_t lastConfirmPrint=0;

enum GripZone : uint8_t { Z_NONE = 0, Z_CLOSE = 1, Z_OPEN = 2 };
const char* ZONE_NAME[] = { "none", "CLOSE", "OPEN" };

GripZone zoneNow = Z_NONE;
GripZone zoneStable = Z_NONE;
GripZone zoneActedOn = Z_NONE;
int zoneVotes = 0;
uint32_t lastGripChange = 0;
uint32_t motionDoneAt = 0;
int noEchoRuns = 0;
bool gripWantClosed = false;

static GripZone zoneOf(long cm) {
if (cm == DIST_NO_ECHO) return Z_NONE;
if (cm <= GRIP_CLOSE_AT_CM) return Z_CLOSE;
if (cm >= GRIP_OPEN_AT_CM) return Z_OPEN;
return Z_NONE;
}
int fsrValue[NUM_FSR] = {0};

static int readAdc(int pin) {
analogRead(pin);
delayMicroseconds(150);
return analogRead(pin);
}
static int fmgAverage() {
long s = 0;
for (int i = 0; i < NUM_FMG; i++) s += readAdc(FMG_PIN[i]);
return (int)(s / NUM_FMG);
}
static int flexDelta() {
if (!baselineValid) return 0;
long s = 0;
for (int i = 0; i < NUM_FMG; i++) s += (readAdc(FMG_PIN[i]) - fmgBaseline[i]);
return (int)max(0L, s / NUM_FMG);
}
static void captureBaseline() {
long acc[NUM_FMG] = {0};
const int N = 12;
for (int n = 0; n < N; n++) {
for (int i = 0; i < NUM_FMG; i++) acc[i] += readAdc(FMG_PIN[i]);
delay(15);
}
for (int i = 0; i < NUM_FMG; i++) fmgBaseline[i] = (int)(acc[i] / N);
baselineValid = true;
Serial.print(F("[fit] FMG baseline:"));
for (int i = 0; i < NUM_FMG; i++) { Serial.print(' '); Serial.print(fmgBaseline[i]); }
Serial.println();
}
static long readDistanceCm() {
digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
digitalWrite(PIN_TRIG, LOW);
unsigned long d = pulseIn(PIN_ECHO, HIGH, PING_TIMEOUT_US);
if (d == 0) return DIST_NO_ECHO;
const long cm = (long)(d * 0.0343 / 2.0);
if (cm <= 0 || cm > 400) return DIST_NO_ECHO;
return cm;
}

long distBuf[GRIP_MEDIAN_SAMPLES];
int distBufCount = 0, distBufIdx = 0;

static void pushDistance(long cm) {
distBuf[distBufIdx] = cm;
distBufIdx = (distBufIdx + 1) % GRIP_MEDIAN_SAMPLES;
if (distBufCount < GRIP_MEDIAN_SAMPLES) distBufCount++;
}

static long medianDistance() {
if (distBufCount == 0) return DIST_NO_ECHO;
long v[GRIP_MEDIAN_SAMPLES];
for (int i = 0; i < distBufCount; i++) v[i] = distBuf[i];
for (int i = 1; i < distBufCount; i++) {
long k = v[i]; int j = i - 1;
while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
v[j + 1] = k;
}
return v[distBufCount / 2];
}

static bool armDetected(long cm) {
if (cm == DIST_NO_ECHO) return false;
if (cm < ARM_MIN_VALID_CM) return false;
return cm <= ARM_PRESENT_CM;
}

static void crStop(int i) {
if (!fingersAttached) return;
finger[i].writeMicroseconds(CR_STOP_US + CR_STOP_TRIM_US[i]);
}
static void crStopAll() {
for (int i = 0; i < NUM_FINGERS; i++) crStop(i);
}

static void crDrive(int i, float speed) {
if (!fingersAttached) return;
speed = constrain(speed, -1.0f, 1.0f);
const int us = CR_STOP_US + CR_STOP_TRIM_US[i] + (int)(speed * CR_MAX_OFFSET);
finger[i].writeMicroseconds(us);
}

static uint32_t crTravelMsFor(int degrees) {
const float effective = CR_SPEED_DEG_PER_S * 0.66f;
return (uint32_t)((degrees / effective) * 1000.0f);
}

static float easeInOut(float t) {
t = constrain(t, 0.0f, 1.0f);
return t * t * (3.0f - 2.0f * t);
}

static int pulseFor(int f, float pct) {
pct = constrain(pct, 0.0f, 100.0f);
float us = OPEN_US[f] + (CLOSED_US[f] - OPEN_US[f]) * (pct / 100.0f);
return constrain((int)(us + 0.5f), MIN_PULSE_US, MAX_PULSE_US);
}
static void attachFingers() {
if (fingersAttached) return;
for (int i = 0; i < NUM_FINGERS; i++) {
finger[i].setPeriodHertz(50);
finger[i].attach(SERVO_PIN[i], MIN_PULSE_US, MAX_PULSE_US);

const int us = pulseFor(i, posPct[i]);
finger[i].writeMicroseconds(us);
lastPulseUs[i] = us;
}
fingersAttached = true;
}
static void detachFingers() {
if (!fingersAttached) return;
for (int i = 0; i < NUM_FINGERS; i++) {
finger[i].detach();

pinMode(SERVO_PIN[i], OUTPUT);
digitalWrite(SERVO_PIN[i], LOW);
lastPulseUs[i] = 0;
}
fingersAttached = false;
}
static void beginMove(float pct) {
attachFingers();
fingersArrivedAt = 0;
motionDoneAt = 0;
const uint32_t now = millis();
const float target = constrain(pct, 0.0f, 100.0f);

for (int i = 0; i < NUM_FINGERS; i++) {
moveFromPct[i] = posPct[i];
targetPct[i] = target;

const int order = (target > posPct[i]) ? i : (NUM_FINGERS - 1 - i);

if (CONTINUOUS_ROTATION_SERVOS && fabsf(target - posPct[i]) < 0.5f) {
moveStartAt[i] = 0;
moveDurMs[i] = 0;
} else {
moveStartAt[i] = now + (uint32_t)order * FINGER_STAGGER_MS;

const bool closing = (target > posPct[i]);
moveDurMs[i] = crTravelMsFor(closing ? CR_CLOSE_DEG : CR_OPEN_DEG);
}
forceStopped[i] = false;
stopWhy[i] = W_NONE;
}
lastMoveStart = now;
}
static void freezeFinger(int f, uint8_t why) {
targetPct[f] = posPct[f]; forceStopped[f] = true; stopWhy[f] = why;
}
static void stopAllFingers(uint8_t why) {
for (int i = 0; i < NUM_FINGERS; i++) freezeFinger(i, why);
}

static void setBandTarget(int deg) {
const int d = constrain(deg, 0, BAND_MAX_DEG);
if (d != bandTargetDeg) bandsArrivedAt = 0;
bandTargetDeg = d;
}

static void restBands() {
if (!bandsAttached) {
band1.attach(PIN_SV_BAND1, 500, 2500);
band2.attach(PIN_SV_BAND2, 500, 2500);
bandsAttached = true;
}
bandTargetDeg = BAND_EMPTY_DEG;
band1Deg = band2Deg = BAND_EMPTY_DEG;
band1.write(band1Deg); band2.write(band2Deg);
bandsArrivedAt = 0;
baselineValid = false;
}
static bool bandsAtTarget() { return band1Deg == bandTargetDeg && band2Deg == bandTargetDeg; }
static bool isMovingAnyFinger() {
for (int i = 0; i < NUM_FINGERS; i++)
if (fabsf(targetPct[i] - posPct[i]) > 0.5f) return true;
return false;
}

static uint8_t averageGrip() {
float s = 0;
for (int i = 0; i < NUM_FINGERS; i++) s += posPct[i];
return (uint8_t)(s / NUM_FINGERS + 0.5f);
}

static void drawScreen() {
oled.clearDisplay();
oled.setTextColor(SSD1306_WHITE);

if (phase == P_SHUTDOWN) {
oled.setTextSize(2); oled.setCursor(10, 6); oled.println(F("STOPPED"));
oled.setTextSize(1); oled.setCursor(0, 32); oled.println(F("Emergency shutdown"));
oled.setCursor(0, 46); oled.println(F("Hold button 1.5s"));
oled.display(); return;
}
if (phase == P_FAULT) {
oled.setTextSize(2); oled.setCursor(22, 4); oled.println(F("FAULT"));
oled.setTextSize(1); oled.setCursor(0, 30); oled.println(faultMsg);
oled.setCursor(0, 46); oled.println(F("Remove arm to reset"));
oled.display(); return;
}
if (phase == P_CLOSING) {
oled.setTextSize(2); oled.setCursor(6, 14); oled.println(F("WELCOME"));
oled.setTextSize(1); oled.setCursor(10, 44); oled.println(F("Closing socket..."));
oled.display(); return;
}

oled.setTextSize(1); oled.setCursor(0, 0);
if (phase == P_EMPTY) oled.println(F("EMPTY - insert arm"));
else if (phase == P_CLOSING) oled.println(F("Closing socket..."));
else if (phase == P_LOCKOUT) oled.println(F("Released"));
else oled.println(F("OCCUPIED"));
oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);

oled.setCursor(0, 16); oled.setTextSize(2);
oled.println(sweatAlert ? F("SWEAT!") : F("Skin OK"));

oled.setTextSize(1);
oled.setCursor(0, 36); oled.printf("Humidity: %3d %%", (int)hum);
oled.setCursor(0, 46); oled.printf("Temp: %3d C", (int)tempC);
oled.setCursor(0, 56);
if (sweepActive) oled.print(F("Finger check..."));
else if (phase == P_OCCUPIED) oled.printf("%s %3d%%",
gripWantClosed ? "CLOSED" : "OPEN ", averageGrip());
else if (phase == P_LOCKOUT) oled.printf("Wait %lus", (lockoutUntil - millis()) / 1000);
else oled.printf("Bands %d deg", band1Deg);
oled.display();
}

static void raiseFault(const char* why) {
faultMsg = why;
setBandTarget(BAND_EMPTY_DEG);
stopAllFingers(W_STOPPED);
gripping = handClosed = false;
phase = P_FAULT;
Serial.printf("[FAULT] %s\n", why);
drawScreen();
}

static void releaseSocket(uint32_t now) {
sweepActive = false;
gripping = handClosed = false;
gripWantClosed = false;
zoneNow = zoneStable = zoneActedOn = Z_NONE;
zoneVotes = 0;
gripTarget = 0;
beginMove(0);
setBandTarget(BAND_EMPTY_DEG);
baselineValid = false;
lockoutUntil = now + RELEASE_LOCKOUT_MS;
phase = P_LOCKOUT;
Serial.printf("[btn] released -> bands to %d deg, ultrasonic off for %lu s\n",
BAND_EMPTY_DEG, RELEASE_LOCKOUT_MS / 1000);
}

static void enterShutdown() {
shutdownLatched = true;
sweepActive = false;
if (CONTINUOUS_ROTATION_SERVOS) {
for (int i = 0; i < NUM_FINGERS; i++) moveStartAt[i] = 0;
crStopAll();
}
setBandTarget(BAND_EMPTY_DEG);
stopAllFingers(W_STOPPED);
gripping = handClosed = false;
gripTarget = 0;
beginMove(0);
phase = P_SHUTDOWN;
Serial.println(F("[!!] EMERGENCY SHUTDOWN"));
drawScreen();
}
static void clearShutdown() {
shutdownLatched = false;
phase = P_EMPTY;
armSeenAt = 0;
Serial.println(F("[ok] shutdown cleared"));
}

void setup() {
Serial.begin(115200);
delay(300);
Serial.println(F("\n=== HandLink — single ESP32 ==="));
Serial.println(F("Fingers stop on MEASURED fingertip pressure, not on a timer."));
Serial.println(F("No radio in use, so ADC2 (GPIO 4, 2) is available."));

pinMode(PIN_TRIG, OUTPUT);
pinMode(PIN_ECHO, INPUT);
pinMode(PIN_ESTOP, INPUT_PULLUP);
analogReadResolution(12);
for (int i = 0; i < NUM_FSR; i++) pinMode(FSR_PIN[i], INPUT);
for (int i = 0; i < NUM_FMG; i++) pinMode(FMG_PIN[i], INPUT);

Wire.begin(PIN_SDA, PIN_SCL);
if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
Serial.println(F("OLED not found — check wiring, or try address 0x3D."));
} else {
oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
oled.setTextSize(1); oled.setCursor(0, 28);
oled.println(F(" HandLink starting"));
oled.display();
}

dht.begin();

ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
ESP32PWM::allocateTimer(2); ESP32PWM::allocateTimer(3);
band1.setPeriodHertz(50); band1.attach(PIN_SV_BAND1, 500, 2500);
band2.setPeriodHertz(50); band2.attach(PIN_SV_BAND2, 500, 2500);
restBands();

attachFingers();
for (int i = 0; i < NUM_FINGERS; i++) {
posPct[i] = targetPct[i] = 0;
moveStartAt[i] = 0;
}

if (CONTINUOUS_ROTATION_SERVOS) {

crStopAll();
Serial.println(F("[servo] mode: CONTINUOUS ROTATION (pulse = speed)"));
Serial.println(F("[servo] holding STOP for 3 s - fingers must be MOTIONLESS."));
Serial.println(F("[servo] if one creeps, raise/lower its CR_STOP_TRIM_US."));
const uint32_t t0 = millis();
while (millis() - t0 < 3000) { crStopAll(); delay(20); }
Serial.println(F("[servo] stop test done."));
} else {
for (int i = 0; i < NUM_FINGERS; i++)
finger[i].writeMicroseconds(pulseFor(i, 0));
Serial.println(F("[servo] mode: STANDARD POSITIONAL (pulse = angle)"));
}

Serial.println(F("[selftest] sweeping cinch servos 0 -> 60 -> 0 deg"));
for (int d = 0; d <= 60; d += 2) { band1.write(d); band2.write(d); delay(20); }
delay(400);
for (int d = 60; d >= 0; d -= 2) { band1.write(d); band2.write(d); delay(20); }
band1.write(BAND_EMPTY_DEG); band2.write(BAND_EMPTY_DEG);
Serial.println(F("[selftest] done. No movement => power/wiring, not software."));

if (!USE_FMG_FOR_FIT)
Serial.printf("[cfg] forearm sensors DISABLED - bands cinch to %d deg\n",
BAND_FIXED_DEG);
else
Serial.println(F("[cfg] forearm sensors ENABLED for fit detection"));

lastTick = millis();
Serial.printf("[cfg] bands rest at %d deg, close to %d deg\n",
BAND_EMPTY_DEG, BAND_CLOSED_DEG);
Serial.printf("Ready. Bring something within %d cm to close the socket.\n",
ARM_PRESENT_CM);
Serial.println(F("Press the button while occupied to release."));
}

void loop() {
const uint32_t now = millis();

if (digitalRead(PIN_ESTOP) == LOW) {
if (btnDownAt == 0) {
btnDownAt = now;
if (shutdownLatched) {

} else if (phase == P_OCCUPIED || phase == P_CLOSING) {
releaseSocket(now);
} else {
enterShutdown();
}
} else if (shutdownLatched && now - btnDownAt >= ESTOP_CLEAR_HOLD_MS) {
clearShutdown();
btnDownAt = 0;
while (digitalRead(PIN_ESTOP) == LOW) delay(10);
}
} else btnDownAt = 0;

if (now - lastTick >= TICK_MS) {
lastTick = now;

bool anyMoving = false;

if (CONTINUOUS_ROTATION_SERVOS) {

for (int i = 0; i < NUM_FINGERS; i++) {
if (moveStartAt[i] == 0) { crStop(i); continue; }

if (now < moveStartAt[i]) { crStop(i); anyMoving = true; continue; }

const uint32_t el = now - moveStartAt[i];
const uint32_t dur = moveDurMs[i] ? moveDurMs[i] : crTravelMsFor(CR_CLOSE_DEG);
if (el >= dur) {
crStop(i);
moveStartAt[i] = 0;
moveDurMs[i] = 0;
posPct[i] = targetPct[i];
} else {

const float t = (float)el / (float)dur;
const float env = easeInOut(t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f);
const float dir = (targetPct[i] > moveFromPct[i]) ? 1.0f : -1.0f;
crDrive(i, dir * env);
posPct[i] = moveFromPct[i] + (targetPct[i] - moveFromPct[i]) * t;
anyMoving = true;
}
}

} else {

for (int i = 0; i < NUM_FINGERS; i++) {
if (forceStopped[i]) { }
else if (now < moveStartAt[i]) { anyMoving = true; }
else {
const uint32_t el = now - moveStartAt[i];
if (el >= MOVE_DURATION_MS) {
posPct[i] = targetPct[i];
if (stopWhy[i] == W_NONE && gripping) stopWhy[i] = W_TARGET;
} else {
const float t = (float)el / (float)MOVE_DURATION_MS;
posPct[i] = moveFromPct[i] +
(targetPct[i] - moveFromPct[i]) * easeInOut(t);
anyMoving = true;
}
}
posPct[i] = constrain(posPct[i], 0.0f, 100.0f);
if (fingersAttached) {
const int us = pulseFor(i, posPct[i]);
if (abs(us - lastPulseUs[i]) >= PULSE_DEADBAND_US) {
finger[i].writeMicroseconds(us);
lastPulseUs[i] = us;
}
}
}
}

if (!anyMoving && !sweepActive) {
if (fingersArrivedAt == 0) {
fingersArrivedAt = now;
motionDoneAt = now;
Serial.println(F("[idle] fingers stopped"));
}

if (CONTINUOUS_ROTATION_SERVOS) crStopAll();
else if (RELEASE_SERVOS_WHEN_IDLE && fingersAttached &&
now - fingersArrivedAt >= HOLD_AFTER_MOVE_MS) {
detachFingers();
}
} else {
fingersArrivedAt = 0;
}

}

if (shutdownLatched) {

if (CONTINUOUS_ROTATION_SERVOS) crStopAll();
if (now - lastOled >= 300) { lastOled = now; drawScreen(); }
return;
}

if (USE_FSR_FORCE && fingersAttached && now - lastForce >= FORCE_CHECK_MS) {
lastForce = now;
int firm = 0;

for (int s = 0; s < NUM_FSR; s++) {
const int f = FSR_FINGER[s];
const int v = readAdc(FSR_PIN[s]);
fsrValue[s] = v;

if (v >= FSR_CRUSH_LIMIT) {
if (stopWhy[f] != W_CRUSH) {
freezeFinger(f, W_CRUSH);
Serial.printf("[FORCE] %s CRUSH (%d) - stopped\n", FNAME[f], v);
}

if (sweepActive) {
sweepActive = false;
for (int i = 0; i < NUM_FINGERS; i++) targetPct[i] = posPct[i];
Serial.println(F("[sweep] ABORTED - finger met resistance"));
}
firm++; continue;
}
if (!gripping || now - lastMoveStart < FORCE_SETTLE_MS) continue;

if (!forceStopped[f] && v >= FSR_STOP_TARGET) {
freezeFinger(f, W_FORCE);
Serial.printf("[FORCE] %s %d -> stop at %.0f%%\n", FNAME[f], v, posPct[f]);
}
if (v >= FSR_STOP_TARGET - 120) firm++;

if (forceStopped[f] && stopWhy[f] == W_FORCE && v < FSR_HOLD_MIN) {
const float next = min((float)GRIP_MAX_PCT, posPct[f] + RETIGHTEN_STEP_PCT);
if (next > posPct[f]) {
targetPct[f] = next; forceStopped[f] = false;
lastMoveStart = now; slipEvents++;
Serial.printf("[SLIP] %s %d -> tighten %.0f%%\n", FNAME[f], v, next);
}
}
}

if (gripping && firm >= 2) {
for (int f = 0; f < NUM_FINGERS; f++)
if (fsrOfFinger[f] == -1 && !forceStopped[f]) freezeFinger(f, W_INFERRED);
}
}

const bool usActive = (phase == P_EMPTY || phase == P_CLOSING ||
phase == P_OCCUPIED || phase == P_FAULT);
if (usActive) {
if (now - lastPing >= PING_INTERVAL_MS) {
lastPing = now;
dist = readDistanceCm();
pushDistance(dist);
if (armDetected(dist)) armLastSeen = now;
}
} else {
dist = DIST_NO_ECHO;
}

const bool armNow = usActive && (armLastSeen != 0) &&
(now - armLastSeen <= ARM_DROPOUT_GRACE_MS);

switch (phase) {

case P_EMPTY:
setBandTarget(BAND_EMPTY_DEG);

if (armNow) {
if (armSeenAt == 0) {
armSeenAt = now;
Serial.printf("[arm] contact at %ld cm - confirming...\n", dist);
}

if (now - lastConfirmPrint >= 200) {
lastConfirmPrint = now;
Serial.printf("[arm] confirming %lu/%lu ms\n",
now - armSeenAt, ARM_CONFIRM_MS);
}
if (now - armSeenAt >= ARM_CONFIRM_MS) {
Serial.printf("[arm] CONFIRMED -> CLOSING socket to %d deg\n", BAND_CLOSED_DEG);
setBandTarget(BAND_CLOSED_DEG);
phase = P_CLOSING;
}
} else if (armSeenAt != 0) {

Serial.println(F("[arm] lost - countdown reset"));
armSeenAt = 0;
}
break;

case P_CLOSING:
setBandTarget(BAND_CLOSED_DEG);
if (bandsAtTarget()) {
Serial.printf("[arm] socket closed at %d deg -> OCCUPIED\n", BAND_CLOSED_DEG);
Serial.println(F("[us] ultrasonic now OFF until release"));

sweepActive = true;
sweepStart = now;
attachFingers();
fingersArrivedAt = 0;
gripping = false;
handClosed = false;
gripWantClosed = false;
zoneNow = zoneStable = zoneActedOn = Z_NONE;
zoneVotes = 0;
noEchoRuns = 0;
lastGripChange = now;
motionDoneAt = now;
if (USE_FMG_FOR_FIT) captureBaseline();
phase = P_OCCUPIED;
}
break;

case P_OCCUPIED: {

if (sweepActive && CONTINUOUS_ROTATION_SERVOS) {
sweepActive = false;
crStopAll();
Serial.println(F("[sweep] skipped (continuous-rotation servos)"));
}
if (sweepActive) {
const uint32_t el = now - sweepStart;
if (el >= SWEEP_TOTAL_MS) {
sweepActive = false;
for (int i = 0; i < NUM_FINGERS; i++) posPct[i] = targetPct[i] = 0;
Serial.printf("[sweep] finger check complete (%lu ms)\n", SWEEP_TOTAL_MS);
} else {

const float half = SWEEP_TOTAL_MS / 2.0f;
const float raw = (el < half) ? (el / half) : (2.0f - el / half);
const float pct = easeInOut(raw) * 100.0f;
if (!fingersAttached) attachFingers();
for (int i = 0; i < NUM_FINGERS; i++) {
posPct[i] = targetPct[i] = constrain(pct, 0.0f, 100.0f);
const int us = pulseFor(i, posPct[i]);
if (abs(us - lastPulseUs[i]) >= PULSE_DEADBAND_US) {
finger[i].writeMicroseconds(us);
lastPulseUs[i] = us;
}
}
}
break;
}

if (!sweepActive) {
const long dMed = medianDistance();
GripZone z = zoneOf(dMed);

if (z == Z_NONE) {
noEchoRuns++;
if (gripWantClosed && noEchoRuns <= NOECHO_HOLD_READS) z = Z_CLOSE;
else if (noEchoRuns > NOECHO_HOLD_READS) z = Z_OPEN;
} else {
noEchoRuns = 0;
}

if (z == zoneNow) { if (zoneVotes < 1000) zoneVotes++; }
else { zoneNow = z; zoneVotes = 1; }

if (zoneVotes >= GRIP_STABLE_READS) zoneStable = z;

const bool settled = !isMovingAnyFinger();
const bool spaced = (now - lastGripChange >= GRIP_MIN_INTERVAL_MS);

if (zoneStable != Z_NONE && zoneStable != zoneActedOn && settled && spaced) {
zoneActedOn = zoneStable;
lastGripChange = now;

if (zoneStable == Z_CLOSE) {
gripWantClosed = true;
gripping = true;
gripTarget = GRIP_MAX_PCT;
beginMove(GRIP_MAX_PCT);
Serial.printf("[grip] %ld cm -> CLOSE\n", dMed);
} else {
gripWantClosed = false;
gripping = false;
gripTarget = 0;
beginMove(0);
Serial.printf("[grip] %ld cm -> OPEN\n", dMed);
}
}
}

break;
}

case P_LOCKOUT:
setBandTarget(BAND_EMPTY_DEG);
if (now >= lockoutUntil) {
Serial.println(F("[us] lockout over — detecting again"));
armSeenAt = 0;
phase = P_EMPTY;
}
break;

case P_FAULT:
setBandTarget(BAND_EMPTY_DEG);
if (now - armLastSeen > ARM_GONE_MS) { phase = P_EMPTY; armSeenAt = 0; }
break;

case P_SHUTDOWN: break;
}

if (now - lastBandStep >= BAND_STEP_MS) {
lastBandStep = now;
const bool bandsMoving = (band1Deg != bandTargetDeg) || (band2Deg != bandTargetDeg);

if (bandsMoving) {

if (!bandsAttached) {
band1.attach(PIN_SV_BAND1, 500, 2500);
band2.attach(PIN_SV_BAND2, 500, 2500);
bandsAttached = true;
}
if (band1Deg != bandTargetDeg) {
band1Deg += (bandTargetDeg > band1Deg) ? BAND_STEP_DEG : -BAND_STEP_DEG;
if (abs(band1Deg - bandTargetDeg) < BAND_STEP_DEG) band1Deg = bandTargetDeg;
band1Deg = constrain(band1Deg, 0, BAND_MAX_DEG);
band1.write(band1Deg);
}
if (band2Deg != bandTargetDeg) {
band2Deg += (bandTargetDeg > band2Deg) ? BAND_STEP_DEG : -BAND_STEP_DEG;
if (abs(band2Deg - bandTargetDeg) < BAND_STEP_DEG) band2Deg = bandTargetDeg;
band2Deg = constrain(band2Deg, 0, BAND_MAX_DEG);
band2.write(band2Deg);
}
bandsArrivedAt = 0;
} else {

if (bandsArrivedAt == 0) bandsArrivedAt = now;
if (RELEASE_SERVOS_WHEN_IDLE && bandsAttached &&
now - bandsArrivedAt >= HOLD_AFTER_MOVE_MS) {
band1.detach(); band2.detach();
pinMode(PIN_SV_BAND1, OUTPUT); digitalWrite(PIN_SV_BAND1, LOW);
pinMode(PIN_SV_BAND2, OUTPUT); digitalWrite(PIN_SV_BAND2, LOW);
bandsAttached = false;
Serial.printf("[idle] bands reached %d deg - released\n", bandTargetDeg);
}
if (!RELEASE_SERVOS_WHEN_IDLE && bandsAttached) {
band1.write(band1Deg); band2.write(band2Deg);
}
}
}

if (now - lastDht >= 2000) {
lastDht = now;
const float h = dht.readHumidity(), t = dht.readTemperature();
if (!isnan(h)) hum = h;
if (!isnan(t)) tempC = t;
const bool alert = (hum > HUMIDITY_ALERT || tempC > TEMP_ALERT_C);
if (alert && !sweatAlert) {
Serial.printf("[skin] SWEAT ALERT %.0f%%RH %.1fC\n", hum, tempC);
sweatAlert = true;
drawScreen();
}
sweatAlert = alert;
}

if (now - lastOled >= 250) { lastOled = now; drawScreen(); }

if (now - lastPrint >= 1000) {
lastPrint = now;
Serial.printf("%-8s arm=",
phase==P_EMPTY?"EMPTY":phase==P_CLOSING?"CLOSING":
phase==P_OCCUPIED?"OCCUPIED":phase==P_LOCKOUT?"LOCKOUT":
phase==P_FAULT?"FAULT":"STOP");

if (!usActive) Serial.print("US OFF ");
else if (dist == DIST_NO_ECHO) Serial.print("no echo");
else Serial.printf("%3ld cm", dist);
Serial.printf(" bands=%d/%d", band1Deg, band2Deg);
if (phase == P_OCCUPIED) {
Serial.printf(" med=%ldcm zone=%s(x%d) acted=%s grip=%s",
medianDistance(), ZONE_NAME[zoneNow], zoneVotes,
ZONE_NAME[zoneActedOn], gripWantClosed ? "CLOSED" : "open");
}
Serial.printf(" FSR %4d %4d %4d | pos",
fsrValue[0], fsrValue[1], fsrValue[2]);
for (int i = 0; i < NUM_FINGERS; i++) Serial.printf(" %3.0f", posPct[i]);
Serial.print(" why");
for (int i = 0; i < NUM_FINGERS; i++) Serial.printf(" %s", WHY[stopWhy[i]]);
Serial.println();
}
}