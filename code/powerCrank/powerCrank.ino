#include "HX711.h"
#include <Adafruit_MPU6050.h>

#include <Wire.h>
#include <math.h>
#include <EEPROM.h>

// all must be floats for calculations
#define sensMin -200941.0  // reading of the sensor with 0 force
#define sensMax -351117.0  // reading of the sensor with weightMax of force
#define weightMax 120.0
#define radius 0.175		   // distance form crank center to pedal center
#define sensRadius 0.115   // distance of the MPU sensor to crank center 

#define zeroAngle 180	   // angle, where to tare - all angles according to the Cycling Power Measurement specifications
						   // left crank forward - 0 degrees
#define freq 0.5		   // notify frequency

#define LOADCELL_DOUT_PIN 16
#define LOADCELL_SCK_PIN 17

Adafruit_MPU6050 mpu;
HX711 scale;

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"			// UUID of the service, random, must match the one on the console
#define POWER_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"	// UUID of the characteristic, random, must match the one on the console
#define CADENCE_CHARACTERISTIC_UUID "ceb5483e-36e1-4688-b7f5-ea07361b26a8"	// UUID of the characteristic, random, must match the one on the console

BLECharacteristic sendPower(POWER_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
BLECharacteristic sendCadence(CADENCE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);

// average variables:
// avr - rolling average lasts one rotation or 2 seconds
// avgFin - the final average of avr with the frequency of "freq"
long reading;
float weight, weightOld, power, rpm, avr;
double angle;
float reg[160];	// samples for the rolling average
bool test;

bool tareTest = true;
int samples, cnt, tareCnt, offCnt;
int8_t tare;
long avgFin;

bool _BLEClientConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    _BLEClientConnected = true;
    Serial.println("conn");
  };

  void onDisconnect(BLEServer* pServer) {
    _BLEClientConnected = false;
    Serial.println("disc");
  }
};

void InitBLE() {
    BLEDevice::init("crank");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
	BLEService *pPower = pServer->createService(SERVICE_UUID);

	pPower->addCharacteristic(&sendPower);
	BLE2902* powerDesc = new BLE2902();
	powerDesc->setIndications(1);                                       
	sendPower.addDescriptor(powerDesc);
	pPower->addCharacteristic(&sendCadence);
	BLE2902* cadenceDesc = new BLE2902();
	cadenceDesc->setIndications(1);
	sendCadence.addDescriptor(cadenceDesc);
	pServer->getAdvertising()->addServiceUUID(SERVICE_UUID);
	pServer->getAdvertising()->start();
	pPower->start();
}
  void onDisconnect(BLEServer* pServer) {
    _BLEClientConnected = false;
  }

void setup() {
    pinMode(32, OUTPUT);
    digitalWrite(32, HIGH);
    Serial.begin(115200);
    Serial.println("Start");
    InitBLE();
    mpu.begin();
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
	
    mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_94_HZ);
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
	// the mpu must be fast, therefore max setting
	
	pinMode(5, OUTPUT);	  // tare LED setup
	digitalWrite(5, HIGH);
	EEPROM.begin(1);		//read tare
	tare = EEPROM.read(0); 
}
uint8_t blink;
void loop() {
	cnt++;
  
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
  
    reading = scale.read(); // sensor must be set in 80 Hz mode, otherwise the loop will run too slow
							// this blocking command provides timing
    weight = ((reading - sensMin) * (weightMax) / (sensMax - sensMin)); // calculate the force on the pedal in N

	// angle measurement from gravitaional acceleration
	// centripetal acceleration acceleration is accaunted for
	// the angle is adjusted to match the gatt specifications with fmod
    angle = fmod(((atan2(a.acceleration.y + (g.gyro.z*g.gyro.z*sensRadius), a.acceleration.x)) * 360 / 6.28) + 180, 360.0);  
	
	// if crank is in position and weight isn't changing increment counter
	// after slightly more than two seconds tare and write to eeprom 
	if ((abs(angle - zeroAngle) < 10) && (int(rpm) == 0) && tareTest && (abs(weight-weightOld)< 2)) 
	{       
		tareCnt++;
		digitalWrite(5, (blink & 0x8)); // blink LED if in position
		blink++;
		if (tareCnt >= 200)
		{
			tare = weight;
			EEPROM.write(0, tare);
			EEPROM.commit();
			digitalWrite(5, LOW);
			delay(1000);
			digitalWrite(5, HIGH);
			tareCnt = 0;
			
			tareTest = false;
			
		}
	}
	else if(!(abs(angle - zeroAngle) < 10 && int(rpm) == 0)) 
	{
		weightOld = weight;
		tareCnt = 0;
		//tareTest = 1;
		//Serial.println((int)tareTest);
		digitalWrite(5, HIGH);
	}
	
	
	weight -= tare;	// subtract tare
    power = (-g.gyro.z*radius*weight);  // calculate power from angular velocity and force, negative because of the sensor mounting

    reg[0] = power;	// 0th index assigned for rolling average 

    samples = constrain(abs(4800/rpm), 1, 160);	// calculate the number of samples taken into account
	
    for (int i=0; i<samples; i++)	// get the average
    {
    avr = avr + reg[i];
    }	
    avr = avr/samples;
	
    for (int i=160-2; i>=0; i--)	// shift the array
    {
    reg[i+1] = reg[i];
    }
  
    rpm = -(g.gyro.z/6.28)*60;		// calculate RPM from rotational velocity

    avgFin=avgFin+int(avr);			// sum the final average
  
    if(cnt>=(int(80/freq)))			// every 1/freq, 80 is the number of samples (loop repeats) per second
    {
      if (!_BLEClientConnected) offCnt++;
      else offCnt = 0;
    
      if (offCnt > 60*freq) esp_deep_sleep_start();
      avgFin = constrain((2*avgFin)/cnt, 0, 2000); // calculate the average - 2* for two legs (measuring only one)

	    int16_t power = (int16_t)avgFin;
	    int16_t cadence = (int16_t)rpm;
	    
	    uint8_t powerOut[2] = { (uint8_t)(power >> 8), (uint8_t)(power & 0xff) };		// set values
	    uint8_t cadenceOut[2] = { (uint8_t)(cadence >> 8), (uint8_t)(cadence & 0xff) };
	    
	    // send and notify
	    sendPower.setValue(powerOut, 2);
	    sendPower.notify();
	    sendCadence.setValue(cadenceOut, 2);
	    sendCadence.notify();
     Serial.println(offCnt);

	    // print data for debugging
        Serial.print("Kąt: ");
        Serial.print(angle);
        Serial.print("   ");

        Serial.print("Sila: ");
        Serial.print(weight);
        Serial.print("   ");
	    
	    Serial.print("Wartosc: ");
	    Serial.print(reading);
	    Serial.print("   ");

        Serial.print(" Tara: ");
        Serial.print(tare);
        Serial.print("   ");

  
        Serial.print(" S. moc: ");
        Serial.print(avgFin);
        Serial.print("   ");

        Serial.print(" Kadencja: ");
        Serial.print(rpm);
        Serial.print("   ");
  
  
        Serial.println();
        cnt = 0;
        avgFin = 0;
    }



}
