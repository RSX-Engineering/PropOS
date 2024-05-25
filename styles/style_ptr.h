#ifndef STYLES_STYLE_PTR_H
#define STYLES_STYLE_PTR_H

#include "blade_style.h"

// Usage: StylePtr<BLADE>
// BLADE: COLOR
// return value: suitable for preset array
// Most blade styls are created by taking a blade style template and wrapping it
// this class, which implements the BladeStyle interface. We do this so that the
// getColor calls will be inlined in this loop for speed.

struct HandledTypeResetter {
  HandledTypeResetter() { BladeBase::ResetHandledTypes(); }
};

struct HandledTypeSaver {
  HandledTypeSaver() { handled_features_ = BladeBase::GetHandledTypes(); }
  bool IsHandled(HandledFeature feature) {
    return (handled_features_ & feature) != 0;
  }
  HandledFeature handled_features_;
};

class StyleBase : public BladeStyle {
protected:
  void activate() override { }
  HandledTypeResetter handled_type_resetter_;
};

template<class RetType>
class StyleHelper : public StyleBase {
public:
  virtual RetType getColor2(int i) = 0;
  OverDriveColor getColor(int i) override { return getColor2(i); }


  void runloop(BladeBase* blade) {

    int num_leds = blade->num_leds();
    // int rotation = (SaberBase::GetCurrentVariation() & 0x7fff) * 3;
    int rotation = (SaberBase::GetCurrentVariation() & 0x7fff); // 0-32767
    for (int i = 0; i < num_leds; i++) {
      RetType c = getColor2(i);
      // c.c = c.c.rotate(rotation);
      c.c = c.c.rotate(SaberBase::GetCurrentRotation(), SaberBase::GetCurrentDesaturation());

      // scale with masterBrightness [0, 65535]
      uint32_t tmp = c.c.r * userProfile.masterBrightness;
      c.c.r = tmp >> 16;
      tmp = c.c.g * userProfile.masterBrightness;
      c.c.g = tmp >> 16;
      tmp = c.c.b * userProfile.masterBrightness;
      c.c.b = tmp >> 16;
      // Apply color
      if (c.getOverdrive()) blade->set_overdrive(i, c.c);
      else  blade->set(i, c.c);      
    }

  }
};

template<class T>
class Style : public StyleHelper<decltype(T().getColor(0))> {
public:
  bool IsHandled(HandledFeature effect) override {
    return handled_type_saver_.IsHandled(effect);
  }

  virtual auto getColor2(int i) -> decltype(T().getColor(0)) override {
    return base_.getColor(i);
  }

  void run(BladeBase* blade) override {
    if (!RunStyle(&base_, blade))
      blade->allow_disable();
    this->runloop(blade);
  }
private:
  T base_;
  HandledTypeSaver handled_type_saver_;
};

// Get a pointer to class.
template<class STYLE>
StyleAllocator StylePtr() {
  static StyleFactoryImpl<Style<STYLE> > factory;
  return &factory;
};

  
#endif
