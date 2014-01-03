/* 
----------------------------
  A r d a S o l   Project
----------------------------
*/

#define Version "V5.0"
#define Release "R9"

/*
Version: 5.0
Version Date: 10.12.2013

Creation Date: 10.5.2013
Author: Heinz Pieren
Email: heinz.pieren@semprevacanze.it

Abstract
--------
This Software runs on ARDUINO MEGA and communicates with an Aurora Power One Photovoltaik Inverter
Model PVI-3.0-TL-OUTD-S. It gets from the inverter the following data:
- Instant date and time
- Instant power produced
- Daily grid peak power
- Daily cumulated energy
- Total energy produced
- Daily average energy produced

The consumed energy data is get from the ArdaSol Energy Monitor.
It sends down:
- Solar power produced
- Command for resetting the daily energy consumption counter
It gets up:
- Grid voltage
- Instant power consumed
- Energy consumed in a day

This data set will be displayed on a Sure Electronics Dot Matrix 32x16 dual color display.
Data Acqusition is done on a SD-Card and Part of Data is sent do the Xively Server on the internet.

The phtovoltaic plant is located in Italy, Region Apulia, location San Pietro In Bevagna, next to the famous
place Manduria, where the marvellous Primitivo Vine is produced.
Coordinates: 
40.312115 North
17.697827 East
It's a 2.88kWp plant with 12 Sunerg XP60/156 240Wp panels on the roof of our house.
The plant started to produce energy in August 2012 and I will expect in the first year at total of more than 4000kWh.
Our private electricity needs are about 2000kWh in one year, so we will feed into the grid about 2000kWh.

Thanks to:
---------
- PVI Communication Doc. on http://stephanos.io/archives/96
- CRC16 Calculation Algorithm and Aurora communication definitions: Curtis J. Blank curt@curtronics.com
- Arduino demo program for Holtek HT1632 LED driver chip,
  Nov, 2008 by Bill Westfield ("WestfW")Copyrighted and distributed under the terms of the Berkeley license
  (copy freely, but include this notice of original author.) Dec 2010, FlorinC - adapted for 3216 display
- Open Energy for the emon library http://openenergymonitor.org/emon/

*/

#include <Time.h>

// #include <SoftwareSerial.h>

// #define rxPin 2
// #define txPin 3

#define DataLED  5		// pin nr. 5

// SoftwareSerial auroraSerial(rxPin, txPin); 

// unsigned long 		 lastCmdRequestTime; 

// const unsigned long 	 requestInterval = 10*1000; //delay bewteen two requests

#define DISPLAYTIME  3000  // Dislaytime of each panel

// log messages for USB Serial port


#define logMsgFieldDefs		"F01:"		// field definitions of log file

#define logMsgStart			"D01:Start"
#define logMsgData			"D02:Data"

#define logMsgShowData		"I01:ShowData"
#define logMsgTimeSync		"I02:TimeSync"
#define logMsgNoPVI			"I03:NoPVIfound"
#define logMsgGotoSleep		"I04:GotoSleep"
#define logMsgAwaked		"I05:Awaked"

#define logMsgNoSDcard		"I06:NoSDfound"
#define logMsgSDcard		"I07:SDfound"

#define logMsgNoIntConn		"I08:NoIntConn"
#define logMsgIntConn		"I09:IntConn"

#define logMsgFlushRecBuf	"E01:FlushRB x="
#define logMsgRecChkSumErr	"E02:RecChkSumErr"
#define logMsgRecTimeout	"E03:RecTimeout"

#define logMsgLen 150

char logMessage[logMsgLen];

// Emon Data definitions

unsigned int		GridEmonVoltage = 0;
unsigned int    	GridEmonPower = 0;

//unsigned int		EmonEnergyDaykWhTenth = 0;

unsigned long		EmonEnergyDayWs = 0;
unsigned long		EmonEnergyDayWh = 0;


//unsigned long       EmonEnergyTotalkWhTenth = 0;

unsigned long       EmonEnergyTotalWh = 0;
unsigned long		EmonEnergyTotalkWh = 0;

unsigned long       EmonEnergyAverageDay = 0; // average a day calculated from start of year

unsigned int		EmonEnergyDayWriteCounter = 0;

boolean 			resetEnergyDayUsed = false;

// energy balance definitions

unsigned long		OldEmonEnergyDayWh = 0;
unsigned long		OldEnergyDayWh = 0;

long		        EnergyConsFromGridWh = 0;

// Aurora Data definitions

unsigned long            DateAndTime = 0; //absolute format of date and time in seconds

unsigned long            GridVoltage = 0; //grid tension in Volts

unsigned long            GridPower = 0; //power fed to grid in W

unsigned long            GridPowerPeakDay = 0; //peak value of power fed to grid in W

unsigned long            EnergyDay = 0; // energy produced in day in Wh
unsigned long            EnergyTotal = 0; // total energy produced in inverters lifetime in Wh
unsigned long            EnergyTotalkWh = 0;
unsigned long            EnergyAverageDay = 0; // average a day calculated from start of operation of the PVI

// Absolute time defs

#define timeSyncTrigger  30    //seconds, if timedifference > timeSyncTrigger then sync arduino clock

const unsigned long      timeOffset1970to2000 = 946684800;    // because aurora time base is 1.1.2000
const unsigned long      startOfPVIOperationDate = 1345161600;  // it was the 17.8.2012 

unsigned long            at;      // current time

byte                     solDay;

long                     timeDiff;
boolean                  arduinoClockIsSet;
boolean                  inverterIsAlife;

boolean                  sleepMode;

boolean                  SDcardAvailable;        // true when sd card is inserted 

boolean 				internetAvailable;
byte 					internetConnFailCnt;
#define maxConnFail		20

unsigned long            dataAcqTimeStart;
const unsigned long      dataAcqWait = 20000;  //Data Acquisitopn Rate in ms

int                       lastSleepTime;        // last time went to sleep, stored as minutes of a day
int                       lastAwakeTime;        // last time awaked, stored as minutes of a day

int                       startTimeAwakeCheck;      // starts the awake process

#define earliestAwakeTimeLimit   5*60    //  set to lastAwakeTime in setup routine
#define latestAwakeTimeLimit     7*60    //  latest awake time at 7am

#define earliestSleepTimeLimit   18*60    // set to lastSleepTime in setup routine
#define latestSleepTimeLimt      21*60    // latest sleep time at 9pm


#define sleepTimeTolerance        30      // minutes tolerance time in minutes check before lastSleepTime

#define awakeCheckPeriod          10      // minutes interval for request time in the awake process

byte                      sleepAttempts;      // counter  for going to sleep
#define maxSleepAttempts  10



// temperature stuff

int ambientTemperature;
unsigned long messuredTemp;


int ambientTempAvrOneTenthClesius;

#define oTVsize 10
int oldTempValues[oTVsize];
byte oTVpointer;

//TMP36 Pin Variables
#define temperaturePin 0 //the analog pin the TMP36's Vout (sense) pin is connected to
                        //the resolution is 10 mV / degree centigrade 
                        //(500 mV offset) to make negative temperatures an option
                        
                        
// IR Sensor stuff

#define IRSensorPin A1


                        
//-----------------------------------------------------------------------
// Communication stuff
#include <ArdaSolComm.ino>
//-----------------------------------------------------------------------


//-----------------------------------------------------------------------
// Display stuff

// possible values for a pixel;
#define BLACK  0
#define GREEN  1
#define RED    2
#define ORANGE 3

#include <ArdaSolDisp.ino>
//-----------------------------------------------------------------------


//-----------------------------------------------------------------------
// Data Acquisition stuff


boolean xivelySentSolarDataState ;

#include <ArdaSolDatAcq.ino>
//-----------------------------------------------------------------------


//-----------------------------------------------------------------------
// EEPROM Stuff

#include <EEPROM.h>

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
// Writes the total amount of energy to persistent memory (PVI total Wh)

void writePersistentEnergyTotal(unsigned long totEWr)

{

unsigned int hwrd;

hwrd = totEWr >> 16;  // high word of total energy

EEPROM.write(0, lowByte(totEWr));		// location 0
EEPROM.write(1, highByte(totEWr));		// location 1
EEPROM.write(2, lowByte(hwrd));			// location 2
EEPROM.write(3, highByte(hwrd));		// location 3

}

//-----------------------------------------------------------------------
// reads the total amount of energy from persistent memory (PVI total Wh)

unsigned long readPersistentEnergyTotal()

{

unsigned long x;
unsigned long y;

y=EEPROM.read(0);		// location 0
x=EEPROM.read(1);		// location 1
y+= x << 8;
x=EEPROM.read(2);		// location 2
y+= x << 16;
x=EEPROM.read(3);		// location 3
y+= x << 24;

return(y);

}

//-----------------------------------------------------------------------
// Writes the total amount of energy to persistent memory (emon total Wh)

void writePersistentEmonEnergyTotal(unsigned long totEWr)

{

unsigned int hwrd;

hwrd = totEWr >> 16;  // high word of total energy

EEPROM.write(4, lowByte(totEWr));		// location 4
EEPROM.write(5, highByte(totEWr));		// location 5
EEPROM.write(6, lowByte(hwrd));			// location 6
EEPROM.write(7, highByte(hwrd));		// location 7

}

//-----------------------------------------------------------------------
// reads the total amount of energy from persistent memory (emon total Wh)

unsigned long readPersistentEmonEnergyTotal()

{

unsigned long x;
unsigned long y;

y=EEPROM.read(4);		// location 4
x=EEPROM.read(5);		// location 5
y+= x << 8;
x=EEPROM.read(6);		// location 6
y+= x << 16;
x=EEPROM.read(7);		// location 7
y+= x << 24;

return(y);

}

//-----------------------------------------------------------------------
// Writes the start date of energy total statistic counter to EEPROM

void writePersistentStartDateEnergyTotal(unsigned long startD)

{

unsigned int hwrd;

hwrd = startD >> 16;  // high word of total energy

EEPROM.write(8, lowByte(startD));		// location 8
EEPROM.write(9, highByte(startD));		// location 9
EEPROM.write(10, lowByte(hwrd));		// location 10
EEPROM.write(11, highByte(hwrd));		// location 11

}

//-----------------------------------------------------------------------
// reads the start date of energy total statistic counter from EEPROM

unsigned long readPersistentStartDateEnergyTotal()

{

unsigned long x;
unsigned long y;

y=EEPROM.read(8);		// location 8
x=EEPROM.read(9);		// location 9
y+= x << 8;
x=EEPROM.read(10);		// location 10
y+= x << 16;
x=EEPROM.read(11);		// location 11
y+= x << 24;

return(y);

}

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

void printDigits(int digits)
{
  // utility function for digital clock display: prints preceding colon and leading 0
  
  int cx;
  int i;
  
	i=0;
	cx=strlen(logMessage);
	i = i+cx;
	cx = snprintf (logMessage+i, logMsgLen-i, ":");
	i = i+cx;
  
	if(digits < 10) 
		{
			cx= snprintf (logMessage+i, logMsgLen-i, "0");
			i = i+cx;
		}
	snprintf (logMessage+i, logMsgLen-i, "%d",digits);
}

//-----------------------------------------------------------------------

void checkSleepMode()
{
  int atMinutes;
  unsigned long atatempt;
  
  atMinutes= hour(at)*60 + minute(at);
  
  if (sleepMode)             // we are sleeping, time to awake?
    {  
                     
       if ((solDay != day(at)) &&  ((atMinutes > latestAwakeTimeLimit) || (atMinutes > startTimeAwakeCheck)))
           {
              getTime();
              atatempt = DateAndTime + timeOffset1970to2000;
              if (year(atatempt) > 2012)
                {
                  sleepMode = false;
                  sleepAttempts = 0;  
                  lastAwakeTime = atMinutes;			  
                  writeLogData(logMsgAwaked);
				  writeLogData(logMsgFieldDefs);
				  
				  if ((month(atatempt) == 1) && (day(atatempt) == 1))  // alwas at 1.January we reset the total energy counters
				  {
					EmonEnergyTotalWh = 0;
					EmonEnergyTotalkWh = 0;
					EnergyTotal = 0;
					EnergyTotalkWh = 0;
					writePersistentEnergyTotal(EnergyTotal);
					writePersistentStartDateEnergyTotal(atatempt);
				  }
				  
				  writePersistentEmonEnergyTotal(EmonEnergyTotalWh);
				  resetEnergyDayUsed=true;
				  EnergyConsFromGridWh=0;
				  OldEmonEnergyDayWh=0;
				  OldEnergyDayWh=0;
				  sendNewDayDataToXively();			  
                }
              else startTimeAwakeCheck += awakeCheckPeriod;
           }
    } // end of then (sleepMode)
  else                      // we are awake, goto sleep?
    {
      if ((atMinutes > latestSleepTimeLimt) || (sleepAttempts > maxSleepAttempts))
        {
          sleepAttempts=0;
          sleepMode = true;  //go to sleep allowed only at a specific time window
          lastSleepTime = atMinutes;
          startTimeAwakeCheck = earliestAwakeTimeLimit;
          writeLogData(logMsgGotoSleep);
		  writePersistentEnergyTotal(EnergyTotal);
          sendSleepDataToXively();  
        }
      
      else 
        {
          if ((atMinutes > (lastSleepTime - sleepTimeTolerance))) sleepAttempts++;
        }
            
    } // end of else (sleepmode)
  
}

//-----------------------------------------------------------------------
// manage all time stuff

void manageTimeStuff()
{
    
    DateAndTime = 0;
    
    if (!sleepMode) getTime();
    
    at = DateAndTime + timeOffset1970to2000;

    if (year(at) > 2012) 
        {
          inverterIsAlife = true;
          solDay = day(at);                  // current solar day
        }
    else inverterIsAlife = false;
     
    if (arduinoClockIsSet)
       {
         if (inverterIsAlife)
             {  
               timeDiff = at - now();
               timeDiff = abs(timeDiff);
               if ((timeDiff > timeSyncTrigger) && (year(at) > 2012))
                 {
                   setTime(at);    // synchronize arduino clock with inverter
                   writeLogData(logMsgTimeSync);
                 }
           
             } //end of then (inverterIsAlife)
          else 
             {
               at = now();                 // time from inverter is not available take time from arduino
               checkSleepMode();           // goto sleep or awake?
             } // end of else (inverterIsAlife)   
             
       } //end of then (arduinoClockIsSet)
    else 
       {
         if ((inverterIsAlife) && (year(at) > 2012))
              {
                setTime(at);                      // Sync the arduino clock
                writeLogData(logMsgTimeSync);
                arduinoClockIsSet = true;
              }
       } //end of else (arduinoClockIsSet)
 
}

//-----------------------------------------------------------------------
// Get all Data from PVI

void getEnergyData()
{
  
if (inverterIsAlife)
     {
       digitalWrite(DataLED, HIGH);  //Turns Data LED On
	   
	   // GridVoltage = 0;
       getGridVoltage();
	   
	   // GridPower = 0;
       getGridPower();
  
       digitalWrite(DataLED, LOW);  //Turns Data LED Off
		
	   // GridPowerPeakDay = 0;
       getGridPowerPeakDay();
	   
	   //digitalWrite(DataLED, HIGH);  //Turns Data LED On
  
       // EnergyDay = 0;
       getEnergyDay();
	   
	   //digitalWrite(DataLED, LOW);  //Turns Data LED Off
  
       //EnergyTotal = 0;
       //EnergyTotalkWh = 0; 	   
       // getEnergyTotal(); not used due to memroy failure in PVI
	   
	   if ((EnergyDay + readPersistentEnergyTotal()) > EnergyTotal) 
	   {
			EnergyTotal = EnergyDay + readPersistentEnergyTotal();  
			EnergyTotalkWh = EnergyTotal / 1000;
			if ((EnergyTotal % 1000) > 499)   ++EnergyTotalkWh;  // round up
	   }
	   
     }
 else if ((arduinoClockIsSet) && (solDay == day(at) )) 
     {
       GridPower = 0;
     }
 else
     {
       GridPower = 0;
       GridPowerPeakDay = 0;
       EnergyDay = 0;
     }  
}

//-----------------------------------------------------------------------
// Get all Data from ArdaSol Energy Monitor

void getEmonEnergyData()
{

	digitalWrite(DataLED, HIGH);  //Turns Data LED On
	   
	GridEmonVoltage = 0;
	getEmonGridVoltage();
	
	GridEmonPower = 0;
	getEmonGridPower();
	
	// EmonEnergyDayWs = 0;
	getEmonEnergyDay();
	
	EmonEnergyDayWh = EmonEnergyDayWs/3600;
	if ((EmonEnergyDayWs % 3600) > 1799) ++ EmonEnergyDayWh;
	  
	EmonEnergyTotalWh = readPersistentEmonEnergyTotal();
	EmonEnergyTotalWh = EmonEnergyTotalWh + EmonEnergyDayWh;
	
	EmonEnergyTotalkWh = EmonEnergyTotalWh/1000;
	if ((EmonEnergyTotalWh % 1000) > 499) ++EmonEnergyTotalkWh;
	
  
    digitalWrite(DataLED, LOW);  //Turns Data LED Off


}
//-----------------------------------------------------------------------
void getAmbientTemperature()
{

// Volatge refernece set 1100mV
// 10mV = 1 degree celsius

byte i;
long x;
long y;

unsigned long avrsum;

const int c1 = 480;  // Offset für diesen sensor
const int c2 = 110;  // Reference voltage 110 is 1100 mV
const int c3 = 1023; // analog to digital converter resolution

x = 0;
avrsum=0; 


 for (i=0; i<10; i++) 
     {
       x = x + analogRead(temperaturePin);
       delay(10);
     } 

 messuredTemp = x/10;
 if ((x % 10) > 4) messuredTemp++;

 
x =  (messuredTemp - c1) * c2;
ambientTemperature = x/c3;
if ((x % c3) > 511) ambientTemperature++;

// write temp value to average array
if (oTVpointer > oTVsize) 
	{							// first init of array
		for (i=0; i < oTVsize; i++) oldTempValues[i] = messuredTemp;  // init average calc array
		oTVpointer=0;
	}
else
	{
		oldTempValues[oTVpointer] = messuredTemp;
		oTVpointer++;
		oTVpointer=oTVpointer % oTVsize;
	}

// calculate average of measured temp
for (i=0; i < oTVsize; i++) avrsum= avrsum+oldTempValues[i]; 

x = avrsum/oTVsize;
if ((avrsum % 10) > 4) x++;

Serial.print("ptr=");
Serial.print(oTVpointer);

Serial.print(" avr=");
Serial.print(x);

y = (x - c1) * c2 * 10; //zehntelgrad deshalb mal 10
ambientTempAvrOneTenthClesius = y/c3;
if ((y % c3) > 511) ambientTempAvrOneTenthClesius++;

Serial.print(" ATavr=");
Serial.println(ambientTempAvrOneTenthClesius);


}

//-----------------------------------------------------------------------
// calculates to averag value of energy day since start of operation

void calculateAverageDayEnergy()
{
unsigned int daysInOperation;

if ((year(at) > 2012) && (EnergyTotalkWh > 0))
    {
      daysInOperation = ((at - readPersistentStartDateEnergyTotal() ) / 86400) + 1;
	  // daysInOperation = constrain(daysInOperation,1,366);	// only one year
      EnergyAverageDay = EnergyTotal / daysInOperation;
    }
else EnergyAverageDay = 0;

EnergyAverageDay = EnergyAverageDay % 20000;  // because of PVI RAM Failed problem
}

//-----------------------------------------------------------------------
// calculates the daily energy balance grid/solar

void calculateEnergyBalance()
{
unsigned int daysInOperation;

if ((year(at) > 2012) && (EmonEnergyTotalkWh > 0))
    {
      daysInOperation = ((at - readPersistentStartDateEnergyTotal() ) / 86400) + 1;
	  // daysInOperation = constrain(daysInOperation,1,366);	// only one year
      EmonEnergyAverageDay = EmonEnergyTotalWh / daysInOperation;
    }
else EmonEnergyAverageDay = 0;



long deltaEmon;
long deltaSolar;

deltaEmon = EmonEnergyDayWh - OldEmonEnergyDayWh;
if ((deltaEmon < 0) || (deltaEmon > 200)) deltaEmon = 0;   // we only allow positive changes and not greater than 0.2 kW
OldEmonEnergyDayWh = EmonEnergyDayWh;

deltaSolar = EnergyDay - OldEnergyDayWh;
if ((deltaSolar < 0) || (deltaSolar > 100)) deltaSolar = 0;   // we only allow positive changes and not greater than 0.1 kW
OldEnergyDayWh = EnergyDay;

if ((deltaEmon - deltaSolar) > 0) EnergyConsFromGridWh = EnergyConsFromGridWh + (deltaEmon - deltaSolar);

}
 
 
//-----------------------------------------------------------------------
// check state of IR-Sensor

boolean someOneIsHere()
{
 return digitalRead(IRSensorPin);
}

//-----------------------------------------------------------------------
// send logdata to serial interface

void writeLogData(char msgID[])
{

int cx;
int i;

i=0;
cx=0;

 
if (msgID[0] == 'F')
 {					// log filed keywords
  cx = snprintf (logMessage, logMsgLen, "Date;");   // 1.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Wday;");   // 2.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Time;");   // 3.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Tcel;");   // 4.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Tadc;");   // 5.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Msg;");   // 6.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Atime;");   // 7.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Tdif;");   // 8.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Pday;");   // 9.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Ppk;");   // 10.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Eday;");   // 11.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Etot;");   // 12.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Eavr;");   // 13.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Ugr;");   // 14.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Pem;");   // 15.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Uem;");   // 16.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "EemD;");   // 17.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "EemT;");   // 18.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "EgrdD;");   // 19.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "EWCtr;");   // 20.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "\r\n");   // 21.
  
 }

else 
 {
  if (day() < 10) 
		{
			cx = snprintf (logMessage, logMsgLen, "0");		// 1. date
		}
  i = i +cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "%d.",day());	
  
  
  
  if (month() < 10) 
		{
			i = i +cx;
			cx = snprintf (logMessage+i, logMsgLen-i, "0");	
		}
  i = i +cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "%d.%d;",month(),year());	
  
  i = i +cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "%d;",weekday());	 // 2. day in week

  i = i +cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "%d",hour());			// 3. time

  printDigits(minute());
  printDigits(second());
  
  i=strlen(logMessage);
  cx = snprintf (logMessage+i, logMsgLen-i, ";");
  
  i = i +cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "%d;",ambientTemperature);	 // 4. temperature in degree celsius

  i = i +cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",messuredTemp);	 // 5. temperature value from AD converter
 
  i = i +cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "%s;",msgID);	 // 6. message id
 
  i = i +cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",DateAndTime);	 // 7. PVI absolute time based on 1.1.2000
 
  i = i +cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",timeDiff);	 // 8. Time difference Arduino clock vs PVI clock
  
  if (msgID[0] == 'D')
     {
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",GridPower);	 // 9.  Instantaneous power to grid
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",GridPowerPeakDay);	 //  10. Power peak sent to grid
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",EnergyDay);	 //  11. Energy produced this day Wh
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",EnergyTotal);	 // 12. Total energy produced since start Wh
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",EnergyAverageDay);	 // 13. Average energy a day
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",GridVoltage);	 // 14. Grid Voltage
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;",GridEmonPower);	 // 15. Emon Power
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;",GridEmonVoltage);	 // 16. Emon Voltage
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;", EmonEnergyDayWh);	 // 17. Emon Energy day in Wh
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;", EmonEnergyTotalWh);	 // 18. Emon Energy Tot in Wh
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;", EnergyConsFromGridWh);	 // 19. Energy taken from Grid a day
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;", EmonEnergyDayWriteCounter);	 // 20. Emon Energy write EEPROM counter
	
	i = i+cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "\r\n");   // 20.
	
     }
   else
     {
	
    i = i+cx;
    cx = snprintf (logMessage+i, logMsgLen-i, ";;;;;;;;;;;;");   // 9. - 20.	
	i = i+cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "\r\n");   // 20.
	
     }

 }
 
Serial.print(logMessage);
if (getLogFileNameFromDate()) WriteDataRecordToSD();

}
//-----------------------------------------------------------------------
// Shows the data on the dot matrix display

void showDataOnDisplay()

{
  if (arduinoClockIsSet)
    {
      writeLogData(logMsgShowData);
    
      displayTempAndTime(ambientTemperature,hour(at),minute(at));
      delay(DISPLAYTIME);
      clearDisplay();

      if (inverterIsAlife)
	  {	displayPowerDayPro(GridPower);
		delay(DISPLAYTIME);
		clearDisplay();
	  }
      
      displayPowerCons(GridEmonPower );
      delay(DISPLAYTIME);
      clearDisplay();

      if (GridPowerPeakDay > 0) 
	  {	displayPowerDayPeakPro(GridPowerPeakDay);
		delay(DISPLAYTIME);
		clearDisplay();
	  }

      if (EnergyDay > 0)
	  {	displayEnergyDayPro(EnergyDay);
		delay(DISPLAYTIME);
		clearDisplay();
	  }
	  
	  displayEnergyDayCons(EmonEnergyDayWh);
      delay(DISPLAYTIME);
      clearDisplay();
	  
	   if (EnergyConsFromGridWh >= 0)
	  {	displayEnergyDayConsGrid(EnergyConsFromGridWh);
		delay(DISPLAYTIME);
		clearDisplay();
	  }

      displayEnergyTotalPro(EnergyTotalkWh);
      delay(DISPLAYTIME);
      clearDisplay();
	  
	  displayEnergyTotalCons(EmonEnergyTotalkWh);
      delay(DISPLAYTIME);
      clearDisplay();
    
      displayEnergyAverageDayPro(EnergyAverageDay);
      delay(DISPLAYTIME);
      clearDisplay();
	  
	  displayEnergyAverageDayCons(EmonEnergyAverageDay);
      delay(DISPLAYTIME);
      clearDisplay();
    
      displayWeekdayAndDate(weekday(at),day(at),month(at));
      delay(DISPLAYTIME);
      clearDisplay();
    }
    else 
    {
      writeLogData(logMsgNoPVI);
      displayL1andL2("Arda ",RED,"  Sol",ORANGE);
      delay(DISPLAYTIME);
      clearDisplay();
      displayL1andL2("noPVI",RED,"found",RED);
      delay(DISPLAYTIME);
      clearDisplay();
    }
 
}

//-----------------------------------------------------------------------
// Init stuff
//-----------------------------------------------------------------------

void setup() 
{
    
  pinMode(DataLED, OUTPUT);
  
  digitalWrite(DataLED, HIGH);  //Turns Data LED On
  
  oTVpointer=oTVsize+1;   // means array of old temperature values not initialized
  
  analogReference(INTERNAL1V1);
  
  pinMode(IRSensorPin, INPUT);

  Serial.begin(115200);        // log output
  Serial3.begin(19200);    		// aurora PVI via XBee
  
// **************************************************************
// Init value has to be written only once or for corrections
// **************************************************************
// Switched OFF

// writePersistentEnergyTotal(5775000);   			   //total 5775 kWh value dated 11.12.2013 14:45
// writePersistentEmonEnergyTotal(2978300);  			  // total 2979 kWh value dated 13.12.2013 14:45
// writePersistentStartDateEnergyTotal(startOfPVIOperationDate);  //start of PVI operation = 17.8.2012

// Switched OFF
// **************************************************************
// Init value has to be written only once or for corrections
// **************************************************************
  
  EnergyTotal = 0;
  EnergyTotal=readPersistentEnergyTotal();
  Serial.print("Pers_Wh PVI=");
  Serial.println(EnergyTotal);
  
  EmonEnergyTotalWh = 0;
  EmonEnergyTotalWh=readPersistentEmonEnergyTotal();
  Serial.print("Pers_Wh Emon=");
  Serial.println(EmonEnergyTotalWh);
   
  Serial.print("StatStartDate=");
  Serial.println((unsigned long) readPersistentStartDateEnergyTotal());
  
  resetEnergyDayUsed=false; //flag for signalling reset daily energy counter in emon

  
// ---------------------------------------------------------------------------------------------  
  
  
  
  ht1632_setup();
  
  initSDCard();
  
  initEthernetCard();

  
  
  digitalWrite(DataLED, LOW);  //Turns Data LED Off
  
  displayL1andL2("Arda ",RED,"  Sol",ORANGE);
  delay(DISPLAYTIME);
  clearDisplay();
  displayL1andL2(Version,GREEN,Release,GREEN);
  delay(DISPLAYTIME);
  clearDisplay();
  
  arduinoClockIsSet = false;
  
  sleepMode = false;
  sleepAttempts = 0;
  
  lastSleepTime = earliestSleepTimeLimit;
  lastAwakeTime = earliestAwakeTimeLimit;
  startTimeAwakeCheck = earliestAwakeTimeLimit;
  
  dataAcqTimeStart = millis();  //remember when started data acq
  manageTimeStuff();
  getEnergyData();
  getEmonEnergyData();
  getAmbientTemperature();
  calculateAverageDayEnergy();
  
  if ((EmonEnergyDayWh - EnergyDay) >= 0) 
  {
	EnergyConsFromGridWh = EmonEnergyDayWh - EnergyDay;  //starup compromise!
	if (EmonEnergyDayWh >0) OldEmonEnergyDayWh = EmonEnergyDayWh;
	if (EnergyDay >0) OldEnergyDayWh = EnergyDay; 
  }
  
  calculateEnergyBalance();
  
  if (SDcardAvailable) writeLogData(logMsgSDcard);
  else writeLogData(logMsgNoSDcard);
  
  if (internetAvailable) writeLogData(logMsgIntConn);
  else writeLogData(logMsgNoIntConn);
  
  writeLogData(logMsgFieldDefs);
  
  writeLogData(logMsgStart);
  
  xivelySentSolarDataState = false;
  
}

//-----------------------------------------------------------------------
// Main Loop
//----------------------------------------------------------------------- 

void loop()
{
  if ((millis() - dataAcqTimeStart) > dataAcqWait)
     {
        dataAcqTimeStart = millis();  //remember when started data acq
        manageTimeStuff();
        getEnergyData();
		getEmonEnergyData();
        getAmbientTemperature();
        calculateAverageDayEnergy();
		calculateEnergyBalance();
		writeLogData(logMsgData);
        if (!sleepMode) 
		{
			// writeLogData(logMsgData);
			
			if (inverterIsAlife) 			// alternate mode: solar/emon Data to xively (due to packet size limitation)
				{
					if (xivelySentSolarDataState)
					{
						sendEmonDataToXively();
						xivelySentSolarDataState = false;
					}
					else
					{
						sendSolarDataToXively();
						xivelySentSolarDataState = true;
					}
				}
		}	
		
     }
  
  if (someOneIsHere())  
    {
        showDataOnDisplay();
    }

}
//-----------------------------------------------------------------------
// That's all folks
//-----------------------------------------------------------------------
