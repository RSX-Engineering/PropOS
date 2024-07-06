// This config file contains all the things should be the same for all
// proffie lite  proffiboard configurations. In most cases there should be no need
// to change anything in this file.
#ifndef ULTRAESP_CONFIG_HW
#define ULTRAESP_CONFIG_HW

    #define NO_BATTERY_MONITOR


    #define INVALID_PIN 50  // it is the pin specially created to g_APinDescription to indicate a invalid , no use // as 29
    // Proffieboard pin map
    enum SaberPins {
            // I2S
        bclkPin = 5,             // NOT USED ON HW TODO // BCLK (digital audio)   PB13  was = 3
        txd0Pin = 4,             // NOT USED ON HW TODO // TXD0 (digital audio)   PA10  was = 26
        lrclkPin = 6,            // NOT USED ON HW TODO // LRCLK (digital audio)  PB12 was = 2

        // I2C
        i2cDataPin = 2,                   // I2C bus, Used by motion sensors  PB9 - I2C1_SDA
        i2cClockPin = 1,                  // I2C bus, Used by motion sensors  PB8 - I2C1_SCL

        // Buttons
        powerButtonPin = 14,               // power button  PB6
        auxPin = 15,                       // AUX button    PB5
        aux2Pin = INVALID_PIN,             // AUX2 button   PB4

        // Memory card
        sdCardSelectPin = INVALID_PIN,     // PB14

        amplifierPin = 46, //21,                  // Amplifier enable pin PB4
        boosterPin = 46, //13,                    // Booster enable pin   PH0        INVALID_PIN
        motionSensorInterruptPin = -1,      // motion sensor interrupt PC13

        // No fastled support yet
        spiLedSelect = INVALID_PIN,         
        spiLedDataOut =INVALID_PIN,
        spiLedClock = INVALID_PIN,

        // Neopixel pins
        bladePin = INVALID_PIN,                  // blade control, either WS2811 or PWM PA0
        bladeIdentifyPin = -1,          // blade identify input / FoC
        blade2Pin = INVALID_PIN,                 // PB8
        blade3Pin = INVALID_PIN,        // PB3
        blade4Pin = INVALID_PIN,        // PA4
        blade5Pin = INVALID_PIN,        // PA15 (also UART)
        blade6Pin = INVALID_PIN,        // PA02 (also UART)

        // Blade power control
        bladePowerPin1 = 7,             // blade power control PA1
        bladePowerPin2 = 8,// 17,            // blade power control PB8
        bladePowerPin3 = 9,//23,            // blade power control PA8
        bladePowerPin4 = 10,//24,            // blade power control PB3
        bladePowerPin5 = INVALID_PIN,   // blade power control PB15
        bladePowerPin6 = INVALID_PIN,   // blade power control PB0

        // If there is no neopixels, these can be used as PWM output
        // If neopixels are present, but not on these pins, PWM may still
        // be possible at 800kHz.
        // You can drive 20mA LEDs with a resistor from these pins, or
        // hook up an external FET to drive more powerful LEDs
        bladePowerPin7 = INVALID_PIN,   // PB03 (also neopix4)
        bladePowerPin8 = INVALID_PIN,   // PB10 (also neopix2)
        bladePowerPin9 = INVALID_PIN,   // PB11 (also neopix3)
        bladePowerPin10 = INVALID_PIN,  // PA00 (also blade ID / neopixels)

        // Analog pins
        batteryLevelPin = 13,           // battery level input PA04 was 1
        batteryRefPin = INVALID_PIN,             // battery level input PA04 was 1
        // UART
        rxPin = INVALID_PIN,                      // PA15
        txPin = INVALID_PIN,                      // PA02

        statusLEDPin = INVALID_PIN,      // no fw status led 
        chargeDetectPin = INVALID_PIN,            // charge detect 
        chargeEnable =  INVALID_PIN,              // charge enable
        // MiCOM setup
        trigger1Pin = INVALID_PIN,      // power button
        trigger2Pin = INVALID_PIN,      // aux button
        trigger3Pin = INVALID_PIN,      // aux2 button
        trigger4Pin = INVALID_PIN,      // data2
        trigger5Pin = INVALID_PIN,      // data3
        trigger6Pin = INVALID_PIN,      // data4
        trigger7Pin = INVALID_PIN,      // RX
        trigger8Pin = INVALID_PIN,      // TX
    };

    // -------------- Define Tester ----------------------------------
        // Helper DEFINES to CHECK the CONFIGURATION    // do not erase 
    #define XSTR(x) STR(x)          
    #define STR(x) #x
    #define MQUOATE '

    // Concatenate preprocessor tokens Q0, .., Q1 without expanding macro definitions
    // (however, if invoked from a macro, macro arguments are expanded).
    #define HWID_CONCAT_NX(Q0, L1, L2, L3, L4, Q1) Q0 ## L1 ## L2 ## L3 ## L4 ## Q1
    // Concatenate preprocessor tokens A and B after macro-expanding them.
    #define HWID_CONCAT(Q0, L1, L2, L3, L4, Q1) HWID_CONCAT_NX(Q0, L1, L2, L3, L4, Q1)
    #define HWL_CONCAT_NX(q1, l2, q3) q1 ## l2 ## q3
    #define HWL_CONCAT(q1, l2, q3) HWL_CONCAT_NX(q1, l2, q3)

    #define FORMED_HWVER HWID_CONCAT(MQUOATE, HW_PREFIX, HW_LETTER1, HW_LETTER2, HW_LETTER3, MQUOATE)

    #if HWVER == FORMED_HWVER

    #else
        #error HW VERSION ERROR
    #endif

    #if HWL_CONCAT(MQUOATE, HW_LETTER3, MQUOATE) == 'L'
        #define GYRO_CLASS LSM6DS3H     // gyro class 
    #else
        #error error SNS DEFINE NOT FOUND
    #endif

    #if HW_LETTER4 == 2
        #define PWM_CHANNELS   2   // number of PWM channels
        #define PIXEL_CHANNELS   1   // number of pixel channels
    #elif HW_LETTER4 == 4
        #define PWM_CHANNELS   4   // number of PWM channels
        #define PIXEL_CHANNELS   2   // number of pixel channels

    #else
        #error error PWM CH NOt OKAY
    #endif

    #if HW_LETTER5 == 1
        #define ULTRAPROFFIE_BOOSTER 
    #elif HW_LETTER5 == 0
        
    #else
        #error error BOOSTER define not found
    #endif

    #if HW_LETTER1 == 2
        #define ULTRAPROFFIE_DAC                 
    #elif HW_LETTER1 == 3
        #define ULTRAPROFFIE_DAC               
    #else
        #error error AMP NOT FOUND 
    #endif

    #if HW_LETTER2 == 1
        #define ULTRAPROFFIE_CHARGER
    #elif HW_LETTER2 == 0
        // #define PF_CHARGER_EQ   0
    #else
        #error error NO CHARGER DEFINE
    #endif

#endif
// -------------- Define Tester ---------------------------------- 
