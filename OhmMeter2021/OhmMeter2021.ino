#include "Arduino.h"
#include <Wire.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <Adafruit_ADS1015.h>  //Library required for ADS1115 ADC
#include <LiquidCrystal_I2C.h> //Library required for liquid crystal display
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); //Assigns liquid crystal display to arduino pins SDA & SCL(in this design A4 & A5 are used as these two pins respectively)

float analogPin = A0;
float analogPin1 = A1;
float analogPin2 = A2;
float roughSensorValue = 0;          //Values for Rx and ground voltages measured by arduino analog pins
float roughSensorValueGND = 0;
float roughSensorValueSupply = 0;
float roughVoltage;                  //Rx voltage, ground voltage and Rx resistance values determined based on Arduino analog pin(A0 & A1) readings
float roughVoltageGND;
float roughVoltageSupply;
float roughRx;
float multiplier = 0.0001875F;       //ADC reading multiplied by this value gives the output in volts (this value changes with the ACD gain level)
float adc0V, adc1V, adc2V, adc3V;    //Voltages for single ended readings from ADC
float voltage = 0.0;                 //Voltage for differential across Rx from ADC
float Rx = 0;                        //Resistance of resistor being measured
float displayedRx = 0;               //Reading for measured resistance displayed on LCD screen
int powerRx = 7;                     //The next power of 10 above the measurement for resistance Rx
float resolutionCountsRx = 4000;     //Number of counts in resolution for resistance reading displayed on LCD screen
int digitsDisplayedRx = 5;           //Number of digits required to display reading with a resolution of 4000 counts
float RxLimit = 10000000;            //Maximum value device is attempting to measure
float R0 = 99.9;//true value         //Resistances of branch resistors or Ri resistors
float R1 = 328.2;//true value
float R2 = 994.0;//true value
float R3 = 3289.0;//true value
float R4 = 9890.0;//true value
float R5 = 32840.0;//true value
float R6 = 99300.0;//true value
float R7 = 329300.0;//true value
float RArray[8] = {R0, R1, R2, R3, R4, R5, R6, R7};
float Ri = R7;                       //Resistance of resistor scale initially set to highest level
int resistorBranchIndex = 7;         //Index of relay branch currently shorted
float RDiff;                         //Difference between measured roughRx resistance and resistance of Ri branch resistor

//ohm sign
byte ohmSymbol[8] = {
  B00000,
  B01110,
  B10001,
  B10001,
  B10001,
  B01010,
  B11011,
  B00000
};

//Rough measurements for voltage across Rx and voltage at ground taken using Arduino analog pins A0 & A1
void readArduinoAnalog() {
  roughSensorValue = analogRead(analogPin);                             //Read the input between Rx(measured resistance) and Ri(branch resistance) using Arduino A0 pin
  roughSensorValueGND = analogRead(analogPin1);                         //Read the input at GND using Arduino A1 pin
  roughSensorValueSupply = analogRead(analogPin2);                      //Read the input at VDD using Arduino A2 pin
  roughVoltageGND = roughSensorValueGND * (5.0 / 1023.0);               //GND voltage calculated based on Arduino analog pins
  roughVoltage = (roughSensorValue * (5.0 / 1023.0)) - roughVoltageGND; //Voltage across Rx calculated based on Arduino analog pins
  roughVoltageSupply = roughSensorValueSupply * (5.0 / 1023.0);         //Supply or VDD voltage calculated based on Arduino analog pins
  roughRx = Ri / ((5.0 / roughVoltage) - 1);                            //Rx resistance calculated based on Arduino analog pin readings
}

//Hardware set to its initial state when meter is switched on
void setup()
{
  lcd.backlight();                //Liquid crystal display backlight turned on
  Serial.begin(9600);             //Setup serial
  lcd.begin(16, 2);               //Liquid crystal display initialized with the proper dimensions
  lcd.print("WELCOHM!");
  Serial.println("WELCOHM!");
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);           //R7 relay set to LOW(short) initially
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);          //Other relays set to HIGH(open) initially
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH);
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  // The ADC input range (or gain) can be changed via the following
  // functions, but be careful never to exceed VDD +0.3V max, or to
  // exceed the upper and lower limits if you adjust the input range!
  // Setting these values incorrectly may destroy your ADC!
  //                                                       ADS1015          ADS1115
  //                                                       -------          -------
  ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  //ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  //ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  //ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  //ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  //ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV

  lcd.createChar(1, ohmSymbol); //Initialize ohm symbol
  
  delay(1000);
  
  lcd.clear();
}

//Main loop starts
void loop()
{
  int16_t adc0, adc1, adc2, adc3;    //Single ended readings from ADC
  int16_t voltReading;               //Reading for differential across Rx from ADC
  
  readArduinoAnalog();                       //Rough voltages taken using Arduino analog pins

  if (roughRx < 0 or roughRx > RxLimit) {    //If the resistance value calculated is negative or above the limit, the meter shifts to the largest resistance branch
    resistorBranchIndex = 7;
  } else {
    RDiff = INFINITY;                        //Difference between rough value of Rx and value of Ri is set to infinity and next will be decreased from there
    resistorBranchIndex = 0;

    for (int i = 0; i < 8; i++) {            //Rough value of Rx is cycled through all resistor branches until the closest one is found
      if (abs(roughRx - RArray[i]) < RDiff) {
        RDiff = abs(roughRx - RArray[i]);
        resistorBranchIndex = i;
      }
    }
  }
    //resistorBranchIndex = 0;  //Override Ri branch selection if one is calibrating a particular branch
    //resistorBranchIndex = 1;
    //resistorBranchIndex = 2;
    //resistorBranchIndex = 3;
    //resistorBranchIndex = 4;
    //resistorBranchIndex = 5;
    //resistorBranchIndex = 6;
    //resistorBranchIndex = 7;
     
  for (int i = 0; i < 8; i++) {               //Relays not connected to the resistor branch closest in resistance to Rx are set to open circuit
    if (i != resistorBranchIndex) {
      digitalWrite(i + 2, HIGH);
    }
  }
  
  digitalWrite(resistorBranchIndex + 2, LOW); //Relay connected to resistor branch closest in resistance to Rx set to closed circuit
  Ri = RArray[resistorBranchIndex];           //Ri variable set to the value of resistor that is now in series with Rx

  delay(100);
  
  readArduinoAnalog();                        //Retake rough voltages now that the correct scale has been set

  voltReading = ads.readADC_Differential_0_1(); //Differential reading taken by ADC between ports 0 and 1
  voltage = voltReading * multiplier;           //ADC differential reading between ports 0 and 1 converted to volts

  //adc0 = ads.readADC_SingleEnded(0);          //Single ended ADC readings taken
  //adc1 = ads.readADC_SingleEnded(1);
  //adc2 = ads.readADC_SingleEnded(2);
  //adc3 = ads.readADC_SingleEnded(3);

  //adc0V = (float)adc0 * multiplier;           //Single ended ADC readings converted to volts
  //adc1V = (float)adc1 * multiplier;
  //adc2V = (float)adc2 * multiplier;
  //adc3V = (float)adc3 * multiplier;

  delay(100);
    
  adc2 = ads.readADC_SingleEnded(2); //ADC reading taken at port 2 connected to +5 to measure the supply
  adc2V = (float)adc2 * multiplier;  //Supply voltage determined based on ADC reading at port 2

  Rx = Ri / ((adc2V / voltage) - 1); //Voltage divider equation used to solve for Rx
  Serial.print("Resistance before adjustment: "); Serial.print(Rx, 10); Serial.println(" Ohm"); //Resistance reading prior to adjustment printed to serial
    
  if (resistorBranchIndex == 7){ //Rx measurements for each Ri scale adjusted by polynomial regression adjustment function generated through the calibration process
    Rx = Rx + (5.3223750858051280 * pow(10,3) + -2.4281582980199931 * pow(10,-2) * Rx + 1.3858975736226595 * pow(10,-7) * pow(Rx,2) + -4.2059331915537060 * pow(10,-15) * pow(Rx,3) + 4.1271826356625583 * pow(10,-21) * pow(Rx,4));
    } else if (resistorBranchIndex == 6){
      Rx = Rx + (2.1238142565083130 * pow(10,4) + -8.2090219791235153 * pow(10,-1) * Rx + 1.1372997600431447 * pow(10,-5) * pow(Rx,2) + -6.4352245140838588 * pow(10,-11) * pow(Rx,3) + 1.3027891620169220 * pow(10,-16) * pow(Rx,4));
    } else if (resistorBranchIndex == 5){
      Rx = Rx + (-2.6682837895516827 * pow(10,3) + 2.9802012919653392 * pow(10,-1) * Rx + -1.1318873407960057 * pow(10,-5) * pow(Rx,2) + 1.8436711714578837 * pow(10,-10) * pow(Rx,3) + -1.0526126447491875 * pow(10,-15) * pow(Rx,4));
    } else if (resistorBranchIndex == 4){
      Rx = Rx + (-1.2990448466433213 * pow(10,2) + 4.8829867274151485 * pow(10,-2) * Rx + -6.2831774966867742 * pow(10,-6) * pow(Rx,2) + 3.6590517360171962 * pow(10,-10) * pow(Rx,3) + -7.2182993976645724 * pow(10,-15) * pow(Rx,4));
    } else if (resistorBranchIndex == 3){
      Rx = Rx + (-3.0645335973349188 * pow(10,2) + 3.3322808339874338 * pow(10,-1) * Rx + -1.2823237430990934 * pow(10,-4) * pow(Rx,2) + 2.0934500325115132 * pow(10,-8) * pow(Rx,3) + -1.2262451315039650 * pow(10,-12) * pow(Rx,4));
    } else if (resistorBranchIndex == 2){
      Rx = Rx + (3.8050144267716369 * pow(10,1) + -1.2341715053441385 * pow(10,-1) * Rx + 1.3788516929628944 * pow(10,-4) * pow(Rx,2) + -6.0023327366530932 * pow(10,-8) * pow(Rx,3) + 8.9429964326831901 * pow(10,-12) * pow(Rx,4));
    } else if (resistorBranchIndex == 1){
      Rx = Rx + (-3.0171211611623892 * pow(10,1) + 3.1041927275360964 * pow(10,-1) * Rx + -1.1679559388185310 * pow(10,-3) * pow(Rx,2) + 1.8440343171380762 * pow(10,-6) * pow(Rx,3) + -1.0616325389725969 * pow(10,-9) * pow(Rx,4));
    } else if (resistorBranchIndex == 0){
      Rx = Rx + (-1.0087015799543466 * pow(10,0) + 4.0743320497918902 * pow(10,-3) * Rx + -8.1469322428050719 * pow(10,-5) * pow(Rx,2) + 6.9157686317257333 * pow(10,-7) * pow(Rx,3) + -1.6912410018321967 * pow(10,-9) * pow(Rx,4));
    }

  Serial.print("Resistance: "); Serial.print(Rx, 10); Serial.println(" Ohm"); //Resistance reading after adjustment printed to serial
  
  powerRx = ceil(log10(Rx));                                                                                                         //The next power of 10 above the measurement for Rx
  displayedRx = (pow(10, powerRx) / resolutionCountsRx) * round(Rx / (pow(10, powerRx) / resolutionCountsRx));                       //Measured resistance is rounded to a resolution of 4000 counts to be displayed on LCD screen

  lcd.clear(); //Clears screen so new value can be displayed
  
  if (Rx >= 1000000) {
    float displayedRxM = displayedRx / 1000000.0;
        if (Rx >= (RxLimit * 1.1)) {                                                                                                 //Some leeway is given for readings marginally too high and they are still displayed to the user
          lcd.print("OVER LIMIT");                                                                                                   //When the measured resistance is well above the limit of 10 Mohm the user is notified
          } else {
            lcd.print(displayedRxM, abs(powerRx - digitsDisplayedRx - 6)); lcd.print(" M"); lcd.write(1);                            //Resistance reading displayed on LCD in Mohm Range
  }
    Serial.print("Resistance on LCD: "); Serial.print(displayedRxM, abs(powerRx - digitsDisplayedRx - 6)); Serial.println(" Mohm");
  } else if (Rx >= 1000) {
    float displayedRxK = displayedRx / 1000.0;
    lcd.print(displayedRxK, abs(powerRx-digitsDisplayedRx - 3)); lcd.print(" k"); lcd.write(1);                                      //Resistance reading displayed on LCD in Kohm Range
    Serial.print("Resistance on LCD: "); Serial.print(displayedRxK, abs(powerRx-digitsDisplayedRx - 3)); Serial.println(" kohm");
  } else {
    lcd.print(displayedRx, abs(powerRx-digitsDisplayedRx)); lcd.write(1);                                                            //Resistance reading displayed on LCD in Ohm Range
    Serial.print("Resistance on LCD: "); Serial.print(displayedRx, abs(powerRx-digitsDisplayedRx)); Serial.println(" ohm");
  }

  Serial.print("Rough Resistance: "); Serial.print(roughRx, 5); Serial.println(" Ohm"); //Outputs and variables printed to serial
  Serial.print("Voltage: "); Serial.print(voltage, 5); Serial.println(" Volts");
  Serial.print("Rough Voltage: "); Serial.print(roughVoltage, 5); Serial.println(" Volts");
  Serial.print("Supply Voltage: "); Serial.print(adc2V, 5); Serial.println(" Volts");
  Serial.print("Rough Supply Voltage: "); Serial.print(roughVoltageSupply, 5); Serial.println(" Volts");
  Serial.print("Rough Ground Voltage: "); Serial.print(roughVoltageGND, 5); Serial.println(" Volts");
  Serial.print("Ri Resistor Branch Index: "); Serial.println(resistorBranchIndex);
  Serial.print("Ri Branch Resistance: "); Serial.print(Ri);  Serial.println(" Ohm");
  Serial.println();
  
  delay(100);
}
