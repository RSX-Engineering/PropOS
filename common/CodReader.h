#ifndef XCOD_READER_H
#define XCOD_READER_H

/********************************************************************
/** Read and manage COD files                                       *
 *  (C) Cosmin PACA @ RSX Engineering. Licensed under GNU GPL.      *
 *  fileversion: v1.2 @ 2022/08                                     *
 ********************************************************************
 *  -                                 
 *  -                  
 *  -                                         
 ********************************************************************/
#include "lsfs.h"
#include "codids.h"

#define COD_HEADER_LEN       8       // header len 
#define COD_ENTYPE_TABLE     1       // table entry type 
#define COD_ENTYPE_STRUCT    2       // struct entry type
#define COD_ENTYPE_BIN       3       // bin entry type  

// Properties of a table read from a COD file
struct Table {
    uint16_t    ID;                    // ID unique in file , over all entries 
    uint16_t     Handler;              // Tell the useage of table 
    uint16_t    Rows;                  // numbers of rows 
    uint16_t    Columns;               // number of columns 
    uint8_t     DataType;              // data type 1 - uint8_t , 2- int16 , 3 - float 
                                       // rows * columns * datatype = size of table in bytes 
};
// Properties of struct read from a COD file 
struct Struct {
    uint16_t    ID;                    // ID unique in file , over all entries 
    uint16_t     Handler;              // Tell the useage of Handler
    uint16_t    Size;                  // size of struct in bytes 
};

struct Bin {
    uint16_t    ID;                   // ID unique in file , over all entries 
    uint16_t    Handler;              // Tell the useage of Handler
    uint32_t    Size;                 // size of bin in bytes
    uint32_t    CRC32;                // CRC of bin content
};
// Con entry union , can be either a table or a struct 
union CodEntry { 
    Table table;                                            
    Struct structure;
    Bin bin;
};                                  

enum CodDataType
{
    xCod_Unknown = 0,
    // v1 suported 
    xCod_Uint8 = 1,             // to be consistent with V1
    xCod_Int16 = 2,             // to be consistent with V1
    xCod_Float = 3,             // to be consistent with V1
    // v2 suported (plus V1) 
    xCod_Double = 4,
    xCod_Int8 = 5,
    xCod_Int32 = 6,
    xCod_Uint16 = 7,
    xCod_Uint32 = 8
    
};

class CodInterpreter {

protected:
    File *pFile;
    CodEntry* codProperties;        // ID, handler and size
    uint8_t *entryType;             // COD_ENTYPE_TABLE=1, COD_ENTYPE_STRUCT=2
public: 
    uint32_t entryDataSize;         // was private
    uint32_t currentCodOffset;

    virtual bool ReadProp(uint16_t id, uint8_t searchBy = 0);
    virtual bool SearchProp(uint16_t id, uint32_t *dataOffset, uint8_t searchBy);
    virtual uint32_t BinCrc(uint32_t fileOffset, uint32_t size)
    {
        return 0;
    }
    /*  @brief  : 
    *   @param  :
    *   @retval :
    */
    CodInterpreter()
    {
        pFile = NULL;
        codProperties = NULL;
        entryType = NULL;
        entryDataSize = 0;
        currentCodOffset = 0;
    }

    /*  @brief  : 
    *   @param  :
    *   @retval :
    */
    CodInterpreter(File * pFile, CodEntry* codProperties, uint8_t *entryType) 
    {
        this->pFile = pFile;
        this->codProperties = codProperties;
        this->entryType = entryType;
        this->entryDataSize = 0;
        this->currentCodOffset = 0;
    }

    /*  @brief  :   Get CRC of file 
    *   @param  :   pointer to CRC Read
    *   @retval :   0 - CRC failed 
    *               else - returned CRC 
    */
    uint32_t GetFileCrC(uint32_t* crcRead) 
    {
        return FileCrC(crcRead);
    }

    /*  @brief  :   Get CRC of file 
    *   @param  :   pointer to CRC Read
    *   @retval :   0 - update of CRC failed 
    *               else - updated CRC 
    */
    uint32_t UpdateFileCrC() 
    {
        return FileCrC(NULL);
    }
    
    /*  @brief  :  Read data from a specific table 
    *   @param  :  id           - id of table 
                   destBuffer   - destination buffer
                   size         - nr of bytes to read  
    *   @retval :  true  - all ok
    *              false - something went wrong // todo add error codes maybe 
    */
    bool ReadData8(uint16_t id, uint8_t* destBuffer, uint32_t size) //__attribute__((optimize("Og")))
    {
        bool result;                       
        uint16_t bytesRead;
        uint32_t bytesAvalable; // , localEntryDataSize;
        uint32_t mult;
        #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
        if(!pFile || *pFile == NULL)
            return false;
        #else 
        if(!pFile || *pFile == 0)
            return false;
        #endif

        if(id != codProperties->table.ID)                     // check the id and read the properties of id if neccesary 
        {
            result = ReadProp(id);
            if(!result)
                return false;
        }

        if(size > entryDataSize)           // localEntryDataSize
            return false;

        if(pFile->position() != currentCodOffset) // check if position is located at desired offset 
            if(!pFile->seek(currentCodOffset))    // set desired offset 
                return false ;

        bytesAvalable = pFile->available();               // get available bytes 
        if(!bytesAvalable || (bytesAvalable < size) )   // check if we have availabe bytes
            return false;

        bytesRead = pFile->read(destBuffer, size);    // read desired bytes 
        if(bytesRead != size)                       // check if read was completed as needed
            return false;

        return true;
    }

    private: 
    /* @brief   :   Get CRC of file 
    *  @param   : crcRead - pointer to data , if NULL update the CRC  
    *  @retval  : 0 - failed 
    *             else - returned CRC    
    */
    uint32_t FileCrC(uint32_t* crcRead) //__attribute__((optimize("Og")))
    {
        uint32_t bytesAvalable, toRead, crcCalculated;
        bool result;
        uint8_t localStorVar[4];

        #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
        if(!pFile || *pFile == NULL)
            return false;
        #else 
        if(!pFile || *pFile == 0)
            return false;
        #endif

       LOCK_SD(true);

        if(pFile->position() != 0)
            pFile->seek(0);

        bytesAvalable = pFile->available();
        if(!bytesAvalable || (bytesAvalable % 4))
            return 0;
        
        if (bytesAvalable < 4)
            return 0;

        toRead = bytesAvalable - 4;
        if(!toRead) return 0;

        for(uint32_t indx = 0 ; indx < toRead; indx += 4)
        {
            pFile->read((uint8_t*)&localStorVar[0], 4);
            #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
            	 // TODO ADD CRC FOR CODReDER FOR ESP
            if(indx == 0)
                crcCalculated = HAL_CRC_Calculate(&stm32l4_crc, (uint32_t*)&localStorVar[0], 4); // reset crc and calculate      4 
            else
                crcCalculated = HAL_CRC_Accumulate(&stm32l4_crc, (uint32_t*)&localStorVar[0], 4); // accumulate       4 
            #endif
        }
        if(crcRead) {
            pFile->read((uint8_t*)&localStorVar[0], 4);
            *crcRead = (localStorVar[3] << 24) | (localStorVar[2] << 16) | (localStorVar[1] << 8) | localStorVar[0];
        }
        else {
            // we need to update the CRC 
            int32_t writeResult; 
            writeResult = pFile->write((uint8_t*)&crcCalculated, 4);
            if(writeResult != 4) 
                crcCalculated = 0; // update failed 
        }
        pFile->seek(0); // reset file position 
        // result = this->ReadHeader(); TODO check out this

        LOCK_SD(false);

        return crcCalculated;
    }

};

class CodInterpreter_V2 : public CodInterpreter {
public:
    static const uint8_t V2_START_OFFSET     = 9;       // start address
    static const uint8_t V2_TABLE_PROP_SIZE  = 16;      // table properties size
    static const uint8_t V2_STRUCT_PROP_SIZE = 7;      // struct properties size   
    static const uint8_t V2_ENTYPE_TABLE     = 116;     // table entry type ascii for "t"
    static const uint8_t V2_ENTYPE_STRUCT    = 115;     // struct entry type ascii for "s"
    static const uint8_t V2_DATATYPE_SIZE    = 7;

  /*  @brief  :
    *   @param  :
    *   @retval :
    * */
   uint8_t GetEntryType(uint8_t entryType)
   {
        switch(entryType)
        {
            case V2_ENTYPE_TABLE:
                return COD_ENTYPE_TABLE;
            case V2_ENTYPE_STRUCT:
                return COD_ENTYPE_STRUCT;
        }
        return 255;
   }
    /*  @brief  :
    *   @param  :
    *   @retval :
    * */
    int8_t GetSizeOfType(uint8_t dataType) //__attribute__((optimize("Og")))
    {
        switch(dataType)
        {
            case xCod_Uint8:
            case xCod_Int8:
                return 1;
            case xCod_Int16:
            case xCod_Uint16:
                return 2;
            case xCod_Float:
            case xCod_Int32:
            case xCod_Uint32:
                return 4;
            case xCod_Double:
                return 8;  
        }
        return -1;
    }
    /*  @brief  : return size in bytes from string type name  
    *   @param  : 
    *   @retval :
    */
    uint8_t GetTypeFromString(const char* source) //__attribute__((optimize("Og")))
    {
        if(!strcmp(source, "uint8"))
            return xCod_Uint8;
        else if(!strcmp(source, "uint16"))
            return xCod_Uint16;
        else if(!strcmp(source, "uint32"))
            return xCod_Uint32;
        else if(!strcmp(source, "int8"))
            return xCod_Int8;
        else if(!strcmp(source, "int16"))
            return xCod_Int16;
        else if(!strcmp(source, "int32"))
            return xCod_Int32;
        else if(!strcmp(source, "double"))
            return xCod_Double;
        else if(!strcmp(source, "float"))
            return xCod_Float;

        return xCod_Unknown;
    }


public:
    /*  @brief  :
    *   @param  :
    *   @retval :
    * */
   CodInterpreter_V2(File * pFile, CodEntry* codProperties, uint8_t *entryType) : CodInterpreter(pFile, codProperties, entryType)
   {

   }
  
    /*  @brief  : Search table with specific id and read its properties if exists 
    *   @param  : id - id of table 
                  offset - offset location in bytes of data location
                  searchBy - id 
    *   @retval : true - all good 
                  false - fail
    */
    bool SearchProp(uint16_t id, uint32_t *dataOffset, uint8_t searchBy) //__attribute__((optimize("Og")))
    {
        uint32_t bytesAvalable;
        uint16_t tmpID, tmpSizeR, tmpSizeC, tmpHandler;
        uint8_t tmpDataType, tmpEntryType;
        uint32_t localOffset, fileSize, skipBytes;
        uint8_t multiplier = 0;
        uint8_t localNrCrt = 0;
        localOffset = V2_START_OFFSET;                     // start the search from beggining , linear for now 
        fileSize = pFile->size();                             // get file size

        while(localOffset < fileSize)
        {
            if(pFile->position() != localOffset)
                pFile->seek(localOffset);

            bytesAvalable = pFile->available();
            if(bytesAvalable < V2_STRUCT_PROP_SIZE)   // we must have at least structure porp size  
                return false;

            pFile->read((uint8_t*)&tmpEntryType, 1);  // get entry type 
            pFile->read((uint8_t*)&tmpID, 2);         // get ID
            pFile->read((uint8_t*)&tmpHandler, 2);       // get lut type 
            if(tmpEntryType == V2_ENTYPE_STRUCT) {
                pFile->read((uint8_t*)&tmpSizeC, 2);      // get size of structure, in bytes
                if(!tmpSizeC)
                    return false;
                localOffset += V2_STRUCT_PROP_SIZE;
                skipBytes = tmpSizeC;
                localNrCrt++; 
                if( (tmpID == id && !searchBy) || (localNrCrt == id && searchBy) )
                {   // found struct with the right ID
                    *dataOffset = localOffset;      
                    codProperties->structure.ID = tmpID;                
                    codProperties->structure.Size = tmpSizeC;
                    codProperties->structure.Handler = tmpHandler;
                    *entryType = GetEntryType(tmpEntryType);
                    entryDataSize = skipBytes;
                    return true;
                }
            } else if(tmpEntryType == V2_ENTYPE_TABLE) {

                pFile->read((uint8_t*)&tmpSizeC, 2);      // get SizeH (columns)
                pFile->read((uint8_t*)&tmpSizeR, 2);      // get SizeV (rows)
                uint8_t localTypeChar[V2_DATATYPE_SIZE];
                pFile->read((uint8_t*)&localTypeChar, V2_DATATYPE_SIZE);   // get data type
                tmpDataType = GetTypeFromString((char*)&localTypeChar);

                if(!tmpSizeR || !tmpSizeC || !tmpDataType)   // Check validity
                    return false;
                multiplier = GetSizeOfType(tmpDataType);
                localOffset += V2_TABLE_PROP_SIZE;
                skipBytes = (uint32_t)(tmpSizeR * tmpSizeC * multiplier);
                localNrCrt++; 
                if( (tmpID == id && !searchBy) || (localNrCrt == id && searchBy))
                {   // found table with the right ID
                    *dataOffset = localOffset;      
                    codProperties->table.ID = tmpID;                
                    codProperties->table.Rows = tmpSizeR;                
                    codProperties->table.Columns = tmpSizeC;             
                    codProperties->table.DataType = tmpDataType;                
                    codProperties->table.Handler = tmpHandler;
                    *entryType = GetEntryType(tmpEntryType);
                    entryDataSize = skipBytes;
                    return true;
                }
            } else {
                return false;   // entry type not valid , dont know how many byte to skip so declare failure
            }

            localOffset += skipBytes;
        }

        return false;

    }

    /*  @brief  : Read the properties of an entry (table or struct)
    *   @param  : id - id of entry
    *   @retval : true - all good
    *             false - something went wrong 
    */
    bool ReadProp(uint16_t id, uint8_t searchBy = 0) //__attribute__((optimize("Og")))
    {   
        uint32_t bytesAvalable;
        uint16_t res;
        bool searchResult;

        searchResult = SearchProp(id, &currentCodOffset, searchBy);
        if(!searchResult)
            return false;
        return true;
    }
};

class CodInterpreter_V1 : public CodInterpreter {

private:
    static const uint8_t V1_TABLE_PROP_SIZE = 10;       // table properties size
    static const uint8_t V1_STRUCT_PROP_SIZE= 7;       // struct properties size   
    static const uint8_t V1_ENTYPE_TABLE    = 1;       // table entry type 
    static const uint8_t V1_ENTYPE_STRUCT   = 2;       // struct entry type 
    static const uint8_t V1_START_OFFSET    = 9;

public: 
    /*
    *
    *
    * */
    CodInterpreter_V1(File * pFile, CodEntry* codProperties, uint8_t *entryType) : CodInterpreter(pFile, codProperties, entryType)
   {
    
   }
    /*  @brief  : Search table with specific id and read its properties if exists 
    *   @param  : id - id of table 
                  offset - offset location in bytes of data location 
    *   @retval : true - all good 
                  false - fail
    */
    bool SearchProp(uint16_t id, uint32_t *dataOffset, uint8_t searchBy) //__attribute__((optimize("Og")))
    {
        uint32_t bytesAvalable;
        uint16_t tmpID, tmpSizeR, tmpSizeC, tmpHandler;
        uint8_t tmpDataType, tmpEntryType;
        uint32_t localOffset, fileSize, skipBytes;
        uint8_t multiplier = 0;
        uint8_t localNrCrt;
        localOffset = V1_START_OFFSET;                     // start the search from beggining , linear for now 
        fileSize = pFile->size();                             // get file size

        while(localOffset < fileSize)
        {
            if(pFile->position() != localOffset)
                pFile->seek(localOffset);

            bytesAvalable = pFile->available();
            if(bytesAvalable < V1_STRUCT_PROP_SIZE)   // we must have at least structure porp size  
                return false;

            pFile->read((uint8_t*)&tmpEntryType, 1);  // get entry type 
            pFile->read((uint8_t*)&tmpID, 2);         // get ID
            pFile->read((uint8_t*)&tmpHandler, 2);       // get lut type 

            if(tmpEntryType == V1_ENTYPE_STRUCT) {
                pFile->read((uint8_t*)&tmpSizeC, 2);      // get size of structure, in bytes
                if(!tmpSizeC)
                    return false;
                localOffset += V1_STRUCT_PROP_SIZE;
                skipBytes = tmpSizeC;
                localNrCrt++;
                if((tmpID == id && !searchBy) || (localNrCrt == id && searchBy))
                {   // found struct with the right ID
                    *dataOffset = localOffset;      
                    codProperties->structure.ID = tmpID;                
                    codProperties->structure.Size = tmpSizeC;
                    codProperties->structure.Handler = tmpHandler;
                    *entryType = tmpEntryType;
                    entryDataSize = skipBytes;
                    return true;
                }

            } else if(tmpEntryType == V1_ENTYPE_TABLE) {

                pFile->read((uint8_t*)&tmpSizeC, 2);      // get SizeH (columns)
                pFile->read((uint8_t*)&tmpSizeR, 2);      // get SizeV (rows)

                pFile->read((uint8_t*)&tmpDataType, 1);   // get data type
                if(!tmpSizeR || !tmpSizeC || !tmpDataType)   // Check validity
                    return false;
                if(tmpDataType == 3)    // type is float , so 4 byte
                    multiplier = 4;
                else 
                    multiplier = tmpDataType;

                localOffset += V1_TABLE_PROP_SIZE;
                skipBytes = (uint32_t)(tmpSizeR * tmpSizeC * multiplier);
                localNrCrt++;  
                if((tmpID == id && !searchBy) || (localNrCrt == id && searchBy))
                {   // found table with the right ID
                    *dataOffset = localOffset;      
                    codProperties->table.ID = tmpID;                
                    codProperties->table.Rows = tmpSizeR;                
                    codProperties->table.Columns = tmpSizeC;             
                    codProperties->table.DataType = tmpDataType;                
                    codProperties->table.Handler = tmpHandler;
                    *entryType = tmpEntryType;
                    entryDataSize = skipBytes;
                    return true;
                }

            } else {
                return false;   // entry type not valid , dont know how many byte to skip so declare failure
            }

            localOffset += skipBytes;
        }

        return false;

    }

    /*  @brief  : Read the properties of an entry (table or struct)
    *   @param  : id - id of entry
    *   @retval : true - all good
    *             false - something went wrong 
    */
    bool ReadProp(uint16_t id, uint8_t searchBy = 0) //__attribute__((optimize("Og")))
    {   
        uint32_t bytesAvalable;
        uint16_t res;
        bool searchResult;

        searchResult = SearchProp(id, &currentCodOffset, searchBy);
        if(!searchResult)
            return false;
        return true;
    }


};

class CodInterpreter_V3 : public CodInterpreter_V2 {
    private:
    static const uint8_t V3_ENTYPE_BIN    = 98;     // struct entry type ascii for "s"
    static const uint8_t V3_BIN_PROP_SIZE = 13;
    /*  @brief  :
    *   @param  :
    *   @retval :
    * */
   uint8_t GetEntryType(uint8_t entryType)
   {
        switch(entryType)
        {
            case V2_ENTYPE_TABLE:
                return COD_ENTYPE_TABLE;
            case V2_ENTYPE_STRUCT:
                return COD_ENTYPE_STRUCT;
            case V3_ENTYPE_BIN:
                return COD_ENTYPE_BIN;
        }
        return 255;
   }

   public:
    /*  @brief  :
    *   @param  :
    *   @retval :
    * */
   CodInterpreter_V3(File * pFile, CodEntry* codProperties, uint8_t *entryType) : CodInterpreter_V2(pFile, codProperties, entryType)
   {

   }

    /*  @brief  : Search table with specific id and read its properties if exists 
    *   @param  : id - id of table 
                  offset - offset location in bytes of data location
                  searchBy - id 
    *   @retval : true - all good 
                  false - fail
    */
    bool SearchProp(uint16_t id, uint32_t *dataOffset, uint8_t searchBy) //__attribute__((optimize("Og")))
    {
        uint32_t bytesAvalable;
        uint16_t tmpID, tmpSizeR, tmpSizeC, tmpHandler;
        uint8_t tmpDataType, tmpEntryType;
        uint32_t localOffset, fileSize, skipBytes;
        uint8_t multiplier = 0;
        uint8_t localNrCrt = 0;
        localOffset = V2_START_OFFSET;                     // start the search from beggining , linear for now 
        fileSize = pFile->size();                             // get file size

        while(localOffset < fileSize)
        {
            if(pFile->position() != localOffset)
                pFile->seek(localOffset);

            bytesAvalable = pFile->available();
            if(bytesAvalable < V2_STRUCT_PROP_SIZE)   // we must have at least structure porp size  
                return false;

            pFile->read((uint8_t*)&tmpEntryType, 1);  // get entry type 
            pFile->read((uint8_t*)&tmpID, 2);         // get ID
            pFile->read((uint8_t*)&tmpHandler, 2);       // get lut type 
            if(tmpEntryType == V2_ENTYPE_STRUCT) {
                pFile->read((uint8_t*)&tmpSizeC, 2);      // get size of structure, in bytes
                if(!tmpSizeC)
                    return false;
                localOffset += V2_STRUCT_PROP_SIZE;
                skipBytes = tmpSizeC;
                localNrCrt++; 
                if( (tmpID == id && !searchBy) || (localNrCrt == id && searchBy) )
                {   // found struct with the right ID
                    *dataOffset = localOffset;      
                    codProperties->structure.ID = tmpID;                
                    codProperties->structure.Size = tmpSizeC;
                    codProperties->structure.Handler = tmpHandler;
                    *entryType = GetEntryType(tmpEntryType);
                    entryDataSize = skipBytes;
                    return true;
                }
            } else if(tmpEntryType == V2_ENTYPE_TABLE) {

                pFile->read((uint8_t*)&tmpSizeC, 2);      // get SizeH (columns)
                pFile->read((uint8_t*)&tmpSizeR, 2);      // get SizeV (rows)
                uint8_t localTypeChar[V2_DATATYPE_SIZE];
                pFile->read((uint8_t*)&localTypeChar, V2_DATATYPE_SIZE);   // get data type
                tmpDataType = GetTypeFromString((char*)&localTypeChar);

                if(!tmpSizeR || !tmpSizeC || !tmpDataType)   // Check validity
                    return false;
                multiplier = GetSizeOfType(tmpDataType);
                localOffset += V2_TABLE_PROP_SIZE;
                skipBytes = (uint32_t)(tmpSizeR * tmpSizeC * multiplier);
                localNrCrt++; 
                if( (tmpID == id && !searchBy) || (localNrCrt == id && searchBy))
                {   // found table with the right ID
                    *dataOffset = localOffset;      
                    codProperties->table.ID = tmpID;                
                    codProperties->table.Rows = tmpSizeR;                
                    codProperties->table.Columns = tmpSizeC;             
                    codProperties->table.DataType = tmpDataType;                
                    codProperties->table.Handler = tmpHandler;
                    *entryType = GetEntryType(tmpEntryType);
                    entryDataSize = skipBytes;
                    return true;
                }
            } else if(tmpEntryType == V3_ENTYPE_BIN) {
                uint32_t crc32, lsize;
                pFile->read((uint8_t*)&lsize, 4);      // get size of structure, in bytes
                pFile->read((uint8_t*)&crc32, 4);      // get size of structure, in bytes
                if(!lsize)
                    return false;
                localOffset += V3_BIN_PROP_SIZE;
                skipBytes = lsize;
                localNrCrt++; 
                if( (tmpID == id && !searchBy) || (localNrCrt == id && searchBy) )
                {   // found struct with the right ID
                    *dataOffset = localOffset;      
                    codProperties->bin.ID = tmpID;
                    codProperties->bin.Handler = tmpHandler;                
                    codProperties->bin.Size = lsize;
                    codProperties->bin.CRC32 = crc32;
                    *entryType = GetEntryType(tmpEntryType);
                    entryDataSize = skipBytes;
                    return true;
                }
            } else {
                return false;   // entry type not valid , dont know how many byte to skip so declare failure
            }

            localOffset += skipBytes;
        }

        return false;

    }

    /*  @brief : Calculate crc of bin content - must be multiple of 4 bytes
    *   @param : fileOffset - offset in file where data starts 
    *            size - size in bytes of content
    *   @retval: 
    */
    uint32_t BinCrc(uint32_t fileOffset, uint32_t size)
    {
        uint32_t bytesAvalable, toRead, crcCalculated, curPos;
        bool result;
        
        uint8_t localStorVar[4];
        #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
        if(!pFile || *pFile == NULL)    // ensure we have a file stream to read 
            return 0;                   
        #else 
        if(!pFile || *pFile == 0)    // ensure we have a file stream to read 
            return 0;
        #endif
        if(size % 4 || (fileOffset + size > pFile->size()) ) // data content multiple of 4
            return 0;                                        // offset in file plus file size must not exceed file size 

        LOCK_SD(true);                                      // lock SD - so operation wont be disturbed 
        curPos = pFile->position();
        if(curPos != fileOffset)                 // position in file at desired offset 
            if(!pFile->seek(fileOffset)) return 0;          

        bytesAvalable = pFile->available();                 // get available bytes 
        if(!bytesAvalable || (bytesAvalable < size))        // make after position we have the available bytes 
            return 0;
        // Calculate CRC32
        for(uint32_t indx = 0 ; indx < size; indx += 4)
        {
            pFile->read((uint8_t*)&localStorVar[0], 4);
            #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
            // TODO add crc for ESP
            if(indx == 0)
                crcCalculated = HAL_CRC_Calculate(&stm32l4_crc, (uint32_t*)&localStorVar[0], 4); // reset crc and calculate      4 
            else
                crcCalculated = HAL_CRC_Accumulate(&stm32l4_crc, (uint32_t*)&localStorVar[0], 4); // accumulate       4 
            #endif
        }
        pFile->seek(curPos);       // reset file position at find position  
        // result = this->ReadHeader(); TODO check out this

        LOCK_SD(false);

        return crcCalculated;
    }
};


class CodReader{
public:
    CodEntry codProperties; // ID, handler and size
    uint8_t entryType;      // COD_ENTYPE_TABLE=1, COD_ENTYPE_STRUCT=2
    CodInterpreter *codInterpreter;
    uint8_t headerNrEntries;
    File file;   // moved to public because we want full accsess 
private:        

    char headerName[COD_HEADER_LEN];
    uint8_t openMode;
public:
    /* @brief   :   class constructor , initialize members to default values 
    *  @param   : void 
    *  @retval  : void   
    */
    CodReader() //__attribute__((optimize("O0")))
    {
        file = File();      // make it null 
        headerNrEntries = 0;
        codInterpreter = NULL;
        memset(&headerName, 0, COD_HEADER_LEN);
        codProperties.table.ID = 0;                         // default value , id  in file must not be 0 so the search will perform accordingly
        codProperties.table.Rows = 0;
        codProperties.table.Columns = 0;
        codProperties.table.DataType = 0;
        codProperties.table.Handler = 0;
        openMode = 0;
    }

    /* @brief   : class destructor, dealloc created obj interpreter
    *  @param   :  
    *  @retval  : 
    */
    ~CodReader() //__attribute__((optimize("O0")))
    {
        if(codInterpreter) {
            delete codInterpreter;
            codInterpreter = NULL;
        }
    }
 
    /*  @brief  : Open for reading the COD file 
    *   @param  : filename - name of file want to open 
                  0 - read 
                  1 - read + overwrite 
    *   @retval : true - all good 
    *             false - something went wrong 
    */
    bool Open(const char* filename, uint8_t openFor = 0) // __attribute__((optimize("Og")))
    {
        // if (!strstr(filename, ".dat"))      // check if the extention of file is lut specific   
        //     return false;
        
        uint32_t calcCrC, readCRC;

        LOCK_SD(true);

        if(!LSFS::Exists(filename)) // check if the file we want to open exists 
            {LOCK_SD(true); return false;}           
        if(!openFor)
            file = LSFS::Open(filename); // open the file for read
        else 
            file = LSFS::OpenForOverWrite(filename); // open the file for read
        
        if(!file)                    // check if 
            {LOCK_SD(true); return false;}   
        openMode = openFor;
        if(!this->ReadHeader()) 
            {LOCK_SD(true); return false;}   


        LOCK_SD(false);
        return true;
    }

    /*  @brief  : Close currently open file
    *   @param  : void 
    *   @retval : void 
    */
    void Close() //__attribute__((optimize("Og")))
    {   
        LOCK_SD(true);
        if(file)            // close the file 
            file.close();
        LOCK_SD(false);
        if(codInterpreter) {
            delete codInterpreter;
            codInterpreter = NULL;
        }
        //CodReader();   // reset ptivate memebers to default 
    }


    /*  @brief  : Find an entry with specific nujmeratin on file 
    *   @param  : crtNr 
    *
    *   @retval : -1 - erro 
    *              0 - not found 
    *              1 (COD_ENTYPE_TABLE) - table found 
    *              2 (COD_ENTYPE_STRUCT) - struct found
    */
    int8_t GetEntry(uint8_t nrCrt)
    {
        bool result;

        if(!file) 
            return -1;            // if we dont have an open file return error 
        if(nrCrt > this->headerNrEntries)
            return 0;

        LOCK_SD(true);
        result = codInterpreter->ReadProp(nrCrt , 1); // read properties
        LOCK_SD(false);
        if(!result) 
            return 0;
        else 
            return (int8_t)(this->entryType);

    }

    /*  @brief  : Find an entry with a specific id 
    *   @param  : ID - id of entry 
                  handler: handler type  
                  *propertties
    *   @retval : -1 - erro 
    *              0 - not found 
    *              1 (COD_ENTYPE_TABLE) - table found 
    *              2 (COD_ENTYPE_STRUCT) - struct found
    */
    int8_t FindEntry(uint16_t ID)  //__attribute__((optimize("Og")))
    {
        bool result;

        if(!file) 
            return -1;            // if we dont have an open file return error 
        LOCK_SD(true);
        // result = this->ReadHeader();    // Read the header of lut 

        // if(result)
        {   result = codInterpreter->ReadProp(ID); // read properties
            LOCK_SD(false);
            if(!result) 
                return 0;
            else 
                return (int8_t)(this->entryType);
        }

        LOCK_SD(false);
        return -1; 
    }
   

    /*  @brief  : Read data of an entry with specific ID and put it on dest pointer
    *   @param  : ID 
                  dest - pointer to dest bfr
                  destBytes - size of dest bytes  
    *   @retval : number of bytes transferred to destination  
    */
    uint32_t ReadEntry(uint16_t ID, void* dest, uint32_t destBytes) //__attribute__((optimize("Og")))
    {
        int8_t result;
        CodEntry tmpProperties;
        bool readRes;

        result = this->FindEntry(ID);

        if (result > 0)
        {
            if(destBytes < codInterpreter->entryDataSize)
                return 0;       
            LOCK_SD(true);
            readRes =  codInterpreter->ReadData8(ID, (uint8_t*)dest, codInterpreter->entryDataSize);
            LOCK_SD(false);
            if (readRes) 
                return codInterpreter->entryDataSize;       // success
        }
        return 0;    // failed
    }

    // Read an entry, check handler and size, then close file
    bool CheckAndReadEntryThenClose(uint16_t ID, uint16_t handler, void* dest, uint32_t destBytes) 
    {   uint32_t readResult = ReadEntry(ID, dest, destBytes);           // attempt to read
        Close();
        if (readResult != destBytes) return false;                      // wrong size
        if (codProperties.structure.Handler != handler) return false;   // wrong handler
        return true;    
    }

    /*
    *  @ID_NRCrt: ID of entry to  overwrite, if searchBy = false (default)
                  Number of entru to overwrite, if searchBy = true
    *  @retval  : 0 - id or NRCrt not found 
    *            -1 - file not open or file is not open for write
    *            -2 - nr of bytes we want to overWrite do not matched with entry size 
    *            -3 - CRC update failed 
    *            -4 - offset in File cound not be set 
    *            -5 - write failed -nr of command to write do not matched written bytes 
    *            >0 - nr of bytes written 
    */
    int32_t OverwriteEntry(uint16_t ID_NRCrt, void *buffer, uint32_t nrBytes, uint8_t searchBy = 0) // __attribute__((optimize("Og")))
    {
        int32_t result;
        bool resultOpen;
        if(!file || !openMode) 
            return -1;            // if we dont have an open file or an open file for overwrite

        // File is open on read , must add write properties to it
        if(!searchBy) 
            result = this->FindEntry(ID_NRCrt);
        else 
            result = this->GetEntry(ID_NRCrt);

        if(result > 0) 
        {
            if(nrBytes != codInterpreter->entryDataSize)
                return -2;
            LOCK_SD(true);
                
            if(file.position() != codInterpreter->currentCodOffset) // check if position is located at desired offset 
                if(!file.seek(codInterpreter->currentCodOffset))    // set desired offset 
                    return -4;

            result = file.write((uint8_t*)buffer, nrBytes);
            if(result != nrBytes)
                return -5;
            
            if(!strcmp((char*)&headerName[0], "cod_v02") || !strcmp((char*)&headerName[0], "cod_v03"))
            {   
                if(codInterpreter->UpdateFileCrC() == 0) result = -3;
            }

            LOCK_SD(false);
            return result;

        }
        return result;   // id not found 
    }

    /*  @brief  : Print entries between idmin and idmanx 
    *   @param  : IDmin 
                  IDmax
    *   @retval : void
    */
    void PrintEntries(uint16_t IDmin, uint16_t IDmax) 
    {
        int8_t searchResult;
        for (uint16_t id = IDmin; id<IDmax; id++) {
            searchResult = FindEntry(id);
            switch (searchResult) {
                case COD_ENTYPE_TABLE:  STDOUT.print("- Found ID "); STDOUT.print(id);
                                        STDOUT.print(": table "); STDOUT.print(codProperties.table.Rows); STDOUT.print(" x "); STDOUT.print(codProperties.table.Columns);
                                        STDOUT.print(", handler = "); STDOUT.print(codProperties.table.Handler);
                                        STDOUT.println(".");
                                        break;
                case COD_ENTYPE_STRUCT: STDOUT.print("- Found ID "); STDOUT.print(id);
                                        STDOUT.print(": struct with "); STDOUT.print(codProperties.structure.Size); 
                                        STDOUT.print(" bytes, handler = "); STDOUT.print(codProperties.table.Handler);
                                        STDOUT.println(".");
                                        break;
            }
        }
    }
private: 

    /*  @brief  : Read the header 
    *   @param  : void 
    *   @retval : true - all okay
    *             false - fail 
    */
    bool ReadHeader()  //__attribute__((optimize("Og")))
    {
        uint32_t bytesAvalable;
        bool result;
        if(file.position() != 0)
            file.seek(0);

        bytesAvalable = file.available();
        if(!bytesAvalable)
            return false;

        file.read((uint8_t*)&headerName[0], COD_HEADER_LEN);
        file.read((uint8_t*)&headerNrEntries, 1);
        
        if(!strcmp((char*)&headerName[0], "cod_v01")) {
            codInterpreter = new CodInterpreter_V1(&file, &codProperties, &entryType);
            if(!codInterpreter) return false;   // allocation failed 

            return true;
        } 
        else if(!strcmp((char*)&headerName[0], "cod_v02"))
        {   
            codInterpreter = new CodInterpreter_V2(&file, &codProperties, &entryType);
            if(!codInterpreter) return false;   // allocation failed
            uint32_t readCRC, calcCRC;
            calcCRC = codInterpreter->GetFileCrC(&readCRC);
            if(calcCRC != readCRC)
                #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
                return false;    // TODO false
                #else
                return true;
                #endif
            return true;
        }  
        else if(!strcmp((char*)&headerName[0], "cod_v03"))
        {
            codInterpreter = new CodInterpreter_V3(&file, &codProperties, &entryType);
            if(!codInterpreter) return false;   // allocation failed
            uint32_t readCRC, calcCRC;
            calcCRC = codInterpreter->GetFileCrC(&readCRC);
            if(calcCRC != readCRC)
                return false;    // TODO false 
            return true;
        }
        // result = this->CheckHeader();

        return false;
    }
   
};


#endif // XCOD_READER_H
