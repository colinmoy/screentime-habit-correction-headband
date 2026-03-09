const int buttonPosture = 4;
const int buttonDistance = 5;
const int ledWarning = 21;
const int ledPostureAlarm = 22;
const int ledDistanceAlarm = 23;

unsigned long postureStartTime = 0;
unsigned long distanceStartTime = 0;
const unsigned long thresholdDelay = 3000;

bool postureWarningActive = false;
bool distanceWarningActive = false;
bool postureAlarmActive = false;
bool distanceAlarmActive = false;

void setup() {
  Serial.begin(115200);
  
  pinMode(buttonPosture, INPUT_PULLUP);
  pinMode(buttonDistance, INPUT_PULLUP);
  
  pinMode(ledWarning, OUTPUT);
  pinMode(ledPostureAlarm, OUTPUT);
  pinMode(ledDistanceAlarm, OUTPUT);

  Serial.println("System Ready. Monitoring posture and screen distance...");
}

void loop() {
  bool postureBad = (digitalRead(buttonPosture) == LOW);
  bool distanceBad = (digitalRead(buttonDistance) == LOW);

  unsigned long currentMillis = millis();

  if (postureBad) {
    if (!postureWarningActive) {
      postureWarningActive = true;
      postureStartTime = currentMillis;
      Serial.println("[WARNING] Bad posture detected. Activating moderate feedback.");
    }
    
    if (!postureAlarmActive && (currentMillis - postureStartTime >= thresholdDelay)) {
      postureAlarmActive = true;
      Serial.println("[ALARM] Posture threshold exceeded (3s). Activating full feedback.");
    }
  } else {
    if (postureWarningActive || postureAlarmActive) {
      Serial.println("[IDLE] Posture corrected.");
    }
    postureWarningActive = false;
    postureAlarmActive = false;
  }

  if (distanceBad) {
    if (!distanceWarningActive) {
      distanceWarningActive = true;
      distanceStartTime = currentMillis;
      Serial.println("[WARNING] Screen too close. Activating moderate feedback.");
    }
    
    if (!distanceAlarmActive && (currentMillis - distanceStartTime >= thresholdDelay)) {
      distanceAlarmActive = true;
      Serial.println("[ALARM] Distance threshold exceeded (3s). Activating full feedback.");
    }
  } else {
    if (distanceWarningActive || distanceAlarmActive) {
      Serial.println("[IDLE] Distance corrected.");
    }
    distanceWarningActive = false;
    distanceAlarmActive = false;
  }

  if (postureWarningActive || distanceWarningActive) {
    digitalWrite(ledWarning, HIGH);
  } else {
    digitalWrite(ledWarning, LOW);
  }

  digitalWrite(ledPostureAlarm, postureAlarmActive ? HIGH : LOW);
  digitalWrite(ledDistanceAlarm, distanceAlarmActive ? HIGH : LOW);
}