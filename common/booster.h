#ifndef COMMON_BOOSTER_H
#define COMMON_BOOSTER_H

#include "battery_monitor.h"

#if defined(PROFFIEBOARD) || defined(ULTRAPROFFIE_BOOSTER) 

// Turns off booster when we don't need it anymore.
// class Booster : Looper, StateMachine, CommandParser 
class Booster : Looper, StateMachine, CommandParser {
public:
  Booster() : Looper(), CommandParser() {}



  const char* name() override { return "Booster"; }

  bool Active() {
#ifdef ENABLE_AUDIO    
    if (amplifier.Active()) return true;
#endif    
    uint32_t t = millis() - last_enabled_;
    if (t < 10000) return true;
    if (prop.NeedsPower()) return true;
    bool on = false;
    //SaberBase::DoIsOn(&on); was , but for some reason it fails 
    on = SaberBase::IsOn();
    return on;
  }

  void Enable() {
    last_enabled_ = millis();
    if (!on_) {
      battery_monitor.SetPinHigh(true);
      // pinMode(boosterPin, OUTPUT);
      // digitalWrite(boosterPin, HIGH);
      on_ = true;
      delay(10); // Give it a little time to wake up.
      battery_monitor.SetPinHigh(false);
    }
  }

protected:
  void Setup() override {
    on_ = false;
    // pinMode(boosterPin, OUTPUT);
    // digitalWrite(boosterPin, LOW); // turn the booster off
    last_enabled_ = millis();
  }

  void Loop() override {
    STATE_MACHINE_BEGIN();
    while (true) {
      while (Active())
      {
        YIELD();
      }
      // 10 * 0.1 s = 1 second
      for (i_ = 0; i_ < 10 && !Active(); i_++)
	SLEEP(100);
      if (Active()) continue;
      #if defined(DIAGNOSE_AUDIO)
      STDOUT.println("Booster off.");
      #endif
      // digitalWrite(boosterPin, LOW); // turn the booster off
      // // pinMode(amplifierPin, INPUT_ANALOG); // Let the pull-down do the work
      on_ = false;
      while (!Active()) YIELD();
    }
    STATE_MACHINE_END();
  }

  bool Parse(const char *cmd, const char* arg) override {
  #ifdef ENABLE_DEVELOPER_COMMANDS
    if (!strcmp(cmd, "booster")) {
      if (!strcmp(arg, "on")) {
	      Enable();
        return true;
      }
      if (!strcmp(arg, "off")) {
        // digitalWrite(boosterPin, LOW); // turn the booster off
        return true;
      }
    }
  #endif // ENABLE_DEVELOPER_COMMANDS
    return false;
  }

  void Help() {
    #if defined(COMMANDS_HELP) 
    STDOUT.println(" booster on/off - turn booster on or off");
    #endif
  }



private:
  bool on_;
  int i_;
  uint32_t last_enabled_;
};

Booster booster;

inline void EnableBooster() {
  booster.Enable();
}
#ifdef ULTRAPROFFIE
void SilentEnableBooster(bool on) {
  // if(on)
  //       digitalWrite(boosterPin, HIGH);
  // else 
  //       digitalWrite(boosterPin, LOW);
}
#endif

#else
  inline void EnableBooster() { }
#endif  

#endif
