#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <BLE2902.h>

#define MACAddress "24:62:ab:d4:da:9a" // address of the device sending data

const int16_t minPowLev = 0; // power level range settings, no app cares about this
const int16_t maxPowLev = 500;
const int16_t minPowInc = 1;

const int16_t maxResLev = 40;// resistance level range settings, no app cares about this

#define fitnessMachineService BLEUUID((uint16_t)0x1826) // fitness machine service uuid, as defined in gatt specifications

// required characteristics
BLECharacteristic fitnessMachineFeatureCharacteristics(BLEUUID((uint16_t)0x2ACC), BLECharacteristic::PROPERTY_READ);
BLECharacteristic indoorBikeDataCharacteristic(BLEUUID((uint16_t)0x2AD2), BLECharacteristic::PROPERTY_NOTIFY);
BLECharacteristic resistanceLevelRangeCharacteristic(BLEUUID((uint16_t)0x2AD6), BLECharacteristic::PROPERTY_READ);
BLECharacteristic powerLevelRangeCharacteristic(BLEUUID((uint16_t)0x2AD8), BLECharacteristic::PROPERTY_READ);
BLECharacteristic fitnessMachineControlPointCharacteristic(BLEUUID((uint16_t)0x2AD9), BLECharacteristic::PROPERTY_INDICATE | BLECharacteristic::PROPERTY_WRITE);
BLECharacteristic fitnessMachineStatusCharacteristic(BLEUUID((uint16_t)0x2ADA), BLECharacteristic::PROPERTY_NOTIFY);
BLEAdvertisementData advertisementData = BLEAdvertisementData();

static BLEUUID 	serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");	// custom uuid service of the crank
static BLEUUID   powerUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");		// custom uuid of power characteristic
static BLEUUID   cadenceUUID("ceb5483e-36e1-4688-b7f5-ea07361b26a8");	// custom uuid of cadence characteristic

BLERemoteCharacteristic* pCadenceCharacteristic;
BLERemoteCharacteristic* pPowerCharacteristic;
BLEAddress* myDevice;

// no app cares about these flags, but they are there just in case
const uint16_t indoorBikeDataCharacteristicDef = 0b0000000001000100; // flags for indoor bike data characteristics - power and cadence
const uint32_t fitnessMachineFeaturesCharacteristicsDef = 0b00000000000000000100000010000010; // flags for Fitness Machine Features Field - cadence, resistance level and inclination level
const uint32_t targetSettingFeaturesCharacteristicsDef = 0b00000000000000000010000000001100;  // flags for Target Setting Features Field - power and resistance level, Indoor Bike Simulation Parameters 
uint16_t speedOut = 100;
int16_t powerOut = 100;
uint8_t indoorBikeDataCharacteristicData[8] = { 	     // values for setup - little endian order
	(uint8_t)(indoorBikeDataCharacteristicDef & 0xff),	
	(uint8_t)(indoorBikeDataCharacteristicDef >> 8), 
	(uint8_t)(speedOut & 0xff),
	(uint8_t)(speedOut >> 8), 
	(uint8_t)(speedOut & 0xff), 
	(uint8_t)(speedOut >> 8), 
	0x64, 
	0 
};
                                     
uint8_t fitnessMachineFeatureCharacteristicsData[8] = {	 // values for setup - little endian order
	 (uint8_t)(fitnessMachineFeaturesCharacteristicsDef & 0xff),
	(uint8_t)(fitnessMachineFeaturesCharacteristicsDef >> 8),
	(uint8_t)(fitnessMachineFeaturesCharacteristicsDef >> 16),
	(uint8_t)(fitnessMachineFeaturesCharacteristicsDef >> 24),
	(uint8_t)(targetSettingFeaturesCharacteristicsDef & 0xff),
	(uint8_t)(targetSettingFeaturesCharacteristicsDef >> 8),
	(uint8_t)(targetSettingFeaturesCharacteristicsDef >> 16),
	(uint8_t)(targetSettingFeaturesCharacteristicsDef >> 24) 
};

uint8_t resistanceSet;
int16_t powerSet, grade;

bool sim = false;;
class MyCallback : public BLECharacteristicCallbacks {	// write performed on the control point characteristic
	void onWrite(BLECharacteristic* pCharacteristic) {
		std::string message = pCharacteristic->getValue(); // std::string - very important, as it doesnt stop when getting a byte of zeros
		Serial.print("'");
		for(int i=0; i<message.length(); i++) Serial.print(message[i]); // print the message byte by byte - any conversion to a normal String terminates it on a byte of zeros
		Serial.print("'");												// for debugging and new messages
		uint8_t value[3] = { 0x80, (uint8_t)message[0], 0x02 };	// confirmation data, default - 0x02 - Op "Code not supported", no app cares
		uint8_t statusValue[1];
		switch ((uint8_t)message[0])
		{
		case 0x03:	// inclination level setting
			grade = (message[2] << 8) + message[1];
			grade *= 10;
			value[2] = 0x01; // ok sent  
			sim = true;
			break;
		case 0x04:	// resistance level setting
			resistanceSet = message[1] / 10;
			//Serial.print("  ");
			//Serial.println(resistanceSet);
			value[2] = 0x01; // ok sent 
			sim = false;
			break;
		case 0x00:	// request control
			//Serial.println("control");
			value[2] = 0x01; // ok sent 
			//sim = false;
			break;
		case 0x07:	// start the training, not used, just confirmation
			value[2] = 0x01; // ok sent 
			//sim = false;
			break;
		case 0x05:  // power level setting
			powerSet = (message[2] << 8) + message[1];
			//Serial.print("  ");
			//Serial.println(powerSet);
			value[2] = 0x01; // ok sent 
			sim = false;
			break;
		case 0x11:
			int16_t windSpeed = (message[2] << 8) + message[1];
			grade = (message[4] << 8) + message[3];
			int8_t crr = message[5];
			int8_t cw = message[6];
//			Serial.print("	");
//			Serial.print(windSpeed);
//			Serial.print("	");
//			Serial.print(grade);
//			Serial.print("	");
//			Serial.print(crr);
//			Serial.print("	");
//			Serial.print(cw);
//			Serial.print("	");
//			Serial.println(message.length());
			
			uint8_t statusValue[7] = { 0x12, (uint8_t)message[1], (uint8_t)message[2], (uint8_t)message[3], (uint8_t)message[4], (uint8_t)message[5], (uint8_t)message[6] };
			fitnessMachineStatusCharacteristic.setValue(statusValue, 7); // Fitness Machine Status updatet, no app cares
			//Serial.println("set");
			fitnessMachineStatusCharacteristic.notify();
			
			value[2] = 0x01; // ok sent 
			sim = true;
			break;
		}
		pCharacteristic->setValue(value, 3); // response to write
		pCharacteristic->indicate();		 // indicate response
	}
};

int16_t cadenceIn;
int16_t powerIn;

void powerCallback(		// when power reading notification received
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
	uint8_t* pData,
	size_t length,
	bool isNotify) {
	powerIn = (int16_t)(pData[1] + (pData[0] << 8));
	Serial.print("powerIn	");
	Serial.println(powerIn);
}


void cadenceCallback(	// when cadence reading notification received
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
	uint8_t* pData,
	size_t length,
	bool isNotify) {
	cadenceIn = (int16_t)(pData[1] + (pData[0] << 8));
	cadenceIn = constrain(cadenceIn, 0, 300);
	Serial.print("cadenceIn	");
	Serial.println(cadenceIn);
}

bool connectToServer() {
	Serial.print("Forming a connection to ");
	//Serial.println(myDevice->getAddress().toString().c_str());
    
	BLEClient*  pClient  = BLEDevice::createClient();
	Serial.println(" - Created client");
	
	myDevice = new BLEAddress(MACAddress);

	// Connect to the remove BLE Server.
	pClient->connect(*myDevice, BLE_ADDR_TYPE_PUBLIC);
	if (pClient->isConnected())
	{
		Serial.println(" - Connected to server");
	}
	else
	{
		Serial.println("failed");
	}

	// Obtain a reference to the service we are after in the remote BLE server.
	BLERemoteService* pRemoteService = pClient->getService(serviceUUID);

	if (pRemoteService == nullptr) {
		Serial.print("Failed to find our service UUID: ");
		Serial.println(serviceUUID.toString().c_str());
		pClient->disconnect();
		return false;
	}
	Serial.println(" - Found our service");


	// Obtain a reference to the characteristic in the service of the remote BLE server.
	pPowerCharacteristic = pRemoteService->getCharacteristic(powerUUID);
	pPowerCharacteristic->registerForNotify(powerCallback);
	pCadenceCharacteristic = pRemoteService->getCharacteristic(cadenceUUID);
	pCadenceCharacteristic->registerForNotify(cadenceCallback);
	
	return true;
}

void InitBLEServer() {
	BLEServer *pServer = BLEDevice::createServer();

	const std::string fitnessData = { 0b00000001, 0b00100000, 0b00000000 };	 // advertising data on "Service Data AD Type" - byte of flags (little endian) and two for Fitness Machine Type (little endian)
  																			 // indoor bike supported
	advertisementData.setServiceData(fitnessMachineService, fitnessData);	 // already includdes Service Data AD Type ID and Fitness Machine Service UUID
  																			 // with fitnessData 6 bytes
	BLEService *pFitness = pServer->createService(fitnessMachineService);

	// added characteristics and descriptors - 2902 required for characteristics that notify or indicate
	
	pFitness->addCharacteristic(&indoorBikeDataCharacteristic);
	indoorBikeDataCharacteristic.addDescriptor(new BLE2902());

	pFitness->addCharacteristic(&fitnessMachineFeatureCharacteristics);
	pFitness->addCharacteristic(&resistanceLevelRangeCharacteristic);
	
	pFitness->addCharacteristic(&fitnessMachineControlPointCharacteristic);
	
	BLE2902* descr = new BLE2902();
	descr->setIndications(1); // default indications on
	
	fitnessMachineControlPointCharacteristic.addDescriptor(descr);
		
	pFitness->addCharacteristic(&fitnessMachineStatusCharacteristic);
	fitnessMachineStatusCharacteristic.addDescriptor(new BLE2902());

	fitnessMachineControlPointCharacteristic.setCallbacks(new MyCallback()); // set callback for write

	pServer->getAdvertising()->addServiceUUID(fitnessMachineService);
	pServer->getAdvertising()->setAdvertisementData(advertisementData);
	pFitness->start();
	pServer->getAdvertising()->start();
	
	uint8_t powerRange[6] = { 
		(uint8_t)(minPowLev & 0xff),
		(uint8_t)(minPowLev >> 8),
		(uint8_t)(maxPowLev & 0xff),
		(uint8_t)(maxPowLev >> 8),
		(uint8_t)(minPowInc & 0xff),
		(uint8_t)(minPowInc >> 8)
	};
	
	uint8_t resistanceRange[6] = { 
		(uint8_t)(0 & 0xff),
		(uint8_t)(0 >> 8),
		(uint8_t)(maxResLev & 0xff),
		(uint8_t)(maxResLev >> 8),
		(uint8_t)(1 & 0xff),
		(uint8_t)(1 >> 8)
	};
	resistanceLevelRangeCharacteristic.setValue(resistanceRange, 6);
	powerLevelRangeCharacteristic.setValue(powerRange, 6);
	fitnessMachineFeatureCharacteristics.setValue(fitnessMachineFeatureCharacteristicsData, 8); // flags
}

// default Arduino map function doesnt work on floats
float mapF(float x, float in_min, float in_max, float out_min, float out_max) {	
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
