/********************************************************************
 *  BOARD, INSTALL AND USER CONFIGURATION                           *
 *  (C) Marius RANGU @ RSX Engineering                              *
 *  fileversion: v1.0 @ 2022/08;    license: GPL3                   *
 ********************************************************************
 
 ********************************************************************/
 



#ifndef XCONFIG_H
#define XCONFIG_H

#define INSTALL_MAXID   10          // Will search in install.cod for the install entry with smallest ID in the range 1 ... INSTALL_MAXID




// Get a pointer to current_config.bladeX
inline BladeBase ** BladeAddress_Ptr(uint8_t bladeNo) {
    switch (bladeNo) {
        #if NUM_BLADES >= 1
            case 1: return &(blades[0].blade1);
        #endif
         #if NUM_BLADES >= 2
            case 2: return &(blades[0].blade2);
        #endif
        #if NUM_BLADES >= 3
            case 3: return &(blades[0].blade3);
        #endif
        #if NUM_BLADES >= 4
            case 4: return &(blades[0].blade4);
        #endif
        #if NUM_BLADES >= 5
            case 5: return &(blades[0].blade5);
        #endif
        #if NUM_BLADES >= 6
            case 6: return &(blades[0].blade6);
        #endif                                   
        default: return 0;
    }
}

// Install blade from COD file
// __attribute__((optimize("Og")))
// bladeNo starts at 1 !!!
bool InstallBlade(uint8_t bladeNo, const char* filename, uint16_t ID)  {
    union {
        struct {    // Analog blade definition
            char bladeType;         // 'a'
            uint16_t nLEDs;         // number of AnalogLEDs = [1, 4]. Normally a single AnalogLED per blade;
            uint16_t ledIDs[4];     // ID for each AnalogLED (handler = xAnalogLED)
            uint8_t pins[4][4];     // PWM pins for each emitter of each AnalogLED = [1, 4]
            float bladeBrightness;
        } __attribute__((packed))analogBlade;   
        struct {    // Pixel blade definition
            char bladeType;     // 'p'
            uint16_t driverID;  // PixelDriver 
            uint8_t pin; // Data pin = [1, 2]
            float bladeBrightness;
            uint8_t notused[23]; 
        } __attribute__((packed)) pixelBlade;
    } bladeData;

    CodReader reader;


     
    // 1. Read blade definition from COD file
    uint8_t err=0;      // assume success
    if (bladeNo<1 || bladeNo>NUM_BLADES) err = 1; // return false;      // Blade not supported
    if (!reader.Open(filename)) err = 2;    // return false;               // File not found
    if (reader.FindEntry(ID) != COD_ENTYPE_STRUCT) err = 3; //{ reader.Close(); return false; }   // wrong entry type
    if (reader.codProperties.structure.Handler != HANDLER_BladeInstaller) err = 4; // { reader.Close(); return false; }   // wrong handler
    uint32_t numBytes = reader.ReadEntry(ID, (void*)&bladeData, sizeof(bladeData));      // Attempt to read a structure with the requested ID. 
    reader.Close();     
    if (numBytes != sizeof(bladeData)) err = 5; // return false;  

    BladeBase ** destinationBladePtr;  // this is where we'll hold the pointer to the new blade
    destinationBladePtr = BladeAddress_Ptr(bladeNo);   
    if (!destinationBladePtr) err = 6; // return false;  

    if (err) {
        // Report error and end installer here
        #ifdef DIAGNOSE_BOOT
            STDOUT.print("* Installing blade #"); STDOUT.print(bladeNo); 
            STDOUT.print(" from file "); STDOUT.print(filename);
            STDOUT.print(", ID = "); STDOUT.print(ID); STDOUT.println(" ..."); 

            STDOUT.print("FAILED, ");
            switch (err) {
                case 1: STDOUT.println("invalid blade number. "); break;
                case 2: STDOUT.println("cannot open file. "); break;
                case 3: STDOUT.println("cannot find blade definition. "); break;
                case 4: STDOUT.println("wrong handler. "); break;
                case 5: STDOUT.println("invalid blade definition. "); break;
                case 6: STDOUT.println("blade already in use or not supported. "); break;               
            }
        #endif
        return false;
    }

    switch (bladeData.analogBlade.bladeType) {
        case 'a':  { // -- Analog blade --
            #ifdef DIAGNOSE_BOOT
                STDOUT.print("* Installing analog blade #"); STDOUT.print(bladeNo); 
                STDOUT.print(" at "); STDOUT.print((int)(100*bladeData.analogBlade.bladeBrightness)); 
                STDOUT.print("% power from "); STDOUT.print(filename); 
                STDOUT.print(", ID = "); STDOUT.print(ID);  STDOUT.println(" ..."); 
            #endif // DIAGNOSE_BOOT
            if (bladeData.analogBlade.nLEDs) { 
                // 2. Create permanent objects
                Simple_Blade* tmpBladePtr;
                tmpBladePtr = new Simple_Blade();         // create blade (never deleted!)
                *destinationBladePtr = tmpBladePtr;     // store blade pointer in current_config

                // 3. Install analog LEDs
                for (uint8_t led=0; led<bladeData.analogBlade.nLEDs; led++) {
                    // For each LED: 
                    // 3.1 Seach for AnalogLED definition
                    #ifdef DIAGNOSE_BOOT
                        STDOUT.print("... Installing analog LED #"); STDOUT.print(led+1); 
                    #endif // DIAGNOSE_BOOT
                    struct {
                        uint8_t nEm;
                        uint16_t channelIDs[4];
                    }__attribute__((packed)) ledData;

                    if (bladeData.analogBlade.ledIDs[led]) {
                        bool foundLED = false;    // assume failure
                        reader.Open(filename);    // Search in current file
                        if (reader.FindEntry(bladeData.analogBlade.ledIDs[led]) == COD_ENTYPE_STRUCT) {
                                #ifdef DIAGNOSE_BOOT
                                    STDOUT.print(" from file "); STDOUT.print(filename);
                                    STDOUT.print(", ID = "); STDOUT.print(bladeData.analogBlade.ledIDs[led]); STDOUT.println("... ");
                                #endif
                                if (reader.codProperties.table.Handler == HANDLER_AnalogLED) {
                                    uint32_t numBytes = reader.ReadEntry(bladeData.analogBlade.ledIDs[led], (void*)(&ledData), sizeof(ledData));      // Attempt to read a table with the requested ID. 
                                    reader.Close();     
                                    if (numBytes == sizeof(ledData)) foundLED = true;  
                                }
                                else reader.Close();
                        }
                        else { // Search in ledlib
                            #ifdef DIAGNOSE_BOOT
                                STDOUT.print(" from file "); STDOUT.print(LEDLIB_FILE);
                                STDOUT.print(", ID = "); STDOUT.print(bladeData.analogBlade.ledIDs[led]); STDOUT.println("... ");
                            #endif
                            if (reader.Open(LEDLIB_FILE))
                                if (reader.FindEntry(bladeData.analogBlade.ledIDs[led]) == COD_ENTYPE_STRUCT) 
                                    if (reader.codProperties.table.Handler == HANDLER_AnalogLED) {
                                        uint32_t numBytes = reader.ReadEntry(bladeData.analogBlade.ledIDs[led], (void*)(&ledData), sizeof(ledData));      // Attempt to read a table with the requested ID. 
                                        reader.Close();     
                                        if (numBytes) foundLED = true;  
                                    }
                                    else reader.Close();
                        } 
                        if (!foundLED) {
                            #ifdef DIAGNOSE_BOOT
                                STDOUT.print("FAILED, could not find LED definition, ID="); STDOUT.println(bladeData.analogBlade.ledIDs[led]);
                            #endif // DIAGNOSE_BOOT
                            return false;
                        }

                        // 3.2 Count LED channels, emitters and pins. Check pin assignment is consistent before creating objects.
                        uint8_t numChannels = 0;
                        for (uint8_t i=0; i<ALED_MAXCH ; i++)
                            if (ledData.channelIDs[i]) numChannels ++;
                        bool success = true;
                        if (!numChannels) success = false;     // LED  with no channel
                        xAnalogLED_Channel::ledChannelData channelData[ALED_MAXCH];     //  data for all channels
                        uint8_t totalLEDems;                // total number of emitters, across all channels
                        for (uint8_t i=0; i<numChannels; i++) {
                            if (!xAnalogLED_Channel::get_ledChannelData(filename, ledData.channelIDs[i], channelData+i)) 
                                if (!xAnalogLED_Channel::get_ledChannelData(LEDLIB_FILE, ledData.channelIDs[i], channelData+i)) {
                                #ifdef DIAGNOSE_BOOT
                                    STDOUT.print("FAILED, could not find channel data,  ID="); STDOUT.println(ledData.channelIDs[i]);
                                #endif // DIAGNOSE_BOOT
                                return false;           // fatal error, do not continue
                            }
                            totalLEDems += channelData[i].nEm;
                            // STDOUT.print("[InstallBlade] Updating number of emitters to ");  STDOUT.println(totalLEDems); 
                        }
                        
                        if (totalLEDems != ledData.nEm) success = false;       // inconsistent number of emitters
                        uint8_t ledPins[4];
                        uint8_t totalLEDpins = 0;
                        for (uint8_t i=0; i<ALED_MAXCH; i++) {
                            // hwPins[i] = HardwarePin(bladeData.analogBlade.pins[led][i]);        // those pins must be distributted along all channels
                            // if (hwPins[i] != NO_PIN) totalLEDpins++;
                            ledPins[i] = bladeData.analogBlade.pins[led][i];        // those pins must be distributted along all channels
                            if (ledPins[i]) totalLEDpins++;
                        }
                        if (totalLEDems != totalLEDpins) success = false;       // inconsistent number of pins
                        if (!success) {
                            #ifdef DIAGNOSE_BOOT
                                STDOUT.println("FAILED, inconsistent pin assignment. Check LED library!");
                            #endif // DIAGNOSE_BOOT
                            return false;
                        }


                        // 3.3 Install each channel
                        xAnalogLED* ledPtr;
                        volatile xAnalogLED* extraptr;
                        xAnalogLED_Channel* channelPtr;
                        uint8_t pinIndex = 0;                   // index of next unassigned pin
                        ledPtr = new xAnalogLED();              // Create analog LED (never deleted!)
                        for (uint8_t ch=0; ch<numChannels; ch++) {
                            uint8_t channelPins[ALED_MAXEM] = { 0, 0, 0 };    
                            for (uint8_t i=0; i<channelData[ch].nEm; i++)     // Populate channelPins vector with as many pins as the channel needs; the rest remain NO_PIN
                                channelPins[i] = ledPins[pinIndex++];
                            channelPtr = new xAnalogLED_Channel();    // never deleted!
                            extraptr = ledPtr;
                            #ifdef DIAGNOSE_BOOT
                                STDOUT.print("..... Installing analog LED Channel #"); STDOUT.print(ch+1); STDOUT.print(" at pin(s) LED"); STDOUT.print(channelPins[0]); 
                                if (channelPins[1]) {
                                    STDOUT.print(", LED");  STDOUT.print(channelPins[1]);
                                    if (channelPins[2]) {
                                        STDOUT.print(", LED"); STDOUT.print(channelPins[2]); 
                                    }

                                }
                                STDOUT.print(", ID = "); STDOUT.print(ledData.channelIDs[ch]); STDOUT.println("... "); 
                            #endif // DIAGNOSE_BOOT                            
                            static uint8_t usedLEDpins = 0;     // keep track of LED pins in use
                            bool success = true;        // assume success                                                        
                            for (uint8_t i=0; i<channelData[ch].nEm; i++) {
                                bool pinSuccess = true;
                                if (channelPins[i] > PWM_CHANNELS) { // LED pin not physically available - warning but let it install
                                    #ifdef DIAGNOSE_BOOT
                                        STDOUT.print(" >>>>>>>>>> WARNING: pin LED"); STDOUT.print(channelPins[i]); STDOUT.println(" not phisically available! <<<<<<<<<< "); 
                                    #endif
                                    // pinSuccess = false;
                                }
                                uint8_t usageMask = 1 << (channelPins[i]-1); // set a single bit, position indicates DAT pin
                                if (usageMask & usedLEDpins) {
                                    #ifdef DIAGNOSE_BOOT
                                        STDOUT.print("....... FAILED, Pin LED"); STDOUT.print(channelPins[i]); STDOUT.println(" already in use, cannot install driver "); 
                                    #endif
                                    pinSuccess = false;
                                }
                                if (pinSuccess) {
                                    channelPins[i] = HardwarePin(channelPins[i]);   // Convert pin numbering (LED1, LED2, etc) to hardware pin
                                    usedLEDpins |= usageMask;       // mark DAT pin in use
                                }
                                else success = false;
                            }
                            if (success) {  // attempt to install channel
                                if (channelPtr->Install(&(channelData[ch]), channelPins, bladeData.analogBlade.bladeBrightness)) {
                                    ledPtr->channels[ch] = channelPtr;        // assign newly installed channel to the LED
                                    ledPtr->nCh++;
                                } 
                                else success = false;
                            }
                            if (!success)  { // Failed to initialized at least one channel
                                delete tmpBladePtr;         // free all memories
                                delete channelPtr;           
                                delete ledPtr;
                                *destinationBladePtr = 0;
                                #ifdef DIAGNOSE_BOOT
                                    STDOUT.print("FAILED to install analog LED channel, ID="); STDOUT.println(ledData.channelIDs[ch]);
                                #endif // DIAGNOSE_BOOT
                                return false; 
                            }    
                        }  
                        // 3.4. Assign LED to blade 
                        if (!tmpBladePtr->AssignLED(ledPtr)) {
                            #ifdef DIAGNOSE_BOOT
                                STDOUT.println("FAILED to assign LED to blade"); 
                            #endif // DIAGNOSE_BOOT                     
                            return false;
                        }     // LED initialized, associate it with the blade
                    } else {
                        #ifdef DIAGNOSE_BOOT
                            STDOUT.print("FAILED, unspecified LED #"); STDOUT.println(led+1);
                        #endif // DIAGNOSE_BOOT                     
                        return false;
                    }
                }                
            } else {
                #ifdef DIAGNOSE_BOOT
                    STDOUT.println("FAILED, no LED specified for analog blade.");
                #endif // DIAGNOSE_BOOT
                return false;    // no LED specified in file
            }
          }
        #ifdef DIAGNOSE_BOOT
            STDOUT.print("Blade #"); STDOUT.print(bladeNo); STDOUT.println(" installed successfully.");
        #endif // DIAGNOSE_BOOT
        return true;

        case 'p':  {  // -- Pixel blade --
            // 2. Read pixel driver definition from COD file  
            struct {
                char driver_type;   // 't' = WS281x on timer
                uint16_t nPixels;   // number of pixels = [1, maxLedsPerStrip]
                uint8_t byteorder;  // RGB=0, RBG=1, GRB=2 (default), GBR=3
                uint16_t frequency; // Data frequency [kHz]. Default = 800
                uint16_t reset;     // Frame time [us]. Default = 300
                uint16_t t0h;       // 0-code high time [ns]. Default = 294
                uint16_t t1h;       // 1-code high time [ns]. Default = 892
                uint16_t poffDelay; // Power-off delay [ms]. Default = 3000. Does nothing on Proffie Lite.
            } __attribute__((packed)) pixelDriver;    
            bool foundDriver = false;    // assume failure    
            #ifdef DIAGNOSE_BOOT
                STDOUT.print("* Installing pixel blade #"); STDOUT.print(bladeNo); 
                STDOUT.print(" at "); STDOUT.print((int)(100*bladeData.pixelBlade.bladeBrightness));
                STDOUT.print("% power from "); STDOUT.print(filename); 
                STDOUT.print(", ID = "); STDOUT.print(ID);  STDOUT.println(" ..."); 
            #endif // DIAGNOSE_BOOT
            reader.Open(filename);      // 2.1 Search in current file
            if (reader.FindEntry(bladeData.pixelBlade.driverID) == COD_ENTYPE_STRUCT) 
                    if (reader.codProperties.structure.Handler == HANDLER_PixelDriver) {
                        uint32_t numBytes = reader.ReadEntry(bladeData.pixelBlade.driverID, (void*)&pixelDriver, sizeof(pixelDriver));      // Attempt to read a structure with the requested ID. 
                        reader.Close();     
                        if (numBytes) foundDriver = true;  
                    }
                else reader.Close();
            else { // 2.2 Search in ledlib.cod 
            if (reader.Open(LEDLIB_FILE))
                if (reader.FindEntry(bladeData.pixelBlade.driverID) == COD_ENTYPE_STRUCT) 
                    if (reader.codProperties.structure.Handler == HANDLER_PixelDriver) {
                        uint32_t numBytes = reader.ReadEntry(bladeData.pixelBlade.driverID, (void*)&pixelDriver, sizeof(pixelDriver));      // Attempt to read a structure with the requested ID. 
                        reader.Close();     
                        if (numBytes==sizeof(pixelDriver)) foundDriver = true;  
                    }
                    else reader.Close();
            } 
            if (!foundDriver) return false;
            #ifdef DIAGNOSE_BOOT
                STDOUT.print("... Installing "); 
                switch (pixelDriver.driver_type) {
                    case 't':
                        STDOUT.print("WS281x timer-based driver for ");
                    break;
                    default:
                        STDOUT.print("unknown driver for ");
                }
                STDOUT.print(pixelDriver.nPixels); 
                STDOUT.print(" pixels, ID = "); STDOUT.print(bladeData.pixelBlade.driverID);  STDOUT.print(" ..."); 
            #endif // DIAGNOSE_BOOT            

            // 3. Check and assign DAT pin
            if (bladeData.pixelBlade.pin > PIXEL_CHANNELS) { //  error: invalid DAT pin
                #ifdef DIAGNOSE_BOOT
                    STDOUT.print(" FAILED, pin DAT"); STDOUT.print(bladeData.pixelBlade.pin); STDOUT.println(" not phisically available.");
                #endif
                return false;
            }
            static uint8_t usedDATpins = 0;     // keep track of DAT pins in use
            uint8_t usageMask = 1 << (bladeData.pixelBlade.pin-1);      // set a single bit, position indicates DAT pin
            if (usageMask & usedDATpins) { // fatal error: trying to assign more than one blade to the same DAT pin
                #ifdef DIAGNOSE_BOOT
                    STDOUT.print("FAILED, pin DAT"); STDOUT.print(bladeData.pixelBlade.pin); STDOUT.println(" already in use.");
                #endif
                return false;
            }
            usedDATpins |= usageMask;       // mark DAT pin in use
            int8_t pin;
            switch (bladeData.pixelBlade.pin) {
                case 1: pin = bladePin; break;
                case 2: pin = blade2Pin; break;
                default: 
                        #ifdef DIAGNOSE_BOOT
                            STDOUT.print("FAILED, invalid pin DAT"); STDOUT.println(bladeData.pixelBlade.pin);
                        #endif
                        return false;
            }
            #ifdef DIAGNOSE_BOOT
                STDOUT.println(""); 
            #endif

            // 3. Create and initialize permanent objects                        
            WS2811PIN* tmpPinPtr;            
            switch(pixelDriver.byteorder) {
                // We need to keep BYTEORDER templetized for runtime efficiency, but this is not the best way as each case adds 912 bytes to code
                // TODO: de-templetize WS2811PinBase                
                // case WS2811_RGB:
                //     tmpPinPtr = new WS2811PinBase<Color8::Byteorder::RGB> (pixelDriver.nPixels, pin, pixelDriver.frequency, pixelDriver.reset, pixelDriver.t0h, pixelDriver.t1h);
                // break;                
                // case WS2811_RBG:
                //     tmpPinPtr = new WS2811PinBase<Color8::Byteorder::RBG> (pixelDriver.nPixels, pin, pixelDriver.frequency, pixelDriver.reset, pixelDriver.t0h, pixelDriver.t1h);
                // break; 
                // case WS2811_GBR:
                //     tmpPinPtr = new WS2811PinBase<Color8::Byteorder::GBR> (pixelDriver.nPixels, pin, pixelDriver.frequency, pixelDriver.reset, pixelDriver.t0h, pixelDriver.t1h);
                // break;                             
                default:
                case WS2811_GRB:
                #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
                    tmpPinPtr = new WS2811PinBase<Color8::Byteorder::GRB> (pixelDriver.nPixels, pin, 1000*pixelDriver.frequency, pixelDriver.reset, pixelDriver.t0h, pixelDriver.t1h);
                #else
                    #ifdef RMT_WITH_TASK 
                    tmpPinPtr = new RMTLedPinBase(pixelDriver.nPixels, Color8::Byteorder::GRB);
                    #else 
                    // tmpPinPtr = new RMTLedPinBase<Color8::Byteorder::GRB>(131, GPIO_NUM_11, 740000, 300, 294, 892);
     
                    static uint8_t pinSel = 0;
                    STDOUT.print("PIN Selection "); STDOUT.println(pinSel);
                        if(!pinSel) {
                            tmpPinPtr = new ESPILedPinBase<Color8::Byteorder::GRB>(131, GPIO_NUM_11, 740000, 300, 294, 892);
                            pinSel++;
                        } else {
                            pinSel++;
                            tmpPinPtr = new ESPILedPinBase<Color8::Byteorder::GRB>(131, GPIO_NUM_12, 740000, 300, 294, 892);
                        }
                    #endif
                    //tmpPinPtr = new RMTLedPinBase<Color8::Byteorder::GRB> (pixelDriver.nPixels, pin, 1000*pixelDriver.frequency, pixelDriver.reset, pixelDriver.t0h, pixelDriver.t1h);
                #endif

                break;
            }
            tmpPinPtr->setInstalledBrightness(bladeData.pixelBlade.bladeBrightness);
            WS2811_Blade* tmpBladePtr;
            tmpBladePtr = new WS2811_Blade(tmpPinPtr, 0, pixelDriver.poffDelay);         // create blade (never deleted!)
            *destinationBladePtr = tmpBladePtr;     // store blade pointer in current_config
          }
        #ifdef DIAGNOSE_BOOT
            STDOUT.print("Blade #"); STDOUT.print(bladeNo); STDOUT.println(" installed successfully.");
        #endif // DIAGNOSE_BOOT       
        installConfig.monochrome = false;   
        return true;

        

    }
    return false;
}


struct {
        uint8_t nButtons;       // Number of buttons = [0, 2]
        uint8_t nBlades;        // Number of blades = [0, 6]
        uint16_t bladeID[6];    // Local ID for each blade (0 for not used) - must be defined in the same file !!!
        float maxVolume;        // Scale factory-default maximum audio volume between 0.5 and 1.5 
        float charge;           // Set battery charger current (0 = disable)
        uint16_t APOtime;       // Auto Power Off time [sec]
    } __attribute__((packed)) installData;   // install configuration


// Install blades and peripherals from COD file
// __attribute__((optimize("Og")))
bool Install(const char* filename)  {
    // 1. Find install entry with smallest ID
    CodReader reader;
    int8_t entryType;
    uint8_t id = 1;            

   #ifdef DIAGNOSE_BOOT
        STDOUT.println("");
        STDOUT.println("Starting Install ................................."); 
    #endif
    
    if (!reader.Open(filename)) { 
        #ifdef DIAGNOSE_BOOT
            STDOUT.print("FAILED TO INSTALL: could not find ");
            STDOUT.println(filename);
            STDOUT.println("Install ended ......................... FAILED!"); 
        #endif
        return false;               // File not found
    }
    // STDOUT.print("... file open");
    while (id <= INSTALL_MAXID) {
        if (reader.FindEntry(id) == COD_ENTYPE_STRUCT)   {
            // found a struct with the right ID
            if (reader.codProperties.structure.Handler == HANDLER_Installer)
                break;  // found the smallest ID with the right handler, stop searching
        }
        id++;
    }
    if (id==INSTALL_MAXID+1) { 
        reader.Close();  
        #ifdef DIAGNOSE_BOOT
            STDOUT.print("FAILED TO INSTALL: could not find install entry in file ");
            STDOUT.println(filename);
            STDOUT.println("Install ended ......................... FAILED!"); 
        #endif
        return false;   }    // Install entry not found with the ID in the range 1 ... INSTALL_MAXID
    // STDOUT.print("... found ID "); STDOUT.print(id);

    // 2. Read install data from file
    uint32_t numBytes;
    numBytes = reader.ReadEntry(id, (void*)&installData, sizeof(installData));      // Attempt to read a structure with the requested ID. 
    reader.Close();     
    if (numBytes != sizeof(installData)) {
        #ifdef DIAGNOSE_BOOT
            STDOUT.print("FAILED TO INSTALL: failed to read install data from "); STDOUT.print(filename);
            STDOUT.print(", ID = "); STDOUT.println(id);
            STDOUT.println("Install ended ......................... FAILED!"); 
        #endif
        return false;                             // Could not read 
    }
    // STDOUT.print("... read "); STDOUT.print(numBytes); STDOUT.print("bytes");
    
    // 3. Install buttons (TODO)

    // 4. Install blades
    bool retval = true; // assume success
    if (installData.nBlades) {
        installConfig.nBlades = 0;      // will count only valid blades
        for (uint8_t bladeNo=1; bladeNo <= installData.nBlades; bladeNo++)
            // if (InstallBlade(bladeNo, filename, installData.bladeID[bladeNo-1])) 
            if (InstallBlade(installConfig.nBlades+1, filename, installData.bladeID[bladeNo-1])) // skip invalid blades
                installConfig.nBlades++;    // 
            else
                retval = false; // InstallBlade handles own error messages
            

    }
    else {
        #ifdef DIAGNOSE_BOOT
            STDOUT.println("* WARNING: no blade installed!");
        #endif
    }
    // 5. Set configuration
    if (installData.maxVolume < 0) installData.maxVolume = 0;
    if (installData.maxVolume > 1.5) installData.maxVolume = 1.5;
    installConfig.audioFSR = HW_NOMINAL_VOLUME * installData.maxVolume;     // that's the installed volume, maximum that the board is allowed.
    
    dynamic_mixer.set_volume(installConfig.audioFSR);   // dynamix mixer takes VOLUME pre-install, 'cause mah
    // TODO: move set_volume to profile initialization, just set audioFSR here.

    #ifdef DIAGNOSE_BOOT
        STDOUT.print("* Setting audio power to "); STDOUT.print((int)(100*(installData.maxVolume))); STDOUT.println("%.");
        // STDOUT.print("* VOLUME = "); STDOUT.println(VOLUME); 
    #endif


   #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) && defined(ULTRAPROFFIE_CHARGER)
        installConfig.chargerCurrent = 1000 * installData.charge;       // current in milliamps: 0, 850 or 1000
        if (charger.SetChargeLimit((installConfig.chargerCurrent))) {
            #ifdef DIAGNOSE_BOOT
                STDOUT.print("* Setting charger maximum current to "); STDOUT.print(installConfig.chargerCurrent); STDOUT.println(" milliAmps.");
            #endif
        } else {
            #ifdef DIAGNOSE_BOOT
                STDOUT.print("* Invalid charger setting "); STDOUT.print(installConfig.chargerCurrent); STDOUT.println(", charger disabled.");
            #endif
                installConfig.chargerCurrent = 0;
                retval = 0;
        }
    #endif 
        installConfig.APOtime = installData.APOtime;    // [s]
        installConfig.APOtime *= 1000;                  // [ms]
        #ifdef DIAGNOSE_BOOT
            STDOUT.print("* Setting auto power off time to "); STDOUT.print(installData.APOtime); STDOUT.println(" seconds.");
        #endif

    // STDOUT.println("");
    #ifdef DIAGNOSE_BOOT
        STDOUT.print("Install ended ........................."); 
        if (retval) STDOUT.println(" SUCCESS."); 
        else STDOUT.println(" FAILED!"); 
    #endif
    return retval;

}


#endif // XCONFIG_H