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

unsigned long RED_DURATION_MS = 2000;
unsigned long YELLOW_DURATION_MS = 600;
unsigned long GREEN_DURATION_MIN_MS = 1200;
unsigned long GREEN_DURATION_MAX_MS = 2500;

int POT_SAMPLE_COUNT = 5;
float EMA_COEFF = 0.1;

float POT_1_UP = 596;
float POT_1_DOWN = 533;

float POT_2_UP = 436;
float POT_2_DOWN = 517;

float SERVO_FORWARD_POWER = 0.3;
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
bool beam1;
bool beam2;

float* pot1Raws = (float*)calloc(POT_SAMPLE_COUNT, sizeof(float));
float* pot2Raws = (float*)calloc(POT_SAMPLE_COUNT, sizeof(float));

float prevPot1Raw = POT_1_UP;
float prevPot2Raw = POT_2_UP;

int gameWonColor = 3;
bool redLightsOn = false;

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

  resetPotFilter();

  timeMs = millis();

  setState(PLAY);
  setColor(RED);
}

void loop() {
  buttonPressed = digitalRead(BUTTON) == LOW;
  beam1 = digitalRead(BEAM_1) == HIGH;
  beam2 = digitalRead(BEAM_2) == HIGH;

  float pot1Value = analogRead(POT_1);
  float pot2Value = analogRead(POT_2);

  pushArray(pot1Raws, POT_SAMPLE_COUNT, pot1Value);
  pushArray(pot2Raws, POT_SAMPLE_COUNT, pot2Value);

  pot1Value = closestInArray(pot1Raws, POT_SAMPLE_COUNT, POT_1_UP);
  pot2Value = closestInArray(pot2Raws, POT_SAMPLE_COUNT, POT_2_UP);

  pot1Value = pot1Value * EMA_COEFF + (1 - EMA_COEFF) * prevPot1Raw;
  pot2Value = pot2Value * EMA_COEFF + (1 - EMA_COEFF) * prevPot2Raw;

  prevPot1Raw = pot1Value;
  prevPot2Raw = pot2Value;

  Serial.print("pot 1: ");
  Serial.print(pot1Value);
  Serial.print("\t");
  Serial.print("pot 2: ");
  Serial.print(pot2Value);
  Serial.println();

  float servo1Power = (pot1Value - POT_1_UP) / (POT_1_DOWN - POT_1_UP);
  float servo2Power = (pot2Value - POT_2_UP) / (POT_2_DOWN - POT_2_UP);

  servo1Power = clip(servo1Power, 0, 1);
  servo2Power = clip(servo2Power, 0, 1);

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

void resetPotFilter() {
  fillArray(pot1Raws, POT_SAMPLE_COUNT, POT_1_UP);
  fillArray(pot2Raws, POT_SAMPLE_COUNT, POT_2_UP);
}

void setState(State newState) {
  state = newState;
  stateSetMs = timeMs;

  resetPotFilter();
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
      if (!beam1) setState(P1_WIN);
      if (!beam2) setState(P2_WIN);

      if (color == RED) {
        if (servo1IsPowered) setState(P1_LOSE);
        if (servo2IsPowered) setState(P2_LOSE);
      }
      break;
    case P1_WIN:
      // remove the power from P2
      setServoPower(servo2, servo2Power, false);
    
      // check if we pressed the button to trigger RESET
      bool flag = false;
      int initialTimeOfColorSwitch = millis();
      const int timeToSwitch = 2000;
      
      do {
        buttonPressed = digitalRead(BUTTON) == LOW;
        if(buttonPressed) {
          flag = true;
          setState(RESET);
          break;
        }
      } while( millis() - initialTimeOfColorSwitch <= timeToSwitch );

      gameWonColor = getNextWinningColor(gameWonColor);
      gameWonLigths(gameWonColor);

    case P2_WIN:
      // remove the power from P1
      setServoPower(servo1, servo1Power, false);
    
      // check if we pressed the button to trigger RESET
      bool flag = false;
      int initialTimeOfColorSwitch = millis();
      const int timeToSwitch = 2000;
      
      do {
        buttonPressed = digitalRead(BUTTON) == LOW;
        if(buttonPressed) {
          flag = true;
          setState(RESET);
          break;
        }
      } while( millis() - initialTimeOfColorSwitch <= timeToSwitch );
      
      gameWonColor = getNextWinningColor(gameWonColor);
      gameWonLigths(gameWonColor);

    case P1_LOSE:
      // remove the power from P1 & P2
      setServoPower(servo1, servo1Power, false);
      setServoPower(servo2, servo2Power, false);
    
      // check if we pressed the button to trigger RESET
      bool flag = false;
      int initialTimeOfColorSwitch = millis();
      const int timeToSwitch = 2000;
      
      do {
        buttonPressed = digitalRead(BUTTON) == LOW;
        if(buttonPressed) {
          flag = true;
          setState(RESET);
          break;
        }
      } while( millis() - initialTimeOfColorSwitch <= timeToSwitch );
      gameLostLights(redLightsOn);

      case P2_LOSE:
      // remove the power from P1 & P2
      setServoPower(servo1, servo1Power, false);
      setServoPower(servo2, servo2Power, false);
    
      // check if we pressed the button to trigger RESET
      bool flag = false;
      int initialTimeOfColorSwitch = millis();
      const int timeToSwitch = 2000;
      
      do {
        buttonPressed = digitalRead(BUTTON) == LOW;
        if(buttonPressed) {
          flag = true;
          setState(RESET);
          break;
        }
      } while( millis() - initialTimeOfColorSwitch <= timeToSwitch );
      gameLostLights(redLightsOn);

    case RESET:
      if (!buttonPressed) setState(PLAY);
      break;
  }
}

void updateColor() {
  if (timeMs > colorChangeAtMs) {
    if (color == RED)
      setColor(GREEN);
    else
      setColor(RED);
  }

  if (color == RED) {
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_Y, LOW);
    digitalWrite(LED_G, LOW);
  } else {
    digitalWrite(LED_R, LOW);

    if (timeMs > colorChangeAtMs - YELLOW_DURATION_MS) {
      digitalWrite(LED_Y, HIGH);
      digitalWrite(LED_G, LOW);
    } else {
      digitalWrite(LED_Y, LOW);
      digitalWrite(LED_G, HIGH);
    }
  }
}

void gameWonLigths(currentColor) {
    switch (currentColor) {
    case 1:
      digitalWrite(LED_G, HIGH);
      digitalWrite(LED_Y, LOW);
      digitalWrite(LED_R, LOW);
      break;
    case 2:
      digitalWrite(LED_G, LOW);
      digitalWrite(LED_Y, HIGH);
      digitalWrite(LED_R, LOW);
      break;
    case 3:
      digitalWrite(LED_G, LOW);
      digitalWrite(LED_Y, LOW);
      digitalWrite(LED_R, HIGH);
      break;
    default:
      break;
  }
}

int getNextWinningColor(int currentWinningColor) {
  int nextWinningColor = 1;
  
  switch(currentWinningColor) {
    case 1:
      nextWinningColor = 2;
      break;
    case 2:
      nextWinningColor = 3;
      break;
    case 3:
      nextWinningColor = 1;
      break;
    default:
      nextWinningColor = 2;
      break;
  }
}

void gameLostLights(redLightsOn) {
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_Y, LOW);
  
    if(redLightsOn) {
    digitalWrite(LED_R, LOW);
    return;
  }
  digitalWrite(LED_R, HIGH);
}
