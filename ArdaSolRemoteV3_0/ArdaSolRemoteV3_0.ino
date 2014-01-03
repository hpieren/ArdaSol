/* 
---------------------------------------
ArdaSol Photovoltaic Inverter Display
---------------------------------------
*/

#define Version "V3.0 "
#define Release "R1"

/*
Version: 1.0.0
Creation Date: 5.6.2013
Author: Heinz Pieren
Email: heinz.pieren@semprevacanze.it
---------------------------------------
XBEE Remote to Aurora PVI and
XBEE Remote to ArdaSol communication
---------------------------------------

RS485:
pin 2 = Rx,  Pin 3 = TX
pin 5 = RTS, for HDX operation, High = Transmit, Low = Receive and RS485 LED indication

PVI parameters:
Baudrate 19200, 8 Data, no parity, 1 stop
RS485 PVI-ID address = 2 (default)

XBEE:
pin 6 = Rx, Pin 7 = Tx
pin 8 = RTS, for XBEE LED indication

The PVI-Data-Packets from the XBee are stored in CommandBuf
and the answers from PVI are stored in ResponseBuf, due to Softare Serial only one Channel can be listened.
The packets in both directions will be check for a valid checksum

Log Data is sent to USB Serial

*/

#include <SoftwareSerial.h>

#define rx485Pin 2      // arduino input
#define tx485Pin 3      // arduino output
#define rts485Pin 5     // arduino output

SoftwareSerial RS485Serial(rx485Pin, tx485Pin); 

#define rxXBeePin 6      // arduino input
#define txXBeePin 7      // arduino output
#define rtsXBeePin 8     // arduino output

SoftwareSerial XBeeSerial(rxXBeePin, txXBeePin);

#define logMessageStart            "*01:start"

#define logMessageFlushXBeeRecBuf    "*E1:FlushXBeeRB x="
#define logMessageXBeeRecChkSumErr   "*E2:XBeeChkSumErr"
#define logMessageXBeeRecTimeout     "*E3:XBeeRecTimeout"
#define logMessageFlushPVIRecBuf     "*E4:FlushPVIRB x="
#define logMessagePVIRecChkSumErr    "*E5:PVIChkSumErr"
#define logMessagePVIRecTimeout      "*E6:PVIRecTimeout"

#define cmdSize 10    // 10 Bytes command Packet
static byte CommandBuf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // 10 elements

#define rspSize 8    // 8 Bytes response Packet
static byte ResponseBuf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // 8 elements

unsigned long            sequenceCounter;

unsigned long            receiveTimeoutStart;
const unsigned int       receiveMaxXBeeWait =  500;  //500ms timeout for XBEE packet to receive
const unsigned int       receiveMaxRS485Wait = 1000;  //1000ms timeout for PVI packet receive

unsigned int             crc16Checksum;

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
void clearRS485SerialInput()
{
byte x;
byte y;

x=0;

while (RS485Serial.available() > 0) 
    {
      y = RS485Serial.read();
      x++;
    }
    
if (x>0)
    {
      Serial.print(sequenceCounter);
      Serial.print(':');
      Serial.print(logMessageFlushPVIRecBuf);  // buffer has been flushed
      Serial.println(x);
    }
}


//-----------------------------------------------------------------------

// waits for a valid PVI Command from XBee (ArdaSol)
boolean CommandPacketReceived()
{
  boolean cmdOk;
  boolean cmdTimeout;
  unsigned int cmdCRC;
  byte i;
  
  cmdOk = false;
  
  if (!XBeeSerial.isListening())
         {
          XBeeSerial.listen();        // switch over to XBee communication
          clearXBeeSerialInput();
         } 
  
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
  
return (cmdOk);
}

//-----------------------------------------------------------------------
// sends a valid PVI Command to RS485
void SendCommandPacketToPVI()
{
  
byte i;

if (!RS485Serial.isListening())
         {
          RS485Serial.listen();        // switch over to RS485 communication
          clearRS485SerialInput();
         } 

  
  for (i = 0; i < cmdSize ; ++i )
  {
    digitalWrite(rts485Pin, HIGH);  //Turns RTS On for transmisson
    RS485Serial.write(CommandBuf[i] );		        
    digitalWrite(rts485Pin, LOW);  //Turns RTS Off
  }
 
}

//-----------------------------------------------------------------------
// waits for a valid PVI Response from RS485 (Aurora)
boolean ResponsePacketReceived()
{

  boolean respOk;
  boolean respTimeout;
  unsigned int respCRC;
  byte i;
  
  respOk = false;
  i=0;
  respTimeout = false;
  receiveTimeoutStart = millis();
  
  if (!RS485Serial.isListening())
         {
          RS485Serial.listen();        // switch over to RS485 communication
          clearRS485SerialInput();
         } 
  
    do            // the receive loop with timeout function
    {
      
      if (RS485Serial.available()) 
      {
          ResponseBuf[i] = RS485Serial.read();
          ++i;
      }
      
      if ((millis() - receiveTimeoutStart)  > receiveMaxRS485Wait) respTimeout = true;
    } while ((i < rspSize) && !respTimeout);
  
    if ((!respTimeout) && (i == rspSize)) 
    {
      respCRC = ResponseBuf[6] + (ResponseBuf[7] << 8);
      if (respCRC == uiCrc16Cal (ResponseBuf, 6)) respOk = true ;
      else 
          {
            Serial.print(sequenceCounter);
            Serial.print(':');
            Serial.println(logMessagePVIRecChkSumErr);
          }
    }
    else 
        {
          Serial.print(sequenceCounter);
          Serial.print(':');
          Serial.println(logMessagePVIRecTimeout);
        }
  
return (respOk);

  
}

//-----------------------------------------------------------------------
// sends a valid PVI Response to XBee (ArdaSol)
void SendResponsePacketToXBee()
{

byte i;

if (!XBeeSerial.isListening())
         {
          XBeeSerial.listen();        // switch over to XBee communication
          clearXBeeSerialInput();
         } 
  
  for (i = 0; i < rspSize ; ++i )
  {
    digitalWrite(rtsXBeePin, HIGH);  //Turns RTS On for transmisson
    XBeeSerial.write(ResponseBuf[i] );		        
    digitalWrite(rtsXBeePin, LOW);  //Turns RTS Off
  }
  
}

//-----------------------------------------------------------------------
// Init stuff
//-----------------------------------------------------------------------

void setup() 
{
  pinMode(rx485Pin, INPUT);
  pinMode(tx485Pin, OUTPUT);
  pinMode(rts485Pin, OUTPUT);
  
  pinMode(rxXBeePin, INPUT);
  pinMode(txXBeePin, OUTPUT);
  pinMode(rtsXBeePin, OUTPUT);
  
  Serial.begin( 115200 );      // console terminal for log info
  
  RS485Serial.begin(19200);    // to aurora PVI
  XBeeSerial.begin(19200);     // to ArdaSol
  
  sequenceCounter=0;
  
  Serial.print("ArdaSolRemote ");
  Serial.print(Version);
  Serial.println(Release);
  
  Serial.print(sequenceCounter);
  Serial.print(':');
  Serial.println(logMessageStart); 
}

//-----------------------------------------------------------------------
// Main Loop
//----------------------------------------------------------------------- 

void loop()
{

if (CommandPacketReceived())    SendCommandPacketToPVI();   // CommandPacketReceived() is waiting until valid packet received

if (ResponsePacketReceived())   SendResponsePacketToXBee(); // ResponsePacketReceived() is waiting until valid packet received or timeout occurred

sequenceCounter++;

}
