/*  Omnimagnet Thermocouple Teensy Code
************************************************
************************************************
*** Dominick Ropella                         ***
*** Vanderbilt University 2019               ***
*** V1.4 Stable Release                      ***
*** 7/10/19                                  ***
*** Contact: dominick.ropella@vanderbilt.edu ***
************************************************  
************************************************
*/

#include <ros.h> //enable ROS communications
#include <geometry_msgs/Vector3.h> //ROS 3d vector msg type
#include <std_msgs/Bool.h> //ROS bool msg type
#include <Adafruit_MAX31856.h> //Thermocouple header
#include "Wire.h"  //for I2C comm for OLED screen
#include "SPI.h"  //for SPI comm for thermocouple boards
#include <Adafruit_GFX.h>  //Graphics header
#include <Adafruit_SSD1306.h> //OLED screen header

// Set up ROS node and publisher for coil temps and thermal shutdown activated
geometry_msgs::Vector3 coil_temps_msg;
std_msgs::Bool thermal_stop_msg;
ros::Publisher pub_field("Coil_Temps", &coil_temps_msg);
ros::Publisher pub_field2("Thermal_Stop", &thermal_stop_msg);
ros::NodeHandle nh;

// Initialize thermocouple objects and setup hardware SPI pins for each
// Using software SPI: input any 4 digital I/O pins, order: CS, DI, DO, CLK
// Using hardware SPI: input CS pin. Can be any digital I/O pin as well
Adafruit_MAX31856 innerCoil = Adafruit_MAX31856(10);
Adafruit_MAX31856 middleCoil = Adafruit_MAX31856(9);
Adafruit_MAX31856 outerCoil = Adafruit_MAX31856(8);

// Pins for over-temperature, generic fault, and master shutdown pin
const int overtempPinInner = 0;
const int overtempPinMiddle = 1;
const int overtempPinOuter = 2;
const int faultPin = 3;  //indicates fault detected on any thermocouple board
const int masterSwitch = 4; //final safety digital logic to interface with e-stop switch
const float maxTempLimit = 30.0; //Celcius, maxiumum allowable temp for any coil.
const float lowerThreshold = 5.0; //Celcius lower bound for indicating errors sometimes not detected by thermocouple boards

// Fault delay and checks for all systems OK
const int loopCount = 5; //number of loops that the fault pin must be active to trigger masterSwitch
int faultLoops = 0; //Initial value for the number of consecutive loops that the fault pin has been active
int safetyChecks = 0; //This is a running sum that resets every loop. Each safety check contributes +1 to the sum, and it must be a certain value to avoid triggering master switch.

// OLED Screen Setup
const int OLED_RESET = 14;
Adafruit_SSD1306 display(OLED_RESET); //reset (wipe) display
const int pixPosUnitX = 110; //horizontal spacing for Celcius label
const int pixOffsetX = 60; //horizontal spacing for equals sign
const int pixOffsetX2 = 70; //horizontal spacing for Temperature readings
const int pixVert = 8; //vertical pixel row spacing unit going from top to bottom
const int barLength = 30; //pixel length of rectangle that clears temp data field on screen

// Temperature Readings Variables and Sensor Faults. Faults initialized to no errors. 
float innerTemp = 0.0;
float middleTemp = 0.0;
float outerTemp = 0.0;
uint8_t innerFault = 0;
uint8_t middleFault = 0;
uint8_t outerFault = 0;

void setup() {
  delay(1000); //This delay ensures successful startup when using low-quality power supplies
  Serial.begin(115200); //Speed doesn't matter here since teensy runs on native USB

  // Start the display
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
  
  // text properties
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

  // Update display screen
  display.display();

  // Set all status pins to ouputs and set to low values initially
  pinMode(overtempPinInner, OUTPUT);
  digitalWriteFast(overtempPinInner, LOW);
  pinMode(overtempPinMiddle, OUTPUT);
  digitalWriteFast(overtempPinMiddle, LOW);
  pinMode(overtempPinOuter, OUTPUT);
  digitalWriteFast(overtempPinOuter, LOW);
  pinMode(faultPin, OUTPUT);
  digitalWriteFast(faultPin, LOW);

  // Master Switch is active low, meaning shut off when the logic level is low. Initialize to high
  pinMode(masterSwitch, OUTPUT);
  digitalWriteFast(masterSwitch, HIGH);

  // Begin each coil thermocouple sensor board communication
  innerCoil.begin();
  middleCoil.begin();
  outerCoil.begin();

  // Set the thermocouple type for each (all type K for now)
  innerCoil.setThermocoupleType(MAX31856_TCTYPE_K);
  middleCoil.setThermocoupleType(MAX31856_TCTYPE_K);
  outerCoil.setThermocoupleType(MAX31856_TCTYPE_K);

  //Initialize ROS Node and add two publishers
  nh.initNode();
  nh.advertise(pub_field);
  nh.advertise(pub_field2);
}

void loop() {

  // Clear temperature areas of display
  display.fillRect(pixOffsetX2,         0, barLength, pixVert, BLACK);//clear text area for inner temp
  display.fillRect(pixOffsetX2,   pixVert, barLength, pixVert, BLACK);//clear text area for mid temp
  display.fillRect(pixOffsetX2, 2*pixVert, barLength, pixVert, BLACK);//clear text area for outer temp

  // Record new readings from thermocouples
  innerTemp  = innerCoil.readThermocoupleTemperature();
  middleTemp = middleCoil.readThermocoupleTemperature();
  outerTemp  = outerCoil.readThermocoupleTemperature();

  // Display new temperature readings
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

  // Indicate if any faults have occured 
  if (innerFault != 0 || middleFault != 0 || outerFault != 0 || innerTemp < lowerThreshold || middleTemp < lowerThreshold || outerTemp < lowerThreshold) {
    faultLoops = faultLoops + 1; // add one to the fault loops total
    if (faultLoops >= loopCount){ // if the faults have existed for 5 loops, trigger master switch
      digitalWriteFast(faultPin, HIGH);
    }
  }
  else {
    faultLoops = 0; //reset fault loops counter
    safetyChecks = safetyChecks + 1; // safety checks gets +1 added to total
    digitalWriteFast(faultPin, LOW);
  }

  // Set overtemp pin to HIGH if temperature is over threshold on any thermocouple
  if (innerTemp >= maxTempLimit){
    digitalWriteFast(overtempPinInner, HIGH);
  }
  else {
    digitalWriteFast(overtempPinInner, LOW);
    safetyChecks = safetyChecks + 1;
  }

  if (middleTemp >= maxTempLimit) {
    digitalWriteFast(overtempPinMiddle, HIGH);
  }
  else {
    digitalWriteFast(overtempPinMiddle, LOW);
    safetyChecks = safetyChecks + 1;
  }
  
  if (outerTemp >= maxTempLimit) {
    digitalWriteFast(overtempPinOuter, HIGH);
  }
  else {
    digitalWriteFast(overtempPinOuter, LOW);
    safetyChecks = safetyChecks + 1;
  }

  if (safetyChecks == 4) { // all 4 checks must pass to keep master switch high
    digitalWriteFast(masterSwitch, HIGH);
    thermal_stop_msg.data = false;
    safetyChecks = 0; // reset safety checks for next loop
  }
  else {
    digitalWriteFast(masterSwitch, LOW);
    thermal_stop_msg.data = true;
    safetyChecks = 0; 
  }
  if(nh.connected()){
    coil_temps_msg.x = innerTemp;
    coil_temps_msg.y = middleTemp;
    coil_temps_msg.z = outerTemp;
    
    pub_field.publish(&coil_temps_msg);
    pub_field2.publish(&thermal_stop_msg);
  }
  nh.spinOnce();
  delay(10);
}
