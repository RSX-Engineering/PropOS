/********************************************************************
 *  ANALOG LED                                                  *
 *  (C) Marius RANGU @ RSX Engineering                *
 *  fileversion: v1.2 @ 2022/07;    license: GPL3                   *
 ********************************************************************
 
 ********************************************************************/
 



#ifndef XANALOGLED_H
#define XANALOGLED_H

#include "pwm_pin.h"
#include "../common/command_parser.h"
#include "../common/CodReader.h"
#include "../common/Interpolator.h"
#include <vector>    
#include <string.h>


#define ALED_MAXCH      4   // Maximum number of channels an analog LED suppports
#define ALED_MAXEM      3   // Maximum number of emitters an analog LED channel supports



#ifdef ARDUINO_ARCH_STM32L4   // STM architecture
namespace std {
  void __throw_length_error(char const*) { while(1); }
  void __throw_bad_alloc() { while(1); }
  void __throw_logic_error(char const*) { while(1); }
}   // fixing Arduino junk...
#endif

// TODO: treat new and vector::resize exceptions, so we can recover from such errors.
using namespace std;


// oPWL driver parameters
#define pPWL_NR_MAX     8   // Maximum number of control references (segments)
#define pPWL_NVP_MAX    7   // Maximum number of voltage points (normally there are 7: 3.0, 3.2, 3.4, 3.6, 3.8, 4.0 and 4.2)
#define pPWL_VBAT_MIN   3   // Minimum battery voltage used for optical calibration. Don't change this unless you're me...
#define pPWL_VBAT_MAX  4.2  // Maximum battery voltage used for optical calibration. Don't change this unless you're me...


// -----------------------------------------------------------
// ANALOG LED DRIVERS: 
// Base class for all drivers 
class AnalogLED_DriverInterface  {
friend class xAnalogLED_Channel;
protected:
    uint16_t brightness;                // Latest brightness for which the register value was calculated
#ifdef X_LIGHTTEST
    uint16_t regVal;                    // Latest register value - only needed for remote light testing
#endif
public:
    virtual bool Init(void* data) { return false; } // initialize from internal memory
    virtual bool Init(const char* filename, uint16_t ID) { return false; } // initialize from external memory
    virtual uint16_t Get_regVal(uint16_t bright) = 0; // Calculates the register value (0-32767) based on desired brightness (0-65535) and battery voltage
};


// Direct drive: use only if the LED can withstand the full battery voltage or an external PWM-driven current regulator is used
class AnalogLED_Driver_Direct : public AnalogLED_DriverInterface {
private:
    float scale;            // register value = scale * brightness
public:
    // Init driver. Data = scale (float*)
    bool Init(void* data) override { 
        if (data)
            scale = *((float*)data);
        else
            scale = 0.5;      // default scale if no data provided
        if (!scale) return false;
        brightness = 0;      
        // STDOUT.print("[AnalogLED_Driver_Direct.Init] Initialized direct driver with scale "); STDOUT.println(scale); 
        return true;
    }

    // Calculate register value
    uint16_t Get_regVal(uint16_t bright) override {
        float retval = scale * (float)bright;
        if (retval>32767) retval=32767;
        brightness = bright;    // store latest brightness
        #ifdef X_LIGHTTEST 
            regVal = retval;    // store register value to report back 
        #endif
        return (uint16_t)retval;
    } 
};



// Legacy LED specifications. 
// Legacy Proffie driver uses a voltage-dependent PWM multiplier calculated eventually as 'a / (BatteryVoltage + b)' 
// so we're just gonna store 'a' and 'b' instead of re-calculating them each time from basic specs
struct LegacyLED {
public:
    float a;    // a = MaxAmps * ( ledRes + R )
    float b;    // b = ledRes * MaxAmps - MaxVolts
    uint8_t Red;    // TODO: replace RGB with hue
    uint8_t Green;
    uint8_t Blue;
};

// Template function to get the specs from a LED structure defined in led.h
// (Those LED structures should have been objects, not types!)
template <class LED>
void GetLegacyLEDSpecs(LegacyLED* dest) {
    dest->a = 0; dest->b = 0;           // assume failure
    if (LED::MaxAmps - LED::P2Amps == 0) return;        // that would result in division by zero
    float ledRes = (LED::MaxVolts - LED::P2Volts) / (LED::MaxAmps - LED::P2Amps);   // slope of the LED's voltage-current curve = LED's internal resistance (it's actually not a constant)
    dest->a = LED::MaxAmps * (ledRes + LED::R);
    dest->b = ledRes * LED::MaxAmps - LED::MaxVolts;
    dest->Red = LED::Red;
    dest->Green = LED::Green;
    dest->Blue = LED::Blue;
    // STDOUT.print("Get Legacy LED specs for LED "); STDOUT.print(LED::Red); STDOUT.print(", "); STDOUT.print(LED::Green); STDOUT.print(", "); STDOUT.println(LED::Blue); 
}

// Proffie legacy driver: linear voltage derating based on analytical electrical model
class AnalogLED_Driver_Legacy : public AnalogLED_DriverInterface {
private:
    LegacyLED ledSpecs;        // Legacy LED specifications
public:
    // Initialization from internal memory:
    /* To initialize from a LED structure as defined in leds.h, use the following template:
         LegacyLED specs;    // create a specs object
         GetLegacyLEDSpecs<CreeXPE2WhiteTemplate<550>>(&specs);   // copy specs from legacy LED structure to object
         legacyDriver.Init((uint8_t*)&specs);  // initialize with specs object (no longer needed)
    */
    bool Init(void* data) override {
        ledSpecs = *((LegacyLED*)data);
        if (!ledSpecs.a && !ledSpecs.b) return false;           // LegacyLED not initialized
        // STDOUT.print("Initialized Legacy LED driver with a="); STDOUT.print(ledSpecs.a); STDOUT.print(" and b="); STDOUT.println(ledSpecs.b);
        brightness = 0;      
        return true;
     } 

    // Initialization from external memory: 
    bool Init(const char* filename, uint16_t ID) override { 

        // 1. Read a structure from the COD file
        CodReader reader;
        uint32_t numBytes;
        struct {
            // this structure must have EXACTLY the same fields as the COD entry with handler 'HANDLER_AnalogLED_Driver_Legacy', because it is copied byte-by-byte
            float MaxAmps, MaxVolts, P2Amps, P2Volts, R;
            uint8_t Red, Green, Blue;
        } __attribute__((packed)) datStruct;
        if (!reader.Open(filename)) return false;          // file not found
        // STDOUT.print("[AnalogLED_Driver_Legacy::Init] Open file "); STDOUT.println(filename);
        numBytes = reader.ReadEntry(ID, (void*)&datStruct, sizeof(datStruct));  // Attempt to read a structure with the requested ID. We can safely do this without checking the properties first,
                                                                                // because even if the reader puts rubbish at destination, it won't exceed the specified size.
        reader.Close();
        // STDOUT.print("[AnalogLED_Driver_Legacy::Init] Read entry type "); STDOUT.print(reader.entryType); STDOUT.print(", handler is ");  STDOUT.print(reader.codProperties.structure.Handler);
        // STDOUT.print(", size is "); STDOUT.println(numBytes);
        if (!numBytes) return false;            // ID not found
        if (reader.entryType != COD_ENTYPE_STRUCT) return false;    // found ID but it's not a structure
        if (reader.codProperties.structure.Handler != HANDLER_AnalogLED_Driver_Legacy) return false;   // found structure with the right ID but wrong handler
        if (numBytes > sizeof(datStruct)) return false;    // struct size larger than expected
        if (datStruct.MaxAmps == datStruct.P2Amps) return false;    // that is certainly wrong data, it would result in division by 0

        // 2. Store data read from COD in RAM, under ledSpecs
        float ledRes = (datStruct.MaxVolts - datStruct.P2Volts) / (datStruct.MaxAmps - datStruct.P2Amps);
        ledSpecs.a = datStruct.MaxAmps * (ledRes + datStruct.R);
        ledSpecs.b = ledRes * datStruct.MaxAmps - datStruct.MaxVolts;
        ledSpecs.Red = datStruct.Red;
        ledSpecs.Green = datStruct.Green;
        ledSpecs.Blue = datStruct.Blue;

        // STDOUT.print("[AnalogLED_Driver_Legacy::Init] initialized a "); STDOUT.print(ledSpecs.Red); STDOUT.print("-"); STDOUT.print(ledSpecs.Green); STDOUT.print("-"); STDOUT.print(ledSpecs.Blue);
        // STDOUT.print(" LED with a ="); STDOUT.print(ledSpecs.a); STDOUT.print(" and b = "); STDOUT.println(ledSpecs.b);
        brightness = 0;      
        return true;
    }

    uint16_t Get_regVal(uint16_t bright) override {
        float batVolt = battery_monitor.battery();
        float tmp = ledSpecs.a / (batVolt + ledSpecs.b);     // multiplier
        tmp *= bright;                                      // register value
        if (tmp>32767) tmp=32767;
        brightness = bright;    // store latest brightness
        #ifdef X_LIGHTTEST
            regVal = tmp;    // store register value to report back 
        #endif
        return (uint16_t) tmp;
    }     
};

// Photometric piecewise linear driver
class AnalogLED_Driver_pPWL : public AnalogLED_DriverInterface {
private:
    uint16_t refVoltage;                // Voltage at which the references were calculated last time
    uint8_t NR;                         // Number of references = number of control segments
    uint8_t NVP;                        // Number of voltage points
    vector<uint16_t> refData;           // References data. This is a resizeable vector, containing all the LUT values row-by-row
    vector<LUT<uint16_t, uint16_t>> lutRef; // Lookup tables to calculate references = f ( voltage )
    vector<uint16_t> ctrlData;          // Control data. This vector stores the references and provides data for the control LUT
    LUT<uint16_t, uint16_t> lutCtrl;    // Control lookup table - used to calculate register value = f ( brightness )

public:

     // Initialize from external memory
    bool Init(const char* filename, uint16_t ID) override __attribute__((optimize("Og"))) { 
        CodReader reader;
        uint32_t nrOfBytes = 0;
        
        // 1. Look for data in the COD file
        // STDOUT.print("[AnalogLED_Driver_pPWL.Init] Installing ID "); STDOUT.println(ID);
        if (!reader.Open(filename)) return false;     // Open COD file
        if(reader.FindEntry(ID) != COD_ENTYPE_TABLE) { reader.Close(); return false; }    // Look for ID and fill properties if a table was found
        if (reader.codProperties.table.Handler != HANDLER_AnalogLED_Driver_pPWL) { reader.Close(); return false; } // Check if table has the correct handler
        if (reader.codProperties.table.Rows < 1 || reader.codProperties.table.Rows > pPWL_NR_MAX || 
            reader.codProperties.table.Columns < 1 ||  reader.codProperties.table.Columns > pPWL_NVP_MAX)  { reader.Close(); return false; } // Check if table has the correct size
        if (reader.codProperties.table.DataType != CodDataType::xCod_Uint16 ) { reader.Close(); return false; } // Check if data is represented as uint16
                                                                                         
        // 2. Copy data from file
        refData.resize(reader.codProperties.table.Rows*reader.codProperties.table.Columns);  // resize vector to store all data, row-by-row
        // STDOUT.print("[AnalogLED_Driver_pPWL::Init] Resized refData to "); STDOUT.print(refData.size()); STDOUT.println(" values.");
        nrOfBytes = reader.ReadEntry(ID, (void*)refData.data(), 2*reader.codProperties.table.Rows*reader.codProperties.table.Columns);   // Read table and copy data to vector
        // STDOUT.print("[AnalogLED_Driver_pPWL::Init] Read "); STDOUT.print(nrOfBytes); STDOUT.print(" bytes from "); STDOUT.print(filename); STDOUT.println(".");
        reader.Close();  
        if (!nrOfBytes) return false;        // could not read data...   
        NR = reader.codProperties.table.Rows;           // Update number of references
        NVP = reader.codProperties.table.Columns;       // Update number of voltage points

        // STDOUT.print("[AnalogLED_Driver_pPWL::Init] Initialized with: ");
        // for (uint8_t row=0; row<NR; row++) {
        //     STDOUT.print("Row "); STDOUT.print(row); STDOUT.print(" data:");
        //     for (uint8_t col=0; col<NVP; col++) { STDOUT.print(" "); STDOUT.print(refData[row*NVP+col]); }
        //     STDOUT.println("");
        // }

        // 3. Initialize lookup tables
        lutRef.resize(NR);
        ctrlData.resize(NR+1);  // Control LUT has one extra point not carried by the COD file: the origin. The (0,0) point is added manually by UpdateReferences()
        uint16_t vBatMin = pPWL_VBAT_MIN / battery_monitor.voltageLSB();  // For LED driving, we're monitoring battery voltage as a low-resolution integer, to prevent excessive recalculations
        uint16_t vBatMax = pPWL_VBAT_MAX / battery_monitor.voltageLSB() ; //        
        bool success = true;
        for (uint8_t ref=0; ref<NR; ref++) {
            if (!lutRef[ref].Init(refData.data()+ref*NVP, NVP)) success=false;   // Initialize each reference LUT with one row of data
            lutRef[ref].Start(vBatMin, vBatMax);                                 // Start each reference LUT and initialize voltage range
        }
        if (!UpdateReferences(3.7 / battery_monitor.voltageLSB())) success=false;                 // Calculate references and initialize control LUT at 3.7V (mainly to check data is valid)
        if (!success) {
            // failed somewhere, clear all data
            refData.clear();
            lutRef.clear();
            ctrlData.clear();
            return false;
        }
        brightness = 0;      
        return true;
    }

    // Calculate control references = f(battery voltage)
    bool UpdateReferences(uint16_t iVoltage) {
        ctrlData[0] = 0;    // add origin  manually
        for (uint8_t i=0; i<NR; i++) {
            ctrlData[i+1] = lutRef[i].Get(iVoltage);              // Calculate references for current voltage
        }
        // STDOUT.print("[AnalogLED_Driver_pPWL.UpdateReferences] Calculated for new iVoltage "); STDOUT.print(iVoltage); STDOUT.print(" [V] on emitter "); STDOUT.println((uint32_t)this);
        // for (uint8_t i=0; i<NR; i++) {
            // STDOUT.print(" "); STDOUT.print(ctrlData[i+1]);
        // }
        // STDOUT.println("");
        if (!lutCtrl.Init(ctrlData.data(), NR+1)) return false;   // Reinitialize control LUT with new references
        lutCtrl.Start(0, 65535);                                // Start control LUT, with brightness range [0-65535]
        refVoltage = iVoltage;                                  // store last voltage used to calculate references
        return true;
    }

    // Calculate register value = f(brightness)
    // uint16_t Get_regVal(uint16_t bright, float batVolt) override {
    uint16_t Get_regVal(uint16_t bright) override {
        uint16_t iVoltage = battery_monitor.iBattery();
        if (iVoltage != refVoltage) UpdateReferences(iVoltage);         // Recalculate references only if voltage changed with at least battery monitor's LSB
        uint16_t retval = lutCtrl.Get(bright);
        if (retval>32767) retval=32767;                     // clamp to range
        brightness = bright;    // store latest brightness
        #ifdef X_LIGHTTEST
            regVal = retval;    // store register value to report back 
        #endif
        return retval;
    } 


};



// -----------------------------------------------------------
// COLOR RENDERER
// Get 8-bit value from hexRGB: CH: 0=Red, 1=Green, 2=Blue, anything else = rubbish
#define HEXRGB_GET8b(uint32_RGB, CH) *((((uint8_t*)&uint32_RGB)+2-CH))

// Renderer interface
class ColorRenderer_Interface {
friend class xAnalogLED_Channel;
protected:
    // uint8_t nEm;                     // Number of emitters
    uint32_t ctrlVal;                   // Current control value [ 0x00RRGGBB aka hexRGB ]
    uint16_t brightness[ALED_MAXEM];    // Current brightness for each emitter
public:
    virtual bool Init(void* data, float channelBrightness) { return false; };        // Initialize from internal memory
    virtual bool Init(const char* filename, uint16_t ID, float channelBrightness) { return false; };     // Initialize from external memory
    virtual bool UpdateBrightness(uint32_t ctrlval) = 0;    // Calculates brightness [0-65535] based on control value (hexRGB), for all channels 
    virtual bool AppplyGamma(const char* filename, uint16_t* IDs)  { return true; } // Assign a transfer function for gamma correction to each emitter


protected:
    // uint16_t IntegerScaleFromFloatGain(void* data, float channelBrightness) {    // gets the integer needed to rescale [0,255] -> [0,65535] when data = gain as float. Allowed range is [0,10] with 0.003891 resolution
    //     if (!data) return 257;          // default gain = 1 
    //     float gain = *((float*)data);
    //     gain *= channelBrightness;
    //     if (gain<0) return 0;
    //     if (gain>10) return 2570;         // saturate gain to 10
    //     return gain * 257;  
    // }

    uint16_t IntegerScaleFromFloatGain(float* data, float channelBrightness) __attribute__((optimize("Og"))) {    // gets the integer needed to rescale [0,255] -> [0,65535] when data = gain as float. Allowed range is [0,10] with 0.003891 resolution
    if (!data) return 257;          // default gain = 1 
    float gain;
    // gain = *data;
    gain = 1;
    gain *= (*data);
    gain *= channelBrightness;
    if (gain<0) return 0;
    if (gain>10) return 2570;         // saturate gain to 10
    return gain * 257;  
}
};

// Scales and drives directly: R -> out1, G -> out2, B -> out3
class ColorRenderer_Direct : public ColorRenderer_Interface {
private:
    uint16_t scale;     // scale = 257  => 255 scales to 65535

public:
    ColorRenderer_Direct() { scale = 0; }
    // Initialize from internal memory. Data = gain as float* = (0, 10) 
    // bool Init(uint8_t nCh_, void* data) { 
        // if (!nCh_ || nCh_>ALED_MAXEM) return false;     // wrong number of channels       
        // nEm = nCh_;
    bool Init(void* data, float channelBrightness) { 
        // brightness.resize(1);   // Direct renderer has a single output
        scale = IntegerScaleFromFloatGain((float*)data, channelBrightness);
        ctrlVal = 0;
        // STDOUT.print("[ColorRenderer_Direct.Init] Initialized with scale "); STDOUT.println(scale); 
        return true; 
    }     

    // Calculates brightness (0-65535) based on control value (hexRGB) 
    bool UpdateBrightness(uint32_t ctrlval) {
        uint32_t iBrightness;        
        if (!scale) return false;   // renderer not initialized
        ctrlVal = ctrlval;
        for (uint8_t i=0; i<3; i++) {   // Input comes as tristimulus so a direct renderer must have 3 outputs
            iBrightness = scale * HEXRGB_GET8b(ctrlVal, i);    // this might exeed 65535 because scale can be more than 1
            if (iBrightness > 65535) iBrightness = 65535;
            brightness[i] = iBrightness;
        }
        return true;
    }
};



// // Single-output renderer controlled by hue, saturation or lightness and an enhancement curve
class ColorRenderer_HSLe : public ColorRenderer_Interface {
private:
    uint16_t scale;         // scale = 257  -> 255 scales to 65535
    char type;              // use 'h', 's' or 'l' to specify which parameter to respond to
    uint16_t tR, tG, tB;    // target R, G, B to enhance (only used by H enhacer)
    TF<uint8_t, uint16_t>* enhancer;    // pointer to enhancer's tranfer function (specialization that gets 256*output)
    vector<uint8_t> enhRef;             // reference points for enhancer's transfer function
    #ifdef ARDUINO_ARCH_STM32L4   // STM architecture
    inline static uint16_t Cmin, Cmax;    // min and max of last {R, G, B}, for S and L calculations. Those are static so can reuse Cmin and Cmax across successive renderers
    inline static uint32_t last_ctrlVal;  // ... to know when to recalculate Cmin and Cmax
    #else
    static uint16_t Cmin, Cmax;    // min and max of last {R, G, B}, for S and L calculations. Those are static so can reuse Cmin and Cmax across successive renderers
    static uint32_t last_ctrlVal;  // ... to know when to recalculate Cmin and Cmax
    #endif
public:
    ColorRenderer_HSLe() { enhancer = 0; scale = 0; }
    ~ColorRenderer_HSLe() { if (enhancer) delete enhancer; }

    // Initialize from internal memory (proportional only). 
    // Data = { 0 or gain as float* = [0, 10]; type as char } for 's' and 'l'
    // Data = { 0 or gain as float* = [0, 10]; type as char, target R, targetG, targetB as uint8 } for 'h' 
    // TODO: extend to TF from internal memory
    bool Init(void* data, float channelBrightness) { 
        type = *(((char*)data)+4);                                  
        if (type!='h' && type!='s' && type!='l') return false;      // unspecified type
        scale = IntegerScaleFromFloatGain((float*)data, channelBrightness);      // first 4 bytes
        if (enhancer) delete enhancer;    // delete transfer function, if exists
        enhRef.resize(0);                 // no need to keep transfer function references for proportional engancer 
        enhancer = 0;
        if (type=='h') {
            tR = *(((char*)data)+5); 
            tG = *(((char*)data)+6); 
            tB = *(((char*)data)+7); 
            uint16_t sum = tR + tG + tB;
            tR = (tR<<8) / sum;     // store target RGB as scaled weights
            tG = (tG<<8) / sum;
            tB = (tB<<8) / sum;
        }
        // last_ctrlVal = 0;
        for (uint8_t i=0; i<ALED_MAXEM; i++)
            brightness[i] = 0;
        ctrlVal = 0;
        // STDOUT.print("[ColorRenderer_HSLe.Init] Initialized '"); STDOUT.print(type); STDOUT.print("' renderer with ");  STDOUT.print(nEm); STDOUT.println(" emitters");
        return true; 
    }


    // Initialize from external memory
    bool Init(const char* filename, uint16_t ID, float channelBrightness)  __attribute__((optimize("Og"))) { 
        CodReader reader;
        uint32_t numBytes;
        struct {
            char type;          // 'h', 's' or 'l'
            float gain;         // [0.003891, 10]
            uint8_t tR;         // target Red (for H enhancer only) 
            uint8_t tG;         // target Green (for H enhancer only) 
            uint8_t tB;         // target Blue (for H enhancer only)
            uint16_t tfID;      // transfer function ID (0 for proportional enhancer)
        } __attribute__((packed)) codData;
        // brightness.resize(0);   // assume failure
        if (enhancer) delete enhancer;    // delete transfer function, if already exists
        enhancer = 0;
        if (!reader.Open(filename)) return false;          // file not found
        numBytes = reader.ReadEntry(ID, (void*)&codData, sizeof(codData));      // Attempt to read a structure with the requested ID. We can safely do this without checking the properties first,
        reader.Close();                                                         // because even if the reader puts rubbish at destination, it won't exceed the specified size.
        if (!numBytes) return false;                                            // Could not read entry
        if (reader.entryType != COD_ENTYPE_STRUCT) return false;                // Found ID but it's not a struct   
        if (reader.codProperties.structure.Handler != HANDLER_ColorRenderer_HSLe) return false;    // Wrong handler
        // if (!codData.nEm || codData.nEm>ALED_MAXEM) return false;                 // Wrong number of channels   
        if (codData.type!='h' && codData.type!='s' && codData.type!='l') return false;      // unknown type
        type = codData.type;
        scale = IntegerScaleFromFloatGain(&codData.gain, channelBrightness);
        if (type=='h') {
            // calculate weights for hue enhancer
            uint16_t sum = codData.tR + codData.tG + codData.tB;
            tR = (codData.tR<<8) / sum;     // store target RGB as scaled weights
            tG = (codData.tG<<8) / sum;
            tB = (codData.tB<<8) / sum;
        }       
        if(codData.tfID) {
            // Use transfer function 
            reader.Open(filename);      // reopen the file, transfer function should also be found here
            int8_t tmp = reader.FindEntry(codData.tfID);
            reader.Close();
            if (tmp != COD_ENTYPE_TABLE) return false;          // could not find table with the specified ID          
            enhancer = new TF<uint8_t, uint16_t>;                // create transfer function for enhancer
            enhRef.resize(2*reader.codProperties.table.Rows);   // resize vector to keep 2*refSize reference values = refSize reference points
            if (!enhancer->Init(filename, codData.tfID, enhRef.data(), 2*reader.codProperties.table.Columns)) {  // Init transfer function from file, store reference points in enhRef vector
                // Failed to initialize transfer function from file.
                delete enhancer;
                enhancer = 0;
                enhRef.resize(0);
                enhRef.shrink_to_fit();
                return false;   
            }
            enhancer->Start();
        }
        // nEm = 1; 
        last_ctrlVal = 0;
        // brightness.resize(nEm);
        for (uint8_t i=0; i<ALED_MAXEM; i++)
            brightness[i] = 0;
        ctrlVal = 0;
        // STDOUT.print("[ColorRenderer_HSLe.Init] Initialized '"); STDOUT.print(type); STDOUT.println("' renderer");
        return true; 
    }

    // Calculates brightness (0-65535) based on control value (hexRGB) 
    bool UpdateBrightness(uint32_t ctrlVal_)  { // __attribute__((optimize("Og"))) {
        if (!scale) return false;   // renderer not initialized
        ctrlVal = ctrlVal_;
        if (ctrlVal != last_ctrlVal) {
            // ctrlVal changed, need to recalculate Cmin and Cmax, as following HSL enhancers on different channels might also need it
            // uint8_t R = HEXRGB_GET8b(ctrlVal_, 0);
            // uint8_t G = HEXRGB_GET8b(ctrlVal_, 1);
            // uint8_t B = HEXRGB_GET8b(ctrlVal_, 2); 
            Cmax = std::max<uint8_t>(HEXRGB_GET8b(ctrlVal_, 0), std::max<uint8_t>(HEXRGB_GET8b(ctrlVal_, 1), HEXRGB_GET8b(ctrlVal_, 2)));   // max {R, G, B}
            Cmin = std::min<uint8_t>(HEXRGB_GET8b(ctrlVal_, 0), std::min<uint8_t>(HEXRGB_GET8b(ctrlVal_, 1), HEXRGB_GET8b(ctrlVal_, 2)));   // min {R, G, B}
            last_ctrlVal =  ctrlVal;
        }
        if (!Cmax) {    // Render 0 if all inputs are 0, regardless of type. Also prevent division by 0
            for (uint8_t i=0; i<ALED_MAXEM; i++) 
                    brightness[i] = 0;
            return true;
        }
        uint32_t hsv;       // hue, saturation or value (all non-standard)
        switch(type) {
            case 'h': { // Hue Enhancer
                uint8_t R = HEXRGB_GET8b(ctrlVal, 0);
                uint8_t G = HEXRGB_GET8b(ctrlVal, 1);
                uint8_t B = HEXRGB_GET8b(ctrlVal, 2); 
                hsv = ( tR*R + tG*G + tB*B) * (Cmax-Cmin); // scaled with 256 because tR, tG and tB are scaled
                hsv /= Cmax;
                hsv >>= 8;      // scale back 
                if (!enhancer) hsv *= scale;      // apply gain if transfer function not assigned
                else hsv = enhancer->Get(hsv);    // ... or pass through 8-bit transfer function (specialization scales output with 256)
                break;
            }

            case 's':  // Saturation enhancer
                if (!enhancer) {
                    // proportional
                    hsv = Cmin * scale;         // (1-S) * V = "whiteness" scaled by value
                    // STDOUT.print("Sat = "); STDOUT.println(sat);
                }
                else {
                    // transfer function
                    // hsv = enhancer->Get(Cmin);   // (1-S) passed through 8-bit transfer function (specialization scales output with 256)
                    hsv = enhancer->Get(255-(Cmax-Cmin));   // (1-S) passed through 8-bit transfer function (specialization scales output with 256)

                    // hsv = enhancer->Get(Cmax);   // V passed through 8-bit transfer function (specialization scales output with 256)
                    // STDOUT.print("TF("); STDOUT.print(Cmax); STDOUT.print(") = "); STDOUT.print(hsv>>8);
                    // hsv *= Cmin;                // Cmin/Cmax = (1-S) so we get (1-S) * TF(V)
                    // hsv /= Cmax;
                    // STDOUT.print("; brightness = "); STDOUT.println(hsv);
                }
            break;
            

            case 'l': // Lightness enhancer
                if (!enhancer) {
                    // // proportional 
                    hsv = scale * Cmax;     // "V" as in HSV
                }
                else {
                    // transfer function
                    hsv = enhancer->Get(Cmax);   // pass through 8-bit transfer function (specialization scales output with 256)
                }     
            break;   
            
        }
        if (hsv>65535) hsv=65535;   // saturated h, s or v [0,65535] 
        for (uint8_t i=0; i<ALED_MAXEM; i++) 
            brightness[i] = hsv;
        return true;
       
};

};
#ifdef ARDUINO_ARCH_ESP32   // ESP architecture
uint32_t ColorRenderer_HSLe::last_ctrlVal = 0;
uint16_t ColorRenderer_HSLe::Cmax = 0;
uint16_t ColorRenderer_HSLe::Cmin = 0;
#endif
class ColorRenderer_CRM : public ColorRenderer_Interface {
private:
    uint8_t nOuts;                                  // number of outpus (comes from CRM size)
    vector<float> crm;                              // Color Rendering Matrix (vector of 3-column rows)
    TF<uint16_t, uint16_t>* gammaTF[ALED_MAXEM];      // pointers to tranfer functions for gamma correction for each output 
    vector<uint8_t> gammaRef;                       // reference points for gamma transfer function
public:
    ColorRenderer_CRM() { 
        nOuts=0;
        for (uint8_t i=0; i<ALED_MAXEM; i++) 
            gammaTF[i] = 0;
    }

    // Initialize from external memory
    bool Init(const char* filename, uint16_t ID, float channelBrightness)  __attribute__((optimize("Og"))) { 
        CodReader reader;
        uint32_t nrOfBytes;

        if (!reader.Open(filename)) return false;     // Open COD file
        if(reader.FindEntry(ID) != COD_ENTYPE_TABLE) { reader.Close(); return false; }    // Look for ID and fill properties if a table was found
        if (reader.codProperties.table.Handler != HANDLER_ColorRenderer_CRM) { reader.Close(); return false; } // Check if table has the correct handler
        if (reader.codProperties.table.Rows < 1 || reader.codProperties.table.Rows > ALED_MAXEM || reader.codProperties.table.Columns != 3)  { 
            reader.Close(); return false; } // Must have 3 columns (RGB input) and nEm=[1, ALED_MAXEM] rows (output channels).
        nOuts = reader.codProperties.table.Rows;    // number of renderer outputs comes from CRM size
        crm.resize(3*nOuts);        // row-by-row, 3 elements/row         
        nrOfBytes = reader.ReadEntry(ID, (void*)crm.data(), 12*nOuts);   // Read table and copy data to crm
        reader.Close();  
        if (!nrOfBytes) return false;        // could not read or fit data...   
        // Scale CRM with channel brightness
        for (uint8_t i=0; i<nOuts; i++) 
            crm[i] *= channelBrightness;    
        // Set initial state
        ctrlVal = 0;
        for (uint8_t i=0; i<ALED_MAXEM; i++) {
            brightness[i] = 0;
            gammaTF[i] = 0;
        }
        // STDOUT.print("[ColorRenderer_CRM.Init] Initialized CRM renderer with "); STDOUT.print(nOuts); STDOUT.println(" outputs:");
        // for (uint8_t i=0; i<nOuts; i++) {
        //     STDOUT.print("     [ "); STDOUT.print(crm[3*i]), 
        //     STDOUT.print(", "); STDOUT.print(crm[3*i+1]);
        //     STDOUT.print(", "); STDOUT.print(crm[3*i+2]); STDOUT.println(" ]");
        // }
        return true; 
    };    

    // Assign a transfer function for gamma correction to each output.
    // IDs = pointer to vector if uint16, one for each emitter
    bool AppplyGamma(const char* filename, uint16_t* IDs) { // __attribute__((optimize("Og"))) {
        if (!nOuts) return false;     // Init() must be called first!
        CodReader reader;
     
        bool retval = true;         // assume success
        int8_t entryType;
        for (uint8_t ch=0; ch<nOuts; ch++) 
            if (*(IDs+ch)) {
                // 1. Check if the ID has been already assigned to a emitter, so we won't double the data
                uint16_t sameEmitter = 0;       // emitter previously assigned to the same transfer funtion ID
                if (!ch) 
                    for (uint8_t i=0; i<ch; i++)
                        if (*(IDs+ch) == *(IDs+i)) {sameEmitter=i; break; }
                if (!sameEmitter) { // Assign new ID
                    // 2. Search for ID in file
                    reader.Open(filename);    
                    entryType = reader.FindEntry(*(IDs+ch));
                    reader.Close();
                    if (entryType != COD_ENTYPE_TABLE) retval = false;      // could not find ID
                    else {  // Found transfer function
                        // 3. Assign RAM
                        gammaRef.resize(gammaRef.size() + 2*reader.codProperties.table.Columns);    // number of reference points = 2 * columns
                        gammaTF[ch] = new TF<uint16_t, uint16_t>;
                        // 4. Init TF, adding the new reference points at the end of the gammaRef vector
                        if (!gammaTF[ch]->Init(filename, *(IDs+ch), (uint16_t*)(gammaRef.data() + gammaRef.size() - 2*reader.codProperties.table.Columns), (uint32_t)(4*reader.codProperties.table.Columns))) retval = false;
                    }
                } else { // Assign same ID as sameEmitter
                    // 2. Search for ID in file - no need
                    // 3. Assign RAM
                    gammaTF[ch] = new TF<uint16_t, uint16_t>;
                    // 4. Init TF with reference points already existing in the gammaRef vector
                    if (!gammaTF[ch]->Init(filename, *(IDs+ch), gammaTF[sameEmitter]->GetRef(), (uint32_t)(4*gammaTF[sameEmitter]->GetSize()))) retval = false;
                }
            }
        // 5. Restore memory in case of failure
        if (!retval) {
            for (uint8_t ch=0; ch<ALED_MAXEM; ch++) 
                if (gammaTF[ch]) { delete gammaTF[ch]; gammaTF[ch]=0;}
            gammaRef.resize(0);
            gammaRef.shrink_to_fit();
        } 
        // 6. Start all assigned transfer functions
        else for (uint8_t ch=0; ch<nOuts; ch++) 
                if (gammaTF[ch]) gammaTF[ch]->Start();

        return retval;   

    }

    // Calculates brightness [0-65535] based on control value (hexRGB) 
    /*      |  brightness[0]  |     |  crm[0]  crm[1]  crm[2]  |     |  R  |
            |  brightness[1]  | =   |  crm[3]  crm[4]  crm[5]  |  *  |  G  |
            |  brightness[2]  |     |  crm[6]  crm[7]  crm[8]  |     |  B  |
    */
    bool UpdateBrightness(uint32_t ctrlval)  { // __attribute__((optimize("Og"))) {
        if (!nOuts) return false;   // renderer not initialized
        float R = (float)HEXRGB_GET8b(ctrlVal, 0);
        float G = (float)HEXRGB_GET8b(ctrlVal, 1);
        float B = (float)HEXRGB_GET8b(ctrlVal, 2); 
        for (uint8_t ch=0; ch<nOuts; ch++) {
            brightness[ch] = (uint16_t) (crm[ch]*R + crm[ch+1]*G + crm[ch+2]*B);
            if (gammaTF[ch])    // Apply gamma correction on each emitter, if transfer function assigned
                brightness[ch] = gammaTF[ch]->Get(brightness[ch]);
        }
        return true;
    }
};


// -----------------------------------------------------------
// ANALOG LED CHANNEL
#define NO_PIN 255      // Use 255 for no pin
// Get hardware pin (as defined in board config) from pin index (LED1=1, ... LED6=6)
uint8_t HardwarePin(uint8_t pinIndex) {
    switch (pinIndex) {
        case 1: return bladePowerPin1;  // LED1
        case 2: return bladePowerPin2;  // LED2
        case 3: return bladePowerPin3;  // LED3
        case 4: return bladePowerPin4;  // LED4
        // case 5: return bladePowerPin5;  // LED5
        // case 6: return bladePowerPin6;  // LED6
        default: return NO_PIN;
    }
}

class xAnalogLED_Channel   {
public: xAnalogLED_Channel() {  // 
        nEm = 0;
        renderer = 0;
        active = false;
        for (uint8_t i=0; i<ALED_MAXEM; i++) {
            pins[i] = NO_PIN;
            drivers[i] = 0;
        }
    }     
    ~xAnalogLED_Channel() {     // Garbage collector
        if (renderer) delete renderer;
        for (uint8_t i=0; i<3; i++)
            if (drivers[i]) delete drivers[i];
    }

   enum DriverType {
        driver_direct = 0,
        driver_legacy,
        driver_pPWL
    };


private:
    uint8_t nEm;                                // Number of emitters
    uint8_t pins[ALED_MAXEM];                            // PWM pins
    AnalogLED_DriverInterface* drivers[ALED_MAXEM];     // Drivers
    ColorRenderer_Interface* renderer;         // Color renderer
    bool active;                                //

    // Get the index of the first emitter with unassigned driver. Returns [0,3] or -1 if all emitters have drivers
    int8_t nextEmitter() {
        // if (!renderer) return -1;       // Renderer not initialized, we don't know the number of emitters
        for (uint8_t i=0; i<=nEm; i++)
            if (!drivers[i]) return i;  // found fist driver pointer not initialized
        return -1;      // all emitters have drivers
    }

public:

    // Creates and initializes a color renderer from external memory. Type of renderer comes from COD handler
    // This assignment also defines the number of channel's emitters, so it must run before driver assignment
    // Special assignment for direct renderer (no file needed): AssignRenderer(0, 0);
    bool AssignRenderer(const char* filename, uint16_t ID, float channelBrightness)  { //__attribute__((optimize("Og"))) {
        // 0. Special assignment: ID = 0 ==> Assign direct driver without checking file
        if (!ID) {
            renderer = new ColorRenderer_Direct;
            if (renderer->Init(0, channelBrightness)) return true;
            delete renderer; renderer = 0; 
            return false; 
        }
        // 1. Find in the COD file an entry with the right ID
        CodReader reader;
        int8_t entryType;
        if (!reader.Open(filename)) return false;          // file not found
        entryType = reader.FindEntry(ID);                  // search for ID
        reader.Close();  
        // STDOUT.print("[xAnalogLED_Channel.AssignRenderer]: "); STDOUT.print(filename); STDOUT.print(", "); STDOUT.print(ID); STDOUT.print(" FindEntry() result: "); STDOUT.println(entryType);
        // 2. Create new renderer
        switch (entryType) {
            case COD_ENTYPE_STRUCT:     // Found a structure
                if (reader.codProperties.structure.Handler == HANDLER_ColorRenderer_HSLe) {
                    // found HSL enhancer
                    renderer = new ColorRenderer_HSLe;  
                    break;
                } else return false;           // wrong handler
            case COD_ENTYPE_TABLE:      // Found a table
                if (reader.codProperties.structure.Handler == HANDLER_ColorRenderer_CRM) {
                    // found CRM
                    renderer = new ColorRenderer_CRM;  
                    break;
                } else return false;           // wrong handler
            default: return false;      // wrong entry type
        }
       // 3. Attempt to initialize
        if (renderer->Init(filename, ID, channelBrightness)) return true;  // this also sets the number of emitters
        delete renderer; // could not initialize renderer...
        renderer = 0; 
        return false; 
    
    }

    // Creates and initializes a driver from internal memory
    // Order of emitters = order of driver assignment !!!
    bool AssignDriver(uint8_t pin, DriverType type, void* data) { // __attribute__((optimize("Og"))) {
        // if (!renderer) return false;      // renderer not assigned, we don't know the number of emitters
        uint8_t currentEmitter = nextEmitter(); // find index of the first unassigned emitter (-1 if fails)
        if (currentEmitter == -1) return false; // could not find unassigned emitter
        // 1. Create new driver
        switch(type) {
            case driver_direct:  drivers[currentEmitter] = new AnalogLED_Driver_Direct; break;
            case driver_legacy: drivers[currentEmitter] = new AnalogLED_Driver_Legacy; break;
            case driver_pPWL: drivers[currentEmitter] = new AnalogLED_Driver_pPWL; break;
            default: return false;
        }
        // 2. Attempt to initialize
        if (!drivers[currentEmitter]->Init(data)) { 
            delete drivers[currentEmitter];
            drivers[currentEmitter] = 0;
            return false; 
        }
        // 3. Success, update data
        pins[currentEmitter] = pin;
        nEm ++;
        return true;
    }

    // Creates and initializes a driver from external memory. Type of driver comes from COD handler
    // Order of emitters = order of assignment !!!
    // Special assignment: ID=0 => direct driver
    bool AssignDriver(uint8_t pin, const char* filename, uint16_t ID) { // __attribute__((optimize("Og"))) {
        // if (!renderer) return false;      // renderer not assigned, we don't know the number of emitters
        int8_t currentEmitter = nextEmitter(); // find index of the first unassigned emitter (-1 if fails)
        if (currentEmitter == -1) return false; // could not find unassigned emitter

        CodReader reader;
        int8_t entryType;
        if (!ID) { // Assign direct driver 
            drivers[currentEmitter] = new AnalogLED_Driver_Direct;  
            drivers[currentEmitter]->Init(0);   // cannot fail with default scale
        } else { // Assign driver from file
            // 1. Find in the COD file an entry with the right ID
            if (!reader.Open(filename)) return false;          // file not found
            entryType = reader.FindEntry(ID);                  // search for ID
            reader.Close();  
            // 2. Create new driver
            switch (entryType) {
                case COD_ENTYPE_STRUCT:     // Found a structure
                    if (reader.codProperties.structure.Handler == HANDLER_AnalogLED_Driver_Legacy) {
                        // found legacy data
                        drivers[currentEmitter] = new AnalogLED_Driver_Legacy;  
                        break;
                    }
                    return false;           // handler not recognized
                case COD_ENTYPE_TABLE:      // Found a table
                    if (reader.codProperties.structure.Handler == HANDLER_AnalogLED_Driver_pPWL) {
                        // found pPWL data
                        drivers[currentEmitter] = new AnalogLED_Driver_pPWL;   
                        break;
                    }
                    return false;           // handler not recognized
                default: return false;      // wrong entry type
            }
            // 3. Attempt to initialize
            if (!drivers[currentEmitter]->Init(filename, ID)) { 
                delete drivers[currentEmitter]; 
                drivers[currentEmitter] = 0; 
                return false; 
            }    
        }

        // Success, update pin data
        pins[currentEmitter] = pin;
        nEm ++;
        return true;

    }

    struct ledChannelData {
            uint8_t nEm;            // Number of emitters
            uint16_t renderer;      // Renderer ID, 0 for direct
            uint16_t drivers[3];    // Driver ID for each emitter
            uint16_t gammaTFs[3];    // Transfer function ID for each emitter's gamma correction
    } __attribute__((packed));

    // Read channel data from file. 
    static bool get_ledChannelData(const char* filename, uint16_t ID, ledChannelData* data) {// __attribute__((optimize("Og"))) {
        CodReader reader;
        uint32_t numBytes=0;        
        if (!reader.Open(filename)) return false;                               // File not found
        if (reader.FindEntry(ID) == COD_ENTYPE_STRUCT) 
            numBytes = reader.ReadEntry(ID, (void*)data, sizeof(ledChannelData));      // Attempt to read a structure with the requested ID. 
        reader.Close();    
        // STDOUT.print("[xAnalogLED_Channel.get_ledChannelData] Read ");  STDOUT.print(numBytes); STDOUT.print(" bytes from "); STDOUT.print(filename);
        // STDOUT.print(", ID="); STDOUT.print(reader.codProperties.structure.ID);  STDOUT.print(", Expected ID="); STDOUT.print(ID); 
        // STDOUT.print(", Handler="); STDOUT.print(reader.codProperties.structure.Handler);
        // STDOUT.print(". Channel data: nEm="); STDOUT.print(data->nEm); STDOUT.print(", renderer="); STDOUT.print(data->renderer); 
        // STDOUT.print(", driver1="); STDOUT.print(data->drivers[0]); STDOUT.print(", driver2="); STDOUT.print(data->drivers[1]); STDOUT.print(", driver3="); STDOUT.println(data->drivers[2]); 
        if (numBytes != sizeof(ledChannelData)) return false;                                            // Could not read from file
        if (reader.codProperties.structure.Handler != HANDLER_AnalogLED_Channel) return false;    // Wrong handler
        return true;
    }


 // Initialize LED channel from internal memory. Does not activate it.
    bool Install(ledChannelData* data, uint8_t* pins_, float channelBrightness)  { // __attribute__((optimize("Og"))) {
        // 1. Assign drivers
        bool success = true;            // assume success
        for (uint8_t i=0; i<data->nEm; i++)  {
            #ifdef DIAGNOSE_BOOT
                STDOUT.print("....... Installing driver for hardware pin "); STDOUT.print(pins_[i]); STDOUT.print(", ID = "); STDOUT.print(data->drivers[i]); STDOUT.println("... ");
            #endif // DIAGNOSE_BOOT
            if (!AssignDriver(pins_[i], INSTALL_FILE, data->drivers[i]))          // 1. Search driver in install file (or install direct driver if ID=0)
                if (!AssignDriver(pins_[i], LEDLIB_FILE, data->drivers[i]))  {     // 2. Search driver in library file
                    success = false;
                    break;
                }
        }        

        // 2. Assign color renderer 
        if (success) {
            #ifdef DIAGNOSE_BOOT
                STDOUT.print("....... Installing renderer, ID = "); STDOUT.print(data->renderer); STDOUT.println("... ");
            #endif // DIAGNOSE_BOOT
            if (data->renderer) {
                // Specified renderer => number of emitters comes from renderer's definition
                if (!AssignRenderer(INSTALL_FILE, data->renderer, channelBrightness))            // 1. Search renderer in install file
                    if (!AssignRenderer(LEDLIB_FILE, data->renderer, channelBrightness))         // 2. Search renderer in library file
                        success = false;
            } else {
                // Unspecified renderer => assign direct renderer 
                if (!AssignRenderer(0, 0, channelBrightness)) success = false;
            }
        }
        
        // 3. Assign gamma transfer functions
        if (success)
            if (!renderer->AppplyGamma(INSTALL_FILE, &(data->gammaTFs[0]))) 
                if (!renderer->AppplyGamma(LEDLIB_FILE, &(data->gammaTFs[0]))) 
                    success=false;
        

        if (!success) { // failed, free memories
            if (renderer) delete renderer;
            renderer = 0;
            for (uint8_t i=0; i<data->nEm; i++) {
                if (drivers[i]) delete drivers[i];
                drivers[i] = 0;
                pins[i] = NO_PIN;
            }
            return false;
        } 
        if (nEm>1) installConfig.monochrome = false;
           
        return true;
     
    }

    // Initialize LED channel from file and pins. Does not activate it.
    bool Install(const char* filename, uint16_t ID, uint8_t pin1, uint8_t pin2, uint8_t pin3, float channelBrightness)  { // __attribute__((optimize("Og"))) {
        // 1. Count pins
        uint8_t nPins=0;        // number of pins available for assignment
        if (pin1 != NO_PIN) nPins++;
        if (pin2 != NO_PIN) nPins++;
        if (pin3 != NO_PIN) nPins++;
        if (!nPins) return false;

        // 2. Read LED channel definition from file
        CodReader reader;
        uint32_t numBytes;
        ledChannelData codData;
        

        if (!reader.Open(filename)) return false;                               // File not found
        numBytes = reader.ReadEntry(ID, (void*)&codData, sizeof(codData));      // Attempt to read a structure with the requested ID. 
        reader.Close();      

        // STDOUT.print("[AnalogLED_Channel.Install] Install ID ");  STDOUT.print(ID); STDOUT.print(" from file "); STDOUT.print(filename); 
        // STDOUT.print(" at pins = "); STDOUT.print(pin1); STDOUT.print(", "); STDOUT.print(pin2); STDOUT.print(", "); STDOUT.print(pin3); 
        // STDOUT.print(". Read "); STDOUT.print(numBytes); STDOUT.println(" bytes.");
        if (!numBytes) return false;                                            // Could not read from file
        if (reader.entryType != COD_ENTYPE_STRUCT) return false;                // Found ID but it's not a struct   
        if (reader.codProperties.structure.Handler != HANDLER_AnalogLED_Channel) return false;    // Wrong handler

        // 4. Assign drivers and update number of emitters
        bool success = true;            // assume success
        uint8_t pins_[3] = { pin1, pin2, pin3 };     // need those as vector, until pins is initialized
        for (uint8_t i=0; i<3; i++) 
         if (pins_[i] != NO_PIN) {
            #ifdef DIAGNOSE_BOOT
                STDOUT.print(" ---> Installing driver for hardware pin "); STDOUT.print(pins_[i]); STDOUT.print(", ID = "); STDOUT.print(codData.drivers[i]); STDOUT.println(" ... ");
            #endif // DIAGNOSE_BOOT
            if (!AssignDriver(pins_[i], filename, codData.drivers[i]))                      //  1. Search driver in current file (or install direct driver if ID=0)
                if (!AssignDriver(pins_[i], INSTALL_FILE, codData.drivers[i]))       // 2. Search driver in install file
                    if (!AssignDriver(pins_[i], LEDLIB_FILE, codData.drivers[i]))   // 3. Search driver in library file
                        success = false;
        }
        if (nEm != nPins) success = false; // wrong number of pins, must match number of emitters         

        // 4. Assign color renderer 
        #ifdef DIAGNOSE_BOOT
            STDOUT.print(" ---> Installing renderer, ID = "); STDOUT.print(codData.renderer); STDOUT.println(" ... ");
        #endif // DIAGNOSE_BOOT
        if (success && codData.renderer) {
            // Specified renderer => number of emitters comes from renderer's definition
            if (!AssignRenderer(filename, codData.renderer, channelBrightness))                        // 1. Search renderer in current file
                if (!AssignRenderer(INSTALL_FILE, codData.renderer, channelBrightness))         // 2. Search renderer in install file
                    if (!AssignRenderer(LEDLIB_FILE, codData.renderer, channelBrightness))     // 3. Search renderer in library file
                       success = false;
        } else {
            // Unspecified renderer => assign direct renderer 
            if (!AssignRenderer(0, 0, channelBrightness)) success = false;
        }

        // 5. Assign gamma transfer functions
        if (success)
            if (!renderer->AppplyGamma(INSTALL_FILE, &(codData.gammaTFs[0]))) 
                if (!renderer->AppplyGamma(LEDLIB_FILE, &(codData.gammaTFs[0]))) 
                    success=false;
        

        if (!success) { // failed, free memories
            if (renderer) delete renderer;
            renderer = 0;
            for (uint8_t i=0; i<nPins; i++) {
                if (drivers[i]) delete drivers[i];
                drivers[i] = 0;
                pins[i] = NO_PIN;
            }
            return false;
        } 

        return true;
     
    }

    // void Raport() {
    //     STDOUT.print(nEm); STDOUT.print(" emitters: ");
    //     if (nEm) {
    //         for (uint8_t i=0; i<nEm; i++) {
    //             STDOUT.print("pin"); STDOUT.print(i); STDOUT.print("="); STDOUT.print(pins[i]); STDOUT.print(", ");
    //             STDOUT.print("driver"); STDOUT.print(i); STDOUT.print("="); STDOUT.print((long)(drivers[i])); STDOUT.print(", ");
    //         }
    //         STDOUT.print("renderer="); STDOUT.print((long)renderer);
    //     }
    // }

    // Activate PWM on all pins
    void Activate() {
        if (!renderer) return;       // no emitters assigned
        for (uint8_t i=0; i<nEm; i++) {
            LSanalogWriteSetup(pins[i]);
            LSanalogWrite(pins[i], 0);  // make it black
        }
        // STDOUT.println("Activating LED channel");
        active = true;
    }

     // Deactivate PWM on all pins
    void Deactivate() {
        if (!renderer) return;       // no emitters assigned
        for (uint8_t i=0; i<nEm; i++) {
            LSanalogWrite(pins[i], 0);  // make it black
            LSanalogWriteTeardown(pins[i]);
        }
        // STDOUT.println("Deactivating LED channel");
        active = false;

    }

    // Set color
    void Set(uint32_t hexRGB) { // __attribute__((optimize("Og"))) {
        if (!renderer) return;          // LED channel not initialized
        if (!active) return;            // LED channel not activated
        // STDOUT.print("[AnalogLED_Channel.Set]: Set hexRGB = "); STDOUT.println(hexRGB);

        // if (renderer->ctrlVal == hexRGB) return;        // Color already set
        if (!renderer->UpdateBrightness(hexRGB)) return;      // Renderer failure
        // STDOUT.print("[AnalogLED_Channel.Set]: Set hexRGB = "); STDOUT.print(hexRGB);
        // STDOUT.print(" on channel with "); STDOUT.print(nEm); STDOUT.print(" emitters, at pins: "); 
        // STDOUT.print(pins[0]); STDOUT.print(", "); STDOUT.print(pins[1]); STDOUT.print(", "); STDOUT.print(pins[2]); STDOUT.print(": "); 
        for (uint8_t i=0; i<nEm; i++) 
            // if (drivers[i]->brightness != renderer->brightness[i]) {
            //     // only call driver if input brightness changed
            //     uint16_t regVal = drivers[i]->Get_regVal(renderer->brightness[i]);   // Get PWM register value (0-32767)
            //     if (pins[i]!=NO_PIN) LSanalogWrite(pins[i], regVal);                                      // Apply register value
            //     // STDOUT.print(" brightness["); STDOUT.print(i); STDOUT.print("]="); STDOUT.print(renderer->brightness[i]);
            //     // STDOUT.print(", regVal["); STDOUT.print(i); STDOUT.print("]="); STDOUT.print(regVal);
            // }
        {   
            uint16_t regVal = drivers[i]->Get_regVal(renderer->brightness[i]);   // Get PWM register value (0-32767)
            if (pins[i]!=NO_PIN) LSanalogWrite(pins[i], regVal);                                      // Apply register value
            
        }
        // STDOUT.println("");
    }

 #ifdef X_LIGHTTEST
    // Serve parser 
    bool Parse(const char* cmd, const char* arg)  { // __attribute__((optimize("Og"))) {
        if (!strcmp(cmd, "aled_pintest")) {      
            if (!nEm) return false;     // LED not initialized      
            char initial_arg[20];
            for(uint8_t i=0; i<strlen(arg); i++) initial_arg[i] = arg[i];
            char* token = strtok(initial_arg, ",");
            // 1. Extract target emitter
            uint8_t ledPinIndex = atoi(token);   
            uint8_t targetPin = HardwarePin(ledPinIndex);
            if (targetPin == NO_PIN) return false;      // unknown channel
            uint8_t targetEm = 255;
            for (uint8_t i=0; i<nEm; i++)
                if (pins[i] == targetPin) targetEm = i;
            if (targetEm == 255) return false;          // channel not assigned to this LED           
            // 2. Extract brightness
            token = strtok(NULL, ",");
            uint16_t bri= atoi(token);   // Extract brightness        
            // 3. Set and report   
            uint16_t regVal = drivers[targetEm]->Get_regVal(bri);          // Get PWM register value (0-32767)
            Activate();         
            LSanalogWrite(pins[targetEm], regVal);
            STDOUT.print(battery_monitor.battery()); STDOUT.print(", ");    // report battery voltage
            STDOUT.println(regVal);                                         // report register value
            return true;
        }
        return false;
    }

    void print_bright() {
        if (nEm>0) { STDOUT.print(renderer->brightness[0]);  } // report renderer's calculated brightness on 1st channel
        if (nEm>1) { STDOUT.print(", ");  STDOUT.print(renderer->brightness[1]);  } // report renderer's calculated brightness on 2nd channel
        if (nEm>2) { STDOUT.print(", ");  STDOUT.print(renderer->brightness[2]);  } // report renderer's calculated brightness on 3rd channel          
        STDOUT.println("");
    }


#endif // X_LIGHTTEST

};

// Wrapper over multiple AnalogLED_Channels. Channels must be installed individually
class xAnalogLED {
#ifdef X_LIGHTTEST
private: 
    bool activated;     // we need to activate the LED on serial command, but only once
#endif
public:
    uint8_t nCh;        // number of channels = 1 ... ALED_MAXCH
    xAnalogLED_Channel* channels[ALED_MAXCH];

    xAnalogLED() {
        nCh = 0; 
        for (uint8_t i=0; i<ALED_MAXCH; i++) 
            channels[i] = 0;
        #ifdef X_LIGHTTEST
            activated = false;
        #endif
    }

    // Activate all channels
    void Activate() {
        #ifdef X_LIGHTTEST
            if (activated) return;      // already activated
            activated = true;           // activate now
        #endif
        for (uint8_t i=0; i<ALED_MAXCH; i++) 
            if (channels[i]) channels[i]->Activate();
    }

    // Deactivate all channels
    void Deactivate() {
        #ifdef X_LIGHTTEST
            if (!activated) return;      // already deactivated
            activated = false;           // deactivate now
        #endif
        for (uint8_t i=0; i<ALED_MAXCH; i++) 
            if (channels[i]) channels[i]->Deactivate();
    }

    // Set color on all channels
    void Set(uint32_t hexRGB) {
        // STDOUT.print("Set AnalogLED to "); STDOUT.println(hexRGB);
        for (uint8_t i=0; i<ALED_MAXCH; i++) 
            if (channels[i]) channels[i]->Set(hexRGB);
    }

 #ifdef X_LIGHTTEST
    // Serve parser 
    bool Parse(const char* cmd, const char* arg)  { // __attribute__((optimize("Og"))) {        
		if (!strcmp(cmd, "aled_chtest")) {     
            // 1. Extract RGB      
            char initial_arg[20];
            for(uint8_t i=0; i<strlen(arg); i++) initial_arg[i] = arg[i];
            char* token = strtok(initial_arg, ",");
            uint8_t targetCh = atoi(token) - 1;   // Extract channel number (starts at 1 in command argument)
            if (targetCh >= nCh) return false;      // Wrong channel number
            token = strtok(NULL, ",");
            uint8_t Red= atoi(token);   // Extract R
            token = strtok(NULL, ",");
            uint8_t Green= atoi(token);   // Extract G
            token = strtok(NULL, ",");
            uint8_t Blue= atoi(token);   // Extract B

            // 2. Pack hexRGB
            uint32_t hexRGB = Red<<8;
            hexRGB += Green;
            hexRGB <<= 8;
            hexRGB += Blue;

            // 3. Set and report
            Activate();
            channels[targetCh]->Set(hexRGB);    
            // STDOUT.print("Red="); STDOUT.print(Red); STDOUT.print(", Green="); STDOUT.print(Green); STDOUT.print(", Blue="); STDOUT.print(Blue); STDOUT.print(". HexRGB is");  STDOUT.println(hexRGB);
            STDOUT.print(battery_monitor.battery()); STDOUT.print(", ");            // report battery voltage
            channels[targetCh]->print_bright();

            return true;
        }
        return false;
    }
#endif // X_LIGHTTEST

};



#endif // XANALOGLED_H