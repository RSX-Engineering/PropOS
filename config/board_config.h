
#ifdef CONFIG_TOP
  #ifdef ULTRAPROFFIE    // ================== UltraProffies Config TOP
  
    #if ULTRAPROFFIE_VERSION == 'P' // UltraProffie Proper
        #include "ultraproffie_esp_config.h"    
    #elif  ULTRAPROFFIE_VERSION == 'Z' || ULTRAPROFFIE_VERSION == 'L'  // UltraProffie Zero & Lite
        #include "ultraproffie_stm_config.h"
    #else
        #error UNKNOWN ULTRAPROFFIE
    #endif 


    // =============    DEVELOPER MODE  ============================= 
    // ----- DEVELOPER MODE ON : enable additional developer cmds 
    //                           and dont deactivate diagnostic cmds 
    #ifdef PF_DEVELOPER_MODE_ON 
        #define ENABLE_DEVELOPER_MODE 
    #elif defined(PF_DEVELOPER_MODE_OFF)
        #undef  ENABLE_DEVELOPER_MODE       // undifined developer commands
        // #define DISABLE_DIAGNOSTIC_COMMANDS     // disable diagnostic commands

    #endif
    // ============     END OF DEVELOPER MODE =======================
    
    // ============     STATUS REPORT       ============================
    // ----- STATUS REPORT ON : Enables status reporting when installing 
    //                          or initing different components          
    #ifdef PF_STATUS_REPORT_ON

    #elif defined(PF_STATUS_REPORT_OFF)   // ----- STATUS REPORT OFF : Disables status reporting when installing 
                                        // or initing different components 
    #endif
    // ============ END STATUS REPORT =====================================

    // ============     VOICE MSGs       ============================          
    #ifdef PF_VOICE_MSG_DISABLED // ----- VOICE_MSG : Disables talking status msgs
                    
    #elif defined(PF_VOICE_FEMALE)     // ----- VOICE_MSG_MALE : Enables talking status msgs with male voice
                    
    #elif defined(PF_VOICE_MALE)  // ----- VOICE_MSG_MALE : Enables talking status msgs with male voice  

    #endif
    // ============ END STATUS REPORT =====================================
    

    #define GYRO_MEASUREMENTS_PER_SECOND  1600                   // default Measuremnts per second , default do not remove 
    #define ACCEL_MEASUREMENTS_PER_SECOND 1600                  // default Measuremnts per second , default do not remove 

    #define NUM_BLADES 2
    #ifdef HW_NUM_BLADES 
        #define NUM_BLADES HW_NUM_BLADES
    #endif
    #define NUM_BUTTONS HW_NUM_BUTTONS
    #define HW_NOMINAL_VOLUME 3000  // don't change this unless you change the hardware (0-3 V max dac output) !!!
    #define VOLUME HW_NOMINAL_VOLUME                            // redefined at runtime 
    
    const unsigned int maxLedsPerStrip = 144;                   // max led per strip supported 
    #define ENABLE_AUDIO                                        // enable the audio 
    #define ENABLE_MOTION                                       // enable motion
    #define ENABLE_WS2811                                       // enable ws2811 , only this supported on PFL and PFZ 
    #define ENABLE_SD                                           // enable storage (sd or extern flash , board PFL and PBZ specific)

    // #define OSX_ENABLE_MTP
    #define SERIAL_ASCII_BAUD 9600
    #define SERIAL_BIN_BAUD  921600   
    
    #include <stdint.h>
    #include "../motion/sensitivities.h"
    // =============    INSTALL CONFIGURATION  =============================       
    #define INSTALL_FILE "_osx_/install.cod"    // installation file
    #define OFFLINE_FILE "_osx_/offline.txt"    // publish conent here for offline use
    #define LEDLIB_FILE "_osx_/ledlib.cod"    // LED library file
    static struct  {
        uint16_t audioFSR = VOLUME;
        uint16_t chargerCurrent = 0; // 0, 850 or 1000 [mA]
        uint8_t nBlades = 0;        // number of installed blades
        bool monochrome = true;     // assume monochrome, set true at install time
        uint32_t APOtime;           // auto power off time [ms]
    } installConfig;
    #undef VOLUME
    #define VOLUME installConfig.audioFSR
    // =============    END INSTALL CONFIGURATION  ==========================


    // =====================   USER PROFILE  ================================
    #define PRESETS_FILE "_osx_/presets.cod"    // Presets file
    #define PROFILE_FILE "_osx_/profile.cod"    // User profile file
    
    static struct {
        uint16_t masterVolume = 65535;              // active volume
        uint16_t masterBrightness = 65535;          // active brightness
        uint8_t preset = 0;
        uint16_t apID = 0;   // active presets ID (to know which entry to overwrite in COD)
        ClashSensitivity clashSensitivity;          // all sensitivities inherit from 'Sensitivity' clash and hold both the user setting (0...65535) and the derived working parameters 
        uint16_t combatVolume = 65535;             // master Volume when stealth = off
        uint16_t combatBrightness = 65535;         // master Brightness when stealth = off
        uint16_t stealthVolume = 10000;             // master Volume when stealth = on
        uint16_t stealthBrightness = 10000;         // master Brightness when stealth = off
        SwSensitivity swingSensitivity;
        StabSensitivity stabSensitivity;
        ShakeSensitivity shakeSensitivity;
        TapSensitivity tapSensitivity;
        TwistSensitivity twistSensitivity;
        MenuSensitivity menuSensitivity;
    } userProfile;
    #define CLASH_THRESHOLD_G userProfile.clashSensitivity.clashThreshold
        
    // ==================   END USER PROFILE  =============================== 

    #define OSX_SUBVERSION       "013"       // Fork version 
    #define OSX_ENABLE_MTP              // enables proffieOSx serial transfer protcol                 

    #ifdef ENABLE_DEVELOPER_MODE
        #define ENABLE_DEVELOPER_COMMANDS       // ProffieOS developer commands
        #define X_PROBECPU                      // Enable CPU probes (results reported at STDOUT under "top"). Adds 3k program memory
        // #define X_BROADCAST                      // Enable broadcasting of binary monitoring data. For debug only
        // #ifdef X_BROADCAST
            // #define OBSIDIANFORMAT  // reuse matlabs
            // #define BROADCAST_MOTION             // Broadcast acc & gyro
            // #define BROADCAST_THETA              // Broadcast data for twist navigation
            // #define BROADCAST_SHAKE             // Broadcast data for shake detection
            // #define BROADCAST_2TAP             // Broadcast data for double-tap detection
        // #endif
        // #define X_LIGHTTEST                       // Enable parsing LED test commands
        // #define X_MENUTEST                      // Enable parsing TTMenu commands
        // #define DIAGNOSE_SENSOR
        // #define DIAGNOSE_STORAGE
        // #define DIAGNOSE_AUDIO
        // #define DIAGNOSE_BLADES
        // #define DIAGNOSE_BOOT                      
        // #define DIAGNOSE_EVENTS
        // #define DIAGNOSE_PRESETS
        #define DIAGNOSE_PROP        
        #define DIAGNOSE_POWER
    #endif

    #ifdef PF_STATUS_REPORT_ON
        #define ENABLE_DIAGNOSE_COMMANDS
        #define DIAGNOSE_SENSOR
        #define DIAGNOSE_STORAGE
        #define DIAGNOSE_AUDIO
        #define DIAGNOSE_BLADES
        #define DIAGNOSE_BOOT                      
        // #define DIAGNOSE_EVENTS
        #define DIAGNOSE_PRESETS
        #define DIAGNOSE_PROP        
        // #define DIAGNOSE_POWER
 #endif




  #else // ==================  ProffieBoards Config TOP
        #if PROFFIEBOARD_VERSION - 0 == 2
            #include "proffieboard_v2_config.h"
        #elif PROFFIEBOARD_VERSION - 0 == 3
            #include "proffieboard_v3_config.h"
        #else
        #error UNKNOWN PROFFIEBOARD
        #endif


        #define NUM_BLADES 2
        #ifdef HW_NUM_BLADES 
            #define NUM_BLADES HW_NUM_BLADES
        #endif
        #define NUM_BUTTONS 2


        #include <stdint.h>
        #include "../motion/sensitivities.h"
        // =============    INSTALL CONFIGURATION  ============================= 
        #define INSTALL_FILE "_osx_/install.cod"    // installation file
        #define OFFLINE_FILE "_osx_/offline.txt"    // publish conent here for offline use
        #define LEDLIB_FILE "_osx_/ledlib.cod"    // LED library file

        #define ENABLE_AUDIO                                        // enable the audio 
        #define ENABLE_MOTION                                       // enable motion
        #define ENABLE_WS2811                                       // enable ws2811 , only this supported on PFL and PFZ 
        #define ENABLE_SD                                           // enable storage (sd or extern flash , board PFL and PBZ specific)
        #define HW_NOMINAL_VOLUME 3000
        #define VOLUME HW_NOMINAL_VOLUME
        const unsigned int maxLedsPerStrip = 144;                   // max led per strip supported 


        #define PWM_CHANNELS   6   // number of PWM channels
        #define PIXEL_CHANNELS   2   // number of pixel channels
        static struct  {
            uint16_t audioFSR = VOLUME;
            uint16_t chargerCurrent = 0; // 0, 850 or 1000 [mA]
            uint8_t nBlades = 0;        // number of installed blades
            bool monochrome = true;     // assume monochrome, set true at install time
            uint32_t APOtime;           // auto power off time [ms]
        } installConfig;
        #undef VOLUME
        #define VOLUME installConfig.audioFSR

        // =============    END INSTALL CONFIGURATION  ==========================


        // =====================   USER PROFILE  ================================
        #define PRESETS_FILE "_osx_/presets.cod"    // Presets file
        #define PROFILE_FILE "_osx_/profile.cod"    // User profile file
            
        static struct {
            uint16_t masterVolume = 65535;              // active volume
            uint16_t masterBrightness = 65535;          // active brightness
            uint8_t preset = 0;
            uint16_t apID = 0;   // active presets ID (to know which entry to overwrite in COD)
            ClashSensitivity clashSensitivity;          // all sensitivities inherit from 'Sensitivity' clash and hold both the user setting (0...65535) and the derived working parameters 
            uint16_t combatVolume = 65535;             // master Volume when stealth = off
            uint16_t combatBrightness = 65535;         // master Brightness when stealth = off
            uint16_t stealthVolume = 10000;             // master Volume when stealth = on
            uint16_t stealthBrightness = 10000;         // master Brightness when stealth = off
            SwSensitivity swingSensitivity;
            StabSensitivity stabSensitivity;
            ShakeSensitivity shakeSensitivity;
            TapSensitivity tapSensitivity;
            TwistSensitivity twistSensitivity;
            MenuSensitivity menuSensitivity;
        } userProfile;
        #define CLASH_THRESHOLD_G userProfile.clashSensitivity.clashThreshold
            

        
        // ==================   END USER PROFILE  =============================== 
        #define OSX_SUBVERSION       "011"       // Fork version 
        
        // #define INSTALL_FILE "_osx_/testled.cod"    // installation file
        // #define LEDLIB_FILE "_osx_/ledlib.cod"    // LED library file

        #define PF_PROP_ULTRASABERS         // define  ultrasabers props 
        // #define PF_PROP_SABER
        #define ENABLE_DEVELOPER_MODE       // developer mode
        #define PF_STATUS_REPORT_ON         // status report ON , comment to off 
        #define PF_VOICE_MSG_DISABLED       // talkie : 0- VOICE MSG DISABLED
                                            //          1- VOICE MSG FEMALE 
                                            //          2- VOICE MSG MALE



        #define OSX_ENABLE_MTP              // enables proffieOSx serial transfer protcol   
            #ifdef OSX_ENABLE_MTP
                #define SERIAL_ASCII_BAUD 9600
                #define SERIAL_BIN_BAUD  921600   
            #endif              
        #ifdef ENABLE_DEVELOPER_MODE
            // #define ENABLE_DEVELOPER_COMMANDS       // ProffieOS developer commands
            #define DIAGNOSE_EVENTS            
            #define X_PROBECPU                      // Enable CPU probes (results reported at STDOUT under "top"). Adds 3k program memory
            //#define X_BROADCAST                      // Enable broadcasting of binary monitoring data. For debug only
            #define DIAGNOSE_POWER        
            #ifdef X_BROADCAST
                // #define OBSIDIANFORMAT  // reuse matlabs
                // #define BROADCAST_MOTION             // Broadcast acc & gyro
                // #define BROADCAST_THETA              // Broadcast data for twist navigation
                // #define BROADCAST_SHAKE             // Broadcast data for shake detection
                // #define BROADCAST_2TAP             // Broadcast data for double-tap detection
            #endif            
            // #define X_LIGHTTEST                       // Enable parsing LED test commands
        #endif

        #ifdef PF_STATUS_REPORT_ON
            #define ENABLE_DIAGNOSE_COMMANDS
            #define DIAGNOSE_SENSOR
            #define DIAGNOSE_STORAGE
            // #define DIAGNOSE_EVENTS
            #define DIAGNOSE_AUDIO
            // #define DIAGNOSE_POWER        
            #define DIAGNOSE_BOOT                      // Enable developer stream on serial port
        #endif
  #endif // ULTRAPROFFIE
#endif // CONFIG_TOP

#ifdef CONFIG_PRESETS
  #include <vector>
  #include "../styles/styles.h"

    // ProffieOSx presets: nothing to do here, use presets XML
    vector<Preset> presets;

    // ProffieOSx configuration: nothing to do here, use install XML.
    BladeConfig blades[] = {
    { 0, 
      DECLARE_NULL_BLADES,  
      &presets }  
    };    
#endif // CONFIG_PRESETS

#ifdef CONFIG_BUTTONS 
    Button PowerButton(BUTTON_POWER, powerButtonPin, "pow");
    // Button AuxButton(BUTTON_AUX, auxPin, "aux");  
#endif