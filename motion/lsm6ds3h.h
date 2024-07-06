#ifndef MOTION_LSM6DS3H_H
#define MOTION_LSM6DS3H_H

// Supports LSM6DS3, LSM6DSM and LSM6DSO
#ifdef ULTRAPROFFIE
  // Subscribes to power domain CPU but does not request power, will stay alive as long as the CPU is alive
  class LSM6DS3H : public I2CDevice, Looper, StateMachine, PowerSubscriber {
  public:
    LSM6DS3H() : I2CDevice(106), Looper(), PowerSubscriber(pwr4_CPU) {enabled = true;}
    
    bool enabled;
    void PwrOn_Callback() override { 
      enabled = true; 
      #ifdef DIAGNOSE_POWER
        STDOUT.println(" sns+ ");  
      #endif
    }          
    void PwrOff_Callback() override { 
      enabled = false; 
      #ifdef DIAGNOSE_POWER
        STDOUT.println(" sns- ");  
      #endif
      uint32_t startTime = millis();
      i2cbus.scheduledDeinitTime(0);   // set deinit timeout to 0 
      while (millis() - startTime <= 50) {   // was 1 
        // Run lsm and i2c loop to allow the state machine to disable sensor
        i2cbus.Loop(); // run i2cbus loop 
        Loop();        // run motion loop 
        // DoLoop();
      }
      i2cbus.scheduledDeinitTime(2000);  // restore timeout 
    }         

#else 
  class LSM6DS3H : public I2CDevice, Looper, StateMachine {
  public:
    LSM6DS3H() : I2CDevice(106), Looper() {}
#endif



  const char* name() override { return "LSM6DS3H"; }
  enum Registers {
    FUNC_CFG_ACCESS = 0x1,
    SENSOR_SYNC_TIME_FRAME = 0x4,
    FIFO_CONTROL1 = 0x6,
    FIFO_CONTROL2 = 0x7,
    FIFO_CONTROL3 = 0x8,
    FIFO_CONTROL4 = 0x9,
    FIFO_CONTROL5 = 0xA,
    ORIENT_CFG_G = 0xB,
    INT1_CTRL = 0xD,
    INT2_CTRL = 0xE,
    WHO_AM_I = 0xF,
    CTRL1_XL = 0x10,
    CTRL2_G = 0x11,
    CTRL3_C = 0x12,
    CTRL4_C = 0x13,
    CTRL5_C = 0x14,
    CTRL6_C = 0x15,
    CTRL7_G = 0x16,
    CTRL8_XL = 0x17,
    CTRL9_XL = 0x18,
    CTRL10_C = 0x19,
    MASTER_CONFIG = 0x1A,
    WAKE_UP_SRC = 0x1B,
    TAP_SRC = 0x1C,
    D6D_SRC = 0x1D,
    STATUS_REG = 0x1E,
    STATUS_SPIAux = 0x1E,
    OUT_TEMP_L = 0x20,
    OUT_TEMP_H = 0x21,
    OUTX_L_G = 0x22,
    OUTX_H_G = 0x23,
    OUTY_L_G = 0x24,
    OUTY_H_G = 0x25,
    OUTZ_L_G = 0x26,
    OUTZ_H_G = 0x27,
    OUTX_L_XL = 0x28,
    OUTX_H_XL = 0x29,
    OUTY_L_XL = 0x2A,
    OUTY_H_XL = 0x2B,
    OUTZ_L_XL = 0x2C,
    OUTZ_H_XL = 0x2D,
    SENSORHUB1_REG = 0x2E,
    SENSORHUB2_REG = 0x2F,
    SENSORHUB3_REG = 0x30,
    SENSORHUB4_REG = 0x31,
    SENSORHUB5_REG = 0x32,
    SENSORHUB6_REG = 0x33,
    SENSORHUB7_REG = 0x34,
    SENSORHUB8_REG = 0x35,
    SENSORHUB9_REG = 0x36,
    SENSORHUB10_REG = 0x37,
    SENSORHUB11_REG = 0x38,
    SENSORHUB12_REG = 0x39,
    FIFO_STATUS1 = 0x3A,
    FIFO_STATUS2 = 0x3B,
    FIFO_STATUS3 = 0x3C,
    FIFO_STATUS4 = 0x3D,
    FIFO_DATA_OUT_L = 0x3E,
    FIFO_DATA_OUT_H = 0x3F,
    TIMESTAMP0_REG = 0x40,
    TIMESTAMP1_REG = 0x40,
    TIMESTAMP2_REG = 0x41,
    STEP_TIMESTAMP_L = 0x49,
    STEP_TIMESTAMP_H = 0x4A,
    STEP_COUNTER_L = 0x4B,
    STEP_COUNTER_H = 0x4C,
    SENSORHUB13_REG = 0x4D,
    SENSORHUB14_REG = 0x4E,
    SENSORHUB15_REG = 0x4F,
    SENSORHUB16_REG = 0x50,
    SENSORHUB17_REG = 0x51,
    SENSORHUB18_REG = 0x52,
    FUNC_SRC = 0x53,
    TAP_CFG = 0x58,
    TAP_THS_6D = 0x59,
    INT_DUR2 = 0x5A,
    WAKE_UP_THS = 0x5B,
    WAKE_UP_DUR = 0x5C,
    FREE_FALL = 0x5D,
    MD1_CFG = 0x5E,
    MD2_CFG = 0x5F,
    OUT_MAG_RAW_X_L = 0x66,
    OUT_MAG_RAW_X_H = 0x67,
    OUT_MAG_RAW_Y_L = 0x68,
    OUT_MAG_RAW_Y_H = 0x69,
    OUT_MAG_RAW_Z_L = 0x6A,
    OUT_MAG_RAW_Z_H = 0x6B,
    CTRL_SPIAux = 0x70
  };
  
#define I2CLOCK() do {							\
  state_machine_.sleep_until_ = millis();				\
  while (!I2CLock()) {							\
    if (millis() - state_machine_.sleep_until_ > I2C_TIMEOUT_MILLIS) goto i2c_timeout; \
    YIELD();								\
  }									\
} while(0)
    
  void Loop() override {
#if 0
    // Uncomment this to debug motion timeouts.
    SaberBase::RequestMotion();
    if (!random(300)) delay(350);
#endif
    STATE_MACHINE_BEGIN();

    while (!i2cbus.inited()) YIELD();

    while (1) {
      first_motion_ = true;
      first_accel_ = true;

      #if defined(DIAGNOSE_SENSOR)
      STDOUT.print("Motion chip ... ");    
      #endif

      I2CLOCK();

      I2C_READ_BYTES_ASYNC(WHO_AM_I, databuffer, 1);
      id_ = databuffer[0];
      #if defined(DIAGNOSE_SENSOR)
      STDOUT.print(id_);
      #endif
      if (id_  == 105 || id_ == 106 || id_ == 108) {
        #if defined(DIAGNOSE_SENSOR)
        STDOUT.println(" found.");
        #endif
      } else  {
        #if defined(DIAGNOSE_SENSOR)
        STDOUT.println(" not found.");
        #endif
	      goto i2c_timeout;
      }
      I2C_WRITE_BYTE_ASYNC(CTRL1_XL, 0x84);  // 1.66kHz accel, 16G range
      I2C_WRITE_BYTE_ASYNC(CTRL2_G, 0x8C);   // 1.66kHz gyro, 2000 dps
      I2C_WRITE_BYTE_ASYNC(CTRL3_C, 0x44);   // ?
      I2C_WRITE_BYTE_ASYNC(CTRL4_C, 0x00);
      I2C_WRITE_BYTE_ASYNC(CTRL5_C, 0x00);
      I2C_WRITE_BYTE_ASYNC(CTRL6_C, 0x00);
      I2C_WRITE_BYTE_ASYNC(CTRL7_G, 0x00);
      I2C_WRITE_BYTE_ASYNC(CTRL8_XL, 0x00);
      I2C_WRITE_BYTE_ASYNC(CTRL9_XL, 0x38);  // accel xyz enable
      I2C_WRITE_BYTE_ASYNC(CTRL10_C, 0x38);  // gyro xyz enable
      if (motionSensorInterruptPin != -1) {
        I2C_WRITE_BYTE_ASYNC(INT1_CTRL, 0x3);  // Activate INT on data ready
        pinMode(motionSensorInterruptPin, INPUT);
      }
      I2CUnlock();

      last_event_ = millis();
#ifdef ARDUINO_ARCH_STM32L4   // STM architecture 
  attachInterrupt(digitalPinToInterrupt(motionSensorInterruptPin), motion_irq, RISING);

  #ifdef ULTRAPROFFIE
      while (enabled) {
  #else 
      while (SaberBase::MotionRequested()) {
  #endif
        Poll();
        if ((last_event_ + I2C_TIMEOUT_MILLIS * 2 - millis()) >> 31) {
          #if defined(DIAGNOSE_SENSOR)
          STDOUT.println("Motion timeout.");
          #endif
          detachInterrupt(digitalPinToInterrupt(motionSensorInterruptPin));
          goto i2c_timeout;
  	    }
	    YIELD();
    }
    detachInterrupt(digitalPinToInterrupt(motionSensorInterruptPin));

#else  // nPROFFIEBOARD
      while (true) {
	YIELD();
	if (!SaberBase::MotionRequested()) break;
	if (motionSensorInterruptPin == -1) {
	  while (!I2CLock((last_event_ + I2C_TIMEOUT_MILLIS * 2 - millis()) >> 31)) YIELD();
	  I2C_READ_BYTES_ASYNC(STATUS_REG, databuffer, 1);
	  if ((databuffer[0] & 3) != 3) {
	    I2CUnlock();
	    continue;
	  }
	} else {
	  if (!digitalRead(motionSensorInterruptPin)) {
	    if (millis() - last_event_ > I2C_TIMEOUT_MILLIS * 2) {
	      goto i2c_timeout;
	    }
	    continue;
	  } else {
	    last_event_ = millis();
	  }
	  while (!I2CLock((last_event_ + I2C_TIMEOUT_MILLIS * 2 - millis()) >> 31)) YIELD();
	}
	
	I2C_READ_BYTES_ASYNC(OUTX_L_G, databuffer, 12);
	// accel data available
	prop.DoAccel(
	  MotionUtil::FromData(databuffer + 6, 16.0 / 32768.0,   // 16 g range
			       Vec3::BYTEORDER_LSB, Vec3::ORIENTATION),
	  first_accel_);
	first_accel_ = false;
	// gyroscope data available
	prop.DoMotion(
	  MotionUtil::FromData(databuffer, 2000.0 / 32768.0,  // 2000 dps
			       Vec3::BYTEORDER_LSB, Vec3::ORIENTATION),
	  first_motion_);
	first_motion_ = false;

        I2CUnlock();
      }

#endif // Architecture

      #if defined(DIAGNOSE_SENSOR)
      STDOUT.print("Disabling motion...");
      #endif
      I2CLOCK();
      I2C_WRITE_BYTE_ASYNC(CTRL2_G, 0x0);  // accel disable
      I2C_WRITE_BYTE_ASYNC(CTRL1_XL, 0x0);  // gyro disable
      I2CUnlock();
      #if defined(DIAGNOSE_SENSOR)
      STDOUT.println(" done!");
      #endif

      #ifdef ULTRAPROFFIE
        while (!enabled) YIELD();
      #else 
        while (!SaberBase::MotionRequested()) YIELD();
      #endif 
      continue;

    i2c_timeout:
      #if defined(DIAGNOSE_SENSOR)
      STDOUT.println("Motion chip timeout, trying auto-reboot of motion chip!");
      #endif
      while (!I2CLock(true)) YIELD();
      Reset();
      SLEEP(20);
#define i2c_timeout i2c_timeout2
      I2C_WRITE_BYTE_ASYNC(CTRL3_C, 1);
#undef i2c_timeout      
    i2c_timeout2:
      I2CUnlock();
      SLEEP(20);
    }

    STATE_MACHINE_END();
  }



  void Dump() override {
    STDOUT << "LSM6DS3H: last_event_ " << last_event_
	   << " LINE: " << state_machine_.next_state_
	   << "\n";
  }

#ifdef ARDUINO_ARCH_STM32L4   // STM architecture
  void Poll() {
    if (!digitalRead(motionSensorInterruptPin)) {
      return;
    }
    I2CLockAndRun();
  }

  void RunLocked() override {
    ScopedCycleCounter cc(motion_interrupt_cycles);
    // All chunks are full
    if (!digitalRead(motionSensorInterruptPin)) {
      goto fail;
    }
    if (!stm32l4_i2c_notify(Wire._i2c, &LSM6DS3H::DataReceived, this, (I2C_EVENT_ADDRESS_NACK | I2C_EVENT_DATA_NACK | I2C_EVENT_ARBITRATION_LOST | I2C_EVENT_BUS_ERROR | I2C_EVENT_OVERRUN | I2C_EVENT_RECEIVE_DONE | I2C_EVENT_TRANSMIT_DONE | I2C_EVENT_TRANSFER_DONE | I2C_EVENT_TRANSMIT_ERROR))) {
      goto fail;
    }

    Wire._tx_data[0] = OUTX_L_G;
    if (!stm32l4_i2c_transfer(Wire._i2c, address_,
			      Wire._tx_data, 1,
			      databuffer, 12,
			      0)) {
      goto fail;
    }
    return;
  fail:
    I2CUnlock();
  }

  static void DataReceived(void *context, uint32_t event) {
    ScopedCycleCounter cc(motion_interrupt_cycles);
    ((LSM6DS3H*)context)->DataReceived2();
  }
  void DataReceived2() {
    stm32l4_i2c_notify(Wire._i2c, nullptr, 0, 0);
    I2CUnlock();
    // accel data available
    prop.DoAccel(MotionUtil::FromData(databuffer + 6, 16.0 / 32768.0,   // 16 g range
				      Vec3::BYTEORDER_LSB, Vec3::ORIENTATION),
		 first_accel_);
    
    first_accel_ = false;
    // gyroscope data available
    prop.DoMotion(MotionUtil::FromData(databuffer, 2000.0 / 32768.0,  // 2000 dps
				       Vec3::BYTEORDER_LSB, Vec3::ORIENTATION),
		  first_motion_);
    first_motion_ = false;
    last_event_ = millis();
    Poll();
  }
#endif // Architecture

  uint8_t databuffer[12];
  volatile uint32_t last_event_;
  bool first_motion_;
  bool first_accel_;
  uint8_t id_ = 105;
};

#endif
