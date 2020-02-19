#include "Adafruit_PS2_Trackpad_NoBlocking.h"
#include "Adafruit_MPRLS_AllThumbs.h"
#include <EEPROM.h>

/*************************************************** 
  This is an example for the Adafruit Capacitive Trackpad

  Designed specifically to work with the Adafruit Capacitive Trackpad 
  ----> https://www.adafruit.com/products/837

  These devices use PS/2 to communicate, 2 pins are required to  
  interface
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/
 
/*
We suggest using a PS/2 breakout adapter such as this
https://www.adafruit.com/products/804
for easy wiring!

PS/2 pin #1 (Brown)  - Data 
PS/2 pin #2 (White)  - N/C
PS/2 pin #3 (Black)  - Ground
PS/2 pin #4 (Green)  - 5V
PS/2 pin #5 (Yellow) - Clock
PS/2 pin #6 (Red)    - N/C
*/

/*
   Rob Yapchanyk
   2020

   The purpose of this sketch is to transmit trackpad and pressure events to an external
   program over serial. The primary goal was to create a custom solution for someone with
   a disability only allowing movement of their thumb to improve PC accessibility. The
   pressure sensor is to support Sip & Puff functionality via oral tube.

   This code incorporates Adafruit's MPRLS ported sensor & code:
   https://www.adafruit.com/product/3965

   It changes the read() method of the original trackpad code to make it non-blocking.
   This allows us to poll the trackpad, MPRLS sensor, and watch for any inbound serial
   data to report/react accordingly in the main loop().

   See below for PS/2 Data/Clock pins, which it seems needed to be moved to those instead 
   of the example code once the MPRLS (I2C) module was added to the bus.

   The purpose of this sketch is to transmit trackpad and pressure sensor data via serial
   to an application which handles the data accordingly. In the original scope, this 
   program was a WinForms (C#) app that translated trackpad coordinates to mouse movements
   and events depending on where the touch happened on the trackpad.

   MPRLS data is transmitted based on pressure thresholds, this is to support Sip & Puff
   data which can be leveraged by the receiving program and be translated into events.
*/

// PS2 uses two digital pins
#define PS2_DATA A2
#define PS2_CLK A3

// 'absolute' tablet mode
Adafruit_PS2_Trackpad ps2(PS2_CLK, PS2_DATA);

// You dont *need* a reset and EOC pin for most uses, so we set to -1 and don't connect
#define RESET_PIN  -1  // set to any GPIO pin # to hard-reset on begin()
#define EOC_PIN    -1  // set to any GPIO pin to read end-of-conversion by pin
Adafruit_MPRLS mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN);

const char* initCommand = "RQ_INIT_";
const char* sipSetThresholdCommand = "RQ_SETSIPTHRESH_";
const char* puffSetThresholdCommand = "RQ_SETPUFFTHRESH_";
void ReadTouchpadData(uint8_t clockWaits = 0);
const int sipThresholdAddr = 0; //Sip threshold setting EEPROM address
const int puffThresholdAddr = 1; //Puff threshold setting EEPROM address
const int sipDefaultThreshold = 10;
const int puffDefaultThreshold = 5;
int sipThreshold = 10; //Amount of pressure (hPa) below spBaseValue needed to trigger sending a sip event
int puffThreshold = 5; //Amount of pressure (hPa) above spBaseValue needed to trigger sending a puff event
float spBaseValue = 0; //Set upon init, used to determine if the current event is a sip or a puff
bool spPresent = false;

void setup() {
  
  Serial.begin(115200);

  Serial.println("Locating MPRLS (pressure) sensor...");
  if (! mpr.begin()) {
    Serial.println("Failed to communicate with MPRLS (pressure) sensor");
    Serial.println("Sip and Puff functionality not available");
    spPresent = false;
  } 
  else
  {
    Serial.println("Found MPRLS (pressure) sensor");
    spPresent = true;
    spBaseValue = GetSipPuffBaseValue();
    sipThreshold = GetSipThreshold();
    puffThreshold = GetPuffThreshold();
  }  
  
  if (ps2.begin()) 
  {
    Serial.println("Successfully found PS2 mouse device");
    Serial.print("PS/2 Mouse with ID 0x");
    Serial.println(ps2.readID(), HEX);

    Serial.println("Ready");
  }
  else
  {
    Serial.println("Did not find PS2 mouse device");
  }

}

void loop() {

  //Only parses one pipe terminated command per call
  CheckForReceiveData();

  //Checks to see if the sensor is reporting the current pressure is +/- the corresponding threshold
  CheckForSipPuffEvent();

  //Waits up the number of clock high wait while loops before 
  //returning and continuing main loop (max 254, 0 to disable [blocking wait])
  ReadTouchpadData(254);
}

float ReadPressure()
{
  if(!spPresent) return 0.00;
  float pressure_hPa = mpr.readPressure();
  return pressure_hPa;
}

void CheckForSipPuffEvent()
{
  float currentPressure = ReadPressure();
  if(currentPressure == 0.00) return;
  
  if(currentPressure >= (puffThreshold + spBaseValue))
  {
    Serial.print("P|");
    Serial.println(currentPressure, 2);
  }
  else if (currentPressure <= (spBaseValue - sipThreshold))
  {
    Serial.print("S|");
    Serial.println(currentPressure, 2);
  }
}

float GetSipPuffBaseValue()
{
  //Read sensor for current pressure
  float currentPressure = ReadPressure();
  Serial.print("Baseline pressure: ");
  Serial.print(currentPressure, 2);
  Serial.println(" (hPa)");
  return currentPressure;
}

int GetSipThreshold()
{
  int tempSipThreshold = EEPROM.read(sipThresholdAddr);
  if(tempSipThreshold > 254 || tempSipThreshold <= 0)
  {
    EEPROM.update(sipThresholdAddr,sipDefaultThreshold); 
    tempSipThreshold = sipDefaultThreshold;
  }
  Serial.print("Current Sip pressure threshold (+/-): ");
  Serial.print(tempSipThreshold, DEC);
  Serial.println(" (hPa)");
  return tempSipThreshold;
}

int GetPuffThreshold()
{
  int tempPuffThreshold = EEPROM.read(puffThresholdAddr);
  if(tempPuffThreshold > 254 || tempPuffThreshold <= 0)
  {
    EEPROM.update(puffThresholdAddr,puffDefaultThreshold); 
    tempPuffThreshold = puffDefaultThreshold;
  }
  Serial.print("Current Puff pressure threshold (+/-): ");
  Serial.print(tempPuffThreshold, DEC);
  Serial.println(" (hPa)");
  return tempPuffThreshold;
}

void ReadTouchpadData(uint8_t clockWaits)
{
  if (ps2.readData(clockWaits))
  {
    Serial.print(ps2.x, DEC);
    Serial.print(",");
    Serial.print(ps2.y, DEC);
    Serial.print(",");
    Serial.print(ps2.z, DEC);
    Serial.print("|");
    if (ps2.finger) Serial.print("F");
    if (ps2.right) Serial.print("R");
    if (ps2.left) Serial.print("L");
    if (ps2.gesture) Serial.print("G");  

    Serial.println();
  }
}

void CheckForReceiveData()
{
  //This function will only parse one command token then return
  //Make sure responses fo not contain commas as client will encounter 
  //an exception attempting to parse x,y,z
  
  //If there is data waiting to be received
  if(Serial.available() > 0)
  {
    delay(50);
    const int maxBytes = 110; //Safeguard to break out of the loop
    int totalBytes = 0;
    int makeSerialStringPosition = 0;
    int inByte;
    char serialReadString[50];
    const int terminatingChar = 124; //pipe (|)    
    inByte = Serial.read();

    //
    if (inByte > 0 && inByte != terminatingChar)
    {
      while (inByte != terminatingChar && Serial.available() > 0)
      {
        serialReadString[makeSerialStringPosition] = inByte; 
        makeSerialStringPosition++; 
        
        //If the incoming string exceeds the limit before finding the terminating char
        //reset the char array and send an error message
        if(makeSerialStringPosition > 49)
        {
          memset(serialReadString, 0, sizeof(serialReadString));
          makeSerialStringPosition = 0;  
          Serial.print("RQE|LINE_LENGTH_EXCEEDED: Reset and continue reading from postition ");        
          Serial.println(totalBytes, DEC);
        }

        //Get the next byte
        inByte = Serial.read(); 
        totalBytes++;
        
        //Too many bytes without finding a terminating char, exit
        if(totalBytes >= maxBytes)
        {
          Serial.print("RQE|MAX_BYTES_EXCEEDED: Discarding bytes 0-");
          Serial.println(totalBytes, DEC);
          return;
        }
      }

      //If the last char is the terminating, else all available bytes have been read
      if (inByte == terminatingChar) 
      {
        serialReadString[makeSerialStringPosition] = 0; 

        //Check for known commands
        if (strcmp(serialReadString, "RQ_ECHO") == 0)
        {
          Serial.println("RQS|HI");
        }
        else if (strstr(serialReadString, sipSetThresholdCommand))
        {
          String sipThreshData = serialReadString;
          sipThreshData.replace("RQ_SETSIPTHRESH_", "");
          int tempSipThreshold = sipThreshData.toInt();
          if (tempSipThreshold <= 0 || tempSipThreshold > 254)
          {
            Serial.println("RQE|Invalid value - must be 1-254");
          }
          else
          {
            sipThreshold = tempSipThreshold;
            EEPROM.update(sipThresholdAddr,sipThreshold); 
            Serial.println("RQS|OK");
          }
        }   
        else if (strstr(serialReadString, puffSetThresholdCommand))
        {
          String puffThreshData = serialReadString;
          puffThreshData.replace("RQ_SETPUFFTHRESH_", "");
          int tempPuffThreshold = puffThreshData.toInt();
          if (tempPuffThreshold <= 0 || tempPuffThreshold > 254)
          {
            Serial.println("RQE|Invalid value - must be 1-254");
          }
          else
          {
            puffThreshold = tempPuffThreshold;
            EEPROM.update(puffThresholdAddr,puffThreshold); 
            Serial.println("RQS|OK");
          }
        }  
        else if (strcmp(serialReadString, "RQ_GETSIPTHRESH") == 0)
        {
          Serial.print("RQS|");
          Serial.println(sipThreshold, DEC);
        }  
        else if (strcmp(serialReadString, "RQ_GETPUFFTHRESH") == 0)
        {
          Serial.print("RQS|");
          Serial.println(puffThreshold, DEC);
        }          
        else if (strstr(serialReadString, initCommand))
        {
          String initData = serialReadString;
          initData.replace("RQ_INIT_", "");

          //Do something with initData if needed
          //Not currently implemented
          //Serial.println(initData);
          
          Serial.print("RQS|OK");
        }
        else
        {
          Serial.print("RQE|UNKNOWN_COMMAND: ");
          Serial.println(serialReadString);
        }
      }
      else
      {
        //End of available bytes, but no terminating char found
        serialReadString[makeSerialStringPosition] = inByte;
        serialReadString[makeSerialStringPosition+1] = 0;
        Serial.print("RQE|TERMINATING_CHAR_MISSING: ");
        Serial.println(serialReadString);
      }
    }
  }  
}
