/* 
----------------------------
  A r d a S o l   Project
----------------------------
*/

#define Version "V8.1"
#define Release "R04"

/*
Version: 8.1
Version Date: 7.1.2015

Get Windchill temperature from ArdaSol Meteo and display it
when not equal to instant temperature

Version: 8.0
Version Date: 24.12.2014

Integration of the real-time clock hardware

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

Weather data is collected from ArdaMeteo weather station and wind generator simulator.
It gets up:
- Average 2 minutes wind speed and direction
- Rain in  day
- Atmospheric pressure
- Humidity and temperature
- Light level
- Power and energy produced by a 1kW wind generator

This data set will be displayed on a Sure Electronics Dot Matrix 32x16 dual color display.
Data Acqusition is done on a SD-Card and Part of Data is sent do the Xively Server on the internet.

The photovoltaic plant is located in Italy, Region Apulia, location San Pietro In Bevagna, next to the famous
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

/* the time library stuff */
#include <Time.h>

/* used for the RTC 1307 clock */
#include <Wire.h>  

#define DS1307_I2C_ADDRESS 0x68  // This is the I2C address
// Arduino version compatibility Pre-Compiler Directives
#if defined(ARDUINO) && ARDUINO >= 100   // Arduino v1.0 and newer
  #define I2C_WRITE Wire.write 
  #define I2C_READ Wire.read
#else                                   // Arduino Prior to v1.0 
  #define I2C_WRITE Wire.send 
  #define I2C_READ Wire.receive
#endif

#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t

#define DataLED  5		// pin nr. 5

#define startATmode "+++"							//AT command for starting AT-mode

#define cmdATDLadrRemote 	"ATDL 409BC011\r"		//XBEE DL Adress of ArdaSol Remote
#define cmdATDLadrEmon 		"ATDL 40A6951E\r"		//XBEE DL Address of ArdaSol Energy Monitor
#define cmdATDLadrMeteo 	"ATDL 409BC005\r"		//XBEE DL Address of ArdaSol Meteo Station

#define cmdATDLadrReserve 	"ATDL 40B3C271\r"		//XBEE DL Address of reserve XBEE

#define exitATmode			"ATCN\r"				//AT command for exit AT-mode

// to know which XBEE address is now active

#define NONE	0
#define REMOTE	1
#define EMON	2
#define METEO	3

byte XBEEAddress = NONE;

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

#define logMsgFlushRecBuf		"E01:FlushRB x="
#define logMsgRecChkSumErr		"E02:RecChkSumErr"
#define logMsgRecTimeout		"E03:RecTimeout"
#define logMsgRecTimeoutRemote	"E04:RecTimeoutRemote"
#define logMsgRecTimeoutEmon	"E05:RecTimeoutEmon"
#define logMsgRecTimeoutMeteo	"E06:RecTimeoutMeteo"

#define logMsgLen 200

char logMessage[logMsgLen];

// Meteo Data definitions

int					MeteoTemp = 0;      // temperature 1/10 of degree Celsius
unsigned int		Humidity = 0;		// rel. humidity in 1/10-percent
int					WindChillTemp = 0;	// windchill temperature 1/10 of degree Celsius

unsigned int    	Baro = 0;			// Atmospheric pressure in hekto Pascal
int    				BaroDeltaDay = 0;	// delta day Atmospheric pressure in Pascal

unsigned int		LightLevel = 0;		// light level 1/10 of Lux

unsigned int		Rain1h = 0;			// in last hour Rain Level in 1/100 of mm
unsigned int		RainDay = 0;		// total day Rain Level in 1/100 of mm

unsigned int		WindSpeed = 0;		// average 2 minutes wind speed in mm/s
unsigned int		WindSpeedkmh = 0;	// average 2 minutes wind speed in 1/10 kmh
unsigned int		WindDir = 0;		// average 2 minutes wind direction in degree

unsigned int		WindGustSpeed = 0;		// day wind gust speed in mm/s
unsigned int		WindGustSpeedkmh = 0;	// day wind gust speed in 1/10 kmh
unsigned int		WindGustDir = 0;		// day wind gust direction in degree

unsigned int		MeteoEnergyWriteCounter = 0;
boolean 			dailyMeteoDataReset = false;

// Generator Type 3 : Horizontal Axis  HY-1000
unsigned int			Gen3Power=0;				// instantaneous generator 3 Power in Watts
unsigned int			Gen3PowerPeakDay=0;			// generator 3 Peak Power in Watts
unsigned int			Gen3Energy=0;				// Total generator 3 Energy in 1/10 kWh
unsigned int			Gen3EnergyDay=0;			// in day generator 3 Energy in 1/10 kWh
unsigned int			Gen3EnergyAverageDay;


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

int					balancePower=0;			// the diffrence between Solarpower and ConsumedPower

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
const unsigned long      startOfPVIAverageYearDate = 1388534400;  // it was the 1.1.2014 

unsigned long            at;      // current time


long                     timeDiff;
boolean                  arduinoClockIsSet;
boolean                  inverterIsAlife;

boolean					 displayToggle=true;

boolean                  sleepMode;

boolean                  SDcardAvailable;        // true when sd card is inserted 

boolean 				internetAvailable;
byte 					internetConnFailCnt;
#define maxConnFail		20

byte					sendDataToInternetState=0;			//controls the internet data acquisition

boolean					displayData=false;					//with this flag a showDataOnDisplay can be signalized

unsigned long            dataAcqTimeStart;
const unsigned long      dataAcqWait = 20000;  //Data Acquisition Rate in ms

unsigned long            lastSummaryDispStart;
const unsigned long      summaryDispWait = 60000;  //60 sec wait at least between summary display

unsigned long				lastPVISleepTime;		// last time PVI went to sleep
unsigned long				lastPVIAwakeTime;		// last time PVI awake


/* RTC memory stuff
-------------------

Memory administration
Base Address = 0
Max Address = 55
total 56 bytes avalable

bytes -> 1 byte
integers -> 2 bytes
longs -> 4 bytes

*/

// long integres:
#define awaTimeRTCAdr				0
#define slpTimeRTCAdr				4
#define EnergyDayRTCAdr				8
#define EnergyConsFromGridWhRTCAdr	12

// integers:

// bytes
#define sleepModeRTCAdr				16

// Last free Location = 17

#define awakeCheckPeriod          5      // minutes interval for request time in the awake process

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
boolean xivelySentMeteoDataState ;

#include <ArdaSolDatAcq.ino>
//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
// RTC Persistent Memory Stuff
// Advantage: No limits in write cycles

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
// Writes a byte to RTC addressed by locationOffset

void writeToRTCByte(byte locOffset, byte data)

{

Wire.beginTransmission(DS1307_I2C_ADDRESS);   

I2C_WRITE(0x08+locOffset); // Set the register pointer to be just past the date/time registers.

I2C_WRITE(data);
delay(10);
	  
Wire.endTransmission();
			  
}

//-----------------------------------------------------------------------
// Reads a Byte From RTC addressed by locationOffset

byte readFromRTCByte(byte locOffset)

{
byte data;

Wire.beginTransmission(DS1307_I2C_ADDRESS);   
                                            
I2C_WRITE(0x08+locOffset); // Set the register pointer to be just past the date/time registers.

Wire.endTransmission();

Wire.requestFrom(DS1307_I2C_ADDRESS, 1);

data=I2C_READ();

return data;
			  
}



//-----------------------------------------------------------------------
// Writes an integer to RTC addressed by locationOffset

void writeToRTCInteger(byte locOffset, int data)

{

Wire.beginTransmission(DS1307_I2C_ADDRESS);   

I2C_WRITE(0x08+locOffset); // Set the register pointer to be just past the date/time registers.

I2C_WRITE(lowByte(data));
delay(10);
	  
I2C_WRITE(highByte(data));
delay(10);
			  
Wire.endTransmission();
			  
}

//-----------------------------------------------------------------------
// Reads an integer From RTC addressed by locationOffset

int readFromRTCInteger(byte locOffset)

{
int data;

Wire.beginTransmission(DS1307_I2C_ADDRESS);   
                                            
I2C_WRITE(0x08+locOffset); // Set the register pointer to be just past the date/time registers.

Wire.endTransmission();

Wire.requestFrom(DS1307_I2C_ADDRESS, 2);

data=I2C_READ();
data=data + 256*I2C_READ();

return data;
			  
}


//-----------------------------------------------------------------------
// Writes a long integer to RTC addressed by locationOffset

void writeToRTCLong(byte locOffset, long data)

{
int hwrd;

hwrd = data >> 16;  // high word of data


Wire.beginTransmission(DS1307_I2C_ADDRESS);   

I2C_WRITE(0x08+locOffset); // Set the register pointer to be just past the date/time registers.

I2C_WRITE(lowByte(data));
delay(10);
	  
I2C_WRITE(highByte(data));
delay(10);

I2C_WRITE(lowByte(hwrd));
delay(10);
	  
I2C_WRITE(highByte(hwrd));
delay(10);
			  
Wire.endTransmission();
			  
}

//-----------------------------------------------------------------------
// Reads a long integer From RTC addressed by locationOffset

long readFromRTCLong(byte locOffset)

{
long data;
long x;

Wire.beginTransmission(DS1307_I2C_ADDRESS);  

I2C_WRITE(0x08+locOffset); // Set the register pointer to be just past the date/time registers.

Wire.endTransmission();

Wire.requestFrom(DS1307_I2C_ADDRESS, 4);

data=I2C_READ();
x=I2C_READ();
data+= x << 8;
x=I2C_READ();
data+= x << 16;
x=I2C_READ();
data+= x << 24;


return data;
			  
}

 

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
// Save Energy Day Values of Generator 3
//-----------------------------------------------------------------------
// Writes the total amount of energy to persistent memory

void writePersistentEnergyDayGen3(int totEWr)

{

EEPROM.write(12, lowByte(totEWr));		// location 12
EEPROM.write(13, highByte(totEWr));		// location 13

}

//-----------------------------------------------------------------------
// reads the total amount of energy from persistent memory

int readPersistentEnergyDayGen3()

{

int x;
int y;

y=EEPROM.read(12);		// location 12
x=EEPROM.read(13);		// location 13
y+= x << 8;

return(y);

}


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

void printDateWdayTime(unsigned long absoluteTime)

  // utility function to print the absolute time in human readable format
{  

Serial.print(day(absoluteTime));
Serial.print('.');
Serial.print(month(absoluteTime));
Serial.print('.');
Serial.print(year(absoluteTime));
Serial.print(',');
Serial.print(weekday(absoluteTime));
Serial.print(',');
Serial.print(hour(absoluteTime));
Serial.print(':');
Serial.print(minute(absoluteTime));
Serial.print(':');
Serial.print(second(absoluteTime));
Serial.println();

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
      if ((at > (lastPVIAwakeTime + 82800)) && ((minute(at) % awakeCheckPeriod) == 0))  //last awake time plus 23 hours every 5 minutes make a check
           {
              getTime();
              atatempt = DateAndTime + timeOffset1970to2000;
              if (year(atatempt) > 2012)
                {
                  sleepMode = false;
				  writeToRTCByte(sleepModeRTCAdr,sleepMode);
				   
                  sleepAttempts = 0;  
                  lastPVIAwakeTime = now();
				  writeToRTCLong(awaTimeRTCAdr,lastPVIAwakeTime);
				  
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
				  
				  EnergyConsFromGridWh = 0;
				  writeToRTCLong(EnergyConsFromGridWhRTCAdr,EnergyConsFromGridWh);
				  
				  OldEmonEnergyDayWh=0;
				  OldEnergyDayWh=0;
				  sendNewDayDataToXively();

				  writePersistentEnergyDayGen3(Gen3Energy);
				  dailyMeteoDataReset=true;
				  Gen3PowerPeakDay=0;
				  Gen3EnergyDay=0;				  
				  
                }
            //else ;
           }
    } // end of then (sleepMode)
  else                      // we are awake, goto sleep?
    {
      if ((at > (lastPVISleepTime + 82800)) && (sleepAttempts > maxSleepAttempts))  // after 23 hours we check max 10 times if inverter is alive
        {
          sleepAttempts=0;
		  
          sleepMode = true;  //go to sleep allowed only at a specific time window
		  writeToRTCByte(sleepModeRTCAdr,sleepMode);
		  
          lastPVISleepTime = now();
		  writeToRTCLong(slpTimeRTCAdr,lastPVISleepTime);
		  
          writeLogData(logMsgGotoSleep);
		  writePersistentEnergyTotal(EnergyTotal);
          sendSleepDataToXively();  
        }
      
      else 
        {
          if (at > (lastPVISleepTime + 82800)) sleepAttempts++;
        }
            
    } // end of else (sleepmode)
  
}

//-----------------------------------------------------------------------
// manage all time stuff

void manageTimeStuff()
{
    
DateAndTime = 0;
    
if(setXBEEdestadr(cmdATDLadrRemote))   // set XBEE address to ArdaSol Remote
{	
    XBEEAddress=REMOTE;
	digitalWrite(DataLED, HIGH);  //Turns Data LED On

	if (!sleepMode) getTime();
    
    at = DateAndTime + timeOffset1970to2000;

    if (year(at) > 2012) 
        {
          inverterIsAlife = true;
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
				   RTC.set(at);	   // adjust also the realtime clock
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
				RTC.set(at);	   // adjust also the realtime clock
                writeLogData(logMsgTimeSync);
                arduinoClockIsSet = true;
              }
       } //end of else (arduinoClockIsSet)

	digitalWrite(DataLED, LOW);  //Turns Data LED Off
}

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
  
       //digitalWrite(DataLED, LOW);  //Turns Data LED Off
		
	   // GridPowerPeakDay = 0;
       getGridPowerPeakDay();
	   
	   //digitalWrite(DataLED, HIGH);  //Turns Data LED On
  
       // EnergyDay = 0;
       getEnergyDay();
	   
	   digitalWrite(DataLED, LOW);  //Turns Data LED Off
  
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
 else if ((arduinoClockIsSet) && (sleepMode) && (GridPower > 0 )) 		// always after inverter goes to sleep
 
     {
       GridPower = 0;
     }
 else if ((arduinoClockIsSet) && (sleepMode) &&((GridPowerPeakDay > 0) || (EnergyDay > 0)) && (hour(at) == 0)) 	// always at midnight
     {
       GridPower = 0;
       GridPowerPeakDay = 0;
       EnergyDay = 0;
	   writeToRTCLong(EnergyDayRTCAdr,EnergyDay);
	   
     }  
}

//-----------------------------------------------------------------------
// Get all Data from ArdaSol Energy Monitor

void getEmonEnergyData()
{
	
	if (setXBEEdestadr(cmdATDLadrEmon))		// set XBEE address to ArdaSol Energy Monitor
	{
		XBEEAddress=EMON;
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
    


}

//-----------------------------------------------------------------------
// Get all Data from ArdaMeteo Weather Station

void getMeteoData()
{
	float f;
	
	if (setXBEEdestadr(cmdATDLadrMeteo))		// set XBEE address to ArdaSol Meteo Station
	{
		 XBEEAddress=METEO;
		 digitalWrite(DataLED, HIGH);  //Turns Data LED On
		
		if (dailyMeteoDataReset) 
			{
				//Serial.print("MeteoReset:");
				getMeteoDayResetConfirmation();
				//if (dailyMeteoDataReset) Serial.print("Not ");
				//Serial.println("Done");
			}
	
		//WindSpeed = 0;
		//WindDir = 0;
		getMeteoAvr2MinWind();
	
		f = ((float)WindSpeed * 36) / 1000;   // Zehntel kmh
		WindSpeedkmh = (int)f;
	
		//Serial.print("Wind mm/s,km/h,DEG:");
		//Serial.print(WindSpeed);
		//Serial.print("/");
		//Serial.print(WindSpeedkmh,1);
		//Serial.print("/");
		//Serial.println(WindDir);
	
		//WindGustSpeed = 0;
		//WindGustDir = 0;
		getMeteoDayWindGust();
	
		f = ((float)WindGustSpeed * 36) / 1000;   // Zehntel kmh
		WindGustSpeedkmh = (int)f;
	
		//Serial.print("Gust mm/s,km/h,DEG:");
		//Serial.print(WindGustSpeed);
		//Serial.print("/");
		//Serial.print(WindGustSpeedkmh,1);
		//Serial.print("/");
		//Serial.println(WindGustDir);
	   
		//Gen3Power = 0;
		//Gen3Energy = 0;
		// Gen3EnergyDay=0;
		getWindGen3();		//gets the power and energy produced of a virtual 1kW wind generator
		if ((Gen3Energy > 0) && (Gen3Energy > readPersistentEnergyDayGen3())) Gen3EnergyDay = Gen3Energy - readPersistentEnergyDayGen3();
		if ((Gen3Power > Gen3PowerPeakDay) && (Gen3Power < 1500)) Gen3PowerPeakDay=Gen3Power;
		calculateAverageDayGen3Energy();
	
		//Serial.print("W,kWhTenth:");
		//Serial.print(Gen3Power);
		//Serial.print("/");
		//Serial.println(Gen3Energy);
	
		//Rain1h = 0;
		//RainDay = 0;
		getMeteoRain();
	
		//Serial.print("R1h,Rday:");
		//Serial.print(Rain1h);
		//Serial.print("/");
		//Serial.println(RainDay);
	
		//Baro = 0;
		//BaroDeltaDay = 0;
		getMeteoBaro();
	
		//Serial.print("BaroInst,Delta:");
		//Serial.print(Baro);
		//Serial.print("/");
		//Serial.println(BaroDeltaDay);
	
		//MeteoTemp = 0;
		//Humidity = 0;
		getMeteoHumidity();
	
		//Serial.print("Temp,Hum:");
		//Serial.print(MeteoTemp);
		//Serial.print("/");
		//Serial.println(Humidity);
		
		//WindChillTemp = 0;
		getWindChillTemperature();
	
		//Serial.print("WindChillTemp:");
		//Serial.println(WindChillTemp);
	
	
		//LightLevel = 0;
		getMeteoLight();
	
		//Serial.print("Light:");
		//Serial.println(LightLevel);
	
	
		//Serial.println("****");
		
		digitalWrite(DataLED, LOW);  //Turns Data LED Off
	}
	
	
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

//Serial.print("ptr=");
//Serial.print(oTVpointer);

//Serial.print(" avr=");
//Serial.print(x);

y = (x - c1) * c2 * 10; //zehntelgrad deshalb mal 10
ambientTempAvrOneTenthClesius = y/c3;
if ((y % c3) > 511) ambientTempAvrOneTenthClesius++;

//Serial.print(" ATavr=");
//Serial.println(ambientTempAvrOneTenthClesius);


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
// calculates to averag value of energy day since start date 14.7.2014 = 195
// 14.7.2014 bis 31.12.2014 = 170 Tage

void calculateAverageDayGen3Energy()
{
unsigned int daysInOperation;
unsigned long wattHours;

if ((year(at) > 2012) && (Gen3Energy > 0))
    {
	  wattHours = long(Gen3Energy) * 100;		//long must be set otherwise range overflow
      // daysInOperation = ((at - readPersistentStartDateEnergyTotal() ) / 86400) + 1 - 195;
	  daysInOperation = ((at - readPersistentStartDateEnergyTotal() ) / 86400) + 1 + 170;
	  // daysInOperation = constrain(daysInOperation,1,366);	// only one year
      Gen3EnergyAverageDay = wattHours / daysInOperation;
    }
else Gen3EnergyAverageDay = 0;


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

if ((deltaEmon - deltaSolar) > 0) 
		{
			EnergyConsFromGridWh = EnergyConsFromGridWh + (deltaEmon - deltaSolar);
			writeToRTCLong(EnergyConsFromGridWhRTCAdr,EnergyConsFromGridWh);
		}

balancePower = int(GridPower) - int(GridEmonPower);    // Solarpower - Consumed Power

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
  cx = snprintf (logMessage+i, logMsgLen-i, "Msg;");   // 5.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Atime;");   // 6.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Pday;");   // 7.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Ppk;");   // 8.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Eday;");   // 9.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Etot;");   // 10.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Eavr;");   // 11.
  
  i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Ugr;");   // 12.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Pem;");   // 13.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Uem;");   // 14.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "EemD;");   // 15.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "EemT;");   // 16.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "EgrdD;");   // 17.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Mtemp;");   // 18.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Mhum;");   // 19.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Mbaro;");   // 20.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "MbaroD;");   // 21.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "Mrain;");   // 22.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "MWsp;");   // 23.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "MWdir;");   // 24.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "MWGsp;");   // 25.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "MWGdir;");   // 26.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "MGPday;");   // 27.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "MGPpk;");   // 28.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "MGEday;");   // 29.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "MGEtot;");   // 30.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "MGEavg;");   // 31.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "MWCtemp;");   // 32.
  
   i = i+cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "\r\n");   // 33.
  
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
  cx = snprintf (logMessage+i, logMsgLen-i, "%s;",msgID);	 // 5. message id
 
  i = i +cx;
  cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",at);	 // 6. Arduino absolute time based on 1.1.1970
 
  
  if (msgID[0] == 'D')
     {
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",GridPower);	 // 7.  Instantaneous power to grid
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",GridPowerPeakDay);	 //  8. Power peak sent to grid
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",EnergyDay);	 //  9. Energy produced this day Wh
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",EnergyTotal);	 // 10. Total energy produced since start Wh
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",EnergyAverageDay);	 // 11. Average energy a day
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;",GridVoltage);	 // 12. Grid Voltage
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;",GridEmonPower);	 // 13. Emon Power
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;",GridEmonVoltage);	 // 14. Emon Voltage
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;", EmonEnergyDayWh);	 // 15. Emon Energy day in Wh
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;", EmonEnergyTotalWh);	 // 16. Emon Energy Tot in Wh
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%ld;", EnergyConsFromGridWh);	 // 17. Energy taken from Grid a day
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;", MeteoTemp);	 //18. 
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%u;", Humidity);	 // 19. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%u;", Baro);	 // 20. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;", BaroDeltaDay);	 // 21. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%u;", RainDay);	 // 22. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%u;", WindSpeedkmh);	 // 23. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%u;", WindDir);	 // 24. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%u;", WindGustSpeedkmh);	 // 25. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%u;", WindGustDir);	 // 26. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;", Gen3Power);	 // 27. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;", Gen3PowerPeakDay);	 // 28. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;", Gen3EnergyDay);	 // 29. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;", Gen3Energy);	 // 30. 
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;", Gen3EnergyAverageDay);	 // 31. 
	
	i = i +cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "%d;", WindChillTemp);	 //32. 
	
	i = i+cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "\r\n");   // 33.
	
     }
   else
     {
	
    i = i+cx;
    cx = snprintf (logMessage+i, logMsgLen-i, ";;;;;;;;;;;;;;;;;;;;;;;;;;");   // 7. - 32.	
	i = i+cx;
    cx = snprintf (logMessage+i, logMsgLen-i, "\r\n");   // 33.
	
     }

 }
 
Serial.print(logMessage);

//test
//Serial.print("snprintfLEN=");
//Serial.println(i+cx);

if (getLogFileNameFromDate()) WriteDataRecordToSD();

}
//-----------------------------------------------------------------------
// Shows the data on the dot matrix display

void showDataOnDisplay()

{
  if (arduinoClockIsSet)
    {
		writeLogData(logMsgShowData);

//start test
/*
int tdata[] = { 0, -1234, -456, -89, -5, 9, 45, 999, 2500, 0 }; // 10 elements
for (byte i=0; i < 10; i++)
{
balancePower = tdata[i];
displayBalancePower(balancePower);
delay(DISPLAYTIME);
clearDisplay();
}			
*/
//ende test

	  
		// if ((millis() - lastSummaryDispStart) > summaryDispWait)  
		if (false)	 
		
		{	/*
			lastSummaryDispStart = millis();  //remember when started summary display
			
			displayTempAndTime(ambientTemperature,hour(at),minute(at));
			delay(DISPLAYTIME);
			clearDisplay();
			
			displayBalancePower(balancePower);
			delay(DISPLAYTIME);
			clearDisplay();
				
			
			if (EnergyDay > 0)
			{	displayEnergyDayPro(EnergyDay);
				delay(DISPLAYTIME);
				clearDisplay();
			}
			
			displayMeteoTempHum(MeteoTemp,Humidity);
			delay(DISPLAYTIME);
			clearDisplay();
			
	  
			displayMeteoBaro(Baro,BaroDeltaDay);
			delay(DISPLAYTIME);
			clearDisplay();
			
			if (RainDay > 0)
			{
				displayMeteoRain(RainDay);
				delay(DISPLAYTIME);
				clearDisplay();
			}
	  
			displayMeteoWind(WindSpeedkmh,WindDir);
			delay(DISPLAYTIME);
			clearDisplay();
				
			displayEnergyDayPro((Gen3EnergyDay*100));
			delay(DISPLAYTIME);
			clearDisplay();	
			*/
		}
		else // display details alternating energy / meteo
		{
			lastSummaryDispStart = millis();  //force detail display in next time
			
			if(displayToggle==true)  //display energy data
			{  	  
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
	  
				displayToggle=!displayToggle;
			}
			else 	// display meteo data
			{
      
				displayWeekdayAndDate(weekday(at),day(at),month(at));
				delay(DISPLAYTIME);
				clearDisplay();
	  
				displayMeteoTempHum(MeteoTemp,Humidity);
				delay(DISPLAYTIME);
				clearDisplay();
				
				int t1 = MeteoTemp/10;
				if ((MeteoTemp % 10) >4) t1++;
				
				int t2 = WindChillTemp/10;
				if ((WindChillTemp % 10) >4) t2++;
				
				if (t2 < t1)
				{
					displayWindChillTemp(WindChillTemp);
					delay(DISPLAYTIME);
					clearDisplay();
				}
				
				displayMeteoBaro(Baro,BaroDeltaDay);
				delay(DISPLAYTIME);
				clearDisplay();
	  
				if (RainDay > 49)		// display at least if its 1mm of rain
				{
					displayMeteoRain(RainDay);
					delay(DISPLAYTIME);
					clearDisplay();
				}
	  
				displayMeteoWind(WindSpeedkmh,WindDir);
				delay(DISPLAYTIME);
				clearDisplay();
	  
				displayMeteoWindGust(WindGustSpeedkmh);
				delay(DISPLAYTIME);
				clearDisplay();
	  	  
				displayPowerDayPro(Gen3Power);
				delay(DISPLAYTIME);
				clearDisplay();
	  
				displayPowerDayPeakPro(Gen3PowerPeakDay);
				delay(DISPLAYTIME);
				clearDisplay();
	  
				displayEnergyDayPro((Gen3EnergyDay*100));
				delay(DISPLAYTIME);
				clearDisplay();
	  
				displayEnergyTotalPro((Gen3Energy/10));
				delay(DISPLAYTIME);
				clearDisplay();
	  
				displayEnergyAverageDayPro(Gen3EnergyAverageDay);
				delay(DISPLAYTIME);
				clearDisplay();
	  
				displayToggle=!displayToggle;
			}
		}


//Ende test  

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
// Send data part to Xively Cloud Data Service

void SendDataToInternet()

{

switch (sendDataToInternetState)
{
	case 0:			// Send Solar Date
	
		sendSolarDataToXively();
	
	break;
	
	case 1:			// Send Energy Monitor Data
	
		sendEmonDataToXively();
		
	break;			
	
	case 2:			// Send Meteo Data
	
		sendMeteoPart1DataToXively();
			
	break;
	
	case 3:			// Send Meteo Data
	
		sendMeteoPart2DataToXively();
	
	break;
}

sendDataToInternetState = ++sendDataToInternetState % 4;

}

//-----------------------------------------------------------------------
// Checks the IR Sensor has been activated, if yes sets the display flag
// and waits waitMillis until return

void checkIRsensor(int waitMillis)

{
long waitStartTime;

if (someOneIsHere()) displayData = true;  //check the state if the IR sensor

if (waitMillis > 0)
{
	waitMillis = waitMillis % 5000;  //we allow max 5s of wait time
	waitStartTime = millis();

	while ((millis() - waitStartTime) < waitMillis)  
		{
		if (someOneIsHere()) displayData = true;  //check the state if the IR sensor
		}

}

}



//-----------------------------------------------------------------------
// Shows the data on Display when IR Sensor has been activated
// if nothing to dislay it waits waitMillis until return

void checkShowDataOnDisplay(int waitMillis)

{
long waitStartTime;

if (waitMillis > 0)
{
	waitMillis = waitMillis % 5000;  //we allow max 5s of wait time
	waitStartTime = millis();

	while ((millis() - waitStartTime) < waitMillis)  
		{
		if (someOneIsHere() || displayData)  
			{
			showDataOnDisplay();
			displayData = false;
			}
		}
}
else
{
if (someOneIsHere() || displayData)  
		{
		showDataOnDisplay();
		displayData = false;
		}
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
  
  Serial.println("ArdaSolDisplay ");
  Serial.println(Version);
  Serial.println(Release);
  
// **************************************************************
// Init value has to be written only once or for corrections
// **************************************************************
// Switched OFF

// writePersistentEnergyTotal(4032000);          //setup dated 5.12.2014
// writePersistentEmonEnergyTotal(2384000);     // setup dated 5.12.2014

// writePersistentEnergyDayGen3(800);		//setup 10.10.2014 virtual generator 3

// Set start date of statistical year, normaly set automaticly
// writePersistentStartDateEnergyTotal(startOfPVIAverageYearDate);  //start of PVI Statistical Year

// Switched OFF
// **************************************************************
// Init value has to be written only once or for corrections
// **************************************************************
  
  Serial.println("EEPROM persistence Data:");
  
  //Gen3Energy = 0;
  Gen3Energy=readPersistentEnergyDayGen3();
  Serial.print("Pers_kWhTenth Gen3=");
  Serial.println(Gen3Energy);
  
  //EnergyTotal = 0;
  EnergyTotal=readPersistentEnergyTotal();
  
  EnergyTotalkWh = EnergyTotal / 1000;
  if ((EnergyTotal % 1000) > 499)   ++EnergyTotalkWh;  // round up	
  
  Serial.print("Pers_Wh PVI=");
  Serial.println(EnergyTotal);
  
  // EmonEnergyTotalWh = 0;
  EmonEnergyTotalWh=readPersistentEmonEnergyTotal();
  Serial.print("Pers_Wh Emon=");
  Serial.println(EmonEnergyTotalWh);
   
  Serial.print("StatStartDate=");
  Serial.print((unsigned long) readPersistentStartDateEnergyTotal());
  Serial.print(' ');
  printDateWdayTime((unsigned long) readPersistentStartDateEnergyTotal());
  
  resetEnergyDayUsed=false; //flag for signalling reset daily energy counter in emon
  dailyMeteoDataReset=false; // flag for signalling a daily data reset to meteostation

  
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
  
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
							  // every 5 Minutes system time is synchronized with RTC
  if(timeStatus()!= timeSet) 
     {
	 Serial.println("Unable to sync with the RTC");
	 arduinoClockIsSet = false;
	 }
  else
	 {
		Serial.println("RTC has set the ArdaSol System Time:");
		Serial.print("Absolute Time=");
		Serial.print((unsigned long) now());
		Serial.print(' ');
		printDateWdayTime((unsigned long) now());
		
		arduinoClockIsSet = true;
		sleepMode = readFromRTCByte(sleepModeRTCAdr);
		lastPVISleepTime = readFromRTCLong(slpTimeRTCAdr);
		lastPVIAwakeTime = readFromRTCLong(awaTimeRTCAdr);
		EnergyConsFromGridWh = readFromRTCLong(EnergyConsFromGridWhRTCAdr);
		EnergyDay = readFromRTCLong(EnergyDayRTCAdr);
		
	 }
   
  Serial.println("RTC persistence Data:");
  Serial.print("SleepMode=");
  Serial.println(sleepMode); 
  
  Serial.print("lastPVISleepTime=");
  Serial.print(lastPVISleepTime);
  Serial.print(' ');
  printDateWdayTime(lastPVISleepTime);
  
  Serial.print("lastPVIAwakeTime=");
  Serial.print(lastPVIAwakeTime);
  Serial.print(' ');
  printDateWdayTime(lastPVIAwakeTime);
  
  Serial.print("EnergyConsFromGridWh=");
  Serial.println(EnergyConsFromGridWh);
  
  Serial.print("EnergyDay=");
  Serial.println(EnergyDay);
  
   
  sleepAttempts = 0;
   
  dataAcqTimeStart = millis();  //remember when started data acq
  lastSummaryDispStart = millis();  //wait summary display when started
  	
  manageTimeStuff();
  getEnergyData();
  
  delay(1100);     //XBEE AT command wait
  getEmonEnergyData();
  
  getAmbientTemperature();
  calculateAverageDayEnergy();
  
  OldEmonEnergyDayWh = EmonEnergyDayWh;
  OldEnergyDayWh = EnergyDay; 
  
  calculateEnergyBalance();
  
  if (SDcardAvailable) writeLogData(logMsgSDcard);
  else writeLogData(logMsgNoSDcard);
  
  if (internetAvailable) writeLogData(logMsgIntConn);
  else writeLogData(logMsgNoIntConn);
  
  writeLogData(logMsgFieldDefs);
  
  writeLogData(logMsgStart);
  
  xivelySentSolarDataState = false;
  xivelySentMeteoDataState = false;
  
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
	
		checkIRsensor(1100);			//check max 1100 ms if a person is in front of the display
										//waiting is necessary between last communication and address change in XBEE's
		getEmonEnergyData();	
		getAmbientTemperature();
		calculateAverageDayEnergy();
		calculateEnergyBalance();
	
		checkShowDataOnDisplay(1100);
	
		getMeteoData();
	
		checkShowDataOnDisplay(1100);
	
		SendDataToInternet();
	
		checkShowDataOnDisplay(1100);

		writeLogData(logMsgData);
	}
	
checkShowDataOnDisplay(0);

}
//-----------------------------------------------------------------------
// That's all folks
//-----------------------------------------------------------------------
