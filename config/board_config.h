
#ifdef CONFIG_TOP


  /**********************************************************
   *             GENERIC TOP (ALL BOARDS)                   *
   **********************************************************/  

    // Arduino switch translations
    // ===========================
    #ifdef PF_DEVELOPER_MODE_ON 
        #define ENABLE_DEVELOPER_MODE 
    #elif defined(PF_DEVELOPER_MODE_OFF)
        #undef  ENABLE_DEVELOPER_MODE      
        // #define DISABLE_DIAGNOSTIC_COMMANDS     // disable diagnostic commands
    #endif

    #ifdef PF_STATUS_REPORT_ON
        #define ENABLE_DIAGNOSE_MODE        
    #elif defined(PF_STATUS_REPORT_OFF)  
        #undef ENABLE_DIAGNOSE_MODE      
    #endif

    // Generic firmware configuration 
    // ==============================    
    #define OSX_SUBVERSION       "016"       // Fork version 
    // #define PF_PROP_ULTRASABERS                   // define  ultrasabers props 
    // #define PF_PROP_SABER    
    #define OSX_ENABLE_MTP                  // enables proffieOSx serial transfer protcol   
    #ifdef OSX_ENABLE_MTP
        #define SERIAL_ASCII_BAUD 9600
        // #define SERIAL_ASCII_BAUD 115200
        // #define SERIAL_ASCII_BAUD 921600
        #define SERIAL_BIN_BAUD  921600   
    #endif       
    #define GYRO_MEASUREMENTS_PER_SECOND  1600    // default Measuremnts per second,  do not remove 
    #define ACCEL_MEASUREMENTS_PER_SECOND 1600    // default Measuremnts per second,  do not remove 
    const unsigned int maxLedsPerStrip = 144;                   // max led per strip supported 
    #define ENABLE_AUDIO                                        // enable the audio 
    #define ENABLE_MOTION                                       // enable motion
    #define ENABLE_WS2811                                       // enable ws2811 , only this supported on PFL and PFZ 
    #define ENABLE_SD                                           // enable storage (sd or extern flash , board PFL and PBZ specific)



  /**********************************************************
   *            SABERPROP Config Top                        *
   **********************************************************/  
  #ifdef SABERPROP    
     
    // Hardware configuration
    // =======================
    #if SABERPROP_VERSION == 'P' //  SaberProp
        #include "saberprop_esp_config.h"    
    #elif  SABERPROP_VERSION == 'Z' || SABERPROP_VERSION == 'L'  // UltraProffie Zero & Lite
        #include "ultraproffie_stm_config.h"
    #elif SABERPROP_VERSION == 'S'
        #include "saberprop_stm_config.h"
    #else
        #error UNKNOWN SABERPROP
    #endif 
   

    // Firmware configuration 
    // ======================
    #define NUM_BLADES 2
    #ifdef HW_NUM_BLADES 
        #define NUM_BLADES HW_NUM_BLADES
    #endif
    #define NUM_BUTTONS HW_NUM_BUTTONS
    #if SABERPROP_VERSION == 'P'    //  SaberProp
        #define HW_NOMINAL_VOLUME 2300  
    #else
        #define HW_NOMINAL_VOLUME 3000  // don't change this unless you change the hardware (0-3 V max dac output) !!!        
    #endif
    #define VOLUME HW_NOMINAL_VOLUME                            // redefined at runtime             

    // =====   Install (SaberProp)  =====
    #include <stdint.h>
    #include "../motion/sensitivities.h"
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

    // =====   Profile (SaberProp)  =====
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
                
    //  ==== Developer mode ( SaberProp) ====
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
		 #define DIAGNOSE_MENU
    #endif

    // =====   Diagnose mode ( SaberProp)  =====
    #ifdef ENABLE_DIAGNOSE_MODE
        #define ENABLE_DIAGNOSE_COMMANDS
        #define DIAGNOSE_SENSOR
        #define DIAGNOSE_STORAGE
        #define DIAGNOSE_AUDIO
        #define DIAGNOSE_BLADES
        #define DIAGNOSE_BOOT                      
        // #define DIAGNOSE_EVENTS
        #define DIAGNOSE_PRESETS
        // #define DIAGNOSE_PROP                
        // #define DIAGNOSE_POWER
        #define DIAGNOSE_BLE

 #endif

  
  
  /**********************************************************
   *             PROFFIE BOARDS Config Top                  *
   **********************************************************/  
  #else 
        
    //  Hardware configuration
    // =======================
    #if PROFFIEBOARD_VERSION - 0 == 2
        #include "proffieboard_v2_config.h"
    #elif PROFFIEBOARD_VERSION - 0 == 3
        #include "proffieboard_v3_config.h"
    #else
    #error UNKNOWN PROFFIEBOARD
    #endif

    // Firmware configuration 
    // ======================
    #define NUM_BLADES 2
    #ifdef HW_NUM_BLADES 
        #define NUM_BLADES HW_NUM_BLADES
    #endif
    #define NUM_BUTTONS 2
    #define PWM_CHANNELS   6   // number of PWM channels
    #define PIXEL_CHANNELS   2   // number of pixel channels
          

    // =====   Install (ProffieBoard)   =====
    #include <stdint.h>
    #include "../motion/sensitivities.h"
    #define INSTALL_FILE "_osx_/install.cod"    // installation file
    #define OFFLINE_FILE "_osx_/offline.txt"    // publish conent here for offline use
    #define LEDLIB_FILE "_osx_/ledlib.cod"    // LED library file

    #define HW_NOMINAL_VOLUME 3000
    #define VOLUME HW_NOMINAL_VOLUME

    static struct  {
        uint16_t audioFSR = VOLUME;
        uint16_t chargerCurrent = 0; // 0, 850 or 1000 [mA]
        uint8_t nBlades = 0;        // number of installed blades
        bool monochrome = true;     // assume monochrome, set true at install time
        uint32_t APOtime;           // auto power off time [ms]
    } installConfig;
    #undef VOLUME
    #define VOLUME installConfig.audioFSR

    // =====   Profile  (ProffieBoard)   =====
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
            
    // =====  Developer mode (ProffieBoard)   =====         
    #define ENABLE_DEVELOPER_MODE       // developer mode
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

    // =====   Diagnose mode (ProffieBoard)   =====
    #define ENABLE_DIAGNOSE_MODE         // status report ON , comment to off         
    #ifdef ENABLE_DIAGNOSE_MODE
        #define ENABLE_DIAGNOSE_COMMANDS
        #define DIAGNOSE_SENSOR
        #define DIAGNOSE_STORAGE
        // #define DIAGNOSE_EVENTS
        #define DIAGNOSE_AUDIO
        // #define DIAGNOSE_POWER        
        #define DIAGNOSE_BOOT                     
    #endif
  #endif // SABERPROP
#endif // CONFIG_TOP


/**********************************************************
 *             CONFIG PRESETS (all boards)                 *
 **********************************************************/  
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


/**********************************************************
 *             CONFIG BUTTONS (all boards)                 *
 **********************************************************/  
#ifdef CONFIG_BUTTONS 
    Button PowerButton(BUTTON_POWER, powerButtonPin, "pow");
    // Button AuxButton(BUTTON_AUX, auxPin, "aux");  
#endif