#ifndef CODIDS_H
#define CODIDS_H

/* COD IDs
    0 = null
    1 ... 999 = local IDs
    1000 ... 1999 = Analog LED emitter drivers
    2000 ... 2999 = Color renderers & Transfer functions
    3000 ... 3999 = Analog LED channels
    4000 ... 4999 = Analog LEDs
    5000 ... 5999 = Digital blade drivers
    6000 ... 9999 = Presets


*/

#define HANDLER_xUpdate 1

// Handlers
#define  HANDLER_BladeInstaller                10      // Blade install 
#define  HANDLER_Installer                     11      // Install settings
#define  HANDLER_Profile                       12      // User profile
#define  HANDLER_Preset                        13      // Preset
#define  HANDLER_PresetList                    14      // List of active presets



#define  HANDLER_Interpolator                  99      // Any interpolator reference data
#define  HANDLER_AnalogLED_Driver_Legacy       100     // Legacy driver
#define  HANDLER_AnalogLED_Driver_pPWL         101     // pPWL driver

#define  HANDLER_ColorRenderer_HSLe            110     // HSL enhancer
#define  HANDLER_ColorRenderer_CRM             111     // Color rendering matrix
#define  HANDLER_AnalogLED_Channel             112     // Single/multi-emitter LED Channel
#define  HANDLER_AnalogLED                     113     // Single/multi-channel LED Module

#define  HANDLER_PixelDriver                    120     // Pixel driver configuration


#endif // CODIDS_H





