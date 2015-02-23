/* 
----------------------------
  A r d a S o l   Project
----------------------------

Version: 8.1
Version Date: 7.1.2015

Creation Date: 10.5.2013
Author: Heinz Pieren
Email: heinz.pieren@semprevacanze.it

Aurora PVI communication
------------------------
Checks via RS485 Interface the availability of
a Power one Aurora Photovoltaic Inverter PVI-3.0-TL-OUTD
The RS485 Interface is driven by SoftwareSerial Routines

pin 2 = Rx,  Pin 3 = TX
pin 5 = RTS, for HDX operation, High = Transmit, Low = Receive

PVI parameters:
Baudrate 19200, 8 Data, no parity, 1 stop
RS485 PVI-ID address = 2 (default)

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

Example: Get total amount of produced energy
02 4E 05 00 00 00 00 00 BC DD
			----- crc = 0xDDBC
      -- Accumulated data type 5 = total
   -- Get accumulated data command = 78 (0x4E)
-- PVI address = 2

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
00 06 00 00 36 34 5A 62
		  ----- crc = 0x625A
      ----------- power in kwh (32Bit) = 0x3634 = 13876 Wh
   -- MState = 6 ?
-- State = 0 ?
*/

// ArdaMeteo command definitions

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



// ArdaSol Emon commands definitions

#define emonadr            3


// Aurora PVI commands definitions

#define pviadr            2


#define cmdGridVoltage    0x3B01		//grid voltage
//#define cmdGridVoltage    0x3B20		//average grid voltage

#define cmdGridPower      0x3B03
#define cmdGridPowerPeak  0x3B23
#define cmdTime           0x4600
#define cmdEnergyDay      0x4E00
#define cmdEnergyTotal    0x4E05

#define cmdResetEnergyValue 0x0052	//reset energy total command for ArdaSol Energy Monitor



unsigned long            ATOK_TimeoutStart;
const unsigned int       ATOK_MaxWait = 2000;  //max wait time for at ok

static char ATresponse[] = { ' ', ' ', ' ' };


static byte CmdBuf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // 10 elements
static byte RecBuf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // 8 elements

unsigned long            receiveTimeoutStart;
const unsigned int       receiveMaxWait = 3000;  //three second max for the answer of the PVI

unsigned int             crc16Checksum;

//**********************************************************************************
// procedure gets the OK-AT confirmation
//**********************************************************************************

boolean getXBEE_ATOK()
{
boolean recAtOk = false;
boolean recAtTimeout = false;
int i;

ATOK_TimeoutStart = millis(); //set timeout start value
for (i=0; i < sizeof(ATresponse); i++) ATresponse[i]='n';
i=0;
  
  do            // the receive loop with timeout function
  {
    if (Serial3.available()) 
        {
          ATresponse[i] = Serial3.read();
          //Serial.print(ATresponse[i]);
          ++i;
        }
    if ((millis() - ATOK_TimeoutStart) > ATOK_MaxWait) recAtTimeout = true;
	
	if (someOneIsHere()) displayData = true;  //check the state if the IR sensor
	
  } while ((i < sizeof(ATresponse)) && (recAtTimeout == false));
  
if (!recAtTimeout) 
  {
    if ((ATresponse[0] == 'O')&&(ATresponse[1] == 'K')&&(ATresponse[2] == '\r')) recAtOk = true ;
    else Serial.println("NoAtOk");  // Serial.println(logMsgRecAtNoOkErr);
  }
   else Serial.println("ATOkTimeout");   //else writeLogData(logMsgRecATOkTimeout);
  
   Serial.print((millis() - ATOK_TimeoutStart));
   Serial.print("ms:");
   Serial.print(ATresponse[0]);
   Serial.println(ATresponse[1]);

return (recAtOk);
}


//**********************************************************************************
// procedure Set XBEE Destination Address to a desired value
// Returns true when Address changed confirmed by the XBEE module
//**********************************************************************************



boolean setXBEEdestadr(const char * cmdDLadr)
{
  boolean atcmdOk = false;
  byte XBRecDAT;
  
  while (Serial3.available() > 0) XBRecDAT=Serial3.read();	//flush receive buffer
  XBEEAddress=NONE;		
  
  // I asume that at least one second, no communication has been done to the XBEE module
  
  Serial3.write(startATmode);		//set XBEE coordinator to AT command mode
  
  atcmdOk=getXBEE_ATOK();
  
  if (atcmdOk) 
	{	
		Serial3.write(cmdDLadr);  //set new Destination Low Address
		atcmdOk=getXBEE_ATOK();
	}	
  
  if (atcmdOk) 
	{
		Serial3.write(exitATmode);
		atcmdOk=getXBEE_ATOK();
	}
  
	Serial.println(cmdDLadr);
	
  return(atcmdOk);
}


//-----------------------------------------------------------------------
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
// Sends the Reset command and gets confirmation

void getMeteoDayResetConfirmation()
{

int resConf;
  
CmdBuf[0] = meteoadr;      //RS485 chain / XBEE address of meteo station
CmdBuf[1] = highByte(cmdResetMeteoValues);
CmdBuf[2] = lowByte(cmdResetMeteoValues);

CmdBuf[6] = 0;		// no second parameter
CmdBuf[7] = 0;


crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    { 
	  Gen3Power = RecBuf[2];
	  Gen3Power = Gen3Power << 8;
      Gen3Power = Gen3Power | RecBuf[3];	//inst power produced in watt
		
      resConf = RecBuf[4];			
	  resConf = resConf << 8;
      resConf = resConf | RecBuf[5]; 	//confirmation if reset done
	  
	  if (resConf == 0xFF) dailyMeteoDataReset=false;
	 
    }
}


//-----------------------------------------------------------------------
// Sends the command get data of wind generator 3 (1kW)

void getWindGen3()
{
  
CmdBuf[0] = meteoadr;      //RS485 chain / XBEE address of meteo station
CmdBuf[1] = highByte(cmdWindGen3Data);
CmdBuf[2] = lowByte(cmdWindGen3Data);

CmdBuf[6] = 0;		// no second parameter
CmdBuf[7] = 0;


crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    { 
	  Gen3Power = RecBuf[2];
	  Gen3Power = Gen3Power << 8;
      Gen3Power = Gen3Power | RecBuf[3];	//inst power produced in watt
		
      Gen3Energy = RecBuf[4];			
	  Gen3Energy = Gen3Energy << 8;
      Gen3Energy = Gen3Energy | RecBuf[5]; 	// energy produced in 1/10 kWh
	 
    }
}


//-----------------------------------------------------------------------
// Sends the command get temperature and humidity

void getMeteoLight()
{
  
CmdBuf[0] = meteoadr;      //RS485 chain / XBEE address of meteo station
CmdBuf[1] = highByte(cmdLightLevelData);
CmdBuf[2] = lowByte(cmdLightLevelData);

CmdBuf[6] = 0;		// no second parameter
CmdBuf[7] = 0;


crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    { 
	  MeteoEnergyWriteCounter = RecBuf[2];
	  MeteoEnergyWriteCounter = MeteoEnergyWriteCounter << 8;
      MeteoEnergyWriteCounter = MeteoEnergyWriteCounter | RecBuf[3];	//EEPROM write cycles (Energy values)
		
      LightLevel = RecBuf[4];			
	  LightLevel = LightLevel << 8;
      LightLevel = LightLevel | RecBuf[5]; 	// light level 1/10 of Lux
	 
    }
}




//-----------------------------------------------------------------------
// Sends the command get temperature and humidity

void getMeteoHumidity()
{
  
CmdBuf[0] = meteoadr;      //RS485 chain / XBEE address of meteo station
CmdBuf[1] = highByte(cmdTempHumidityData);
CmdBuf[2] = lowByte(cmdTempHumidityData);

CmdBuf[6] = 0;		// no second parameter
CmdBuf[7] = 0;


crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    { 
	  MeteoTemp = RecBuf[2];
	  MeteoTemp = MeteoTemp << 8;
      MeteoTemp = MeteoTemp | RecBuf[3];	//outside temperature 1/10 degree celsius
		
      Humidity = RecBuf[4];			
	  Humidity = Humidity << 8;
      Humidity = Humidity | RecBuf[5]; 	// air humidity in 1/10 of percent
	 
    }
}


//-----------------------------------------------------------------------
// Sends the command get the windchill temperature

void getWindChillTemperature()
{
  
CmdBuf[0] = meteoadr;      //RS485 chain / XBEE address of meteo station
CmdBuf[1] = highByte(cmdWindChillTempData);
CmdBuf[2] = lowByte(cmdWindChillTempData);

CmdBuf[6] = 0;		// no second parameter
CmdBuf[7] = 0;


crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    { 
	  		
      WindChillTemp = RecBuf[4];			
	  WindChillTemp = WindChillTemp << 8;
      WindChillTemp = WindChillTemp | RecBuf[5]; 	// windchill temperature
	 
    }
}



//-----------------------------------------------------------------------
// Sends the command get atmospheric pressure

void getMeteoBaro()
{
  
CmdBuf[0] = meteoadr;      //RS485 chain / XBEE address of meteo station
CmdBuf[1] = highByte(cmdAtmosPressureData);
CmdBuf[2] = lowByte(cmdAtmosPressureData);

CmdBuf[6] = 0;		// no second parameter
CmdBuf[7] = 0;


crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    { 
	
	  BaroDeltaDay = RecBuf[2];
	  BaroDeltaDay = BaroDeltaDay << 8;
      BaroDeltaDay = BaroDeltaDay | RecBuf[3];	//delta pressure rel. to start day pressure in pascal
		
      Baro = RecBuf[4];			
	  Baro = Baro << 8;
      Baro = Baro | RecBuf[5]; 	// Barometric pressure in Hecto-Pascal
	  
    }
}


//-----------------------------------------------------------------------
// Sends the command get rain data

void getMeteoRain()
{
  
CmdBuf[0] = meteoadr;      //RS485 chain / XBEE address of meteo station
CmdBuf[1] = highByte(cmdRainData);
CmdBuf[2] = lowByte(cmdRainData);

CmdBuf[6] = 0;		// no second parameter
CmdBuf[7] = 0;


crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    { 
	  Rain1h = RecBuf[2];
	  Rain1h = Rain1h << 8;
      Rain1h = Rain1h | RecBuf[3];	//Rain in last hour in 1/100 of mm
		
      RainDay = RecBuf[4];			
	  RainDay = RainDay << 8;
      RainDay = RainDay | RecBuf[5]; // total rain in day in 1/100 of mm
	 
    }
}


//-----------------------------------------------------------------------
// Sends the command get average 2 minutes wind data

void getMeteoAvr2MinWind()
{
  
CmdBuf[0] = meteoadr;      //RS485 chain / XBEE address of meteo station
CmdBuf[1] = highByte(cmdAvr2MinWindData);
CmdBuf[2] = lowByte(cmdAvr2MinWindData);

CmdBuf[6] = 0;		// no second parameter
CmdBuf[7] = 0;


crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    { 
	  WindSpeed = RecBuf[2];		
	  WindSpeed = WindSpeed << 8;
      WindSpeed = WindSpeed | RecBuf[3];	// Average 2 minutes wind speed in mm/s
		
      WindDir = RecBuf[4];			
	  WindDir = WindDir << 8;
      WindDir = WindDir | RecBuf[5];		// Average 2 minutes wind direction 0-360 degrees
	 
    }
}

//-----------------------------------------------------------------------
// Sends the command get day wind gust data

void getMeteoDayWindGust()
{
  
CmdBuf[0] = meteoadr;      //RS485 chain / XBEE address of meteo station
CmdBuf[1] = highByte(cmdDayWindGustData);
CmdBuf[2] = lowByte(cmdDayWindGustData);

CmdBuf[6] = 0;		// no second parameter
CmdBuf[7] = 0;


crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    { 
	  WindGustSpeed = RecBuf[2];		
	  WindGustSpeed = WindGustSpeed << 8;
      WindGustSpeed = WindGustSpeed | RecBuf[3];	// day wind gust speed in mm/s
		
      WindGustDir = RecBuf[4];			
	  WindGustDir = WindGustDir << 8;
      WindGustDir = WindGustDir | RecBuf[5];		// day wind gust direction 0-360 degrees
	 
    }
}

//-----------------------------------------------------------------------
// Sends the command get current voltage measured ArdaSolEmon

void getEmonGridVoltage()
{
  
CmdBuf[0] = emonadr;      //RS485 chain / XBEE address of energy monitor
CmdBuf[1] = highByte(cmdGridVoltage);
CmdBuf[2] = lowByte(cmdGridVoltage);

CmdBuf[6] = highByte(GridVoltage);		// send value of Gridvoltage to emon
CmdBuf[7] = lowByte(GridVoltage);


crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    { 
	  EmonEnergyDayWriteCounter = RecBuf[2];
	  EmonEnergyDayWriteCounter = EmonEnergyDayWriteCounter << 8;
      EmonEnergyDayWriteCounter = EmonEnergyDayWriteCounter | RecBuf[3];

unsigned int grdV;
		
      grdV = RecBuf[4];				// validate
	  grdV = grdV << 8;
      grdV = grdV | RecBuf[5];
	  
	  if (grdV < 400) GridEmonVoltage=grdV;
    }
}


//-----------------------------------------------------------------------
// Sends the command get current power measured ArdaSolEmon

void getEmonGridPower()
{
  
CmdBuf[0] = emonadr;      //RS485 chain / XBEE address of energy monitor
CmdBuf[1] = highByte(cmdGridPower);
CmdBuf[2] = lowByte(cmdGridPower);

CmdBuf[6] = highByte(GridPower);		// send value of Solar Grid power to emon
CmdBuf[7] = lowByte(GridPower);


crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    {  
	
unsigned int grdP;

      grdP = RecBuf[4];				//validate
	  grdP = grdP << 8;
      grdP = grdP | RecBuf[5];
	  
	  if (grdP < 10000) GridEmonPower=grdP;
    }
}


//-----------------------------------------------------------------------
// Sends the command get current power measured ArdaSolEmon

void getEmonEnergyDay()
{
  
CmdBuf[0] = emonadr;      //RS485 chain / XBEE address of energy monitor
CmdBuf[1] = highByte(cmdEnergyDay);
CmdBuf[2] = lowByte(cmdEnergyDay);

if (resetEnergyDayUsed) 
{
	CmdBuf[6] = highByte(cmdResetEnergyValue);
	CmdBuf[7] = lowByte(cmdResetEnergyValue);
}
else
{
	CmdBuf[6] = 0;
	CmdBuf[7] = 0;
}

crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    {  
	
unsigned long grdE;

	  grdE = getReceivedValueAsInt();	  
	  if (grdE == 0) resetEnergyDayUsed=false;
	  if (grdE < 360000000) EmonEnergyDayWs=grdE;			//not more than 100kWh in a day!
	  
    }
}

//-----------------------------------------------------------------------
// Sends the command get current power feeding to grid

void getGridVoltage()
{
  
CmdBuf[0] = pviadr;      //RS485 chain address of PVI
CmdBuf[1] = highByte(cmdGridVoltage);
CmdBuf[2] = lowByte(cmdGridVoltage);

CmdBuf[6] = 0;
CmdBuf[7] = 0;

crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    {  
	
float grdVpvi;

      grdVpvi = *getReceivedValueAsFloat();
	  if ((int(grdVpvi) >= 0) && (int(grdVpvi) < 400))
      {
		GridVoltage = int(grdVpvi);
		if (int((grdVpvi-GridVoltage)*10) >= 5) ++GridVoltage;      //round up
	  }
    }
}

//-----------------------------------------------------------------------
// Sends the command get current power feeding to grid

void getGridPower()
{
  
CmdBuf[0] = pviadr;      //RS485 chain address of PVI
CmdBuf[1] = highByte(cmdGridPower);
CmdBuf[2] = lowByte(cmdGridPower);

CmdBuf[6] = 0;
CmdBuf[7] = 0;

crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 
                
sendRequest();

if (receiveAnswer()) 
    {  
	
float grdPpvi;

      grdPpvi = *getReceivedValueAsFloat();
	  if ((int(grdPpvi) >= 0) && (int(grdPpvi) < 4000))
	  {
		GridPower = int(grdPpvi);
		if (int((grdPpvi-GridPower)*10) >= 5) ++GridPower;      //round up
	  }
    }
}

//-----------------------------------------------------------------------
// Sends the command get current date and time

void getTime()
{
CmdBuf[0] = pviadr;      //RS485 chain address of PVI
CmdBuf[1] = highByte(cmdTime);
CmdBuf[2] = lowByte(cmdTime);

CmdBuf[6] = 0;
CmdBuf[7] = 0;

crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 

sendRequest();

if (receiveAnswer()) DateAndTime = getReceivedValueAsInt();  

}

//-----------------------------------------------------------------------
// Sends the command get the cumulated energy current day

void getEnergyDay()
{
CmdBuf[0] = pviadr;      //RS485 chain address of PVI
CmdBuf[1] = highByte(cmdEnergyDay);
CmdBuf[2] = lowByte(cmdEnergyDay);

CmdBuf[6] = 0;
CmdBuf[7] = 0;

crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 

sendRequest();

if (receiveAnswer()) 
{
unsigned long Epvi;

	Epvi = getReceivedValueAsInt(); 
	if (Epvi < 40000) 	//validate <40kWh in a day
		{ 
			EnergyDay = Epvi;		
			writeToRTCLong(EnergyDayRTCAdr,EnergyDay);
		}
}
}

//-----------------------------------------------------------------------
// Sends the command get the total cumulated
// no more used at the moment

void getEnergyTotal()
{
CmdBuf[0] = pviadr;      //RS485 chain address of PVI
CmdBuf[1] = highByte(cmdEnergyTotal);
CmdBuf[2] = lowByte(cmdEnergyTotal);

CmdBuf[6] = 0;
CmdBuf[7] = 0;

crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 

sendRequest();

if (receiveAnswer()) 
        {
          EnergyTotal = getReceivedValueAsInt();
		  EnergyTotal = EnergyTotal % 100000000;   // due to problem with PVI RAM failed
          EnergyTotalkWh = EnergyTotal / 1000;
          if ((EnergyTotal % 1000) > 499)   ++EnergyTotalkWh;  // round up
        }
            
}

//-----------------------------------------------------------------------
// Sends the command get the grid power peak current day

void getGridPowerPeakDay()
{
CmdBuf[0] = pviadr;      //RS485 chain address of PVI
CmdBuf[1] = highByte(cmdGridPowerPeak);
CmdBuf[2] = lowByte(cmdGridPowerPeak);

CmdBuf[6] = 0;
CmdBuf[7] = 0;

crc16Checksum = uiCrc16Cal (CmdBuf, 8) ;
CmdBuf[8] = lowByte(crc16Checksum); 
CmdBuf[9] = highByte(crc16Checksum); 

sendRequest();

if (receiveAnswer()) 
    { 

float GPPD;	
      
	  GPPD = *getReceivedValueAsFloat();
	  if ((int(GPPD) >= 0) && (int(GPPD) < 4000))
	  {
		GridPowerPeakDay = int(GPPD);
		if (int((GPPD-GridPowerPeakDay)*10) >= 5) ++GridPowerPeakDay;
	  }
    }
}

//-----------------------------------------------------------------------
void clearAuroraSerialInput()
{
byte x;
byte y;

const byte msglen = 24;

char msg[msglen];

x=0;

while (Serial3.available() > 0) 
    {
      y = Serial3.read();
      x++;
    }
    
if (x>0)
    {
      snprintf (msg, msglen, "%s,%d", logMsgFlushRecBuf,x);
	  
	  // msg = logMsgFlushRecBuf;
      // msg += x;
      writeLogData(msg);  // buffer has been flushed
    }

}


//-----------------------------------------------------------------------

void sendRequest()
{
  clearAuroraSerialInput();
  
  
  
  for ( byte i = 0; i < sizeof(CmdBuf); ++i )
  {
    // digitalWrite(DataLED, HIGH);  //Turns RTS On for transmisson
    Serial3.write(CmdBuf[i] );		        // or ev. ohne for -> auroraSerial.write(CmdGetEnergyTot, sizeof(CmdGetEnergyTot));
    // digitalWrite(DataLED, LOW);  //Turns RTS Off
  }
  

 
}
//-----------------------------------------------------------------------

boolean receiveAnswer()
{
  boolean recOk = false;
  boolean recTimeout = false;
  unsigned int recCRC;
  byte i;

  for (i = 0; i < sizeof(RecBuf); ++i )  RecBuf[i] = 0; //clear buffer
  
  receiveTimeoutStart = millis(); //set timeout start value
  i=0;
  
  do            // the receive loopwith timeout function
  {
    if (Serial3.available()) 
        {
          RecBuf[i] = Serial3.read();
          ++i;
        }
    if ((millis() - receiveTimeoutStart) > receiveMaxWait) recTimeout = true;
	
	if (someOneIsHere()) displayData = true;  //check the state if the IR sensor
	
  } while ((i < sizeof(RecBuf)) && (recTimeout == false));
  
  if (!recTimeout) 
  {
    recCRC = RecBuf[6] + (RecBuf[7] << 8);
    if (recCRC == uiCrc16Cal (RecBuf, 6)) recOk = true ;
    else Serial.println(logMsgRecChkSumErr);
  }
  else 
  {
	//Serial.print("XBEEAddress=");
	//Serial.println(XBEEAddress);
	if ( XBEEAddress == REMOTE) writeLogData(logMsgRecTimeoutRemote);
	else if ( XBEEAddress == EMON) writeLogData(logMsgRecTimeoutEmon);
	else if ( XBEEAddress == METEO) writeLogData(logMsgRecTimeoutMeteo);
	else writeLogData(logMsgRecTimeout);
  }

return (recOk);
}

//-----------------------------------------------------------------------

static unsigned long getReceivedValueAsInt()
{
unsigned long value;

  value = RecBuf[2];
  value = value << 8;
  value = value | RecBuf[3];
  value = value << 8;
  value = value | RecBuf[4];
  value = value << 8;
  value = value | RecBuf[5];

return (value);
}
//-----------------------------------------------------------------------

static float *getReceivedValueAsFloat()
{
byte cValue[4];
float *value;
  
    cValue[0] = RecBuf[5];
    cValue[1] = RecBuf[4];
    cValue[2] = RecBuf[3];
    cValue[3] = RecBuf[2];

    value = (float *)cValue;

return(value);
}
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------



