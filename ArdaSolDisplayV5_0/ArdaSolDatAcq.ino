/* 
----------------------------
  A r d a S o l   Project
----------------------------

Version: 5.0
Version Date: 10.12.2013

Creation Date: 5.6.2013
Author: Heinz Pieren
Email: heinz.pieren@semprevacanze.it

--------------------------------------------
Data Acquisition from ArdaSol Display
Storage on SD-Card and on a Internet Service
--------------------------------------------

*/

// SD Card Stuff:
// -------------

#include <SD.h>

#define SSpin        10      // -> On ethernet shield: SS pin
#define CSpin        4       // -> On ethernet shield: CS pin

File logFile;
char curLogFileName[13];  // 13 -> last char has to be a NUL char for termination


#define logMessageStart            "*D1DAq:start"
#define logMessageWrittenTo        "I1DAq:WrittenTo->"
#define logMessageFileNotOpened    "*E5DAq:LogFileNotOpened"

 


// Ethernet Connection Stuff
// -------------------------

unsigned long lastConnectionTime = 0;          // last time you connected to the server, in milliseconds
boolean lastConnected = false;                 // state of the connection last time through the main loop
const unsigned long responseTimeout = 10*1000; //timeout for the answer from xively


#include <SPI.h>
#include <WiFi.h>

// ArdaSol xively account Data

#define             APIKEY         "zLE7r7z9cdNARx_9KRUy2xez8zeSAKxUR0h0K0VmMXFtST0g" // your cosm api key
#define             FEEDID         70230 // your feed ID
#define             USERAGENT      "ArdaSol v3.0" // user agent is the project name

char ssid[] = "NETGEAR";      //  your network SSID (name) 
char pass[] = "faxfog100%";   // your network password

// the IP address for the shield:
IPAddress WiFiIP(192, 168, 0, 32);    


// initialize the library instance:
WiFiClient client;


// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(216,52,233,121);      // numeric IP for api.cosm.com
//IPAddress server(64.94.18.120);      // numeric IP for api.xively.com
// char server[] = "api.cosm.com";   // name address for cosm API
char server[] = "api.xively.com";   // name address for xively API

// Xively Data Channels

#define XivelyCHinstPower          "1_Pp"
#define XivelyCHinstEnergy         "2_Ep"
#define XivelyCHpeakPower          "3_PpPk"
#define XivelyCHtotEnergy          "4_EpTo"

#define XivelyCHinstTemperature    "5_Temp"

#define XivelyCHinstGridVoltage    "6a_Vg"
#define XivelyCHemonVoltage    	   "6b_Vg"
#define XivelyCHemonPower          "7_Pc"
#define XivelyCHemonEnergy         "8a_Ec"
#define XivelyCHemonEnergyGrid     "8b_Ecg"
#define XivelyCHemontotEnergy      "9_EcTo"

#define XivelyCHstateSD            "A_SD"
#define XivelyCHemonEnergyDayWriteCounter 	"B_WC"






byte state_SD;


#define nDSlen 201

char newDataString[nDSlen];

//-----------------------------------------------------------------------
// SD Card Stuff
//-----------------------------------------------------------------------

void initSDCard()

{
  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin 
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output 
  // or the SD library functions will not work. 
  
   pinMode(SSpin, OUTPUT);
  
   
  if (SD.begin(CSpin)) 
  {
    SDcardAvailable = true;
  }
    else 
  {
    SDcardAvailable = false;
  }
  
}

//-----------------------------------------------------------------------
// Getlogfile name from date
//-----------------------------------------------------------------------

boolean getLogFileNameFromDate()

{

byte i;
byte x;

boolean fileNameSet;

fileNameSet = false;

x = year(at)/10;
x = x % 10;
      curLogFileName[0] = char(x + 0x30);  // Year zehner
x = year(at) % 10;
      curLogFileName[1] = char(x + 0x30); // Year einer
      curLogFileName[2] = '-';

x = month(at)/10;
x = x % 10;      
      curLogFileName[3] = constrain(char(x + 0x30),'0','1');  // month zehner
x = month(at) % 10;	  
      curLogFileName[4] = char(x + 0x30); // month einer
      curLogFileName[5] = '-';

x = day(at)/10;
x = x % 10;            
      curLogFileName[6] = constrain(char(x + 0x30),'0','3');  // day zehner
x = day(at) % 10;	  
      curLogFileName[7] = char(x + 0x30); // day einer
      curLogFileName[8] = '.';
      curLogFileName[9] = 'c';
      curLogFileName[10] = 's';
      curLogFileName[11] = 'v';
      curLogFileName[12] = 0;
      
      fileNameSet = true;    
  
return (fileNameSet);
}


//-----------------------------------------------------------------------
// Get Valid Solar Data and send it to xively
//-----------------------------------------------------------------------

void sendSolarDataToXively()

{

int EikW;		// instant Energy kW
int EiWh;		// instant Energy W hunderter
int EiWz;		// instant Energy W zehner
int EiWe;		// instant Energy W einer

int x;

int cx;
int i;


i=0;  // the index of the newDataString

// instant power
// -------------

  if (GridPower > 0)
	{
		cx = snprintf (newDataString+i, nDSlen-i, "%s,%ld\r\n", XivelyCHinstPower,GridPower);
		i = i+cx;
	}

// instant energy
// --------------

		EikW = EnergyDay/1000;
		x = EnergyDay % 1000;
		EiWh = x / 100;
		x = x % 100;
		EiWz = x / 10;
		x = x % 10;
		EiWe = x;
		
	 if (EnergyDay > 0)
	 {
		cx = snprintf (newDataString+i, nDSlen-i, "%s,%d.%d%d%d\r\n", XivelyCHinstEnergy,EikW,EiWh,EiWz,EiWe);
		i = i+cx;
	 }

// peak power
// ----------

  if (GridPowerPeakDay > 0)
  {
		cx = snprintf (newDataString+i, nDSlen-i, "%s,%ld\r\n", XivelyCHpeakPower,GridPowerPeakDay);
		i = i+cx;
  }
  
// total energy
// ------------

  if (EnergyTotalkWh > 0)
  {
		cx = snprintf (newDataString+i, nDSlen-i, "%s,%ld\r\n", XivelyCHtotEnergy,EnergyTotalkWh);
		i = i+cx;
  }
  
// instant temperature
// -------------------

  if ((ambientTemperature > -10) && (ambientTemperature < 70))
  {
		cx = snprintf (newDataString+i, nDSlen-i, "%s,%d.%d\r\n", XivelyCHinstTemperature,ambientTempAvrOneTenthClesius/10,abs(ambientTempAvrOneTenthClesius%10));
		i = i+cx;
  }
  
// instant grid voltage
// --------------------

  if (GridVoltage > 0)
	{
		cx = snprintf (newDataString+i, nDSlen-i, "%s,%ld\r\n", XivelyCHinstGridVoltage,GridVoltage);
		i = i+cx;
	}  



SendDataRecordToXively();

}


//-----------------------------------------------------------------------
// Get Valid Emon Data and send it to xively
//-----------------------------------------------------------------------

void sendEmonDataToXively()

{

int EikW;		// instant Energy kW
int EiWh;		// instant Energy W hunderter
int EiWz;		// instant Energy W zehner
int EiWe;		// instant Energy W einer

int x;

int cx;
int i;


i=0;  // the index of the newDataString

  
// instant emon voltage
// --------------------
	
	if (GridEmonVoltage > 0)
	{
		cx = snprintf (newDataString+i, nDSlen-i, "%s,%d\r\n", XivelyCHemonVoltage,GridEmonVoltage);
		i = i+cx;
	}

	
// emon power
// ----------

  if (GridEmonPower > 0)
	{
		cx = snprintf (newDataString+i, nDSlen-i, "%s,%d\r\n", XivelyCHemonPower,GridEmonPower);
		i = i+cx;
	}
  

// emon energy in a day
// --------------------

		EikW = EmonEnergyDayWh/1000;
		x = EmonEnergyDayWh % 1000;
		EiWh = x / 100;
		x = x % 100;
		EiWz = x / 10;
		x = x % 10;
		EiWe = x;
		
		if (EmonEnergyDayWh > 0)
		{
			cx = snprintf (newDataString+i, nDSlen-i, "%s,%d.%d%d%d\r\n", XivelyCHemonEnergy,EikW,EiWh,EiWz,EiWe);
			i = i+cx;
		}

	 
// emon energy in a day from grid
// ------------------------------

		EikW = EnergyConsFromGridWh/1000;
		x = EnergyConsFromGridWh % 1000;
		EiWh = x / 100;
		x = x % 100;
		EiWz = x / 10;
		x = x % 10;
		EiWe = x;
		
	 if (EnergyConsFromGridWh > 0)
	 {
		cx = snprintf (newDataString+i, nDSlen-i, "%s,%d.%d%d%d\r\n", XivelyCHemonEnergyGrid,EikW,EiWh,EiWz,EiWe);
		i = i+cx;
	 }

// total energy
// ------------

  if (EmonEnergyTotalkWh > 0)
  {
		cx = snprintf (newDataString+i, nDSlen-i, "%s,%ld\r\n", XivelyCHemontotEnergy,EmonEnergyTotalkWh);
		i = i+cx;
  }  
  

// some arduino hw data, state of sd card
  
cx = snprintf (newDataString+i, nDSlen-i, "%s,%d\r\n", XivelyCHstateSD,state_SD);
i = i+cx;


// some arduino hw data, emon energy write cycles in EEPROM
  
cx = snprintf (newDataString+i, nDSlen-i, "%s,%d\r\n", XivelyCHemonEnergyDayWriteCounter,EmonEnergyDayWriteCounter);
i = i+cx;
  

SendDataRecordToXively();

}

 
//-----------------------------------------------------------------------
// Send Solar Data to Xively when enter in sleep mode
//-----------------------------------------------------------------------

void sendSleepDataToXively()

{

int cx;
int i;

i=0;
cx = snprintf (newDataString+i, nDSlen-i, "%s,0\r\n", XivelyCHinstPower);

i = i+cx;
cx = snprintf (newDataString+i, nDSlen-i, "%s,%d.%d\r\n", XivelyCHinstTemperature,ambientTempAvrOneTenthClesius/10,abs(ambientTempAvrOneTenthClesius%10));

i = i+cx;
cx = snprintf (newDataString+i, nDSlen-i, "%s,%d\r\n", XivelyCHstateSD,state_SD);

// i = i+cx;
// cx = snprintf (newDataString+i, nDSlen-i, "%s,%d\r\n", XivelyCHstateRAM,freeRam());


SendDataRecordToXively();

}


//-----------------------------------------------------------------------
// Send Solar Data to Xively when awaked
//-----------------------------------------------------------------------

void sendNewDayDataToXively()

{
int cx;
int i;

i=0;
cx = snprintf (newDataString+i, nDSlen-i, "%s,0.0\r\n", XivelyCHinstEnergy);

i = i+cx;
cx = snprintf (newDataString+i, nDSlen-i, "%s,0\r\n", XivelyCHinstPower);

i = i+cx;
cx = snprintf (newDataString+i, nDSlen-i, "%s,%d.%d\r\n", XivelyCHinstTemperature,ambientTempAvrOneTenthClesius/10,abs(ambientTempAvrOneTenthClesius%10));

i = i+cx;
cx = snprintf (newDataString+i, nDSlen-i, "%s,0\r\n", XivelyCHpeakPower);

i = i+cx;
cx = snprintf (newDataString+i, nDSlen-i, "%s,%ld\r\n", XivelyCHtotEnergy,EnergyTotalkWh);

i = i+cx;
cx = snprintf (newDataString+i, nDSlen-i, "%s,%d\r\n", XivelyCHstateSD,state_SD);

// i = i+cx;
// cx = snprintf (newDataString+i, nDSlen-i, "%s,%d\r\n", XivelyCHstateRAM,freeRam());

SendDataRecordToXively();

}

  
  
//-----------------------------------------------------------------------
// Write Log Data to SD Card
//-----------------------------------------------------------------------

void WriteDataRecordToSD()

{

logFile = SD.open( curLogFileName , FILE_WRITE);

if (logFile)
{
  logFile.print(logMessage);
  
  Serial.print(at);
  Serial.print(':');
  Serial.print(logMessageWrittenTo);
  Serial.println(curLogFileName);
  state_SD = 1;
}
else 
{
  Serial.print(at);
  Serial.print(':');
  Serial.println(logMessageFileNotOpened);
  state_SD = 0;
}


logFile.close();
}

//-----------------------------------------------------------------------
// Ethernet Stuff
//-----------------------------------------------------------------------

void initEthernetCard()


{
  int status = WL_IDLE_STATUS;

  internetAvailable=true;

 // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) 
    {
      Serial.println("WiFi shield not present");
      internetAvailable=false;
    }

  else
    {
      if ( status != WL_CONNECTED)
      { 
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);
		
		// set the id adress of the shield
		// not possible with this library
		// WiFi.config(WiFiIP);

        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:    
        status = WiFi.begin(ssid, pass);
      }
     
      if (status == WL_IDLE_STATUS) 
      {
        internetAvailable = false;
        Serial.println("Network not available");
      }
      else 
      {
        internetConnFailCnt=0;				//reset the error counter
		Serial.println("Network available");
        printWifiStatus();
      }
    }
    

}

//---------------------------------------------

void printWifiStatus() 
{
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  
  Serial.print("Firmware Version:");
  Serial.println(WiFi.firmwareVersion());
}

//-----------------------------------------------------------------------
// Internet Stuff
//----------------------------------------------------------------------- 


void SendDataRecordToXively()

{
  
    digitalWrite(DataLED, HIGH);  //Turns Data LED On
	
	delay(500);		//wait if there is a sd operation in the course
        
    sendData(newDataString);
    
  do {
  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:
 
  while (client.available() > 0)
  {
     char c = client.read();
     Serial.print(c);
  }

  // if there's no net connection, but there was one last time
  // through the loop, then stop the client:
  lastConnected = client.connected();
  if (!lastConnected) {
     
	 client.stop();
     Serial.print("disconn.(");
	 Serial.print(internetConnFailCnt);
	 Serial.print(") ");
    
     Serial.println(freeRam()); 

  }
    
  } while (lastConnected && (millis() - lastConnectionTime < responseTimeout));
  
  if (lastConnected) 
      {
        client.flush();
        Serial.print("flush and disconn. ");
        client.stop();
        Serial.println(freeRam()); 
        lastConnected=false;
      }
	  
   if (internetConnFailCnt > maxConnFail) initEthernetCard();  // try to connect to wireless router again
      
    digitalWrite(DataLED, LOW);  //Turns Data LED Off

}



//-----------------------------------------------------------------------
// this method makes a HTTP connection to the server:

void sendData(char thisData[]) 

{

// test for length of put request
	
	Serial.println();
	Serial.print("PUT /v2/feeds/");
    Serial.print(FEEDID);
    Serial.println(".csv HTTP/1.1");
    Serial.println("Host: api.xively.com");
    Serial.print("X-ApiKey: ");
    Serial.println(APIKEY);
    Serial.print("User-Agent: ");
    Serial.println(USERAGENT);
    Serial.print("Content-Length: ");
    Serial.println(strlen(thisData));

    // last pieces of the HTTP PUT request:
    Serial.println("Content-Type: text/csv");
    Serial.println("Connection: close");
    
    // here's the actual content of the PUT request:
    Serial.print(thisData);



// if there's a successful connection:
if (client.connect(server, 80)) 
{
    Serial.print("conn to Xively..");
    Serial.println(freeRam());
     
    
	// send the HTTP PUT request:
    client.print("PUT /v2/feeds/");
    client.print(FEEDID);
    client.println(".csv HTTP/1.1");
    client.println("Host: api.xively.com");
    client.print("X-ApiKey: ");
    client.println(APIKEY);
    client.print("User-Agent: ");
    client.println(USERAGENT);
    client.print("Content-Length: ");
    client.println(strlen(thisData));

    // last pieces of the HTTP PUT request:
    client.println("Content-Type: text/csv");
    client.println("Connection: close");
    client.println();				// has to be!

    // here's the actual content of the PUT request:
    client.print(thisData);
	
	internetConnFailCnt=0;				//reset the error counter
	
    } 
  else 
    {
    // if you couldn't make a connection:
	internetConnFailCnt+=1;		//increment error counter
    Serial.print("conn. failed(");
	Serial.print(internetConnFailCnt);
	Serial.println("), disconnecting");
    client.stop();
	
    }
  // note the time that the connection was made or attempted:
  lastConnectionTime = millis();
}
    

    

//-----------------------------------------------------------------------
// Test Stuff
//-----------------------------------------------------------------------
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}



