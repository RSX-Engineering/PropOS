#ifndef BLADES_WS2811_BLADE_H
#define BLADES_WS2811_BLADE_H

#include "abstract_blade.h"
#include "../styles/styles.h"


#ifdef ENABLE_WS2811

#ifndef EXTRA_COLOR_BUFFER_SPACE
#define EXTRA_COLOR_BUFFER_SPACE 0
#endif

Color16 color_buffer[maxLedsPerStrip + EXTRA_COLOR_BUFFER_SPACE];
BladeBase* current_blade = NULL;

class WS2811PIN {
  protected:
    uint32_t installedBrightness;   // For energy management, not for dynamic effects. 65535 = 100%
  public:
    void setInstalledBrightness(float bladeBrightness) {
      if (bladeBrightness < 0) bladeBrightness = 0;
      if (bladeBrightness > 1) bladeBrightness = 1;
      // installedBrightness = 65535 * bladeBrightness;  // proportional brightness
      installedBrightness = 3*65535 * bladeBrightness;  // current-saturating brightness
      // Serial.begin(115200);
      // Serial.print("[WS2811PinBase.setInstalledBrightness]: setting brightness scale to "); Serial.print(installedBrightness);
      // Serial.print(" ("); Serial.print((uint16_t)(100*bladeBrightness)); Serial.println("%).");
    }
public:
  virtual bool IsReadyForBeginFrame() = 0;
  virtual Color16 *BeginFrame() = 0;
  virtual bool IsReadyForEndFrame() = 0;
  virtual void WaitUntilReadyForEndFrame();
  virtual void EndFrame() = 0;
  virtual int num_leds() const = 0;
  virtual Color8::Byteorder get_byteorder() const = 0;
  virtual void Enable(bool enable) = 0;
  #ifdef RMT_WITH_TASK
  virtual int pin() const = 0;
  #endif
};


// Common, size adjusted to ~2000 interrupts per second.
#ifdef ARDUINO_ARCH_STM32L4   // STM architecture
  DMAMEM uint32_t displayMemory[200];
  #include "stm32l4_ws2811.h"
  #define DefaultPinClass WS2811Pin
  #define ProffieOS_yield() armv7m_core_yield()
#else 
  #include "rmt_pin.h"
  // #define DefaultPinClass RMTLedPin
  #include "esp_spiledpin.h"
  #define DefaultPinClass ESPILedPin
  #define ProffieOS_yield() do { } while(0)
#endif


void WS2811PIN::WaitUntilReadyForEndFrame() {
  while (!IsReadyForEndFrame()) ProffieOS_yield();
}
// WS2811-type blade implementation.
// Note that this class does nothing when first constructed. It only starts
// interacting with pins and timers after Activate() is called.
#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
class WS2811_Blade : public AbstractBlade, CommandParser, Looper, PowerSubscriber {
public:
  void PwrOn_Callback() override { 
    #ifdef DIAGNOSE_POWER
      STDOUT.println(" pix+ "); 
    #endif
  }
  void PwrOff_Callback() override { 
    #ifdef DIAGNOSE_POWER
      STDOUT.println(" pix- "); 
    #endif
  }
 bool HoldPower() override {  // Return true to pause power subscriber timeout
  return manual_;
 }

#else // ULTRAPROFFIE
class WS2811_Blade : public AbstractBlade, CommandParser, Looper {
#endif // ULTRAPROFFIE

public:
WS2811_Blade(WS2811PIN* pin,
             PowerPinInterface* power,
             uint32_t poweroff_delay_ms) :
    AbstractBlade(),
    CommandParser(NOLINK),
    Looper(NOLINK),
    poweroff_delay_ms_(poweroff_delay_ms),
    power_(power),
    pin_(pin)
#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
    , PowerSubscriber(pwr4_Pixel)
#endif
    {
    
    }
  const char* name() override { return "WS2811_Blade"; }

  void Power(bool on) {
    if (on) EnableBooster();
    if (!powered_ && on) {
#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
      RequestPower();
#endif      
      if (power_) power_->Init();
      pin_->Enable(true);
      colors_ = pin_->BeginFrame();
      for (int i = 0; i < pin_->num_leds(); i++) set(i, Color16());
      pin_->WaitUntilReadyForEndFrame();
      if (power_) power_->Power(on);
      pin_->EndFrame();
      colors_ = pin_->BeginFrame();
      for (int i = 0; i < pin_->num_leds(); i++) set(i, Color16());
      pin_->EndFrame();
      colors_ = pin_->BeginFrame();
      for (int i = 0; i < pin_->num_leds(); i++) set(i, Color16());
      pin_->EndFrame();
      current_blade = NULL;
    } else if (powered_ && !on) {
      colors_ = pin_->BeginFrame();
      for (int i = 0; i < pin_->num_leds(); i++) set(i, Color16());
      pin_->EndFrame();
      // Wait until it's sent before powering off.
      pin_->WaitUntilReadyForEndFrame();
      if (power_) power_->Power(on);
      pin_->Enable(false);
      if (power_) power_->DeInit();
      current_blade = NULL;
    }
    powered_ = on;
    allow_disable_ = false;
  }

  void Activate() override {
    #ifdef DIAGNOSE_BLADES
      STDOUT.print("Activating pixel blade with ");
      STDOUT.print(pin_->num_leds());
      STDOUT.println(" leds.");
    #endif // DIAGNOSE_BLADES
    run_ = true;
    CommandParser::Link();
    Looper::Link();
    AbstractBlade::Activate();
  }

  void Deactivate() override {
    Power(false);
    CommandParser::Unlink();
    Looper::Unlink();
    AbstractBlade::Deactivate();
  }
  // BladeBase implementation
  int num_leds() const override {
    return pin_->num_leds();
  }
  Color8::Byteorder get_byteorder() const {
    return pin_->get_byteorder();
  }
  bool is_on() const override {
    return on_;
  }
  bool is_powered() const override {
    return powered_;
  }
  void set(int led, Color16 c) override {
    Color16* pos = colors_ + led;
    if (pos >= color_buffer + NELEM(color_buffer)) pos -= NELEM(color_buffer);
    *pos = c;
  }
  void allow_disable() override {
    if (!on_) allow_disable_ = true;
  }
  void SetStyle(BladeStyle* style) override{
    AbstractBlade::SetStyle(style);
    run_ = true;
  }
  BladeStyle* UnSetStyle() override {
    return AbstractBlade::UnSetStyle();
  }
  
  StyleHeart StylesAccepted() override { return StyleHeart::_4pixel; } 

  // SaberBase implementation.
  void SB_IsOn(bool* on) override {
    if (on_ || powered_) *on = true;
  }
  void SB_On() override {
    AbstractBlade::SB_On();
    run_ = true;
    on_ = true;
    power_off_requested_ = false;
  }
  void SB_Effect2(BladeEffectType type, float location) override {
    AbstractBlade::SB_Effect2(type, location);
    run_ = true;
    power_off_requested_ = false;
  }
  void SB_Off(OffType off_type) override {
    AbstractBlade::SB_Off(off_type);
    on_ = false;
    if (off_type == OFF_IDLE) {
      power_off_requested_ = true;
    }
  }

  void SB_Top(uint64_t total_cycles) override {
    STDOUT.print("blade fps: ");
    loop_counter_.Print();
    STDOUT.println("");
  }

  bool Parse(const char* cmd, const char* arg) override {
    #if defined(X_LIGHTTEST) 
      if (!strcmp(cmd, "pblade")) {
        if (!strcmp(arg, "on")) {
          manual_ = false;
          SB_On();
          return true;
        }
        if (!strcmp(arg, "off")) {
          manual_ = false;
          SB_Off(OFF_NORMAL);
          return true;
        }
      // Not on or off, maybe color?
      char initial_arg[20];
      for(uint8_t i=0; i<strlen(arg); i++) initial_arg[i] = arg[i];
      char* token = strtok(initial_arg, ",");
      if (!token) return false;
      uint16_t Red = atoi(token) << 8;   // Extract R (16 bit)
      token = strtok(NULL, ",");
      if (!token) return false;      
      uint16_t Green= atoi(token) << 8;   // Extract G  (16 bit)
      token = strtok(NULL, ",");
      if (!token) return false;      
      uint16_t Blue= atoi(token) << 8;   // Extract B (16 bit)     

      // Fill color
      manual_ = true;      
      Color16 fillColor(Red, Green, Blue);
      uint8_t i, N;
      N = pin_->num_leds();
      pin_->Enable(true);
      colors_ = pin_->BeginFrame();
      for (i = 0; i < N; i++) 
        set(i, fillColor);
      while (!pin_->IsReadyForEndFrame()) ProffieOS_yield();
      pin_->EndFrame();
      return true;
      }
    #endif
    return false;
  }

  void Help() override {
    #if defined(COMMANDS_HELP) 
    STDOUT.println(" pblade on/off - turn pixel blade(s) on/off");
    #endif
  }
  void PowerOff() {
    if (!poweroff_delay_start_) {
      poweroff_delay_start_ = millis();
    }
    if (millis() - poweroff_delay_start_ < poweroff_delay_ms_) {
      return;
    }
    Power(false);
    run_ = false;
    power_off_requested_ = false;
  }

#define BLADE_YIELD() do {			\
  YIELD();					\
  /* If Power() was called.... */		\
  if (current_blade != this) goto retry;	\
} while(0)
			     
protected:
  void Loop() override {
    STATE_MACHINE_BEGIN();
    while (true) {
    retry:
      if (current_blade == this) current_blade = NULL;
      YIELD();
      if (manual_) {
        allow_disable_ = false;
        continue;
      }
      if (!current_style_ || !run_) {
	loop_counter_.Reset();
	continue;
      }
      // Wait until it's our turn.
      if (current_blade) {
	continue;
      }
      current_blade = this;
      if (power_off_requested_) {
	PowerOff();
	continue;
      }

      // Update pixels
      while (!pin_->IsReadyForBeginFrame()) BLADE_YIELD();
      colors_ = pin_->BeginFrame();
      
      allow_disable_ = false;
      current_style_->run(this);

      if (!powered_) {
	if (allow_disable_) continue;
	Power(true);
      }

      while (!pin_->IsReadyForEndFrame()) BLADE_YIELD();
      pin_->EndFrame();
      loop_counter_.Update();

#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
      RequestPower();
#endif

      if (powered_ && allow_disable_) {
	PowerOff();
	run_ = false;
      }
    }
    STATE_MACHINE_END();
  }

  
private:
  // Loop should run.
  bool run_ = false;
  bool manual_ = false;   // don't update pixel map from style, it's under manual control
  // Blade is "on"
  bool on_ = false;
  // Blade has power
  bool powered_ = false;
  // Style has indicated that it's ok to shutd down until the next wakeup event.
  bool allow_disable_ = false;
  // We should power off and stop running, even if the blade is on.
  bool power_off_requested_ = false;
  uint32_t poweroff_delay_ms_;
  uint32_t poweroff_delay_start_ = 0;
  LoopCounter loop_counter_;
  StateMachineState state_machine_;
  PowerPinInterface* power_;
  WS2811PIN* pin_;
  Color16* colors_;
};


#define WS2811_RGB      0       // The WS2811 datasheet documents this way
#define WS2811_RBG      1
#define WS2811_GRB      2       // Most LED strips are wired this way
#define WS2811_GBR      3

#define WS2811_800kHz 0x00      // Nearly all WS2811 are 800 kHz
#define WS2811_400kHz 0x10      // Adafruit's Flora Pixels
#define WS2813_800kHz 0x20      // WS2813 are close to 800 kHz but has 300 us frame set delay
#define WS2811_580kHz 0x30      // PL9823
#define WS2811_ACTUALLY_800kHz 0x40      // Normally we use 740kHz instead of 800, this uses 800.

constexpr Color8::Byteorder ByteOrderFromFlags(int CONFIG) {
  return
    (CONFIG & 0xf) == WS2811_RGB ? Color8::RGB :
    (CONFIG & 0xf) == WS2811_RBG ? Color8::RBG :
    (CONFIG & 0xf) == WS2811_GRB ? Color8::GRB :
    (CONFIG & 0xf) == WS2811_GBR ? Color8::GBR :
    Color8::BGR;
}

constexpr int FrequencyFromFlags(int CONFIG) {
  return
    (CONFIG & 0xf0) == WS2811_800kHz ? 740000 :
    (CONFIG & 0xf0) == WS2811_400kHz ? 400000 :
    (CONFIG & 0xf0) == WS2811_580kHz ? 580000 :
    800000;
}

// template<int LEDS, int CONFIG, int DATA_PIN = bladePin, class POWER_PINS = PowerPINS<bladePowerPin1, bladePowerPin2, bladePowerPin3>,
template<int LEDS, int CONFIG, int DATA_PIN = bladePin, 
#ifdef PROFFIEBOARD
class POWER_PINS = PowerPINS<bladePowerPin1, bladePowerPin2, bladePowerPin3>,
#else 
class POWER_PINS = PowerPINS<>,
#endif
  template<int, int, Color8::Byteorder, int, int, int, int> class PinClass = DefaultPinClass,
  int reset_us=300, int t0h=294, int t1h=892,
  int POWER_OFF_DELAY_MS=3000>
class BladeBase *WS2811BladePtr() {
  static_assert(LEDS <= maxLedsPerStrip, "update maxLedsPerStrip");
  static POWER_PINS power_pins;
  static PinClass<LEDS, DATA_PIN, ByteOrderFromFlags(CONFIG), FrequencyFromFlags(CONFIG), reset_us, t0h, t1h> pin;
  static WS2811_Blade blade(&pin, &power_pins, POWER_OFF_DELAY_MS);
  return &blade;
}

template<int LEDS,
          int DATA_PIN = bladePin,
          Color8::Byteorder byteorder,
			#ifdef PROFFIEBOARD
			class POWER_PINS = PowerPINS<bladePowerPin1, bladePowerPin2, bladePowerPin3>,
			#else 
			class POWER_PINS = PowerPINS<>,
			#endif
          template<int, int, Color8::Byteorder, int, int, int, int> class PinClass = DefaultPinClass,
          int frequency=800000, int reset_us=300, int t0h=294, int t1h=892,
          int POWER_OFF_DELAY_MS = 3000>
class BladeBase *WS281XBladePtr() {
  static POWER_PINS power_pins;
  static PinClass<LEDS, DATA_PIN, byteorder, frequency, reset_us, t0h, t1h> pin;
  static WS2811_Blade blade(&pin, &power_pins, POWER_OFF_DELAY_MS);
  return &blade;
}




#endif  // ENABLE_WS2811

#endif
