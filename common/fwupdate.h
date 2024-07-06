#ifndef FWUPD_H
#define FWUPD_H

    #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
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
    #endif // end of ULTRAPROFFIE
#endif