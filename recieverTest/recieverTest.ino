int xAxis = 8;
int yAxis = 7;
unsigned long xDuration;
unsigned long yDuration;

unsigned long yLow = 1100;
unsigned long yHigh = 1890;
unsigned long xLow = 1100;
unsigned long xHigh = 1860;

void setup()
{
  pinMode(xAxis, INPUT);
  pinMode(yAxis, INPUT);
  Serial.begin(9600);
}

void loop()
{
  xDuration = pulseIn(xAxis, HIGH, 1000000);
  yDuration = pulseIn(yAxis, HIGH, 1000000);
  //Serial.print(xDuration, DEC);
  //Serial.print(", ");
  //Serial.print(yDuration, DEC);
  //Serial.print(" => ");  
  
  byte xVal = normalise(xDuration, xLow, xHigh);
  byte yVal = normalise(yDuration, yLow, yHigh);

  Serial.print(xVal, DEC);
  Serial.print(", ");
  Serial.print(yVal, DEC);
  
  Serial.print(" => ");  
  
  //convert from a 2d mode into tank tracks.
  
  byte leftTank = max(min(((yVal - 127) + ((xVal - 127) * 10 / abs(yVal - 127))) + 127, 255), 0);
  byte rightTank = max(min(((yVal - 127) - ((xVal - 127) * 10 / abs(yVal - 127))) + 127, 255), 0);

//topright 255,255 => 255,127
//topleft  0,255   => 127,255
//top      127,255 => 255,255
//left     0,127   => 0  ,255

  Serial.print(leftTank, DEC);
  Serial.print(", ");
  Serial.print(rightTank, DEC);
  
  Serial.println("");
}

byte normalise(unsigned long val, unsigned long low, unsigned long high) {
  if (val <= low) return 0;
  if (val >= high) return 0xff;
  
  return map(val, low, high, 0, 255);
}
