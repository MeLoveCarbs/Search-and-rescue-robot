#include <serialize.h>
#include <util/delay.h>
#include "packet.h"
#include "constants.h"

typedef enum
{
   STOP=0,
   FORWARD=1,
   BACKWARD=2,
   LEFT=3,
   RIGHT=4,
} TDirection;


char ultramsg[] = "Forward too close!";
char irmsg[] = "Side too close!";
int ultraflag = 0;
volatile int irflag = 0;
volatile int uflag = 0;

volatile TDirection dir = STOP;
/*
 * Alex's configuration constants
 */

// Number of ticks per revolution from the 
// wheel encoder.

#define COUNTS_PER_REV      192

// Wheel circumference in cm.
// We will use this to calculate forward/backward distance traveled 
// by taking revs * WHEEL_CIRC
//PI, for calculating turn circumference
//#define PI  3.141592654
#define WHEEL_CIRC         20.1 

//Alex's length and breadth in cm
#define ALEX_LENGTH 18
#define ALEX_BREADTH 11

// Motor control pins. You need to adjust these till
// Alex moves in the correct direction
#define LF                  6   // Left forward pin
#define LR                  5   // Left reverse pin
#define RF                  10  // Right forward pin
#define RR                  11  // Right reverse pin

#define S0 4
#define S1 7
#define S2 12
#define S3 13
#define sensorOut A5
/*
 *    Alex's State Variables
 */

// Store the ticks from Alex's left and
// right encoders.
volatile unsigned long leftForwardTicks; 
volatile unsigned long rightForwardTicks;
volatile unsigned long leftReverseTicks; 
volatile unsigned long rightReverseTicks;

// Store the ticks turns from Alex's left and
// right encoders.
volatile unsigned long leftForwardTicksTurns; 
volatile unsigned long rightForwardTicksTurns;
volatile unsigned long leftReverseTicksTurns; 
volatile unsigned long rightReverseTicksTurns;

// Store the revolutions on Alex's left
// and right wheels
volatile unsigned long leftRevs;
volatile unsigned long rightRevs;

// Forward and backward distance traveled
volatile unsigned long forwardDist;
volatile unsigned long reverseDist;


//Variables to keep track of whether we have moved a commanded distance
unsigned long deltaDist;
unsigned long newDist;

//Alex's diagonal. Compute and store once
float AlexDiagonal = 21.1;
float AlexCirc = 66.3;

//Variables to keep track of our turning angle
unsigned long deltaTicks;
unsigned long targetTicks;

/*
 * 
 * Alex Communication Routines.
 * 
 */
 
TResult readPacket(TPacket *packet)
{
    // Reads in data from the serial port and
    // deserializes it.Returns deserialized
    // data in "packet".
    
    char buffer[PACKET_SIZE];
    int len;

    len = readSerial(buffer);

    if(len == 0)
      return PACKET_INCOMPLETE;
    else
      return deserialize(buffer, len, packet);
    
}

void sendStatus()
{
  // Implement code to send back a packet containing key
  // information like leftTicks, rightTicks, leftRevs, rightRevs
  // forwardDist and reverseDist
  // Use the params array to store this information, and set the
  // packetType and command files accordingly, then use sendResponse
  // to send out the packet. See sendMessage on how to use sendResponse.
  //
  TPacket statusPacket;
  statusPacket.packetType = PACKET_TYPE_RESPONSE;
  statusPacket.command = RESP_STATUS;
  statusPacket.params[0] = leftForwardTicks;
  statusPacket.params[1] = rightForwardTicks;
  statusPacket.params[2] = leftReverseTicks;
  statusPacket.params[3] = rightReverseTicks;
  statusPacket.params[4] = leftForwardTicksTurns;
  statusPacket.params[5] = rightForwardTicksTurns;
  statusPacket.params[6] = leftReverseTicksTurns;
  statusPacket.params[7] = rightReverseTicksTurns;
  statusPacket.params[8] = forwardDist;
  statusPacket.params[9] = reverseDist;
  sendResponse(&statusPacket);
}

void sendMessage(const char *message)
{
  // Sends text messages back to the Pi. Useful
  // for debugging.
  
  TPacket messagePacket;
  messagePacket.packetType=PACKET_TYPE_MESSAGE;
  strncpy(messagePacket.data, message, MAX_STR_LEN);
  sendResponse(&messagePacket);
}

void sendBadPacket()
{
  // Tell the Pi that it sent us a packet with a bad
  // magic number.
  
  TPacket badPacket;
  badPacket.packetType = PACKET_TYPE_ERROR;
  badPacket.command = RESP_BAD_PACKET;
  sendResponse(&badPacket);
  
}

void sendBadChecksum()
{
  // Tell the Pi that it sent us a packet with a bad
  // checksum.
  
  TPacket badChecksum;
  badChecksum.packetType = PACKET_TYPE_ERROR;
  badChecksum.command = RESP_BAD_CHECKSUM;
  sendResponse(&badChecksum);  
}

void sendBadCommand()
{
  // Tell the Pi that we don't understand its
  // command sent to us.
  
  TPacket badCommand;
  badCommand.packetType=PACKET_TYPE_ERROR;
  badCommand.command=RESP_BAD_COMMAND;
  sendResponse(&badCommand);

}

void sendBadResponse()
{
  TPacket badResponse;
  badResponse.packetType = PACKET_TYPE_ERROR;
  badResponse.command = RESP_BAD_RESPONSE;
  sendResponse(&badResponse);
}

void sendOK()
{
  TPacket okPacket;
  okPacket.packetType = PACKET_TYPE_RESPONSE;
  okPacket.command = RESP_OK;
  sendResponse(&okPacket);  
}

void sendResponse(TPacket *packet)
{
  // Takes a packet, serializes it then sends it out
  // over the serial port.
  char buffer[PACKET_SIZE];
  int len;

  len = serialize(buffer, packet, sizeof(TPacket));
  writeSerial(buffer, len);
}


/*
 * Setup and start codes for external interrupts and 
 * pullup resistors.
 * 
 */
// Enable pull up resistors on pins 2 and 3
void enablePullups()
{
  // Use bare-metal to enable the pull-up resistors on pins
  // 2 and 3. These are pins PD2 and PD3 respectively.
  // We set bits 2 and 3 in DDRD to 0 to make them inputs. 

  DDRD  &= 0b11110011;
  PORTD |= 0b00001100;
}

// Functions to be called by INT0 and INT1 ISRs.
void leftISR()
{
   // We calculate forwardDist and reverseDist only in leftISR because we
  // assume that the left and right wheels move at the same
  // time.
  if (dir == FORWARD) {
    leftForwardTicks++;
    forwardDist = (unsigned long) ((float)leftForwardTicks / COUNTS_PER_REV * WHEEL_CIRC);
  }
  else if (dir == BACKWARD) {
    leftReverseTicks++;
    reverseDist = (unsigned long) ((float)leftReverseTicks / COUNTS_PER_REV * WHEEL_CIRC);
  }
  else if (dir == LEFT) {
    leftReverseTicksTurns++;
  }
  else if (dir == RIGHT) {
    leftForwardTicksTurns++;
  }
}

void rightISR()
{
  if (dir == FORWARD) {
    rightForwardTicks++;
  }
  else if (dir == BACKWARD) {
    rightReverseTicks++;
  }
  else if (dir == LEFT) {
    rightForwardTicksTurns++;
  }
  else if (dir == RIGHT) {
    rightReverseTicksTurns++;
  }
}

// Set up the external interrupt pins INT0 and INT1
// for falling edge triggered. Use bare-metal.
void setupEINT()
{
  // Use bare-metal to configure pins 2 and 3 to be
  // falling edge triggered. Remember to enable
  // the INT0 and INT1 interrupts.

  EICRA = 0b00001010;
  EIMSK = 0b00000011;
}

//set up pinchange interrupts register
void setupPCICR(){
 //pinchange interrupt set
  PCICR |= 0b00000011;
  
  //mask
  PCMSK1 |= 0b00001100;
}


// Implement the external interrupt ISRs below.
// INT0 ISR should call leftISR while INT1 ISR
// should call rightISR.

ISR(INT0_vect)
{
  leftISR();
}

ISR(INT1_vect)
{
  rightISR();
}


// Implement INT0 and INT1 ISRs above.

/*
 * Setup and start codes for serial communications
 * 
 */
// Set up the serial connection. For now we are using 
// Arduino Wiring, you will replace this later
// with bare-metal code.
void setupSerial()
{
  // To replace later with bare-metal.
  Serial.begin(57600);
}

// Start the serial connection. For now we are using
// Arduino wiring and this function is empty. We will
// replace this later with bare-metal code.

void startSerial()
{
  // Empty for now. To be replaced with bare-metal code
  // later on.
  
}

// Read the serial port. Returns the read character in
// ch if available. Also returns TRUE if ch is valid. 
// This will be replaced later with bare-metal code.

int readSerial(char *buffer)
{

  int count=0;

  while(Serial.available())
    buffer[count++] = Serial.read();

  return count;
}

// Write to the serial port. Replaced later with
// bare-metal code

void writeSerial(const char *buffer, int len)
{
  Serial.write(buffer, len);
}

/*
 * Alex's motor drivers.
 * 
 */

// Set up Alex's motors. Right now this is empty, but
// later you will replace it with code to set up the PWMs
// to drive the motors.
void setupMotors()
{
  /* Our motor set up is:  
   *    A1IN - Pin 5, PD5, OC0B
   *    A2IN - Pin 6, PD6, OC0A
   *    B1IN - Pin 10, PB2, OC1B
   *    B2In - pIN 11, PB3, OC2A
   */
}

// Start the PWM for Alex's motors.
// We will implement this later. For now it is
// blank.
void startMotors()
{
  
}

// Convert percentages to PWM values
int pwmVal(float speed)
{
  if(speed < 0.0)
    speed = 0;

  if(speed > 100.0)
    speed = 100.0;

  return (int) ((speed / 100.0) * 255.0);
}

// Move Alex forward "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// move forward at half speed.
// Specifying a distance of 0 means Alex will
// continue moving forward indefinitely.
void forward(float dist, float speed)
{
 
  if (dist > 0)
    deltaDist = dist;
  else
    deltaDist = 9999999;
  newDist = forwardDist + deltaDist;

  dir = FORWARD;
  int val = pwmVal(speed);
  // For now we will ignore  dist and move
  // forward indefinitely. We will fix this
  // in Week 9.

  // LF = Left forward pin, LR = Left reverse pin
  // RF = Right forward pin, RR = Right reverse pin
  // This will be replaced later with bare-metal code.

  analogWrite(LF, val);
  analogWrite(RF, (0.95 * val));
  analogWrite(LR, 0);
  analogWrite(RR, 0);
}

ISR(PCINT1_vect){
  if (irflag == 0) {
    //stop();
     sendMessage(irmsg);
  }
  irflag = 1; 
}

// Reverse Alex "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// reverse at half speed.
// Specifying a distance of 0 means Alex will
// continue reversing indefinitely.
void reverse(float dist, float speed)
{
  if (dist > 0)
    deltaDist = dist;
  else
    deltaDist = 9999999;
  newDist = reverseDist + deltaDist;

  dir = BACKWARD;
  int val = pwmVal(speed);
  // For now we will ignore dist and
  // reverse indefinitely. We will fix this
  // in Week 9.

  // LF = Left forward pin, LR = Left reverse pin
  // RF = Right forward pin, RR = Right reverse pin
  // This will be replaced later with bare-metal code.
  analogWrite(LR, val);
  analogWrite(RR,  (0.85 * val));
  analogWrite(LF, 0);
  analogWrite(RF, 0);
}
//Compute Delta Ticks function
unsigned long computeDeltaTicks(float ang) {
  unsigned long ticks = (unsigned long) ((0.7 * ang * AlexCirc * COUNTS_PER_REV) / (360.0 * WHEEL_CIRC));
  return ticks;
}

// Turn Alex left "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn left indefinitely.
void left(float ang, float speed)
{
 int val = pwmVal(speed);
  dir = RIGHT;
  if (ang == 0) {
    deltaTicks = 99999999;
  } else{
    deltaTicks = computeDeltaTicks(ang);
  }
  targetTicks = rightReverseTicksTurns + (float)(0.8 * deltaTicks);
  

  // For now we will ignore ang. We will fix this in Week 9.
  // We will also replace this code with bare-metal later.
  // To turn right we reverse the right wheel and move
  // the left wheel forward.
  analogWrite(RR, 0);
  analogWrite(LF, 0);
  analogWrite(LR, val);
  analogWrite(RF, val);
}

// Turn Alex right "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn right indefinitely.
void right(float ang, float speed)
{
 int val = (pwmVal(speed));
  dir = RIGHT;

  if (ang == 0) {
    deltaTicks = 99999999;
  } else{
    deltaTicks = computeDeltaTicks(ang);
  }
  targetTicks = rightReverseTicksTurns + (float)(0.6 * deltaTicks);
  

  // For now we will ignore ang. We will fix this in Week 9.
  // We will also replace this code with bare-metal later.
  // To turn right we reverse the right wheel and move
  // the left wheel forward.
  analogWrite(RR, val);
  analogWrite(LF, val);
  analogWrite(LR, 0);
  analogWrite(RF, 0);
}

// Stop Alex. To replace with bare-metal code later.
void stop()
{
  dir = STOP;
  analogWrite(LF, 0);
  analogWrite(LR, 0);
  analogWrite(RF, 0);
  analogWrite(RR, 0);
}
  char redmsg[] = "Red object";
  char greenmsg[] = "Green object";
  //char trymsg[] = "White object";
  char nothingmsg[] = "No object";
  char trymsg[] = "Try again";
void colour()
{
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
  //map(redfreq, 80, 100, 255, 0);
  //map(greenfreq, 200, 130, 255, 0);
  float ratio = (float)redfreq / (float)greenfreq; 
  /*if(redfreq < 140 && greenfreq < 150) {
    if (greenfreq - redfreq >= 8) {
      sendMessage(trymsg);
    } 
  } else {
    if ((greenfreq - redfreq) >= 40) {
      sendMessage(redmsg);
    } else {
      sendMessage(nothingmsg);
    }
  }*/
  
   /*if (ratio < 0.7) {
    sendMessage(redmsg);
  } else if ((greenfreq >= 140 && ratio > 0.85) ) {
    sendMessage(greenmsg);
  } else if ((redfreq - greenfreq <= 7) || (greenfreq - redfreq <= 7)) {
    sendMessage(trymsg);
  } else {
    sendMessage(trymsg);
  }
  }
  */
  
 // if (redfreq <= 200 && greenfreq <= 200) {
    if ((greenfreq - redfreq) >= 20) {
      sendMessage(redmsg);
    } else if ((greenfreq - redfreq) < 5 || redfreq - greenfreq < 5) {
      sendMessage(greenmsg);
    } else {
      sendMessage(trymsg);
    }
}
    
  //} else {
   // sendMessage(nothingmsg);
  

/*
 * Alex's setup and run codes
 * 
 */

// Clears all our counters
void clearCounters()
{
  rightReverseTicksTurns=0;
  leftReverseTicksTurns=0;
  rightForwardTicksTurns=0;
  leftForwardTicksTurns=0;
  leftRevs=0;
  rightRevs=0;
  forwardDist=0;
  reverseDist=0; 
  leftForwardTicks=0;
  rightForwardTicks=0;
  leftReverseTicks=0; 
  rightReverseTicks=0;
}

// Clears one particular counter
void clearOneCounter(int which)
{
  clearCounters();
}
// Intialize Vincet's internal states

void initializeState()
{
  clearCounters();
}

void handleCommand(TPacket *command)
{
  switch(command->command)
  {
    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_FORWARD:
        sendOK();
        forward((float) command->params[0], (float) command->params[1]);
      break;
    case COMMAND_REVERSE:
      sendOK();
      reverse((float) command->params[0], (float) command->params[1]);
      break;

    case COMMAND_TURN_LEFT:
      sendOK();
      left((float) command->params[0], (float) command->params[1]);
      break;

    case COMMAND_TURN_RIGHT:
      sendOK();
      right((float) command->params[0], (float) command->params[1]);
      break;

    case COMMAND_STOP:
      sendOK();
      stop();
      break;

    case COMMAND_GET_STATS:
      sendStatus();
      break;

    case COMMAND_CLEAR_STATS:
      clearOneCounter(command->params[0]);
      sendOK();
      break;

    case COMMAND_COLOUR:
      colour();
      sendOK();
      break;

    /*
     * Implement code for other commands here.fo
     * 
     */
        
    default:
      sendBadCommand();
  }
}

void waitForHello()
{
  int exit=0;

  while(!exit)
  {
    TPacket hello;
    TResult result;
    
    do
    {
      result = readPacket(&hello);
    } while (result == PACKET_INCOMPLETE);

    if(result == PACKET_OK)
    {
      if(hello.packetType == PACKET_TYPE_HELLO)
      {
     

        sendOK();
        exit=1;
      }
      else
        sendBadResponse();
    }
    else
      if(result == PACKET_BAD)
      {
        sendBadPacket();
      }
      else
        if(result == PACKET_CHECKSUM_BAD)
          sendBadChecksum();
  } // !exit
}

void setup() {
  // put your setup code here, to run once:

  cli();
  WDT_off();
  setupEINT();
  setupPCICR();
  setupSerial();
  startSerial();
  setupMotors();
  startMotors();
  enablePullups();
  initializeState();
  sei();
  pinMode(8, OUTPUT);
  pinMode(9, INPUT);
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);

  // Setting frequency-scaling to 20%
  digitalWrite(S0,HIGH);
  digitalWrite(S1,LOW);

  // Power saving
  setupPowerSaving();
}

void WDT_off()
{
  MCUSR &= ~(1<<WDRF);
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  WDTCSR = 0x00;
}

void setupPowerSaving() 
{
  ADCSRA &= 0b01111111; // off adc
  PRR |= 0b10000001;
  
}

void handlePacket(TPacket *packet)
{
  switch(packet->packetType)
  {
    case PACKET_TYPE_COMMAND:
      handleCommand(packet);
      break;

    case PACKET_TYPE_RESPONSE:
      break;

    case PACKET_TYPE_ERROR:
      break;

    case PACKET_TYPE_MESSAGE:
      break;

    case PACKET_TYPE_HELLO:
      break;
  }
}

void loop() {

// Uncomment the code below for Step 2 of Activity 3 in Week 8 Studio 2

// Uncomment the code below for Week 9 Studio 2


 // put your main code here, to run repeatedly:
  TPacket recvPacket; // This holds commands from the Pi

  TResult result = readPacket(&recvPacket);
  
  if(result == PACKET_OK) {
    handlePacket(&recvPacket);
  } else {
    if(result == PACKET_BAD)
    {
      sendBadPacket();
    } else {
      if(result == PACKET_CHECKSUM_BAD)
      {
        sendBadChecksum();
      } 
    }
  }
  if (deltaDist > 0)
  {
    if (dir == FORWARD)
    {
      if (forwardDist > newDist)
      {
        deltaDist = 0;
        newDist = 0;
        uflag = 0;
        irflag = 0;
        stop();
        //uflag = 0;
        //irflag = 0;
      }
    }
    else if (dir == BACKWARD)
    {
      if (reverseDist > newDist)
      {
        deltaDist = 0;
        newDist = 0;
        stop();
        uflag = 0;
        irflag = 0;
      }
    }
    else if (dir == STOP)
    {
      deltaDist = 0;
      newDist = 0;
      stop();
      uflag = 0;
      irflag = 0;
    }
  }

    //check leftReverseTicksTurns/rightReverseTicksTurns has exceeded targetTicks, in which case we stop:

  if (deltaTicks > 0) { // compute angle > 0
    if (dir == LEFT) {
      if (leftReverseTicksTurns >= targetTicks) {
        deltaTicks = 0;
        targetTicks = 0;
        stop();
        uflag = 0;
        irflag = 0;
      }
  
    } else if (dir == RIGHT) {
      if (rightReverseTicksTurns >= targetTicks) {
        deltaTicks = 0;
        targetTicks = 0;
        stop();
        uflag = 0;
        irflag = 0;
      }
    } else if (dir == STOP) {
      deltaTicks = 0;
      targetTicks = 0;
      stop();
      uflag = 0;
      irflag = 0;
    }
  
  }
  
  //IR sensors ======================================================
  int LeftIR = analogRead(A2); 
  int RightIR = analogRead(A3);
 //Ultrasonic sensors ===============================================
 long duration = 0;
 float distance = 0;
 //for (int i = 0; i < 3; i ++) {
  digitalWrite(8, LOW);
  _delay_ms(0.002);
  digitalWrite(8, HIGH);
  _delay_ms(0.002);
  digitalWrite(8, LOW);
  duration = pulseIn(9, HIGH, 5000);
  distance += 330 * pow(10, -4) * (duration / 2);
 //}
  //distance /= 3;
  if (distance <= 1.5 && distance >= 1) {
   if (uflag== 0) {
    stop();
    sendMessage(ultramsg);
    uflag = 1;
   }
   }
      
}
