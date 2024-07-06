#ifndef XUTILS_H
#define XUTILS_H

/********************************************************************
 * Various tools                                                    *
 *  (C) Marius RANGU @ RSX Engineering. Licensed under GNU GPL.     *
 *  fileversion: v1.0 @ 2022/02                                     *
 ********************************************************************
 *  - enabled by #define X_PROBECPU                                *
 *  - overwrites ScopedCycleCounter and LoopCounter                 *
 *  - adds CPUprobe and RangeStats                                        *
 ********************************************************************/


#include <limits>
#include <math.h>


// Define a read-only member of a class (leaves visibility public!)
// Only works with TYPE = built-in type (no classes, structs or enums) !
#define _readonly_(TYPE, name)   \
protected:                                \
    TYPE p_##name;                      \
public:                                 \
    const TYPE& name = p_##name         // public interface does not duplicate protected data!

// helper for real time debug
#define PRINT_TIMESTAMP     {       \
    uint32_t ccnow = DWT->CYCCNT;   \
    STDOUT.print(ccnow); STDOUT.print(": "); STDOUT.print(millis()); STDOUT.print(": [ms]: "); }

#ifdef ARDUINO_ARCH_STM32L4   // STM architecture
// clock frequency, in MHz (should be 80)
#define _SYSTEM_CORE_CLOCK_MHZ_     _SYSTEM_CORE_CLOCK_/1000000
#else
#define _SYSTEM_CORE_CLOCK_MHZ_     getCpuFrequencyMhz() // /1000000
#endif


/** -----------------------------------------------------------
 * Maintain range statistics on a stream of numbers
 *  T = data type. Must be a signed type!
 *  EMA_ORDER = order of the Exponential Moving Average filter 
 *  Usage:  range.Add() adds value to the range and update statistics
 *          range.min / .max / .avg gets range statistics
 *          range.Rescale() scales value to new range 
 *          
 */ 
template<class T, uint8_t EMA_ORDER>
class RangeStats {
    _readonly_(T, val);       // p_val keeps current value
    _readonly_(T, min);       // p_min keeps stream minimum 
    _readonly_(T, max);       // p_max_keeps stream maximum
    _readonly_(T, avg);       // p_avg keeps moving average

public:
    RangeStats() {  this->Reset();  }  // Simple constructor resets stream (for auto-ranging)
    RangeStats(T min_, T max_) { p_min = min_; p_max = max_; } // Initialized constructor sets limits (for rapid rescale)

    // Rescale a value from current range limits (min & max) to new limits (MIN & MAX)
    // Calculated in float, regardless of T. Not efficient, use rarely!
    float Rescale(T value, float MIN, float MAX) {
        if (value<p_min) value=p_min;
        if(value>p_max) value=p_max;
        float retVal = abs(MAX - MIN);
        retVal /= (p_max - p_min);      // slope
        retVal *= (value - p_min);      
        if (MIN <= MAX) {    // preserve monotony
            retVal = MIN + retVal;
            if (retVal < MIN) retVal = MIN; // correct potential rounding errors
            if (retVal > MAX) retVal = MAX;
        }
        else { // reverse monotony
            retVal = MIN - retVal;
            if (retVal < MAX) retVal = MAX; // correct potential rounding errors
            if (retVal > MIN) retVal = MIN;
        }        
        return retVal;
    }

    // reset stream 
    void Reset() {
        // empty stream, so use <T> limits for min and max
        p_val = 0;
        p_min = std::numeric_limits<T>::max();      
        p_max = std::numeric_limits<T>::min();
        p_avg = 0;          
    }

    // add a number to the stream
    void Add(const T& value) {
        p_val = value;   
        if (!p_avg) {
            // Fist number in the empty stream: 
            p_avg = value;
        }
        else {   
            // stream already initialized
            if (value < p_min) p_min = value;         // update min and max
            if (value > p_max) p_max = value; 
            // p_avg += (value-p_avg) / (2^EMA_ORDER);   // EMA iteration (hopefuly T is an integer type)
            p_avg += ((value-p_avg) >> EMA_ORDER);   // EMA iteration 
            // TODO: this is an integer specialization, write general template as well
        }
           
    }
};


#endif  // XUTILS_H


