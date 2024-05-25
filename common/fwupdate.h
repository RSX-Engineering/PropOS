#ifndef FWUPD_H
#define FWUPD_H

    #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
        #if HWL_CONCAT(MQUOATE, HW_PREFIX, MQUOATE) == 'L'
        #define UPDATE_FILE "_osx_/osx.cod"

        #include "stm32l4_fwupg.h"
        #include "CodReader.h"     // make sure that code reader is included 

        bool __attribute__((optimize("O0"))) CheckFwUpdate()
        {
            CodReader reader;
            struct {
                uint16_t version;       // 
                uint8_t force;          // 
                uint8_t cnt;            //
                uint16_t binID;         //
            } __attribute__((packed)) fwdata;   // install configuration

            if (!reader.Open(UPDATE_FILE, 1))
                return false;          // file not found
            
            
            if (reader.FindEntry(1) == COD_ENTYPE_STRUCT) 
            {
            // found a struct with the right ID
                if (reader.codProperties.structure.Handler == HANDLER_xUpdate)
                {
                     uint32_t numBytes = reader.ReadEntry(1, (void*)&fwdata, sizeof(fwdata));
                     if (numBytes != sizeof(fwdata)) 
                     {
                        reader.Close();
                        return false;
                     }

                     int convVersion = atoi(OSX_SUBVERSION);
                     if(fwdata.force || fwdata.version > convVersion)
                     {
                        if(fwdata.force) fwdata.force = 0;
                        fwdata.cnt += 1;

                        if(reader.FindEntry(fwdata.binID) == COD_ENTYPE_BIN)
                        {
                            uint32_t savedOffset, crcCalc;
                            uint32_t dataSize;
                            dataSize = reader.codProperties.bin.Size;
                            savedOffset = reader.codInterpreter->currentCodOffset;
                            // dataSize = 231062;
                            crcCalc = reader.codInterpreter->BinCrc(savedOffset, dataSize);
                            if(crcCalc != reader.codProperties.bin.CRC32)
                            {   
                                reader.Close();
                                return false;
                            }
                            reader.OverwriteEntry(1, (void*)&fwdata, sizeof(fwdata));
                            reader.Close();
                            // TODO STOP ALL INTERRUPTS !!!!! expect SYSTICK IQR
                            stm32l4_ll_fwpgr(UPDATE_FILE, savedOffset, dataSize);
                            return true;
                        }
                    }
                }
            }
            reader.Close();
            return false;
        }

        #endif // end of Ultra proffie LITE 
    #elif defined(SABERPROP) && defined(ARDUINO_ARCH_ESP32) // end of SABERPROP

    #include <Update.h>

      #define UPDATE_FILE "_osx_/osx.cod"

    // #if defined(SABERPROP) && defined(ARDUINO_ARCH_ESP32) // STM Saberprop and UltraProffies
    class BinUpdater: public CommandParser {
        public:
        /* @brief   : public constructor 
        *  @param   :
        *  @retval  :  
        */
        BinUpdater(void) : CommandParser() 
        {

        }

        /* @brief   : perform the actual update from a given stream 
        *  @param   :
        *  @retval  :  
        */
        void performUpdate(Stream &updateSource, size_t updateSize) 
        {
            if (Update.begin(updateSize)) {      
                size_t written = Update.writeStream(updateSource);
                if (written == updateSize) {
                    STDOUT.println("Written : " + String(written) + " successfully");
                }
                else {
                    STDOUT.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
                }
                if (Update.end()) {
                    STDOUT.println("Update done!");
                    if (Update.isFinished()) {
                        STDOUT.println("Update successfully completed. Rebooting.");
                    }
                    else {
                        STDOUT.println("Update not finished? Something went wrong!");
                    }
                    ESP.restart();
                }
                else {
                    STDOUT.println("Error Occurred. Error #: " + String(Update.getError()));
                }

            }
            else
            {
                STDOUT.println("Not enough space to begin OTA");
            }
        }
        bool CheckFwUpdate()
        {
            return CheckFwUpdate(UPDATE_FILE);
        }

        bool __attribute__((optimize("O0"))) CheckFwUpdate(const char* filepath)
        {
            CodReader reader;
            struct {
                uint16_t version;       // 
                uint8_t force;          // 
                uint8_t cnt;            //
                uint16_t binID;         //
            } __attribute__((packed)) fwdata;   // install configuration

            if (!reader.Open(filepath, 1))
            {
            //    STDOUT.print(filepath);STDOUT.println("FilePath Not found !");
               return false;          // file not found
            }
            STDOUT.print("Searching for new firmware in "); STDOUT.println(filepath);
            
            
            if (reader.FindEntry(1) == COD_ENTYPE_STRUCT) 
            {
            // found a struct with the right ID
                if (reader.codProperties.structure.Handler == HANDLER_xUpdate)
                {
                     uint32_t numBytes = reader.ReadEntry(1, (void*)&fwdata, sizeof(fwdata));
                     if (numBytes != sizeof(fwdata)) 
                     {
                        STDOUT.println("File content mismatch");
                        reader.Close();
                        return false;
                     }

                     int convVersion = atoi(OSX_SUBVERSION);
                     if(fwdata.force || fwdata.version > convVersion)
                     {
                        if(fwdata.force) fwdata.force = 0;
                        fwdata.cnt += 1;

                        if(reader.FindEntry(fwdata.binID) == COD_ENTYPE_BIN)
                        {
                            uint32_t savedOffset, crcCalc;
                            uint32_t dataSize;
                            dataSize = reader.codProperties.bin.Size;
                            savedOffset = reader.codInterpreter->currentCodOffset;
                            // dataSize = 231062;
                            crcCalc = reader.codInterpreter->BinCrc(savedOffset, dataSize);
                            if(crcCalc != reader.codProperties.bin.CRC32)
                            {   
                                STDOUT.println("CRC mismatch");
                                reader.Close();
                                return false;
                            }
                            reader.OverwriteEntry(1, (void*)&fwdata, sizeof(fwdata));
                            reader.Close();
                            
                            updateFromSD(filepath, savedOffset, dataSize);
                            return true;
                        }
                    } else {
                        STDOUT.println("Already on newer or same version");
                    }
                }
            }
            reader.Close();
            return false;
        }

        // check given FS for valid update.bin and perform update if available
        void updateFromSD(const char* fileName, uint32_t offset, uint32_t size) 
        {   
            File updateBin = LSFS::Open(fileName);
            if (updateBin) {
                if(updateBin.isDirectory()){
                    STDOUT.print("Error,");STDOUT.print(fileName);
                    STDOUT.println("is not a file");
                    updateBin.close();
                    return;
                }

                size_t updateSize = updateBin.size();
                if(!updateBin.seek(offset, SeekSet))    // set desired offset 
                {
                     STDOUT.println("could not position in file at desired offset ");
                     return;
                }
                if(updateBin.position() != offset)
                {
                     STDOUT.println("current position is not at desired ");
                     return;
                }

                if (updateSize > 0) {
                    STDOUT.println("Try to start update");
                    performUpdate(updateBin, size);
                }
                else {
                    STDOUT.println("Error, file is empty");
                }

                updateBin.close();
                
                // whe finished remove the binary from sd card to indicate end of the process
                //fs.remove("/update.bin");      
            }
            else {
                STDOUT.println("Could not load update.bin from sd root");
            }
        }

        /* @brief   : public constructor 
        *  @param   :
        *  @retval  :  
        */
        virtual bool Parse(const char* cmd, const char* arg) 
        {
            if (!strcmp(cmd, "performUpdate")) 
            {
                STDOUT.println("Performing update...");
                if(arg) {
                    CheckFwUpdate(arg);
                } else {
                    CheckFwUpdate(UPDATE_FILE);
                }
          
                return true;
            }

            return false;
        }

        virtual void Help() {}
    
    };
    BinUpdater Binupdater;
    #endif
    // #endif

#endif