#include <Servo.h>
char serialBuffer[64];
Servo left;
Servo right;
byte printDebug = 1;


//pins for the PWM(?) input from the radio reciever.
int xAxis = 8;
int yAxis = 7;
int throttle = 12;
int switchPin = 4;

int leftPinOut = 10;
int rightPinOut = 11;
int brakePin = 3; //One pin controls both electronic brakes

int powerPin = 9;
int selfPowerPin = 6;

int utility1Pin = 5;
int utility2Pin = A5;
int utility3Pin = A4;

int HvPin = A0;
//baseline timings for axis inputs. These are the values sent when the stick is at the extreme left and right (or bottom and top). If in doubt make these slightly further apart than they need to be.
unsigned long xLow = 1220;
unsigned long xHigh = 1790;
unsigned long yLow = 1100;
unsigned long yHigh = 1680;

//Threshold for positive output. This is the number of positions either side of center to be ignored and the vehicle will stay in neutral (90,90). Make this bigger to create a larger deadzone if the vehicle twitches when sticks are centered. 50 Seems a good place to start. Note that the larger this is, the more of the extremes of each stick will be lost.

unsigned long deadzone = 50;


/*
 * There are 3 different possible methods for turning an X/Y youstick movement into the tank tracks input needed by the motors.
 * 
 * Each method has 1 or 2 coefficients which control how much does steering affects the tank tracks.
 *
 * Method 1
 * ========
 *
 * The "normal" forward/backward motion is adjusted by ((x*c1)/(y+c2))
 * 
 * Analysing the WA contour plot (http://wolfr.am/1jzwx7m):
 * - The angle of the line for y=0 shows us how "twitchy" the steering is at low speed.
 * -- This should reach the lowest/highest values at the extremes (but not before) to allow the fastest steering.
 * - The value of x=-127, y=127 (top left) dictates how fast the steering is at high speed.
 * -- If this is less than the value at x=0, y=0 then we will turn on the spot even if we are at maximum forward input (this is undesireable and hard to achieve!)
 * -- The higher (lighter colour) this corner has, the least effective the steering is at speed.
 * 
 * Method 2 - should yield less jerky steering but might be a bit slow to start.
 * ========
 *
 * http://po.st/s1MttS 
 *
 * Only 1 coefficient which is inverted. 1 will mean steering is harsh, 10 means steering will barely have an effect.
 *
 * Method 3 - is simpler but might not be as good?
 * ========
 *
 * http://po.st/hmBo4G 
 *
 * 2 coefficients which are both inverted. 
 *  - 1 changes how effective the throttle is (1 means full power at full throttle, no steering. 1.5 means you only reach full power if you are also steering).
 *  - 2 changes how effective the steering is (1 means twitchy steering and 10 means ineffective steering)
 */
byte steeringMethod = 2;

byte steeringCoefficient1_1 = 70;
byte steeringCoefficient1_2 = 50;

byte steeringCoefficient2_1 = 2;

byte steeringCoefficient3_1 = 1;
byte steeringCoefficient3_2 = 2;

//A scaling of the output values. 255 allows full power. 0 would be disable output. 64 would be a good training mode where all outputs are scaled by a quarter.
byte scaling = 255;

unsigned long xDuration;
unsigned long yDuration;
unsigned long throttleDuration = 1000;
unsigned long switchDuration = 1000;

unsigned long switch1State = 0;

unsigned long utility1State = 0;
unsigned long utility2State = 0;
unsigned long utility3State = 0;

void setup()
{
  
  pinMode(xAxis, INPUT);
  pinMode(yAxis, INPUT);
  
  left.attach(leftPinOut, 1000, 2000);
  right.attach(rightPinOut, 1000, 2000);

  
  pinMode(throttle, INPUT);
  pinMode(switchPin, INPUT);

  pinMode(brakePin, OUTPUT);
  brakeOff();
  
  pinMode(powerPin, OUTPUT);
  pinMode(selfPowerPin, OUTPUT);
  pinMode(utility1Pin, OUTPUT);
  pinMode(utility2Pin, OUTPUT);
  pinMode(utility3Pin, OUTPUT);
  digitalWrite(utility1Pin, HIGH);
  digitalWrite(utility2Pin, HIGH);
  digitalWrite(utility3Pin, HIGH);
  digitalWrite(selfPowerPin, LOW);
  digitalWrite(powerPin, HIGH);
  delay(1000);
  digitalWrite(powerPin, LOW);
  Serial.begin(9600);
}

void loop()
{
  //Reset locks for next cycle - These values only affect debug output
  int failsafeLocked = 0;
  int xLocked = 0;  
  int yLocked = 0;

  xDuration = pulseIn(xAxis, HIGH, 1000000);
  yDuration = pulseIn(yAxis, HIGH, 1000000);
  throttleDuration = pulseIn(throttle, HIGH, 1000000);

  //check to see if the values fall within the deadzone and adjust the values backwards to keep fine control intact
  unsigned long xCentre = (xLow + xHigh)/2;
  if (xDuration <= (xCentre + deadzone) && xDuration >= (xCentre - deadzone)){
    xLocked = 1;
    xDuration = xCentre;
  } else if (xDuration >= xCentre) {
    xDuration = (xDuration - deadzone);
  } else if (xDuration < xCentre) {
    xDuration = (xDuration + deadzone);
  }
  unsigned long yCentre = (yLow + yHigh)/2;
  if (yDuration <= (yCentre + deadzone) && yDuration >= (yCentre - deadzone)){
    yLocked = 1;
    yDuration = yCentre;
  } else if (yDuration >= yCentre) {
    yDuration = (yDuration - deadzone);
  } else if (yDuration < yCentre) {
    yDuration = (yDuration + deadzone);
  }

  switchDuration = pulseIn(switchPin, HIGH, 1000000);

  byte xVal = normalise(xDuration, xLow, xHigh);
  byte yVal = normalise(yDuration, yLow, yHigh);

  //convert from a 2d mode into tank tracks.
  byte leftTank;
  byte rightTank;
  if (steeringMethod == 1) {
    leftTank = pegToByte(((yVal - 127) + ((xVal - 127) * steeringCoefficient1_1 / (abs(yVal - 127) + steeringCoefficient1_2))) + 127);
    rightTank = pegToByte(((yVal - 127) - ((xVal - 127) * steeringCoefficient1_1 / (abs(yVal - 127) + steeringCoefficient1_2))) + 127);
  } else if (steeringMethod == 2) {
    leftTank = pegToByte((((yVal - 127) * abs(yVal - 127) / 127) + ((xVal - 127) / steeringCoefficient2_1)) + 127);
    rightTank = pegToByte((((yVal - 127) * abs(yVal - 127) / 127) - ((xVal - 127) / steeringCoefficient2_1)) + 127);
  } else {
    leftTank = pegToByte((((yVal - 127) / steeringCoefficient3_1) + ((xVal - 127) / steeringCoefficient3_1 )) + 127);
    rightTank = pegToByte((((yVal - 127) / steeringCoefficient3_1) - ((xVal - 127) / steeringCoefficient3_1 )) + 127);
  }
  
  leftTank = ((leftTank - 127) * scaling / 255) + 127;
  rightTank = ((rightTank - 127) * scaling / 255) + 127;

  //servo library needs a value between 0 and 180
  int leftServo = map(leftTank, 0, 255, 0, 180);
  int rightServo = map(rightTank, 0, 255, 0, 180);

  if (throttleDuration < 1200) {
    leftServo = 90;
    rightServo = 90;
    failsafeLocked = 1;
  }

  if (switchDuration > 1200){
    //switch "on" condition
    switch1State = 1;
  } else {
    //switch "off" condition
    switch1State = 0;
  }

  if (printDebug) {
    Serial.print("D ");
    Serial.print(xDuration, DEC);
    Serial.print(", ");
    Serial.print(yDuration, DEC);
  
    Serial.print(" => ");  

    Serial.print(xVal, DEC);
    Serial.print(", ");
    Serial.print(yVal, DEC);
  
    Serial.print(" => ");  

    Serial.print(leftServo, DEC);
    Serial.print(", ");
    Serial.print(rightServo, DEC);
    if (xLocked == 1){
      Serial.print(", ");
      Serial.print("X Deadzone");
    }
    if (yLocked == 1){
      Serial.print(", ");
      Serial.print("Y Deadzone");
    }
    if (failsafeLocked == 1){
      Serial.print(", ");
      Serial.print("Failsafe");
    }
  
    Serial.println("");
  }  
  
  left.write(leftServo);
  right.write(rightServo);
  
  checkSerial();
}

byte normalise(unsigned long val, unsigned long low, unsigned long high) {
  if (val <= low) return 0;
  if (val >= high) return 0xff;
  
  return map(val, low, high, 0, 255);
}

// take a signed int value which should be //about// 0-255, peg it to exactly 0-255 and convert to a byte.
byte pegToByte(int input) {
  return max(min(input, 255), 0);
}

//routines to avoid confusion with the brakes and the relay being backwards
void brakeOn() {
  digitalWrite(brakePin, HIGH);
}

void brakeOff() {
  digitalWrite(brakePin, LOW);
}

void batteryCheck() {
  int senseVoltage = analogRead(HvPin);
  float voltage = senseVoltage * (31.786 / 1023.0);
  Serial.print("B");
  Serial.print(voltage);
  Serial.println("");
}

//handlers for utilities and switches, accepts 3 commands: 1 turns the utility on. 0 turns it off. ? requests serial output of the utility's current state
void switch1(char command) {
  if (command == '0') {
//state of the switch cannot be changed by software
  } else if (command == '1') {
//state of the switch cannot be changed by software
  } else if (command == '?') {
    Serial.print("C 1 ");
    Serial.print(switch1State); //state of the switch can however be reported
    Serial.println("");
  }
}
void utility1(char command) {
  if (command == '0') {
    digitalWrite(utility1Pin, HIGH);
    utility1State = 0;
  } else if (command == '1') {
    digitalWrite(utility1Pin, LOW);
    utility1State = 1;
  } else if (command == '?') {
    Serial.print("U 1 ");
    Serial.print(utility1State);
    Serial.println("");
  }
}
void utility2(char command) {
  if (command == '0') {
    digitalWrite(utility2Pin, HIGH);
    utility2State = 0;
  } else if (command == '1') {
    digitalWrite(utility2Pin, LOW);
    utility2State = 1;
  } else if (command == '?') {
    Serial.print("U 2 ");
    Serial.print(utility2State);
    Serial.println("");
  }
}
void utility3(char command) {
  if (command == '0') {
    digitalWrite(utility3Pin, HIGH);
    utility3State = 0;
  } else if (command == '1') {
    digitalWrite(utility3Pin, LOW);
    utility3State = 1;
  } else if (command == '?') {
    Serial.print("U 3 ");
    Serial.print(utility3State);
    Serial.println("");
  }
}

void allStop() { 
  digitalWrite(powerPin, HIGH);
  digitalWrite(selfPowerPin, HIGH);
  if (printDebug) {
    Serial.print("D ALL STOP");
  }  
  Serial.println("");
}

void driveStop() { //stops the motor output without killing the machine dead. Engages brake.
  scaling = 0;
  brakeOn();
  if (printDebug) {
    Serial.print("D DRIVE STOP");
  }
  Serial.println("");
}

void driveScale(char newScale) { //sets a new scaling value, adjusting the effective speed of the machine
  int scalePerc;
  if (newScale == 'F') {
    scalePerc = 10;
    scaling = map(scalePerc, 0, 10, 0, 255);
    if (scaling > 0) {
      brakeOff();
    }
  } else if (newScale == '?') {
    Serial.print("v s ");
    //TODO: Would rounding mean we read back a different value to what we set?
    int outputValue = map(scaling, 0, 255, 0, 10);
    if (outputValue == 10) {
      Serial.print('F');
    } else {
      Serial.print(outputValue);
    }
    Serial.println("");
  } else {
    scalePerc = newScale-'0'; // char -> int
    scaling = map(scalePerc, 0, 10, 0, 255);
    if (scaling > 0) {
      brakeOff();
    }
  }
}

//check for serial input and act accordingly
void checkSerial() {
  if (Serial.available() > 0) {
    byte bytesRead = Serial.readBytesUntil('\n', serialBuffer, 64);

    if (bytesRead == 0) {
      //too little to be interesting
    }
    else if (bytesRead == 1) {
      //single character, probably a debugging command
      switch (serialBuffer[0]) {
        case 'b':
          batteryCheck();
          break;
        case 'A':
          allStop();
          break;
        case 'S':
          driveStop();
          break;
        case 'D':
          printDebug = 1;
          break;
        case 'd':
          printDebug = 0;
          break;
      }
    }
    else if (bytesRead == 5) {
      //a utility command or other instruction
      switch (serialBuffer[0]) {
        case 'u':
          switch (serialBuffer[2]) {
            case '1':
              utility1(serialBuffer[4]);
              break;
            case '2':
              utility2(serialBuffer[4]);
              break;
            case '3':
              utility3(serialBuffer[4]);
              break;
          }
          break;
        case 'v':
          switch (serialBuffer[2]) {
            case 's':
              driveScale(serialBuffer[4]);
              break;
          }
        case 'c':
          switch (serialBuffer[2]) {
            case '1':
              switch1(serialBuffer[4]);
              break; 
          }
          break;
      }
    }
  }
}
