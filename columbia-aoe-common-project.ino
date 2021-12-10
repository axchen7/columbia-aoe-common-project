#include <Servo.h>

typedef enum { PLAY, P1_WIN, P2_WIN, P1_LOSE, P2_LOSE, RESET } State;
typedef enum { RED, GREEN } Color;

// ports

int SERVO_1 = 3;
int SERVO_2 = 10;

int POT_1 = A0;
int POT_2 = A1;

int BEAM_1 = 6;
int BEAM_2 = 7;

int BUTTON = 8;

int LED_R = 11;
int LED_Y = 12;
int LED_G = 13;

// constants

int LOOP_DELAY_MS = 20;

unsigned long RED_DURATION_MS = 1500;
unsigned long YELLOW_DURATION_MS = 500;
unsigned long GREEN_DURATION_MIN_MS = 1200;
unsigned long GREEN_DURATION_MAX_MS = 2500;

unsigned long LED_CYCLE_DURATION_MS = 300;
unsigned long LED_BLINK_DURATION_MS = 600;

unsigned long LOSE_SERVO_FORWARD_DURATION_MS = 1200;
unsigned long WIN_SERVO_BACK_DURATION_MS = 400;

int POT_SAMPLE_COUNT = 5;
int BEAM_SAMPLE_COUNT = 3;
float EMA_COEFF = 0.1;
float SERVO_STILL_THRESH = 0.1;

float POT_1_UP = 591;
float POT_1_DOWN = 533;

float POT_2_UP = 441;
float POT_2_DOWN = 517;

float SERVO_PLAYER_FORWARD_POWER = 0.15;
float SERVO_LOSE_FORWARD_POWER = 0.3;
float SERVO_BACKWARD_POWER = 0.3;

// servos

Servo servo1;
Servo servo2;

// global variables

unsigned long timeMs;

State state;
unsigned long stateSetMs;

Color color;
unsigned long colorChangeAtMs;

bool buttonPressed;
bool beam1Broken;
bool beam2Broken;
float pot1Value;
float pot2Value;
float servo1PowerRaw;
float servo2PowerRaw;

float* beam1Raws = (float*)calloc(BEAM_SAMPLE_COUNT, sizeof(float));
float* beam2Raws = (float*)calloc(BEAM_SAMPLE_COUNT, sizeof(float));

float* pot1Raws = (float*)calloc(POT_SAMPLE_COUNT, sizeof(float));
float* pot2Raws = (float*)calloc(POT_SAMPLE_COUNT, sizeof(float));

float prevPot1Raw = POT_1_UP;
float prevPot2Raw = POT_2_UP;

void setup() {
  Serial.begin(9600);

  servo1.attach(SERVO_1);
  servo2.attach(SERVO_2);

  pinMode(BEAM_1, INPUT_PULLUP);
  pinMode(BEAM_2, INPUT_PULLUP);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_Y, OUTPUT);
  pinMode(LED_G, OUTPUT);

  timeMs = millis();

  resetBeamFilter();
  resetPotFilter();

  setState(PLAY);
  setColor(RED);
}

void loop() {
  timeMs = millis();

  readSensors();

  updateState();
  updateColor();

  setServoPowers();

  logDebugInfo();

  delay(LOOP_DELAY_MS);
}

void setServoPower(Servo servo, float power, bool flip) {
  servo.write(90 + power * (flip ? -90 : 90));
}

float clip(float power, float low, float high) {
  if (power > high) return high;
  if (power < low) return low;
  return power;
}

void fillArray(float* arr, int len, float val) {
  for (int i = 0; i < len; i++) {
    arr[i] = val;
  }
}

void pushArray(float* arr, int len, float val) {
  for (int i = len - 1; i > 0; i--) {
    arr[i] = arr[i - 1];
  }

  arr[0] = val;
}

float closestInArray(float* arr, int len, float val) {
  float closest = arr[0];
  float delta = abs(val - arr[0]);

  for (int i = 1; i < len; i++) {
    float newDelta = abs(val - arr[i]);
    if (newDelta < delta) {
      closest = arr[i];
      delta = newDelta;
    }
  }

  return closest;
}

float countArray(float* arr, int len, float val) {
  float count = 0;
  for (int i = 0; i < len; i++) {
    if (arr[i] == val) count++;
  }
  return count;
}

void resetBeamFilter() {
  fillArray(beam1Raws, BEAM_SAMPLE_COUNT, 0);
  fillArray(beam2Raws, BEAM_SAMPLE_COUNT, 0);
}

void resetPotFilter() {
  fillArray(pot1Raws, POT_SAMPLE_COUNT, POT_1_UP);
  fillArray(pot2Raws, POT_SAMPLE_COUNT, POT_2_UP);
  prevPot1Raw = POT_1_UP;
  prevPot2Raw = POT_2_UP;
}

void readSensors() {
  buttonPressed = digitalRead(BUTTON) == LOW;

  pushArray(beam1Raws, BEAM_SAMPLE_COUNT, digitalRead(BEAM_1));
  pushArray(beam2Raws, BEAM_SAMPLE_COUNT, digitalRead(BEAM_2));

  beam1Broken =
      countArray(beam1Raws, BEAM_SAMPLE_COUNT, LOW) == BEAM_SAMPLE_COUNT;
  beam2Broken =
      countArray(beam2Raws, BEAM_SAMPLE_COUNT, LOW) == BEAM_SAMPLE_COUNT;

  pushArray(pot1Raws, POT_SAMPLE_COUNT, analogRead(POT_1));
  pushArray(pot2Raws, POT_SAMPLE_COUNT, analogRead(POT_2));

  pot1Value = closestInArray(pot1Raws, POT_SAMPLE_COUNT, POT_1_UP);
  pot2Value = closestInArray(pot2Raws, POT_SAMPLE_COUNT, POT_2_UP);

  pot1Value = pot1Value * EMA_COEFF + (1 - EMA_COEFF) * prevPot1Raw;
  pot2Value = pot2Value * EMA_COEFF + (1 - EMA_COEFF) * prevPot2Raw;

  prevPot1Raw = pot1Value;
  prevPot2Raw = pot2Value;

  servo1PowerRaw = (pot1Value - POT_1_UP) / (POT_1_DOWN - POT_1_UP);
  servo2PowerRaw = (pot2Value - POT_2_UP) / (POT_2_DOWN - POT_2_UP);

  servo1PowerRaw = clip(servo1PowerRaw, 0, 1);
  servo2PowerRaw = clip(servo2PowerRaw, 0, 1);
}

void setState(State newState) {
  state = newState;
  stateSetMs = timeMs;

  resetBeamFilter();
  resetPotFilter();
  setColor(RED);
}

void setColor(Color newColor) {
  color = newColor;

  if (color == RED) {
    colorChangeAtMs = timeMs + RED_DURATION_MS;
  } else {
    colorChangeAtMs =
        timeMs + random(GREEN_DURATION_MIN_MS, GREEN_DURATION_MAX_MS);
  }
}

void updateState() {
  if (state != RESET && buttonPressed) {
    setState(RESET);
    return;
  }

  switch (state) {
    case PLAY:
      if (beam1Broken) {
        setState(P1_WIN);
        return;
      }

      if (beam2Broken) {
        setState(P2_WIN);
        return;
      }

      if (color == RED) {
        if (servo1PowerRaw > SERVO_STILL_THRESH)
          setState(P1_LOSE);
        else if (servo2PowerRaw > SERVO_STILL_THRESH)
          setState(P2_LOSE);
      }

      break;

    case RESET:
      if (!buttonPressed) setState(PLAY);
      break;
  }
}

int getLedCycle(int n, unsigned long durationMs) {
  unsigned long curPeriodMs = timeMs % (durationMs * n);
  return curPeriodMs / durationMs;
}

int ledWrite(bool r, bool y, bool g) {
  digitalWrite(LED_R, r ? HIGH : LOW);
  digitalWrite(LED_Y, y ? HIGH : LOW);
  digitalWrite(LED_G, g ? HIGH : LOW);
}

void updateColor() {
  switch (state) {
    case PLAY:
      if (timeMs > colorChangeAtMs) {
        if (color == RED)
          setColor(GREEN);
        else
          setColor(RED);
      }

      if (color == RED) {
        ledWrite(1, 0, 0);
      } else {
        if (timeMs > colorChangeAtMs - YELLOW_DURATION_MS)
          ledWrite(0, 1, 0);
        else
          ledWrite(0, 0, 1);
      }

      break;

    case P1_WIN:
    case P2_WIN:
      switch (getLedCycle(3, LED_CYCLE_DURATION_MS)) {
        case 0:
          ledWrite(1, 0, 0);
          break;
        case 1:
          ledWrite(0, 1, 0);
          break;
        case 2:
          ledWrite(0, 0, 1);
          break;
      }
      break;

    case P1_LOSE:
    case P2_LOSE:
      if (getLedCycle(2, LED_BLINK_DURATION_MS) == 0)
        ledWrite(1, 0, 0);
      else
        ledWrite(0, 0, 0);
      break;

    case RESET:
      if (getLedCycle(2, LED_BLINK_DURATION_MS) == 0)
        ledWrite(0, 1, 0);
      else
        ledWrite(0, 0, 0);
      break;
  }
}

void setServoPowers() {
  switch (state) {
    case PLAY: {
      float servo1Power = servo1PowerRaw * SERVO_PLAYER_FORWARD_POWER;
      float servo2Power = servo2PowerRaw * SERVO_PLAYER_FORWARD_POWER;
      setServoPower(servo1, servo1Power, true);
      setServoPower(servo2, servo2Power, false);
      break;
    }

    case RESET: {
      float servo1Power = servo1PowerRaw * -SERVO_BACKWARD_POWER;
      float servo2Power = servo2PowerRaw * -SERVO_BACKWARD_POWER;
      setServoPower(servo1, servo1Power, true);
      setServoPower(servo2, servo2Power, false);
      break;
    }

    case P1_WIN:
      setServoPower(servo1, 0, true);
      if (timeMs < stateSetMs + WIN_SERVO_BACK_DURATION_MS)
        setServoPower(servo2, -SERVO_BACKWARD_POWER, false);
      else
        setServoPower(servo2, 0, false);
      break;

    case P2_WIN:
      setServoPower(servo2, 0, false);
      if (timeMs < stateSetMs + WIN_SERVO_BACK_DURATION_MS)
        setServoPower(servo1, -SERVO_BACKWARD_POWER, true);
      else
        setServoPower(servo1, 0, true);
      break;

    case P1_LOSE:
      setServoPower(servo1, 0, true);
      if (timeMs < stateSetMs + LOSE_SERVO_FORWARD_DURATION_MS)
        setServoPower(servo2, SERVO_LOSE_FORWARD_POWER, false);
      else
        setServoPower(servo2, 0, false);
      break;

    case P2_LOSE:
      setServoPower(servo2, 0, false);
      if (timeMs < stateSetMs + LOSE_SERVO_FORWARD_DURATION_MS)
        setServoPower(servo1, SERVO_LOSE_FORWARD_POWER, true);
      else
        setServoPower(servo1, 0, true);
      break;
  }
}

void logDebugInfo() {
  // Serial.print("pot 1: ");
  // Serial.print(pot1Value);
  // Serial.print("\t");
  // Serial.print("pot 2: ");
  // Serial.print(pot2Value);
  // Serial.println();

  // Serial.print("timeMs: ");
  // Serial.print(timeMs);
  // Serial.print("\t");
  // Serial.print("colorChangeAtMs: ");
  // Serial.print(colorChangeAtMs);
  // Serial.println();
}