/*  signalMapper is a quick and dirty geologger for mobile network signals.
While Honduras ProParque staff were selecting sites for pilot installations
of our Hidrosonico stream gauge (https://github.com/DAI-Maker-Lab/hidrosonico),
one issue they ran into was finding sites that were suitable geographically
but which also had a GSM signal so that the unit could send its data home. 

With signalMapper, teams can just activate the unit and carry it with 
them as they go about their business, and then cut and paste signalMapper's
data into a service like CartoDB and generate a heatmap of GSM network
signal strength, which they can then use to cross-reference against other 
site selection criteria. Not only does this ease the survey process, but since
signalMapper uses similar hardware to the Hidrosonico and other DAI Maker Lab
sensor units (i.e., the Adafruit FONA, Seeeduino Stalker v3 and whatever antenna 
configuration is selected), it should be more predictive of performance than a 
team member's individual mobile phone.

The remainder of this code is released under the MIT License. You are free to use 
and change this code as you like, provided you retain attribution to Development
Alternatives, Inc. Non-code portions of the project are made available under
the Creative Commons-Attribution (CC BY) license.

If you use it, please let us know (robert_ryan-silva[at]dai.com), and if you 
improve it, please share! */



#include <SPI.h>              //    SD card datalogging requires SPI
#include <SD.h>               //    SD card library
#include <SoftwareSerial.h>   //    For FONA
#include <Adafruit_FONA.h>
#include <Sleep_n0m1.h>       //    Sleep library

//    FONA pins
#define FONA_RST A0
#define FONA_RX A1
#define FONA_TX A2
#define FONA_KEY A3
#define FONA_PS A4

#define dataLogger 10        //    SD datalogger pin on the Stalker

//    LEDs to indicate operating status to user
#define redLED 3             //    Indicates no GPS fix
#define greenLED 4           //    Indicates GPS fix
#define blueLED 5            //    Indicates usable GSM signal

/* The "on/off" switch does not actually cut power to the unit; when flipped
to the "off" position, it triggers code in the loop that turns off the LEDs and
shuts down the FONA in an orderly manner, then puts the microcontroller to sleep.
When switched "on", the rising voltage triggers an interrupt that wakes the 
microcontroller up, and the FONA is booted. This means that some power is used
even when the unit is off, but it allows for the orderly shutdown of the FONA
and SD logger -- so you don't accidentally corrupt the log by shutting it down
mid-write -- and allows the battery to be charged through the FONA's charge
circuit even if the unit is "off". */

#define sleepPin 2           //    Pin leading to the "on/off" switch
#define sleepInt 0           //    Interrupt associated with the sleepPin

//    New readings are triggered by time rather than distance travelled
unsigned long nextRead = 0;
int readInterval = 30000;    //    The number of milliseconds between readings

boolean wasAsleep = false;

SoftwareSerial fonaSerial = SoftwareSerial (FONA_TX, FONA_RX);
Adafruit_FONA fona = Adafruit_FONA (FONA_RST);

File mapData;                //    Define the SD card file

Sleep sleep;                 //    Create the sleep object



static void onOff()
{
        //    Blank IRQ, just to wake up microcontroller
}



void setup() 
{
        Serial.begin (115200);
                
        pinMode (FONA_RST, OUTPUT);
        pinMode (FONA_RX, OUTPUT);
        pinMode (FONA_KEY, OUTPUT);        
        pinMode (redLED, OUTPUT);
        pinMode (greenLED, OUTPUT);
        pinMode (blueLED, OUTPUT);
        pinMode (sleepPin, INPUT);
        
        //    Light all three LEDs to indicate startup
        digitalWrite (redLED, HIGH);
        digitalWrite (greenLED, HIGH);
        digitalWrite (blueLED, HIGH);
        
        //    Initiate the SD card logger
        if (SD.begin (dataLogger) == false) 
        {
                Serial.println (F("Card failed, or not present"));
        }
        
        //    If the data file does not exist already, create it
        if (SD.exists ("mapData.csv") == false)
        {
                Serial.println (F("Creating mapData.csv."));
                File mapData = SD.open ("mapData.csv", FILE_WRITE);
                mapData.println (F("UTC,latitude,longitude,RSSI"));
                mapData.close();
                
                if (SD.exists ("mapData.csv") == true)
                {
                        Serial.println (F("mapData.csv created."));
                }
                else
                { 
                        Serial.println (F("Failed to create mapData.csv."));
                }
        }
        else
        {
                //    If the file exists, send it over the Serial port for cut and paste
                mapData = SD.open ("mapData.csv");
                Serial.println (F("mapData:"));
                Serial.println (F("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - "));
                while (mapData.available())
                {
                        Serial.write (mapData.read());
                }
                Serial.println (F("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - "));
                mapData.close();
        }
        
        fonaStartup();
        
        //    Initiate the interrupt
        attachInterrupt (sleepInt, onOff, RISING);
        interrupts();
}



void loop() 
{
        /* If the FONA is off and the "on/off" switch is on -- probably because
       we just turned the unit on -- start the FONA */
       
        if (digitalRead (FONA_PS) == LOW && digitalRead (sleepPin) == HIGH)
        {
                Serial.println (F("Waking up."));
                fonaStartup();
        }
  
        boolean fix;
        
        if (fona.GPSstatus() >= 2)
        {
                fix = true;
        }
        else
        {
                fix = false;
        }
        
        if (millis() >= nextRead && fix == true)    
        //    If it's time for a reading and we have a fix...
        {
                logRSSI();        //    Log the reading...
                nextRead = millis() + readInterval;    
                                  //    ...and set the time for the next reading
        }
        
        if (fix == true)
        {
                digitalWrite (redLED, LOW);
                digitalWrite (greenLED, HIGH);
        }
        else
        {
                digitalWrite (redLED, HIGH);
                digitalWrite (greenLED, LOW);
        }
        
        // If the "on/off" switch is off, we shut the system down.
        if (digitalRead (sleepPin) == LOW)
        {
                digitalWrite (redLED, LOW);
                digitalWrite (greenLED, LOW);
                digitalWrite (blueLED, LOW);
                
                wasAsleep = true;
                
                Serial.println (F("Sleeping..."));
                Serial.flush();
                
                stopFona();
                
                sleep.pwrDownMode();
                sleep.sleepInterrupt (0, RISING);
        }
        else
        {
                wasAsleep = false;
        }
        
        //    If the system was just asleep, we turn on the LEDs to indicate it's now awake
        if (wasAsleep == true)
        {
                digitalWrite (redLED, HIGH);
                digitalWrite (greenLED, HIGH);
                digitalWrite (blueLED, HIGH);
                
                wasAsleep = false;
        }
}



void fonaStartup()
{
        byte bootAttempts = 0;
        
        //    We'll try to boot the Fona up to three times
        while (bootFona() == false && bootAttempts < 2)
        {
                delay (500);
                bootFona();
                bootAttempts++;
        }

        delay (500);
        fona.enableGPS(true);
}



boolean bootFona()
{
	// Power up the FONA if it needs it
	if (digitalRead(FONA_PS) == LOW)
	{
		Serial.print(F("Powering FONA on..."));
		while (digitalRead(FONA_PS) == LOW)
		{
			digitalWrite(FONA_KEY, LOW);
			delay(500);
		}
		digitalWrite(FONA_KEY, HIGH);
		Serial.println(F(" done."));
		delay(500);
	}

	// Start the FONA
	Serial.print(F("Initializing FONA..."));
	fonaSerial.begin(4800);

	// See if the FONA is responding
	if (! fona.begin(fonaSerial))
	{
		Serial.println(F("Couldn't find FONA"));
		return false;
		while (1);
	}
        else
        {
                Serial.println(F("done."));
                return true;
        }
}



void stopFona()
{
	delay(5000);	   //	Shorter delays yield unpredictable results

	Serial.println(F("Turning off Fona "));
	while(digitalRead(FONA_PS) == HIGH)
	{
		digitalWrite(FONA_KEY, LOW);
	}
	digitalWrite(FONA_KEY, HIGH);

	delay(4000);
}



void logRSSI()
{
        float latitude;
        float longitude;
        char gpsQuery[80];
        char* ignore;
        char* UTC;
        
        //    This command gets the lat and long and converts it to decimal
        fona.getGPS (&latitude, &longitude);
        
        /*    We'll get the date and time by parsing the whole string. UTC
        is the last variable the command returns; we'll just ignore the others */
        
        fona.getGPS (0, gpsQuery, 80);
        
        ignore = strtok (gpsQuery, ",");
        ignore = strtok (NULL, ",");
        ignore = strtok (NULL, ",");
        ignore = strtok (NULL, ",");
        UTC = strtok (NULL, ",");
        
        int rssi = fona.getRSSI();
        
        if (rssi > 0 && rssi < 99)
        {
                digitalWrite (blueLED, HIGH);
        }
        else
        {
                digitalWrite (blueLED, LOW);
        }
        
        File mapData = SD.open ("mapData.csv", FILE_WRITE);
        
        if (mapData)
        {
                mapData.print (UTC);
                mapData.print (F(","));
                mapData.print (latitude, 6);
                mapData.print (F(","));
                mapData.print (longitude, 6);
                mapData.print (F(","));
                mapData.println (rssi);
                mapData.close();
                
                Serial.print (UTC);
                Serial.print (F(","));
                Serial.print (latitude, 6);
                Serial.print (F(","));
                Serial.print (longitude, 6);
                Serial.print (F(","));
                Serial.println (rssi);  
        }      
        else
        {
                Serial.println (F("Failed to open mapData.csv"));
        }
}
