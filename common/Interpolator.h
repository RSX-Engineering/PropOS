/****************************************************************************
 *  INTERPOLATORS                                                           *
 *  (C) Marius RANGU & Cosmin PACA @ RSX Engineering                        *
 *  fileversion: v1.3 @ 2023/11;    license: GPL3                           *
 ****************************************************************************
 *  Base class Interpolator provides a common interface for                 *
 *  multiple interpolators:                                                 *
 *  - LUT (look-up table): Linear, uni-dimensional, on constant grid        *
 *  - TF (transfer function): Linear, uni-dimensional                       *
 *  - RTS (real time sequence): Linear, uni-dimensional, synchronous        *
 ****************************************************************************/

#ifndef XINTERPOLATOR_H
#define XINTERPOLATOR_H


#include "Utils.h"
#include "CodReader.h"
#include "codids.h"
#include <type_traits>

#define MAX_TIME_SCALE 10000 // maximum timeScale for RTS is 10000 (10 seconds)


enum InterpolatorState {
    interpolator_not_initialized=0,   
    interpolator_ready,
    interpolator_running,
    interpolator_error
};


// -----------------------------------------------------------
// Template of parent class for multiple interpolators
// REFT = reference type; usually we want to store reference points on lower resolution, to save memory
// WORKT = working type, normally higher resolution than REFT. WORKT must be able to represent -REFT^2 !!! 
template<class REFT, class WORKT>
class Interpolator {   
protected:
    uint16_t refSize;            // Number of reference values. This is the number of <REFT> values in refData and not necessary the number of reference points.
    REFT* refData;              // Pointer to reference data points
    WORKT* precalcData;         // Pointer to extra RAM to keep pre-calculated data - for speedup!
public:
    InterpolatorState state;    // Current state

public:
    // Find reference data in a file and get size in bytes
    uint32_t FindRef(const char* filename, uint16_t ID) // __attribute__((optimize("Og")))
    { 
        CodReader reader;
        uint32_t nrOfBytes = 0;

        if (!reader.Open(filename)) return 0;      // Open COD file
        if(reader.FindEntry(ID) != COD_ENTYPE_TABLE) {reader.Close(); return 0; }    // Look for ID and handler, return 1  and fill properties if a table was found
        reader.Close();         // close file, we only need the properties so won't read the actual entry
        if (reader.codProperties.table.Handler != HANDLER_Interpolator)  return 0; // Found the ID but has a wrong handler
        switch (reader.codProperties.table.DataType) {
            // TODO: check type and protect against improper assignment
            case 1: nrOfBytes = 1; break;       // uint8 needs 1 byte
            case 2: nrOfBytes = 2; break;       // int16 needs 2 bytes
            case 3: nrOfBytes = 4; break;       // float needs 4 bytes
            default: return 0;                  // unknown data type
        }
        nrOfBytes *= reader.codProperties.table.Rows * reader.codProperties.table.Columns;      // calculate table size, in bytes
        
        // STDOUT.print("[FindRef]: Found interpolator data: ID="); STDOUT.print(ID); STDOUT.print(", DataType="); STDOUT.print(reader.codProperties.table.DataType); STDOUT.print(", Handler="); STDOUT.print(reader.codProperties.table.Handler);
        // STDOUT.print(", Rows="); STDOUT.print(reader.codProperties.table.Rows); STDOUT.print(", Columns="); STDOUT.print(reader.codProperties.table.Columns); STDOUT.print(", Bytes="); STDOUT.print(nrOfBytes); STDOUT.println(".");
        return nrOfBytes;
    }

     REFT* GetRef() {
        return refData;
    }
     uint16_t GetSize() {
        return refSize;
    }
protected:
    // Read reference data from a file and copy to RAM. Set properties and return number of <REFT> values copied to data
    // dataBytes shows the available size of 'data', it must be large enough to store all <REFT> values
    uint16_t GetRef(const char* filename, uint16_t ID, REFT* data, uint32_t dataBytes) // __attribute__((optimize("Og"))) 
    {
        CodReader reader;
        bool result;
        uint32_t nrOfBytes = 0, refRead = 0;

        if (!reader.Open(filename)) return 0;     // Open COD file
        if(reader.FindEntry(ID) != COD_ENTYPE_TABLE) { reader.Close(); return 0; }    // Look for ID and fill properties if a table was found
        if (reader.codProperties.table.Handler != HANDLER_Interpolator) { reader.Close(); return 0; } // Check table has the correct handler
        nrOfBytes = reader.ReadEntry(ID, (void*)data, dataBytes);   // Read table and copy at most 'dataBytes' to 'data'
        reader.Close();         
        
        if (!nrOfBytes) return 0;       // failed to read entry
        refRead = nrOfBytes / sizeof(REFT);
        if (nrOfBytes % refRead) return 0;      // something's wrong if number of bytes is not an integer multiple of number of reference values
        if (refRead>65535) return 0;            // interpolators only allow up to 65535 reference points
        // STDOUT.print("[GetRef]: Got interpolator data: ID="); STDOUT.print(ID); STDOUT.print(", DataType="); STDOUT.print(reader.codProperties.table.DataType); STDOUT.print(", Handler="); STDOUT.print(reader.codProperties.table.Handler);
        // STDOUT.print(", Rows="); STDOUT.print(reader.codProperties.table.Rows); STDOUT.print(", Columns="); STDOUT.print(reader.codProperties.table.Columns); STDOUT.print(", Bytes="); STDOUT.print(nrOfBytes); STDOUT.println(".");

        return refRead;

     
    }

    // Check validity of reference data; size = number of values in data* (not number of bytes!)
    virtual bool CheckRef(REFT* data, uint16_t size)  {
        return true;
    }
public:
    // Initialize from internal memory - includes reset:
    bool Init(REFT* data, uint16_t size)  {
        state = interpolator_not_initialized;        // assume failure
        if (!CheckRef(data, size)) return false;    // check validity
        // all good, initialize and reset:
        refSize = size; 
        refData = data;  
        precalcData = 0;     
        Reset();                                    // reset internal data and set state to ready
        // STDOUT.print("[Interpolator.Init] Initialized with "); STDOUT.print(refSize); STDOUT.print(" references:");
        // for (uint8_t i=0; i<refSize; i++)
        // { STDOUT.print(" "); STDOUT.print(*(data+i)); }
        // STDOUT.println("");
        return true;                                
    }

    // Initialize from external memory  - includes reset:
    bool Init(const char* filename, uint16_t ID, REFT* ram, uint32_t ramSize) { // __attribute__((optimize("Og")))  { 
        state = interpolator_not_initialized;       // assume failure     
        uint32_t numOf;
        numOf = FindRef(filename, ID);           // Find reference in file and get number of bytes needed in RAM
        // STDOUT.print("[TF.Init()] got "); STDOUT.print(numOf); STDOUT.print(" reference values, allocated RAM is"); STDOUT.println(ramSize);
        if (!numOf) return false;                // Reference not found
        if (numOf > ramSize) return false;       // Not enough RAM    
        numOf = GetRef(filename, ID, ram, ramSize);  // Read reference from file and store in RAM; get number of values
        // if (!CheckRef(ram, numOf)) return false;    // check validity

        // Success: we have 'numOf' reference values in RAM, starting at address 'ram'
        // STDOUT.print("[Init] Successfully read "); STDOUT.print(numOf); STDOUT.println(" reference values");
        // for (uint8_t i=0; i<numOf; i++) {
        //     STDOUT.print(" ");
        //     STDOUT.print(*(ram+i));
        // }
        // STDOUT.println();


        refSize = numOf;
        refData = ram;
        precalcData = 0;
        Reset();    // reset internal data and set state to ready
        return true;
    }
    
    // De-initialize. Call or reproduce this when overriding!
    void Free() { 
        refSize = 0;              
        refData = 0;
        precalcData = 0;
        state = interpolator_not_initialized;
    }


    // Start interpolator:
    virtual void  Start() { 
        if (state != interpolator_ready) return;
        state = interpolator_running;
    }

    // Pre-calculate and store whatever possible
    // Working RAM must be reserved externally. Please don't malloc!
    // ramSize is not really needed, but we'll check it to guard the working RAM
    virtual bool SpeedUp(WORKT* ram, uint32_t ramSize) { 
        precalcData = 0;    // unless overritten there's no precalculated data
        return false;
    }


    // Stop and reset internal state:
    virtual void  Reset() { 
        state = interpolator_ready;
    }

    virtual WORKT Get(WORKT x = 0) { 
        if (state != interpolator_running) return 0;   // interpolator should be initialized and started before Get()
        // ...
        return 0; 
    }


};



//  -----------------------------------------------------------
// LOOK UP TABLE
// Linear uni-dimensional interpolator on X grid 
template<class REFT, class WORKT>
class LUT  : public Interpolator<REFT, WORKT> {
public:
    // Bring dependent names in scope (lost when binding templates against inheritance):
    using Interpolator<REFT, WORKT>::refSize;          
    using Interpolator<REFT, WORKT>::refData;          
    using Interpolator<REFT, WORKT>::precalcData;      // | slope[0]| slope[1] | ... | slope[N-2] |
    using Interpolator<REFT, WORKT>::state;
// public:
private:
    WORKT xMin, xMax;        // range on X axis
    WORKT xStep;             // scale factor on X axis. x_scaled = xMin + index * xStep
 public:   
    // CONSTRUCTORS:
    // Constructor with default range
    LUT() {
        xMin = 0;
        if (std::is_floating_point<WORKT>::value) xMax = 1;    // default range when working with floats is 0:1
        else xMax = std::numeric_limits<WORKT>::max();    // default range when working with integers is 0:typemax
        xStep = 0;          // calculated by start(), as it depends on refSize, which is not specified yet
        // STDOUT.println("LUT::LUT constructor");
    }

    // Constructor with specified range
    LUT(WORKT xMin_, WORKT xMax_) {
        xMin = xMin_;
        xMax = xMax_;
        xStep = 0;          // calculated by start(), as it depends on refSize, which is not specified yet
        // STDOUT.print("LUT::LUT("); STDOUT.print(xMin_); STDOUT.print(", "); STDOUT.print(xMax_); STDOUT.println(") constructor");
    }


    // Check validity of reference data; size = number of values in data* (not number of bytes!)
    bool CheckRef(REFT* data, uint16_t size) override {
        if (size<2) return false;                     // need at least two reference points
        if (!data) return false;                      // need actual data
        return true;
    }

    // Pre-calculate the slope of all segments and store them in RAM, to speed up further calculations
    bool SpeedUp(WORKT* ram, uint32_t ramSize) override {
        uint16_t i;        
        // 1. Check state:
        precalcData = 0;              // assume failure
        if (state!=interpolator_running) return false;          // Need everything initialized for speedup
        // 2. Assign RAM:
        if (ramSize < (refSize-1) * sizeof(WORKT)) return false;        // not enough RAM
        precalcData = ram;      // assign RAM for pre-calculated data
        // 3. Calculate and store slopes:
        for (i=0; i<refSize-1; i++) {
            ram[i] = ((WORKT)refData[i+1] - (WORKT)refData[i]);       
            ram[i] /= xStep;
        }
        return true;
    }

    // Start interpolator:
    void  Start() override { 
        if (state != interpolator_ready) return;
        xStep = ( xMax - xMin ) / (refSize-1);
        state = interpolator_running;     
        // STDOUT.print("[LUT.Start] Starting with range ");  STDOUT.print(xMin); STDOUT.print(" - "); STDOUT.print(xMax); STDOUT.print(" and step "); STDOUT.print(xStep); STDOUT.print(". References: ");
        // for (uint8_t i=0; i<refSize; i++)
        //     { STDOUT.print(refData[i]); STDOUT.print(" "); }
        // STDOUT.println("");
    }

    // Update range, then start interpolator with updated range:
    void Start(WORKT xMin_, WORKT xMax_) {
        if (xMax_>xMin_) {
            xMin = xMin_;       
            xMax = xMax_;
        }     
        // STDOUT.print("[LUT.Start(min,max)] Setting range to ");  STDOUT.print(xMin_); STDOUT.print(" - "); STDOUT.print(xMax_); STDOUT.print("");
        Start();
    }



    // Run the interpolator on input x and return output
    WORKT Get(WORKT x = 0) override {   
        if (state != interpolator_running) return 0;   // interpolator should be initialized and started before Get()
        uint16_t k = (x-xMin) / xStep;              // index of segment in refData
        WORKT xk = xMin + k*xStep;
        WORKT slope;
        if (precalcData) 
            slope = precalcData[k];        // slope already pre-calculated
        else 
            slope = ((WORKT)refData[k+1] - (WORKT)refData[k]) / xStep; // calculate slope now
        return (WORKT)refData[k] + slope * (x - xk);      // calculate and return y(x)
    }
    
};

// LUT specialization for <uint16_t, uint16_t>. 
// This is an illegal <REFT, WORKT> pair as WORKT must be able to hold -REFT^2, normally we should use <uint16_t, int32_t> 
// This specialization worksaround the integer overflow problem but speedup will no longer be available, as we can't store signed slopes as uint.
template<>
uint16_t LUT<uint16_t, uint16_t>::Get(uint16_t x) {   
    if (state != interpolator_running) return 0;   // interpolator should be initialized and started before Get()
    if (x<xMin)  x=xMin; 
    uint16_t k = (x-xMin) / xStep;              // index of segment in refData
    uint16_t xk = xMin + k*xStep;
    int32_t retval = x - xk;           // we'll calculate in int32 to avoid overflowing 
    // STDOUT.print("DeltaX = "); STDOUT.println(retval);
    int16_t deltaY = refData[k+1] - refData[k];
    // STDOUT.print("DeltaY = "); STDOUT.println(deltaY);
    retval *= deltaY;
    // STDOUT.print("DeltaX * DeltaY = "); STDOUT.println(retval);
    retval /= xStep;
    // STDOUT.print("DeltaX * DeltaY / xStep = "); STDOUT.println(retval);
    retval += refData[k];
    uint16_t retval16;
    if (retval>65535) {   
        retval = 65535;     // clamp to uint16
        // STDOUT.print("[LUT<uint16, uint16>.Get] Overflow, retval is "); STDOUT.print(retval); STDOUT.println(", clamped to 65535 !!!"); 
    }
    else retval16 = (uint16_t)retval;
    // STDOUT.print("[LUT.Get] x = "); STDOUT.print(x); STDOUT.print(", k = "); STDOUT.print(k); STDOUT.print(", xStep = "); STDOUT.print(xStep); 
    // STDOUT.print(", xk = "); STDOUT.print(xk); STDOUT.print(", output = "); STDOUT.println(retval16);
    return retval16;
}


//  -----------------------------------------------------------
// TRANSFER FUNCTION
// Linear uni-dimensional interpolator 
template<class REFT, class WORKT>
class TF  : public Interpolator<REFT, WORKT> {
public:
    // Bring dependent names in scope (lost when binding templates against inheritance):
    using Interpolator<REFT, WORKT>::refSize;          
    using Interpolator<REFT, WORKT>::refData;          
    using Interpolator<REFT, WORKT>::precalcData;      // | slope[0]| slope[1] | ... | slope[N-2] |
    using Interpolator<REFT, WORKT>::state;
// public:
protected:
    uint16_t N;      // number of reference points = refSize / 2

    // Find index of the leftmost reference X (=Xk), for a specified x. No protection at ends.
    uint16_t FindK(WORKT x) {
        uint16_t k = 1;
        while(k <= N-1) {
            if (x <= (WORKT)refData[k]) return k-1;
            k++;
        }
        return k-1;
    }

 public:   
    // Check validity of reference data; size = number of values in data* (not number of bytes!)
    bool CheckRef(REFT* data, uint16_t size) override {
        if (size<4) return false;                     // need at least two reference points
        if (size%2) return false;                     // need an even number of values (X and Y for each reference point)
        if (!data) return false;                      // need actual data
        for (uint16_t i=0; i<size/2-1; i++)
            if (data[i] >= data[i+1]) return false;    // X values must be increasing monotonic
        return true;
    }

    // Pre-calculate the slope of all segments and store them in RAM, to speed up further calculations
    // ram should be able to store N-1 WORKT numbers
    bool SpeedUp(WORKT* ram, uint32_t ramSize) override {
        uint16_t i;        
        // 1. Check state:
        precalcData = 0;              // assume failure
        if (state!=interpolator_running) return false;          // Need everything initialized for speedup
        // 2. Assign RAM:
        if (ramSize < (N-1)*sizeof(WORKT)) return false;        // not enough RAM
        precalcData = ram;      // assign RAM for pre-calculated data
        // 3. Calculate and store slopes:
        for (i=0; i<N-1; i++) {
            ram[i] = (WORKT)refData[N+i+1] - (WORKT)refData[N+i]; // y[k+1] - y[k]
            ram[i] /= (WORKT)refData[i+1] - (WORKT)refData[i];    // / ( x[k+1]-x[k] )
        }
        return true;
    }

    // Start interpolator:
    void  Start() override { 
        if (state != interpolator_ready) return;
        N = refSize / 2;
        state = interpolator_running;       
    }



    // Run the interpolator on input x and return output
    WORKT Get(WORKT x = 0) override {   
        if (state != interpolator_running) return 0;   // interpolator should be initialized and started before Get()
        // if (x<=xMin) return refData[0];                // outside range extend first / last value 
        // if (x>=xMax) return refData[refSize-1];        
        uint16_t k = FindK(x);              // index of segment in refData
        WORKT slope;
        if (precalcData) {
            slope = precalcData[k];        // slope already pre-calculated
            return (WORKT)refData[k+N] + slope * (x - (WORKT)refData[k]);      // calculate and return y(x)
        }
        else {  // calculate now
            slope = ((WORKT)refData[k+N+1] - (WORKT)refData[k+N]) * (x - (WORKT)refData[k]);
            slope /= ((WORKT)refData[k+1] - (WORKT)refData[k]);
            return (WORKT)refData[k+N] + slope;
        }
    }



    
};

// Rogue TF specialization for <uint8, int16>. Y gets scaled [0, 255] --> [0, 65535]
template<>
bool TF<uint8_t, int16_t>::SpeedUp(int16_t* ram, uint32_t ramSize)  {
    uint16_t i;        
    // 1. Check state:
    precalcData = 0;              // assume failure
    if (state!=interpolator_running) return false;          // Need everything initialized for speedup
    // 2. Assign RAM:
    if (ramSize < (N-1)*sizeof(int16_t)) return false;        // not enough RAM
    precalcData = ram;      // assign RAM for pre-calculated data
    // 3. Calculate and store slopes:
    for (i=0; i<N-1; i++) {
        ram[i] = refData[N+i+1] - refData[N+i];         // y[k+1] - y[k]
        ram[i] <<= 8;       // 256 * ( y[k+1] - y[k] )
        ram[i] /= refData[i+1] - refData[i];    // 256 * ( y[k+1] - y[k] ) / ( x[k+1]-x[k] )
    }
    return true;
}

template<>
int16_t TF<uint8_t, int16_t>::Get(int16_t x) {
    if (state != interpolator_running) return 0;   // interpolator should be initialized and started before Get() 
    uint16_t k = FindK(x);              // index of segment in refData
    int16_t slope;
    if (precalcData) 
        slope = precalcData[k];        // slope already pre-calculated
    else {
        slope = refData[N+k+1] - refData[N+k];         // y[k+1] - y[k]
        slope <<= 8;       // 256 * ( y[k+1] - y[k] )
        slope /= refData[k+1] - refData[k];    // 256 * ( y[k+1] - y[k] ) / ( x[k+1]-x[k] )
    }
    slope *= (x -refData[k]);                   // 256 * deltaY
    slope >>= 8;                                // delta Y
    // STDOUT.print("TF("); STDOUT.print(x); STDOUT.print("): k="); STDOUT.print(k); STDOUT.print(", slope="); STDOUT.print(slope); STDOUT.print(", output="); STDOUT.println(refData[k+N] + slope);
    return refData[k+N] + slope;                // Y

 } 


// TF specialization for <uint8, uint16>. This is illegal as WORKT must be able to represent -REFT^2. Slope and output are scaled with 256
// Input range is [0,255]. Output range is [0,65535]. No speedup for this specialization!
template<>
uint16_t TF<uint8_t, uint16_t>::Get(uint16_t x) {
    if (state != interpolator_running) return 0;   // interpolator should be initialized and started before Get() 
    uint16_t k = FindK(x);              // index of segment in refData
    int32_t slope;
    slope = refData[N+k+1] - refData[N+k];  // y[k+1] - y[k]
    slope <<= 8;                            // 256 * ( y[k+1] - y[k] )
    slope /= refData[k+1] - refData[k];     // 256 * ( y[k+1] - y[k] ) / ( x[k+1]-x[k] )
    slope *= (x -refData[k]);               // 256 * deltaY
    return (refData[k+N]<<8) + slope;       // 256 * Y
 } 


//  -----------------------------------------------------------
// SCALABLE TRANSFER FUNCTION 
// Same as TF but allows rescaling of both axes. Much slower than TF
template<class REFT, class WORKT>
class scalableTF  : public TF<REFT, WORKT> {

    using TF<REFT, WORKT>::refSize;          
    using TF<REFT, WORKT>::refData;          
    using TF<REFT, WORKT>::state;          
private:    
    float xScale, yScale;
    REFT xOffset, yOffset;
    REFT xMin, yMin;


// Re-initialize a transfer function with scaled references
public: 
    // Stop and reset internal state. Scale reverts to 1:1
    void  Reset() override { 
        state = interpolator_ready;
        xMin = 0;   xOffset = 0;    xScale = 1;  // no scaling on X
        yMin = 0;   yOffset = 0;    yScale = 1;  // no scaling on Y        
    }

    // Set new input range: X = [xMin, xMax]
    bool SetXrange(REFT xMin_, REFT xMax_) {
        if (state!=interpolator_ready && state!=interpolator_running) return false;     // need it initialized to apply scale 
        if (xMin_ >= xMax_) return false;     // invalid scale
        xMin = xMin_;
        xOffset = refData[0];
        xScale = (refData[refSize/2-1] - refData[0]) / (xMax_ - xMin_);
        // STDOUT.print("[scalableTF.SetXRange] Rescaled input --> ");  STDOUT.print(xMin_); STDOUT.print(", "); STDOUT.println(xMax_);
        // STDOUT.print("[scalableTF.SetXrange] Rescaled input: "); STDOUT.print(refData[0]); STDOUT.print(", ");
        // STDOUT.print(refData[refSize/2-1]); STDOUT.print("  -->  "); STDOUT.print(xMin_); STDOUT.print(", "); STDOUT.print(xMax_);
        // STDOUT.print(". Scale = "); STDOUT.print(xScale); STDOUT.print(", Offset = "); STDOUT.println(xOffset);
        return true;
    }

    // Set new output range: Y = [yMin, yMax]
    bool SetYrange(REFT yMin_, REFT yMax_) {
        if (state!=interpolator_ready && state!=interpolator_running) return false;     // need it initialized to apply scale 
        if (!(refData[refSize-1] - refData[refSize/2])) return false;                   // invalid reference points, would result in division by 0
        yMin = yMin_;
        yOffset = refData[refSize/2];
        yScale = (yMax_ - yMin_) / (refData[refSize-1] - refData[refSize/2]);
        // STDOUT.print("[scalableTF.SetYRange] Rescaled output --> ");  STDOUT.print(yMin_); STDOUT.print(", "); STDOUT.println(yMax_);
        // STDOUT.print("[scalableTF.SetYRange] Rescaled output: "); STDOUT.print(refData[refSize/2]); STDOUT.print(", ");
        // STDOUT.print(refData[refSize-1]); STDOUT.print("  -->  "); STDOUT.print(yMin_); STDOUT.print(", "); STDOUT.print(yMax_);
        // STDOUT.print(". Scale = "); STDOUT.print(yScale); STDOUT.print(", Offset = "); STDOUT.println(yOffset);
        return true;
    }

    WORKT Get(WORKT x = 0) override {   
        // 1. Scale X
        float temp = (x-xMin);       // calculate scaled values in float, to avoid integer losses
        temp *= xScale;
        temp += xOffset;            // X scaled
        
        // 2. Get unscaled Y from regular TF
        temp =  TF<REFT, WORKT>::Get((WORKT)temp);  // Y unscaled
        
        // 3. Scale Y
        temp -= yOffset;  
        temp *= yScale;
        temp += yMin;            // Y scaled
        return temp;        
    }

};

//  -----------------------------------------------------------
// REAL-TIME SEQUENCE
// Defined by time-value points, with time scaled by 'timeScale' 
// Average Get() run times:
//  STM32: 1us (uint8, uint8), 1 us (uint8, float)
//  ESP32: 3us (uint8, uint8), 3 us (uint8, float)
template<typename REFT, typename WORKT>
class RTS : public Interpolator<REFT, WORKT> {
    uint8_t remRuns; // Number of runs remaining, 0 = infinite
    uint8_t N; // Number of reference points
    uint8_t k; // Current time point
    uint16_t timeScale; // Time scale factor, all reference 't' values are multiplied by this to get the actual time in ms
    WORKT slope; // Slope of the current segment [k, k+1]
    uint32_t startTime; // Time when the interpolator was started
    uint32_t timePrev;   // Time at t[k]
    uint32_t timeNext;   // Time at t[k+1]

    // Bring dependent names in scope (lost when binding templates against inheritance):
    public:
    using Interpolator<REFT, WORKT>::refSize;          
    using Interpolator<REFT, WORKT>::refData;          
    using Interpolator<REFT, WORKT>::precalcData;      // | slope[0]| slope[1] | ... | slope[N-2] |


    using Interpolator<REFT, WORKT>::state;

    bool CheckRef(REFT* data, uint16_t size) override {
        // refData = [t0, ... tN-1, v0, ... vN-1]
        // refSize = 2*N
        // Real time = t * timeScale
        // Check validity of reference data
        if (size<4) return false;                     // need at least two reference points
        if (size%2) return false;                     // need an even number of values (X and Y for each reference point)
        if (!data) return false;                      // need actual data
        for (uint16_t i=0; i<size/2-1; i++)
            if (data[i] >= data[i+1]) return false;    // X values must be increasing monotonic 
        // STDOUT.print("[RTS.CheckRef] Valid reference data: ");
        // for (uint16_t i=0; i<size; i++)
        //     { STDOUT.print(data[i]); STDOUT.print(" "); }   
        // STDOUT.println("");
        return true;            
    }


    // Function to calculate slope. Also updates timePrev and timeNext
    WORKT CalculateSlope(uint8_t k_) {
        if (k_ >= N) return 0;
        WORKT slope_ = (WORKT)refData[N+k_+1] - (WORKT)refData[N+k_];
        slope_ /= timeScale * ( (WORKT)refData[k_+1] - (WORKT)refData[k_] );
        timePrev = timeScale * refData[k_];
        timeNext = timeScale * refData[k_+1];
        // STDOUT.print("[RTS.CalculateSlope] k = "); STDOUT.print(k_); STDOUT.print(", t[k]="); STDOUT.print(timeScale *refData[k_]); 
        // STDOUT.print(", t[k+1]="); STDOUT.print(timeScale *refData[k_+1]); STDOUT.print(", v[k]="); STDOUT.print(refData[N+k_]); 
        // STDOUT.print(", v[k+1]="); STDOUT.print(refData[N+k_+1]); STDOUT.print(", slope = "); STDOUT.println(slope_);
        return slope_;
    }

    // Start with specified time scale and number of runs. nRuns=0 => infinite loop
    void Start(uint16_t timeScale_, uint8_t nRuns, uint32_t del = 0) {
        if (state != interpolator_ready) return;
        timeScale = timeScale_;
        k = 0;
        N = refSize / 2;
        slope = CalculateSlope(0);
        state = interpolator_running;
        startTime = millis();
        remRuns = nRuns;
        if (del) {
            timeNext = 0;       // signal that we're in delay mode (timeNext cannot be 0 in normal mode)    
            timePrev = startTime + del;   // timePrev is the end of the delay
        }        
        // STDOUT.print("[RTS.Start] Starting with time scale "); STDOUT.print(timeScale); STDOUT.print(", slope = "); STDOUT.println(slope);
        // STDOUT.println("[RTS.Start] References points (time-value): ");
        // for (uint16_t i=0; i<N; i++) {
        //     STDOUT.print("   "); STDOUT.print(timeScale*refData[i]); STDOUT.print(" - "); STDOUT.println(refData[N+i]);
        // }
    }

    void Start() override {
        Start(10, 1);     // in unspecified, default time scale is 10 ms and start with only one repetition
    }

    // Run the interpolator and return output at current time
    WORKT Get(WORKT x = 0) override {
        if (state != interpolator_running) { // interpolator should be initialized and started before Get()
            if (k == N-1) return slope;   // if the interpolator was previously started, return the last value stored in slope    
            else return 0;                                              
        }
        
        // Check if we're in delay mode
        if (!timeNext) {
            if (millis() < timePrev) return refData[N];  // still in delay mode, return first value
            else {  // end of delay, restore times and run normally 
                startTime = millis();    // Starting to run now
                timePrev = timeScale * refData[0];  
                timeNext = timeScale * refData[1];   
                // STDOUT.print("[RTS.Get] End of delay, timePrev = "); STDOUT.print(timePrev); STDOUT.print(", timeNext = "); STDOUT.print(timeNext);   
                // STDOUT.print(", slope = "); STDOUT.println(slope);
                             
            }
        }

        WORKT val; // Current output value        
        uint32_t timeNow =  millis() - startTime;  // Current time  
        // Check if we're still in the current segment
        if (timeNow < timeNext) {
            // We're still in the current segment, calculate and return current value
            val = refData[N+k] + slope * (timeNow - timePrev);
            // STDOUT.print("[RTS.Get] val = "); STDOUT.println(val);
            return val;
        }
        else {  // Move to next segment
            k++;
            if (k < N-1) {
                    val = refData[N+k];
                    slope = CalculateSlope(k);
                    // STDOUT.print("[RTS.Get] Changed segment to "); STDOUT.print(k); STDOUT.print(", slope = "); STDOUT.println(slope); 
                    // STDOUT.print("[RTS.Get] val = "); STDOUT.println(val);                     
                    return val;
            }                
            else {  // Reached last segment, repeat or end
                val = refData[refSize-1];                
                int16_t remainingRuns = remRuns-1;    // -1 if we're in infinite loop, 0 if we're in last run, >0 if we still have runs left
                if (remainingRuns) {    // repeat
                    k = 0;
                    slope = CalculateSlope(k);
                    // STDOUT.print("[RTS.Get] Repeating, slope = "); STDOUT.println(slope); 
                    // STDOUT.print("[RTS.Get] val = "); STDOUT.println(val); 
                    if (remRuns) remRuns = remainingRuns;   // if we're in infinite loop, remRuns is 0, so don't change it
                    startTime = millis();
                    return val;
                }
                else {  // end
                    state = interpolator_ready;
                    slope = val;        // Store last value
                    // STDOUT.print("[RTS.Get] End of interpolator, val = "); STDOUT.println(val); 
                    return val;
                }
            }
        }       
    }

    // Get current time scale, if interpolator is running
    uint16_t getTimeScale() { 
        if (state == interpolator_running)
            return timeScale; 
        else return 0;
    }
    
    // Set new time scale (if interpolator is running) and maintain relative positon.
    // If newTimeScale = 0, stop interpolator
    void setTimeScale(uint16_t newTimeScale) { 
        if (state != interpolator_running) return;   // interpolator not started so makes no sense to set timescale, will be overwritten by Start() anyway
        if (newTimeScale == timeScale) return;       // nothing to do
        
        if (!newTimeScale) {    // stop interpolator
            state = interpolator_ready;
            return;
        }
        if (newTimeScale < 0 || newTimeScale > MAX_TIME_SCALE) return; // invalid time scale
    
        float RF = (float)newTimeScale / (float)timeScale;  // rescale factor
        // Interpolator not running yet, rescale the delay
        if (!timeNext) {    
            int32_t delayRemaining = timePrev - millis(); // remaining delay
            if(delayRemaining < 0) delayRemaining = 0;    // clamp to 0
            uint32_t newDelay = delayRemaining * RF; // calculate the new remaining delay
            timeScale = newTimeScale;   // set the new time scale
            slope = CalculateSlope(0);  // recalculate slope, timePrev and timeNext
            startTime = millis();       // restart delay counting
            timePrev = startTime + newDelay; // set the new end of the delay     
            timeNext = 0;   // signal that we're in delay mode (changed by CalculateSlope)
            // STDOUT.print("[RTS.setTimeScale] New end of delay = "); STDOUT.print(timePrev); 
            // STDOUT.print(", rescale factor = "); STDOUT.print(RF);  
            // STDOUT.print(", new time scale = "); STDOUT.println(timeScale);
            return;
        }
        // Interpolator is running, need to adjust times to maintain relative position
        float newStartTime = RF*(int32_t)startTime + millis()*(1-RF); // new start time, to maintain relative position
        if (newStartTime < 0) newStartTime = 0;   // clamp to 0
        if (newStartTime > millis()) newStartTime = millis()-1; // clamp to 1 ms earlier
        
        startTime = newStartTime;         // Set new start time
        timeScale = newTimeScale;         // Set new time scale
        slope = CalculateSlope(k);        // Recalculate slope, timePrev and timeNext
        // STDOUT.print("[RTS.setTimeScale] New time scale = "); STDOUT.print(timeScale); STDOUT.print(", rescale factor = "); STDOUT.print(RF);
        // STDOUT.print(", new start time = "); STDOUT.println(startTime);       
    }

};


// RTS specialization for <uint8_t, uint8_t> - optimized for execution speed
// Need to speccialize the entire class because we can't store the slope as uint8
template<>
class RTS<uint8_t, uint8_t> : public Interpolator<uint8_t, uint8_t> {
    uint8_t remRuns; // Number of runs remaining, 0 = infinite
    uint8_t N; // Number of reference points
    uint8_t k; // Current time point
    uint16_t timeScale; // Time scale factor, all reference 't' values are multiplied by this to get the actual time in ms
    int16_t slope; // Slope of the current segment [k, k+1]. Note that we need to store it as int16_t to avoid overflow 
    uint32_t startTime; // Time when the interpolator was started
    uint32_t timePrev;   // Time at t[k]
    uint32_t timeNext;   // Time at t[k+1]

    // Bring dependent names in scope (lost when binding templates against inheritance):
    using Interpolator<uint8_t, uint8_t>::refSize;          
    using Interpolator<uint8_t, uint8_t>::refData;          
    using Interpolator<uint8_t, uint8_t>::precalcData;      // | slope[0]| slope[1] | ... | slope[N-2] |

    // Function to calculate slope. Also updates timePrev and timeNext
    int16_t CalculateSlope(uint8_t k_) {
        if (k_ >= N) return 0;
        int32_t slope_;  // need higher resolution to avoid overflow
        slope_ = (refData[N+k_+1] - refData[N+k_]); // y[k+1] - y[k]
        slope_ *= 256;   // slope is scaled by 256!
        slope_ /= timeScale*(refData[k_+1] - refData[k_]); // / ( x[k+1]-x[k] )
        timePrev = timeScale * refData[k_];
        timeNext = timeScale * refData[k_+1];
        // STDOUT.print("[RTS.CalculateSlope] k = "); STDOUT.print(k_); STDOUT.print(", t[k]="); STDOUT.print(timeScale *refData[k_]); 
        // STDOUT.print(", t[k+1]="); STDOUT.print(timeScale *refData[k_+1]); STDOUT.print(", v[k]="); STDOUT.print(refData[N+k_]); 
        // STDOUT.print(", v[k+1]="); STDOUT.print(refData[N+k_+1]); STDOUT.print(", slope = "); STDOUT.println(slope_);
        return slope_;
    }

public:
    using Interpolator<uint8_t, uint8_t>::state;
    bool CheckRef(uint8_t* data, uint16_t size) override {
        // refData = [t0, ... tN-1, v0, ... vN-1]
        // refSize = 2*N
        // Real time = t * timeScale
        // Check validity of reference data
        if (size<4) return false;                     // need at least two reference points
        if (size%2) return false;                     // need an even number of values (X and Y for each reference point)
        if (!data) return false;                      // need actual data
        for (uint16_t i=0; i<size/2-1; i++)
            if (data[i] >= data[i+1]) return false;    // X values must be increasing monotonic 
        // STDOUT.print("[RTS.CheckRef] Valid reference data: ");
        // for (uint16_t i=0; i<size; i++)
        //     { STDOUT.print(data[i]); STDOUT.print(" "); }   
        // STDOUT.println("");
        return true;            
    }


    // Start with specified time scale and number of runs. nRuns=0 => infinite loop
    // A delay can be specified in ms. Between start time and start time + delay, the output is v[0]
    void Start(uint16_t timeScale_, uint8_t nRuns, uint32_t del = 0) {
        if (state==interpolator_not_initialized || state==interpolator_error) return; // allow restart if initialized
        timeScale = timeScale_;
        k = 0;
        N = refSize / 2;
        slope = CalculateSlope(0);
        state = interpolator_running;
        startTime = millis();
        remRuns = nRuns;
        if (del) {
            timeNext = 0;       // signal that we're in delay mode (timeNext cannot be 0 in normal mode)    
            timePrev = startTime + del;   // timePrev is the end of the delay
        }
        // STDOUT.print("[RTS.Start] Starting with time scale "); STDOUT.print(timeScale); STDOUT.print(", slope = "); STDOUT.println(slope);
        // STDOUT.println("[RTS.Start] References points (time-value): ");
        // for (uint16_t i=0; i<N; i++) {
        //     STDOUT.print("   "); STDOUT.print(timeScale*refData[i]); STDOUT.print(" - "); STDOUT.println(refData[N+i]);
        // }
    }

    void Start() override {
        Start(10, 1);     // in unspecified, default time scale is 10 ms and start with only one repetition
    }

    // Run the interpolator and return output at current time
    uint8_t Get(uint8_t x = 0) override {
        if (state != interpolator_running) { // interpolator should be initialized and started before Get()
            if (k == N-1) return slope;   // if the interpolator was previously started, return the last value stored in slope    
            else return 0;                                              
        }
        // Check if we're in delay mode
        if (!timeNext) {
            if (millis() < timePrev) return refData[N];  // still in delay mode, return first value
            else {  // end of delay, restore times and run normally 
                startTime = timePrev;    // adjust start time to account for delay
                timePrev = timeScale * refData[0];  
                timeNext = timeScale * refData[1];                
            }
        }

        int32_t val; // Current output value        
        uint32_t timeNow =  millis() - startTime;  // Current time  

        // Check if we're still in the current segment
        if (timeNow < timeNext) {
            // We're still in the current segment, calculate and return current value
            val = slope * (timeNow - timePrev);
            val >>= 8;  // divide by 256
            val += refData[N+k];        
            // STDOUT.print("[RTS.Get] val = "); STDOUT.println(val);
            return (uint8_t)val;
        }
        else {  // Move to next segment
            k++;
            if (k < N-1) {
                    // val = refData[N+k];
                    slope = CalculateSlope(k);
                    val = slope * (timeNow - timePrev);
                    val >>= 8;  // divide by 256
                    val += refData[N+k];                     
                    // STDOUT.print("[RTS.Get] Changed segment to "); STDOUT.print(k); STDOUT.print(", slope = "); STDOUT.println(slope); 
                    // STDOUT.print("[RTS.Get] val = "); STDOUT.println(val);                   
                    return val;
            }                
            else {  // Reached last segment, repeat or end
                val = refData[refSize-1];                
                int16_t remainingRuns = remRuns-1;    // -1 if we're in infinite loop, 0 if we're in last run, >0 if we still have runs left
                if (remainingRuns) {    // repeat
                    k = 0;
                    slope = CalculateSlope(k);
                    // STDOUT.print("[RTS.Get] Repeating, slope = "); STDOUT.println(slope);
                    // STDOUT.print("[RTS.Get] val = "); STDOUT.println(val);
                    if (remRuns) remRuns = remainingRuns;   // if we're in infinite loop, remRuns is 0, so don't change it
                    startTime = millis();
                    return val;
                }
                else {  // end
                    state = interpolator_ready;
                    slope = val;        // Store last value
                    // STDOUT.print("[RTS.Get] End of interpolator, val = "); STDOUT.println(val); 
                    return val;
                }
            }
        }
       
    }

    // Get current time scale, if interpolator is running
    uint16_t getTimeScale() { 
        if (state == interpolator_running)
            return timeScale; 
        else return 0;
    }
    
    // Set new time scale (if interpolator is running) and maintain relative positon.
    // If newTimeScale = 0, stop interpolator
    void setTimeScale(uint16_t newTimeScale) { 
        if (state != interpolator_running) return;   // interpolator not started so makes no sense to set timescale, will be overwritten by Start() anyway
        if (newTimeScale == timeScale) return;       // nothing to do
        
        if (!newTimeScale) {    // stop interpolator
            state = interpolator_ready;
            return;
        }
        if (newTimeScale < 0 || newTimeScale > MAX_TIME_SCALE) return; // invalid time scale
    
        float RF = (float)newTimeScale / (float)timeScale;  // rescale factor
        // Interpolator not running yet, rescale the delay
        if (!timeNext) {    
            int32_t delayRemaining = timePrev - millis(); // remaining delay
            if(delayRemaining < 0) delayRemaining = 0;    // clamp to 0
            uint32_t newDelay = delayRemaining * RF; // calculate the new remaining delay
            timeScale = newTimeScale;   // set the new time scale
            slope = CalculateSlope(0);  // recalculate slope, timePrev and timeNext
            startTime = millis();       // restart delay counting
            timePrev = startTime + newDelay; // set the new end of the delay     
            timeNext = 0;   // signal that we're in delay mode (changed by CalculateSlope)
            // STDOUT.print("[RTS.setTimeScale] New end of delay = "); STDOUT.print(timePrev); 
            // STDOUT.print(", rescale factor = "); STDOUT.print(RF);  
            // STDOUT.print(", new time scale = "); STDOUT.println(timeScale);
            return;
        }
        // Interpolator is running, need to adjust times to maintain relative position
        float newStartTime = RF*(int32_t)startTime + millis()*(1-RF); // new start time, to maintain relative position
        if (newStartTime < 0) newStartTime = 0;   // clamp to 0
        if (newStartTime > millis()) newStartTime = millis()-1; // clamp to 1 ms earlier
        
        startTime = newStartTime;         // Set new start time
        timeScale = newTimeScale;         // Set new time scale
        slope = CalculateSlope(k);        // Recalculate slope, timePrev and timeNext
        // STDOUT.print("[RTS.setTimeScale] New time scale = "); STDOUT.print(timeScale); STDOUT.print(", rescale factor = "); STDOUT.print(RF);
        // STDOUT.print(", new start time = "); STDOUT.println(startTime);       
    }

};





// // #define WORK_TYPE uint8_t
// #define WORK_TYPE float
// #define REF_TYPE uint8_t
// #define NUM_REF_POINTS 5

// void testRTS() {
//     // Define the interpolator
//     RTS<REF_TYPE, WORK_TYPE> interpolator;

//     // Initialize the table with NUM_REF_POINTS with unevenly spaced time points
//     REF_TYPE table[2 * NUM_REF_POINTS] = {0, 15, 35, 70, 85, 0, 10, 255, 0, 50 };

//     // Initialize the interpolator with the table
//     if (!interpolator.Init(table, 2 * NUM_REF_POINTS)) {
//         STDOUT.println("Failed to initialize the interpolator");
//         return;
//     }

//     delay(100);     // get some millis

//     // Start the interpolator
//     uint32_t startTime = millis();
//     interpolator.Start(5, 2, 50);

//     // Define RangeStats object to keep track of execution times
//     RangeStats<int, 8> execTimes;

//     // Report the current value until the interpolator ends
//     uint32_t timeNow;
//     bool was1st = false;        // first intervention
//     bool was2nd = false;        // second intervention

//     while (interpolator.state == interpolator_running) {
//         uint32_t execStart = micros(); // Start execution timer
//         timeNow = millis();
//         WORK_TYPE val = interpolator.Get();
//         uint32_t execTime = micros() - execStart; // Calculate execution time
//         execTimes.Add(execTime); // Add execution time to stats
//         uint32_t runTime = timeNow - startTime;

        
//         // if (!was1st && runTime > 25) {  // shorten delay
//         //     interpolator.setTimeScale(1);
//         //     was1st = true;
//         // }

//         // if (!was1st && runTime > 25) {  // prelong delay
//         //     interpolator.setTimeScale(10);
//         //     was1st = true;
//         // }

//         if (!was1st && runTime > 200) {  // shorten 1st sequence
//             interpolator.setTimeScale(2);
//             was1st = true;
//         }
//         if (!was2nd && runTime > 350
//         ) {  // restore 2nd sequence
//             interpolator.setTimeScale(5);
//             was2nd = true;
//         }        

//         // if (!was1st && runTime > 200) {  // prelong 1st sequence
//         //     interpolator.setTimeScale(10);
//         //     was1st = true;
//         // }
//         // if (!was2nd && runTime > 800) {  // restore 2nd sequence
//         //     interpolator.setTimeScale(5);
//         //     was2nd = true;
//         // }

//         STDOUT.print(runTime);
//         STDOUT.print(" ");
//         STDOUT.println(val);
//         delay(2);
//     }

//     // Report execution time stats
//     STDOUT.print("Min execution time: ");
//     STDOUT.print(execTimes.min); STDOUT.println(" us");
//     STDOUT.print("Max execution time: ");
//     STDOUT.print(execTimes.max);    STDOUT.println(" us");
//     STDOUT.print("Avg execution time: ");
//     STDOUT.print(execTimes.avg);    STDOUT.println(" us");    
// }

#endif  // INTERPOLATOR_H
