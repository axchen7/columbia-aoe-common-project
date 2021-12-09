#include <Servo.h>

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

int POT_SAMPLE_COUNT = 8;

float POT_1_UP = 593;
float POT_1_DOWN = 520;

float POT_2_UP = 428;
float POT_2_DOWN = 510;

float SERVO_FORWARD_POWER = 0.3;
float SERVO_BACKWARD_POWER = 0.3;

float DECEL_POWER_PER_SEC = SERVO_FORWARD_POWER * 1.0;

// servos

Servo servo1;
Servo servo2;

// global variables

float* pot1Raws = (float*)calloc(POT_SAMPLE_COUNT, sizeof(float));
float* pot2Raws = (float*)calloc(POT_SAMPLE_COUNT, sizeof(float));

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

  fillArray(pot1Raws, POT_SAMPLE_COUNT, POT_1_UP);
  fillArray(pot2Raws, POT_SAMPLE_COUNT, POT_2_UP);

  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_Y, HIGH);
  digitalWrite(LED_G, HIGH);
}

void loop() {
  Serial.print("B1: ");
  Serial.print(digitalRead(BEAM_1));
  Serial.print("\t");
  Serial.print("B2: ");
  Serial.print(digitalRead(BEAM_2));
  Serial.println();

  float pot1Value = analogRead(POT_1);
  float pot2Value = analogRead(POT_2);

  pushArray(pot1Raws, POT_SAMPLE_COUNT, pot1Value);
  pushArray(pot2Raws, POT_SAMPLE_COUNT, pot2Value);

  pot1Value = closestInArray(pot1Raws, POT_SAMPLE_COUNT, POT_1_UP);
  pot2Value = closestInArray(pot2Raws, POT_SAMPLE_COUNT, POT_2_UP);

  // Serial.print("pot 1: ");
  // Serial.print(pot1Value);
  // Serial.print("\t");
  // Serial.print("pot 2: ");
  // Serial.print(pot2Value);
  // Serial.println();

  float servo1Power = (pot1Value - POT_1_UP) / (POT_1_DOWN - POT_1_UP);
  float servo2Power = (pot2Value - POT_2_UP) / (POT_2_DOWN - POT_2_UP);

  servo1Power = clip(servo1Power, 0, 1);
  servo2Power = clip(servo2Power, 0, 1);

  bool buttonPressed = digitalRead(BUTTON) == LOW;

  if (buttonPressed) {
    servo1Power *= -SERVO_BACKWARD_POWER;
    servo2Power *= -SERVO_BACKWARD_POWER;

    setServoPower(servo1, servo1Power, true);
    setServoPower(servo2, servo2Power, false);
  } else {
    servo1Power *= SERVO_FORWARD_POWER;
    servo2Power *= SERVO_FORWARD_POWER;

    setServoPower(servo1, servo1Power, true);
    setServoPower(servo2, servo2Power, false);
  }

  delay(20);
}

void setServoPower(Servo servo, float power, bool flip) {
  servo.write(90 + power * (flip ? -90 : 90));
  // servo.write(90);
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
