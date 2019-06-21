#include <Adafruit_MAX31856.h>
#include "Wire.h"  //for I2C comm
#include "SPI.h"  //for SPI comm
#include <Adafruit_GFX.h>  //Graphics header
#include <Adafruit_SSD1306.h> //OLED Screen header

// Initialize thermocouple objects and setup software SPI pins for each
// Use software SPI: CS, DI, DO, CLK
Adafruit_MAX31856 innerCoil = Adafruit_MAX31856(10);
Adafruit_MAX31856 middleCoil = Adafruit_MAX31856(9);
Adafruit_MAX31856 outerCoil = Adafruit_MAX31856(8);

int overtempPin = 0;

//OLED Screen Setup
#define OLED_RESET 14
Adafruit_SSD1306 display(OLED_RESET); //reset (wipe) display
const int pixPosUnitX = 110;
const int pixOffsetX = 23; //for equals sign
const int pixVert = 8; //vertical pixel spacing unit going from top to bottom
const int pixLoopTimeX = 100;
const int pixLoopTimeY = 24;
int16_t  X1, Y1;
int16_t  X2, Y2;
int16_t  X3, Y3;
int16_t  X4, Y4;
const bool display_flag = true; //choose to display field data or not.

//Temperature Readings Variables
float innerTemp, middleTemp, outerTemp;

void setup() {
  delay(1000);
  Serial.begin(115200);
  SPI.begin();
  delay(500);
  
  Serial.println("Omnimag thermocouple test");
  delay(500);

  //Start the display
  display.begin(SSD1306_SWITCHCAPVCC, 0X3C);  // initialize with the I2C addr 0X3D (for the 128x64)
  display.setRotation(4); //long side is horizontal with pins on right side of board
  display.clearDisplay();
  display.display(); //turns on display and clears
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.setTextColor(WHITE);
  display.println("Starting");
  display.print("System");
  display.display(); //displays starting sensor text at beginning
  delay(500);
  // setup static text (so we don't waste time refreshing it over and over)
  display.clearDisplay();
  display.display(); //update the display to clear
  //text properties
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(WHITE);
  // Display Bx field text, equals sign, and units (first row)
  display.print("Inner Coil");
  display.setCursor(60,0);
  display.print("=");
  display.setCursor(pixPosUnitX, 0);
  display.print("C");
  // Display By field text, equals sign, and units (second row)
  display.setCursor(0, pixVert);
  display.print("Mid   Coil");
  display.setCursor(60,pixVert);
  display.print("=");
  display.setCursor(pixPosUnitX, pixVert);
  display.println("C");
  // Display Bz field text, equals sign, and units (third row)
  display.setCursor(0, pixVert * 2);
  display.print("Outer Coil");
  display.setCursor(60,pixVert*2);
  display.print("=");
  display.setCursor(pixPosUnitX, pixVert * 2);
  display.println("C");
  display.display();

  //Set fault pin type 
  pinMode(overtempPin, OUTPUT);
  digitalWriteFast(overtempPin, LOW);

  //Begin each coil thermocouple sensor board communication
  innerCoil.begin();
  middleCoil.begin();
  outerCoil.begin();

  //Set the thermocouple type for each (all type K for now)
  innerCoil.setThermocoupleType(MAX31856_TCTYPE_K);
  middleCoil.setThermocoupleType(MAX31856_TCTYPE_K);
  outerCoil.setThermocoupleType(MAX31856_TCTYPE_K);
  delay(2000);

}

void loop() {
  display.fillRect(70, 0, 30, 8, BLACK); // clear text area for inner coil temp
  display.fillRect(70, 8, 30, 8, BLACK); // clear text area for middle coil temp
  display.fillRect(70,16, 30, 8, BLACK); // clear text area for outer coil temp

  innerTemp = innerCoil.readThermocoupleTemperature();
  middleTemp = middleCoil.readThermocoupleTemperature();
  outerTemp = outerCoil.readThermocoupleTemperature();
  
  display.setCursor(70,0);
  display.print(innerTemp);
  display.setCursor(70,8);
  display.print(middleTemp);
  display.setCursor(70,16);
  display.print(outerTemp);
  
  display.display();
  // Check and print any faults
  uint8_t innerFault = innerCoil.readFault();
  uint8_t middleFault = middleCoil.readFault();
  uint8_t outerFault = outerCoil.readFault();
  

  if (innerFault != 0 || middleFault != 0 || outerFault != 0) {
  digitalWriteFast(overtempPin, HIGH);
  delay(100);
  digitalWriteFast(overtempPin, LOW);
  delay(100);
  digitalWriteFast(overtempPin, HIGH);
  delay(100);
  digitalWriteFast(overtempPin, LOW);
  delay(100);
  }
  if (innerTemp >= 30.0 || middleTemp >= 30.0 || outerTemp >= 30.0) {
  digitalWriteFast(overtempPin, HIGH);
  }
  else {
  digitalWriteFast(overtempPin, LOW);
  }
  delay(100);
}
