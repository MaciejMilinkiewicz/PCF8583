/*
  Implements a simple interface to the time function of the PCF8583 RTC chip

  Works around the device's limited year storage by keeping the year in the
  first two bytes of user accessible storage

  Assumes device is attached in the standard location - Analog pins 4 and 5
  Device address is the 8 bit address (as in the device datasheet - normally A0)

  Copyright (c) 2009, Erik DeBill


  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <Arduino.h>
#include <Wire.h>
#include "PCF8583.h"

namespace {
  bool IsLeapYear(int year) {
    return !(year % 400) || ((year % 100) && !(year % 4));
  }

  byte DayOfWeek(const PCF8583 &now) {
	  
/***********************************************
IDE 1.5.7 and above have a new compiler which requires PROGMEM data to be const. Every other detail regarding PROGMEM is fine.

Always use the version with the PROGMEM after the declaration ( and before the initialization ), and avoid the prog_xxx_t types ( they 
add PROGMEM before the id) for maximum compatibility. And add const to your declarations now, so when you upgrade or someone else uses 
your code, it will work without a hitch.

Code: [Select]
int var PROGMEM = 1234;
char pgmArr[] PROGMEM = "foo";
	  
***********************************************/
//static char PROGMEM MonthTable[24] = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5, -1, 2, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};
	char  MonthTable[24] PROGMEM = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5, -1, 2, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};
    byte y = now.year % 100, c = 6 - 2 * ((now.year / 100) % 4); 
    return (now.day + pgm_read_byte_near(MonthTable + IsLeapYear(now.year) * 12 + now.month - 1) + y + (y / 4) + c) % 7;
  }
}

// provide device address as a full 8 bit address (like the datasheet)
PCF8583::PCF8583(int device_address) {
  address = device_address >> 1;  // convert to 7 bit so Wire doesn't choke
  Wire.begin();
}

// initialization 
void PCF8583::init()
{

Wire.beginTransmission(address);
Wire.write(0x00);
Wire.write(0x04);   // Set alarm on int\ will turn to vcc
Wire.endTransmission();

}

void PCF8583::get_time(){

  Wire.beginTransmission(address);
  Wire.write(0xC0);   // stop counting, don't mask
  Wire.endTransmission();

  Wire.beginTransmission(address);
  Wire.write(0x02);
  Wire.endTransmission();
  Wire.requestFrom(address, 5);

 // second = bcd_to_byte(Wire.read());
 // minute = bcd_to_byte(Wire.read());
 // hour   = bcd_to_byte(Wire.read());
 
 //month(4)+day(5)+hour(5)+minute(6)=20 bits you can add 4bits more for 15 years span making 24 bits (3 bytes)
 // or we can use day(5)+hour(5)+minute(6)=16 bits always
 //below lines added by asim
 
 // second = Wire.read(); //not used
 second = bcd_to_byte(Wire.read());
  minutec = Wire.read();
  hourc   = Wire.read();
// second = bcd_to_byte(Wire.read());
  minute = bcd_to_byte(minutec);
  hour   = bcd_to_byte(hourc);
 


  
 //above lines added by asim
  
  byte incoming = Wire.read(); // year/date counter
  //asim start

  //timeshort=((incoming & 0x3f)<< 11)+((hourc & 0x1F)<< 6)+(minutec & 0x3F);
  timeshort=((incoming & 0x1f)<< 11)+((hourc & 0x1F)<< 6)+(minutec & 0x3F);
  //asim end
  
  day    = bcd_to_byte(incoming & 0x3f);
  year   = (int)((incoming >> 6) & 0x03);      // it will only hold 4 years...
  incoming = Wire.read();
  month  = bcd_to_byte(incoming & 0x1f);
  dow = incoming >> 5;

  //  but that's not all - we need to find out what the base year is
  //  so we can add the 2 bits we got above and find the real year
  Wire.beginTransmission(address);
  Wire.write(0x10);
  Wire.endTransmission();
  Wire.requestFrom(address, 2);
  year_base = 0;
  year_base = Wire.read();
  year_base = year_base << 8;
  year_base = year_base | Wire.read();
  year = year + year_base;
}


void PCF8583::set_time()
 {

  if (!IsLeapYear(year) && 2 == month && 29 == day) {
    month = 3;
    day = 1;
  }

  // Attempt to find the previous leap year
  year_base = year - year % 4;
  if (!IsLeapYear(year_base)) {
    // Not a leap year (new century), make sure the calendar won't use a 29 days February.
    year_base = year - 1;
  }

  dow = DayOfWeek(*this);

  Wire.beginTransmission(address);
  Wire.write(0xC0);   // stop counting, don't mask
  Wire.endTransmission();

  Wire.beginTransmission(address);
  Wire.write(0x02);
  Wire.write(int_to_bcd(second));
  Wire.write(int_to_bcd(minute));
  Wire.write(int_to_bcd(hour));
  Wire.write(((byte)(year - year_base) << 6) | int_to_bcd(day));
  Wire.write((dow << 5) | (int_to_bcd(month) & 0x1f));
  Wire.endTransmission();

  Wire.beginTransmission(address);
  Wire.write(0x10);
  Wire.write(year_base >> 8);
  Wire.write(year_base & 0x00ff);
  Wire.endTransmission();
  
  init(); // re set the control/status register to 0x04

  }

//Get the alarm at 0x09 adress
void PCF8583::get_alarm()
{
Wire.beginTransmission(address);
Wire.write(0x0A); // Set the register pointer to (0x0A) 
Wire.endTransmission();

Wire.requestFrom(address, 4); // Read 4 values 

alarm_second = bcd_to_byte(Wire.read());
alarm_minute = bcd_to_byte(Wire.read());
alarm_hour   = bcd_to_byte(Wire.read());

Wire.beginTransmission(address);
Wire.write(0x0E); 
Wire.endTransmission();

Wire.requestFrom(address, 1); // Read weekday value 

alarm_day = bcd_to_byte(Wire.read());
}

//Set a daily alarm
void PCF8583::set_daily_alarm()
{
Wire.beginTransmission(address);
Wire.write(0x08); 
Wire.write(0x90);  // daily alarm set 
Wire.endTransmission();

Wire.beginTransmission(address);
Wire.write(0x09); // Set the register pointer to (0x09)
Wire.write(0x00); // Set 00 at milisec 
Wire.write(int_to_bcd(alarm_second));
Wire.write(int_to_bcd(alarm_minute));
Wire.write(int_to_bcd(alarm_hour));
Wire.write(0x00); // Set 00 at day 
Wire.endTransmission();
}

int PCF8583::bcd_to_byte(byte bcd){
  return ((bcd >> 4) * 10) + (bcd & 0x0f);
}

byte PCF8583::int_to_bcd(int in){
  return ((in / 10) << 4) + (in % 10);
}

unsigned long int PCF8583::makeTime(){   
// assemble time elements into time_t 
// note year argument is offset from 1970 (see macros in time.h to convert to other formats)
// previous version used full four digit year (or digits since 2000),i.e. 2009 was 2009 or 9
  static  const int monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31}; // asim
  int i;
  uint32_t tseconds;

  // seconds from 1970 till 1 jan 00:00:00 of the given year
  tseconds= (year-1970)*(SECS_PER_DAY * 365);
  for (i = 1970; i < year-1; i++) {
    if (IsLeapYear(i)) {
      tseconds +=  SECS_PER_DAY;   // add extra days for leap years
    }
  }
  
  // add days for this year, months start from 1
  for (i = 1; i < month; i++) {
    if ( (i == 2) && IsLeapYear(year)) { 
      tseconds += SECS_PER_DAY * 29;
    } else {
      tseconds += SECS_PER_DAY * monthDays[i-1];  //monthDay array starts from 0
    }
  }
  tseconds+= (day-1) * SECS_PER_DAY;
  tseconds+= hour * SECS_PER_HOUR;
  tseconds+= minute * SECS_PER_MIN;
  tseconds+= second;
 // tseconds-=2208988800;
  return tseconds; 
}