#include <SoftwareSerial.h>

// Pines para el RTC
virtuabotixRTC myRTC(6, 7, 8);

// Pines para los sensores
const int frontSensor1Pin = A0;
const int frontSensor2Pin = A1;
const int rearSensorPin = A2;

// Pines para el módulo Bluetooth
const int btTx = 10;  // TX del Bluetooth conectado al pin digital 10 del Arduino
const int btRx = 11;  // RX del Bluetooth conectado al pin digital 11 del Arduino

SoftwareSerial btSerial(btRx, btTx);

const int threshold = 400;  // Umbral inicial para detectar una pisada

unsigned long lastTime = 0;
bool lastFrontState1 = false;
bool lastFrontState2 = false;
bool lastRearState = false;
bool frontSensorReleased = false;
bool rearSensorPressed = false;
unsigned long frontSensorReleasedTime = 0;
unsigned long rearSensorPressedTime = 0;

// Variables para el promedio de fase 1 y fase 2
const int maxSteps = 10;
unsigned long phase1Times[maxSteps];
unsigned long phase2Times[maxSteps];
unsigned long stepTimes[maxSteps]; // Historial de tiempos de pasos
int phase1Count = 0;
int phase2Count = 0;
int stepCount = 0; // Contador de pasos

// Variables para almacenar los niveles de presión
const int maxReadings = 100; // Máximo número de lecturas para calcular la moda
int frontSensor1Levels[maxReadings];
int frontSensor2Levels[maxReadings];
int rearSensorLevels[maxReadings];
int readingCount = 0;

void setup() {
  Serial.begin(9600);  // Comunicación serial para depuración
  btSerial.begin(9600); // Comunicación serial con el módulo Bluetooth

  // Inicializar los historiales de tiempos y niveles de presión a 0
  for (int i = 0; i < maxSteps; i++) {
    phase1Times[i] = 0;
    phase2Times[i] = 0;
    stepTimes[i] = 0;
  }

  for (int i = 0; i < maxReadings; i++) {
    frontSensor1Levels[i] = 0;
    frontSensor2Levels[i] = 0;
    rearSensorLevels[i] = 0;
  }
}

int getPressureLevel(int sensorValue) {
  if (sensorValue < 400) return 0;
  if (sensorValue <= 524) return 1;
  if (sensorValue <= 649) return 2;
  if (sensorValue <= 774) return 3;
  if (sensorValue <= 899) return 4;
  return 5;
}

int calculateMode(int levels[], int size) {
  int frequency[6] = {0}; // Array para contar la frecuencia de cada nivel
  for (int i = 0; i < size; i++) {
    frequency[levels[i]]++;
  }
  int mode = 0;
  for (int i = 1; i < 6; i++) {
    if (frequency[i] > frequency[mode]) {
      mode = i;
    }
  }
  return mode;
}

void loop() {
  myRTC.updateTime();

  int frontSensor1Value = analogRead(frontSensor1Pin);
  int frontSensor2Value = analogRead(frontSensor2Pin);
  int rearSensorValue = analogRead(rearSensorPin);

  bool currentFrontState1 = frontSensor1Value > threshold;
  bool currentFrontState2 = frontSensor2Value > threshold;
  bool currentRearState = rearSensorValue > threshold;

  int frontPressureLevel1 = getPressureLevel(frontSensor1Value);
  int frontPressureLevel2 = getPressureLevel(frontSensor2Value);
  int rearPressureLevel = getPressureLevel(rearSensorValue);

  // Almacenar los niveles de presión
  if (readingCount < maxReadings) {
    frontSensor1Levels[readingCount] = frontPressureLevel1;
    frontSensor2Levels[readingCount] = frontPressureLevel2;
    rearSensorLevels[readingCount] = rearPressureLevel;
    readingCount++;
  }

  // Enviar los niveles de presión por Serial y Bluetooth
  String pressureMessage = "Niveles de presión - Frente 1: " + String(frontPressureLevel1) +
                           ", Frente 2: " + String(frontPressureLevel2) +
                           ", Trasero: " + String(rearPressureLevel) + "\n";
  Serial.print(pressureMessage);
  btSerial.print(pressureMessage);

  // Fase 1: Desde que cualquiera de los sensores delanteros es soltado hasta que el sensor trasero es presionado
  if ((!currentFrontState1 && lastFrontState1) || (!currentFrontState2 && lastFrontState2)) {
    frontSensorReleased = true;
    frontSensorReleasedTime = millis();
  }

  if (frontSensorReleased && currentRearState && !lastRearState) {
    unsigned long currentTime = millis();
    unsigned long elapsedTimePhase1 = currentTime - frontSensorReleasedTime;

    phase1Times[phase1Count % maxSteps] = elapsedTimePhase1;
    phase1Count++;

    unsigned long sumPhase1 = 0;
    int countPhase1 = min(phase1Count, maxSteps);
    for (int i = 0; i < countPhase1; i++) {
      sumPhase1 += phase1Times[i];
    }
    float averageTimePhase1 = (float)sumPhase1 / countPhase1;

    String elapsedTimeMessagePhase1 = "Tiempo fase 1: " + String(elapsedTimePhase1) + " ms\n";
    String averageTimeMessagePhase1 = "Promedio fase 1: " + String(averageTimePhase1, 2) + " ms\n";
    Serial.print(elapsedTimeMessagePhase1);
    Serial.print(averageTimeMessagePhase1);
    btSerial.print(elapsedTimeMessagePhase1);
    btSerial.print(averageTimeMessagePhase1);

    frontSensorReleased = false;
    rearSensorPressed = true;
    rearSensorPressedTime = currentTime;
  }

  // Fase 2: Desde que el sensor trasero es presionado hasta que cualquiera de los sensores delanteros es soltado
  if (rearSensorPressed && ((!currentFrontState1 && lastFrontState1) || (!currentFrontState2 && lastFrontState2))) {
    unsigned long currentTime = millis();
    unsigned long elapsedTimePhase2 = currentTime - rearSensorPressedTime;

    phase2Times[phase2Count % maxSteps] = elapsedTimePhase2;
    phase2Count++;

    unsigned long sumPhase2 = 0;
    int countPhase2 = min(phase2Count, maxSteps);
    for (int i = 0; i < countPhase2; i++) {
      sumPhase2 += phase2Times[i];
    }
    float averageTimePhase2 = (float)sumPhase2 / countPhase2;

    String elapsedTimeMessagePhase2 = "Tiempo fase 2: " + String(elapsedTimePhase2) + " ms\n";
    String averageTimeMessagePhase2 = "Promedio fase 2: " + String(averageTimePhase2, 2) + " ms\n";
    Serial.print(elapsedTimeMessagePhase2);
    Serial.print(averageTimeMessagePhase2);
    btSerial.print(elapsedTimeMessagePhase2);
    btSerial.print(averageTimeMessagePhase2);

    rearSensorPressed = false;

    // Medir tiempo de un paso completo (sensor trasero presionado -> sensor delantero soltado)
    unsigned long elapsedTimeStep = currentTime - rearSensorPressedTime;
    stepTimes[stepCount % maxSteps] = elapsedTimeStep;
    stepCount++;

    // Calcular promedio de pasos por minuto
    unsigned long sumSteps = 0;
    int countSteps = min(stepCount, maxSteps);
    for (int i = 0; i < countSteps; i++) {
      sumSteps += stepTimes[i];
    }
    float averageTimeSteps = (float)sumSteps / countSteps;
    float stepsPerMinute = 60000.0 / averageTimeSteps;

    String stepTimeMessage = "Tiempo de paso: " + String(elapsedTimeStep) + " ms\n";
    String averageStepMessage = "Promedio de pasos por minuto: " + String(stepsPerMinute, 2) + "\n";
    Serial.print(stepTimeMessage);
    Serial.print(averageStepMessage);
    btSerial.print(stepTimeMessage);
    btSerial.print(averageStepMessage);
  }

  // Calcular y mostrar la moda de los niveles de presión cuando se alcanza el máximo de lecturas
  if (readingCount == maxReadings) {
    int modeFrontSensor1 = calculateMode(frontSensor1Levels, maxReadings);
    int modeFrontSensor2 = calculateMode(frontSensor2Levels, maxReadings);
    int modeRearSensor = calculateMode(rearSensorLevels, maxReadings);

    String modeMessage = "Moda de niveles de presión - Frente 1: " + String(modeFrontSensor1) +
                         ", Frente 2: " + String(modeFrontSensor2) +
                         ", Trasero: " + String(modeRearSensor) + "\n";
    Serial.print(modeMessage);
    btSerial.print(modeMessage);

    // Reiniciar el conteo de lecturas
    readingCount = 0;
  }

  lastFrontState1 = currentFrontState1;
  lastFrontState2 = currentFrontState2;
  lastRearState = currentRearState;

  delay(100);
}
