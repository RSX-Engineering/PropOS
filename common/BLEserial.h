#ifndef BLSERIAL_TEST
#define BLSERIAL_TEST

#include "Arduino.h"
#include "Stream.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// BLE specific include 
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "esp_statusleds.h"

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"   // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// #define BL_DEBUG_SERIAL

#define BL_INT_BUFFER_SIZE  1152
// #define BL_ADVERTISE_TIME  60000       // advertise on BLE for 1 minute (or until connected)
#define BL_ADVERTISE_TIME  180000

#define BL_MUTEX_PROTECT
#ifdef BL_MUTEX_PROTECT

#define BLESER_TX_MUTEX_LOCK()    do {} while (xSemaphoreTake(tx_mutex, portMAX_DELAY) != pdPASS)    
#define BLESER_TX_MUTEX_UNLOCK()  xSemaphoreGive(tx_mutex)

#define BLESER_RX_MUTEX_LOCK()    do {} while (xSemaphoreTake(read_mutex, portMAX_DELAY) != pdPASS)    
#define BLESER_RX_MUTEX_UNLOCK()  xSemaphoreGive(read_mutex)
#else 
#define BLESER_RX_MUTEX_LOCK()
#define BLESER_RX_MUTEX_UNLOCK() 

#define BLESER_TX_MUTEX_LOCK()   
#define BLESER_TX_MUTEX_UNLOCK()

#endif



class BLE_Serial: public Stream, public CommandParser, PowerSubscriber
{
public:

    class BLE_ServiceServerCB: public BLEServerCallbacks {

        /* @brief   :
        *  @param   :
        *  @retval  :  
        */ 
        void onConnect(BLEServer* pServer) {
          deviceConnected = true;
          #ifdef DIAGNOSE_BLE
            STDOUT.println("Device Connected");
            #endif
            uint16_t id = pServer->getConnId();
            #ifdef DIAGNOSE_BLE
            STDOUT.print("CON ID");STDOUT.println(id);
            #endif
            esp_ble_tx_power_set((esp_ble_power_type_t)id, ESP_PWR_LVL_P21);
            #ifdef DIAGNOSE_BLE
            int pwrCon  = esp_ble_tx_power_get((esp_ble_power_type_t)id);
            STDOUT.print("CON POWER");STDOUT.println(pwrCon);
            esp_bd_addr_t local_used_addr;
            BLEAddress address = BLEDevice::getAddress();
            esp_gap_conn_params_t paraCon;
            esp_ble_get_current_conn_params(*address.getNative(), &paraCon);
            STDOUT.println(paraCon.interval);                  /*!< connection interval */
            STDOUT.println(paraCon.latency);                   /*!< Slave latency for the connection in number of connection events. Range: 0x0000 to 0x01F3 */
            STDOUT.println(paraCon.timeout);
            #endif
            peerMTU = pServer->getPeerMTU(pServer->getConnId());
            #ifdef DIAGNOSE_BLE
            STDOUT.print("MTU SIZE");
            STDOUT.println(peerMTU);
            #endif
            peerMTU = peerMTU - 4;
        };
        
        /* @brief   :
        *  @param   :
        *  @retval  :  
        */ 
        void onMtuChanged(BLEServer* pServer, esp_ble_gatts_cb_param_t* param)
        {
            peerMTU = pServer->getPeerMTU(pServer->getConnId());
            #ifdef BL_DEBUG_SERIAL
            STDOUT.print("MTU SIZE changed ");
            STDOUT.println(peerMTU);
            #endif
            peerMTU = peerMTU - 4;
        }

        /* @brief   :
        *  @param   :
        *  @retval  :  
        */ 
        void onDisconnect(BLEServer* pServer) {
          deviceConnected = false;
          #ifdef DIAGNOSE_BLE
            STDOUT.println("Device disconencted");
          #endif

        }
    }serviceServerCB;

    class BLE_TXCharacteristicCB: public BLECharacteristicCallbacks {

        /* @brief   :
        *  @param   :
        *  @retval  :  
        */    
        void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) 
        {
            /*
            		
		ERROR_INDICATE_DISABLED,
		ERROR_NOTIFY_DISABLED,
		ERROR_GATT,
		ERROR_NO_CLIENT,
		ERROR_INDICATE_TIMEOUT,
		ERROR_INDICATE_FAILURE
        */
            switch(s)
            {
                case SUCCESS_INDICATE:
                break;

                case SUCCESS_NOTIFY:
                // BLESER_TX_MUTEX_LOCK();
                // STDOUT.println("Notify was success");
                txTriggered = false;
                lastSendTime = millis();    
                // BLESER_TX_MUTEX_UNLOCK();   // unlock transmission 
                break;

                default:
                // errors case , catch them later and act 
                break;
            }
        }

    }txcharCB;

    class BLE_RXCharacteristicCB: public BLECharacteristicCallbacks {

        void onRead(BLECharacteristic *pCharacteristic) 
        {   
            
            // STDOUT.print("OnRead ");STDOUT.println(millis());
        }
        /* @brief   :
        *  @param   :
        *  @retval  :  
        */    
        void onWrite(BLECharacteristic *pCharacteristic) 
        {
            // STDOUT.print("OnWrite ");STDOUT.println(millis());
            if(!rxBuf_handle) return;  // we dont have a buffer to put data !!! 
            BLESER_RX_MUTEX_LOCK();
            uint16_t bytesLen = pCharacteristic->getLength();
            if (bytesLen  > 0) {
                UBaseType_t res =  xRingbufferSend(rxBuf_handle, (void*)pCharacteristic->getData(), pCharacteristic->getLength(), pdMS_TO_TICKS(1000));
                if (res != pdTRUE) {
                    // TODO make a print maybe to signal failure
                }
            }
            BLESER_RX_MUTEX_UNLOCK();
        }

    }rxcharCB;

    void PwrOn_Callback() override {
        // MountSDCard(); 
        StartAdvBLE();
        #ifdef DIAGNOSE_POWER
        STDOUT.println(" ble+ ");  
        #endif
    }

    void PwrOff_Callback() override {
        StopAdvBLE(); 
        #ifdef DIAGNOSE_POWER
        STDOUT.println(" ble- ");  
        #endif
    }

    bool HoldPower() override {  // Return true to pause power subscriber timeout
      if(!blAdv) return false;
      return true;
    }         

    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    BLE_Serial(void) : CommandParser(), PowerSubscriber(pwr4_CPU | pwr4_Pixel) 
    {

        #ifdef BL_MUTEX_PROTECT
        read_mutex = xSemaphoreCreateMutex(); // Create read mutex
        tx_mutex = xSemaphoreCreateMutex(); // Create tx mutex
        #endif
        rxBuf_handle = xRingbufferCreate(BL_INT_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
        txBuf_handle = xRingbufferCreate(BL_INT_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
        blAdv = false;
        // statusMANAGER.linkLed(&blueTooth);

    }

    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    ~BLE_Serial(void)
    {

    }

    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    // bool begin(String localName=String(), bool isMaster=false)
    void createBLE(void)
    {
        // Create ble object only, dont start ADV
        char BLEname[32];
        sprintf(BLEname, "SaberProp [%lu]", (unsigned long)PROFFIE_HDID.xGetSerial());
        sprintf(BoardName, "SaberProp [%lu]", (unsigned long)PROFFIE_HDID.xGetSerial());
        BLEDevice::init(BLEname);                            // Create the BLE Device
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P21); 
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P21);
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN ,ESP_PWR_LVL_P21);

        int pwrAdv  = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV);
        int pwrScan = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_SCAN);
        int pwrDef  = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
        #ifdef DIAGNOSE_BLE       
            STDOUT.println("Power Settings: (ADV,SCAN,DEFAULT)");         //all should show index7, aka +9dbm
            STDOUT.println(pwrAdv);
            STDOUT.println(pwrScan);
            STDOUT.println(pwrDef);
        #endif
        //BLEDevice::setMTU(517);  // max MtU is 517
        pServer = BLEDevice::createServer();                        // Create the BLE Server
        pServer->setCallbacks(&serviceServerCB);           // Set server callback 
        STDOUT.print("createServer aid ");STDOUT.println(BLEDevice::m_appId);    
        pService = pServer->createService(SERVICE_UUID);            // Create the BLE Service

        pTxCharacteristic = pService->createCharacteristic(         // Create a BLE TX Characteristic
                                            CHARACTERISTIC_UUID_TX,
                                            BLECharacteristic::PROPERTY_NOTIFY
                                        );
        bleDescriptor = new BLE2902();                
        pTxCharacteristic->addDescriptor(bleDescriptor);            // add BLE descriptor

        pTxCharacteristic->setCallbacks(&txcharCB); // set RX callback 


        pRxCharacteristic = pService->createCharacteristic(         // Create a BLE RX Characteristic
                                                CHARACTERISTIC_UUID_RX,
                                                BLECharacteristic::PROPERTY_WRITE
                                            );

        pRxCharacteristic->setCallbacks(&rxcharCB); // set RX callback 

        pService->start();                          // Start the service

    }

    void destroyBLE()
    {
        pService->stop();
        delete pRxCharacteristic;
        delete pTxCharacteristic;
        delete pService;
        delete pServer;
        BLEDevice::deinit(false);
        delete bleDescriptor;
        if(!BLEDevice::getInitialized())
        STDOUT.println("Deinit success");
    }

 /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    // bool begin(String localName=String(), bool isMaster=false)
    void Setup(void)
    {
        #ifndef WIFI_ENABLED_ESP32 
        //StartAdvBLE();  
        #endif
    }

    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    int available(void)
    {
        return ble_get_buffered_bytes_len();
    }

    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    int peek(void)
    {
        return 0;
    }
    
    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    bool hasClient(void)
    {
        return deviceConnected;
    }

    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    int read(void)
    {   
        uint8_t c = 0;
        if(ble_read_bytes(&c, 1) == 1)
        {
            return c;
        }
        return -1;
    }

    size_t read(uint8_t *buffer, size_t size)
    {
        return ble_read_bytes(buffer, size);

    }
    size_t readBytes(uint8_t *buffer, size_t size)
    {
        return ble_read_bytes(buffer, size);
    }
    
    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    size_t write(uint8_t c)
    {
        return write(&c, 1);
    }

    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    size_t write(const uint8_t *buffer, size_t size)
    {
        if(!buffer) return 0;

        BLESER_TX_MUTEX_LOCK();  // aquire lock , it will be unlock just after sending 
                                 // make some tst here 
        lastTriggerTime = millis();
        // pTxCharacteristic->setValue((uint8_t *)buffer, size);
        // pTxCharacteristic->notify();
        UBaseType_t res =  xRingbufferSend(txBuf_handle, (void*)buffer, size, 0);
        if (res != pdTRUE) {
                    // TODO make a print maybe to signal failure
          #ifdef DIAGNOSE_BLE
            STDOUT.println("Failed to upload buffer ");
          #endif  
        }
        txTriggered = true;
        BLESER_TX_MUTEX_UNLOCK(); 

        return size;
    }

    inline size_t write(const char * buffer, size_t size)
    {
        return write((uint8_t*) buffer, size);
    }

    inline size_t write(const char * s)
    {
        return write((uint8_t*) s, strlen(s));
    }

    inline size_t write(unsigned long n)
    {
        return write((uint8_t) n);
    }

    inline size_t write(long n)
    {
        return write((uint8_t) n);
    }

    inline size_t write(unsigned int n)
    {
        return write((uint8_t) n);
    }

    inline size_t write(int n)
    {
        return write((uint8_t) n);
    }

    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    void flush()
    {   
        // uint32_t sTime = millis();
        // while(txTriggered && ((millis() - sTime) < 1000));
    }

    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    void end(void)
    {

    }
    
    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    void setTimeout(int timeoutMS)
    {

    }

    void Loop()
    {  if (deviceConnected) {  
            // BLESER_TX_MUTEX_LOCK();
            size_t available = 0;
            vRingbufferGetInfo(txBuf_handle,  NULL, NULL, NULL, NULL, &available);
            // BLESER_TX_MUTEX_UNLOCK();
            if(available) {
                BLESER_TX_MUTEX_LOCK();
                size_t  dlen = 0, available = 0;
                uint8_t * tx_data = NULL;
                
                vRingbufferGetInfo(txBuf_handle, NULL, NULL, NULL, NULL, &available);
                if(!available) {
                    BLESER_TX_MUTEX_UNLOCK();
                    return;
                }
                if(available > peerMTU)available = peerMTU;  // send up to 512 bytes 

                tx_data = (uint8_t *)xRingbufferReceiveUpTo(txBuf_handle, &dlen, 0, available);
                if(!tx_data) {
                    BLESER_TX_MUTEX_UNLOCK();
                    return;
                }
                // memcpy(data+so_far, tx_data, dlen);
                pTxCharacteristic->setValue((uint8_t *)tx_data, dlen);
                pTxCharacteristic->notify();

                vRingbufferReturnItem(txBuf_handle, tx_data);
                BLESER_TX_MUTEX_UNLOCK();
            }
	    }
        else {  // not connected
            if (oldDeviceConnected) {
                // disconnecting
                // blueTooth.startStopEffect(false);
                // color.r = 65535;
                // color.g = 0;
                // color.b = 0;
                // blueTooth.setColor(color);
                StatusLed_comm.setStatus(StatusLed_Comm::COMMSTATE_DISCONNECTED);        
                SaberBase::DoDisconnect();
                delay(500); // give the bluetooth stack the chance to get things ready
                // pServer->startAdvertising(); // restart advertising
                // STDOUT.println("start advertising");
                oldDeviceConnected = false;
                StartAdvBLE();         // start advertising
                // color.r = 0;
                // color.g = 0;
                // color.b = 65535;
                // blueTooth.setColor(color);
                // blueTooth.startStopEffect(true);
                StatusLed_comm.setStatus(StatusLed_Comm::COMMSTATE_BROADCASTING);             
            }
            else {  // not connected
                if (!advertiseUntil) return;    // advertise timed out, nothing to do
                if (millis() >= advertiseUntil) {
                    // stop advertising if it timed out
                    StopAdvBLE();      // stop advertising and LED effect
                }
            }
        }

           
            // connecting
        if (deviceConnected && !oldDeviceConnected) {
            // do stuff here on connecting
            oldDeviceConnected = deviceConnected;
            // blueTooth.startStopEffect(false);
            // color.r = 0;
            // color.g = 65535;
            // color.b = 0;
            // blueTooth.setColor(color);
            StatusLed_comm.setStatus(StatusLed_Comm::COMMSTATE_CONNECTED);
            SaberBase::DoConnect();
            advertiseUntil = 0;  // no advertise timeout 
        }

    }

private:

    size_t ble_read_bytes(uint8_t *data, size_t item_size)
    {
        //Receive data from byte buffer
        if(!data) return 0;
        BLESER_RX_MUTEX_LOCK();
        size_t  dlen = 0, to_read = item_size, so_far = 0, available = 0;
        uint8_t * rx_data = NULL;
        
        vRingbufferGetInfo(rxBuf_handle, NULL, NULL, NULL, NULL, &available);
        if(available < to_read) {
            to_read = available;
        }

        while(to_read){
        dlen = 0;
        rx_data = (uint8_t *)xRingbufferReceiveUpTo(rxBuf_handle, &dlen, 0, to_read);
        if(!rx_data){
            BLESER_RX_MUTEX_UNLOCK();
            return so_far;
        }
        memcpy(data+so_far, rx_data, dlen);
        vRingbufferReturnItem(rxBuf_handle, rx_data);
        so_far+=dlen;
        to_read-=dlen;
        }
        BLESER_RX_MUTEX_UNLOCK();
        return so_far;

    }

    size_t ble_get_buffered_bytes_len()
    {   
        size_t available = 0;
        BLESER_RX_MUTEX_LOCK();
        vRingbufferGetInfo(rxBuf_handle,  NULL, NULL, NULL, NULL, &available);
        BLESER_RX_MUTEX_UNLOCK();
        return available;
    }

    static BLEServer *pServer;
    static BLECharacteristic * pTxCharacteristic;
    static BLECharacteristic * pRxCharacteristic;
    static BLEService *pService;
    static bool deviceConnected;
    static bool oldDeviceConnected;
    static RingbufHandle_t rxBuf_handle;
    static RingbufHandle_t txBuf_handle;
    static uint32_t lastTriggerTime;
    static uint32_t lastSendTime;
    static bool txTriggered;
    uint32_t advertiseUntil = 0;         // timeout to advertise BLE [millis]
    static uint16_t peerMTU;
    bool blAdv;

    static BLE2902 *bleDescriptor;

    #ifdef BL_MUTEX_PROTECT
    static SemaphoreHandle_t read_mutex;
    static SemaphoreHandle_t tx_mutex;
    #endif

public:
    // Start advertising and signaling
    void StartAdvBLE () {
        // return;
        if (deviceConnected) return;        // don't advertise if already connected
        if (!BLEDevice::getInitialized()) {
            createBLE();
            if (!BLEDevice::getInitialized()) {
            #ifdef DIAGNOSE_BLE            
                STDOUT.println("Cannot start BLE because it is not initialized");
            #endif
            return;
            }
        }
        pServer->getAdvertising()->start();         // Start advertising
        #ifdef DIAGNOSE_BLE
            STDOUT.println("Waiting a client connection to notify...");
        #endif
        blAdv = true;
        StatusLed_comm.setStatus(StatusLed_Comm::COMMSTATE_BROADCASTING);        
        advertiseUntil = millis() + BL_ADVERTISE_TIME;     // advertise BL_ADVERTISE_TIME [ms]
    }

    // Stop advertising and signaling
    void StopAdvBLE () {
        // return;
        if(!blAdv) return;
        blAdv = false;
        StatusLed_comm.setStatus(StatusLed_Comm::COMMSTATE_OFF);   
        pServer->getAdvertising()->stop();          // Stop advertising
        advertiseUntil = 0;                         // No advertise timeout
        destroyBLE();
    }
    bool GetAdvStatus()
    {
        return blAdv;
    }

private:
    virtual bool Parse(const char* cmd, const char* arg) {
        if (!strcmp(cmd, "stopBLE")) {   
                StopAdvBLE();
                STDOUT.println("stopBLE-START");
                STDOUT.println("BLE advertising stopped");
                STDOUT.println("stopBLE-STOP");                   
                return true;
        }
        if (!strcmp(cmd, "startBLE")) {  
                STDOUT.println("startBLE-START"); 
                STDOUT.println("Starting BLE advertising");                   
                StartAdvBLE();        // Includes STDOUT message
                STDOUT.println("startBLE-STOP");      
                return true;
        }

        if (!strcmp(cmd, "createBLE")) {  
                STDOUT.println("createBLE-START"); 
                STDOUT.println("Creating resources for ble and start services");                   
                createBLE();        // Includes STDOUT message
                STDOUT.println("createBLE-STOP");      
                return true;
        }

        if (!strcmp(cmd, "destroyBLE")) {  
                STDOUT.println("destroyBLE-START"); 
                STDOUT.println("Destroying BLE resources");                   
                destroyBLE();        // Includes STDOUT message
                STDOUT.println("destroyBLE-STOP");      
                return true;
        }

        return false;
    }

    virtual void Help() {}


};

BLEServer* BLE_Serial::pServer = NULL;
BLECharacteristic* BLE_Serial::pTxCharacteristic = NULL;
BLECharacteristic* BLE_Serial::pRxCharacteristic = NULL;
BLEService* BLE_Serial::pService = NULL;
BLE2902* BLE_Serial::bleDescriptor = NULL;
#ifdef BL_MUTEX_PROTECT
SemaphoreHandle_t BLE_Serial::read_mutex = NULL;
SemaphoreHandle_t BLE_Serial::tx_mutex = NULL;
#endif
RingbufHandle_t BLE_Serial::rxBuf_handle = NULL;
RingbufHandle_t BLE_Serial::txBuf_handle = NULL;
bool BLE_Serial::deviceConnected = false;
bool BLE_Serial::oldDeviceConnected = false;
uint16_t BLE_Serial::peerMTU=512;

uint32_t BLE_Serial::lastTriggerTime = 0;
uint32_t BLE_Serial::lastSendTime = 0;
bool BLE_Serial::txTriggered = false;


BLE_Serial BLSERIAL;

#endif