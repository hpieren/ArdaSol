/* 
----------------------------
  A r d a S o l   Project
----------------------------

Version: 5.0
Version Date: 10.12.2013

Creation Date: 10.5.2013
Author: Heinz Pieren
Email: heinz.pieren@semprevacanze.it

Dot Matrix Display Drivers
--------------------------


*/

//#include <WProgram.h>
#include "ht1632.h"
#include <avr/pgmspace.h>
#include "font.h"

// Display Line 1 and 2 string variables:

#define LLen 7

  char L1[LLen];  // 6 char
  char L2[LLen];  // 6 char

// Display Line 1 and 2 offsets variables:

  byte L1_Ofs;
  byte L2_Ofs;
  

#define X_MAX 32 //32
#define Y_MAX 16
#define CHIP_MAX 4 //Four HT1632Cs on one board
#define CLK_DELAY



#define plot(x,y,v)  ht1632_plot(x,y,v)
#define cls          ht1632_clear

#define LONGDELAY 1000  // This delay in setup
#define DEMODELAY 10000  // This delay in setup

// our own copy of the "video" memory; 64 bytes for each of the 4 screen quarters;
// each 64-element array maps 2 planes:
// indexes from 0 to 31 are allocated for green plane;
// indexes from 32 to 63 are allocated for red plane;
// when a bit is 1 in both planes, it is displayed as orange (green + red);

byte ht1632_shadowram[64][4*2] = {0};



/*
 * Set these constants to the values of the pins connected to the SureElectronics Module
 */
static const byte   ht1632_data    = 6;  // Data pin (pin 7 of display connector)
static const byte   ht1632_wrclk   = 2;  // Write clock pin (pin 5 of display connector)
static const byte   ht1632_cs      = 8;  // Chip Select (pin 1 of display connnector)
static const byte   ht1632_clk     = 9;  // clock pin (pin 2 of display connector)



//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

//Function Name: OutputCLK_Pulse
//Function Feature: enable CLK_74164 pin to output a clock pulse
//Input Argument: void
//Output Argument: void

void OutputCLK_Pulse(void) //Output a clock pulse
{
  digitalWrite(ht1632_clk, HIGH);
  digitalWrite(ht1632_clk, LOW);
}


//-----------------------------------------------------------------------
//Function Name: OutputA_74164
//Function Feature: enable pin A of 74164 to output 0 or 1
//Input Argument: x: if x=1, 74164 outputs high. If x?1, 74164 outputs low.
//Output Argument: void

void OutputA_74164(unsigned char x) //Input a digital level to 74164
{
  digitalWrite(ht1632_cs, (x==1 ? HIGH : LOW));
}


//-----------------------------------------------------------------------
//Function Name: ChipSelect
//Function Feature: enable HT1632C
//Input Argument: select: HT1632C to be selected
// If select=0, select none.
// If s<0, select all.
//Output Argument: void

void ChipSelect(int select)
{
  unsigned char tmp = 0;
  if(select<0) //Enable all HT1632Cs
  {
    OutputA_74164(0);
    CLK_DELAY;
    for(tmp=0; tmp<CHIP_MAX; tmp++)
    {
      OutputCLK_Pulse();
    }
  }
  else if(select==0) //Disable all HT1632Cs
  {
    OutputA_74164(1);
    CLK_DELAY;
    for(tmp=0; tmp<CHIP_MAX; tmp++)
    {
      OutputCLK_Pulse();
    }
  }
  else
  {
    OutputA_74164(1);
    CLK_DELAY;
    for(tmp=0; tmp<CHIP_MAX; tmp++)
    {
      OutputCLK_Pulse();
    }
    OutputA_74164(0);
    CLK_DELAY;
    OutputCLK_Pulse();
    CLK_DELAY;
    OutputA_74164(1);
    CLK_DELAY;
    tmp = 1;
    for( ; tmp<select; tmp++)
    {
      OutputCLK_Pulse();
    }
  }
}

//-----------------------------------------------------------------------
/*
 * ht1632_writebits
 * Write bits (up to 8) to h1632 on pins ht1632_data, ht1632_wrclk
 * Chip is assumed to already be chip-selected
 * Bits are shifted out from MSB to LSB, with the first bit sent
 * being (bits & firstbit), shifted till firsbit is zero.
 */
void ht1632_writebits (byte bits, byte firstbit)
{
  // DEBUGPRINT(" ");
  while (firstbit) {
    // DEBUGPRINT((bits&firstbit ? "1" : "0"));
    digitalWrite(ht1632_wrclk, LOW);
    if (bits & firstbit) {
      digitalWrite(ht1632_data, HIGH);
    } 
    else {
      digitalWrite(ht1632_data, LOW);
    }
    digitalWrite(ht1632_wrclk, HIGH);
    firstbit >>= 1;
  }
}


//-----------------------------------------------------------------------
/*
 * ht1632_sendcmd
 * Send a command to the ht1632 chip.
 */
static void ht1632_sendcmd (byte chipNo, byte command)
{
  ChipSelect(chipNo);
  ht1632_writebits(HT1632_ID_CMD, 1<<2);  // send 3 bits of id: COMMMAND
  ht1632_writebits(command, 1<<7);  // send the actual command
  ht1632_writebits(0, 1); 	/* one extra dont-care bit in commands. */
  ChipSelect(0);
}

//-----------------------------------------------------------------------
/*
 * ht1632_senddata
 * send a nibble (4 bits) of data to a particular memory location of the
 * ht1632.  The command has 3 bit ID, 7 bits of address, and 4 bits of data.
 *    Select 1 0 1 A6 A5 A4 A3 A2 A1 A0 D0 D1 D2 D3 Free
 * Note that the address is sent MSB first, while the data is sent LSB first!
 * This means that somewhere a bit reversal will have to be done to get
 * zero-based addressing of words and dots within words.
 */
static void ht1632_senddata (byte chipNo, byte address, byte data)
{
  ChipSelect(chipNo);
  ht1632_writebits(HT1632_ID_WR, 1<<2);  // send ID: WRITE to RAM
  ht1632_writebits(address, 1<<6); // Send address
  ht1632_writebits(data, 1<<3); // send 4 bits of data
  ChipSelect(0);
}

//-----------------------------------------------------------------------

void ht1632_setup()
{
  pinMode(ht1632_cs, OUTPUT);
  digitalWrite(ht1632_cs, HIGH); 	/* unselect (active low) */
  pinMode(ht1632_wrclk, OUTPUT);
  pinMode(ht1632_data, OUTPUT);
  pinMode(ht1632_clk, OUTPUT);

  for (int j=1; j<5; j++)
  {
    ht1632_sendcmd(j, HT1632_CMD_SYSDIS);  // Disable system
    ht1632_sendcmd(j, HT1632_CMD_COMS00);
    ht1632_sendcmd(j, HT1632_CMD_MSTMD); 	/* Master Mode */
    ht1632_sendcmd(j, HT1632_CMD_RCCLK);  // HT1632C
    ht1632_sendcmd(j, HT1632_CMD_SYSON); 	/* System on */
    ht1632_sendcmd(j, HT1632_CMD_LEDON); 	/* LEDs on */
  }
  
  for (byte i=0; i<96; i++)
  {
    ht1632_senddata(1, i, 0);  // clear the display!
    ht1632_senddata(2, i, 0);  // clear the display!
    ht1632_senddata(3, i, 0);  // clear the display!
    ht1632_senddata(4, i, 0);  // clear the display!
  }
  // delay(LONGDELAY);
}


//-----------------------------------------------------------------------
//Function Name: xyToIndex
//Function Feature: get the value of x,y
//Input Argument: x: X coordinate
//                y: Y coordinate
//Output Argument: address of xy

byte xyToIndex(byte x, byte y)
{
  byte nChip, addr;

  if (x>=32) {
    nChip = 3 + x/16 + (y>7?2:0);
  } else {
    nChip = 1 + x/16 + (y>7?2:0);
  }

  x = x % 16;
  y = y % 8;
  addr = (x<<1) + (y>>2);

  return addr;
}


//-----------------------------------------------------------------------
//Function Name: calcBit
//Function Feature: calculate the bitval of y
//Input Argument: y: Y coordinate
//Output Argument: bitval

#define calcBit(y) (8>>(y&3))



//-----------------------------------------------------------------------
//Function Name: get_pixel
//Function Feature: get the value of x,y
//Input Argument: x: X coordinate
//                y: Y coordinate
//Output Argument: color setted on x,y coordinates

int get_pixel(byte x, byte y) {
  byte addr, bitval, nChip;

  if (x>=32) {
    nChip = 3 + x/16 + (y>7?2:0);
  } else {
    nChip = 1 + x/16 + (y>7?2:0);
  }

  addr = xyToIndex(x,y);
  bitval = calcBit(y);

  if ((ht1632_shadowram[addr][nChip-1] & bitval) && (ht1632_shadowram[addr+32][nChip-1] & bitval)) {
    return ORANGE;
  } else if (ht1632_shadowram[addr][nChip-1] & bitval) {
    return GREEN;
  } else if (ht1632_shadowram[addr+32][nChip-1] & bitval) {
    return RED;
  } else {
    return 0;
  }
}

//-----------------------------------------------------------------------
/*
 * plot a point on the display, with the upper left hand corner
 * being (0,0), and the lower right hand corner being (31, 15);
 * parameter "color" could have one of the 4 values:
 * black (off), red, green or yellow;
 */
void ht1632_plot (byte x, byte y, byte color)
{
   byte nChip, addr, bitval;
  
  if (x<0 || x>X_MAX || y<0 || y>Y_MAX)
    return;
  
  if (color != BLACK && color != GREEN && color != RED && color != ORANGE)
    return;
  
  //byte nChip = 1 + x/16 + (y>7?2:0) ;
  
  if (x>=32) {
    nChip = 3 + x/16 + (y>7?2:0);
  } else {
    nChip = 1 + x/16 + (y>7?2:0);
  }
  
  // x = x % 16;
  // y = y % 8;
  
  
  addr = xyToIndex(x,y);
  bitval = calcBit(y);
  
  switch (color)
  {
    case BLACK:
      if (get_pixel(x,y) != BLACK) { // compare with memory to only set if pixel is other color
        // clear the bit in both planes;
        ht1632_shadowram[addr][nChip-1] &= ~bitval;
        ht1632_senddata(nChip, addr, ht1632_shadowram[addr][nChip-1]);
        addr = addr + 32;
        ht1632_shadowram[addr][nChip-1] &= ~bitval;
        ht1632_senddata(nChip, addr, ht1632_shadowram[addr][nChip-1]);
      }
      break;
    case GREEN:
      if (get_pixel(x,y) != GREEN) { // compare with memory to only set if pixel is other color
        // set the bit in the green plane and clear the bit in the red plane;
        ht1632_shadowram[addr][nChip-1] |= bitval;
        ht1632_senddata(nChip, addr, ht1632_shadowram[addr][nChip-1]);
        addr = addr + 32;
        ht1632_shadowram[addr][nChip-1] &= ~bitval;
        ht1632_senddata(nChip, addr, ht1632_shadowram[addr][nChip-1]);
      }
      break;
    case RED:
      if (get_pixel(x,y) != RED) { // compare with memory to only set if pixel is other color
        // clear the bit in green plane and set the bit in the red plane;
        ht1632_shadowram[addr][nChip-1] &= ~bitval;
        ht1632_senddata(nChip, addr, ht1632_shadowram[addr][nChip-1]);
        addr = addr + 32;
        ht1632_shadowram[addr][nChip-1] |= bitval;
        ht1632_senddata(nChip, addr, ht1632_shadowram[addr][nChip-1]);
      }
      break;
    case ORANGE:
      if (get_pixel(x,y) != ORANGE) { // compare with memory to only set if pixel is other color
        // set the bit in both the green and red planes;
        ht1632_shadowram[addr][nChip-1] |= bitval;
        ht1632_senddata(nChip, addr, ht1632_shadowram[addr][nChip-1]);
        addr = addr + 32;
        ht1632_shadowram[addr][nChip-1] |= bitval;
        ht1632_senddata(nChip, addr, ht1632_shadowram[addr][nChip-1]);
      }
      break;
  }
}

//------------------------------------------------------------------

void ht1632_putchar(byte x, byte y, char c, byte color=GREEN)
{
  byte dots;
/*

  if (c >= 'A' && c <= 'Z' ||
    (c >= 'a' && c <= 'z') ) {
    c &= 0x1F;   // A-Z maps to 1-26
  } 
  else if (c >= '0' && c <= '9') {
    c = (c - '0') + 27;
  } 
  else if (c == ' ') {
    c = 0; // space
  }

*/

 if (c == ' ') {c = 0;}
  else if (c == '!') {c = 1;}
  else if (c == '"') {c = 2;}
  else if (c == '#') {c = 3;}
  else if (c == '$') {c = 4;}
  else if (c == '%') {c = 5;}
  else if (c == '&') {c = 6;}
  else if (c == char(39)) {c = 7;}
  else if (c == '(') {c = 8;}
  else if (c == ')') {c = 9;}
  else if (c == '*') {c = 10;}
  else if (c == '+') {c = 11;}
  else if (c == ',') {c = 12;}
  else if (c == '-') {c = 13;}
  else if (c == '.') {c = 14;}
  else if (c == '/') {c = 15;}  
  
  else if (c >= '0' && c <= '9') {
    c = (c - '0') + 16;
  }
  
  else if (c == ':') {c = 26;} 
  else if (c == ';') {c = 27;} 
  else if (c == '<') {c = 28;}
  else if (c == '=') {c = 29;}
  else if (c == '>') {c = 30;}
  else if (c == '?') {c = 31;}
  else if (c == '@') {c = 32;}   
  
  else if (c >= 'A' && c <= 'Z') {
    c = (c - 'A') + 33;
  }
  
  else if (c == '[') {c = 59;}
  //else if (c == '\') {c = 60;}
  else if (c == ']') {c = 61;}
  else if (c == '^') {c = 62;}
  else if (c == '_') {c = 63;}
  else if (c == '`') {c = 64;}
  
  else if (c >= 'a' && c <= 'z') {
    c = (c - 'a') + 65;
  }
  
  else if (c == '{') {c = 91;}
  else if (c == '|') {c = 92;}
  else if (c == '}') {c = 93;}


  for (char col=0; col< 6; col++) {
    dots = pgm_read_byte_near(&myfont[c][col]);
    for (char row=0; row < 7; row++) {
      if (dots & (64>>row))   	     // only 7 rows.
        plot(x+col, y+row, color);
      else 
        plot(x+col, y+row, 0);
    }
  }
}


//-----------------------------------------------------------------------

void clearDisplay()

{
for (byte y=0; y < Y_MAX; y++) for (byte x=0; x < X_MAX; x++) plot(x, y, BLACK);
}

//------------------------------------------------------------------

void displayWeekdayAndDate(byte dwday, byte dday, byte dmonth )

{
  const byte DOf1  = 8;
  const byte DOf22 = 0;  // 2 Digits Tag
  const byte DOf21 = -3;  // 1 Digit Tag
  
  int i;
  int cx;
  
  L1_Ofs = DOf1;

  if (dday < 10) L2_Ofs = DOf21; // only one Digit
  else L2_Ofs = DOf22;           // two digits
     
  switch (dwday) {
    case 1:
      snprintf (L1, LLen, "Dom");  // Domenica
      break;
    case 2:
      snprintf (L1, LLen, "Lun");  // Lunedi
      break;
    case 3:
      snprintf (L1, LLen, "Mar");  // Martedi
      break;
    case 4:
     snprintf (L1, LLen, "Mer");  // Mercoledi
      break;
    case 5:
      snprintf (L1, LLen, "Gio");  // Giovedi
      break;
    case 6:
      snprintf (L1, LLen, "Ven");  // Venerdi
      break;
    case 7:
     snprintf (L1, LLen, "Sab");  // Sabato
      break;
        
    default: 
     snprintf (L1, LLen, "   ");  
    }

  if ((dday >= 1) && (dday <= 31)) 
      {
        snprintf (L2, LLen, "%d",dday);
		if (dday < 10) L2_Ofs = DOf21; // only one Digit
      }
  else snprintf (L2, LLen, "   "); 

i=strlen(L2);
cx=snprintf (L2+i, LLen-i, "."); 
i=i+cx;     

switch (dmonth) {
    case 1:
      snprintf (L2+i, LLen-i, "Gen");  // Gennaio
      break;
    case 2:
      snprintf (L2+i, LLen-i, "Feb");  // Febraio
      break;
    case 3:
      snprintf (L2+i, LLen-i, "Mar");  // Marzo
      break;
    case 4:
      snprintf (L2+i, LLen-i, "Apr");  // Aprile
      break;
    case 5:
      snprintf (L2+i, LLen-i, "Mag");  // Maggio
      break;
    case 6:
      snprintf (L2+i, LLen-i, "Giu");  // Giugno
      break;
    case 7:
      snprintf (L2+i, LLen-i, "Lug");  // Luglio
      break;
    case 8:
      snprintf (L2+i, LLen-i, "Ago");  // Agosto
      break;
    case 9:
      snprintf (L2+i, LLen-i, "Set");  // Settembre
      break;
    case 10:
     snprintf (L2+i, LLen-i, "Ott");  // Ottobre
      break;
    case 11:
      snprintf (L2+i, LLen-i, "Nov");  // Novembre
      break;
    case 12:
      snprintf (L2+i, LLen-i, "Dic");  // Dicembre
      break;
        
    default: 
      snprintf (L2+i, LLen-i, "   "); 
	  break;
    } 

        
  ht1632_putchar(0  + L1_Ofs, 0, L1[0], ORANGE);  // weekday on line 1
  ht1632_putchar(6  + L1_Ofs, 0, L1[1], ORANGE);
  ht1632_putchar(12 + L1_Ofs, 0, L1[2], ORANGE);
  
  byte x;
  
  if ((dday < 10) && (dday > 0)) x=1;
  else 
    {
      x=0;
      ht1632_putchar(0  + L2_Ofs, 9, L2[0], GREEN);   // day on line 2
    }
  ht1632_putchar(6  + L2_Ofs, 9, L2[1-x], GREEN);
  ht1632_putchar(11 + L2_Ofs, 9, L2[2-x], GREEN);
  ht1632_putchar(15 + L2_Ofs, 9, L2[3-x], GREEN);
  ht1632_putchar(21 + L2_Ofs, 9, L2[4-x], GREEN);
  ht1632_putchar(27 + L2_Ofs, 9, L2[5-x], GREEN); 
  

}  

//------------------------------------------------------------------

void displayTempAndTime(int dtemp, byte dhour, byte dmin )

{
    const byte TOf11 = 3;    // one digit temperature
    const byte TOf12 = 6;    //two digits temperature
    const byte TOf2 = 2;
	
	 int i;
     int cx;
    
    if ((dtemp >= 0) && (dtemp < 10)) L1_Ofs = TOf11;
    else L1_Ofs = TOf12;
    
    L2_Ofs = TOf2;    
    
    if ((dtemp < -9) || (dtemp > 99)) snprintf (L1, LLen, "  ");
    else   snprintf (L1, LLen,"%d",dtemp);
    
    i=strlen(L1);
	
	snprintf (L1+i, LLen-i,"%cC",char(96));  // degree sign
    
  
    if ((dhour < 10) && (dhour >0))   snprintf (L2, LLen, "0%d",dhour); 
    else if ((dhour > 23) || (dhour < 1)) snprintf (L2, LLen, "00");
    else  snprintf (L2, LLen, "%d",dhour);
    
    i=strlen(L2);
	cx=snprintf (L2+i, LLen-i, ":");
	i=i+cx;
    
    if ((dmin < 10) && (dmin >0))  cx=snprintf (L2+i, LLen-i, "0%d",dmin);
    else if ((dmin > 59) || (dmin < 1)) cx=snprintf (L2+i, LLen-i, "00");
    else cx=snprintf (L2+i, LLen-i, "%d",dmin);
	i=i+cx;
   
    byte x;
    if ((dtemp >= 0) && (dtemp < 10)) x=1;
    else 
      {
      x=0;
      ht1632_putchar(0  + L1_Ofs, 0, L1[0], RED);   // temperature
      }
    
    
    ht1632_putchar(0  + L1_Ofs, 0, L1[0-x], RED);    // temperature
    ht1632_putchar(6  + L1_Ofs, 0, L1[1-x], RED);
    ht1632_putchar(11 + L1_Ofs, 0, L1[2-x], RED);
    ht1632_putchar(15 + L1_Ofs, 0, L1[3-x], RED);
    
    ht1632_putchar(0  + L2_Ofs, 9, L2[0], GREEN);            // time
    ht1632_putchar(6  + L2_Ofs, 9, L2[1], GREEN);
    ht1632_putchar(12 + L2_Ofs, 9, L2[2], GREEN);
    ht1632_putchar(17 + L2_Ofs, 9, L2[3], GREEN);
    ht1632_putchar(23 + L2_Ofs, 9, L2[4], GREEN);

}  

//------------------------------------------------------------------

void displayPowerDayPro(unsigned int dpwrday )

{
      const byte POf11 = 10;  // 1 Digit
      const byte POf12 = 7;   // 2 Digits
      const byte POf13 = 4;   // 3 Digits
      const byte POf14 = 1;   // 4 Digits
      const byte POf2 = 7;

snprintf (L1, LLen, "%dW",dpwrday);      
snprintf (L2, LLen, "Pro");

if (dpwrday < 10) L1_Ofs = POf11;
else if (dpwrday < 100) L1_Ofs = POf12;
else if (dpwrday < 1000) L1_Ofs = POf13;
else L1_Ofs = POf14;

L2_Ofs = POf2;
byte len1 = strlen(L1);

for (byte i=0; i < len1-1; i++) ht1632_putchar(i*6  + L1_Ofs, 0, L1[i], ORANGE);
ht1632_putchar((len1-1)*6  + L1_Ofs +1, 0, L1[len1-1], GREEN);
      

for (byte i=0; i < strlen(L2); i++) ht1632_putchar(i*6  + L2_Ofs, 9, L2[i], GREEN);

}

//------------------------------------------------------------------

void displayPowerCons(unsigned int dpwrused )

{
      const byte POf11 = 10;  // 1 Digit
      const byte POf12 = 7;   // 2 Digits
      const byte POf13 = 4;   // 3 Digits
      const byte POf14 = 1;   // 4 Digits
      const byte POf2 = 7;

snprintf (L1, LLen, "%dW",dpwrused);      
snprintf (L2, LLen, "Con");

if (dpwrused < 10) L1_Ofs = POf11;
else if (dpwrused < 100) L1_Ofs = POf12;
else if (dpwrused < 1000) L1_Ofs = POf13;
else L1_Ofs = POf14;

L2_Ofs = POf2;
byte len1 = strlen(L1);

for (byte i=0; i < len1-1; i++) ht1632_putchar(i*6  + L1_Ofs, 0, L1[i], RED);
ht1632_putchar((len1-1)*6  + L1_Ofs +1, 0, L1[len1-1], GREEN);
      

for (byte i=0; i < strlen(L2); i++) ht1632_putchar(i*6  + L2_Ofs, 9, L2[i], GREEN);

}

//------------------------------------------------------------------

void displayEnergyDayCons(unsigned long denergydayused )  // energy in Wh

{
      const byte EOf12 = 9;   // x.x Digits
      const byte EOf13 = 5;   // xx.x Digits
      const byte EOf2  = 0;
      
      unsigned long kWh;
      unsigned int oneTenth_kWh;
      
kWh = denergydayused / 1000;
oneTenth_kWh = (denergydayused % 1000) / 100;

if (((denergydayused % 1000) % 100) > 49 ) 
     {
       if (oneTenth_kWh == 9) 
       {
         oneTenth_kWh = 0;
         kWh++;
       }
       else oneTenth_kWh++;
     }
	 
snprintf (L1, LLen, "%ld.%d",kWh,oneTenth_kWh);      
snprintf (L2, LLen, "kWhCon");


if (kWh < 10) 
    {
      L1_Ofs = EOf12;
      ht1632_putchar(0 + L1_Ofs, 0, L1[0], RED);
      ht1632_putchar(5 + L1_Ofs, 0, L1[1], RED);
      ht1632_putchar(9 + L1_Ofs, 0, L1[2], RED);
    }
else 
    {
      L1_Ofs = EOf13;
      ht1632_putchar(0  + L1_Ofs, 0, L1[0], RED);
      ht1632_putchar(6  + L1_Ofs, 0, L1[1], RED);
      ht1632_putchar(11 + L1_Ofs, 0, L1[2], RED);
      ht1632_putchar(15 + L1_Ofs, 0, L1[3], RED);
    }  

L2_Ofs = EOf2;
ht1632_putchar(0  + L2_Ofs, 9, L2[0], GREEN);
ht1632_putchar(5  + L2_Ofs, 9, L2[1], GREEN);
ht1632_putchar(11 + L2_Ofs, 9, L2[2], GREEN);
ht1632_putchar(17 + L2_Ofs, 9, L2[3], GREEN);
ht1632_putchar(22 + L2_Ofs, 9, L2[4], GREEN);
ht1632_putchar(27 + L2_Ofs, 9, L2[5], GREEN);

}

//------------------------------------------------------------------

void displayEnergyDayConsGrid(long denergydayusedGrid )  // energy in Wh

{
      const byte EOf12 = 9;   // x.x Digits
      const byte EOf13 = 5;   // xx.x Digits
      const byte EOf2  = 0;
      
      unsigned long kWh;
      unsigned int oneTenth_kWh;
      
kWh = denergydayusedGrid / 1000;
oneTenth_kWh = (denergydayusedGrid % 1000) / 100;

if (((denergydayusedGrid % 1000) % 100) > 49 ) 
     {
       if (oneTenth_kWh == 9) 
       {
         oneTenth_kWh = 0;
         kWh++;
       }
       else oneTenth_kWh++;
     }
	 
snprintf (L1, LLen, "%ld.%d",kWh,oneTenth_kWh);      
snprintf (L2, LLen, "kWhGrd");


if (kWh < 10) 
    {
      L1_Ofs = EOf12;
      ht1632_putchar(0 + L1_Ofs, 0, L1[0], RED);
      ht1632_putchar(5 + L1_Ofs, 0, L1[1], RED);
      ht1632_putchar(9 + L1_Ofs, 0, L1[2], RED);
    }
else 
    {
      L1_Ofs = EOf13;
      ht1632_putchar(0  + L1_Ofs, 0, L1[0], RED);
      ht1632_putchar(6  + L1_Ofs, 0, L1[1], RED);
      ht1632_putchar(11 + L1_Ofs, 0, L1[2], RED);
      ht1632_putchar(15 + L1_Ofs, 0, L1[3], RED);
    }  

L2_Ofs = EOf2;
ht1632_putchar(0  + L2_Ofs, 9, L2[0], GREEN);
ht1632_putchar(5  + L2_Ofs, 9, L2[1], GREEN);
ht1632_putchar(11 + L2_Ofs, 9, L2[2], GREEN);
ht1632_putchar(17 + L2_Ofs, 9, L2[3], GREEN);
ht1632_putchar(22 + L2_Ofs, 9, L2[4], GREEN);
ht1632_putchar(27 + L2_Ofs, 9, L2[5], GREEN);

}

//------------------------------------------------------------------

void displayEnergyTotalCons(unsigned long denergytotal )  // energy in kilowatthours

{
      const byte ETOf14 = 5; // 4 Digits
      const byte ETOf15 = 1; // 5 Digits
      const byte ETOf2  = 0;

	  
	  
snprintf (L1, LLen, "%ld",denergytotal);      
snprintf (L2, LLen, "kWhToC");
      

if (denergytotal < 10000) L1_Ofs = ETOf14;
else L1_Ofs = ETOf15;  

byte len1 = strlen(L1);
for (byte i=0; i < len1; i++) ht1632_putchar(i*6  + L1_Ofs, 0, L1[i], RED);

L2_Ofs = ETOf2;
ht1632_putchar(0  + L2_Ofs, 9, L2[0], GREEN);
ht1632_putchar(5  + L2_Ofs, 9, L2[1], GREEN);
ht1632_putchar(11 + L2_Ofs, 9, L2[2], GREEN);
ht1632_putchar(17 + L2_Ofs, 9, L2[3], GREEN);
ht1632_putchar(22 + L2_Ofs, 9, L2[4], GREEN);
ht1632_putchar(27 + L2_Ofs, 9, L2[5], GREEN);

}

//------------------------------------------------------------------

void displayEnergyAverageDayCons(unsigned long denergyday )  // energy in watthours

{
      const byte EOf12 = 9;   // x.x Digits
      const byte EOf13 = 5;   // xx.x Digits
      const byte EOf2  = 0;
      
      unsigned long kWh;
      unsigned int oneTenth_kWh;
      
kWh = denergyday / 1000;
oneTenth_kWh = (denergyday % 1000) / 100;

if (((denergyday % 1000) % 100) > 49 ) 
     {
       if (oneTenth_kWh == 9) 
       {
         oneTenth_kWh = 0;
         kWh++;
       }
       else oneTenth_kWh++;
     }

snprintf (L1, LLen, "%ld.%d",kWh,oneTenth_kWh);      
snprintf (L2, LLen, "kWhAdC");


if (kWh < 10) 
    {
      L1_Ofs = EOf12;
      ht1632_putchar(0 + L1_Ofs, 0, L1[0], RED);
      ht1632_putchar(5 + L1_Ofs, 0, L1[1], RED);
      ht1632_putchar(9 + L1_Ofs, 0, L1[2], RED);
    }
else 
    {
      L1_Ofs = EOf13;
      ht1632_putchar(0  + L1_Ofs, 0, L1[0], RED);
      ht1632_putchar(6  + L1_Ofs, 0, L1[1], RED);
      ht1632_putchar(11 + L1_Ofs, 0, L1[2], RED);
      ht1632_putchar(15 + L1_Ofs, 0, L1[3], RED);
    }  

L2_Ofs = EOf2;
ht1632_putchar(0  + L2_Ofs, 9, L2[0], GREEN);
ht1632_putchar(5  + L2_Ofs, 9, L2[1], GREEN);
ht1632_putchar(11 + L2_Ofs, 9, L2[2], GREEN);
ht1632_putchar(17 + L2_Ofs, 9, L2[3], GREEN);
ht1632_putchar(22 + L2_Ofs, 9, L2[4], GREEN);
ht1632_putchar(27 + L2_Ofs, 9, L2[5], GREEN);

}


//------------------------------------------------------------------

void displayPowerDayPeakPro(unsigned int dpwrdaypk )

{

      const byte POf11 = 10;   // 1 Digit
      const byte POf12 = 7;   // 2 Digits
      const byte POf13 = 4;   // 3 Digits
      const byte POf14 = 1;   // 4 Digits
      const byte POf2 = 2;


snprintf (L1, LLen, "%dW",dpwrdaypk);      
snprintf (L2, LLen, "DayPk");
      

if (dpwrdaypk < 10) L1_Ofs = POf11;
else if (dpwrdaypk < 100) L1_Ofs = POf12;
else if (dpwrdaypk < 1000) L1_Ofs = POf13;
else L1_Ofs = POf14;

L2_Ofs = POf2;
byte len1 = strlen(L1);

for (byte i=0; i < len1-1; i++) ht1632_putchar(i*6  + L1_Ofs, 0, L1[i], ORANGE);
ht1632_putchar((len1-1)*6  + L1_Ofs +1, 0, L1[len1-1], GREEN);
      

for (byte i=0; i < strlen(L2); i++) ht1632_putchar(i*6  + L2_Ofs, 9, L2[i], GREEN);

}


//------------------------------------------------------------------

void displayEnergyDayPro(unsigned long denergyday )  // energy in watthours

{
      const byte EOf12 = 9;   // x.x Digits
      const byte EOf13 = 5;   // xx.x Digits
      const byte EOf2  = 0;
      
      unsigned long kWh;
      unsigned int oneTenth_kWh;
      
kWh = denergyday / 1000;
oneTenth_kWh = (denergyday % 1000) / 100;

if (((denergyday % 1000) % 100) > 49 ) 
     {
       if (oneTenth_kWh == 9) 
       {
         oneTenth_kWh = 0;
         kWh++;
       }
       else oneTenth_kWh++;
     }

snprintf (L1, LLen, "%ld.%d",kWh,oneTenth_kWh);      
snprintf (L2, LLen, "kWhPro");


if (kWh < 10) 
    {
      L1_Ofs = EOf12;
      ht1632_putchar(0 + L1_Ofs, 0, L1[0], ORANGE);
      ht1632_putchar(5 + L1_Ofs, 0, L1[1], ORANGE);
      ht1632_putchar(9 + L1_Ofs, 0, L1[2], ORANGE);
    }
else 
    {
      L1_Ofs = EOf13;
      ht1632_putchar(0  + L1_Ofs, 0, L1[0], ORANGE);
      ht1632_putchar(6  + L1_Ofs, 0, L1[1], ORANGE);
      ht1632_putchar(11 + L1_Ofs, 0, L1[2], ORANGE);
      ht1632_putchar(15 + L1_Ofs, 0, L1[3], ORANGE);
    }  

L2_Ofs = EOf2;
ht1632_putchar(0  + L2_Ofs, 9, L2[0], GREEN);
ht1632_putchar(5  + L2_Ofs, 9, L2[1], GREEN);
ht1632_putchar(11 + L2_Ofs, 9, L2[2], GREEN);
ht1632_putchar(17 + L2_Ofs, 9, L2[3], GREEN);
ht1632_putchar(22 + L2_Ofs, 9, L2[4], GREEN);
ht1632_putchar(27 + L2_Ofs, 9, L2[5], GREEN);

}

//------------------------------------------------------------------

void displayEnergyTotalPro(unsigned long denergytotal )  // energy in kilowatthours

{
      const byte ETOf14 = 5; // 4 Digits
      const byte ETOf15 = 1; // 5 Digits
      const byte ETOf2  = 0;

snprintf (L1, LLen, "%ld",denergytotal);      
snprintf (L2, LLen, "kWhToP");
      

if (denergytotal < 10000) L1_Ofs = ETOf14;
else L1_Ofs = ETOf15;  

byte len1 = strlen(L1);
for (byte i=0; i < len1; i++) ht1632_putchar(i*6  + L1_Ofs, 0, L1[i], ORANGE);

L2_Ofs = ETOf2;
ht1632_putchar(0  + L2_Ofs, 9, L2[0], GREEN);
ht1632_putchar(5  + L2_Ofs, 9, L2[1], GREEN);
ht1632_putchar(11 + L2_Ofs, 9, L2[2], GREEN);
ht1632_putchar(17 + L2_Ofs, 9, L2[3], GREEN);
ht1632_putchar(22 + L2_Ofs, 9, L2[4], GREEN);
ht1632_putchar(27 + L2_Ofs, 9, L2[5], GREEN);

}

//------------------------------------------------------------------

void displayEnergyAverageDayPro(unsigned long denergyday )  // energy in watthours

{
      const byte EOf12 = 9;   // x.x Digits
      const byte EOf13 = 5;   // xx.x Digits
      const byte EOf2  = 0;
      
      unsigned long kWh;
      unsigned int oneTenth_kWh;
      
kWh = denergyday / 1000;
oneTenth_kWh = (denergyday % 1000) / 100;

if (((denergyday % 1000) % 100) > 49 ) 
     {
       if (oneTenth_kWh == 9) 
       {
         oneTenth_kWh = 0;
         kWh++;
       }
       else oneTenth_kWh++;
     }

snprintf (L1, LLen, "%ld.%d",kWh,oneTenth_kWh);      
snprintf (L2, LLen, "kWhAdP");


if (kWh < 10) 
    {
      L1_Ofs = EOf12;
      ht1632_putchar(0 + L1_Ofs, 0, L1[0], ORANGE);
      ht1632_putchar(5 + L1_Ofs, 0, L1[1], ORANGE);
      ht1632_putchar(9 + L1_Ofs, 0, L1[2], ORANGE);
    }
else 
    {
      L1_Ofs = EOf13;
      ht1632_putchar(0  + L1_Ofs, 0, L1[0], ORANGE);
      ht1632_putchar(6  + L1_Ofs, 0, L1[1], ORANGE);
      ht1632_putchar(11 + L1_Ofs, 0, L1[2], ORANGE);
      ht1632_putchar(15 + L1_Ofs, 0, L1[3], ORANGE);
    }  

L2_Ofs = EOf2;
ht1632_putchar(0  + L2_Ofs, 9, L2[0], GREEN);
ht1632_putchar(5  + L2_Ofs, 9, L2[1], GREEN);
ht1632_putchar(11 + L2_Ofs, 9, L2[2], GREEN);
ht1632_putchar(17 + L2_Ofs, 9, L2[3], GREEN);
ht1632_putchar(22 + L2_Ofs, 9, L2[4], GREEN);
ht1632_putchar(27 + L2_Ofs, 9, L2[5], GREEN);

}


//------------------------------------------------------------------

void displayL1andL2(char lin1[], byte colorL1, char lin2[], byte colorL2)

{
    const byte strt = 1;     // Start pixel offset
    
    const byte chrwidth = 6; // pixel size of one char
    const byte chrOfs = 3;   // half size of one char
    
    const byte maxchar = 5;  // maximum number of char per line
  
    byte len1;  // number of char on line 1
    byte len2;  // number of char on line 2 
    byte L1Ofs,L2Ofs; 
    
len1 = constrain(strlen(lin1), 0, maxchar);
L1Ofs = (maxchar - len1)* chrOfs;

len2 = constrain(strlen(lin2), 0, maxchar);
L2Ofs = (maxchar - len2)* chrOfs;;
      
for (byte i=0; i < len1; i++) ht1632_putchar((strt + i*chrwidth + L1Ofs), 0, lin1[i], colorL1);
for (byte i=0; i < len2; i++) ht1632_putchar((strt + i*chrwidth + L2Ofs), 9, lin2[i], colorL2);      

}


//-----------------------------------------------------------------------
//----------------------------------------------------------------------- 

