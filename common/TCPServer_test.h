#ifndef COMMON_TCPSERIAL_H
#define COMMON_TCPSERIAL_H

#ifdef WIFI_ENABLED_ESP32
#include <WiFi.h>         // include the right library for ESP32

#include "Stream.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// this tcp_server demo-code creates its own WiFi-network 
// where the tcp_client demo-code connects to
// the ssid and the portNumber must be the same to make it work

#define WIFI_INT_BUFFER_SIZE  1152
#define WIFI_ADVERTISE_TIME  60000       // advertise on WIFI for 1 minute (or until connected)

#define WIFI_MAX_SSID_LEN 64
#define WIFI_MAX_PASS_LEN 64

#define WIFI_DEBUG_SERIAL

#define WIFI_MUTEX_PROTECT
#ifdef WIFI_MUTEX_PROTECT

#define WIFISER_TX_MUTEX_LOCK()    do {} while (xSemaphoreTake(tx_wifi_mutex, portMAX_DELAY) != pdPASS)    
#define WIFISER_TX_MUTEX_UNLOCK()  xSemaphoreGive(tx_wifi_mutex)

#define WIFISER_RX_MUTEX_LOCK()    do {} while (xSemaphoreTake(read_wifi_mutex, portMAX_DELAY) != pdPASS)    
#define WIFISER_RX_MUTEX_UNLOCK()  xSemaphoreGive(read_wifi_mutex)
#else 
#define WIFISER_RX_MUTEX_LOCK()
#define WIFISER_RX_MUTEX_UNLOCK() 

#define WIFISER_TX_MUTEX_LOCK()   
#define WIFISER_TX_MUTEX_UNLOCK()
#endif

#include <Preferences.h>
#include "nvs.h"
#include "nvs_flash.h"

Preferences prefs;

typedef struct {
  uint8_t valid;
  char ssid[WIFI_MAX_SSID_LEN];
  char pass[WIFI_MAX_PASS_LEN];
}lastWifiCfg;

class WifiSerialManager: public CommandParser, public Stream {

    public:
     
     void print_lastCFG(lastWifiCfg* lcfg)
     {
      STDOUT.print("my last cfg: ");
      STDOUT.println(lcfg->valid);
      STDOUT.print("ssid: ");
      STDOUT.println(lcfg->ssid);
      STDOUT.print("pass: ");
      STDOUT.println(lcfg->pass);
     }
     void print_nvsStat()
     {
        nvs_stats_t nvs_stats;
        esp_err_t err = nvs_get_stats(NULL, &nvs_stats);
        if(err){
          STDOUT.println("Failed to get nvs statistics");   
          return;
        }

        STDOUT.print("used_entries ");
        STDOUT.println(nvs_stats.used_entries);
        STDOUT.print("free_entries ");
        STDOUT.println(nvs_stats.free_entries);
        STDOUT.print("total_entries");
        STDOUT.println(nvs_stats.total_entries);
        STDOUT.print("namespace_count");
        STDOUT.println(nvs_stats.namespace_count);
     }
     
     void erase_lastCFG()
     {     
      prefs.begin("lastCfg");
      prefs.clear();
      prefs.end();
     }
     void save_lastCFG(const char *ssid, const char* pass)
     {
      prefs.begin("lastCfg");
      char localSSID[WIFI_MAX_SSID_LEN];
      char localPass[WIFI_MAX_PASS_LEN];

      strcpy(localSSID, ssid);  // save them locally 
      strcpy(localPass, pass);

      memset(&myLastCfg, 0, sizeof(lastWifiCfg));
      prefs.clear();
      myLastCfg.valid = 1;
      strcpy(myLastCfg.ssid, localSSID);
      strcpy(myLastCfg.pass, localPass);
      print_lastCFG(&myLastCfg);

      prefs.putBytes("lastCfg", &myLastCfg, sizeof(lastWifiCfg)); 
      prefs.end();
     }

     void read_lastCFG()
     {
        prefs.begin("lastCfg"); // use "schedule" namespace
        if(!prefs.getBytes("lastCfg", &myLastCfg, sizeof(lastWifiCfg)))
        {
          STDOUT.println("No saved configuration");
          print_nvsStat();
          prefs.end();    
          return;  
        }
        size_t schLen = prefs.getBytesLength("lastCfg");
        STDOUT.print("Min bytes for structure stored ");
        STDOUT.println(sizeof(lastWifiCfg));
        STDOUT.print("Size of bytes found stored");
        STDOUT.println(schLen);
        print_lastCFG(&myLastCfg);
        //startAP("myESP32_AP", "parola");
        prefs.end(); 
     }

     void triggerConnToLastCFG()
     {
      if(!wificonnected && !pendingLastCon)
        triggerLastCFG = true;
     }

     void connectToLastCFG()
     {
      if(!wificonnected && !pendingLastCon) {
        pendingLastCon = true;
        read_lastCFG();
        if(myLastCfg.valid)  // we have a valid cfg , so try to connect to wifi 
        {
          StatusLed_comm.setStatus(StatusLed_Comm::COMMSTATE_WIFISEARCH); 
          connectToAP(myLastCfg.ssid, myLastCfg.pass);
          if(wificonnected) {
            startTCPServer(65215);
            STDOUT.print("tcp listening at port ");
            STDOUT.println(portNumber);
            pendingLastCon = false;
            return;
          } else {
            STDOUT.println("Start scaning networks");
            if(scanAvailableNetworks(true))
            {
              connectToAP(myLastCfg.ssid, myLastCfg.pass);
              if(wificonnected) {
                startTCPServer(65215);
                STDOUT.print("tcp listening at port ");
                STDOUT.println(portNumber);
                pendingLastCon = false;
                return;
              }
            }
          }
        }
        StatusLed_comm.setStatus(StatusLed_Comm::COMMSTATE_DISCONNECTED); 
        // if(!BLSERIAL.GetAdvStatus())
        pendingLastCon = false;
        BLSERIAL.StartAdvBLE();
      }
        
     }

     void Setup()
     {
        // connectToLastCFG();
     }

     void Loop()
     {
      if(wificonnected) {
        if (!clientActive) {
          if(TCPserver) {
            // listen for incoming clients
            TCPclient = TCPserver->available();
            if (TCPclient) {
              delay(20);
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("\n Got a client connected to my WiFi !");
              #endif
              if (TCPclient.connected()) {
                delay(20);
                #ifdef WIFI_DEBUG_SERIAL
                STDOUT.println("an now this client has connected over TCP!");
                #endif
                clientActive = true;
              } 
              // else {
              //   delay(20);
              //   #ifdef WIFI_DEBUG_SERIAL
              //   STDOUT.println("but it's not connected over TCP!");
              //   #endif        
              //   // TCPclient.stop();  // close the connection:
              // }
            }
          }
        } else {

          if (!TCPclient.connected()) {
            delay(20);
            #ifdef WIFI_DEBUG_SERIAL
            STDOUT.println("Client has disconnected the TCP-connection");
            #endif
            TCPclient.stop();  // close the connection:
            clientActive = false;
          } else {
            if(rxBuf_wifi_handle) {
              WIFISER_RX_MUTEX_LOCK();
              int bytesLen = TCPclient.available();
              if(bytesLen>WIFI_INT_BUFFER_SIZE) bytesLen = WIFI_INT_BUFFER_SIZE;
              if (bytesLen  > 0) {
                  bytesLen = TCPclient.read(readBfr, bytesLen); // dont like this 
                  if(bytesLen > 0) {  
                    UBaseType_t res =  xRingbufferSend(rxBuf_wifi_handle, (void*)readBfr, bytesLen, pdMS_TO_TICKS(1000));
                    if (res != pdTRUE) {
                        // TODO make a print maybe to signal failure
                    }
                  }
              }
              WIFISER_RX_MUTEX_UNLOCK();
            }  // we dont have a buffer to put data !!! 


            // WIFISER_TX_MUTEX_LOCK();
            size_t available = 0;
            vRingbufferGetInfo(txBuf_wifi_handle,  NULL, NULL, NULL, NULL, &available);
            // WIFISER_TX_MUTEX_UNLOCK();
            if(available) {
                WIFISER_TX_MUTEX_LOCK();
                size_t  dlen = 0, available = 0;
                uint8_t * tx_data = NULL;
                
                vRingbufferGetInfo(txBuf_wifi_handle, NULL, NULL, NULL, NULL, &available);
                if(available > WIFI_INT_BUFFER_SIZE)available = WIFI_INT_BUFFER_SIZE;  // send up to 512 bytes 

                tx_data = (uint8_t *)xRingbufferReceiveUpTo(txBuf_wifi_handle, &dlen, 0, available);
                if(!tx_data) {
                    WIFISER_TX_MUTEX_UNLOCK();
                    return;
                }
                // memcpy(data+so_far, tx_data, dlen);
                // pTxCharacteristic->setValue((uint8_t *)tx_data, dlen);
                // pTxCharacteristic->notify();
                TCPclient.write((uint8_t *)tx_data, dlen);
                vRingbufferReturnItem(txBuf_wifi_handle, tx_data);
                WIFISER_TX_MUTEX_UNLOCK();
            }
          }
        }
      } else {
        if(triggerLastCFG)
        {
          triggerLastCFG = false;
          connectToLastCFG();
        }
      }
     }
      /* @brief   : wifi event listener 
      *  @param   : event : wifi event 
      *  @retval  : void
      */
      void static wifiEventListener(WiFiEvent_t event) {

        switch(event) {
          case ARDUINO_EVENT_WIFI_AP_START:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("AP Started");
              #endif
              // WiFi.softAPsetHostname(ssid);
              currentMode = WIFI_MODE_AP;
              break;
          case ARDUINO_EVENT_WIFI_AP_STOP:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("AP Stopped");
              #endif
              currentMode = WIFI_MODE_NULL;
              break;
          case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("a station connected to ESP32 soft-AP");
              #endif
              wificonnected = true;
              break;
          case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("a station disconnected to ESP32 soft-AP");
              #endif
              wificonnected = false;
              StatusLed_comm.setStatus(StatusLed_Comm::COMMSTATE_DISCONNECTED); 
              break;
          case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("ESP32 soft-AP assign an IP to a connected station");
              #endif
              break;
          case ARDUINO_EVENT_WIFI_READY:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("WiFi interface ready");
              #endif
              break;
          case ARDUINO_EVENT_WIFI_SCAN_DONE:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("Completed scan for access points");
              #endif
              break;
          case ARDUINO_EVENT_WIFI_STA_START:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("WiFi client started");
              #endif
              break;
          case ARDUINO_EVENT_WIFI_STA_STOP:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("WiFi clients stopped");
              #endif
              break;
          case ARDUINO_EVENT_WIFI_STA_CONNECTED:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("Connected to access point");
              #endif
              currentMode = WIFI_MODE_STA;
              break;
          case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
              if(wificonnected) {
                #ifdef WIFI_DEBUG_SERIAL
                STDOUT.println("Disconnected from WiFi access point");
                StatusLed_comm.setStatus(StatusLed_Comm::COMMSTATE_DISCONNECTED); 
                #endif
              }
              currentMode = WIFI_MODE_NULL;
              wificonnected = false;
              
              break;
          case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("Authentication mode of access point has changed");
              #endif
              break;
          case ARDUINO_EVENT_WIFI_STA_GOT_IP:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.print("Obtained IP address: ");
              ip = WiFi.localIP();
              STDOUT.println(ip);
              #endif
              wificonnected = true;
              StatusLed_comm.setStatus(StatusLed_Comm::COMMSTATE_WIFICONNECTED);
              break;
          case ARDUINO_EVENT_WIFI_STA_LOST_IP:
              #ifdef WIFI_DEBUG_SERIAL
              STDOUT.println("Lost IP address and IP address is reset to 0");
              #endif
              break;

          default:
              break;
        }
      }

    /* @brief   : public constructor 
    *  @param   :
    *  @retval  :  
    */
    WifiSerialManager(void) : CommandParser()
    {
      currentMode = WIFI_MODE_NULL;
      wificonnected = false;
      clientActive = false;
      TCPserver = NULL;
      TCPclient = NULL;
      memset(ssid, 0, WIFI_MAX_SSID_LEN);
      portNumber = 0;
      triggerLastCFG = false;
      pendingLastCon = false;

      #ifdef WIFI_MUTEX_PROTECT
      read_wifi_mutex = xSemaphoreCreateMutex(); // Create read mutex
      tx_wifi_mutex = xSemaphoreCreateMutex(); // Create tx mutex
      #endif
      rxBuf_wifi_handle = xRingbufferCreate(WIFI_INT_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
      txBuf_wifi_handle = xRingbufferCreate(WIFI_INT_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);

    }

     /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    int available(void)
    {
        return wifi_get_buffered_bytes_len();
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
        return clientActive;
    }

    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    int read(void)
    {   
        uint8_t c = 0;
        if(wifi_read_bytes(&c, 1) == 1)
        {
            return c;
        }
        return -1;
    }

    size_t read(uint8_t *buffer, size_t size)
    {
        return wifi_read_bytes(buffer, size);

    }
    size_t readBytes(uint8_t *buffer, size_t size)
    {
        return wifi_read_bytes(buffer, size);
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

        WIFISER_TX_MUTEX_LOCK();  // aquire lock , it will be unlock just after sending 
                                 // make some tst here 
        UBaseType_t res =  xRingbufferSend(txBuf_wifi_handle, (void*)buffer, size, 0);
        if (res != pdTRUE) {
                    // TODO make a print maybe to signal failure
          #ifdef WIFI_DEBUG_SERIAL
            STDOUT.println("Failed to upload buffer ");
          #endif  
        }
        WIFISER_TX_MUTEX_UNLOCK(); 

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

    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    void setRxBufferSize(int nr)
    {

    }
    
    /* @brief   :
    *  @param   :
    *  @retval  :  
    */
    void begin(int b)
    {

    }

    bool getWifiStatus()
    {
      return wificonnected;
    }

    void disconnect()
    {
      if(currentMode == WIFI_MODE_AP)
      {
        stopAP();
      } else if(currentMode == WIFI_MODE_STA)
      {
        disconnectFromAP();
      }
      stopTCPServer();
    }

    private:
    /* @brief   : Connect to access point 
    *  @param   :
    *  @retval  :  
    */
    bool scanAvailableNetworks(bool matchLastCfg = false)
    {
      bool lastCfgFound = false;
      // Set WiFi to station mode and disconnect from an AP if it was previously connected.
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      delay(100);
      #ifdef WIFI_DEBUG_SERIAL
      STDOUT.println("Scan start");
      #endif
      int n = WiFi.scanNetworks();
      #ifdef WIFI_DEBUG_SERIAL
      STDOUT.println("Scan done");
      #endif
      if (n <= 0) {
          #ifdef WIFI_DEBUG_SERIAL
          STDOUT.println("no networks found");
          #endif
      } else {
      #ifdef WIFI_DEBUG_SERIAL
          STDOUT.print(n);
          STDOUT.println(" networks found");
          for (int i = 0; i < n; i++) {
              // Print SSID and RSSI for each network found
              STDOUT.print(WiFi.SSID(i));
              STDOUT.print(", ");
              STDOUT.println(WiFi.RSSI(i));
              if(matchLastCfg)
              {
                if(!WiFi.SSID(i).compareTo(String(myLastCfg.ssid)))
                {
                  lastCfgFound = true;
                  STDOUT.println("Network found in preferences");
                }

              }
              // switch (WiFi.encryptionType(i))
              // {
              // case WIFI_AUTH_OPEN:
              //     STDOUT.print("open");
              //     break;
              // case WIFI_AUTH_WEP:
              //     STDOUT.print("WEP");
              //     break;
              // case WIFI_AUTH_WPA_PSK:
              //     STDOUT.print("WPA");
              //     break;
              // case WIFI_AUTH_WPA2_PSK:
              //     STDOUT.print("WPA2");
              //     break;
              // case WIFI_AUTH_WPA_WPA2_PSK:
              //     STDOUT.print("WPA+WPA2");
              //     break;
              // case WIFI_AUTH_WPA2_ENTERPRISE:
              //     STDOUT.print("WPA2-EAP");
              //     break;
              // case WIFI_AUTH_WPA3_PSK:
              //     STDOUT.print("WPA3");
              //     break;
              // case WIFI_AUTH_WPA2_WPA3_PSK:
              //     STDOUT.print("WPA2+WPA3");
              //     break;
              // case WIFI_AUTH_WAPI_PSK:
              //     STDOUT.print("WAPI");
              //     break;
              // default:
              //     STDOUT.print("unknown");
              // }
              // STDOUT.println();
              // delay(10);
          }
      #endif
      }
      // Delete the scan result to free memory for code below.
      WiFi.scanDelete();
      return lastCfgFound;
    }

    /* @brief   : Connect to access point 
    *  @param   :
    *  @retval  :  
    */
    void connectToAP(char * apName, char * pass)
    {
      WiFi.onEvent(wifiEventListener);
      WiFi.mode(WIFI_MODE_STA);

      STDOUT.println();
      STDOUT.print("[WiFi] Connecting to ");
      STDOUT.println(apName);
      
      WiFi.begin(apName, pass);
      strcpy(ssid, apName);
      // Auto reconnect is set true as default
      // To set auto connect off, use the following function
      //    WiFi.setAutoReconnect(false);

      // Will try for about 10 seconds (20x 500ms)
      int tryDelay = 500;
      int numberOfTries = 20;
      // Wait for the WiFi event
      while (true) {
        switch(WiFi.status()) {
          case WL_NO_SSID_AVAIL:
            STDOUT.println("[WiFi] SSID not found");
            break;
          case WL_CONNECT_FAILED:
            STDOUT.print("[WiFi] Failed - WiFi not connected! Reason: ");
            return;
            break;
          case WL_CONNECTION_LOST:
            STDOUT.println("[WiFi] Connection was lost");
            break;
          case WL_SCAN_COMPLETED:
            STDOUT.println("[WiFi] Scan is completed");
            break;
          case WL_DISCONNECTED:
            STDOUT.println("[WiFi] WiFi is disconnected");
            break;
          case WL_CONNECTED:
            STDOUT.println("[WiFi] WiFi is connected!");
            STDOUT.print("[WiFi] IP address: ");
            ip = WiFi.localIP();
            STDOUT.println(ip);
            wificonnected = true;
            save_lastCFG(apName, pass);
            return;
            break;
          default:
            STDOUT.print("[WiFi] WiFi Status: ");
            STDOUT.println(WiFi.status());
            break;
        }
        delay(tryDelay);
        
        if(numberOfTries <= 0){
          STDOUT.print("[WiFi] Failed to connect to WiFi!");
          // Use disconnect function to force stop trying to connect
          WiFi.disconnect();
          return;
        } else {
          numberOfTries--;
        }
      }

    }

    /* @brief   : Disconnect from access point 
    *  @param   :
    *  @retval  :  
    */
    void disconnectFromAP()
    {
        WiFi.disconnect(true, false);
    }

    /* @brief   : Start access point 
    *  @param   :
    *  @retval  :  
    */
    bool startAP(char * apName, char * pass)
    {
      #ifdef WIFI_DEBUG_SERIAL
      STDOUT.print("Creating AP (Access Point) with name#");
      STDOUT.print(apName);
      STDOUT.println("#");
      #endif
      WiFi.onEvent(wifiEventListener);
      WiFi.mode(WIFI_MODE_AP);

      if(!WiFi.softAP(apName, pass, 1, 0, 1, false))   // ap name, no pass, broadcast (not hidded), max connection , ftm 
      {
        #ifdef WIFI_DEBUG_SERIAL
        STDOUT.println("Failed to create AP ");
        #endif
        return false;
      }

      strcpy(ssid, apName);
      ip = WiFi.softAPIP();
      #ifdef WIFI_DEBUG_SERIAL
      STDOUT.print(" -> softAP with IP address: ");
      STDOUT.println(ip);
      #endif

      return true;
    }

    /* @brief   : start tcp server   
    *  @param   : portNr
    *  @retval  : true - success
    *             false - failed 
    */
    bool startTCPServer(uint16_t portNr)
    {
      if(TCPserver)
      {
        #ifdef WIFI_DEBUG_SERIAL
        STDOUT.println("TCP server active");
        #endif
        return false;
      }
      TCPserver = new WiFiServer(portNr);
      if(!TCPserver)
      {
        #ifdef WIFI_DEBUG_SERIAL
        STDOUT.println("Failed to create tcp server");
        #endif
        return false;
      }
      TCPserver->begin();
      portNumber = portNr;
      return true;
    }

    /* @brief   : Stop tcp server  
    *  @param   : void 
    *  @retval  :  
    */
    void stopTCPServer()
    {
      if(!TCPserver) return; // noting to stop
      // maybe add some disconenct function
      if(clientActive){
        TCPclient.stop();
        clientActive = false;
      }
      TCPserver->end();
      // delete TCPserver;
      TCPserver = NULL;
    }

    /* @brief   : Stop access point  
    *  @param   : void
    *  @retval  : void   
    */
    void stopAP()
    {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_OFF);
    }

    void printHeap()
    {
      // STDOUT.print(" heapSize:     ");STDOUT.print(ESP.getHeapSize()); 
      // STDOUT.print(" heapfree:     ");STDOUT.print(ESP.getFreeHeap());
      // STDOUT.print(" heapMinFree:  ");STDOUT.print(ESP.getMinFreeHeap()); 
      // STDOUT.print(" heapMaxAlooc: ");STDOUT.println(ESP.getMaxAllocHeap());
    }

    virtual bool Parse(const char* cmd, const char* arg) 
    {
      if(!strcmp(cmd, "printLastWifiCFG"))
      {
        STDOUT.println("printLastWifiCFG-START");
        read_lastCFG();
        STDOUT.println("printLastWifiCFG-STOP");
        return true;  
      }

      if(!strcmp(cmd, "saveWifiCFG"))   // only for test purpose to save a wrong cfg so we cant connect
      {
        STDOUT.println("saveWifiCFG-START");
        if(arg) {
           char *String = (char*)arg;
          char ssidname[WIFI_MAX_SSID_LEN];
          char ssidpass[WIFI_MAX_PASS_LEN];
          char *token;
          for(uint8_t i=0; i<WIFI_MAX_SSID_LEN;i++)
          {
            ssidname[i] = 0;
            ssidpass[i] = 0;
          }

          token = strtok(String, ",");
          uint8_t i = 0;
          while( token != NULL ) {
            if(!i) {
              sprintf(ssidname, "%s", token);
              STDOUT.print("ssidname "); STDOUT.println(ssidname);
              i++;
            } else {
              sprintf(ssidpass, "%s", token);
              STDOUT.print("ssidpass "); STDOUT.println(ssidpass);
              i++;
              break;
            }
            token = strtok(NULL, ",");
          }
          if(i==2) {
            save_lastCFG(ssidname, ssidpass);
          }
        }
        STDOUT.println("saveWifiCFG-STOP");
        return true;  
      }

      if(!strcmp(cmd, "eraseLastWifiCFG"))
      {
        STDOUT.println("eraseLastWifiCFG-START");
        print_nvsStat();
        erase_lastCFG();
        print_nvsStat();
        STDOUT.println("eraseLastWifiCFG-STOP");
        return true;  
      }

      if(!strcmp(cmd, "printNVSStats"))
      {
        STDOUT.println("printNVSStats-START");
        print_nvsStat();
        STDOUT.println("printNVSStats-STOP");
        return true;  
      }


      if (!strcmp(cmd, "startWifiAP")) {
        STDOUT.println("startWifiAP-START");  
        printHeap();
        if(arg) {
           char *String = (char*)arg;
          char ssidname[WIFI_MAX_SSID_LEN];
          char ssidpass[WIFI_MAX_PASS_LEN];
          char *token;
          for(uint8_t i=0; i<WIFI_MAX_SSID_LEN;i++)
          {
            ssidname[i] = 0;
            ssidpass[i] = 0;
          }

          token = strtok(String, ",");
          uint8_t i = 0;
          while( token != NULL ) {
            if(!i) {
              sprintf(ssidname, "%s", token);
              STDOUT.print("ssidname "); STDOUT.println(ssidname);
              i++;
            } else {
              sprintf(ssidpass, "%s", token);
              STDOUT.print("ssidpass "); STDOUT.println(ssidpass);
              i++;
              break;
            }
            token = strtok(NULL, ",");
          }
          if(i==2) {
            startAP(ssidname, ssidpass);
          }
        } else {
          startAP("myESP32_AP", "parola123");
        }

        printHeap();
        STDOUT.println("startWifiAP-STOP");  
        return true;
      }

      if (!strcmp(cmd, "stopWifiAP")) {
        STDOUT.println("stopWifiAP-START");  
        printHeap();
        stopAP(); 
        printHeap();
        STDOUT.println("stopWifiAP-STOP");
        return true;
      }

      if (!strcmp(cmd, "connectToAP")) {
        if (arg) {
          STDOUT.println("connnectToAp-START");
          char *String = (char*)arg;
          char ssidname[WIFI_MAX_SSID_LEN];
          char ssidpass[WIFI_MAX_PASS_LEN];
          char *token;
          for(uint8_t i=0; i<WIFI_MAX_SSID_LEN;i++)
          {
            ssidname[i] = 0;
            ssidpass[i] = 0;
          }

          token = strtok(String, ",");
          uint8_t i = 0;
          while( token != NULL ) {
            if(!i) {
              sprintf(ssidname, "%s", token);
              STDOUT.print("ssidname "); STDOUT.println(ssidname);
              i++;
            } else {
              sprintf(ssidpass, "%s", token);
              STDOUT.print("ssidpass "); STDOUT.println(ssidpass);
              i++;
              break;
            }
            token = strtok(NULL, ",");
          }

          if(i == 2)
          {
            printHeap();
            connectToAP(ssidname, ssidpass);
            printHeap();
          }
          STDOUT.println("connnectToAp-STOP");
        }
        return true;
      }

      if (!strcmp(cmd, "disconnectFromAP")) {
        STDOUT.println("disconnectFromAP-START");
        printHeap();
        STDOUT.println("disconnectFromAP ");
        disconnectFromAP();
        printHeap();
        STDOUT.println("disconnectFromAP-STOP");
        return true;
      }

      if(!strcmp(cmd, "scanNetworks")) {
        STDOUT.println("scanNetworks-START");
        printHeap();
        //STDOUT.println("loooking for networks... ");
        scanAvailableNetworks();
        printHeap();
        STDOUT.println("scanNetworks-STOP");
        return true;
      }

      if(!strcmp(cmd, "startTCP")) {
        STDOUT.println("startTCP-START");
        printHeap();
        if(startTCPServer(65215)){
            STDOUT.print("tcp listening at port ");
            STDOUT.println(portNumber);
        }
        printHeap();
        STDOUT.println("startTCP-STOP");
        return true;
      }

      if(!strcmp(cmd, "stopTCP")) {
        STDOUT.println("stopTCP-START");
        printHeap();
        stopTCPServer();
        STDOUT.println("Stopping TCP");
        printHeap();
        STDOUT.println("stopTCP-STOP");
        return true;
      }

      if(!strcmp(cmd, "getStatus")) {
        STDOUT.println("getStatus-START");
        STDOUT.print("Mode: ");
        switch(currentMode) {
          case WIFI_MODE_NULL:
            STDOUT.println("off");
            break;
          case WIFI_MODE_STA:
            STDOUT.println(" station");
            STDOUT.print(" Connected To ");STDOUT.println(ssid);
            break;
          case WIFI_MODE_AP:
            STDOUT.println("ap");
            STDOUT.print(" Ap name ");STDOUT.println(ssid);
            break;
          case WIFI_MODE_APSTA:
            STDOUT.println("station + ap");
            break;
        }

        STDOUT.print("TCP server: ");
        if(TCPserver) {
          STDOUT.println("enabled");
          STDOUT.print("portNR: ");STDOUT.println(portNumber);
        } else {
          STDOUT.println(" disabled");
          STDOUT.println(" portNR: 0");
        }
        STDOUT.print("IP: ");
        STDOUT.println(ip);


        STDOUT.println("getStatus-STOP");
        return true;
      }

      return false;

    }

      
    size_t wifi_read_bytes(uint8_t *data, size_t item_size)
    {
        //Receive data from byte buffer
        if(!data) return 0;
        WIFISER_RX_MUTEX_LOCK();
        size_t  dlen = 0, to_read = item_size, so_far = 0, available = 0;
        uint8_t * rx_data = NULL;
        
        vRingbufferGetInfo(rxBuf_wifi_handle, NULL, NULL, NULL, NULL, &available);
        if(available < to_read) {
            to_read = available;
        }

        while(to_read){
        dlen = 0;
        rx_data = (uint8_t *)xRingbufferReceiveUpTo(rxBuf_wifi_handle, &dlen, 0, to_read);
        if(!rx_data){
            WIFISER_RX_MUTEX_UNLOCK();
            return so_far;
        }
        memcpy(data+so_far, rx_data, dlen);
        vRingbufferReturnItem(rxBuf_wifi_handle, rx_data);
        so_far+=dlen;
        to_read-=dlen;
        }
        WIFISER_RX_MUTEX_UNLOCK();
        return so_far;

    }

    size_t wifi_get_buffered_bytes_len()
    {   
        size_t available = 0;
        WIFISER_RX_MUTEX_LOCK();
        vRingbufferGetInfo(rxBuf_wifi_handle,  NULL, NULL, NULL, NULL, &available);
        WIFISER_RX_MUTEX_UNLOCK();
        return available;
    }
      

    virtual void Help() {}

    char ssid[WIFI_MAX_SSID_LEN];
    uint16_t portNumber;
    uint8_t readBfr[WIFI_INT_BUFFER_SIZE];
    static bool wificonnected;
    bool clientActive;
    static IPAddress ip;
    bool triggerLastCFG;
    bool pendingLastCon;

    lastWifiCfg myLastCfg;

    WiFiServer* TCPserver;
    WiFiClient TCPclient;

    static RingbufHandle_t rxBuf_wifi_handle;
    static RingbufHandle_t txBuf_wifi_handle;

    #ifdef WIFI_MUTEX_PROTECT
    static SemaphoreHandle_t read_wifi_mutex;
    static SemaphoreHandle_t tx_wifi_mutex;
    #endif

    static wifi_mode_t currentMode;  //  WIFI_MODE_STA,       /**< WiFi station mode */
                            //  WIFI_MODE_AP,        /**< WiFi soft-AP mode */
                            //  WIFI_MODE_APSTA,     /**< WiFi station + soft-AP mode */
};

IPAddress WifiSerialManager::ip;
bool WifiSerialManager::wificonnected = false;
wifi_mode_t WifiSerialManager::currentMode = WIFI_MODE_NULL;
#ifdef WIFI_MUTEX_PROTECT
SemaphoreHandle_t WifiSerialManager::read_wifi_mutex = NULL;
SemaphoreHandle_t WifiSerialManager::tx_wifi_mutex = NULL;
#endif
RingbufHandle_t WifiSerialManager::rxBuf_wifi_handle = NULL;
RingbufHandle_t WifiSerialManager::txBuf_wifi_handle = NULL;

WifiSerialManager wifiSerial;
#endif

#endif