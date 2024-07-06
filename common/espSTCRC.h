#ifndef ESPSTCRC_H
#define ESPSTCRC_H
// Reproduction of ST CRC implementation 

#include <stdint.h>

#define DEFAULT_CRC_INITVALUE 0xFFFFFFFF
#define DEFAULT_CRC_POLY      0x04C11DB7

// #define CRC_TEST

#ifdef CRC_TEST
class STCrc: public CommandParser
#else
class STCrc
#endif
{
    private:
    enum iformat
    {
        crc_bytes = 1,
        crc_halfWords,
        crc_words
    };

    uint32_t currentCRC;
    iformat format;

    public:

    /*  @brief  : Default members 
    *   @param  :
    *   @retval :
    */
    STCrc()
    {
        currentCRC = DEFAULT_CRC_INITVALUE;
        format = crc_bytes;
    }

    /*  @brief  :   
    *   @param  :
    *   @retval :
    */
    uint32_t CRC_Calculate(void* buffer, uint32_t nrBytes)
    {
        CRC_Reset();
        switch (format)
        {
        case crc_bytes:
            currentCRC = CRC_Handle_8((uint8_t*)buffer, nrBytes);
            break;
        
        default:
            break;
        }

        return currentCRC;
    }

    /*  @brief  :   
    *   @param  :
    *   @retval :
    */
    uint32_t CRC_Acumulate(void* buffer, uint32_t nrBytes)
    {
        switch (format)
        {
        case crc_bytes:
            currentCRC = CRC_Handle_8((uint8_t*)buffer, nrBytes);
            break;
        
        default:
            break;
        }

        return currentCRC;
    }

    /*  @brief  :   
    *   @param  :
    *   @retval :
    */
    void CRC_Reset()
    {
         currentCRC = DEFAULT_CRC_INITVALUE;
    }

    /*  @brief  :   
    *   @param  :
    *   @retval :
    */
   uint32_t CRC_Calc32(uint32_t calcCRC, uint32_t data32)
   {    
        calcCRC ^= data32;
        for(uint8_t i = 0; i < 32; i++) {
            if( (calcCRC & 0x80000000) >> 31) {                 // check if MSB is 1 
                calcCRC = (calcCRC << 1) ^ DEFAULT_CRC_POLY;    // if 1 in MSB, shift to left with 1 bit and XOR with polynomial
            } else {
                calcCRC = calcCRC << 1;                         // if 0 in MSB only shift to left with 1 bit
            }
        }
        return calcCRC;
   }
    
    /*  @brief  :   
    *   @param  :
    *   @retval :
    */
    uint32_t CRC_Handle_8(uint8_t* databfr, uint32_t nrOfBytes)
    {   
        if(!databfr) return 0;              // check we have a valid pointer 
        if(!nrOfBytes) return 0;            // check we have valid nr of bytes 

        uint32_t calcCrc = currentCRC;      // start with current crc 
        uint32_t goUntil = (nrOfBytes >> 2) << 2;   // fist go multiple of 4
        uint32_t tempIndex = 0;
        uint32_t data32;

        for(uint32_t i = 0; i < goUntil; i+=4) {    
            data32 = (databfr[i] << 24) | (databfr[i + 1] << 16) | (databfr[i + 2] << 8) | (databfr[i + 3]);    // form data input to u32
            calcCrc = CRC_Calc32(calcCrc, data32);  // calculate crc32
        }

        switch(nrOfBytes % 4)
        {
            case 1:
                data32 = databfr[goUntil];
                calcCrc = CRC_Calc32(calcCrc, data32);
            break;

            case 2:
                data32 = (databfr[goUntil] << 8) | (databfr[goUntil+1]);
                calcCrc = CRC_Calc32(calcCrc, data32);
            break;

            case 3:
                data32 = (databfr[goUntil] << 16) | (databfr[goUntil+1] << 8) | (databfr[goUntil+2] );
                calcCrc = CRC_Calc32(calcCrc, data32);
            break; 
        }
        return calcCrc;

    }

#ifdef CRC_TEST
    bool Parse(const char *cmd, const char* arg) override {
        uint8_t dataBfr9[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        uint8_t dataBfr10[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        uint8_t dataBfr11[11] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
        uint8_t dataBfr12[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        uint8_t dataBfr34[34] = { 112, 97, 116, 104, 32, 76, 101, 110, 32, 53, 13, 10, 112, 97, 116, 104, 32, 76, 101, 110, 32, 53, 13, 10, 171, 0, 4, 0, 1, 0, 0,
    0, 0, 1};    //   crc 141, 215, 126, 98

        if (!strcmp(cmd, "crcTest1")) {
            STDOUT.print("CRC 9 : "); STDOUT.println(CRC_Calculate(dataBfr9, 9));
            STDOUT.print("CRC 10 : "); STDOUT.println(CRC_Calculate(dataBfr10, 10));
            STDOUT.print("CRC 11 : "); STDOUT.println(CRC_Calculate(dataBfr11, 11));
            STDOUT.print("CRC 12 : "); STDOUT.println(CRC_Calculate(dataBfr12, 12));
            STDOUT.print("CRC 38 : "); STDOUT.println(CRC_Calculate(dataBfr34, 34));

            return true;
        }

        return false;
    }

    void Help() override {

        STDOUT.println("crcTest - test implementation of CRC "); 
    
    }

#endif


};

STCrc STCrc_impl;
#endif
