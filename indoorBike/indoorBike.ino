#include "BLE.h"
#include <SPI.h>
#include <U8g2lib.h>
#include <AccelStepper.h>
#include <EEPROM.h>
#include "soc/timer_group_struct.h"	
#include "soc/timer_group_reg.h"	// nesessary for avoiding watchdog triggering on the stepper core
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define wholeWeight 720			// weight of bicycle and rider
#define wheelDiameter 0.33
#define motorRpm 60				// RPM of the stepper motor

#define motorSteps 3200.0		// steps per revolution, needs to be float for calculations

const int levelSteps = 40;		// resistance steps 
#define rotations 4.5			// rotations of the screw for the steps

#define button1 25				// pin of pause button 
#define button2 32				// pin of enter button
#define button3 22				// pin of decrement button
#define button4 21				// pin of increment button

#define interruptTime 70		// miliseconds for debouncing buttons

const int lowCal[3] =	{ 60, 10, 30 }; // cadence[rpm], level, power[W]
const int highCal[3] =	{ 60, 20, 150 }; // cadence[rpm], level, power[W]

float lowTorque = lowCal[2] / ((lowCal[0] / 60.0) * 6.28);		// wheel torque on the low point of calibration
float highTorque = highCal[2] / ((highCal[0] / 60.0) * 6.28);	// wheel torque on the high point of calibration

int gearIndex;
#define gearNum 22			// number of gears
float gears[gearNum] = { 	// gear ratios
	0.323529412,
	0.382352941,
	0.441176471,
	0.458333333,
	0.5		   ,
	0.541666667,
	0.558823529,
	0.617647059,
	0.625	   ,
	0.705882353,		 
	0.708333333,
	0.791666667,
	0.823529412,
	0.875	   ,
	0.941176471,
	1		   ,
	1.088235294,
	1.166666667,
	1.235294118,
	1.333333333,
	1.541666667,
	1.75
};

// lcd defined
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, /* clock=*/ 16 /* A4 */, /* data=*/ 17 /* A2 */, /* CS=*/ 0 /* A3 */, /* reset=*/ 4);
AccelStepper stepper = AccelStepper(1, 33, 26); // stepper defined

TaskHandle_t Task1;	// label for the stepper task

int16_t setPos, curPos;
int scroll;
float level;
bool zero;
bool psB;
bool enterB;
bool menu;

void IRAM_ATTR incr()	  // function to increment the level, gear index or scroll, with debouncing
{
	static unsigned long last_interrupt_time = 0;
	unsigned long interrupt_time = millis();
	if (interrupt_time - last_interrupt_time > interruptTime)
	{
		if (!menu) 
		{
			if (!sim)
			{
				level = int(level);
				level++;
			}
			else
			{
				gearIndex++;
				gearIndex = constrain(gearIndex, 0, gearNum-1);
			}
		}
		else scroll++;
	}
	last_interrupt_time = interrupt_time;
}
void IRAM_ATTR decr()	  // function to decrement the level, gear index or scroll, with debouncing
{
	static unsigned long last_interrupt_time = 0;
	unsigned long interrupt_time = millis();
	if (interrupt_time - last_interrupt_time > interruptTime)
	{
		if (!menu) 
		{
			if (!sim)
			{
				level = int(level);
				level--;
			}
			else
			{
				gearIndex--;
				gearIndex = constrain(gearIndex, 0, gearNum-1);
			}
		}
		else scroll--;
	}
	last_interrupt_time = interrupt_time;
}

void IRAM_ATTR enter()
{
	static unsigned long last_interrupt_time = 0;
	unsigned long interrupt_time = millis();
	if (interrupt_time - last_interrupt_time > interruptTime)
	{
		if (menu) enterB = true;
		menu = true;
		psB = true;
	}
	last_interrupt_time = interrupt_time;
}

void IRAM_ATTR ps()
{
	static unsigned long last_interrupt_time = 0;
	unsigned long interrupt_time = millis();
	if (interrupt_time - last_interrupt_time > interruptTime)
	{
		if (!menu) psB = !psB;
		menu = false;
	}
	last_interrupt_time = interrupt_time;
}

void setup() {
	Serial.begin(115200);
	BLEDevice::init("IndoorBike"); // name of the ble devicev
	while (!connectToServer());
	InitBLEServer();
	
	u8g2.begin();
	u8g2.clearBuffer();
	while (!SD.begin()) {    	   // while the card isnt initialized display message
		u8g2.setFont(u8g2_font_ncenB14_tr);
		u8g2.setCursor(33, 30);
		u8g2.print("WLOZ");
		u8g2.setCursor(28, 48);
		u8g2.print("KARTE");
		u8g2.sendBuffer(); 
		delay(200);   
	}

	xTaskCreatePinnedToCore(	   // setup the task for the second core
	                  Task1code,
		/* Task function. */
	                  "Task1",
		/* name of task. */
	                  10000,
		/* Stack size of task */
	                  NULL,
		/* parameter of the task */
	                  1,
		/* priority of the task */
	                  &Task1,
		/* Task handle to keep track of created task */
	                  0);          /* pin task to core 0 */
	
	stepper.setMaxSpeed(motorRpm / 60 * 3200);	// setup the motor																						 
	stepper.setSpeed(motorRpm / 60 * 3200);
	stepper.setAcceleration(12800);
	stepper.setEnablePin(27);					// enable pin
	stepper.setPinsInverted(0, 0, 1);

	pinMode(button1, INPUT);
	pinMode(button2, INPUT);
	pinMode(button3, INPUT);
	pinMode(button4, INPUT);	   	

	EEPROM.begin(4);
  
	level = EERead(2);
	Serial.println(level);
	curPos = EERead(0);
	Serial.println(curPos);
	stepper.setCurrentPosition(curPos);
	stepper.enableOutputs();
	menu = false;
	psB = true;
}
File file;
int values[64], valIndex;
long pauseStart, pauseEnd, pauseTot, playTime;
bool pauseTest, levelTest = true;
int zeroCnt, loopCnt;
void loop() {
	if (sim) // if in simulation mode (not ERG)
	{	
		float setTorque = (((grade / 10000.0)*wholeWeight) * wheelDiameter)+5.5;  // calculated set resistance torque on the wheel + torque on flat road
		Serial.println(setTorque);
		Serial.println(grade);
		float pedalTorque = gears[gearIndex]*setTorque;	 // set torque on the crank
		Serial.println(gears[gearIndex]);
		Serial.println(pedalTorque);
		level = mapF(pedalTorque, lowTorque, highTorque, lowCal[1], highCal[1]);	// mapped to the level settings
		Serial.println(level);
		level = constrain(level, 0, levelSteps);
		
																	
	}
	speedOut = (cadenceIn * 2.75 * 2.08 * 60*gears[gearIndex]) / 10;						// calculated speed, required by the specification
	indoorBikeDataCharacteristicData[2] = (uint8_t)(speedOut & 0xff);
	indoorBikeDataCharacteristicData[3] = (uint8_t)(speedOut >> 8);							// speed value with little endian order
	indoorBikeDataCharacteristicData[4] = (uint8_t)((cadenceIn * 2) & 0xff);				
	indoorBikeDataCharacteristicData[5] = (uint8_t)((cadenceIn * 2) >> 8);					// cadence value
	indoorBikeDataCharacteristicData[6] = (uint8_t)(constrain(powerIn, 0, 4000) & 0xff);
	indoorBikeDataCharacteristicData[7] = (uint8_t)(constrain(powerIn, 0, 4000) >> 8);		// power value, constrained to avoid negative values, although the specification allows for a sint16
    
	indoorBikeDataCharacteristic.setValue(indoorBikeDataCharacteristicData, 8);				// values sent
	indoorBikeDataCharacteristic.notify();	 												// device notified
	
	if (digitalRead(button2))zeroCnt++; // counter for button hold used for homing
	else zeroCnt = 0;
	if (!menu) { // if menu isn't on, display values
		u8g2.clearBuffer();
		u8g2.setFont(u8g2_font_ncenB14_tr);
		u8g2.setCursor(100, 32 + 7);
		if (!sim) u8g2.print(int(level));
		else	 u8g2.print(gearIndex);
		u8g2.setCursor(2, 30);
		u8g2.print(String(powerIn));
		u8g2.setCursor(2, 50);
		u8g2.print(String(cadenceIn));
		u8g2.setFont(u8g2_font_6x10_tf);
		u8g2.setCursor(1 + (12*String(powerIn).length()), 30);
		u8g2.print("W");
		u8g2.setCursor(1 + (12*String(cadenceIn).length()), 50);
		u8g2.print("rpm");
		u8g2.setFont(u8g2_font_6x13_tf);
		u8g2.setCursor(2, 11);			
		u8g2.print(file.name());
		if (psB && millis() % 2000 > 1000) // blinking "pause" sign
		{
			u8g2.setCursor(97, 11);
			u8g2.print("PAUZA");
		}
		u8g2.sendBuffer();
	}
	else
	{
		u8g2.clearBuffer();
		u8g2.drawRFrame(0, 0, 128, 64, 7);
		File root = SD.open("/");
		file = root.openNextFile();
		if (!file) {
			u8g2.setFont(u8g2_font_ncenB14_tr);
			u8g2.setCursor(30, 32 + 7);
			u8g2.print("Brak plikow");	 // no files
		}
		else  // display file list end show cursor
		{
			String files[32];
			int i = 0;
			u8g2.setFont(u8g2_font_6x13_tf);
			while (file) {        
				if (!(bool)file.isDirectory())
				{
					files[i] = file.name();
					i++;
				}
				file = root.openNextFile();
			}
			for (i = 0; i < 5; i++)
			{
				int scrollPrev;
				if (files[int(i + 5*(floor(scroll / 5)))] != "")
				{
					u8g2.setCursor(20, 14 + (i * 11));
					u8g2.print(files[int(i + 5*(floor(scroll / 5)))]);
				}      
			}
			if (files[scroll] == "") scroll = scroll - 1;
			u8g2.setCursor(5, 14 + ((scroll % 5) * 11));
			u8g2.print("->");

			if (enterB)  // chose file and close the menu
			{
				file = SD.open(files[scroll]);
				Serial.println(file.name());
				enterB = false;
				menu = false;
				String content = file.readString();	   //read file contents
				Serial.println(content);

				int lastIndex = 0;
				int index = content.indexOf(";", lastIndex);
				i = 0;
				while (index >= 0) // get the list of settings
				{
					values[i] = content.substring(lastIndex, index).toInt();
					lastIndex = index + 1;
					index = content.indexOf(";", lastIndex);
					i++;
				}
				psB = true;
				pauseTot = pauseTot + playTime;
				valIndex = 0;
			}
		}
		u8g2.sendBuffer();
	}

	if (psB&&pauseTest)			// deal with pausing
	{
		pauseStart = millis();
		pauseTest = false;
	}
	else if (!psB && !pauseTest)
	{
		pauseEnd = millis();
		pauseTest = true;
		pauseTot = pauseTot + (pauseEnd - pauseStart);
	}

	if (!psB)
	{
		playTime = millis() - pauseTot;
		Serial.println(playTime);
		if (values[valIndex] == playTime / 1000 && levelTest)
		{
			levelTest = false;
			level = values[valIndex + 1];
			valIndex = valIndex + 2;
		}
		else if (values[valIndex] != playTime / 1000) levelTest = true;
	}
  
	if (level > levelSteps) level = levelSteps;
	if (level < 0) level = 0;
	if (scroll < 0) scroll = 0;
	setPos = mapF(level, 0.0, levelSteps, 0.0, motorSteps*rotations);
	Serial.print("setPos	");
	Serial.println(setPos);
	if ((curPos != setPos && !zero) && level >= 0 && level <= levelSteps) // move the motor to a new position
	{
    
		stepper.moveTo(setPos);
		curPos = setPos;

		EEWrite(curPos, 0);
		EEWrite(level, 2);
    
		Serial.println(curPos);
		Serial.println(level);
	}
	if (zeroCnt > 30) zero = true;
	Serial.println(zeroCnt);
	if (zero)		 // move to the end of travel until motor loses steps
	{
		Serial.println("Homing");
		curPos = 0;   
		EEWrite(curPos, 0);
		u8g2.clearBuffer();
		u8g2.setFont(u8g2_font_ncenB14_tr);
		u8g2.setCursor(3, 32 + 7);
		u8g2.print("ZEROWANIE");
		u8g2.sendBuffer();
		stepper.setMaxSpeed(300 / 60 * 3200);
		stepper.moveTo(-(int(rotations*motorSteps)));
		//Serial.println(-(rotations*motorSteps));
		delay(5000);
		curPos = 0;  
		level = 0; 
		stepper.setCurrentPosition(0); 
		stepper.setMaxSpeed(motorRpm / 60 * 3200);
		zero = false;
		menu = false;
	}
	//stepper.run();
}

void EEWrite(int16_t inp, int reg) //write a word to eeprom on register
{
	word ee;
	//Serial.println(inp);
	ee = inp & 0xFF00;
	ee = ee >> 8;
	EEPROM.write(reg, ee);
	ee = inp & 0xFF;
	EEPROM.write(reg + 1, ee);
	EEPROM.commit();
}

int16_t EERead(int reg)			   // read a word on register
{
	word ee;
	ee = EEPROM.read(reg);
	ee = ee << 8;
	ee = ee + EEPROM.read(reg + 1);
	return ee;
}
bool test;
void Task1code(void * parameter) {	   // handle the stepper and button on the other core. Interrupts break bluetooth
	for (;;) {
		TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;	   // the loop needs to run fast, this disables the watchdog
		TIMERG0.wdt_feed = 1;
		TIMERG0.wdt_wprotect = 0;

		if (stepper.isRunning()) stepper.enableOutputs();
		else                    stepper.disableOutputs();
    
		stepper.run();
		if (digitalRead(button4)) incr();
		if (digitalRead(button3)) decr();
		if (digitalRead(button2)) enter();
		if (digitalRead(button1)) ps();
		delayMicroseconds(10);
	}
}
