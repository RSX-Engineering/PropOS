#ifndef BLADES_SIMPLE_BLADE_H
#define BLADES_SIMPLE_BLADE_H

#include "abstract_blade.h"

#include "../styles/styles.h"


// Simple blade, LED string or LED star with optional flash on clash.
// Note that this class does nothing when first constructed. It only starts
// interacting with pins and timers after Activate() is called.
class Simple_Blade : public AbstractBlade, CommandParser, Looper {
public:
  Simple_Blade() : AbstractBlade(), CommandParser(NOLINK), Looper(NOLINK) {
      nLEDs = 0;
      for (uint8_t i=0; i<ALED_MAXCH; i++)
        leds[i] = 0;
    }

  // Assign an xAnalogLED object to the blade. Order of LEDs is determined by the order of AssignLED() calls
  bool AssignLED(xAnalogLED* newLed) {  // __attribute__((optimize("Og"))) {      
      if (nLEDs == ALED_MAXCH ) return false;   // all possible LEDs are already assigned
      if (leds[nLEDs]) return false;      // LED already assigned (should not happen!)
      leds[nLEDs++] = newLed;
      // STDOUT.print("[SimpleBlade.AssignLED]: Assign LED with "); STDOUT.print(newLed->nCh); STDOUT.print(" channels as #"); STDOUT.print(nLEDs); STDOUT.println(":");
      // for (uint8_t i=0; i<newLed->nCh; i++) {
      //   STDOUT.print(" * Channel #"); STDOUT.print(i); STDOUT.print(": "); newLed->channels[i]->Raport(); STDOUT.println("");
      // }
      return true;
  }




  
  const char* name() override { return "Simple_Blade"; }

  void Activate() override {
    #ifdef DIAGNOSE_BLADES
      STDOUT.print("Activating analog blade with ");
      STDOUT.print(nLEDs);
      if (nLEDs>1)
        STDOUT.println(" leds.");
      else 
        STDOUT.println(" leds");
    #endif // DIAGNOSE_BLADES
    Power(true);
    CommandParser::Link();
    Looper::Link();
    AbstractBlade::Activate();
  }

  void Deactivate() override {
    Power(false);
    AbstractBlade::Deactivate();
    Looper::Unlink();
    CommandParser::Unlink();
  }

  void Power(bool on) {
    if (power_ != on && nLEDs) {
      if (on) for (uint8_t i=0; i<nLEDs; i++)
          leds[i]->Activate();
      else for (uint8_t i=0; i<nLEDs; i++)
          leds[i]->Deactivate();
      power_ = on;
    }
  }

  // BladeBase implementation
  int num_leds() const override {
    return nLEDs;   // some styles expect number of distinct LED modules, not number of analog LEDs !!!
  }

  Color8::Byteorder get_byteorder() const override {
      return Color8::RGB;
  }

  bool is_on() const override {
    return on_;
  }
  bool is_powered() const override {
    return power_;
  }
  
  void set(int led, Color16 c) override { // __attribute__((optimize("Og"))) {
    uint32_t hexRGB = c.r << 8;   // 8-bit R
    hexRGB &= 0xFF0000;
    hexRGB += c.g;              // 8-bit G
    hexRGB &= 0xFFFF00;
    hexRGB += (c.b >> 8);      // 8-bit B
    // STDOUT.print("[SimpleBlade.set()]: set led "); STDOUT.print(led); STDOUT.print(" to color16 ");  
    // STDOUT.print(c.r); STDOUT.print(", "); STDOUT.print(c.g); STDOUT.print(", "); STDOUT.println(c.b); // STDOUT.print(" = ");  STDOUT.println(hexRGB); 
    
    leds[led]->Set(hexRGB);


  }

  void set_overdrive(int led, Color16 c) override {
    // leds_[led]->set_overdrive(c);
    set(led, c);
  }

  

  void allow_disable() override {
    if (!on_) Power(false);
  }
  virtual void SetStyle(BladeStyle* style) {
    Power(true);
    AbstractBlade::SetStyle(style);
  }
    StyleHeart StylesAccepted() override { return StyleHeart::_4analog; }  
  // SaberBase implementation
  void SB_IsOn(bool *on) override {
    if (on_ || power_) *on = true;
  }
  void SB_On() override {
    AbstractBlade::SB_On();
    // battery_monitor.SetLoad(true);
    on_ = true;
    Power(true);
  }
  void SB_Effect2(BladeEffectType type, float location) override {
    AbstractBlade::SB_Effect2(type, location);
    // battery_monitor.SetLoad(true);
    Power(true);
  }
  void SB_Off(OffType off_type) override {
    AbstractBlade::SB_Off(off_type);
    // battery_monitor.SetLoad(false);
    on_ = false;
    if (off_type == OFF_IDLE) {
      Power(false);
    }
  }

  bool Parse(const char* cmd, const char* arg) override {
    
    #if defined(X_LIGHTTEST) 
      if (!strcmp(cmd, "ablade")) {
        if (!strcmp(arg, "on")) {
          SB_On();
          return true;
        }
        if (!strcmp(arg, "off")) {
          SB_Off(OFF_NORMAL);
          return true;
        }
      }
    #endif
  

  #if defined(X_LIGHTTEST)
    // parse "aled_pintest" on all analog led channels
    uint8_t ledcnt = nLEDs;
    uint8_t chcnt;
    while (ledcnt) {
        chcnt = leds[--ledcnt]->nCh;
        while (chcnt) 
          if (leds[ledcnt]->channels[--chcnt]->Parse(cmd, arg)) return true;      
    }

    // parse "aled_chtest" on first analog led
    if (nLEDs) 
      if (leds[0]->Parse(cmd, arg)) return true;  
  #endif

    return false;

  }

  void Help() override {
#if defined(COMMANDS_HELP) 
    STDOUT.println(" ablade on/off - turn analog blade on/off");
  #if defined(X_LIGHTTEST)
    STDOUT.println(" aled_pintest <pin>, <brightness> - analog LED test: apply brightness [0, 65535] to LED pin [1, 6]");     
    STDOUT.println(" aled_chtest <channel>, <R>, <G>, <B> - analog LED test: apply RGB color [0, 255] to channel number [1, 4]");     
  #endif
 #endif

  }

protected:
  void Loop() override {
    if (!power_) return;
    // Make sure the booster is running so we don't get low voltage
    // and under-drive any FETs.
    EnableBooster();
    if (current_style_)
      current_style_->run(this);
  }

private:
  uint8_t nLEDs;    // number of single or multi channel Analog LEDs
  xAnalogLED* leds[ALED_MAXCH];  // Pointers to Analog LEDs

  bool on_ = false;
  bool power_ = false;
};




#endif
