#include <SoftwareSerial.h>

// Bluetooth Software Serial on D2 (RX) and D3 (TX)
SoftwareSerial BTSerial(2, 3);

// Motor Driver Pins (L298N)
const int IN1 = 5;
const int IN2 = 6;
const int IN3 = 9;
const int IN4 = 10;

// Water Pump Relay & Buzzer Pins
const int RELAY_PIN = 4;
const int BUZZER_PIN = 13;

// HC-SR04 Ultrasonic Pins
const int TRIG_PIN = 11;
const int ECHO_PIN = 12;

// IR Flame Sensor Pins (Analog)
const int FLAME_LEFT   = A2;
const int FLAME_CENTER = A0;
const int FLAME_RIGHT  = A1;

// System States and Authentication
bool isAuthenticated = false;
String authBuffer = "";
const String AUTH_KEY = "FFRL";

enum RobotMode { NORMAL_MODE, LOCKDOWN_MODE, ESCAPE_MODE };
RobotMode currentMode = NORMAL_MODE;

unsigned long stateStartTime = 0;
bool pumpState = false;

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Active LOW relay off by default
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  BTSerial.begin(9600);
  Serial.begin(9600);
  
  // Initialization alert
  tone(BUZZER_PIN, 1000, 200);
}

long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 25000); // 25ms timeout cap
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void moveForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void moveBackward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnLeft() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void handleBluetoothCommand(char cmd) {
  if (!isAuthenticated) {
    authBuffer += cmd;
    if (authBuffer.length() > 4) {
      authBuffer = authBuffer.substring(authBuffer.length() - 4);
    }
    if (authBuffer == AUTH_KEY) {
      isAuthenticated = true;
      tone(BUZZER_PIN, 1500, 150);
      delay(200);
      tone(BUZZER_PIN, 2000, 150);
    }
    return;
  }

  // Actuate Pump Toggle
  if (cmd == 'X' || cmd == 'x') {
    pumpState = !pumpState;
    digitalWrite(RELAY_PIN, pumpState ? LOW : HIGH);
    return;
  }

  // Navigation commands allowed based on mode
  if (currentMode == LOCKDOWN_MODE) return;

  if (currentMode == ESCAPE_MODE) {
    if (cmd == 'B') moveBackward();
    else if (cmd == 'L') turnLeft();
    else if (cmd == 'R') turnRight();
    else stopMotors();
    return;
  }

  // NORMAL_MODE
  if (cmd == 'F') moveForward();
  else if (cmd == 'B') moveBackward();
  else if (cmd == 'L') turnLeft();
  else if (cmd == 'R') turnRight();
  else stopMotors();
}

void loop() {
  unsigned long currentMillis = millis();
  long distance = readDistanceCM();

  // Safety State Machine: Timed Escape Window
  if (distance < 15 && currentMode == NORMAL_MODE) {
    currentMode = LOCKDOWN_MODE;
    stateStartTime = currentMillis;
    stopMotors();
    digitalWrite(BUZZER_PIN, HIGH);
  }

  if (currentMode == LOCKDOWN_MODE) {
    if (currentMillis - stateStartTime >= 1000) {
      currentMode = ESCAPE_MODE;
      stateStartTime = currentMillis;
      digitalWrite(BUZZER_PIN, LOW);
    }
  } else if (currentMode == ESCAPE_MODE) {
    if (currentMillis - stateStartTime >= 1500) {
      currentMode = NORMAL_MODE;
    }
  }

  // Flame Monitoring Array
  int flameL = analogRead(FLAME_LEFT);
  int flameC = analogRead(FLAME_CENTER);
  int flameR = analogRead(FLAME_RIGHT);

  if ((flameL < 700 || flameC < 700 || flameR < 700) && currentMode != LOCKDOWN_MODE) {
    tone(BUZZER_PIN, 800, 50);
  }

  // Teleoperation Stream
  if (BTSerial.available()) {
    char c = BTSerial.read();
    handleBluetoothCommand(c);
  }
}
