/* 
---------------------------------------
ArdaMeteo Meteo Data Acqusition
---------------------------------------
*/

#define Version "V2.0 "
#define Release "R2"

/*
Version: 2.0
Creation Date: 4.1.2015
Author: Heinz Pieren
Email: heinz.pieren@semprevacanze.it

Wind-Chill Table for temperature inserted
Comm protocol expanded with wind chill temp

Version: 1.0
Creation Date: 1.7.2014
Author: Heinz Pieren
Email: heinz.pieren@semprevacanze.it
-------------------------------------------------------------
ArdaMeteo Meteo Data Acquisition and Wind-generator Simulator
-------------------------------------------------------------

- Weather Data Collection with Sparkfun Weather Shield collected with Wind- and Rain meter
- Following Weather Data is sampled:
	* Wind Direction
	* Wind Speed
	* Rain Gauge
	* Barometric Pressure
	* Relative Humidity
	* Luminosity
	* Temperature
- Raw data is processed by ArdaMeteo calculating some average values
	* Wind Gust/Dir peak in the day
	* Wind Speed/Dir average over last 2 minutes
	* Wind Gust/Dir average over last 10 minutes
	* Rain over last hour
	* Rain total in the day
- Sensor data is collected every 1 second
- Wind generator simulation with 3 different types, power and energy measurement
- Send data by XBEE to ArdaSol Data Management Centre
- Receive reset stat. data from ArdaSol

- ArdaMeteo Software uses some parts of
  Weather Shield Example
  By: Nathan Seidle
  SparkFun Electronics
  Date: November 16th, 2013
  
*/

#include <Wire.h> //I2C needed for sensors
#include "MPL3115A2.h" //Pressure sensor
#include "HTU21D.h" //Humidity sensor

#include <SoftwareSerial.h>

MPL3115A2 myPressure; //Create an instance of the pressure sensor
HTU21D myHumidity; //Create an instance of the humidity sensor


//ArdaMeteo Log messages 

#define logMessageStart            "*01:start"

#define logMessageFlushXBeeRecBuf    "*E1:FlushXBeeRB x="
#define logMessageXBeeRecChkSumErr   "*E2:XBeeChkSumErr"
#define logMessageXBeeRecTimeout     "*E3:XBeeRecTimeout"

//Hardware pin definitions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// digital I/O pins
const byte WSPEED = 3;
const byte RAIN = 2;
const byte STAT1 = 7;
const byte STAT2 = 8;

const byte rxXBee = 9;      // arduino input
const byte txXBee = 10;      // arduino output
const byte rtsXBee = 11;     // arduino output

SoftwareSerial XBeeSerial(rxXBee, txXBee); //Create an instance of the XBeeSerial communication driver

// analog I/O pins
const byte REFERENCE_3V3 = A3;
const byte LIGHT = A1;
const byte BATT = A2;
const byte WDIR = A0;


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

Example: Get Wind Speed
03 3B 01 00 00 00 00 00 CRCL CRCH
                        --------- CRC
    ----- Get Wind instantaneous Speed and Dir command 3B 01
-- ArdaMeteo address = 3

Meteo response 8 byte data packet:
1 State	
2 MState
3 Param1
4 Param2
5 Param3
6 Param4
7 crc low
8 crc high

Example: Response to above request
00 06 0F A0 00 B4 CRCL CRCH
		          --------- CRC
            ---- instantaneous Direction 180 degree -> Sud
	   ---- instantaneous Speed in mm/s = 4000 -> 14,4 km/h
    -- MState = 6 ? same as Aurora PVI
-- State = 0 ?  same as Aurora PVI

*/

#define meteoadr            4           // adress of meteo station

#define cmdInstWindData	  			0x3B01		//instantaneous Wind Speed and Direction
#define cmdAvr2MinWindData	  		0x3B02		//average last 2 minutes Wind Speed and Direction
#define cmdPast10MinWindGustData	0x3B03		// past 10 minutes Wind Gust Speed and Direction
#define cmdDayWindGustData	  		0x3B04		//peak in day Wind Gust Speed and Direction
#define cmdRainData			  		0x3B05		//in last hour and total day
#define cmdAtmosPressureData		0x3B06		//instantaneous pressure
#define cmdLightLevelData			0x3B07		//instantaneous illumination level
#define cmdTempHumidityData			0x3B08		//instantaneous temperature and rel. humidity
#define cmdWindChillTempData		0x3B09		//average 2 minutes wind chill temperature

#define cmdWindGen1Data				0x3B10		//simulation of wind generator Type 1 (400W), inst Power Watt daily Energy 1/10 kWh
#define cmdWindGen2Data				0x3B11		//simulation of wind generator Type 2 (600W), inst Power Watt daily Energy 1/10 kWh
#define cmdWindGen3Data				0x3B12		//simulation of wind generator Type 3 (1000W), inst Power Watt daily Energy 1/10 kWh

#define cmdResetMeteoValues 		0x3BF0		// resets meto values, daily Energy counters and timers

//----------------------------------------------------------------------------------
/* The wind-chill table for look wind-chill temeprature in function of wind speed */
//-----------------------------------------------------------------------------------

/* measured Temp:		15°C,	14°C,	13°C,	12°C,	11°C,	10°C,	9°C,	8°C,	7°C,	6°C,	5°C,	4°C,	3°C,	2°C,	1°C,	0°C,	-1°C,	-2°C,	-3°C,	-4°C,	-5°C,	*/			
/* index:		0	1	2	3	4	5	6	7	8	9	10	11	12	13	14	15	16	17	18	19	20	*/			
const int WCT[12][21] = {  																										
	{	15,	14,	13,	11,	10,	9,	8,	7,	6,	5,	4,	2,	1,	0,	-1,	-2,	-3,	-4,	-6,	-7,	-8,	},	/* Index: 0 WChillTemp für Winspeed: 5 km/h */		
	{	14,	13,	12,	10,	9,	8,	7,	6,	4,	3,	2,	1,	0,	-2,	-3,	-4,	-5,	-6,	-7,	-9,	-10,	},	/* Index: 1 WChillTemp für Winspeed: 10 km/h */		
	{	13,	12,	11,	10,	9,	7,	6,	5,	4,	2,	1,	0,	-1,	-3,	-4,	-5,	-6,	-7,	-9,	-10,	-11,	},	/* Index: 2 WChillTemp für Winspeed: 15 km/h */		
	{	13,	12,	11,	9,	8,	7,	5,	4,	3,	2,	0,	-1,	-2,	-3,	-5,	-6,	-7,	-8,	-10,	-11,	-12,	},	/* Index: 3 WChillTemp für Winspeed: 20 km/h */		
	{	13,	11,	10,	9,	8,	6,	5,	4,	2,	1,	0,	-1,	-3,	-4,	-5,	-7,	-8,	-9,	-10,	-12,	-13,	},	/* Index: 4 WChillTemp für Winspeed: 25 km/h */		
	{	12,	11,	10,	9,	7,	6,	5,	3,	2,	1,	-1,	-2,	-3,	-5,	-6,	-7,	-8,	-10,	-11,	-12,	-14,	},	/* Index: 5 WChillTemp für Winspeed: 30 km/h */		
	{	12,	11,	10,	8,	7,	6,	4,	3,	2,	0,	-1,	-2,	-4,	-5,	-6,	-8,	-9,	-10,	-12,	-13,	-14,	},	/* Index: 6 WChillTemp für Winspeed: 35 km/h */		
	{	12,	11,	9,	8,	7,	5,	4,	3,	1,	0,	-1,	-3,	-4,	-5,	-7,	-8,	-9,	-11,	-12,	-13,	-15,	},	/* Index: 7 WChillTemp für Winspeed: 40 km/h */		
	{	12,	10,	9,	8,	6,	5,	4,	2,	1,	0,	-2,	-3,	-4,	-6,	-7,	-8,	-10,	-11,	-13,	-14,	-15,	},	/* Index: 8 WChillTemp für Winspeed: 45 km/h */		
	{	12,	10,	9,	8,	6,	5,	3,	2,	1,	-1,	-2,	-3,	-5,	-6,	-7,	-9,	-10,	-12,	-13,	-14,	-16,	},	/* Index: 9 WChillTemp für Winspeed: 50 km/h */		
	{	11,	10,	9,	7,	6,	5,	3,	2,	0,	-1,	-2,	-4,	-5,	-6,	-8,	-9,	-11,	-12,	-13,	-15,	-16,	},	/* Index: 10 WChillTemp für Winspeed: 55 km/h */		
	{	11,	10,	9,	7,	6,	4,	3,	2,	0,	-1,	-3,	-4,	-5,	-7,	-8,	-9,	-11,	-12,	-14,	-15,	-16,	},	/* Index: 11 WChillTemp für Winspeed: 60 km/h */		
	};																									



//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//Global Variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// communication stuff

#define cmdSize 10    // 10 Bytes command Packet
static byte CommandBuf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // 10 elements

#define rspSize 8    // 8 Bytes response Packet
static byte ResponseBuf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // 8 elements


unsigned long            receiveTimeoutStart;
const unsigned int       receiveMaxXBeeWait = 1000;  //one second max getting a command

unsigned long            sequenceCounter =0;

unsigned int             crc16Checksum;


// timers and counters

unsigned long            getAllMeteoDataLastStart;
unsigned long            energyCalcLastStart;

const unsigned long      getAllMeteoDataWait = 5000;  // Meteo measure Rate in md

unsigned long			 dxCalcTime;				// evective measuring intervall

const unsigned long      energyMeasureRate = 1000;  	//Energy measurement rate in ms

unsigned long            energyMeasureStoreWait;	//timer for store energy day in EEPROM
const unsigned long      energyMeasureStoreRate = 15*60000;  //every 15 minutes store energy in non volatile memory
															// when:
const int 				 deltaStoreValue = 5;   			// at least 0.5kWh (=5 kWh-Zehntel) must be cumulated for storing in EEPROM
unsigned int			 writePersistentCounter = 0;		// counts the write cycles


byte 					fiveSeconds_2m; 		//Keeps track of the "wind speed/dir avg" over last 2 minutes array of data
byte 					minutes; 				//Keeps track of where we are in various arrays of data
byte 					minutes_10m; 			//Keeps track of where we are in wind gust/dir over last 10 minutes array of data


//We need to keep track of the following variables:
//Wind speed/dir each update
//Wind gust/dir over the day
//Wind speed/dir, avg over 2 minutes (store 5 per second)
//Wind gust/dir over last 10 minutes (store 5 per minute)
//Rain over the past hour (store 1 per minute)
//Total rain over date (store one per day)

unsigned int 	windspdavg[24];	 //24 words to keep track of 2 minute average
unsigned int 	winddiravg[24];  //24 words to keep track of 2 minute average

unsigned int 	windgust_10m[10]; //10 floats to keep track of 10 minute max
unsigned int	windgustdirection_10m[10]; //10 ints to keep track of 10 minute max



float batt_lvl = 11.8; //[analog value from 0 to 1023]


// volatiles are subject to modification by IRQs
volatile unsigned long 	raintime, rainlast, raininterval, rain;
volatile unsigned long 	dailyrain = 0; 	// [rain 1/100 mm so far today in local time]
volatile unsigned long 	rainHour[60]; 	//60 long int numbers to keep track of 60 minutes of rain

volatile unsigned long 	lastWindIRQ = 0;
volatile unsigned int 	windClicks = 0;


// transmitted meteo data

unsigned int			instHumidityPtenth=0;		// rel. humidity in 1/10-percent

int						instTempDEGtenth=0;			// temperature 1/10 of degree Celsius
int						avr2MinWindChillTempDEGtenth=0;			// average 2 min wind chill temperature 1/10 of degree Celsius

unsigned int			instLightLUXtenth=0;		// light level 1/10 of Lux

unsigned long			atmosPressurePa=0;			// Atmospheric pressure in Pascal
unsigned long			atmosStartDayPressurePa=0;	// start day Atmospheric pressure in Pascal
float					atmosStartDayPressure;

unsigned int			atmosPressureHectoPa=0;		// Atmospheric pressure in Hecto-Pascal
int						atmosDeltaDayPressurePa=0;	// delta in day Atmospheric pressure in Pascal
int						atmosPressureTriggerLevel=60;		// variable trigger level for rounding up +60 and down -40

unsigned int			totDayRainMMhun=0;			// total day Rain Level in 1/100 of mm
unsigned int			past1HRainMMhun=0;			// in last hour Rain Level in 1/100 of mm

unsigned int			dayWGustMMpS=0;				// in day peak wind gust in mm/s
unsigned int			dayWDirDEG=0;				// wind direction of this wind gust in degree

unsigned int			past10MinWGustMMpS=0;		// in last 10 minutes peak wind gust in mm/s
unsigned int			past10MinWDirDEG=0;			// wind direction of this wind gust in degree

unsigned int			avr2MinWSpeedMMpS=0;		// average 2 minutes wind speed in mm/s
float					avr2MinWSpeedKMpH=0;		// average 2 minutes wind speed in km/h
unsigned int			avr2MinWDirDEG=0;			// average 2 minutes wind direction in degree

unsigned int			instWSpeedMMpS=0;			// instantaneous wind speed in mm/s
unsigned int			instWDirDEG=0;				// instantaneous wind direction in degree

// virtual wind generators
// Generator Type 1 : Vertikal Axis TIMAR 400W
unsigned int			gen1PowerW=0;				// instantaneous generator 1 Power in Watts
unsigned int			gen1dayEnergyKWHtenth=0;	// in day generator 1 Energy in 1/10 kWh
unsigned long			gen1dayEnergyWs=0;			// in day generator 1 Energy in W/s

// Generator Type 2 : Horizontal Axis  Black 600W
unsigned int			gen2PowerW=0;				// instantaneous generator 2 Power in Watts
unsigned int			gen2dayEnergyKWHtenth=0;	// in day generator 2 Energy in 1/10 kWh
unsigned long			gen2dayEnergyWs=0;			// in day generator 1 Energy in W/s

// Generator Type 3 : Horizontal Axis  HY-1000
unsigned int			gen3PowerW=0;				// instantaneous generator 3 Power in Watts
unsigned int			gen3dayEnergyKWHtenth=0;	// in day generator 3 Energy in 1/10 kWh
unsigned long			gen3dayEnergyWs=0;			// in day generator 1 Energy in W/s




//-----------------------------------------------------------------------
// EEPROM Stuff

#include <EEPROM.h>

//-----------------------------------------------------------------------
// Save Energy Day Values of Generator 1
//-----------------------------------------------------------------------
// Writes the total amount of energy to persistent memory

void writePersistentEnergyDayGen1(int totEWr)

{

EEPROM.write(0, lowByte(totEWr));		// location 0
EEPROM.write(1, highByte(totEWr));		// location 1
++writePersistentCounter;

}

//-----------------------------------------------------------------------
// reads the total amount of energy from persistent memory

int readPersistentEnergyDayGen1()

{

int x;
int y;

y=EEPROM.read(0);		// location 0
x=EEPROM.read(1);		// location 1
y+= x << 8;

return(y);

}

//-----------------------------------------------------------------------
// Save Energy Day Values of Generator 2
//-----------------------------------------------------------------------
// Writes the total amount of energy to persistent memory

void writePersistentEnergyDayGen2(int totEWr)

{

EEPROM.write(2, lowByte(totEWr));		// location 0
EEPROM.write(3, highByte(totEWr));		// location 1
++writePersistentCounter;

}

//-----------------------------------------------------------------------
// reads the total amount of energy from persistent memory

int readPersistentEnergyDayGen2()

{

int x;
int y;

y=EEPROM.read(2);		// location 0
x=EEPROM.read(3);		// location 1
y+= x << 8;

return(y);

}

//-----------------------------------------------------------------------
// Save Energy Day Values of Generator 3
//-----------------------------------------------------------------------
// Writes the total amount of energy to persistent memory

void writePersistentEnergyDayGen3(int totEWr)

{

EEPROM.write(4, lowByte(totEWr));		// location 0
EEPROM.write(5, highByte(totEWr));		// location 1
++writePersistentCounter;

}

//-----------------------------------------------------------------------
// reads the total amount of energy from persistent memory

int readPersistentEnergyDayGen3()

{

int x;
int y;

y=EEPROM.read(4);		// location 0
x=EEPROM.read(5);		// location 1
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

// waits for a valid Command via XBee from ArdaSol
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
		  //test
		  //Serial.print(CommandBuf[i],HEX);
		  //Serial.print(":");
		  //test
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
  
  if (cmdOk && (CommandBuf[0] != meteoadr)) cmdOk = false;  // this command is not for Meteo Station
  

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
// sends a valid Meteo Station Response via XBee to ArdaSol Display
void SendResponsePacketToXBee()
{

byte i;

  digitalWrite(rtsXBee, HIGH);  //signaling transmission with red LED
  digitalWrite(STAT2, HIGH);  //signaling transmission with green LED on weather shield
 

  for (i = 0; i < rspSize ; ++i )
  {
       
    XBeeSerial.write(ResponseBuf[i] );
    
    
	// test
	Serial.print(ResponseBuf[i],HEX );
    Serial.print(':');
     //test	
    
  }
  // test
   Serial.println();
   
   
   digitalWrite(rtsXBee, LOW);  //signaling transmission with red LED
   digitalWrite(STAT2, LOW);  //signaling transmission with green LED on weather shield
   
 
   /*
   Serial.println();
   Serial.print("V=");
   Serial.println(instGridVoltsAvr,HEX);
   Serial.print("W=");
   Serial.println(usedPowerWattsAvr,HEX);
   */

}

//-----------------------------------------------------------------------
// sends a Meteo Station Response via XBee to ArdaSol Display
void SendResponseFromMeteo()

{

  int command;
  

  command = CommandBuf[1];
  command = command << 8;
  command = command | CommandBuf[2];
  
   
  if (command == cmdInstWindData)
    {
      
      ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(instWSpeedMMpS);		
      ResponseBuf[3]=lowByte(instWSpeedMMpS);
      ResponseBuf[4]=highByte(instWDirDEG);
      ResponseBuf[5]=lowByte(instWDirDEG);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();      
    }
  
  else if (command == cmdAvr2MinWindData)
    {
      
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(avr2MinWSpeedMMpS);		
      ResponseBuf[3]=lowByte(avr2MinWSpeedMMpS);
      ResponseBuf[4]=highByte(avr2MinWDirDEG);
      ResponseBuf[5]=lowByte(avr2MinWDirDEG);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
    
    }
	
	 else if (command == cmdPast10MinWindGustData)
    {
      
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(past10MinWGustMMpS);		
      ResponseBuf[3]=lowByte(past10MinWGustMMpS);
      ResponseBuf[4]=highByte(past10MinWDirDEG);
      ResponseBuf[5]=lowByte(past10MinWDirDEG);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
  
    }
	
	else if (command == cmdDayWindGustData)
    {
      
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(dayWGustMMpS);		
      ResponseBuf[3]=lowByte(dayWGustMMpS);
      ResponseBuf[4]=highByte(dayWDirDEG);
      ResponseBuf[5]=lowByte(dayWDirDEG);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
  
    }
	
	else if (command == cmdRainData)
    {
      
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(past1HRainMMhun);		
      ResponseBuf[3]=lowByte(past1HRainMMhun);
      ResponseBuf[4]=highByte(totDayRainMMhun);
      ResponseBuf[5]=lowByte(totDayRainMMhun);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
  
    }
	
	else if (command == cmdAtmosPressureData)
    {
      
	  
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(atmosDeltaDayPressurePa);
      ResponseBuf[3]=lowByte(atmosDeltaDayPressurePa);
	  
	  
      ResponseBuf[4]=highByte(atmosPressureHectoPa);
      ResponseBuf[5]=lowByte(atmosPressureHectoPa);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
  
    }
	
	else if (command == cmdLightLevelData)
    {
      
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(writePersistentCounter);		// for test purpose
      ResponseBuf[3]=lowByte(writePersistentCounter);
      ResponseBuf[4]=highByte(instLightLUXtenth);
      ResponseBuf[5]=lowByte(instLightLUXtenth);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
  
    }
	
	else if (command == cmdTempHumidityData)
    {
      
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(instTempDEGtenth);	
      ResponseBuf[3]=lowByte(instTempDEGtenth);
      ResponseBuf[4]=highByte(instHumidityPtenth);
      ResponseBuf[5]=lowByte(instHumidityPtenth);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
  
    }
	
	else if (command == cmdWindChillTempData)
    {
      
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=0;	
      ResponseBuf[3]=0;
      ResponseBuf[4]=highByte(avr2MinWindChillTempDEGtenth);
      ResponseBuf[5]=lowByte(avr2MinWindChillTempDEGtenth);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
  
    }
	
	else if (command == cmdWindGen1Data)
    {
      
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(gen1PowerW);	
      ResponseBuf[3]=lowByte(gen1PowerW);
      ResponseBuf[4]=highByte(gen1dayEnergyKWHtenth);
      ResponseBuf[5]=lowByte(gen1dayEnergyKWHtenth);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
  
    }
	
	
	else if (command == cmdWindGen2Data)
    {
      
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
	  ResponseBuf[2]=highByte(gen2PowerW);	
      ResponseBuf[3]=lowByte(gen2PowerW);
      ResponseBuf[4]=highByte(gen2dayEnergyKWHtenth);
      ResponseBuf[5]=lowByte(gen2dayEnergyKWHtenth);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
  
    }
	
	
	else if (command == cmdWindGen3Data)
    {
      
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=highByte(gen3PowerW);	
      ResponseBuf[3]=lowByte(gen3PowerW);
      ResponseBuf[4]=highByte(gen3dayEnergyKWHtenth);
      ResponseBuf[5]=lowByte(gen3dayEnergyKWHtenth);
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
  
    }
	
	
	else if (command == cmdResetMeteoValues)
    {
      
	  // todo reset action here
	  dailyrain = 0;
	  dayWGustMMpS = 0;
	  dayWDirDEG = 0;
	  
	  atmosStartDayPressurePa = atmosPressurePa;
	  
	  writePersistentEnergyDayGen3(gen3dayEnergyKWHtenth);	//to synchronize meteo with display persistent memory
	  
	   
	  
	  ResponseBuf[0]=0;
      ResponseBuf[1]=6;
      ResponseBuf[2]=0;	
      ResponseBuf[3]=0;
      ResponseBuf[4]=0;
      ResponseBuf[5]=0xFF;		//reset done
      
      crc16Checksum = uiCrc16Cal (ResponseBuf, 6) ;
      
      ResponseBuf[6] = lowByte(crc16Checksum); 
      ResponseBuf[7] = highByte(crc16Checksum);
     
      SendResponsePacketToXBee();
  
    }

}  


//-------------------------------------------
//Read the wind direction sensor, return heading in degrees
//-------------------------------------------

int get_wind_direction() 
{
  unsigned int adc;

  adc = analogRead(WDIR); // get the current reading from the sensor

  // The following table is ADC readings for the wind direction sensor output, sorted from low to high.
  // Each threshold is the midpoint between adjacent headings. The output is degrees for that ADC reading.
  // Note that these are not in compass degree order! See Weather Meters datasheet for more information.

  if (adc < 440) return (90);
  if (adc < 526) return (135);
  if (adc < 635) return (180);
  if (adc < 754) return (45);
  if (adc < 854) return (225);
  if (adc < 926) return (0);
  if (adc < 968) return (315);
  if (adc < 1000) return (270);
  return (-1); // error, disconnected?
}



//-------------------------------------------
//Returns the wind speed over dxCalcTime period
//-------------------------------------------

unsigned int get_wind_speed()
{
  const unsigned long anemometerSpeedConst = 666667;  // is 1 click per second = 2,4 km/h = 667 mm/s =  0,666 mm/ms
  unsigned int windSpeed;
 
  windSpeed = (windClicks * anemometerSpeedConst)/dxCalcTime;  // time in ms
  windClicks = 0; //Reset and start watching for new wind
 
  return(windSpeed);
}

//-------------------------------------------
//Returns the voltage of the light sensor based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
//-------------------------------------------

float get_light_level()
{
  float operatingVoltage = analogRead(REFERENCE_3V3);

  float lightSensor = analogRead(LIGHT);
  
  operatingVoltage = 3.3 / operatingVoltage; //The reference voltage is 3.3V
  
  lightSensor = operatingVoltage * lightSensor;
  
  return(lightSensor);
}

//-------------------------------------------
//Returns the voltage of the raw pin based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
//Battery level is connected to the RAW pin on Arduino and is fed through two 5% resistors:
//3.9K on the high side (R1), and 1K on the low side (R2)
//-------------------------------------------

float get_battery_level()
{
  float operatingVoltage = analogRead(REFERENCE_3V3);

  float rawVoltage = analogRead(BATT);
  
  operatingVoltage = 3.30 / operatingVoltage; //The reference voltage is 3.3V
  
  rawVoltage = operatingVoltage * rawVoltage; //Convert the 0 to 1023 int to actual voltage on BATT pin
  
  rawVoltage *= 4.90; //(3.9k+1k)/1k - multiple BATT voltage by the voltage divider to get actual system voltage
  
  return(rawVoltage);
}


//-------------------------------------------
// Calculates and gets all weather data
//-------------------------------------------

void calcWeather()
{
  float fl;
  int i;
  unsigned long avr;
  unsigned int minDir=360;
  unsigned int maxDir=0;
  
  int tempIndex;
  int wspeedIndex;
  
  
  //calculate wind speed average over two minutes
  avr = 0;
  for(i = 0 ; i < 24 ; i++) avr += windspdavg[i];  
  avr2MinWSpeedMMpS = avr / 24;
  if ((avr % 24) > 11) ++avr2MinWSpeedMMpS;  //round up
  
  avr2MinWSpeedKMpH = avr2MinWSpeedMMpS * 3.600 / 1000;
  
  //calculate wind dir average over two minutes
  // first we search the minimum and maximum wind direction
  for(i = 0 ; i < 24 ; i++) 
  {
	if (winddiravg[i] < minDir) minDir = winddiravg[i];
	if (winddiravg[i] > maxDir) maxDir = winddiravg[i];
  }
  
  // then we calculate the average
  avr = 0;
  if ((maxDir-minDir) > 180) 
  {
	for(i = 0 ; i < 24 ; i++) 
	{
		if (winddiravg[i] <= (maxDir-180)) avr += winddiravg[i]+360;
		else avr += winddiravg[i];
	}
  }
  else for(i = 0 ; i < 24 ; i++) avr += winddiravg[i]; 
  
  avr2MinWDirDEG = avr / 24;
  if ((avr % 24) > 11) ++avr2MinWDirDEG;  //round up
  
  if (avr2MinWDirDEG >= 360) avr2MinWDirDEG = avr2MinWDirDEG - 360;
  

  //Calc past10MinWGustMMpS
  //Calc past10MinWDirDEG
  //Find the largest windgust in the last 10 minutes
  past10MinWGustMMpS = 0;
  past10MinWDirDEG = 0;
  //Step through the 10 minutes  
  for(i = 0; i < 10 ; i++)
  {
    if(windgust_10m[i] > past10MinWGustMMpS)
    {
      past10MinWGustMMpS = windgust_10m[i];
      past10MinWDirDEG = windgustdirection_10m[i];
    }
  }
  
  //Total rainfall for the day is calculated within the interrupt
  //Calculate amount of rainfall for the last 60 minutes
  past1HRainMMhun = 0;  
  for(i = 0 ; i < 60 ; i++)
  past1HRainMMhun += rainHour[i];
	
  totDayRainMMhun = dailyrain;
  

  //Calc humidity
  fl = myHumidity.readHumidity();
  fl = fl * 10.0;  // we store in 1/10 percent
  instHumidityPtenth = (int)fl;
  
  //Calc tempC from Humidity sensor
  //float temp_h = myHumidity.readTemperature();
  
  fl = myHumidity.readTemperature();
  fl = fl * 10.0;  // we store in 1/10 degree celsius
  instTempDEGtenth = (int)fl;
  
  //calc wind-chill temperature for temperature range 15..-5 °C
  
  if ((instTempDEGtenth < 155) && (instTempDEGtenth > -56) && (avr2MinWSpeedMMpS > 694))
	{
										//Formel für Temperatur-Index:  25-Runden((temperatur+100)/10)
										// Berechnung im positiven Bereich
		tempIndex = (instTempDEGtenth + 100)/10;
		if (( (instTempDEGtenth + 100) % 10) >= 5) tempIndex++;
		tempIndex = 25 - tempIndex;
		tempIndex = constrain(tempIndex, 0, 20);	// sicher isch besser
	
										//Formel für Windgeschwindigkeits- Index: GANZZAHL((windgeschwindigkeit-695)/1389)

		wspeedIndex = avr2MinWSpeedMMpS;
		wspeedIndex = (wspeedIndex -695)/1389;
		wspeedIndex = constrain(wspeedIndex, 0, 11);	// sicher isch besser
	
		avr2MinWindChillTempDEGtenth = WCT[wspeedIndex][tempIndex] * 10;
	
	}
	else avr2MinWindChillTempDEGtenth = instTempDEGtenth;
  
  
  
  //Calc tempC from pressure sensor
  //tempF= myPressure.readTempF();
  
  //fl = myPressure.readTemp();
  //fl = fl * 10.0;  // we store in 1/10 degree celsius
  //instTempDEGtenth = (int)fl;
  

 
 //Calc pressure
  fl = myPressure.readPressure();
  atmosPressurePa = (long)fl;
  atmosDeltaDayPressurePa = atmosPressurePa - atmosStartDayPressurePa;
  
  atmosPressureHectoPa = atmosPressurePa/100;
  
	if ((atmosPressurePa % 100) > atmosPressureTriggerLevel) 
	{
	++atmosPressureHectoPa;
	atmosPressureTriggerLevel = 40;
	}
	else atmosPressureTriggerLevel = 60;
  
  
 
  //Calc light level
  fl = get_light_level();
  fl = fl*5000;			// 1V -> 500 Lux, we 1/10 Lux so we multiply with 5000
  instLightLUXtenth = (int)fl;

  
  //Calc battery level
  batt_lvl = get_battery_level();
}



//-------------------------------------------
// Reading of all meteo data sensors
//-------------------------------------------

void getAllMeteoData()

{

int i;

    //Take a speed and direction reading every 5 seconds for 2 minute average
    if(++fiveSeconds_2m > 23) fiveSeconds_2m = 0;
	
	// get the windspeed in mm/s measured over the last 5 seconds
	instWSpeedMMpS = get_wind_speed();
	
	// get the winddirection in degrees measured now
	i = get_wind_direction();
	if (i >= 0) instWDirDEG = i;	// take only a valid direction
	
	// store values in 2 miunutes array
	windspdavg[fiveSeconds_2m] = instWSpeedMMpS;
    winddiravg[fiveSeconds_2m] = instWDirDEG;

	 //Check to see if this is a gust for the minute
    if(instWSpeedMMpS > windgust_10m[minutes_10m])
    {
      windgust_10m[minutes_10m] = instWSpeedMMpS;
      windgustdirection_10m[minutes_10m] = instWDirDEG;
    }
	
	 //Check to see if this is a gust for the day
    if(instWSpeedMMpS > dayWGustMMpS)
    {
      dayWGustMMpS = instWSpeedMMpS;
      dayWDirDEG = instWDirDEG;
    }
    
	//test
	// if((fiveSeconds_2m % 1) == 0)		//every 12*5 seconds = 1 minute manage minute counters
    if((fiveSeconds_2m % 12) == 0)		//every 12*5 seconds = 1 minute manage minute counters
    {
      if(++minutes > 59) minutes = 0;
      if(++minutes_10m > 9) minutes_10m = 0;

      rainHour[minutes] = 0; 			//Zero out this minute's rainfall amount
      windgust_10m[minutes_10m] = 0; 	//Zero out this minute's gust
    }
 
	calcWeather();		//claculate average values and get sensor data
	
	digitalWrite(STAT1, LOW); //Turn off stat LED
}


//--------------------------------------------------
//Gets the Power Value from Virtual Wind Generator 1
//--------------------------------------------------

unsigned int get_Power_WindGeneator1() 

   /* returns power in W in relation to average 2 minutes windspeed in mm/s

  /*
  The following table represents the windspeed to power relation of generator 1
	Model: Vertical Axis TIMAR 400W
	Rotor							3 –Blatt
	Nennleistung					400W
	maximale Leistung				650 W
	Einschaltwind					1 m/s
	Nenngeschwindigkeit				12 m/s
	Unterbrechung des Windstromes	15 m/s
	max. Windgeschwindigkeit		65 m/s
	Durchmesser						1,06 m
	Rotorhöhe						1,22 m
	Gewicht							29 kg
*/

{

  if (instWSpeedMMpS < 1500) return (0);
  if (instWSpeedMMpS < 2000) return (1);
  if (instWSpeedMMpS < 2500) return (2);
  if (instWSpeedMMpS < 3000) return (4);
  if (instWSpeedMMpS < 3500) return (6);
  if (instWSpeedMMpS < 4000) return (8);
  if (instWSpeedMMpS < 4500) return (10);
  if (instWSpeedMMpS < 5000) return (22);
  if (instWSpeedMMpS < 5500) return (33);
  if (instWSpeedMMpS < 6000) return (45);
  if (instWSpeedMMpS < 6500) return (57);
  if (instWSpeedMMpS < 7000) return (68);
  if (instWSpeedMMpS < 7500) return (80);
  if (instWSpeedMMpS < 8000) return (90);
  if (instWSpeedMMpS < 8500) return (100);
  if (instWSpeedMMpS < 9000) return (125);
  if (instWSpeedMMpS < 9500) return (150);
  if (instWSpeedMMpS < 10000) return (200);
  if (instWSpeedMMpS < 10500) return (250);
  if (instWSpeedMMpS < 11000) return (275);
  if (instWSpeedMMpS < 11500) return (300);
  if (instWSpeedMMpS < 12000) return (325);
  if (instWSpeedMMpS < 12500) return (350);
  if (instWSpeedMMpS < 13000) return (367);
  if (instWSpeedMMpS < 13500) return (383);
  if (instWSpeedMMpS < 14000) return (400);
  if (instWSpeedMMpS < 14500) return (417);
  if (instWSpeedMMpS < 15000) return (433);
  if (instWSpeedMMpS < 15500) return (450);
  return (0);
  
}


//--------------------------------------------------
//Gets the Power Value from Virtual Wind Generator 2
//--------------------------------------------------

unsigned int get_Power_WindGeneator2() 

  /* returns power in W in relation to average 2 minutes windspeed in mm/s

  /*
  The following table represents the windspeed to power relation of generator 2
	Model: Horizontal Axis Windgenerator Black 600
	Rotor				3-Blatt
	Rotormaterial		Carbon-Nylon
	Rotordurchmesser	1,6m
	Generator 			Permanent
	Antrieb				Direktantrieb
	Systemspannung		48V DC
	Nennleistung bei 	11 m/s	600 Watt
 	Ladebeginn bei		1,8m/s
 	Generatorgewicht	20 Kg

*/

{  
  if (instWSpeedMMpS < 1500) return (0);
  if (instWSpeedMMpS < 2000) return (2);
  if (instWSpeedMMpS < 2500) return (3);
  if (instWSpeedMMpS < 3000) return (6);
  if (instWSpeedMMpS < 3500) return (9);
  if (instWSpeedMMpS < 4000) return (16);
  if (instWSpeedMMpS < 4500) return (22);
  if (instWSpeedMMpS < 5000) return (33);
  if (instWSpeedMMpS < 5500) return (43);
  if (instWSpeedMMpS < 6000) return (59);
  if (instWSpeedMMpS < 6500) return (75);
  if (instWSpeedMMpS < 7000) return (97);
  if (instWSpeedMMpS < 7500) return (119);
  if (instWSpeedMMpS < 8000) return (148);
  if (instWSpeedMMpS < 8500) return (178);
  if (instWSpeedMMpS < 9000) return (215);
  if (instWSpeedMMpS < 9500) return (253);
  if (instWSpeedMMpS < 10000) return (300);
  if (instWSpeedMMpS < 10500) return (347);
  if (instWSpeedMMpS < 11000) return (405);
  if (instWSpeedMMpS < 11500) return (462);
  if (instWSpeedMMpS < 12000) return (531);
  if (instWSpeedMMpS < 12500) return (600);
  if (instWSpeedMMpS < 13000) return (620);
  if (instWSpeedMMpS < 13500) return (640);
  if (instWSpeedMMpS < 14000) return (660);
  if (instWSpeedMMpS < 14500) return (680);
  if (instWSpeedMMpS < 15000) return (700);
  if (instWSpeedMMpS < 15500) return (720);
  return (0);
  
}


//--------------------------------------------------
//Gets the Power Value from Virtual Wind Generator 3
//--------------------------------------------------

int get_Power_WindGeneator3() 

  /* returns power in W in relation to average 2 minutes windspeed in mm/s

  /*
  The following table represents the windspeed to power relation of generator 3
	Model: Horizontal Axis Windkraftanlage HY-1000-24
	Rotor							5 –Blatt
	Nennleistung					1000W
	maximale Leistung				1200W
	Einschaltwind					2 m/s
	Nenngeschwindigkeit				12 m/s
	Unterbrechung des Windstromes	15 m/s
	max. Windgeschwindigkeit		50 m/s
	Durchmesser						1,96 m
	Rotorhöhe						1,22 m
	Gewicht							28 kg

*/

{

  if (instWSpeedMMpS < 1500) return (0);
  if (instWSpeedMMpS < 2000) return (3);
  if (instWSpeedMMpS < 2500) return (5);
  if (instWSpeedMMpS < 3000) return (18);
  if (instWSpeedMMpS < 3500) return (30);
  if (instWSpeedMMpS < 4000) return (45);
  if (instWSpeedMMpS < 4500) return (60);
  if (instWSpeedMMpS < 5000) return (90);
  if (instWSpeedMMpS < 5500) return (120);
  if (instWSpeedMMpS < 6000) return (160);
  if (instWSpeedMMpS < 6500) return (200);
  if (instWSpeedMMpS < 7000) return (250);
  if (instWSpeedMMpS < 7500) return (300);
  if (instWSpeedMMpS < 8000) return (355);
  if (instWSpeedMMpS < 8500) return (410);
  if (instWSpeedMMpS < 9000) return (480);
  if (instWSpeedMMpS < 9500) return (550);
  if (instWSpeedMMpS < 10000) return (625);
  if (instWSpeedMMpS < 10500) return (700);
  if (instWSpeedMMpS < 11000) return (840);
  if (instWSpeedMMpS < 11500) return (980);
  if (instWSpeedMMpS < 12000) return (1015);
  if (instWSpeedMMpS < 12500) return (1050);
  if (instWSpeedMMpS < 13000) return (1100);
  if (instWSpeedMMpS < 13500) return (1150);
  if (instWSpeedMMpS < 14000) return (1175);
  if (instWSpeedMMpS < 14500) return (1200);
  if (instWSpeedMMpS < 15000) return (1200);
  if (instWSpeedMMpS < 15500) return (1200);
  return (0);
  
}






//-------------------------------------------
// Calculate Energy Production in a day
//-------------------------------------------

void calculateDailyEnergyProduced()

{
unsigned long 	dxEnergyWms;
unsigned long 	dxEnergyWs;



//generator 1:

//test
//gen1PowerW = 400;			//instant power in watt

gen1PowerW = get_Power_WindGeneator1();			//instant power in watt
dxEnergyWms = gen1PowerW * dxCalcTime;		//Energy produced WattMilliSeconds
dxEnergyWs = dxEnergyWms / 1000;				
if ((dxEnergyWms % 1000) > 499) ++dxEnergyWs;  //Energy produced in WattSeconds
gen1dayEnergyWs += dxEnergyWs;				   //Add to daily total Energy
gen1dayEnergyKWHtenth = gen1dayEnergyWs/360000; // convert to 1/10 kiloWattHours
if ((gen1dayEnergyWs % 360000) > 180000) ++gen1dayEnergyKWHtenth;  //round

//Generator 2:

//test
//gen2PowerW = 600;			//instant power in watt

gen2PowerW = get_Power_WindGeneator2();
dxEnergyWms = gen2PowerW * dxCalcTime;
dxEnergyWs = dxEnergyWms / 1000;
if ((dxEnergyWms % 1000) > 499) ++dxEnergyWs;
gen2dayEnergyWs += dxEnergyWs;
gen2dayEnergyKWHtenth = gen2dayEnergyWs/360000;
if ((gen2dayEnergyWs % 360000) > 180000) ++gen2dayEnergyKWHtenth;

//Generator 3:

//test
//gen3PowerW = 1000;			//instant power in watt

gen3PowerW = get_Power_WindGeneator3();
dxEnergyWms = gen3PowerW * dxCalcTime;
dxEnergyWs = dxEnergyWms / 1000;
if ((dxEnergyWms % 1000) > 499) ++dxEnergyWs;
gen3dayEnergyWs += dxEnergyWs;
gen3dayEnergyKWHtenth = gen3dayEnergyWs/360000;
if ((gen3dayEnergyWs % 360000) > 180000) ++gen3dayEnergyKWHtenth;
	

 if ((millis() - energyMeasureStoreWait) > energyMeasureStoreRate)
    {
      energyMeasureStoreWait = millis();
	  if ((gen1dayEnergyKWHtenth - readPersistentEnergyDayGen1()) >= deltaStoreValue) writePersistentEnergyDayGen1(gen1dayEnergyKWHtenth);
	  if ((gen2dayEnergyKWHtenth - readPersistentEnergyDayGen2()) >= deltaStoreValue) writePersistentEnergyDayGen2(gen2dayEnergyKWHtenth);
	  if ((gen3dayEnergyKWHtenth - readPersistentEnergyDayGen3()) >= deltaStoreValue) writePersistentEnergyDayGen3(gen3dayEnergyKWHtenth);
	}
	
}


//-------------------------------------------
// Write Log Info to Console
//-------------------------------------------

void writeLogData()

{

//test
	Serial.print("WSpeed:AvgKMpH/AvgMMpS/Inst/");
	Serial.print(fiveSeconds_2m);
	Serial.print(":");
	Serial.print(avr2MinWSpeedKMpH,1);
	Serial.print(",");
	Serial.print(avr2MinWSpeedMMpS);
	Serial.print(",");
	Serial.print(instWSpeedMMpS);
	Serial.println(",");
	/*	
	for (int i=0; i<24; i++)
	{	Serial.print(windspdavg[i]);
		Serial.print(",");
	}
	Serial.println();
	*/
	Serial.print("WDir:Avg/Inst:");
	Serial.print(avr2MinWDirDEG);
	Serial.print(",");
	Serial.print(instWDirDEG);
	Serial.println(",");
	/*
	for (int i=0; i<24; i++)
	{	Serial.print(winddiravg[i]);
		Serial.print(",");
	}
	Serial.println();
	*/
	Serial.print("WGust:pDay/p10Min/");
	Serial.print(minutes_10m);
	Serial.print(":");
	Serial.print(dayWGustMMpS);
	Serial.print(",");
	Serial.print(past10MinWGustMMpS);
	Serial.println(",");
	/*
	for (int i=0; i<10; i++)
	{	Serial.print(windgust_10m[i]);
		Serial.print(",");
	}
	Serial.println();
	*/
	Serial.print("WGustDir:pDay/p10Min:");
	Serial.print(dayWDirDEG);
	Serial.print(",");
	Serial.print(past10MinWDirDEG);
	Serial.println(",");
	/*
	for (int i=0; i<10; i++)
	{	Serial.print(windgustdirection_10m[i]);
		Serial.print(",");
	}
	Serial.println();
	*/
	
	Serial.print("Rain:totDay/p1hr/");
	Serial.print(minutes);
	Serial.print(":");
	Serial.print(totDayRainMMhun);
	Serial.print(",");
	Serial.print(past1HRainMMhun);
	Serial.println(",");
	/*
	for (int i=0; i<60; i++)
	{	Serial.print(rainHour[i]);
		Serial.print(",");
	}
	Serial.println();
	*/
	Serial.print("BaroInst/Delta:");
	Serial.print(atmosPressurePa);
	Serial.print(",");
	Serial.print(atmosDeltaDayPressurePa);
	Serial.print(",");
	Serial.print("Light:");
	Serial.print(instLightLUXtenth);
	Serial.print(",");
	Serial.print("Temp:");
	Serial.print(instTempDEGtenth);
	Serial.print(",");
	Serial.print("Hum:");
	Serial.print(instHumidityPtenth);
	Serial.print(",");
	Serial.print("WindChillTemp:");
	Serial.print(avr2MinWindChillTempDEGtenth);
	Serial.println(",");
	
	Serial.print("Gen1:Watt/WattSec/kWhTenth:");
	Serial.print(gen1PowerW);
	Serial.print(",");
	Serial.print(gen1dayEnergyWs);
	Serial.print(",");
	Serial.print(gen1dayEnergyKWHtenth);
	Serial.println(",");
	
	Serial.print("Gen2:Watt/WattSec/kWhTenth:");
	Serial.print(gen2PowerW);
	Serial.print(",");
	Serial.print(gen2dayEnergyWs);
	Serial.print(",");
	Serial.print(gen2dayEnergyKWHtenth);
	Serial.println(",");
	
	Serial.print("Gen3:Watt/WattSec/kWhTenth:");
	Serial.print(gen3PowerW);
	Serial.print(",");
	Serial.print(gen3dayEnergyWs);
	Serial.print(",");
	Serial.print(gen3dayEnergyKWHtenth);
	Serial.println(",");
	
	Serial.print(sequenceCounter);
	Serial.print("-----");
	Serial.print(dxCalcTime);
	Serial.print("-----");
	Serial.println(writePersistentCounter);
	
  
  //Serial.print("Urms=");
  //Serial.print(supplyVoltage);
  //Serial.print(";Uigv=");
  //Serial.println(";");
  
  
  // for (int i=0; i<20; i++) { Serial.print(i); Serial.print(';');Serial.print(rawCurrent[i]);Serial.print(';'); }
  
  // Serial.println();
  
  
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//Interrupt routines (these are called by the hardware interrupts, not by the main code)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


void rainIRQ()
// Count rain gauge bucket tips as they occur
// Activated by the magnet and reed switch in the rain gauge, attached to input D2
{
  raintime = millis(); // grab current time
  raininterval = raintime - rainlast; // calculate interval between this and last event

    if (raininterval > 10) // ignore switch-bounce glitches less than 10mS after initial edge
  {
   	
	dailyrain += 28; 				//Each dump is 0.2794 mm of water
    rainHour[minutes] += 28; 		//Increase this minute's amount of rain

    rainlast = raintime; // set up for next event
  }
}


void wspeedIRQ()
// Activated by the magnet in the anemometer (2 ticks per rotation), attached to input D3
{
  if (millis() - lastWindIRQ > 10) // Ignore switch-bounce glitches less than 10ms (142kmh max reading) after the reed switch closes
  {
    lastWindIRQ = millis(); //Grab the current time
    windClicks++; 			//There is 2.4 km/h for 1 click per second.
  }
}



//**********************************************************************************
// Setup
//**********************************************************************************

void setup()
{  
  Serial.begin(115200);
  
  XBeeSerial.begin(19200);
  XBeeSerial.listen();
  pinMode(rtsXBee, OUTPUT); 
  
  pinMode(STAT1, OUTPUT); //Status LED Blue
  pinMode(STAT2, OUTPUT); //Status LED Green
  
  pinMode(WSPEED, INPUT_PULLUP); // input from wind meters windspeed sensor
  pinMode(RAIN, INPUT_PULLUP); // input from wind meters rain gauge sensor
  
  pinMode(REFERENCE_3V3, INPUT);
  pinMode(LIGHT, INPUT);

  //Configure the pressure sensor
  myPressure.begin(); // Get sensor online
  myPressure.setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
  myPressure.setOversampleRate(7); // Set Oversample to the recommended 128
  myPressure.enableEventFlags(); // Enable all three pressure and temp event flags 

  //Configure the humidity sensor
  myHumidity.begin();

  // timer stuff
  fiveSeconds_2m = 0;
  minutes = 0;
  minutes_10m = 0;
  
  getAllMeteoDataLastStart= millis(); 			// last getAllMeteoDataWait time stamp
  energyCalcLastStart= millis(); 				// last calculation time stamp
  energyMeasureStoreWait = millis();				// wait to check energy store values

  // only first time
  // writePersistentEnergyDayGen1(4);
  // writePersistentEnergyDayGen2(6);
  // writePersistentEnergyDayGen3(800);
  // only first time
  
  gen1dayEnergyKWHtenth = readPersistentEnergyDayGen1();
  gen2dayEnergyKWHtenth = readPersistentEnergyDayGen2();
  gen3dayEnergyKWHtenth = readPersistentEnergyDayGen3();
  
  gen1dayEnergyWs = gen1dayEnergyKWHtenth * 360000;
  gen2dayEnergyWs = gen2dayEnergyKWHtenth * 360000;
  gen3dayEnergyWs = gen3dayEnergyKWHtenth * 360000;
   
  
  //clear arrays
  for(int i = 0 ; i < 24 ; i++) 
	{
		windspdavg[i] = 0;
		winddiravg[i] = 0;
	}
  for(int i = 0 ; i < 10 ; i++) 
	{ 
		windgust_10m[i] = 0; 
		windgustdirection_10m[i] = 0; 
	}	
  for(int i = 0 ; i < 60 ; i++) 
	{ 
		rainHour[i] = 0; 
	}
  
  windClicks = 0;
  dailyrain = 0;
  
  atmosStartDayPressure = myPressure.readPressure();
  delay(1000);
  atmosStartDayPressure = myPressure.readPressure();
  atmosStartDayPressurePa = (long)atmosStartDayPressure;
  
  // attach external interrupt pins to IRQ functions
  attachInterrupt(0, rainIRQ, FALLING);
  attachInterrupt(1, wspeedIRQ, FALLING);

  // turn on interrupts
  interrupts();
  
  Serial.print("ArdaMeteo Station ");
  Serial.print(Version);
  Serial.println(Release);  
  
 }

//**********************************************************************************
// Main Loop
//**********************************************************************************

void loop()
{
  if (( millis() - getAllMeteoDataLastStart) > getAllMeteoDataWait)   // when no command packets are comming
     {	   
	   digitalWrite(STAT1, HIGH); //Blink stat LED
	   dxCalcTime = millis() - energyCalcLastStart;
	   
       getAllMeteoData();
	   calculateDailyEnergyProduced();	
	   
	   sequenceCounter++;	
	   getAllMeteoDataLastStart= millis();
	   energyCalcLastStart= millis();
	   
	   writeLogData();
	   
	   digitalWrite(STAT1, LOW); //Blink stat LED
     } 

  
  if (XBeeSerial.available())
     { 
        if (CommandPacketReceived())    SendResponseFromMeteo(); 
     }  
	 
}
