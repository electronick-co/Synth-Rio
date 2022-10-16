void setup() {
 Serial.begin(9600); //Baud rate: 9600
}
void loop() {
 int sensorValue = analogRead(A4);// read the input on analog pin 0:
 float voltage = sensorValue * (5.0 / 1024.0); // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
 int mic = (sensorValue - 2000) * 2;
 Serial.println(sensorValue); // print out the value you read:
 //delay(200);
} 
