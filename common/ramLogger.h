#ifndef RAM_LOGGER_H
#define RAM_LOGGER_H

#ifdef ENABLE_LOGG_WAVE_OUTPUT

#include <iostream>
#include "stdout.h"

#define RAM_LOGGER_MAX_BYTES (uint32_t)2000000  // eq of 1 MB

// struct LoggedData {
//     uint16_t left;
//     uint16_t right;
// };
struct LoggedData {
    int16_t left;
    // uint16_t right;
};

    char header[] = {
        'R', 'I', 'F', 'F', // Chunk ID
        0, 0, 0, 0,          // Chunk size (to be filled later)
        'W', 'A', 'V', 'E', // Format
        'f', 'm', 't', ' ', // Subchunk1 ID
        16, 0, 0, 0,         // Subchunk1 size
        1, 0,                // Audio format (PCM)
        1, 0,                // Num channels
        #if AUDIO_RATE == 44100
        0x44, 0xAC, 0, 0,    // Sample rate (44100 Hz)
        0x88, 0x58, 0x01, 0, // Byte rate (44100 * 1 * 16 / 8)
        #else 
        0x22, 0x56, 0, 0,    // Sample rate (22050 Hz)
        0xAC, 0x44, 0x00, 0, // Byte rate (22050 * 1 * 16 / 8)
        #endif
        2, 0,                // Block align
        16, 0,               // Bits per sample
        'd', 'a', 't', 'a', // Subchunk2 ID
        0, 0, 0, 0           // Subchunk2 size (to be filled later)
    };

class RAMLogger : public CommandParser {
private:
    LoggedData* logData;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    bool full;
    bool canLog;
    bool tellsaved;


public:
    // Constructor
    RAMLogger(uint32_t maxCapacity) : capacity(maxCapacity), head(0), tail(0), full(false), CommandParser(){
        logData = NULL;
        if(maxCapacity * sizeof(LoggedData) > RAM_LOGGER_MAX_BYTES)
            maxCapacity = RAM_LOGGER_MAX_BYTES / sizeof(LoggedData);
        capacity = maxCapacity;
        canLog = false;
        tellsaved = false;
        // logData = (LoggedData*)heap_caps_malloc(maxCapacity * sizeof(LoggedData) , MALLOC_CAP_SPIRAM);     // maxCapacity * sizeof(LoggedData)
    }

    // Destructor
    ~RAMLogger() {
        if(!logData)
            heap_caps_free(logData);
    }
    // need this function , because allocating mem in ext ram  in constructor will fail 
    void create()
    {
        if(logData) {
            STDOUT<< "Already alloc";
            return;
        }
        
        logData = (LoggedData*)heap_caps_malloc(capacity * sizeof(LoggedData) , MALLOC_CAP_SPIRAM);     // maxCapacity * sizeof(LoggedData)
        if (!logData) {
            STDOUT<< "Failed to alloc ." << capacity * sizeof(LoggedData) << " bytes "; 
            return;
        }
        STDOUT << (uint32_t)logData;     // print pointer addr
    }

    void startLog() {
        if(canLog) return; // nothing to do , already started 
        create();
        if(!logData) return;
        reset();    // reset 
        canLog = true;
    }

    void stopLog( bool save = false) {
        if(!canLog) return; // nothing to do 
        if(!logData) return;
        canLog = false;
        if(save) outputLog(true);
    }

    bool isSaveDone() {
        return tellsaved;
    }

    void setSaveDone(bool state) {
        tellsaved = state;
    }

    // Method to log data
    void log(int16_t value1, int16_t value2) {
        if(!canLog) return;
        if (!logData) {
            STDOUT.println("Logger is not properly initialized.");
            return;
        }
        logData[head].left = value1;
        // logData[head].right = value2;

        head = (head + 1) % capacity;
        if (head == tail)
            full = true;
        if (full)
            tail = (tail + 1) % capacity;
    }

    void log(int16_t value1) {
        if(!canLog) return;
        if (!logData) {
            STDOUT.println("Logger is not properly initialized.");
            return;
        }
        logData[head].left = value1;
        // logData[head].right = value2;

        head = (head + 1) % capacity;
        if (head == tail)
            full = true;
        if (full)
            tail = (tail + 1) % capacity;
    }

    void log(uint16_t *value1) {
        if(!canLog) return;
        if (!logData) {
            STDOUT.println("Logger is not properly initialized.");
            return;
        }

        // logData[head].left = ((*value1) >> 8) | ((*value1) & 0x00FF) << 8;
        logData[head].left = *value1;
        // logData[head].right = value2;

        head = (head + 1) % capacity;
        if (head == tail)
            full = true;
        if (full)
            tail = (tail + 1) % capacity;
    }


    const char* selectFileName()
    {            
        static char str[12];
        uint16_t fileNr = 0;
        sprintf(str, "rec%'.03d.wav\n", fileNr);
        for(uint16_t i = 0; i <= 999; i++)
        {
            sprintf(str, "rec%'.03d.wav\n", i);
            if(!LSFS::Exists(str)) break;
        }
        return str;
    }
    // Method to print logged data
    void outputLog(bool logTofile = false) {
       File file;
       int32_t result;
        if (!logData) {
            STDOUT.println("Logger is not properly initialized.");
        }
        uint32_t index = tail;
        if(logTofile) {

            file = LSFS::OpenForWrite(selectFileName());
            if(file) {
                addWavHeader();
                STDOUT.println(" File created");
            }
        } else {
            STDOUT.print("Logged Data:\n");
        }

        uint32_t count = 0;
        uint32_t sTimeMs;
        if(!logTofile) {
            while (index != head && (full || count < head)) {
                STDOUT << " index: " << count << "left: " << logData[index].left; //<< ", right: " << logData[index].right << " \n";
                index = (index + 1) % capacity;
                count++;
            }
        } else if(file) {
            sTimeMs = millis();
            result = file.write((uint8_t*)header, 44);
            while (index != head && (full || count < head)) {
                result = file.write((uint8_t*)(&(logData[index].left)), 2);
                index = (index + 1) % capacity;
                count++;
            }
            file.close();
            STDOUT << " MS to write File: " << (millis() - sTimeMs) << "\n";
            STDOUT << "Capa " << capacity << " count " << count;
            tellsaved = true;
        }
    }

     void reset() {
        head = 0;
        tail = 0;
        full = false;
    }

    void addWavHeader() {

        uint32_t chunkSize , subChunk2Size;

        if(!full) {
            chunkSize = sizeof(LoggedData) * head + 36;
            subChunk2Size = sizeof(LoggedData) * head;
        } else {
            chunkSize = sizeof(LoggedData) * capacity + 36;
            subChunk2Size = sizeof(LoggedData) * capacity;
        }

        memcpy(header + 4, &chunkSize, sizeof(uint32_t));
        memcpy(header + 40, &subChunk2Size, sizeof(uint32_t));

    }

    bool Parse(const char* cmd, const char* arg) override {
        if(!strcmp(cmd, "startLogger")) {
            startLog();
            return true;
        }

        if(!strcmp(cmd, "stopLogger")) {
            stopLog(true);
            return true;
        }
        
        if(!strcmp(cmd, "genFileNames")) {
            STDOUT.println(selectFileName());
            return true;
        }

        if (!strcmp(cmd, "loggerStatus")) {
            STDOUT << " capacity: " << capacity << "head: " << head << ", tail: " << tail << " \n";
            return true;
        }

        if (!strcmp(cmd, "loggerTest")) {
            STDOUT.print(" PSRAM_Size:     ");STDOUT.print(ESP.getPsramSize()); 
            STDOUT.print(" PSRAM_free:     ");STDOUT.print(ESP.getFreePsram());
            STDOUT.print(" PSRAM_MinFree:  ");STDOUT.print(ESP.getMinFreePsram()); 
            STDOUT.print(" PSRAM_MaxAlooc: ");STDOUT.println(ESP.getMaxAllocPsram());

            create();

            for (int i = 0; i < 50; ++i) 
                log(i, i * 2);
                  // Print the logged data
            outputLog(true);

                  // Reset the logger
            reset();

            // Log some data after reset
            for (int i = 0; i < 10; ++i) {
                log(i, i * 3);
            }

            // Print the logged data after reset
            outputLog();

            return true;
        }
        return false;
    }

    void Help() override {

    }

    
}logger(RAM_LOGGER_MAX_BYTES);
#endif

#endif