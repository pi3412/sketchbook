// this example is public domain. enjoy!
// www.ladyada.net/learn/sensors/thermocouple

#include "max6675.h"

int thermoDO = 4;
int thermoCS = 5;
int thermoCLK = 6;

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
int vccPin = 3;
int gndPin = 2;

#include <TM1637Display.h>

// Module connection pins (Digital Pins)
#define CLK 2
#define DIO 3
TM1637Display display(CLK, DIO);


void setup() {
  Serial.begin(9600);
  // use Arduino pins
  pinMode(vccPin, OUTPUT); digitalWrite(vccPin, HIGH);
  pinMode(gndPin, OUTPUT); digitalWrite(gndPin, LOW);

  Serial.println("MAX6675 test");
  // wait for MAX chip to stabilize
  delay(500);
  
  display.setBrightness(7);

}

void loop() {
  // basic readout test, just print the current temp

  Serial.print("C = ");
  Serial.println(thermocouple.readCelsius());

  display.showNumberDec(round(thermocouple.readCelsius()), 0);

  delay(1000);


}
