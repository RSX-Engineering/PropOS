#ifndef COMMON_SERIAL_H
#define COMMON_SERIAL_H

class SerialAdapter {
public:
  static void begin() {
    // Already configured in Setup().
    // Serial.begin(115200);
  }
  static bool Connected() { return !!Serial; }
  static bool AlwaysConnected() { return false; }
  static Stream& stream() { return Serial; }
  static const char* response_header() { return ""; }
  static const char* response_footer() { return ""; }
#ifdef OSX_ENABLE_MTP
  static HardwareSerial& refAdapter() {return Serial;}
#endif
};
#ifdef ARDUINO_ARCH_STM32L4   // STM architecture
class Serial3Adapter {
public:
  static void begin() { Serial3.begin(115200); }
  static bool Connected() { return true; }
  static bool AlwaysConnected() { return true; }
  static Stream& stream() { return Serial3; }
  static const char* response_header() { return "-+=BEGIN_OUTPUT=+-\n"; }
  static const char* response_footer() { return "-+=END_OUTPUT=+-\n"; }
};
#endif

#ifdef USB_CLASS_WEBUSB
class WebUSBSerialAdapter {
public:
  static void begin() { WebUSBSerial.begin(115200); }
  // static bool Connected() { return !!WebUSBSerial; }
  static bool Connected() { return true; }
  static bool AlwaysConnected() { return true; }
  static Stream& stream() { return WebUSBSerial; }
  static const char* response_header() { return "-+=BEGIN_OUTPUT=+-\n"; }
  static const char* response_footer() { return "-+=END_OUTPUT=+-\n"; }
};
#endif



#ifdef OSX_ENABLE_MTP

#define MTP_UART_TIMEOUT_SHORT 100
#define MTP_UART_TIMEOUT_LONG 10000

#define SERIAL_PROTOCOL_TRID_LEN            2
#define SERIAL_PROTOCOL_PACKET_LEN          2
#define SERIAL_PROTOCOL_FLAGS               2
#define SERIAL_HEADER   SERIAL_PROTOCOL_TRID_LEN + SERIAL_PROTOCOL_PACKET_LEN + SERIAL_PROTOCOL_FLAGS

#define SERIAL_PROTOCOL_CHECKSUM_LEN        4
#define SERIAL_PROTOCOL_DATAPACHET_MAX_LEN  1044
#define SERIAL_PROTOCOL_FILE_MAX_LEN  1024
#define SERIAL_PROTOCOL_DIR_MAX_LEN   1014

#define SERIAL_PROTOCOL_LEN                 SERIAL_HEADER +  SERIAL_PROTOCOL_DATAPACHET_MAX_LEN + SERIAL_PROTOCOL_CHECKSUM_LEN // 2 + 2 + 2 + 4 + 512

// Board Identification data TODO move the definition from here 
#define BOARD_HWID      0x0100        // 1.0.0
#define BOARD_FWID      0x0102        // 1.0.2
#define BOARD_SERIAL    0x22040000    // year, month, nr 
#define BOARD_ERROR_CNT 0x0000        // zero error // TODO repplace with variable that actually counts error

#ifdef ARDUINO_ARCH_STM32L4   // STM architecture
#include <dosfs_api.h>
#endif

#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
  #include "../common/HardwareID.h"
#endif

template<class SA> /* SA = Serial Adapter */
class Serial_Protocol:  private StateMachine
{
public:
    // Serial_Protocol() : Looper() {}   // SUBSCRIBE TO LOOPER 
    // const char* name() override { return "Serial_Protocol"; }
    Serial_Protocol() 
    {
      this->RefreshProtocolFrame();
    }
    // Simple setter of session state  status   static
    static void SetSession(bool status)
    {   
        StubActiveSerial(status);
        _sessionState = status;
    }
    // Simple getter of session state status 
    static bool GetSession()       // static
    {
        return _sessionState;
    }
    // protocol error flags 
    enum pr_err{
      err_OK       = 0b0000000000000001,    // all good 

      err_Header   = 0b1000000000000000,    // header bytes error 
      err_DataLen  = 0b0100000000000000,    // expected nr of bytes exceed max protocol data len , something is wrong
      err_RecData  = 0b0010000000000000,    // data bytes 
      err_CheckSum = 0b0001000000000000,    // crc error
    };

    enum pr_state{
      prState_Idle = 0x00,                  // protocol inj idle state , ready to receive bytes 
      prState_Loading = 0x01,               // received header bytes , waiting for the rest 
      prState_Ready = 0x02,                 // protcol received a frame and is ready to be processed
      // TODO add necessary state 
      prState_Error = 0xFF,                 // error state , something went wrong 
    };

    /* brief    :
    *  param    :
    *  retval   : 
    */
    void Loop()  // __attribute__((optimize("O0")))
    {                             // override
        // STATE_MACHINE_BEGIN();
        // while (true) {
            // while (!SA::Connected()) YIELD();

            // while (SA::Connected()) {
                SA::stream().flush();
                // bytesAvailable = SA::stream().available();
                // while (!SA::stream().available()) YIELD();

                if (!SA::stream().available()) return;

                if(_protocolState == prState_Idle)
                {
                    if(!WaitBytes(SERIAL_HEADER, MTP_UART_TIMEOUT_SHORT))
                    {
                        this->ConsumeRx(MTP_UART_TIMEOUT_SHORT);
                        this->RefreshProtocolFrame();
                        this->_flags = err_Header;
                        this->SendProtocolAnswer(this->_trID, 0, this->_flags, NULL);
                    } else {

                      this->ReadAndSetHeader();

                      if(this->_dataLen > SERIAL_PROTOCOL_DATAPACHET_MAX_LEN)
                      {
                        this->ConsumeRx(MTP_UART_TIMEOUT_SHORT);
                        this->RefreshProtocolFrame();
                        this->_flags = err_DataLen;
                        this->SendProtocolAnswer(this->_trID, 0, this->_flags, NULL);

                      } else {
                        this->_frameBytesNr += SERIAL_HEADER;
                        this->_protocolState = prState_Loading;
                      }

                    }

                }
                if(_protocolState == prState_Loading)
                {
                    if(!WaitBytes(_dataLen + SERIAL_PROTOCOL_CHECKSUM_LEN, MTP_UART_TIMEOUT_LONG))
                    {
                        this->ConsumeRx(MTP_UART_TIMEOUT_SHORT);
                        this->_flags = err_RecData;
                        this->SendProtocolAnswer(this->_trID, 0, this->_flags, NULL);
                        this->RefreshProtocolFrame();
                    } else {
                      #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
                      SA::stream().read( (uint8_t*)&_protocolFrame[this->_frameBytesNr], this->_dataLen);
                      SA::stream().read( (uint8_t*)&_protocolFrame[this->_frameBytesNr + this->_dataLen], 4);
                      #else 
                      SA::stream().readBytes( (uint8_t*)&_protocolFrame[this->_frameBytesNr], this->_dataLen);
                      SA::stream().readBytes( (uint8_t*)&_protocolFrame[this->_frameBytesNr + this->_dataLen], 4);
                      #endif
                      
                      // SA::stream().read( (uint8_t*)(&_checkSum), 4);
                      this->_checkSum = *(uint32_t*)&_protocolFrame[this->_frameBytesNr + this->_dataLen];
                      if(CalculateCRC32((uint8_t*)&_protocolFrame[0], this->_frameBytesNr + this->_dataLen, 1) != this->_checkSum)
                      {
                          this->ConsumeRx(MTP_UART_TIMEOUT_SHORT);
                          this->_flags = err_CheckSum;

                          this->SendProtocolAnswer(this->_trID, 0, this->_flags, NULL);
                          this->RefreshProtocolFrame();
                      } else {
                          // uint16_t returnBytes;
                          this->ConsumeRx(1);
                          this->_flags = err_OK;
                          // returnBytes = this->CommandProcess((uint8_t*)&_protocolFrame[this->_frameBytesNr], this->_dataLen);
                          // this->SendProtocolAnswer(this->_trID, returnBytes, this->_flags, (uint8_t*)&_protocolFrame[this->_frameBytesNr]);
                          // this->RefreshProtocolFrame();
                          this->_protocolState = prState_Ready;
                      }

                    }
                }


            // } // end Connected 
        // } // end true
        // STATE_MACHINE_END();
    } // end Loop 
 protected:

     /* brief    : Refresh Protocol Frame by setting default values of private members  
    *  param    : void
    *  retval   : void
    */
    void RefreshProtocolFrame() // __attribute__((optimize("O0")))
    {
        this->_frameBytesNr = 0;
        this->_trID= 0;
        this->_dataLen = 0;
        this->_checkSum = 0;
        this->_protocolState= prState_Idle;
    }
     /* brief    : Read bytes thar are in queue , 
    *  param    : timeout - timeout in millis
    *  retval   :  void
    */
    void ConsumeRx(uint32_t timeout) // __attribute__((optimize("O0")))
    {
        uint32_t timeStamp = millis();
        while(SA::stream().available() || (millis() - timeStamp < timeout) )
            SA::stream().read();
    }
    /*  @brief  :
    *   @param : description
    *   @retval return value description
    */
    static void StubActiveSerial(bool stubType)// __attribute__((optimize("O0")))
    {
      if(stubType) {
        SA::stream().flush();
        savedDefault_output = default_output;
        savedStdout_output = stdout_output;
        default_output = &EmptySerial;
        stdout_output = &EmptySerial;

         SA::refAdapter().end();
         SA::refAdapter().begin(SERIAL_BIN_BAUD);

      } else {
        default_output = savedDefault_output;
        stdout_output = savedStdout_output;
      }

    }
    /* brief    : Send Protocol answer . calculating the crc of trId - to dataBfr
    *  param    : trID    - Transaction ID 
                  dataLen - nr of bytes of databfr 
                  flags   - error flags 
                  dataBfr - data we want to send
    *  retval   : void
    */
    void SendProtocolAnswer(uint16_t trID, uint16_t dataLen, uint16_t flags, uint8_t* dataBfr, uint8_t reset = 0) // __attribute__((optimize("O0")))
    {
        uint32_t lCheckSum = 0;
        uint16_t crcBytesLen;

        *(uint16_t*)(_protocolFrame) = trID;
        *(uint16_t*)((_protocolFrame+2)) = dataLen;
        *(uint16_t*)((_protocolFrame+4)) = flags;
        crcBytesLen = 6;
        // lCheckSum = CalculateCRC32((uint8_t*)(&trID), 2, 1);
        // lCheckSum = CalculateCRC32((uint8_t*)(&dataLen), 2, 0);
        // lCheckSum = CalculateCRC32((uint8_t*)(&flags), 2, 0);
        if(dataLen)
          crcBytesLen += dataLen;
          //lCheckSum = CalculateCRC32(dataBfr, dataLen, 0);
        lCheckSum = CalculateCRC32((uint8_t*)(&_protocolFrame), crcBytesLen, 1);

        SA::stream().flush();
        SA::stream().write( (uint8_t*)(&trID), 2);
        SA::stream().flush();
        SA::stream().write( (uint8_t*)(&dataLen), 2);
        SA::stream().flush();
        SA::stream().write( (uint8_t*)(&flags), 2);
        SA::stream().flush();
        if(dataLen)
        {
          SA::stream().write( dataBfr, dataLen);
          SA::stream().flush();
        }
        SA::stream().write( (uint8_t*)(&lCheckSum), 4);
        SA::stream().flush();

        if(!_sessionState) {
           SA::refAdapter().end();
           SA::refAdapter().begin(SERIAL_ASCII_BAUD);
        }

        if(reset)
        { 
          #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
          STM32.reset(); // resseting
          #else 
          ESP.restart();
          #endif
        }
    }
    /* brief    : Read and set protocol values
    *  param    : void
    *  retval   : void
    */
    void ReadAndSetHeader() // __attribute__((optimize("O0")))
    {   
      #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
        SA::stream().read( (uint8_t*)&_protocolFrame[0], 2);
        SA::stream().read( (uint8_t*)&_protocolFrame[2], 2); 
        SA::stream().read( (uint8_t*)&_protocolFrame[4], 2);
      #else 
        SA::stream().readBytes( (uint8_t*)&_protocolFrame[0], 2);
        SA::stream().readBytes( (uint8_t*)&_protocolFrame[2], 2); 
        SA::stream().readBytes( (uint8_t*)&_protocolFrame[4], 2);
      #endif

        this->_trID = *(uint16_t*)&_protocolFrame[0];
        this->_dataLen = *(uint16_t*)&_protocolFrame[2];
        this->_flags = *(uint16_t*)&_protocolFrame[4];

    }
    /* brief    : Calculate CRC32 of data receinved data 
    *  param    : buffer    - data we will calculate the CRC
                  nrOFBytes - nr of bytes buffer contains
                  reset     - 0 will acumulate else crc will reset and then calculate 
    *  retval   : calculated CRC 
    */
    uint32_t CalculateCRC32(uint8_t *buffer, uint16_t nrOfBytes, uint8_t reset) // __attribute__((optimize("O0")))
    {   
        uint32_t calcCrc = 0;
        #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
        if(reset)
          calcCrc = HAL_CRC_Calculate(&stm32l4_crc, (uint32_t*)buffer, nrOfBytes);
        else 
          calcCrc = HAL_CRC_Accumulate(&stm32l4_crc, (uint32_t*)buffer, nrOfBytes);
        #else 
        if(reset)
          calcCrc = STCrc_impl.CRC_Calculate((uint32_t*)buffer, nrOfBytes);
        else 
          calcCrc = STCrc_impl.CRC_Acumulate((uint32_t*)buffer, nrOfBytes);
        #endif

        return calcCrc;
    }
    /* brief    : Wait nr of bytes on serial with deseried timeout 
    *  param    : nrOfByes - nr of bytes we want to wait
                  timeout  - miillis we want to wait the desired bytes
    *  retval   : true - nr of bytes we want to wait are available
                  false - timeout reached 
    */
    bool WaitBytes(uint16_t nrOfBytes, uint32_t timeout) // __attribute__((optimize("O0")))
    {   
        uint32_t timeStamp = millis();
        bool status = false;
        while(millis() - timeStamp < timeout)
        {   
            if(SA::stream().available()>= nrOfBytes)
            {
                status = true;
                break;
            }
        }
        return status;
    }


    uint8_t  _protocolState;
    uint16_t _frameBytesNr;
    uint16_t _trID;
    uint16_t _dataLen;
    uint16_t _flags;
    uint32_t _checkSum;
    uint8_t _protocolFrame[SERIAL_PROTOCOL_LEN];
    static bool _sessionState;
    static Print* savedDefault_output;
    static Print* savedStdout_output;
};  // end Serial_Protocol
#ifdef ARDUINO_ARCH_ESP32   // ESP architecture
#define F_MAXPATH 128
#endif
template<class SA> /* SA = Serial Adapter */
class Serial_Transfer : public Serial_Protocol<SA>, CommandParser,Looper
{
  public:
  // constructor 
      // Serial_Protocol() : Looper() {}   // SUBSCRIBE TO LOOPER 

  Serial_Transfer() : Looper()   // SUBSCRIBE TO LOOPER 
  {
    this->_lockState = 1;
    this->_sessionTimeStamp = 0;
  }

  bool Parse(const char* cmd, const char* arg) override {
    if (!strcmp(cmd, "openSession"))
    { 
      // mtpUart.SetSession(true);
      STDOUT.println("Opening Session...\n");
      // TODO find a way to stop all current working service because the session is captured by the the PC 
      Serial_Protocol<SA>::RefreshProtocolFrame();
      Serial_Protocol<SA>::ConsumeRx(MTP_UART_TIMEOUT_SHORT);

      Serial_Protocol<SA>::SetSession(true);

      if(SaberBase::IsOn()) // if blade is on , turn it off 
      {
        SaberBase::TurnOff(SaberBase::OffType::OFF_NORMAL); // turn off blade
        for (size_t i = 0; i < NELEM(wav_players); i++) { // turn off audio
        wav_players[i].Stop();
        }
  
      }

      if(SaberBase::MotionRequested())  // if motion is working
        // SaberBase::TimeoutRequestMotion();
      DisableMotion();

      return true;
    }
    return false;
  }

  void Help() override {
    #if defined(COMMANDS_HELP) 
    STDOUT.println(" openSession - open uart MTP ");
    #endif
  }

  const char* name() override { return "Serial_Transfer"; }

    /* @brief  : Serial transfer worker looper   
    *  @param  : void
    *  @retval : void
    */
    void Loop()
    {   
        if(Serial_Protocol<SA>::GetSession()) 
        {
          Serial_Protocol<SA>::Loop();
          if(Serial_Protocol<SA>::_protocolState == Serial_Protocol<SA>::pr_state::prState_Ready)
          {
            uint16_t returnBytes;
            uint8_t cmdReset;
            cmdReset = *((uint8_t*)&(Serial_Protocol<SA>::_protocolFrame[this->_frameBytesNr]));
            returnBytes = this->CommandProcess((uint8_t*)&(Serial_Protocol<SA>::_protocolFrame[this->_frameBytesNr]), Serial_Protocol<SA>::_dataLen);
            if(cmdReset == DeviceReset) 
            {
              cmdReset = *((uint8_t*)&(Serial_Protocol<SA>::_protocolFrame[this->_frameBytesNr]));
              if(cmdReset == trOk)  Serial_Protocol<SA>::SendProtocolAnswer(Serial_Protocol<SA>::_trID, returnBytes, Serial_Protocol<SA>::_flags, (uint8_t*)&(Serial_Protocol<SA>::_protocolFrame[this->_frameBytesNr]), 1);
            }  
            Serial_Protocol<SA>::SendProtocolAnswer(Serial_Protocol<SA>::_trID, returnBytes, Serial_Protocol<SA>::_flags, (uint8_t*)&(Serial_Protocol<SA>::_protocolFrame[this->_frameBytesNr]));
            Serial_Protocol<SA>::RefreshProtocolFrame();

          }
        }
    }
      private : 
      
      int16_t scanForNULL(uint8_t *buffer) {
        int16_t pfi = -1;
        #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
        for(int16_t i=0; i < F_MAXPATH; i++ )
        #else
        for(int16_t i=0; i < 128; i++ )
        #endif
        {
          if(*(buffer + i) == 0) {
            pfi = i;
            break;
          }
        }
        return pfi;
      }
    enum cmds
    {   // ----- Session control cmds 0x0*-------------------
        Hello         = 0x01, 
        Unlock        = 0x02,
        Bye           = 0x03,
        // ----- Device control cmds 0x2* -------------------
        DeviceReset   = 0x20,
        DeviceErrors  = 0x21,
        // ----- Memory managment cmds 0x4* ----------------
        FS_Info       = 0x40,
        FS_Format     = 0x41,
        FS_GetDirFile = 0x42,
        LSFS_Remove   = 0x43,
        LSFS_RemoveFiles = 0x44,
        LSFS_Rename = 0x45,
        // FS_GetDirFile_old = 0x45,
        // ----- Memory access cmds 0x5* -------------------
        LSFS_OpenRead = 0x50,
        LSFS_OpenWrite = 0x51,
        File_Write = 0x52,
        File_Read = 0x53,
        File_Close = 0x54,
        File_CRC = 0x55,
        // --- obsolete TODO delete 
        File_Write_old = 0x56, 

    };
    // opration error codes 
    enum transferErr
    {
      trOk = 0x00,

      trOpenFirst   = 0x01,
      trAlreadyOpen = 0x02,
      trNothingtoClose = 0x03,
      trPathAttIllegal = 0x04,
      trPathNotFound = 0x05,
      // trCloseOpenFiles = 0x04,
      trLocked = 0xFD,
      trUnknown = 0xFE,
      trFail = 0XFF,
    };  
    /* @brief    : Interpret and execute received cmds
    *  @param    : *cmd - pointer to util data of protocol 
    *              cmdLen - lenght of cmd buffer 
    *  @retval   : response Lenght
    */
   uint16_t CommandProcess(uint8_t *cmd, uint16_t cmdLen)  //   __attribute__((optimize("Og")))
   {  
     uint16_t byteReturn = 0;
     int16_t res;

      switch(*cmd)
      {
        // ================================================ SESSION CONTROL CMDS ===============================================
        // ---------- HELLO Get device identification and initiate comunication session ---------------------------------------
        case Hello:
        {
          this->_lockState = 1;                                   // reset to default 
          *cmd = trOk;
          #if defined(PROFFIEBOARD) || ( defined(ULTRAPROFFIE) && ULTRAPROFFIE_VERSION == 'P')
          *(uint32_t*)(cmd + 1)  = (uint32_t)17966079;          // HW VERSION 
          // *(uint16_t*)(cmd + 3)  = (uint16_t)BOARD_FWID;          // FW VERSION
          *(uint32_t*)(cmd + 5)  = (uint32_t)BOARD_SERIAL;        // SERIAL Number
          #else 
          *(uint32_t*)(cmd + 1)  = PROFFIE_HDID.xGetHwId();          // HW VERSION 
          *(uint32_t*)(cmd + 5)  = PROFFIE_HDID.xGetSerial();
          #endif
          *(uint16_t*)(cmd + 9)  = (uint16_t)0;                   // Error Counts TODO replace with actually counting error
          *(uint16_t*)(cmd + 11) = (uint16_t)SERIAL_PROTOCOL_LEN; // MAX PROTOCOL LEN RX
          *(uint16_t*)(cmd + 13) = (uint16_t)SERIAL_PROTOCOL_LEN; // MAX PROTOCOL LEN RX 
          *(uint8_t*)(cmd + 15)  = this->_lockState;              // session lock state
          this->_sessionTimeStamp = millis();
          *(uint32_t*)(cmd + 16) = this->_sessionTimeStamp;       // timeStamp of Hello 

          return 20;
        }
        // ---------- Unlock , unlock comunication session so we can operate on board memory cmd , before receiving this all cmd except "Hello" and "Bye" are not executed ----
        case Unlock:
        {
          uint32_t hashRec, hashCalc;
          uint8_t tmpBfr[4], newlockState = 1;
          //hashCalc = (*(uint32_t*)(cmd+1)) + BOARD_SERIAL - (this->_sessionTimeStamp>>8);
          *(uint32_t*)(&tmpBfr) = (*(uint32_t*)(cmd+1)) + BOARD_SERIAL - (this->_sessionTimeStamp>>8);
          hashRec = *(uint32_t*)(cmd+5);
          hashCalc = Serial_Protocol<SA>::CalculateCRC32(&tmpBfr[0], 4, 1);
          // if(hashRec != hashCalc) {
          //   *cmd = trFail;
          //   newlockState = 1;
          // } else 
          {
            *cmd = trOk;
            newlockState = 0;
          }
          
          *(cmd+1) = this->_lockState;
          *(cmd+2) = newlockState;
          this->_lockState = newlockState;

          return 3;
        }
        // ---------- Bye . ends communication seesion and resume board. To resume session , host will have to issue "Hello" and "Unlock" cmds -------
        case Bye:
        {
          *cmd = trOk;
          this->_lockState = 1;
          Serial_Protocol<SA>::SetSession(false);

          return 1;
        }
        // ================================================ DEVICE CONTROL CMDS ===============================================
        // ----------------------------------DeviceReset ,perform a devide reset --------------------------------------------
        case DeviceReset:
        {
          if(this->_lockState) *cmd = trLocked;
          else *cmd = trOk;                       // TODO to be implemented 
          return 1;
        }
        case DeviceErrors:
        {
          if(this->_lockState) *cmd = trLocked;
          else *cmd = trOk;                        // TODO report errors  
          return 1;
        }
        // ==============================================  MEMORY ACCESS CMDS =======================================================
        // ------------ FS_Format,  performing a filesystem format meaning just the fat ---------------------------------------------
        case FS_Format:
        {
          if(this->_lockState) {    // Session Locked 
            *cmd = trLocked;
            *(cmd+1) = 0;
          } else {                  // Session is unlocked , perform format 
            *cmd = trOk;
            #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
            res = f_format(0);        // perform format
            #else 
            // res = DOSFS.format();    // TODO: uncomment when arduino board package is running
            #endif
            *(cmd+ 1)= (uint8_t)res; 
            }
            byteReturn = 2;
            return byteReturn;
        }
        // ----------- LSFS_OpenWrite , open for write a file receiving a path (if file and path dont exist create them) ----------
        case LSFS_OpenWrite:
        {    
          char tmpPath[F_MAXPATH];
          if(this->_lockState) {
            *cmd = trLocked;
          } else if(_file) {
            *cmd = trAlreadyOpen;
          } else {
            strcpy(tmpPath, (char*)(cmd+1));
            _file = LSFS::OpenForWrite(tmpPath);
            if(_file) *cmd = trOk;
            else *cmd = trFail;
          } 
          return 1;
        }
        // --------- LSFS_OpenRead, open for reading a file from a receiving path -------------------------------------------------
        case LSFS_OpenRead:
        {
          char tmpPath[F_MAXPATH];
          if(this->_lockState) {
            *cmd = trLocked;
            *(uint32_t*)(cmd+1) = 0;
           
          } else if(_file) {
            *cmd = trAlreadyOpen;
            *(uint32_t*)(cmd+1) = 0;
          } else { 
            strcpy(tmpPath, (char*)(cmd+1));
            _file = LSFS::Open(tmpPath);
            if(_file) {
              *cmd = trOk;
              *(uint32_t*)(cmd+1) = _file.size();
            } else {      // could not open file 
              *cmd = trFail;
              *(uint32_t*)(cmd+1) = 0;
            }
          }
          return 5;
        }
        // ----- File_Read ----------------------------------------------------------------------
        case File_Read:
        { 
          if(this->_lockState) {
            *cmd = trLocked;
            *(uint16_t*)(cmd+1) = 0;
             byteReturn = 3; 
          } else if (!_file) {
            *cmd = trOpenFirst;
            *(uint16_t*)(cmd+1) = 0;
             byteReturn = 3; 
          } else {  
            uint32_t bytesAvailable, inFileOffset;
            uint16_t nrOfBytesReq;

            nrOfBytesReq = *(uint16_t*)(cmd+1);
            inFileOffset = *(uint32_t*)(cmd+3);
            
            if(inFileOffset > _file.size())
            {
              *cmd = trFail;
              *(uint16_t*)(cmd+1) = 0;
              *(uint32_t*)(cmd+3) = 0;
              byteReturn = 7;
              return byteReturn;
            }

            if(nrOfBytesReq > SERIAL_PROTOCOL_FILE_MAX_LEN) nrOfBytesReq = SERIAL_PROTOCOL_FILE_MAX_LEN; 
            if(inFileOffset + nrOfBytesReq > _file.size()) nrOfBytesReq = _file.size() - inFileOffset;
            
            if(_file.position() != inFileOffset)
              _file.seek(inFileOffset);

            bytesAvailable =  _file.available();
            if(bytesAvailable >= nrOfBytesReq && bytesAvailable > 0) {
              res = _file.read((cmd+7), nrOfBytesReq);
              *cmd = trOk;
              *(uint16_t*)(cmd+1) = res;
              *(uint32_t*)(cmd+3) = _file.position();
              byteReturn = nrOfBytesReq + 7; 

            } else if( bytesAvailable > 0 && bytesAvailable < nrOfBytesReq) {
              res = _file.read((cmd+7), bytesAvailable);
              *cmd = trOk;
              *(uint16_t*)(cmd+1) = res;
              *(uint32_t*)(cmd+3) = _file.position();
              byteReturn = bytesAvailable + 7; 
            } else {
              *cmd = trFail;
              *(uint16_t*)(cmd+1) = bytesAvailable;
              *(uint32_t*)(cmd+3) = _file.position();
              byteReturn = 7; 
            }
          }

          return byteReturn;
        }
        // ----- File_Write with offset parameter ----------------------------------------------------------------------
        case File_Write:
        {
          if(this->_lockState){
            *cmd = trLocked;
            *(uint16_t*)(cmd+1) = 0;
          } else if(!_file) {
            *cmd = trOpenFirst;
            *(uint16_t*)(cmd+1) = 0;
          } 
          else {     
            uint16_t byteWritten;
            uint32_t reqOffset, currOffset;

            reqOffset = *(uint32_t*)(cmd+1);    
            if(reqOffset != 0xFFFFFFFF) {
              currOffset = _file.position();
              if(currOffset != reqOffset)
                _file.seek(reqOffset);
            }

            res = _file.write((cmd+7), *(uint16_t*)(cmd+5));
            currOffset = _file.position();

            *cmd = trOk;
            *(uint32_t*)(cmd+1) = currOffset;
            *(uint16_t*)(cmd+5) = res;

          }
          return 7;
        }

        // // ----- File_Write old ----------------------------------------------------------------------
        // case File_Write_old:
        // {
        //   if(this->_lockState){
        //     *cmd = trLocked;
        //     *(uint16_t*)(cmd+1) = 0;
        //   } else if(!_file) {
        //     *cmd = trOpenFirst;
        //     *(uint16_t*)(cmd+1) = 0;
        //   } 
        //   else {     
        //     uint16_t byteWritten;
        //     res = _file.write((cmd+3), *(uint16_t*)(cmd+1));
        //     *cmd = trOk;
        //     *(uint16_t*)(cmd+1) = res;

        //   }
        //   return 3;
        // }

        // ----- File Close ----------------------------------------------------------------------
        case File_Close:
        {
          if(this->_lockState) {
            *cmd = trLocked;
          } else if(!_file) {
            *cmd = trNothingtoClose;
          } else {
            _file.close();
            *cmd = trOk;
          }

          return 1;
        }
        // ------ FS_INfo , file system info ------------------------------------------------------
        case FS_Info:
        {
          #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
          FSInfo localInfo;
          #else 

          #endif
          if(this->_lockState) {
            *cmd = trLocked;
            for(uint16_t i=1; i < 51; i++)
              *(cmd+1) = 0;
          }else {
            *(cmd + 1) = LSFS::IsMounted(); // LSFS is MOunted 
            if(!*(cmd + 1))
            {
              if(LSFS::CanMount()) *(cmd + 2) = LSFS::Begin();
              else *(cmd + 2) = false;
            } else {
              *(cmd + 2) = true;  // was initialy mounte and is mounted now 
            }
            #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
            if(DOSFS.info(localInfo)) *cmd = trOk;
            else *cmd = trFail;
            
            *(uint32_t*)(cmd+3) = (uint32_t)localInfo.totalBytes;
            *(uint32_t*)(cmd+7) = (uint32_t)(localInfo.totalBytes >> 32);
            *(uint32_t*)(cmd+11) = (uint32_t)localInfo.usedBytes;
            *(uint32_t*)(cmd+15) = (uint32_t)(localInfo.usedBytes >> 32);
            *(uint32_t*)(cmd+19) = (uint32_t)localInfo.blockSize;
            *(uint32_t*)(cmd+23) = (uint32_t)(localInfo.blockSize >> 32);
            *(uint32_t*)(cmd+27) = (uint32_t)localInfo.pageSize;
            *(uint32_t*)(cmd+31) = (uint32_t)(localInfo.pageSize >> 32);
            *(uint32_t*)(cmd+35) = (uint32_t)localInfo.maxOpenFiles;
            *(uint32_t*)(cmd+39) = (uint32_t)(localInfo.maxOpenFiles >> 32);
            *(uint32_t*)(cmd+43) = (uint32_t)localInfo.maxPathLength;
            *(uint32_t*)(cmd+47) = (uint32_t)(localInfo.maxPathLength >> 32);
            #else
        
            *(uint32_t*)(cmd+3) = (uint32_t)DOSFS.totalBytes();
            *(uint32_t*)(cmd+7) = (uint32_t)(DOSFS.totalBytes() >> 32);
            *(uint32_t*)(cmd+11) = (uint32_t)DOSFS.usedBytes();
            *(uint32_t*)(cmd+15) = (uint32_t)(DOSFS.usedBytes() >> 32);
            *(uint32_t*)(cmd+19) = (uint32_t)512;
            *(uint32_t*)(cmd+23) = 0;
            *(uint32_t*)(cmd+27) = (uint32_t)512;
            *(uint32_t*)(cmd+31) = 0;
            *(uint32_t*)(cmd+35) = (uint32_t)16;
            *(uint32_t*)(cmd+39) = 0;
            *(uint32_t*)(cmd+43) = (uint32_t)128;
            *(uint32_t*)(cmd+47) = 0;
            #endif
          }          

            return 51;
        }
        // ------ FS_GetDirFile , file system info ------------------------------------------------------
        case FS_GetDirFile:
        {
          char tmpPath[F_MAXPATH];
          char *e = tmpPath;
          uint8_t nrEntries = 0, reqPageNr = 0;
          uint16_t lenStr = 0;
          uint16_t localoffSet = 0;
          uint8_t curPageNr = 0, maxPageNr = 0;
          
          if(this->_lockState) {
            *cmd = trLocked;
            *(cmd+1) = 0;
            localoffSet = 2;

          } else {
            reqPageNr = *(cmd + 1);
            strcpy(tmpPath, (char*)(cmd+2));
            *cmd = trOk;
            localoffSet = 4;    // start local offset

            if(tmpPath[0] == 0x2f && tmpPath[1] == 0)  // to display root directory content
              e = nullptr;
            if (!e || LSFS::Exists(e)) {
              e = tmpPath;
              LSFS::Iterator entry(e);
              // e = tmpPath;
              if((entry.attr() & 0x10) || (*e==0x2f && *(e+1)== 0)) {     // check if path is dir 
                maxPageNr = 1;
                for (LSFS::Iterator dir(e ? e : ""); dir; ++dir) {
                  nrEntries++;
                  lenStr = strlen(dir.name());
                  if( (localoffSet + (lenStr + 7)) > SERIAL_PROTOCOL_DIR_MAX_LEN ) {
                    localoffSet = 4;
                    maxPageNr++;
                  }
                  localoffSet += lenStr + 7;
                }
                if(!reqPageNr || reqPageNr > maxPageNr)
                  reqPageNr = 1;
                curPageNr = 1;
                localoffSet = 4;    // start local offset
                nrEntries = 0;
                e = tmpPath; 
                for (LSFS::Iterator dir(e ? e : ""); dir; ++dir) {
      
                  lenStr = strlen(dir.name());
                  if( (localoffSet + (lenStr + 7)) <= SERIAL_PROTOCOL_DIR_MAX_LEN ) {
                    nrEntries++;
                  } else if(reqPageNr != curPageNr) {
                    curPageNr++;
                    localoffSet = 4;    // start local offset
                    nrEntries = 0; 
                  } else {
                    break;
                  }
                    strcpy((char*)(cmd+localoffSet), dir.name());
                    localoffSet += lenStr;
                    *(cmd+localoffSet) = 0; // add null terminator 
                    localoffSet += 1;
                    *(cmd+localoffSet) = dir.attr();
                    localoffSet += 1;
                    *(uint32_t*)(cmd+localoffSet) = (uint32_t)dir.size();
                    localoffSet += 4;
                }
              } else {
                *cmd = trPathAttIllegal;
              }
            } else {
                *cmd = trPathNotFound;
            }
            *(cmd+1) = curPageNr;
            *(cmd+2) = maxPageNr;
            *(cmd+3) = nrEntries;
          }
  
          return localoffSet;

        } 

        case LSFS_Rename:
        {
          if(this->_lockState) {
            *cmd = trLocked;
            return 1;

          } else {
            const char* pathFrom;
            const char* pathTo;
            int16_t pfi= -1, pft = -1;

            *cmd = trPathNotFound;      // mark as error, only if we succed renaming put okay
            pfi = scanForNULL((uint8_t*)(cmd+1));
            if(pfi > 0) {
              pathFrom = (const char*)(cmd+1);// its the begining 
              pft = scanForNULL((uint8_t*)(cmd + pfi + 2));
              if(pft > 0) { // we get valid index so now we can begin renaming
                pathTo = (const char*)(cmd + pfi + 2);
                if(strcmp(pathFrom, pathTo))
                {
                  if(LSFS::Exists(pathFrom))  // check if path exists
                  {
                    if(LSFS::RenamePath(pathFrom, pathTo))
                      *cmd = trOk;
                    else
                      *cmd = trFail;
                  }
                } else {  // nothing to rename , report OKAY 
                  *cmd = trOk;
                }
              }
            }
            
          }
          return 1;
        }

        // // ------ FS_GetDirFile , file system info ------------------------------------------------------
        // case FS_GetDirFile_old:
        // {
        //   char tmpPath[F_MAXPATH];
        //   char *e = tmpPath;
        //   uint8_t nrDir = 0;
        //   uint16_t lenStr = 0;
        //   uint16_t localoffSet = 0;
          
        //   if(this->_lockState) {
        //     *cmd = trLocked;
        //     *(cmd+1) = 0;
        //     localoffSet = 2;

        //   } else {
        //     strcpy(tmpPath, (char*)(cmd+1));
        //     *cmd = trOk;
        //     localoffSet = 2;

        //     if(tmpPath[0] == 0x2f && tmpPath[1] == 0)  // to display root directory content
        //       e = nullptr;
        //     if (!e || LSFS::Exists(e)) {
        //       LSFS::Iterator entry(tmpPath);
        //       if(entry.attr() & 0x10) {     // check if path is dir 
        //         for (LSFS::Iterator dir(e ? e : ""); dir; ++dir) {
        //           nrDir++;
        //           lenStr = strlen(dir.name());
        //           // *(uint16_t*)(cmd+offset) = lenStr;
        //           strcpy((char*)(cmd+localoffSet), dir.name());
        //           localoffSet += lenStr;
        //           *(cmd+localoffSet) = 0; // add null terminator 
        //           localoffSet += 1;
        //           *(cmd+localoffSet) = dir.attr();
        //           localoffSet += 1;
        //           *(uint32_t*)(cmd+localoffSet) = (uint32_t)dir.size();
        //           localoffSet += 4;
        //         }
        //       } else {
        //         *cmd = trPathAttIllegal;
        //       }
        //     } else {
        //       *cmd = trPathNotFound;
        //     }
        //     *(cmd+1) = nrDir;
        //   }
  
        //   return localoffSet;

        // }

        // ------ File_CRC , file system info ------------------------------------------------------
        case File_CRC:
        {
          char tmpPath[F_MAXPATH];
          if(this->_lockState) {
            *cmd = trLocked;
            *(uint32_t*)(cmd+1) = 0;
           
          } else if(_file) {
            *cmd = trAlreadyOpen;
            *(uint32_t*)(cmd+1) = 0;
          } else { 
            strcpy(tmpPath, (char*)(cmd+1));
            _file = LSFS::Open(tmpPath);
            if(_file) {
              uint32_t bytesAvailable = 0, calcCrc = 0;
              uint16_t byteRead = 0;
              uint8_t  rst = 1;
              bytesAvailable =  _file.available();
              while(bytesAvailable)
              {
              if(bytesAvailable >= SERIAL_PROTOCOL_FILE_MAX_LEN && bytesAvailable > 0) {
                byteRead = _file.read(cmd, SERIAL_PROTOCOL_FILE_MAX_LEN);
                calcCrc =  Serial_Protocol<SA>::CalculateCRC32(cmd, byteRead, rst);
                rst = 0;
              }
              else if( bytesAvailable > 0 && bytesAvailable < SERIAL_PROTOCOL_FILE_MAX_LEN) {
                byteRead = _file.read((cmd), bytesAvailable);
                calcCrc =  Serial_Protocol<SA>::CalculateCRC32(cmd, byteRead, rst);
                rst = 0;
              }
              bytesAvailable =  _file.available();
              }
              _file.close();
              *cmd = trOk;
              *(uint32_t*)(cmd+1) = calcCrc;
            } else {      // could not open file 
              *cmd = trFail;
              *(uint32_t*)(cmd+1) = 0;
            }
          }
          return 5;
        }
         // ------ Remove all files from a folder ------------------------------------------------------
        case LSFS_RemoveFiles:
        {
          if(this->_lockState) {
            *cmd = trLocked;
            *(uint16_t*)(cmd+1) = 0;
            *(uint8_t*)(cmd+3) = 0;
          } else if(_file) {
            *cmd = trAlreadyOpen;
            *(uint16_t*)(cmd+1) = 0;
            *(uint8_t*)(cmd+3) = 0;
          } else {
            char tmpPath[F_MAXPATH];
            strcpy(tmpPath, (char*)(cmd+1));

            *cmd = trOk;
            *(uint16_t*)(cmd+1) = 0;
            *(uint8_t*)(cmd+3) = 0;
            if(LSFS::Exists(tmpPath))
            {
              LSFS::Iterator entry(tmpPath);
              if(entry.attr() & 0x10) { // check if path is dir 
                char* e = tmpPath;
                uint16_t nrOfFilesDeleted = 0;
                uint8_t removeResult = 1;
                for (LSFS::Iterator dir(e ? e : ""); dir; ++dir) {
                  if(dir.attr() & 0x20) {  // check if path is a file attribute 
                  bool localResult; 
                  PathHelper fileToRemovePath(tmpPath, dir.name());
                  localResult = LSFS::Remove((const char*)fileToRemovePath);
                  if(!localResult) {
                    removeResult  = 0;
                  } else {
                    nrOfFilesDeleted++;
                  }
                  }
                }
                *(uint16_t*)(cmd+1) = nrOfFilesDeleted;
                *(uint8_t*)(cmd+3) = removeResult;
              } else {
                *cmd = trPathAttIllegal;
              }

            } else {
               *cmd = trPathNotFound;
            }  
          }  
          return 4;
        }
        // ------ Remove from path  ------------------------------------------------------
        case LSFS_Remove:
        {
          if(this->_lockState) {
            *cmd = trLocked;
            *(uint16_t*)(cmd+1) = 0;
          } else if(_file) {
            *cmd = trAlreadyOpen;
            *(uint16_t*)(cmd+1) = 0;
          } else {
            char tmpPath[F_MAXPATH];
            strcpy(tmpPath, (char*)(cmd+1));

            *cmd = trOk;
            *(uint8_t*)(cmd+1) = 0;
            *(uint8_t*)(cmd+2) = 0;
            if(LSFS::Exists(tmpPath))
            {
              LSFS::Iterator entry(tmpPath);
              if(entry.attr() & 0x20){
                  *(uint8_t*)(cmd+1) = LSFS::Remove(tmpPath);
                  *(uint8_t*)(cmd+2) = entry.attr();
              } 
              else if(entry.attr() & 0x10) {
                *(uint8_t*)(cmd+1) = DOSFS.rmdir(tmpPath);
                *(uint8_t*)(cmd+2) = entry.attr();
              } else {
                *cmd = trPathAttIllegal;
              }

            } else {
               *cmd = trPathNotFound;
            }  
          }  
          return 3;
        }

        default:
            *cmd = trUnknown;
            byteReturn = 1;
            return 1;
      }

      return byteReturn; // SHOULD NOT GET HERE !! 

   }

    File _file;
    uint8_t _lockState;
    uint32_t _sessionTimeStamp;
};

template<class SA> bool Serial_Protocol<SA>::_sessionState = false;
template<class SA> Print *Serial_Protocol<SA>::savedDefault_output = NULL;
template<class SA> Print *Serial_Protocol<SA>::savedStdout_output = NULL;
// template<class SA> File Serial_Transfer<SA>::_file = NULL; 
StaticWrapper<Serial_Transfer<SerialAdapter>> mtpUart;

#endif  // end mtp

// Command-line parser. Easiest way to use it is to start the arduino
// serial monitor.
template<class SA> /* SA = Serial Adapter */
#ifdef ULTRAPROFFIE
class Parser : Looper, StateMachine, PowerSubscriber {
public:
  Parser() : Looper(), PowerSubscriber(pwr4_CPU) {}
#else 
class Parser : Looper, StateMachine {
public:
  Parser() : Looper() {}
#endif 

  const char* name() override { return "Parser"; }

  void Setup() override {
    SA::begin();
  }

  void Loop() override {

#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
    uint32_t CPUtimeout = 300000;   // Exceptional power timeout for serial parser 
#endif

    STATE_MACHINE_BEGIN();
    while (true) {
      while (!SA::Connected()) YIELD();
      if (!SA::AlwaysConnected()) {
        // STDOUT << "Welcome to ProffieOS " << version << ". Type 'help' for more info.\n";        
      }

      while (SA::Connected()) {
        while (!SA::stream().available()) YIELD();
#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
      RequestPower(&CPUtimeout);   // Increase CPU timeout to 5 minutes if parser is active (defaults to 1 minute)
#endif

#ifdef OSX_ENABLE_MTP
        if(Serial_Protocol<SerialAdapter>::GetSession())
        {   
            //mtpUart->ConsumeRx(MTP_UART_TIMEOUT_SHORT);
            YIELD();
            // mtpUart->RefreshProtocolFrame();
            // while(Serial_Protocol<SerialAdapter>::GetSession())
            //   mtpUart->Loop();
        }
        else
        {
#endif 

        int c = SA::stream().read();
        if (c < 0) { break; }
        if (c == '\n' || c == '\r') {
          if (cmd_) ParseLine();
          len_ = 0;
          space_ = 0;
          free(cmd_);
          cmd_ = nullptr;
          continue;
        }
        if (len_ + 1 >= space_) {
          int new_space = space_ * 3 / 2 + 8;
          char* tmp = (char*)realloc(cmd_, new_space);
          if (tmp) {
            space_ = new_space;
            cmd_ = tmp;
          } else {
            STDOUT.println("Line too long.");
            len_ = 0;
            space_ = 0;
            free(cmd_);
            cmd_ = nullptr;
            continue;
          }
        }
        cmd_[len_] = c;
        cmd_[len_ + 1] = 0;
        len_++;

#ifdef OSX_ENABLE_MTP
      }
#endif

      }
      len_ = 0;
      space_ = 0;
      free(cmd_);
      cmd_ = nullptr;
    }
    STATE_MACHINE_END();
  }

  void ParseLine() {
    if (len_ == 0) return;
    while (len_ > 0 && (cmd_[len_-1] == '\r' || cmd_[len_-1] == ' ')) {
      len_--;
      cmd_[len_] = 0;
    }
    if (cmd_[0] == '#') {
      STDOUT.println(cmd_);
      return;
    }
    stdout_output = &SA::stream();
    STDOUT.print(SA::response_header());
    char *cmd = cmd_;
    while (*cmd == ' ') cmd++;
    char *e = cmd;
    while (*e != ' ' && *e) e++;
    if (*e) {
      *e = 0;
      e++;  // e is now argument (if any)
    } else {
      e = nullptr;
    }

    if (!CommandParser::DoParse(cmd, e)) {
      STDOUT.print("Whut? :");
      STDOUT.println(cmd);
    }
    STDOUT.print(SA::response_footer());
    stdout_output = default_output;
  }

private:
  int len_ = 0;
  char* cmd_ = nullptr;
  int space_ = 0;
};

StaticWrapper<Parser<SerialAdapter>> parser;

#ifdef ENABLE_SERIAL
StaticWrapper<Parser<Serial3Adapter>> serial_parser;
#define ENABLE_SERIAL_COMMANDS
#endif

#ifdef USB_CLASS_WEBUSB
StaticWrapper<Parser<WebUSBSerialAdapter>> webusb_parser;
#endif

#ifdef ENABLE_SERIAL_COMMANDS
class SerialCommands : public CommandParser {
 public:
  void HM1XCmd(const char* cmd) {
    STDOUT.print("Sending: ");
    STDOUT.println(cmd);
    STDOUT.print("Reply: ");
    Serial3.print(cmd);
    uint32_t last_char = millis();
    uint32_t timeout = 300;
    while (millis() - last_char < timeout) {
      if (Serial3.available()) {
        last_char = millis();
        timeout = 100;
        STDOUT.write(Serial3.read());
      }
    }
    STDOUT.println("");
  }
  bool Parse(const char* cmd, const char* e) override {
#if 0
    if (!strcmp(cmd, "hm1Xpin")) {
      // Doesn't work, pine is 4 chars, pinb is 6
      HM1XCmd("AT+AUTH1");
      HM1XCmd("AT+DUAL1");
      Serial3.write("AT+PINE");
      Serial3.println(e);
      Serial3.write("AT+PINB");
      Serial3.println(e);
      return true;
    }
    if (!strcmp(cmd, "hm1Xname")) {
      Serial3.write("AT+NAME");
      Serial3.println(e);
      Serial3.write("AT+NAMB");
      Serial3.println(e);
      return true;
    }
#endif
    if (cmd[0] == 'A' && cmd[1] == 'T') {
      HM1XCmd(cmd);
      return true;
    }
    if (!strcmp(cmd, "send") && e) {
      Serial3.println(e);
      STDOUT.print("Wrote: ");
      STDOUT.println(e);
      return true;
    }
#ifdef BLE_PASSWORD
    if (!strcmp(cmd, "get_ble_config")) {
      PrintQuotedValue("password", BLE_PASSWORD);
#ifndef BLE_NAME
#define BLE_NAME "ProffieOS"
#endif
      PrintQuotedValue("name", BLE_NAME);
#ifdef BLE_SHORTNAME
      PrintQuotedValue("shortname", BLE_SHORTNAME);
#else
      if (sizeof(BLE_NAME) - sizeof("") <= 8) {
        PrintQuotedValue("shortname", BLE_NAME);
      } else {
        PrintQuotedValue("shortname", "Saber");
      }
#endif
      return true;
    }
#endif
    return false;
  }
  void Help() override {
    #if defined(COMMANDS_HELP) 
    // STDOUT.println(" hm13pin PIN - configure HM13 PIN");
    // STDOUT.println(" hm13name NAME - configure HM13 NAME");
    STDOUT.println(" get_ble_config - show BLE PIN");
    if (default_output != stdout_output)
      STDOUT.println(" make_default_console - make this connection the default connection");
    #endif
  }
};

StaticWrapper<SerialCommands> serial_commands;

#endif



#endif  // COMMON_SERIAL_H
