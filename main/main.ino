/*  Omnimagnet Thermocouple Teensy Code
************************************************
************************************************
*** Dominick Ropella                         ***
*** Vanderbilt University 2019               ***
*** V1.2 Stable Release                      ***
*** 6/27/19                                  ***
*** Contact: dominick.ropella@vanderbilt.edu ***
************************************************  
************************************************
*/

#include <Adafruit_MAX31856.h> //Thermocouple header
#include "Wire.h"  //for I2C comm for OLED screen
#include "SPI.h"  //for SPI comm for thermocouple boards
#include <Adafruit_GFX.h>  //Graphics header
#include <Adafruit_SSD1306.h> //OLED screen header

// Initialize thermocouple objects and setup hardware SPI pins for each
// Using software SPI: input any 4 digital I/O pins, order: CS, DI, DO, CLK
// Using hardware SPI: input CS pin. Can be any digital I/O pin as well
Adafruit_MAX31856 innerCoil = Adafruit_MAX31856(10);
Adafruit_MAX31856 middleCoil = Adafruit_MAX31856(9);
Adafruit_MAX31856 outerCoil = Adafruit_MAX31856(8);

// Pin for over-temperature system shutdown and temperature limit
const int overtempPinInner = 0;
const int overtempPinMiddle = 1;
const int overtempPinOuter = 2;
const int faultPin = 3;
const int masterSwitch = 4;
const float maxTempLimit = 30.0; //Celcius
const float lowerThreshold = 5.0; //Celcius lower bound for indicating errors
const int loopCount = 5;
int faultLoops = 0;
int safetyChecks = 0;

//OLED Screen Setup
const int OLED_RESET = 14;
Adafruit_SSD1306 display(OLED_RESET); //reset (wipe) display
const int pixPosUnitX = 110; //horizontal spacing for Celcius label
const int pixOffsetX = 60; //horizontal spacing for equals sign
const int pixOffsetX2 = 70; //horizontal spacing for Temperature readings
const int pixVert = 8; //vertical pixel row spacing unit going from top to bottom
const int barLength = 30; //pixel length of rectangle that clears temp data field on screen

//Temperature Readings Variables and Sensor Faults. Faults initialized to no errors. 
float innerTemp = 0.0;
float middleTemp = 0.0;
float outerTemp = 0.0;
uint8_t innerFault = 0;
uint8_t middleFault = 0;
uint8_t outerFault = 0;

void setup() {
  delay(1000); //This delay ensures successful startup when using poor power supplies
  Serial.begin(115200); //Speed doesn't matter here since teensy runs on native USB

  //Start the display
  display.begin(SSD1306_SWITCHCAPVCC, 0X3C); //initialize with the I2C addr 0X3D (for the 128x64)
  display.setRotation(4); //long side is horizontal with pins on bottom side of board
  display.clearDisplay();
  display.display(); //turns on display and clears
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.setTextColor(WHITE);
  display.println("Starting");
  display.print("System");
  display.display(); //displays starting sensor text at beginning
  delay(500); //Only for 0.5 seconds
  
  // setup static text (so we don't waste time refreshing it over and over)
  display.clearDisplay();
  display.display(); //update the display to clear
  //text properties
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(WHITE);
  // Display Bx field text, equals sign, and units (first row)
  display.print("Inner Coil");
  display.setCursor(pixOffsetX,0);
  display.print("=");
  display.setCursor(pixPosUnitX, 0);
  display.print("C");
  // Display By field text, equals sign, and units (second row)
  display.setCursor(0, pixVert);
  display.print("Mid   Coil");
  display.setCursor(pixOffsetX,pixVert);
  display.print("=");
  display.setCursor(pixPosUnitX, pixVert);
  display.println("C");
  // Display Bz field text, equals sign, and units (third row)
  display.setCursor(0, pixVert * 2);
  display.print("Outer Coil");
  display.setCursor(pixOffsetX,pixVert * 2);
  display.print("=");
  display.setCursor(pixPosUnitX, pixVert * 2);
  display.println("C");
  display.display();

  //Set fault pin type and set intially to no fault
  pinMode(overtempPinInner, OUTPUT);
  digitalWriteFast(overtempPinInner, LOW);
  pinMode(overtempPinMiddle, OUTPUT);
  digitalWriteFast(overtempPinMiddle, LOW);
  pinMode(overtempPinOuter, OUTPUT);
  digitalWriteFast(overtempPinOuter, LOW);
  pinMode(faultPin, OUTPUT);
  digitalWriteFast(faultPin, LOW);
  pinMode(masterSwitch, OUTPUT);
  digitalWriteFast(masterSwitch, LOW);

  //Begin each coil thermocouple sensor board communication
  innerCoil.begin();
  middleCoil.begin();
  outerCoil.begin();

  //Set the thermocouple type for each (all type K for now)
  innerCoil.setThermocoupleType(MAX31856_TCTYPE_K);
  middleCoil.setThermocoupleType(MAX31856_TCTYPE_K);
  outerCoil.setThermocoupleType(MAX31856_TCTYPE_K);
}

void loop() {

  //Clear temperature areas of display
  display.fillRect(pixOffsetX2,         0, barLength, pixVert, BLACK);//clear text area for inner temp
  display.fillRect(pixOffsetX2,   pixVert, barLength, pixVert, BLACK);//clear text area for mid temp
  display.fillRect(pixOffsetX2, 2*pixVert, barLength, pixVert, BLACK);//clear text area for outer temp

  //Record new readings from thermocouples
  innerTemp  = innerCoil.readThermocoupleTemperature();
  middleTemp = middleCoil.readThermocoupleTemperature();
  outerTemp  = outerCoil.readThermocoupleTemperature();

  //Display new temperature readings
  display.setCursor(pixOffsetX2,0);
  display.print(innerTemp);
  display.setCursor(pixOffsetX2, pixVert);
  display.print(middleTemp);
  display.setCursor(pixOffsetX2, 2*pixVert);
  display.print(outerTemp);
  display.display();
  
  // Check any faults
  innerFault  = innerCoil.readFault();
  middleFault = middleCoil.readFault();
  outerFault  = outerCoil.readFault();

  //Blink twice quickly if any faults occur
  if (innerFault != 0 || middleFault != 0 || outerFault != 0 || innerTemp < lowerThreshold || middleTemp < lowerThreshold || outerTemp < lowerThreshold) {
    faultLoops = faultLoops + 1;
    if (faultLoops >= loopCount){
      digitalWriteFast(faultPin, HIGH);
      digitalWriteFast(masterSwitch, HIGH);
    }
  }
  else {
    faultLoops = 0;
    safetyChecks = safetyChecks + 1;
    digitalWriteFast(faultPin, LOW);
  }

  //Set overtemp pin to HIGH if temperature is over threshold on any thermocouple
  if (innerTemp >= maxTempLimit){
    digitalWriteFast(overtempPinInner, HIGH);
    digitalWriteFast(masterSwitch, HIGH);
  }
  else {
    digitalWriteFast(overtempPinInner, LOW);
    safetyChecks = safetyChecks + 1;
  }

  if (middleTemp >= maxTempLimit) {
    digitalWriteFast(overtempPinMiddle, HIGH);
    digitalWriteFast(masterSwitch, HIGH);
  }
  else {
    digitalWriteFast(overtempPinMiddle, LOW);
    safetyChecks = safetyChecks + 1;
  }
  
  if (outerTemp >= maxTempLimit) {
    digitalWriteFast(overtempPinOuter, HIGH);
    digitalWriteFast(masterSwitch, HIGH);
  }
  else {
    digitalWriteFast(overtempPinOuter, LOW);
    safetyChecks = safetyChecks + 1;
  }

  if (safetyChecks == 4) {
    digitalWriteFast(masterSwitch, LOW);
    safetyChecks = 0;
  }
  else {
    safetyChecks = 0;
  }
  delay(10);
}
