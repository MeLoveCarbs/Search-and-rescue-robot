#define S0 4
#define S1 7
#define S2 12
#define S3 13
#define sensorOut A5
int frequency = 0;

void setup() {
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);

  // Setting frequency-scaling to 20%
  digitalWrite(S0,HIGH);
  digitalWrite(S1,LOW);
  
  Serial.begin(57600);
}
void loop() {
  
  int redfreq = 0;
  int greenfreq = 0;
  for (int i = 0; i < 50; i++) {
  digitalWrite(S2,LOW);
  digitalWrite(S3,LOW);
  // Reading the output frequency
  redfreq += pulseIn(sensorOut, 0);
  digitalWrite(S2,HIGH);
  digitalWrite(S3,HIGH);
  // Reading the output frequency
  greenfreq += pulseIn(sensorOut, 0);
  }
  redfreq /= 50;
  greenfreq /= 50;
  float ratio = (float)redfreq / (float)greenfreq;
  Serial.print("R= ");
  Serial.print(redfreq);
  Serial.print(" ");
  Serial.print("G= ");//printing name
  Serial.print(greenfreq);//printing RED color frequency
  Serial.print("  ");
  Serial.println(ratio);
  delay(100);

/* white is 70, 22
 *  red is 120+, <70
 *  green is 260+, 90+
 */
 
/*  //blue
  digitalWrite(S2,LOW);
  digitalWrite(S3,HIGH);
  // Reading the output frequency
  frequency = pulseIn(sensorOut, 0);
  //Remaping the value of the frequency to the RGB Model of 0 to 255
  frequency = map(frequency, 25,70,255,0);
  // Printing the value on the serial monitor
  Serial.print("B= ");//printing name
  Serial.print(frequency);//printing RED color frequency
  Serial.println("  ");
  delay(100);*/
  
}
