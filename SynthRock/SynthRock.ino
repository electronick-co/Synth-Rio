#include "max6675.h"

int thermoDO = 19;
int thermoCS = 23;
int thermoCLK = 5;

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

void setup() {
 Serial.begin(9600); //Baud rate: 9600

 Serial.println("Triple sensor test");
  // wait for MAX chip to stabilize
  delay(500);
}


void loop() 
{
 int turbity = analogRead(A4);// read the input on analog pin 0:
 int mic = analogRead(A5);
 //float voltage = sensorValue * (5.0 / 1024.0); // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
 //int mic = (sensorValue - 2000) * 2;

 // Print information
 Serial.print(thermocouple.readCelsius()*100);
 Serial.print(',');
 Serial.print(mic);
 Serial.print(',');
 Serial.println(turbity); // print out the value you read:
 delay(100);
} 
