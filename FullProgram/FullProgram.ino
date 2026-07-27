// Used Pins
// 2, 4, 7, 8, 11, 12, A2, A4, A5

// Servo
#include <Servo.h> // Servo library
Servo servo; // Servo object, to attach to a Pin later

// Flow sensor
int flowPin = 2; // Attach flow sensor to pin 2
volatile unsigned int count = 0; // Pulse count 
double flowRate; // Variable calculated, later in loop(), from the pulses 

// Temperature sensor
#include <OneWire.h> // OneWire devices library
#include <DallasTemperature.h> // Temperature sensor library
#define ONE_WIRE_BUS 4 // Attach temperature sensor to pin 4
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);
double temperature; // Temperature variable, used later in loop()

// Relay
#define RELAY_PIN 7 // Attach relay to pin 7

// LED strip
const int ledPin = 11; // Attach LED strip to pin 11
bool isDay = true; // Variable to help verify which time of day it should be (to change or not)
unsigned long lastSwitchTime = 0; // Variable to "reset" the time after switching day/night

// Water level sensor
const int sensorPin = A2; // Attach water level sensor to pin 12

// Water pump
const int pumpPin = 13; // Attach water pump to pin 13
volatile unsigned int pumpCount = 0;

// LCD 
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
LiquidCrystal_I2C lcd(0x27,  20, 4);

// Times
unsigned long lastFeedTime = 0; // Variable to "reset" the time after feeding time

unsigned long lastFlowTime = 0; // Variable to "reset" the time after water flow reading
const unsigned long flowInterval = 1000; // 1 second interval between readings

unsigned long lastTempTime = 0; // Variable to "reset" the time after temperature reading
const unsigned long tempInterval = 1000; // 1 second interval between readings

unsigned long lastLevelTime = 0; // Variable to "reset" the time after water level reading
const unsigned long levelInterval = 1000; // 1 second interval between readings

unsigned long lastPlotterTime = 0; // Variable to "reset" the time after serial plotter code block 

unsigned long LastLcdTime = 0; // Variable to "reset" the time after updating the LCD
const unsigned long LcdInterval = 1000; // 1 second interval between LCD updates


// Different environments setup
enum Environment {
  BETTA,
  GUPPY,
  // More environments can be added later
};
Environment currentEnv = BETTA;  // Default environment

struct AquariumSettings {
  float minTemp;
  float maxTemp;
  unsigned long dayDuration;
  unsigned long nightDuration;
  unsigned long feedInterval;
  int dayBrightness;
  int nightBrightness;
};

const AquariumSettings* settings;

const AquariumSettings betta = {
  27.5, 28.5,                  // Temperature range (°C) (around 28)
  50400000UL, 36000000UL,      // 14h day / 10h night
  10000UL,                  // Feed every 24h
  255, 0,                      // Light ON / OFF
};

const AquariumSettings guppy = {
  26.0, 27.0,                  // Temperature range (°C) (around 26.5)
  43200000UL, 43200000UL,      // 12h day / 12h night
  86400000UL,                  // Feed every 24h
  255, 0,                      // Light ON / OFF
};

void setEnvironment(Environment env) {
  currentEnv = env;

  switch (env) {
    case BETTA: // Check if current environment is betta
      settings = &betta; // Settings points to betta configuration
      Serial.println("Environment: Betta"); // Terminal message when switching environment
      break; // Exit after executing branch

    case GUPPY: // Check if current environment is guppy
      settings = &guppy; // Settings points to guppy configuration
      Serial.println("Environment: Guppy"); // Terminal message when switching environment
      break; // Exit after executing branch
  }

  isDay = true; // Initialize environment in daytime state
  analogWrite(ledPin, settings->dayBrightness); // Set LED strip to daytime setting

  lastSwitchTime = millis(); // "Reset" day/night cycle timer
  lastFeedTime = millis(); // "Reset" feeder timer 

  delay(10); // Brief stabilization after switch
}


// Flow interrupt
void Flow() {  // Interrupt occurs
  count++;     // Increase count by 1
}


void setup() {
  Serial.begin(9600); // Data rate set to 9600 bits per second

  // Set default enviroment
  setEnvironment(BETTA); // Default initial environment when the program starts

  // Servo
  servo.attach(8); // Attach the servo to pin 8

  // Flow sensor
  pinMode(flowPin, INPUT_PULLUP); // Set the flow sensor's pin as an input pin
  attachInterrupt(digitalPinToInterrupt(flowPin), Flow, RISING); // Sets interrupt so that every pulse on flowPin calls the Flow function

  // Temperature sensor
  sensors.begin(); // Initalizes communication with the sensor

  // Relay
  pinMode(RELAY_PIN, OUTPUT); // Set the relay pin as an output pin
  digitalWrite(RELAY_PIN, HIGH); // Initialize with the relay OFF 

  // LED strip
  pinMode(ledPin, OUTPUT); // Set the LED pin as an output pin
  lastSwitchTime = millis(); // Define the variable as the time that has passed since the program was initialized

  // Water level sensor
  pinMode(sensorPin, INPUT_PULLUP); // Set the water level sensor pin as an input pin

  // Water pump
  pinMode(pumpPin, OUTPUT); // Set the water pump pin as an output pin
  digitalWrite(pumpPin, LOW); // Initialize with pump OFF

  // LCD
  lcd.init();
  lcd.backlight();
}


void loop() {
  unsigned long currentMillis = millis(); // currentMillis is assigned the current running time
  
  // Environment switching
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    switch (cmd) {
      case 'g':
      case 'G':
        if (currentEnv == GUPPY) {
          Serial.println("Guppy Environment Already Applied");
        } else {
          setEnvironment(GUPPY);
        }
        break;

      case 'b':
      case 'B':
        if (currentEnv == BETTA) {
          Serial.println("Betta Environment Already Applied");
        } else {
          setEnvironment(BETTA);
        }
        break;

      default:
        Serial.println("Unknown command");
        break;
    }
  }

  // Feeder servo
  static unsigned long feedStopTime = 0; // Variable to stop the servo after feeding
  static bool feeding = false; // Variable to verify the servo state and if it should change

  if (!feeding && currentMillis - lastFeedTime >= settings->feedInterval) { // Non-blocking times
    lastFeedTime = currentMillis; // "Reset" times for next loop's verifications
    servo.write(180); // Servo to 180 angle
    feeding = true; // Change the servo state
    feedStopTime = currentMillis; // Starts the 2 second count until the servo stops
  }

  if (feeding && currentMillis - feedStopTime >= 2000) { // Non-blocking times
    servo.write(5); // Servo to 5 angle
    feeding = false; // Change the servo state
  }

  // Flow sensor
  if (currentMillis - lastFlowTime >= flowInterval) { // Non-blocking times
    lastFlowTime = currentMillis; // "Reset" times for next loop's verifications

    noInterrupts(); // Disable interrupts (Flow() will stop)
    unsigned int pulses = count; // Take pulse counter
    count = 0; // Reset pulse counter
    interrupts(); // Re-enable interrupts

    flowRate = pulses / 5.5;  // Turn the pulse count to flow rate in L/min

    Serial.print("Flow rate: "); // Print result 
    Serial.print(flowRate, 2);   // to the
    Serial.println(" L/min");    // terminal
  }

  // Temperature sensor
  if (currentMillis - lastTempTime >= tempInterval) { // Non-blocking times
    lastTempTime = currentMillis; // "Reset" times for next loop's verifications

    sensors.requestTemperatures(); // Get temperature measurement at this moment
    temperature = sensors.getTempCByIndex(0); // Get temperature at Celsius

    Serial.print("Temperature: "); // Print 
    Serial.print(temperature);     // to the 
    Serial.println(" C");          // terminal
  }

  // Relay (heater)
  static bool heaterOn = false; // Set relay-heater state

  if (!heaterOn && temperature <= settings->minTemp) { // Check state and temperature
    digitalWrite(RELAY_PIN, LOW); // Relay ON
    heaterOn = true; // Set relay-heater state as ON for next loop's verification
  }
  else if (heaterOn && temperature >= settings->maxTemp) { // Check state and temperature
    digitalWrite(RELAY_PIN, HIGH); // Relay OFF
    heaterOn = false; // Set relay-heater state as OFF for next loop's verifications
  }
  
  // LED strip
  if (isDay && currentMillis - lastSwitchTime >= settings->dayDuration) { // Check state and times
    isDay = false; // Set to night state 
    lastSwitchTime = currentMillis; // "Reset" times for next loop's verification
    analogWrite(ledPin, settings->nightBrightness); // Set LED strip to set night brightness
  }
  else if (!isDay && currentMillis - lastSwitchTime >= settings->nightDuration) { // Check state and times
    isDay = true; // Set to day state
    lastSwitchTime = currentMillis; // "Reset" times for next loop's verification
    analogWrite(ledPin, settings->dayBrightness); // Set LED strip to set day brightness
  }

  // Water level sensor
  int sensorState = analogRead(sensorPin); // Check pin state (0-1023) 
  int levelDisplay = 0; 

  if(currentMillis - lastLevelTime >= levelInterval){ // Non-blocking times
    lastLevelTime = currentMillis; // "Reset" times for next loop's verification
    if (sensorState > 800) { // Liquid detected
    Serial.println("Status: [ LIQUID DETECTED ]"); // Terminal message
    levelDisplay = 1;
    
  } else { // No liquid detected
    Serial.println("Status: No liquid..."); // Terminal message 
    levelDisplay = 0;
    }
  }

  // Water pump
  static bool pumpOn = false; // Set pump state
  static unsigned long oneSecondAgo = 0;  // Variable to help count the seconds

  if (currentMillis - oneSecondAgo >= 1000) { // 1 second non-blocking time
    oneSecondAgo = currentMillis; // Attach the current time to oneSecondAgo
    pumpCount++; // Seconds time counter
  }

  if (!pumpOn && pumpCount >= 360) { // Non-blocking times
    digitalWrite(pumpPin, HIGH); // Start pump
    pumpOn = true; // Change pump state
    pumpCount = 0;  // Reset pumpCount for next loops
  }

  if (pumpOn && pumpCount >= 60) { // Non-blocking times
    digitalWrite(pumpPin, LOW); // Stop pump
    pumpOn = false; // Change pump state
    pumpCount = 0;  // Reset pumpCount for next loops
  }

  // LCD 
  if (currentMillis - LastLcdTime >= LcdInterval) {
    lcd.setCursor(0,0);
    lcd.print("Temp: ");
    lcd.print(temperature);

    lcd.setCursor(0,1);
    lcd.print("Flow: ");
    lcd.print(flowRate);

    lcd.setCursor(0,2);
    lcd.print("Level: ");
    if (levelDisplay = 1) {
      lcd.print("Liquid");
    } else {
      lcd.print("No Liquid");
    }

    lcd.setCursor(0,3);
    lcd.print("Env: ");
    if (currentEnv == BETTA) {
      lcd.print("BETTA");
    } else if (currentEnv == GUPPY) {
      lcd.print("GUPPY");
    }

    lcd.setCursor(15,3);
    if (isDay = true) {
      lcd.print("Day");
    } else {
      lcd.print("Night");
    }
    LastLcdTime = currentMillis;

  }

}


  