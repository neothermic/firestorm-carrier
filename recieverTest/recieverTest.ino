#include <Servo.h>
char serialBuffer[64];
Servo left;
Servo right;
//pins for the PWM(?) input from the radio reciever.
int xAxis = 8;
int yAxis = 7;
int throttle = 12;
int switchPin = 4;

int leftPinOut = 11;
int rightPinOut = 10;
int brakePin = 3; //One pin controls both electronic brakes

int powerPin = 9;
int selfPowerPin = 6;

int inverterPin = 5;
int HvPin = A0;
//baseline timings for axis inputs. These are the values sent when the stick is at the extreme left and right (or bottom and top). If in doubt make these slightly further apart than they need to be.
unsigned long xLow = 1220;
unsigned long xHigh = 1790;
unsigned long yLow = 1100;
unsigned long yHigh = 1680;

//Threshold for positive output. This is the number of positions either side of center to be ignored and the vehicle will stay in neutral (90,90). Make this bigger to create a larger deadzone if the vehicle twitches when sticks are centered. 50 Seems a good place to start. Note that the larger this is, the more of the extremes of each stick will be lost.

unsigned long deadzone = 50;


/*
 * How much does steering affect the tank tracks, particularly at speed.
 * The "normal" forward/backward motion is adjusted by ((x*c1)/(y+c2))
 * 
 * Analysing the WA contour plot (http://wolfr.am/1jzwx7m):
 * - The angle of the line for y=0 shows us how "twitchy" the steering is at low speed.
 * -- This should reach the lowest/highest values at the extremes (but not before) to allow the fastest steering.
 * - The value of x=-127, y=127 (top left) dictates how fast the steering is at high speed.
 * -- If this is less than the value at x=0, y=0 then we will turn on the spot even if we are at maximum forward input (this is undesireable and hard to achieve!)
 * -- The higher (lighter colour) this corner has, the least effective the steering is at speed.
 * 
 */
byte steeringCoefficient1 = 70;
byte steeringCoefficient2 = 50;

//A scaling of the output values. 255 allows full power. 0 would be disable output. 64 would be a good training mode where all outputs are scaled by a quarter.
byte scaling = 255;

unsigned long xDuration;
unsigned long yDuration;
unsigned long throttleDuration = 1000;
unsigned long switchDuration = 1000;

unsigned long inverterState = 0;

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
  pinMode(inverterPin, OUTPUT);
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
  byte leftTank = pegToByte(((yVal - 127) + ((xVal - 127) * steeringCoefficient1 / (abs(yVal - 127) + steeringCoefficient2))) + 127);
  byte rightTank = pegToByte(((yVal - 127) - ((xVal - 127) * steeringCoefficient1 / (abs(yVal - 127) + steeringCoefficient2))) + 127);

  //servo library needs a value between 0 and 180
  int leftServo = map(leftTank, 0, 255, 0, 180);
  int rightServo = map(rightTank, 0, 255, 0, 180);

if (throttleDuration < 1200) {
  leftServo = 90;
  rightServo = 90;
  failsafeLocked = 1;
} 
if (switchDuration > 1200){
 // digitalWrite(inverterPin, LOW);
} else {
 // digitalWrite(inverterPin, HIGH);
}

  
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

void inverterToggle() {
  if (inverterState == 0){
    digitalWrite(inverterPin, LOW);
    inverterState = 1;
    Serial.print("D Inverter ON");
    Serial.println("");
  } else if (inverterState == 1){
    digitalWrite(inverterPin, HIGH);
    inverterState = 0;
    Serial.print("D Inverter OFF");
    Serial.println("");
  }
}

void allStop() {
  digitalWrite(powerPin, HIGH);
  digitalWrite(selfPowerPin, HIGH);
  Serial.print("D ALL STOP");
  Serial.println("");
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
        case 'i':
          inverterToggle();
          break;
        case 'S':
          allStop();
          break;
      }
    }
  }
}
