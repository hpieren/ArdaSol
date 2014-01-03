/* 
---------------------------------------
ArdaSol Solar Data Management
---------------------------------------
*/

#define Version "V2.0 "
#define Release "R3"

/*
Version: 2.0
Creation Date: 12.11.2013
Author: Heinz Pieren
Email: heinz.pieren@semprevacanze.it
---------------------------------------
ArdaSolPwrMon Remote Power Monitor
---------------------------------------

- Power measurement with current transformer and voltage transformer
- Display values on 4 digit 7 segment display and Bargraph LED
- Send data by XBEE to ArdaSol Data Management Center
- Receive Solar Power and show on Bragraph LED


*/

#include <EmonLib.h>                   // Include Emon Library
EnergyMonitor emon1;                   // Create an instance

#include <SoftwareSerial.h>
SoftwareSerial DisplaySerial(8, 7); // RX, TX
SoftwareSerial XBeeSerial(10, 11);    // RX TX
#define rtsXBeePin 6               // RTS Signaling LED

//Pin connected to ST_CP of 74HC595
const byte latchPin=2;
//Pin connected to SH_CP of 74HC595
const byte clockPin=3;
////Pin connected to DS of 74HC595
const byte dataPin=4;

#define ledRed      6
#define ledGreen    5

#define intensityRed 16

boolean ledRedState;

#define logMessageStart            "*01:start"

#define logMessageFlushXBeeRecBuf    "*E1:FlushXBeeRB x="
#define logMessageXBeeRecChkSumErr   "*E2:XBeeChkSumErr"
#define logMessageXBeeRecTimeout     "*E3:XBeeRecTimeout"

// 7 segment display commands

#define ClrDisp 0x76

#define DecimalControl 0x77
#define DecPointTwoOn  0x04
#define DecPointTwoOff 0x00


#define zeroPowerCorrection 40   //for having zero on display when no power is consumed

unsigned long            dataDisplayStart;
const unsigned long      dataDisplayWait = 5000;  //Data Display update rate in ms

unsigned long            barGraphDisplayStart;
const unsigned long      barGraphDisplayWait = 1000;  //Bargraph Display update rate in ms

unsigned long            gridMeasureStartWait;
const unsigned long      gridMeasureWait = 5000;  // ms Grid measure Wait when serial comm is going on

unsigned long            energyMeasureLastStart;
long                     dxTime_ms;
const unsigned long      energyMeasureRate = 1000;  	//Energy measurement rate in ms

unsigned long            energyMeasureStoreWait;	//timer for store energy day in EEPROM
const unsigned long      energyMeasureStoreRate = 15*60000;  //every 15 minutes store energy in non volatile memory
															// when:
const int 				 deltaStoreValue = 10;   			// at least 1kWh (=10 kWh-Zehntel) must be cumulated for storing in EEPROM
int						 writePersistentCounter = 0;		// counts the write cycles

byte                     displayState;

//-------------------------------------------
// Power measurement stuff defs
//-------------------------------------------

float realPower       = 0.0;        //calculated by emon Real Power
float apparentPower   = 0.0;        //calculated by emon Apparent Power
float powerFActor     = 0.0;        //calculated by emon Power Factor
float supplyVoltage   = 0.0;        //measured Vrms
float Irms            = 0.0;        //measured Irms

int usedPowerWatts=0;    // used Power in Watts
int instGridVolts=0;     // instant grid voltage

int usedPowerWattsAvr=0;  // average values
int instGridVoltsAvr=0;

int instGridVoltageFromPVI=0;
int instSolarPowerFromPVI=0;

int instSolarPowerLeft=0;
int usedPowerFromGrid=0;

int lastUsedPowerWattsAvr=0;

long dailyEnergyConsumption_Ws=0;	
long dailyEnergyConsumption_Wh=0;
int dailyEnergyConsumption_kWhTenth=0;		

#define oRPVsize 10
int oldRealPowerValues[oRPVsize];
byte oRPVpointer;

#define oIGVsize 10
int oldIGVValues[oIGVsize];
byte oIGVpointer;

#define unitBGrPWR 250    //one led representing 250W
byte gridPWR=0;           //Normalized power value for bargraph
byte solarPWR=0;          //Normalized solar power value for bargraph


//-------------------------------------------
// Communication stuff defs
//-------------------------------------------
/*
Command Packet structure is a 10 Byte command
1	address
2	operation type
3	data1
4	data2
5	data3
6	data4
7	data5
8	data6
9	crc low
10	crc high

Example: Get total grid voltage from emon and with grid voltage from PVI
03 3B 01 00 00 00 00 F0 CRCL CRCH
                        --------- CRC
		  ----- PVI Voltage = 240V (16 Bit integer)
   ----- GetVoltage command 3B 01
-- emon address = 3

PVI response 8 byte data packet:
1 State	
2 MState
3 Param1
4 Param2
5 Param3
6 Param4
7 crc low
8 crc high

Example: Response to above request
00 06 00 00 00 EE CRCL CRCH
		  --------- CRC
             ---- Voltage in Volts (16Bit integer) 238V
   -- MState = 6 ? same as Aurora PVI
-- State = 0 ?  same as Aurora PVI

*/

#define emonadr            3                    // adress of energy monitor

#define cmdGridVoltage    0x3B01		//grid voltage
#define cmdGridPower      0x3B03
#define cmdEnergyDay      0x4E00

#define cmdResetEnergyValue 0x0052


#define cmdSize 10    // 10 Bytes command Packet
static byte CommandBuf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // 10 elements

#define rspSize 8    // 8 Bytes response Packet
static byte ResponseBuf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // 8 elements

unsigned long            receiveTimeoutStart;
const unsigned int       receiveMaxXBeeWait = 1000;  //one second max getting a command

unsigned long            sequenceCounter;

unsigned int             crc16Checksum;


//-----------------------------------------------------------------------
// EEPROM Stuff

#include <EEPROM.h>

//-----------------------------------------------------------------------
// Save Energy Day Values
//-----------------------------------------------------------------------
// Writes the total amount of energy to persistent memory

void writePersistentEnergyDay(int totEWr)

{

EEPROM.write(0, lowByte(totEWr));		// location 0
EEPROM.write(1, highByte(totEWr));		// location 1
++writePersistentCounter;

}

//-----------------------------------------------------------------------
// reads the total amount of energy from persistent memory

int readPersistentEnergyDay()

{

int x;
int y;

y=EEPROM.read(0);		// location 0
x=EEPROM.read(1);		// location 1
y+= x << 8;

return(y);

}

//-----------------------------------------------------------------------
// Communication stuff
//-----------------------------------------------------------------------

/* CRC16 checksum calculation

    * Copyright (C) 2006-2012 Curtis J. Blank curt@curtronics.com
                                         16   12   5
    this is the CCITT CRC 16 polynomial X  + X  + X  + 1.
    This is 0x1021 when x is 2, but the way the algorithm works
    we use 0x8408 (the reverse of the bit pattern).  The high
    bit is always assumed to be set, thus we only use 16 bits to
    represent the 17 bit value. */

#define POLY 0x8408   /* 1021H bit reversed */

unsigned int uiCrc16Cal (byte * buf, byte length)
{
      byte i;
      unsigned int data;
      unsigned int crc = 0xffff;

      if (length == 0)
        return (~crc);
      do
      {
        for (i=0, data=(unsigned int)0xff & *buf++;
         i < 8; 
         i++, data >>= 1)
        {
          if ((crc & 0x0001) ^ (data & 0x0001))
            crc = (crc >> 1) ^ POLY;
          else  crc >>= 1;
        }
      } while (--length);
      crc = ~crc;

      return (crc);
}

//-----------------------------------------------------------------------
void clearXBeeSerialInput()
{
byte x;
byte y;

x=0;

while (XBeeSerial.available() > 0) 
    {
      y = XBeeSerial.read();
      x++;
    }
    
if (x>0)
    {
      Serial.print(sequenceCounter);
      Serial.print(':');
      Serial.print(logMessageFlushXBeeRecBuf);  // buffer has been flushed
      Serial.println(x);
    }
}

//-----------------------------------------------------------------------

// waits for a valid Command from XBee (ArdaSol)
boolean CommandPacketReceived()
{
  boolean cmdOk;
  boolean cmdTimeout;
  unsigned int cmdCRC;
  byte i;
  
  cmdOk = false;
  
  
  while (!cmdOk)    // we wait as long as we have a valid command from XBee
  {
    i=0;
    cmdTimeout = false;
  
    do            // the receive loop with timeout function
    {
      if (XBeeSerial.available()) 
      {
          CommandBuf[i] = XBeeSerial.read();
          ++i;
          if (i == 1) receiveTimeoutStart = millis();  //set timeout start value when first byte has encountered
      }
      
      if (( i > 0) && ((millis() - receiveTimeoutStart) > receiveMaxXBeeWait)) cmdTimeout = true;
    } while ((i < cmdSize) && !cmdTimeout);
    
    // Serial.println(i);
    
    if ((!cmdTimeout) && (i == cmdSize)) 
    {
      cmdCRC = CommandBuf[8] + (CommandBuf[9] << 8);
      if (cmdCRC == uiCrc16Cal (CommandBuf, 8)) cmdOk = true ;
      else 
          {
            Serial.print(sequenceCounter);
            Serial.print(':');
            Serial.println(logMessageXBeeRecChkSumErr);
          }
    }
    else 
        {
          Serial.print(sequenceCounter);
          Serial.print(':');
          Serial.println(logMessageXBeeRecTimeout); 
        }
        
  }  // end of while !cmdOk
  
  if (cmdOk && (CommandBuf[0] != emonadr)) cmdOk = false;  // this command is not for emon
  

  /*
  // test
  for (i=0; i<cmdSize; i++){
     Serial.print(CommandBuf[i],HEX );
     Serial.print(':');
	 }
	 Serial.println();
	*/  //test
  
return (cmdOk);
}

//-----------------------------------------------------------------------
// sends a valid PVI Response to XBee (ArdaSol)
void SendResponsePacketToXBee()
{

byte i;

  ledRedState=!ledRedState;
  digitalWrite(ledRed, ledRedState);  //signaling transmission with red LED
 

  for (i = 0; i < rspSize ; ++i )
  {
       
    XBeeSerial.write(ResponseBuf[i] );
    
    
	/* // test
	Serial.print(ResponseBuf[i],HEX );
    Serial.print(':');
     //test	
     */
  }
   // Serial.println();
   
   ledRedState=!ledRedState;
   if (ledRedState) analogWrite(ledRed, intensityRed);
   else digitalWrite(ledRed, ledRedState);
   
   
 
   /*
   Serial.println();
   Serial.print("V=");
   Serial.println(instGridVoltsAvr,HEX);
   Serial.print("W=");
   Serial.println(usedPowerWattsAvr,HEX);
   */

}

//-----------------------------------------------------------------------
// sends a emon Response to XBee (ArdaSol)
void SendResponseFromEmon()

{

  int command;
  int value;
  int wrd;

  command = CommandBuf[1];
  command = command << 8;
  command = command | CommandBuf[2];
  
  value = CommandBuf[6];
  value = value << 8;
  value = value | CommandBuf[7];
  
  if (command == cmdGridVoltage)
    {
      instGridVoltageFromPVI=value;
      
      ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(writePersistentCounter);		// for test purpose
      ResponseBuf[3]=lowByte(writePersistentCounter);
      ResponseBuf[4]=highByte(instGridVoltsAvr);;
      ResponseBuf[5]=lowByte(instGridVoltsAvr);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();      
    }
  
  else if (command == cmdGridPower)
    {
      instSolarPowerFromPVI=value;
      
      ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=0;
      ResponseBuf[3]=0;
      ResponseBuf[4]=highByte(usedPowerWattsAvr);;
      ResponseBuf[5]=lowByte(usedPowerWattsAvr);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
    
    }
	
	 else if (command == cmdEnergyDay)
    {
      
	  if (value == cmdResetEnergyValue) 
		{
			dailyEnergyConsumption_Ws=0;
			dailyEnergyConsumption_Wh=0;
			dailyEnergyConsumption_kWhTenth=0;
			writePersistentEnergyDay(dailyEnergyConsumption_kWhTenth);
		}
			
	  wrd = dailyEnergyConsumption_Ws >> 16;  // high word of daily energy
	  
      ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(wrd);
      ResponseBuf[3]=lowByte(wrd);
	  
	  wrd = dailyEnergyConsumption_Ws;
	  
      ResponseBuf[4]=highByte(wrd);
      ResponseBuf[5]=lowByte(wrd);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
    
      gridMeasureStartWait= millis()- gridMeasureWait;  // force imediate measure action
  
  
    }

  
}  
//-------------------------------------------
// Measurement of Grid Power
//-------------------------------------------

void measureGridPower()

{
  
  int i;
  long tmp;
  
  //digitalWrite(ledGreen, HIGH);  //Turns LED green On for measurement
 
  emon1.calcVI(10,1000);         // Calculate all. No.of half wavelengths (crossings), time-out
  // emon1.serialprint();           // Print out all variables (realpower, apparent power, Vrms, Irms, power factor)
  
  
  realPower       = emon1.realPower;        //extract Real Power into variable
  apparentPower   = emon1.apparentPower;    //extract Apparent Power into variable
  powerFActor     = emon1.powerFactor;      //extract Power Factor into Variable
  supplyVoltage   = emon1.Vrms;             //extract Vrms into Variable
  Irms            = emon1.Irms;             //extract Irms into Variable 
  
 
  // power stuff
  
  usedPowerWatts = constrain( (int(realPower) - zeroPowerCorrection), 0, 9990);
  i= usedPowerWatts;
  usedPowerWatts = usedPowerWatts/10;
  if ((i % 10) > 4) ++usedPowerWatts;
  usedPowerWatts = usedPowerWatts *10;
  
  oldRealPowerValues[oRPVpointer] = usedPowerWatts;
  oRPVpointer++;
  oRPVpointer=oRPVpointer % oRPVsize;
   
  // calculate average of measured power
  
  tmp=0;
  for (i=0; i < oRPVsize; i++) tmp = tmp + oldRealPowerValues[i]; 
  usedPowerWattsAvr = tmp/oRPVsize;
  if ((tmp % 10) > 4) ++usedPowerWattsAvr;
  
  i= usedPowerWattsAvr;  
  usedPowerWattsAvr = usedPowerWattsAvr/10;
  if ((i % 10) > 4) ++usedPowerWattsAvr;
  usedPowerWattsAvr = usedPowerWattsAvr *10;
  
  // volts stuff
  
  instGridVolts = constrain( int(supplyVoltage*10), 0, 9990);
  i= instGridVolts;
  
  instGridVolts = instGridVolts/10;
  if ((i % 10) > 4) ++instGridVolts;
  
  oldIGVValues[oIGVpointer] = instGridVolts;
  oIGVpointer++;
  oIGVpointer=oIGVpointer % oIGVsize;
  
  // calculate average of measured voltage
  
  tmp=0;
  for (i=0; i < oIGVsize; i++) tmp = tmp + oldIGVValues[i]; 
  instGridVoltsAvr = tmp/oIGVsize;
  if ((tmp % 10) > 4) ++instGridVoltsAvr;
  
  gridPWR = usedPowerWattsAvr/unitBGrPWR;
  if ((usedPowerWattsAvr % unitBGrPWR) >= (unitBGrPWR/2)) ++gridPWR;
  
  solarPWR = instSolarPowerFromPVI/unitBGrPWR;
  if ((instSolarPowerFromPVI % unitBGrPWR) >= (unitBGrPWR/2)) ++solarPWR;
  
  if (instSolarPowerFromPVI > usedPowerWattsAvr) instSolarPowerLeft = instSolarPowerFromPVI - usedPowerWattsAvr;
  else instSolarPowerLeft=0;
  
  if (usedPowerWattsAvr > instSolarPowerFromPVI) usedPowerFromGrid = usedPowerWattsAvr - instSolarPowerFromPVI;
  else usedPowerFromGrid=0;
  
  
  //digitalWrite(ledGreen, LOW);  //Turns LED green OFF end of measurement
 
 
}

//-------------------------------------------
// Calculate Energy Consumption in a day
//-------------------------------------------

void calculateDailyEnergyCons()

{
long p1;
long p2;
long tmpWms;
long dxEnergy;

	p1=lastUsedPowerWattsAvr;
	p2=usedPowerWattsAvr;
	
	lastUsedPowerWattsAvr = usedPowerWattsAvr;
	energyMeasureLastStart = millis();
	
	if (p2 > p1) tmpWms=(p1*dxTime_ms) + ((p2-p1)*dxTime_ms/2);
	if (p1 > p2) tmpWms=(p2*dxTime_ms) + ((p1-p2)*dxTime_ms/2);
	if (p1 == p2) tmpWms=p2*dxTime_ms; 
	
	/*
	Serial.println(p1);
	Serial.println(p2);
	Serial.println(dxTime_ms);
	Serial.println(tmpWms);
	*/
	
	dxEnergy=tmpWms/1000;
	if ((tmpWms % 500) > 499) ++dxEnergy;
	
	dailyEnergyConsumption_Ws += dxEnergy;
	
	dailyEnergyConsumption_Wh = dailyEnergyConsumption_Ws/3600;
	if ((dailyEnergyConsumption_Ws % 3600) > 1799) ++dailyEnergyConsumption_Wh;
	
	dailyEnergyConsumption_kWhTenth = dailyEnergyConsumption_Wh/100;
	if ((dailyEnergyConsumption_Wh % 100) > 49) ++dailyEnergyConsumption_kWhTenth;
	
	 if ((millis() - energyMeasureStoreWait) > energyMeasureStoreRate)
    {
      energyMeasureStoreWait = millis();
	  if ((dailyEnergyConsumption_kWhTenth - readPersistentEnergyDay()) >= deltaStoreValue) 
		{
			writePersistentEnergyDay(dailyEnergyConsumption_kWhTenth);
		}
	}

}

//-------------------------------------------
// Display Power Values on LED's
//-------------------------------------------

void displayPowerValuesLocal()

{
  char sevenSegmentLEDLine[10]; //Used for sprintf

  digitalWrite(ledRed, LOW);
  ledRedState=false;
  
  digitalWrite(ledGreen, LOW); 
  
  DisplaySerial.write(DecimalControl);
  DisplaySerial.write((byte)DecPointTwoOff);
  
  switch (displayState) 
  {
	case 0:             //display Emon Power Value
      sprintf(sevenSegmentLEDLine, "%4d", usedPowerWattsAvr); //Convert Emon Power into a string that is right adjusted
      break;
  
	case 1:             //display Emon Power Value
      sprintf(sevenSegmentLEDLine, "%4d", usedPowerWattsAvr); //Convert Emon Power into a string that is right adjusted
      break;
          
	case 2:    			//display Energy used in one day
      if (dailyEnergyConsumption_kWhTenth < 10) sprintf(sevenSegmentLEDLine, "  0%1d", dailyEnergyConsumption_kWhTenth);
	  else sprintf(sevenSegmentLEDLine, "%4d", dailyEnergyConsumption_kWhTenth);
	  
	  digitalWrite(ledGreen, HIGH);  //green LED on
	  analogWrite(ledRed, intensityRed);  //red LED on
      ledRedState=true;
	  
	  DisplaySerial.write(DecimalControl);
	  DisplaySerial.write(DecPointTwoOn);
	  
	  if (solarPWR == 0) ++displayState; //skip next display state
	  
      break;
	  
	case 3:
      sprintf(sevenSegmentLEDLine, "%4d", usedPowerWattsAvr); //if no if case (below) occurs
      
      if ((instSolarPowerLeft > 0) && (solarPWR > 0))   // and only when at least one bar of the graph is on
        {
               // display "green" (solar) power available
               sprintf(sevenSegmentLEDLine, "%4d", instSolarPowerLeft);
               digitalWrite(ledGreen, HIGH);  //green LED on
    
         }
       else if (solarPWR > 0)  // only when at least one bar of the graph is on
         {
               // display "red" power taken from grid
               sprintf(sevenSegmentLEDLine, "%4d", usedPowerFromGrid);
              analogWrite(ledRed, intensityRed);  //red LED on
              ledRedState=true;
       
         }
        break;
		         

  }
  
  DisplaySerial.print(sevenSegmentLEDLine); //Send serial string out the soft serial port to the display
  
  // Serial.print(sevenSegmentLEDLine); //Send serial string out the soft serial port to the display
    
  ++displayState;
  displayState = displayState % 4;
  
 
}


//-------------------------------------------
// Update LED Bargraph
//-------------------------------------------

void updateBargraph(byte redbarval, byte yellowbarval)

{
unsigned long shiftReg32;
int i;
unsigned int shiftRegMSWord;
unsigned int shiftRegLSWOrd;

shiftReg32=0;

  redbarval=constrain(redbarval,0,20);  //redbar has 20 LED's
  yellowbarval=constrain(yellowbarval,0,10);  //yellowbar has 10 LED's

  if (redbarval > 0) 
  {
    for (i=0; i<redbarval; i++) bitSet(shiftReg32, i); 
  }

  if (yellowbarval > 0) 
  {
    for (i=0; i<yellowbarval; i++) bitSet(shiftReg32, i+20);
  }

  shiftRegLSWOrd=shiftReg32;
  shiftRegMSWord=shiftReg32 >> 16;

   // take the latchPin low so 
    // the LEDs don't change while you're sending in bits:
    digitalWrite(latchPin, LOW);
    // shift out the bits:
    shiftOut(dataPin, clockPin, MSBFIRST, highByte(shiftRegMSWord));  
    shiftOut(dataPin, clockPin, MSBFIRST, lowByte(shiftRegMSWord));  
    shiftOut(dataPin, clockPin, MSBFIRST, highByte(shiftRegLSWOrd));  
    shiftOut(dataPin, clockPin, MSBFIRST, lowByte(shiftRegLSWOrd));  

    //take the latch pin high so the LEDs will light up:
    digitalWrite(latchPin, HIGH);
    

}


//-------------------------------------------
// Write Log Info to Console
//-------------------------------------------

void writeLogData()

{
  
  Serial.print("Urms=");
  Serial.print(supplyVoltage);
  Serial.print(";Uigv=");
  Serial.print(instGridVolts);
  Serial.print(";UigvAvr=");
  Serial.print(instGridVoltsAvr);
  Serial.print(";Irms=");
  Serial.print(Irms);
  Serial.print(";Preal=");
  Serial.print(realPower);
  Serial.print(";PrealWatts=");
  Serial.print(usedPowerWatts);
  Serial.print(";PrealWattsAvr=");
  Serial.print(usedPowerWattsAvr);
  Serial.print(";Papp=");
  Serial.print(apparentPower);
  Serial.print(";cosFi=");
  Serial.print(powerFActor);
  Serial.print(";Upvi=");
  Serial.print(instGridVoltageFromPVI);
  Serial.print(";Ppvi=");
  Serial.print(instSolarPowerFromPVI);
  Serial.print(";EWh=");
  Serial.print(dailyEnergyConsumption_Wh);
  Serial.print(";wCnt=");
  Serial.print(writePersistentCounter);
  Serial.println(";");
  
  
  // for (int i=0; i<20; i++) { Serial.print(i); Serial.print(';');Serial.print(rawCurrent[i]);Serial.print(';'); }
  
  // Serial.println();
  
  
}



//**********************************************************************************
// Setup
//**********************************************************************************

void setup()
{  
  Serial.begin(115200);
  DisplaySerial.begin(9600);
  
  XBeeSerial.begin(19200);
  XBeeSerial.listen();
  pinMode(rtsXBeePin, OUTPUT); 
  
  pinMode(ledRed,OUTPUT);
  pinMode(ledGreen, OUTPUT);
 
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  
 
  // 16.11.2013 Anzeige 250V gemessen 240 V -> 
  // emon1.voltage(0, 218.0, -2.15);  // Voltage: input pin, calibration, phase_shift
  // wenn kalt emon1.voltage(0, 209.0, -2.15);  // Voltage: input pin, calibration, phase_shift
  
  // mit 1k last über trafo Ueff = 12.3V Uin=249V
  // emon1.voltage(0, 239.0, -2.15);  // Voltage: input pin, calibration, phase_shift
  
  emon1.voltage(0, 239.0, -1.75);  // Voltage: input pin, calibration, phase_shift
  
  //emon1.current(1, 33.8);             // Current: input pin, calibration.
  // ENEL Messung -  10% weniger als ArdaSol Emon
  // 33.8 *0.9 =  30.4
  // 33.8 * 0.85 = 28.7
  
  // 10.12.2013: ENEL kWh Messung während 8 Tagen -> Abweichung + 1.7%
  // 30.4 * 0.983 = 29.9 (vorher war es 30.4)
  
  emon1.current(1, 30.0);             // Current: input pin, calibration.

  Serial.print("ArdaSolPwrMon ");
  Serial.print(Version);
  Serial.println(Release);
  
  DisplaySerial.write(ClrDisp);
  
  for (int i=0; i<oRPVsize; i++) oldRealPowerValues[i]=0;
  for (int i=0; i<oIGVsize; i++) oldIGVValues[i]=0;
  
  oRPVpointer=0;
  oIGVpointer=0;
  
  sequenceCounter=0;
  displayState=0;
  
  digitalWrite(ledRed, LOW);
  ledRedState=false;
  digitalWrite(ledGreen, LOW);
  
  dataDisplayStart= millis() - dataDisplayWait;            // force imediate display action
  barGraphDisplayStart= millis() - barGraphDisplayWait;    // force imediate display action
  gridMeasureStartWait= millis()- gridMeasureWait;         // force imediate measure action

  energyMeasureLastStart=millis();
  energyMeasureStoreWait = millis();
  
  // only first time
  // writePersistentEnergyDay(dailyEnergyConsumption_kWhTenth);
  // only first time
  
  dailyEnergyConsumption_kWhTenth = readPersistentEnergyDay();
  dailyEnergyConsumption_Ws = dailyEnergyConsumption_kWhTenth * 100;   // zehntel kWh *100 = Wh
  dailyEnergyConsumption_Ws = dailyEnergyConsumption_Ws * 3600;   		// Wh * 3600 = Ws
  
  Serial.print("persistent Wh = ");
  Serial.println(dailyEnergyConsumption_kWhTenth*100);
  Serial.print("persistent Ws = ");
  Serial.println(dailyEnergyConsumption_Ws);
  
  
 }

//**********************************************************************************
// Main Loop
//**********************************************************************************

void loop()
{
  
	   
  if ((millis() - gridMeasureStartWait) > gridMeasureWait)   // when no command packets are comming
     {
       measureGridPower();
     }     
  
  dxTime_ms=(millis() - energyMeasureLastStart);
  if (dxTime_ms > energyMeasureRate)
	{
		calculateDailyEnergyCons();		
	}
	
  if (XBeeSerial.available())
     { 
        gridMeasureStartWait= millis();
        if (CommandPacketReceived())    SendResponseFromEmon(); 
     }  
	 

   if ((millis() - barGraphDisplayStart) > barGraphDisplayWait)
    {
      barGraphDisplayStart = millis();
      updateBargraph(gridPWR,solarPWR);
      sequenceCounter++; 
    }

	
   if ((millis() - dataDisplayStart) > dataDisplayWait)
    {
      dataDisplayStart = millis();
      displayPowerValuesLocal();
      writeLogData();    
    }
  
}
