#ifndef BLSERVER_TEST
#define BLSERVER_TEST

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>



BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool parseCmd = false;
char param[128];

uint8_t txValue = 0;


#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device Connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconencted");
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {

        uint8_t idx = 0;
        uint8_t toGo = rxValue.length();
        if(toGo > 128) toGo = 128;

        for( idx = 0; idx < toGo; idx++)
        {
             param[idx] = rxValue[idx];
            if( (uint8_t)param[idx] == 13 || (uint8_t)param[idx] == 10 ) param[idx] = 0;
            // Serial.print((uint8_t)param[idx]);
            //            Serial.print(" ");
        }

        param[idx+1] = 0;
        parseCmd = true;

      }
    }
};

class BlueToothServer //: // Looper
{
public:
  const char* name()  { return "BlserverTEst"; }     // override

  void Setup() //override 
  {
    delay(10000);
    // Create the BLE Device
    BLEDevice::init("UART Service");

    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pTxCharacteristic = pService->createCharacteristic(
                                            CHARACTERISTIC_UUID_TX,
                                            BLECharacteristic::PROPERTY_NOTIFY
                                        );
                        
    pTxCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                                                CHARACTERISTIC_UUID_RX,
                                                BLECharacteristic::PROPERTY_WRITE
                                            );

    pRxCharacteristic->setCallbacks(new MyCallbacks());

    // Start the service
    pService->start();

    // Start advertising
    pServer->getAdvertising()->start();
    Serial.println("Waiting a client connection to notify...");
  }

  void Loop() //override 
  {
    if (deviceConnected) {
        // pTxCharacteristic->setValue(&txValue, 1);
        // pTxCharacteristic->notify();
        // txValue++;
		// delay(10); // bluetooth stack will go into congestion, if too many packets are sent
        if(parseCmd) {   // add semaphore here !!! 
            parseCmd = false;
            if (!CommandParser::DoParse((const char*)&param, NULL)) {
            Serial.print("Whut? :");
            Serial.println(param);
            }
        }

	}

    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
		// do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
  }
};

BlueToothServer BlServer;


#endif