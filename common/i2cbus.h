#ifndef COMMON_I2CBUS_H
#define COMMON_I2CBUS_H

#define I2C_TIMEOUT_MILLIS 300

#ifdef ARDUINO_ARCH_ESP32   // ESP architecture
#include "Wire.h"
#endif

class I2CBus : Looper, StateMachine {
public:
  const char* name() override { return "I2CBus"; }
  void Loop() {
    STATE_MACHINE_BEGIN();
    while (true) {
  
	  
#ifdef ARDUINO_ARCH_STM32L4   // STM architecture
      Wire.begin();
      Wire.setClock(400000);
#else
    //Wire.setPins(i2cDataPin, i2cClockPin);   
    Wire.begin(GPIO_NUM_2, GPIO_NUM_1, (uint32_t)400000);   // TEMPORARY FIX FOR ESP version TODO change this was 
		// Wire.begin(i2cDataPin, i2cClockPin, (uint32_t)400000);   // TEMPORARY FIX FOR ESP version TODO change this was 
    //Wire.begin(i2cDataPin, i2cClockPin, (uint32_t)800000);   // TEMPORARY FIX FOR ESP version TODO change this  400000
#endif

      i2c_detected_ = true;
      while (used()) YIELD();
      i2c_detected_ = false;
      // STDOUT.println("I2C sleeping..");
      Wire.end();
      while (!used()) YIELD();
      }
     
    STATE_MACHINE_END();
  }

  bool used() {
    return !((last_request_millis_ + shtimeMS - millis()) >> 31);
  }


  void scheduledDeinitTime(uint32_t ms) {
    shtimeMS = ms;
  }


  bool inited() {
    last_request_millis_ = millis();
    return i2c_detected_;
  }

  void dump() {
    STDOUT << "I2CBUS: last_request = " << last_request_millis_ << " (now = " << millis() << ")"
	   << " i2c_detected_ = " << i2c_detected_
	   << " used = " << used()
	   << "\n";
#ifdef ARDUINO_ARCH_STM32L4   // STM architecture
    STDOUT << "I2C STATE: " << Wire._i2c->state << "\n";
#endif      
  }
  
private:
  uint32_t last_request_millis_ = 0;
  int i;
  bool clock_detected, data_detected;
  bool i2c_detected_ = false;
  uint32_t shtimeMS = 2000;
};

#endif
