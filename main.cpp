#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// Display Setup
#define OLED_CLOCK    22  // SCL
#define OLED_DATA     21  // SDA
U8G2_SSD1309_128X64_NONAME2_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, OLED_CLOCK, OLED_DATA);

// Timing variables
unsigned long cycleStartTime = 0;
static unsigned long lastDebugTime = 0;
const unsigned long cycleDuration = 30000;  // 30 seconds cycle

// Hysteresis variables and constants
const int hysteresisMargin = 2;  // Adjust this value as needed
const unsigned long debounceDelay = 50;  // 50ms debounce delay
const int numReadings = 10;

// Oven pin definitions
const int ovenPotPin = 32;
const int ovenTempSensorPin = 13;
const int ovenBakeRelayPin = 26;
const int ovenBroilRelayPin = 27;

// Power management variables
bool powerConstraintActive = false; // Set true when battery is low or system demand exceeds limit
const int maxTotalDuty = 40; // Max total duty cycle across all heaters (0-90)
const int maxGridWatts = 1500;
const int maxBatteryWatts = 3000; // Max power from battery
const int wattsPerDutyUnit = 100; // Watts per duty unit (30% duty cycle = 300W)

// Function to get the total duty cycle of all heaters and oven
int getTotalDutyCycle(){
  int total = ovenRelayState ? (ovenSetpoint >= 550 ? 40 : 30) : 0;
  for (int i = 0; i < numHeaters; i++) {
    if (heaters[i].relayState) {
      // If the heater is on, add its duty cycle to the total
      total += heaters[i].dutyCycle;
    }
  }
  return total;
}

// Function to get the total power consumption in watts
int getTotalPowerWatts(){
  int total = 0;
  if (ovenRelayState) {
    total += ovenSetpoint >= 550 ? 3000 : 2585;
  }
  for (int i = 0; i < numHeaters; i++) {
    if (heaters[i].relayState) {
      total += heaters[i].dutyCycle * wattsPerDutyUnit;
    }
  }
  return total;
}

struct PowerSplit {
  int gridWatts;
  int batteryWatts;
};

// Function to calculate power split between grid and battery
PowerSplit calculatePowerSplit(int requestedWatts){
  PowerSplit split;
  if (requestedWatts <= maxGridWatts) {
    split.gridWatts = requestedWatts;
    split.batteryWatts = 0;
  } else {
    split.gridWatts = maxGridWatts;
    split.batteryWatts = min(requestedWatts - maxGridWatts, maxBatteryWatts);
  }
  return split;
}

// Burner Struct
struct HeaterUnit {
  const char* label;
  int adcPin;
  int relayPin;
  int readings[numReadings] = {0};
  int readIndex = 0;
  int total = 0;
  int adcValue = 0;
  float voltage = 0.0;
  int dutyCycle = 0;
  int lastDutyCycle = 0;
  bool relayState = false;
  unsigned long lastRelayChangeTime = 0;

  // Constructor to initialize the HeaterUnit
  HeaterUnit(const char* lbl, int adc, int relay)
    : label(lbl), adcPin(adc), relayPin(relay),
    readIndex(0), total(0), adcValue(0), voltage(0.0),
    dutyCycle(0), lastDutyCycle(0), relayState(false),
    lastRelayChangeTime(0)
  {
    for (int i = 0; i < numReadings; i++) readings[i] = 0;
  }
};

// For burners only
HeaterUnit heaters[] = {
  {"Burner 1", 34, 12},  // potentiometer gpio, relay gpio
  {"Burner 2", 35, 14},
};
const int numHeaters = sizeof(heaters) / sizeof(heaters[0]);

// Oven variables
int ovenReadings[numReadings] = {0};
int ovenReadIndex = 0;
int ovenTotal = 0;
int ovenAdcValue = 0;
float ovenVoltage = 0.0;
int ovenTempF = 0;
int ovenSetpoint = 0;
int ovenDutyCycle = 0;
int ovenLastDutyCycle = 0;
bool ovenRelayState = false;
unsigned long ovenLastRelayChangeTime = 0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // 12-bit resolution (0-4095)
  
  // Init burner pins
  for (int i = 0; i < numHeaters; i++) {
    pinMode(heaters[i].relayPin, OUTPUT);
    digitalWrite(heaters[i].relayPin, LOW);
  }

  // Init oven pins
  pinMode(ovenBakeRelayPin, OUTPUT);
  pinMode(ovenBroilRelayPin, OUTPUT);
  digitalWrite(ovenBakeRelayPin, LOW);
  digitalWrite(ovenBroilRelayPin, LOW);

  if (!oled.begin()) {
    Serial.println(F("U8G2 allocation failed!"));
    while (true); // Halt execution if OLED initialization fails
  }

  // Startup Display
  oled.clearBuffer(); // Clear the buffer before drawing
  oled.setFont(u8g2_font_ncenB14_tr);
  oled.drawStr(0, 24, "Eikos Tech");
  oled.sendBuffer(); // Send the buffer to the display
  delay(1500); // Display for 1.5 seconds
}

void loop() {

  // Get the time in the current 30-second cycle
  unsigned long now = millis();
  unsigned long cycleTime = now - cycleStartTime;
  if (cycleTime > cycleDuration) {
    cycleStartTime = now;
    cycleTime = 0;
  }

  // Update OLED display
  oled.clearBuffer(); // Clear the buffer before drawing
  oled.setFont(u8g2_font_6x13_tr);

  // Burner control
  int burnerPower[2];
  char burnerStatus[2][4];

  bool anyBurnerOn = false;

  for (int i = 0; i < numHeaters; i++) {
    HeaterUnit& h = heaters[i];

    // Smoothing
    h.total -= h.readings[h.readIndex];
    h.readings[h.readIndex] = analogRead(h.adcPin);
    h.total += h.readings[h.readIndex];
    h.readIndex = (h.readIndex + 1) % numReadings;
    h.adcValue = h.total / numReadings;
    h.voltage = h.adcValue * (3.3 / 4095.0); // Convert ADC value to voltage

    // Calculate duty cycle with hysteresis
    int rawDutyCycle = map(h.adcValue, 0, 4095, 0, 30); // Map ADC value to duty cycle (0-30)
    int newDutyCycle = h.lastDutyCycle;
    if (rawDutyCycle > h.lastDutyCycle + hysteresisMargin) {
      newDutyCycle = rawDutyCycle;
    } else if (rawDutyCycle < h.lastDutyCycle - hysteresisMargin) {
      newDutyCycle = rawDutyCycle;
    }
    h.dutyCycle = newDutyCycle;
    h.lastDutyCycle = newDutyCycle;

    // Enforce power constraints
    bool allowBurner = true;
    if (powerConstraintActive) {
      if (ovenRelayState) {
        allowBurner = false; // Oven is on, don't allow burners to turn on
      } else if (anyBurnerOn){
        allowBurner = false; // Only one burner can be on at a time
      }
    }

    // Relay state
    bool shouldBeOn = cycleTime < (h.dutyCycle * (cycleDuration / 30));
    if (shouldBeOn != h.relayState && (now - h.lastRelayChangeTime > debounceDelay)) {
      h.relayState = shouldBeOn;
      digitalWrite(h.relayPin, h.relayState);
      h.lastRelayChangeTime = now;
    }

    if (h.relayState){
      anyBurnerOn = true; // At least one burner is on
    }
    
    // Burner OLED Display Output
    burnerPower[i] = h.dutyCycle * 100 / 30; // Convert duty cycle to percentage
    snprintf(burnerStatus[i], sizeof(burnerStatus[i]), h.relayState ? "ON " : "OFF");
    char burnerLine[64];
    snprintf(burnerLine, sizeof(burnerLine),
      "B1: %3d%%  B2: %3d%%", burnerPower[0], burnerPower[1]);
    oled.drawStr(0, 15, burnerLine);

    // Debugging Output
    if (now - lastDebugTime > 500) {
      Serial.printf("%s | ADC: %d | Voltage: %.2f V | Duty: %d | Power: %d%% | Relay: %s\n",
        h.label, h.adcValue, h.voltage, h.dutyCycle, burnerPower[i], h.relayState ? "ON" : "OFF");
      }
  }

  // Oven Setpoint
  int potValue = analogRead(ovenPotPin);
  int stepSize = 25;
  int maxSteps = 550 / stepSize;
  int step = map(potValue, 0, 4095, 0, maxSteps);
  ovenSetpoint = step * stepSize;

  // Oven Temperature Sensor
  ovenTotal -= ovenReadings[ovenReadIndex];
  ovenReadings[ovenReadIndex] = analogRead(ovenTempSensorPin);
  ovenTotal += ovenReadings[ovenReadIndex];
  ovenReadIndex = (ovenReadIndex + 1) % numReadings;

  ovenAdcValue = ovenTotal / numReadings;
  ovenVoltage = ovenAdcValue * (3.3 / 4095.0); 
  ovenTempF = map(ovenAdcValue, 0, 4095, 0, 550); // Map ADC value to temperature (0-550F)

  // Oven Control
  if (ovenSetpoint >= 550) {
    //Broil mode
    digitalWrite(ovenBroilRelayPin, HIGH);
    digitalWrite(ovenBakeRelayPin, HIGH);
    ovenRelayState = true;
    oled.drawStr(0, 31, "Oven: BROIL");

    if (now - lastDebugTime > 500) {
    Serial.printf("Oven | BROIL | Setpoint: %dF | Temp: %dF | Relays: ON\n", ovenSetpoint, ovenTempF);
    }
  } else if (ovenSetpoint == 0){
    // Oven off 
    digitalWrite(ovenBroilRelayPin, LOW);
    digitalWrite(ovenBakeRelayPin, LOW);
    ovenRelayState = false;
    oled.drawStr(0, 31, "Oven: OFF");

    if (now - lastDebugTime > 500) {
      Serial.printf("Oven | OFF | Temp: %dF | Relays: OFF\n", ovenTempF);
    }
  } else {
    // Bake mode
    digitalWrite(ovenBroilRelayPin, LOW);
    bool shouldBeOn = ovenTempF < (ovenSetpoint - hysteresisMargin);
    if (shouldBeOn != ovenRelayState && (now - ovenLastRelayChangeTime > debounceDelay)) {
      ovenRelayState = shouldBeOn;
      digitalWrite(ovenBakeRelayPin, ovenRelayState);
      ovenLastRelayChangeTime = now;
    }

    char ovenLine[32];
    snprintf(ovenLine, sizeof(ovenLine), "Oven: %3dF", ovenTempF);
    oled.drawStr(0, 31, ovenLine);

    if (now - lastDebugTime > 500) {
      Serial.printf("Oven | BAKE | Setpoint: %dF | Temp: %dF | Relay: %s\n",
        ovenSetpoint, ovenTempF, ovenRelayState ? "ON" : "OFF");
    }
  }

  // Power Management
  int totalRequestedWatts = getTotalPowerWatts();
  PowerSplit split = calculatePowerSplit(totalRequestedWatts);

  //flag to shutoff if total exceeds supply
  bool overload = (split.gridWatts + split.batteryWatts) < totalRequestedWatts;

  // Power Constraint Check
  powerConstraintActive = overload;

  // Oven Setpoint Display
  char spLine[32];
  snprintf(spLine, sizeof(spLine), "Setpoint: %3dF", ovenSetpoint);
  oled.drawStr(0, 47, spLine);

  // Battery Level Display Placeholder
  oled.drawStr(0, 63, "Battery Level: 71%");

  if (now - lastDebugTime > 500) {
    lastDebugTime = now;
  }

  oled.sendBuffer(); // Send the buffer to the display
}