#ifndef TRANSITIONS_DOEFFECT_H
#define TRANSITIONS_DOEFFECT_H

#include "base.h"
#include "../sound/hybrid_font.h"

// Usage: TrDoEffectX<TRANSITION, EFFECT, WAVNUM, LOCATION_LOCASS, WHEN_OFF>
// or: TrDoEffect<TRANSITION, EFFECT, WAVNUM, LOCATION, WHEN_OFF>
// TRANSITION: TRANSITION
// EFFECT: effect type
// WAVNUM, LOCATION: a number
// LOCATION_CLASS: INTEGER
// WHEN_OFF: true or false
// return value: TRANSITION
// Runs the specified TRANSITION and triggers EFFECT
// Can specify WAV file to use for EFFECT with WAVNUM
// NOTE: 0 is first wav file, -1 is random wav
// LOCATION = -1 is random
// If WHEN_OFF is false, TrDoEffect will do nothing if the blade is off
// and will also force the transition to stop when off.

template<class TRANSITION, BladeEffectType EFFECT, class WAVNUM = Int<-1>, class LOCATION = Int<-1>, bool WHEN_OFF=false>
class TrDoEffectX : public TRANSITION {
public:
  void begin() {
    TRANSITION::begin();
    begin_ = true;
    done_ = false;
  }
  void run(BladeBase* blade) {
    TRANSITION::run(blade);
    wavnum_.run(blade);
    location_.run(blade);
    float location = location_.calculate(blade);
    if (location == -1) location = random(32768) / 32768.0;
    if (begin_) {
      if (blade->is_on()) {
        int wavnum = wavnum_.calculate(blade);
        SaberBase::DoEffect(EFFECT, location, wavnum);
      }
      begin_ = false;
    }
    if (!done_) {
      if (!blade->is_on() && !blade->is_powered()) {
	done_ = true;
      }
    }
  }

  bool done() { return done_ || TRANSITION::done(); }

  template<class A, class B>
  auto getColor(const A& a, const B& b, int led) -> decltype(TRANSITION::getColor(a, b, led)) {
    if (done_) return b;
    return TRANSITION::getColor(a, b, led);
  }  

 private:
  PONUA SVFWrapper<WAVNUM> wavnum_;
  PONUA SVFWrapper<LOCATION> location_;
  bool begin_;
  bool done_;
};

template<class TRANSITION, BladeEffectType EFFECT, class WAVNUM, class LOCATION>
class TrDoEffectX<TRANSITION, EFFECT, WAVNUM, LOCATION, true>  : public TRANSITION {
public:
  void begin() {
    TRANSITION::begin();
    begin_ = true;
  }
  void run(BladeBase* blade) {
    TRANSITION::run(blade);
    wavnum_.run(blade);
    location_.run(blade);
    float location = location_.calculate(blade);
    if (location == -1) location = random(32768) / 32768.0;
    if (begin_) {
      int wavnum = wavnum_.calculate(blade);
      SaberBase::DoEffect(EFFECT, location, wavnum);
      begin_ = false;
    }
  }

 private:
  PONUA SVFWrapper<WAVNUM> wavnum_;
  PONUA SVFWrapper<LOCATION> location_;
  bool begin_;
};

template<class TRANSITION, BladeEffectType EFFECT, int WAVNUM = -1, int LOCATION = -1, bool WHEN_OFF = false> using TrDoEffect = TrDoEffectX<TRANSITION, EFFECT, Int<WAVNUM>, Int<LOCATION>, WHEN_OFF>;

#endif
