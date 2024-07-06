#ifndef PROPS_PROP_BASE_H
#define PROPS_PROP_BASE_H

#ifndef PROP_INHERIT_PREFIX
#define PROP_INHERIT_PREFIX
#endif

#include "TTmenu.h"
#ifdef OSX_ENABLE_MTP
  #include "../common/serial.h"
#endif 







struct SoundToPlay {
  const char* filename_;
  Effect* effect_;
  int selection_;

  SoundToPlay() :filename_(nullptr), effect_(nullptr) {}
  explicit SoundToPlay(const char* file) : filename_(file){  }
  SoundToPlay(Effect* effect, int selection = -1) : filename_(nullptr), effect_(effect), selection_(selection) {}
  bool Play(BufferedWavPlayer* player) {
     if (filename_) return player->PlayInCurrentDir(filename_);
     effect_->Select(selection_);
     player->PlayOnce(effect_);
     return true;
   }
   bool isSet() {
      return filename_ != nullptr || effect_ != nullptr;
   }
};



// Base class for props.
#ifdef ULTRAPROFFIE
class PropBase : CommandParser, Looper, public PowerSubscriber, protected SaberBase {
public:
void PwrOn_Callback() override { 
    #ifdef DIAGNOSE_POWER
      STDOUT.println(" cpu+ "); 
    #endif
  }
  void PwrOff_Callback() override { 
    #ifdef DIAGNOSE_POWER
      STDOUT.println(" cpu- "); 
    #endif
  }
#else  
class PropBase : CommandParser, Looper, protected SaberBase {
#endif 

public: 
  TTMenu<uint16_t>* menu;    // if a menu is assigned to prop, events will be redirected to menu.Event()
  #ifdef ULTRAPROFFIE
    PropBase() : CommandParser(), Looper(), PowerSubscriber(pwr4_CPU) { menu=0; accResultant = 0;}
  #else 
    //TTMenu<uint16_t>* menu;    // if a menu is assigned to prop, events will be redirected to menu.Event()
    PropBase() : CommandParser() { menu=0; accResultant = 0;}
  #endif


  BladeStyle* current_style() {
#if NUM_BLADES == 0
    return nullptr;
#else
    if (!current_config->blade1) return nullptr;
    return current_config->blade1->current_style();
#endif
  }

  const char* current_preset_name() {
          return current_preset_->name;
      }

#ifdef ULTRAPROFFIE
  bool HoldPower() override {  // Return true to pause power subscriber timeout
    if (IsOn()) return true;
    if (current_style() && current_style()->NoOnOff()) return true;
    return false;
  }
  inline bool NeedsPower() { return HoldPower(); }
#else 
  bool NeedsPower() {
    if (SaberBase::IsOn()) return true;
    if (current_style() && current_style()->NoOnOff())
      return true;
    return false;
  }
#endif


  int32_t muted_volume_ = 0;
  bool SetMute(bool muted) {
#ifdef ENABLE_AUDIO
    if (muted) {
      if (dynamic_mixer.get_volume()) {
        muted_volume_ = dynamic_mixer.get_volume();
        dynamic_mixer.set_volume(0);
        #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
          SilentEnableAmplifier(false);
          SilentEnableBooster(false);
        #endif
        return true;
      }
    } else {
      if (muted_volume_) {
        dynamic_mixer.set_volume(muted_volume_);
        muted_volume_ = 0;
        #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
          SilentEnableAmplifier(true);
          SilentEnableBooster(true);
        #endif
        return true;
      }
    }
#endif
    return false;
  }

  bool unmute_on_deactivation_ = false;
  uint32_t activated_ = 0;
  uint32_t last_clash_ = 0;
  uint32_t clash_timeout_ = 100;

  bool clash_pending_ = false;
  int8_t pending_clash_is_stab_ = 0;
  float pending_clash_strength_ = 0.0;

  bool on_pending_ = false;

  virtual bool IsOn() {
    return SaberBase::IsOn() || on_pending_;
  }

  virtual void On() {
#ifdef ENABLE_AUDIO
    if (!CommonIgnition()) return;
    SaberBase::DoPreOn();
    on_pending_ = true;
    // Hybrid font will call SaberBase::TurnOn() for us.
#else
    // No sound means no preon.
    FastOn();
#endif    
  }


  void FastOn() {
    if (!CommonIgnition()) return;
    SaberBase::TurnOn();
    SaberBase::DoEffect(EFFECT_FAST_ON, 0);
  }


  void SB_On() override {
    on_pending_ = false;
  }

  virtual void Off(OffType off_type = OFF_NORMAL) {
    if (on_pending_) {
      // Or is it better to wait until we turn on, and then turn off?
      on_pending_ = false;
      SaberBase::TurnOff(SaberBase::OFF_CANCEL_PREON);
      return;
    }
    if (!SaberBase::IsOn()) return;
    if (SaberBase::Lockup()) {
      SaberBase::DoEndLockup();
      SaberBase::SetLockup(SaberBase::LOCKUP_NONE);
    }
    SaberBase::TurnOff(off_type);
    if (unmute_on_deactivation_) {
      unmute_on_deactivation_ = false;
#ifdef ENABLE_AUDIO
      // We may also need to stop any thing else that generates noise..
      for (size_t i = 0; i < NELEM(wav_players); i++) {
        wav_players[i].Stop();
      }
#endif
      SetMute(false);
    }
  }

  float GetCurrentClashThreshold() { return CLASH_THRESHOLD_G; }

  void IgnoreClash(size_t ms) {
    if (clash_pending_) return;
    uint32_t now = millis();
    uint32_t time_since_last_clash = now - last_clash_;
    if (time_since_last_clash < clash_timeout_) {
      ms = std::max<size_t>(ms, clash_timeout_ - time_since_last_clash);
    }
    last_clash_ = now;
    clash_timeout_ = ms;
  }

  virtual void Clash2(int8_t stab, float strength) {
    SaberBase::SetClashStrength(strength);
    #ifdef DIAGNOSE_PROP
        if (stab==1) STDOUT.println("STAB detected!");
        else  if (stab==-1) STDOUT.println("REV STAB detected!");
              else STDOUT.println("CLASH detected!");
    #endif // DIAGNOSE_PROP
            
    if (Event(BUTTON_NONE, stab ? EVENT_STAB : EVENT_CLASH)) {
      IgnoreClash(400);
    } else {
      IgnoreClash(200);
      // Saber must be on and not in lockup mode for stab/clash.
      if (SaberBase::IsOn() && !SaberBase::Lockup()) {
        if (stab) {
          // SaberBase::DoStab();     // Not here! This is part of the prop.
        } else {
          SaberBase::DoClash();
          // STDOUT.print("Orientation = "); STDOUT.println(fusor.angle1());
        }
      }
    }
  }
  virtual void Clash(int8_t stab, float strength) {   // stab=-1 => reversed stab
    // TODO: Pick clash randomly and/or based on strength of clash.
    uint32_t t = millis();
    if (t - last_clash_ < clash_timeout_) {
      if (clash_pending_) {
        pending_clash_strength_ = std::max<float>(pending_clash_strength_, strength);
      } else {
        SaberBase::UpdateClashStrength(strength);
      }
      last_clash_ = t; // Vibration cancellation
      return;
    }
    if (current_modifiers & ~MODE_ON) {
      // Some button is pressed, that means that we need to delay the clash a little
      // to see if was caused by a button *release*.
      last_clash_ = millis();
      clash_timeout_ = 3;
      clash_pending_ = true;
      pending_clash_is_stab_ = stab;
      pending_clash_strength_ = strength;
      return;
    }
    Clash2(stab, strength);
  }

  virtual bool chdir(const char* dir) {
    if (strlen(dir) > 1 && dir[strlen(dir)-1] == '/') {
      STDOUT.println("Directory must not end with slash.");
      return false;
    }
#ifdef ENABLE_AUDIO
    smooth_swing_v2.Deactivate();
    looped_swing_wrapper.Deactivate();
    hybrid_font.Deactivate();

    // Stop all sound!
    // TODO: Move scanning to wav-playing interrupt level so we can
    // interleave things without worry about memory corruption.
    for (size_t i = 0; i < NELEM(wav_players); i++) {
      wav_players[i].Stop();
    }
#endif

    char *b = current_directory;
    for (const char *a = dir; *a; a++) {
      // Skip trailing slash
      if (*a == '/' && (a[1] == 0 || a[1] == ';'))
        continue;
      if (*a == ';') {
        *(b++) = 0;
        continue;
      }
      *(b++) = *a;
    }
    // Two zeroes at end!
    *(b++) = 0;
    *(b++) = 0;

#ifdef ENABLE_AUDIO
    Effect::ScanCurrentDirectory();
    SaberBase* font = NULL;
    hybrid_font.Activate();
    font = &hybrid_font;
    if (font) {
      smooth_swing_config.ReadInCurrentDir("smoothsw.ini");
      if (SFX_lswing) {
        smooth_swing_cfx_config.ReadInCurrentDir("font_config.txt");
        // map CFX values to Proffie (sourced from font_config.txt in font folder)
        smooth_swing_config.SwingSensitivity = smooth_swing_cfx_config.smooth_sens;
        smooth_swing_config.MaximumHumDucking = smooth_swing_cfx_config.smooth_dampen;
        smooth_swing_config.SwingSharpness = smooth_swing_cfx_config.smooth_sharp;
        smooth_swing_config.SwingStrengthThreshold = smooth_swing_cfx_config.smooth_gate;
        smooth_swing_config.Transition1Degrees = smooth_swing_cfx_config.smooth_width1;
        smooth_swing_config.Transition2Degrees = smooth_swing_cfx_config.smooth_width2;
        smooth_swing_config.MaxSwingVolume = smooth_swing_cfx_config.smooth_gain * 3 / 100;
        smooth_swing_config.AccentSwingSpeedThreshold = smooth_swing_cfx_config.hswing;
        smooth_swing_config.Version = 2;
      } else if (!SFX_swingl) {
        smooth_swing_config.Version = 0;
      }
      smooth_swing_config.ApplySensitivity(0);  // store unscaled parameters
      smooth_swing_config.ApplySensitivity(&userProfile.swingSensitivity);  // scale parameters with sensitivity
      switch (smooth_swing_config.Version) {
        case 1:
          looped_swing_wrapper.Activate(font);
          break;
        case 2:
          smooth_swing_v2.Activate(font);
          break;
      }
    }
//    EnableBooster();
#endif // ENABLE_AUDIO
    return false;
  }


  bool BladeOff(bool silent=false) {
#ifdef IDLE_OFF_TIME
    last_on_time_ = millis();
#endif
    bool on = IsOn();
    if (on) {
      if (silent) Off(SILENT_OFF);  // no off
      else Off();
    }
    return on;
  }



  void FreeBladeStyles() {  
    BladeStyle* st;    
    #define UNSET_BLADE_STYLE(N) {  st = current_config->blade##N->UnSetStyle();  if (st) delete st; }
    ONCEPERBLADE(UNSET_BLADE_STYLE)
    // STDOUT.println("[FreeBladeStyles]");



  }
  
  void AllocateBladeStyles() {
      for (uint8_t i=0; i<installConfig.nBlades; i++){
        #ifdef DIAGNOSE_PRESETS
          STDOUT.print("Allocating style '"); STDOUT.print(current_preset_->bladeStyle[i]->name); 
          STDOUT.print("' to blade #"); STDOUT.println(i+1);
        #endif
        BladeBase* bladePtr = BladeAddress(i+1);
        if (bladePtr) // make sure there is a blade
        if (current_preset_->bladeStyle[i])   // make sure a style is assigned
        if (current_preset_->bladeStyle[i]->stylePtr) { // make sure the style maker actually exists
          BladeStyle* tmp = current_preset_->bladeStyle[i]->stylePtr->make();
          bladePtr->SetStyle(tmp);
          // STDOUT.print(", puting style address "); STDOUT.print((uint32_t)tmp);
          // STDOUT.print(", at blade address "); STDOUT.println((uint32_t)bladePtr); 
        }
        else {
          #ifdef DIAGNOSE_PRESETS
              STDOUT.println(" >>>>>>>>>> ERROR: undefined style! <<<<<<<<<<   ");
          #endif
        }            
      }


  }


  private:
    // presetIndex starts from 0 
    void ChangePreset(uint8_t presetIndex) {
      #ifdef DIAGNOSE_PRESETS
        STDOUT.print("Change preset to "); STDOUT.print(presetIndex+1);   // report presets as 1, 2..., even if the actual index starts from 0
        STDOUT.print(" / "); STDOUT.print(presets.size());  STDOUT.print(". ");
      #endif
      presetIndex = presetIndex % presets.size();         // circular indexing
      FreeBladeStyles();                               // delete old styles
      if (presets.size()) {
        current_preset_ = presets.data() + presetIndex;     // set new preset
        #ifdef DIAGNOSE_PRESETS
          STDOUT.print("Preset '"); STDOUT.print(current_preset_->name); STDOUT.println("' activated.");
        #endif
        AllocateBladeStyles();                              // create new styles
      }
    #ifdef DIAGNOSE_PRESETS
      else STDOUT.println("Nothing to change.");
    #endif
      
      SaberBase::SetVariation(current_preset_->variation); // update variation
      chdir(current_preset_->font);                       // change font
      userProfile.preset = presetIndex+1;                 // set current preset in user profile
    }
  
  public:
    // preset_num starts at 1!
    virtual void SetPreset(int preset_num, bool announce) {
      if (!preset_num) return;    // presets are numbered from 1; 0 means preset error
      bool on = BladeOff();
      ChangePreset(preset_num-1);   
      if (on) On(); 
        else if (announce) {  // don't announce if you just did on!
        SaberBase::DoNewFont();
      }
    }

    // preset_num starts at 1!
    void SetPresetFast(int preset_num, bool announce = true) {
      if (!preset_num) return;    // presets are numbered from 1; 0 means preset error
      bool on = BladeOff(true);   // silent off
      ChangePreset(preset_num-1); 
      if (on) {
        hybrid_font.silentOn = true;
        // FastOn();
        if (!CommonIgnition()) return;
        SaberBase::TurnOn();
        // SaberBase::DoEffect(EFFECT_FAST_ON, 0);
        if (announce) SaberBase::DoNewFont();
        else SaberBase::DoEffect(EFFECT_FAST_ON, 0);      
      }
    }
	
  // Go to the next Preset.
  virtual void next_preset() {
    if (!userProfile.preset) return;    // preset error
    if (++userProfile.preset > presets.size()) userProfile.preset=1;  // circular increment 1..size
    SetPreset(userProfile.preset, true);
  }

  // Go to the next Preset skipping NewFont and Preon effects using FastOn.
  void next_preset_fast() {
    if (!userProfile.preset) return;    // preset error
    if (++userProfile.preset > presets.size()) userProfile.preset=1;  // circular increment 1..size
    SetPresetFast(userProfile.preset);

  }

  // Go to the previous Preset.
  virtual void previous_preset() {
    if (!userProfile.preset) return;    // preset error
    if (!--userProfile.preset) userProfile.preset=presets.size();  // circular decrement 1..size
    SetPreset(userProfile.preset, true); 
  }

  // Go to the previous Preset skipping NewFont and Preon effects using FastOn.
  void previous_preset_fast() {
    if (!userProfile.preset) return;    // preset error
    if (!--userProfile.preset) userProfile.preset=presets.size();  // circular decrement 1..size
    SetPresetFast(userProfile.preset, false); 
  }





  // Called from setup to identify the blade and select the right
  // Blade driver, style and sound font.
  void ActivateBlades() {  
    #define ACTIVATE(N) current_config->blade##N->Activate(); 
    ONCEPERBLADE(ACTIVATE);
  }

  // Potentially called from interrupt!
  virtual void DoMotion(const Vec3& motion, bool clear) {
    fusor.DoMotion(motion, clear);
  }

//  CLASH_THRESHOLD_G already defined
  #define STAB_TH     userProfile.stabSensitivity.threshold   // stab threshold = minimum XYZ acceleration: 
  #define STAB_DIR    userProfile.stabSensitivity.dir // 2         // stab directionality = ratio of X and YZ acceleration: 2.0f


#define STAB_SPEED  150       // stab speed = maximum swing speed: 150
  // Potentially called from interrupt! 
  virtual void DoAccel(const Vec3& accel, bool clear) {
    fusor.DoAccel(accel, clear);
    accel_loop_counter_.Update();
    Vec3 diff = fusor.clash_mss();
    float v;
    if (clear) {
      accel_ = accel;
      diff = Vec3(0,0,0);
      v = 0.0;
    } else {
#ifndef PROFFIEOS_DONT_USE_GYRO_FOR_CLASH
      v = (diff.len() + fusor.gyro_clash_value()) / 2.0;
#else      
      v = diff.len();
#endif      
    }

      accResultant = v;    // Need this for out-of-ISR detections and we're not gonna run sqrt() again!
      // Detect clash
      if (v > CLASH_THRESHOLD_G + fusor.gyro().len() / 200.0) { 
        if (clash_pending1_) {
          pending_clash_strength1_ = std::max<float>(v, (float)pending_clash_strength1_);
        } else {
          clash_pending1_ = true;
          pending_clash_is_stab1_ = false;  // for now, check stab later
          pending_clash_strength1_ = v;
        }
      }      
      // Detect stab (different threshold than clash)
      if (v > STAB_TH && !clash_pending1_) { 
        bool revStab = false;
        if ( (accel_ - fusor.down()).len2() > (accel - fusor.down()).len2() ) {
          diff = -diff;
          revStab = true;
        }
        #if defined(PROFFIEBOARD) || ( defined(ULTRAPROFFIE) && ULTRAPROFFIE_VERSION == 'P')
          bool stab = diff.x < - STAB_DIR * sqrtf(diff.y * diff.y + diff.z * diff.z) && fusor.swing_speed() < STAB_SPEED;
        #else 
          bool stab = (diff.x > STAB_DIR * sqrtf(diff.y * diff.y + diff.z * diff.z)) && (fusor.swing_speed() < STAB_SPEED);
        #endif
        if (stab) {
          // if (revStab) { pending_clash_is_stab1_ = -1; STDOUT.println("-1"); }
          // else { pending_clash_is_stab1_ = 1;  STDOUT.println("1"); }
          if (revStab) pending_clash_is_stab1_ = -1; 
          else pending_clash_is_stab1_ = 1;  
          clash_pending1_ = true;
        }
      }      
    

    accel_ = accel;
  }

  void SB_Top(uint64_t total_cycles) override {
    STDOUT.print("Acceleration measurements per second: ");
    accel_loop_counter_.Print();
    STDOUT.println("");
  }

  enum StrokeType {
    TWIST_CLOSE,
    TWIST_LEFT,
    TWIST_RIGHT,

    SHAKE_CLOSE,
    SHAKE_FWD,
    SHAKE_REW
  };
  struct Stroke {
    StrokeType type;
    uint32_t start_millis;
    uint32_t end_millis;
    uint32_t length() const { return end_millis - start_millis; }
  };

  Stroke strokes[5];

  StrokeType GetStrokeGroup(StrokeType a) {
    switch (a) {
      case TWIST_CLOSE:
      case TWIST_LEFT:
      case TWIST_RIGHT:
        return TWIST_CLOSE;
      case SHAKE_CLOSE:
      case SHAKE_FWD:
      case SHAKE_REW:
        break;
    }
    return SHAKE_CLOSE;
  }

  bool ShouldClose(StrokeType a, StrokeType b) {
    // Don't close if it's the same exact stroke
    if (a == b) return false;
    // Different stroke in same stroke group -> close
    if (GetStrokeGroup(a) == GetStrokeGroup(b)) return true;
    // New stroke in different group -> close
    if (GetStrokeGroup(b) != b) return true;
    return false;
  }

  bool DoGesture(StrokeType gesture) {
    if (gesture == strokes[NELEM(strokes)-1].type) {
      if (strokes[NELEM(strokes)-1].end_millis == 0) {
        // Stroke not done, wait.
        return false;
      }
      if (millis() - strokes[NELEM(strokes)-1].end_millis < 50)  {
        // Stroke continues
        strokes[NELEM(strokes)-1].end_millis = millis();
        return false;
      }
    }
    if (strokes[NELEM(strokes) - 1].end_millis == 0 &&
        GetStrokeGroup(gesture) == GetStrokeGroup(strokes[NELEM(strokes) - 1].type)) {
      strokes[NELEM(strokes) - 1].end_millis = millis();
      return true;
    }
    // Exit here if it's a *_CLOSE stroke.
    if (GetStrokeGroup(gesture) == gesture) return false;
    // If last stroke is very short, just write over it.
    if (strokes[NELEM(strokes)-1].end_millis -
        strokes[NELEM(strokes)-1].start_millis > 10) {
      for (size_t i = 0; i < NELEM(strokes) - 1; i++) {
        strokes[i] = strokes[i+1];
      }
    }
    strokes[NELEM(strokes)-1].type = gesture;
    strokes[NELEM(strokes)-1].start_millis = millis();
    strokes[NELEM(strokes)-1].end_millis = 0;
    return false;
  }

  


  #define TWIST_THRESHOLD   userProfile.twistSensitivity.threshold     // minimum gyro to consider it a stroke
  #define TWIST_DIR         userProfile.twistSensitivity.dir    // minimum directionality (ratio of X/Y and X/Z gyro) to consider it a stroke
  #define TWIST_MINTIME     userProfile.twistSensitivity.minTime    // minimum stroke duration
  #define TWIST_MAXTIME     userProfile.twistSensitivity.maxTime   // maximum stroke duration
  #define TWIST_MAXSEP      200UL   // maximum separation to end stroke. Doesn't do much...
  #define TWIST_QUIETTIME   500   // minimum time between successive twists [ms]

  // Detect double twist
  void DetectTwist() {
    static uint32_t lastTwist=0;    // time when twist was last detected 
    if (millis() - lastTwist < TWIST_QUIETTIME) return;     // have some quiet time so we don't detect multiple twists on the same motion
    Vec3 gyro = fusor.gyro();
    bool process = false;
    if (fabsf(gyro.x) > TWIST_THRESHOLD &&
        fabsf(gyro.x) > TWIST_DIR * abs(gyro.y) &&
        fabsf(gyro.x) > TWIST_DIR * abs(gyro.z)) {
      process = DoGesture(gyro.x > 0 ? TWIST_LEFT : TWIST_RIGHT);
    } else {
      process = DoGesture(TWIST_CLOSE);
    }
    if (process) {
      if ((strokes[NELEM(strokes)-1].type == TWIST_LEFT &&
           strokes[NELEM(strokes)-2].type == TWIST_RIGHT) ||
          (strokes[NELEM(strokes)-1].type == TWIST_RIGHT &&
           strokes[NELEM(strokes)-2].type == TWIST_LEFT)) {
        if (strokes[NELEM(strokes) -1].length() > TWIST_MINTIME &&
            strokes[NELEM(strokes) -1].length() < TWIST_MAXTIME &&
            strokes[NELEM(strokes) -2].length() > TWIST_MINTIME &&
            strokes[NELEM(strokes) -2].length() < TWIST_MAXTIME) {
          uint32_t separation = strokes[NELEM(strokes)-1].start_millis - strokes[NELEM(strokes)-2].end_millis;
          if (separation < TWIST_MAXSEP) {
            // STDOUT.println("TWIST");
            lastTwist = millis();
            // We have a twisting gesture.
            Event(BUTTON_NONE, EVENT_TWIST);
          }
        }
      }
    }
  }


  #define SHAKE_THRESHOLD       userProfile.shakeSensitivity.threshold      // minimum mss resultant to call it a peak: 150
  #define SHAKE_MINDURATION     15      // minimum duration of a peak, to discriminate from vibrations [ms]
  #define SHAKE_MAXPERIOD       userProfile.shakeSensitivity.maxPeriod     // maximum time between peaks, to call them successive [ms]: 
  #define SHAKE_NPEAKS          userProfile.shakeSensitivity.nPeaks      // number of successive peaks to call it a shake
  #define SHAKE_RSTTIME         userProfile.shakeSensitivity.resetTime     // quiet time before resetting peaks counter (and triggering end shake) [ms]
  #define SHAKE_QUIETTIME       500     // time to keep clashed disabled after end shake [ms]
  #define ODD(X) (X & 1)
  #define EVEN(X) !ODD(X)
  #if defined(X_BROADCAST) && defined(BROADCAST_2TAP)
    int16_t public_shakeState;    // just for broadcast, to see shake and tap in sync
  #endif
  
  // Detect shake 
  void DetectShake() {
      static uint32_t lastPeakTime = 0;
      static int16_t shakeState = 0;
      static bool shakeTriggered = false;

      uint32_t timeNow = millis();    
      Vec3 mss = fusor.mss();
      float res2 = mss.y * mss.y + mss.z * mss.z; // Squared resultant of Y-Z acceleration. 

      // 1. Check peak start
      if (res2 >= SHAKE_THRESHOLD) { // above peak threshold
          if (EVEN(shakeState)) {   // begin peak
              shakeState++; // state becomes odd
              if ((!shakeState) && (timeNow-lastPeakTime > SHAKE_MAXPERIOD))  // invalid peak, too far apart from previous one
                  shakeState = -1;   // keep state odd but reset it ... will go to 0 at end peak
              lastPeakTime = timeNow;
          }
      }
      // 2. Check peak end
      else { // below peak threshold
        if (ODD(shakeState)) { // end peak
                if (timeNow-lastPeakTime > SHAKE_MINDURATION) { // end peak
                    shakeState++;     
                    if (shakeTriggered) IgnoreClash(SHAKE_RSTTIME); // mask clashes while shaking 
                }
                else // too short for a valid peak, probably clash vibrations
                    shakeState-= 3;   //  Penalty to discriminate from repeated clashes: shakeState will go negative until quiet for SHAKE_RSTTIME

               
        }
        // 3. Check reset conditions            
        if (timeNow-lastPeakTime > SHAKE_RSTTIME) {   // quiet for some time
              if (shakeTriggered)  {  // end shake
                  // STDOUT.println("END-SHAKE detected!");
                  shakeTriggered = false;                                                     
                  Event(BUTTON_NONE, EVENT_ENDSHAKE);  
                  IgnoreClash(SHAKE_QUIETTIME);
              }             
              shakeState = 0; // Reset, regardless
        }

      }
      
      // 4. Check shake conditions
      if (shakeState == SHAKE_NPEAKS && !shakeTriggered) {   // shake
        // STDOUT.println("SHAKE detected!");
        Event(BUTTON_NONE, EVENT_SHAKE);  
        shakeTriggered = true;    // we need to keep track of the shake event distinctly from the shake state, because we detect shake continuously, even after the event
                                  // anti-clash penalty might cause shakeState to reach the threashold reapeatidly and we don't want multiple events
      }      
  #if defined(X_BROADCAST) && defined(BROADCAST_2TAP)
    public_shakeState = shakeState;    // just for broadcast, to see shake and tap in sync
  #endif
  #if defined(X_BROADCAST) && defined(BROADCAST_SHAKE)
        struct {
            int16_t up1, up2, up3;
            int16_t down1, down2, down3;
        } ch1_broadcast;
        ch1_broadcast.up1 = (int16_t)(100*mss.x);
        ch1_broadcast.up2 = (int16_t)(100*mss.y);
        ch1_broadcast.up3 = (int16_t)(10*res2);
        ch1_broadcast.down1 = (int16_t)(shakeState);
        ch1_broadcast.down2 = 0;
        ch1_broadcast.down3 = 0;
        STDOUT.Broadcast(1, &ch1_broadcast, sizeof(ch1_broadcast));        
  #endif // X_BROADCAST   
  }




  #define DTAP_ACCTH     userProfile.tapSensitivity.threshold      // acceleration threshold to consider it a clash
  #define DTAP_CLASHDBT  60     // clash debounce time: time to ignore acceleration after triggering a clash [ms]
  // #define DTAP_MINTIME   userProfile.tapSensitivity.minTime    // minimum time between clashes to call them taps [ms]
  #define DTAP_MINTIME   SHAKE_MAX_PER+1    // minimum time between clashes to call them taps [ms]
  #define DTAP_MAXTIME   userProfile.tapSensitivity.maxTime    // maximum time between clashes to call them taps [ms]
  #define DTAP_QUIETTIME 500    // minimum time between successive double-taps [ms]



// Detect double tap
void Detect2Tap(float acc)  {  // Got acceleration resultant from DoAccel()
  static bool clash=false;      // true if acceleration went over the thresold
  // static uint32_t lastChecked=0;// last time when detection was executed (once @ 10 ms)
  static int32_t lastClash=0;   // negative: time when we started ignoring clashes;  non-negative: time when last clash was detected
  static int16_t tapCounter;    // counts valid taps, with negative penalties
  
  #if defined(X_BROADCAST) && defined(BROADCAST_2TAP)
    static bool Detected2Tap = false;
  #endif

  acc = fusor.accel().len2();   // len squared (no need for sqrt)
  // 0. Ignore if needed
  uint32_t timeNow = millis();
  // uint32_t dT = timeNow-lastChecked;   // time since last valid clash
  // if (dT < 2) return;
  // lastChecked = timeNow;

  if (lastClash<0) {  // ignore DTAP_QUIETTIME [ms], since -lastClash
    if (timeNow+lastClash > DTAP_QUIETTIME) {
        lastClash = 0; // stop ignoring
        #if defined(X_BROADCAST) && defined(BROADCAST_2TAP)
            Detected2Tap = false;   // broadcast end of ignore time
        #endif
    }
  }
  else {
    // 1. Detect clashes  
     uint32_t dT = timeNow-lastClash;   // time since last valid clash
    if (acc>DTAP_ACCTH) { // over thresold
      if (!clash) { // clash event!
          clash=true;
          // 2. Discriminate between clashed and taps
          if (dT>DTAP_QUIETTIME) 
              tapCounter = 1;        // this can only be the first tap, since it came after some long quiet time
          else if (dT<DTAP_MINTIME) 
                  tapCounter = 3;     // clashes are too fast, forget about it! Need QUIETTIME silence to resume tap detection
               else if (dT<DTAP_MAXTIME)   // valid tap, keep counting!
                  tapCounter++;              
          lastClash = timeNow;
          // STDOUT.println(tapCounter); 
      }
    }
    else { // below threshold
      if (clash && dT>DTAP_CLASHDBT) 
        clash=false;
    }

    // 3. Decide on double-tap
    if (timeNow-lastClash > DTAP_CLASHDBT && tapCounter==2) { // double tap!
          Event(BUTTON_NONE, EVENT_2TAP);   // trigger event
          lastClash = -timeNow;
          #if defined(X_BROADCAST) && defined(BROADCAST_2TAP)
            Detected2Tap = true;  // broadcast double-tap for QUIETTIME [ms]
          #endif
          tapCounter = 0;  
          // STDOUT.println("2xTAP detected! ");     
      }

    // 4. Reset if quiet
    if (tapCounter && timeNow-lastClash > DTAP_MAXTIME)  tapCounter = 0;  

  }  

#if defined(X_BROADCAST) && defined(BROADCAST_2TAP)
        struct {
            int16_t up1, up2, up3;
            int16_t down1, down2, down3;
        } ch1_broadcast;
        ch1_broadcast.up1 = (int16_t)(100*acc);
        ch1_broadcast.up2 = public_shakeState;
        ch1_broadcast.up3 = 0;
        ch1_broadcast.down1 = clash;
        ch1_broadcast.down2 = tapCounter;
        ch1_broadcast.down3 = Detected2Tap;
        STDOUT.Broadcast(1, &ch1_broadcast, sizeof(ch1_broadcast));        
  #endif // X_BROADCAST   
  }




  bool swinging_ = false;
  // The prop should call this from Loop() if it wants to detect swings as an event.
  void DetectSwing() {
    if (!swinging_ && fusor.swing_speed() > 250) {
      swinging_ = true;
      Event(BUTTON_NONE, EVENT_SWING);
    }
    if (swinging_ && fusor.swing_speed() < 100) {
      swinging_ = false;
    }
  }

  Vec3 accel_;

  void StartOrStopTrack() {
#ifdef ENABLE_AUDIO
    if (track_player_) {
      track_player_->Stop();
      track_player_.Free();
    } else {
      MountSDCard();
      EnableAmplifier();
      track_player_ = GetFreeWavPlayer();
      if (track_player_) {
          track_player_->Play(current_preset_->track);
      } else {
        STDOUT.println("No available WAV players.");
      }
    }
#else
    STDOUT.println("Audio disabled.");
#endif
  }

  void ListTracks(const char* dir, FileReader* fileWriter) {
    if (!LSFS::Exists(dir)) return;
    for (LSFS::Iterator i2(dir); i2; ++i2) {
      if (endswith(".wav", i2.name()) && i2.size() > 200000) {
          if(fileWriter) {
            fileWriter->Write(dir); fileWriter->Write("/"); 
            fileWriter->Write(i2.name()); fileWriter->Write("\n");
          }
          else { 
            STDOUT.print(dir); STDOUT.print("/"); STDOUT.println(i2.name()); 
          } 
          
      }
    }
  }

  virtual void LowBatteryOff() {
    if (SaberBase::IsOn()) {
      STDOUT.print("Battery low, turning off. Battery voltage: ");
      STDOUT.println(battery_monitor.battery());
      Off();
    }
  }

  virtual void CheckLowBattery() {
  #if defined(PROFFIEBOARD) || ( defined(ULTRAPROFFIE) && ULTRAPROFFIE_VERSION == 'P')
    if (battery_monitor.low()) {
      if (current_style() && !current_style()->Charging()) {
        LowBatteryOff();
        if (millis() - last_beep_ > 15000) {  // (was 5000)
          STDOUT << "Low battery: " << battery_monitor.battery() << " volts\n";
          SaberBase::DoLowBatt();
          last_beep_ = millis();
        }
      }
    }
  #endif // TODO add behavoir for proffie LITe 
  }


  uint32_t last_motion_call_millis_;
  void CallMotion() {
    if (millis() == last_motion_call_millis_) return;
    if (!fusor.ready()) return;
    bool clear = millis() - last_motion_call_millis_ > 100;
    last_motion_call_millis_ = millis();
    SaberBase::DoAccel(fusor.accel(), clear);
    SaberBase::DoMotion(fusor.gyro(), clear);

  }
  volatile bool clash_pending1_ = false;
  volatile int8_t pending_clash_is_stab1_ = 0;  // -1 = reversed stab


  volatile float pending_clash_strength1_ = 0.0;

  uint32_t last_beep_;
  float current_tick_angle_ = 0.0;

  void Loop() override {
    CallMotion();
    if (clash_pending1_) {
      clash_pending1_ = false;
      Clash(pending_clash_is_stab1_, pending_clash_strength1_);
    }
    if (clash_pending_ && millis() - last_clash_ >= clash_timeout_) {
      clash_pending_ = false;
      Clash2(pending_clash_is_stab_, pending_clash_strength_);
    }
    CheckLowBattery();
#ifdef ENABLE_AUDIO
    if (track_player_ && !track_player_->isPlaying()) {
      track_player_.Free();
    }
#endif




#ifdef IDLE_OFF_TIME
    if (SaberBase::IsOn() ||
        (current_style() && current_style()->Charging())) {
      last_on_time_ = millis();
    }
    if (millis() - last_on_time_ > IDLE_OFF_TIME) {
      SaberBase::DoOff(OFF_IDLE);
      last_on_time_ = millis();
    }
#endif

  }

#ifdef IDLE_OFF_TIME
  uint32_t last_on_time_;
#endif


  virtual void PrintButton(uint32_t b) {
    if (b & BUTTON_POWER) STDOUT.print("Power");
    if (b & BUTTON_AUX) STDOUT.print("Aux");
    if (b & BUTTON_AUX2) STDOUT.print("Aux2");
    if (b & BUTTON_UP) STDOUT.print("Up");
    if (b & BUTTON_DOWN) STDOUT.print("Down");
    if (b & BUTTON_LEFT) STDOUT.print("Left");
    if (b & BUTTON_RIGHT) STDOUT.print("Right");
    if (b & BUTTON_SELECT) STDOUT.print("Select");
    if (b & BUTTON_BLADE_DETECT) STDOUT.print("Select");
    if (b & MODE_ON) STDOUT.print("On");
  }

  void PrintEvent(uint32_t e) {
    int cnt = 0;
    if (e >= EVENT_FIRST_PRESSED &&
        e <= EVENT_FOURTH_CLICK_LONG) {
      cnt = (e - EVENT_PRESSED) / (EVENT_SECOND_PRESSED - EVENT_FIRST_PRESSED);
      e -= (EVENT_SECOND_PRESSED - EVENT_FIRST_PRESSED) * cnt;
    }
    switch (e) {
      case EVENT_NONE: STDOUT.print("None"); break;
      case EVENT_PRESSED: STDOUT.print("Pressed"); break;
      case EVENT_RELEASED: STDOUT.print("Released"); break;
      case EVENT_HELD: STDOUT.print("Held"); break;
      case EVENT_HELD_MEDIUM: STDOUT.print("HeldMedium"); break;
      case EVENT_HELD_LONG: STDOUT.print("HeldLong"); break;
      case EVENT_CLICK_SHORT: STDOUT.print("Shortclick"); break;
    #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
      case EVENT_CLICK_MEDIUM: STDOUT.print("MEDIUMclick"); break;
    #endif
      case EVENT_CLICK_LONG: STDOUT.print("Longclick"); break;
      case EVENT_SAVED_CLICK_SHORT: STDOUT.print("SavedShortclick"); break;
      case EVENT_LATCH_ON: STDOUT.print("On"); break;
      case EVENT_LATCH_OFF: STDOUT.print("Off"); break;
      case EVENT_STAB: STDOUT.print("Stab"); break;
      case EVENT_SWING: STDOUT.print("Swing"); break;
      case EVENT_SHAKE: STDOUT.print("Shake"); break;
      case EVENT_ENDSHAKE: STDOUT.print("End-Shake"); break;
      case EVENT_2TAP: STDOUT.print("Double Tap"); break;
      case EVENT_TWIST: STDOUT.print("Twist"); break;
      case EVENT_CLASH: STDOUT.print("Clash"); break;
      case EVENT_THRUST: STDOUT.print("Thrust"); break;
      case EVENT_PUSH: STDOUT.print("Push"); break;
      default: STDOUT.print("?"); STDOUT.print(e); break;
    }
    if (cnt) {
      STDOUT.print('#');
      STDOUT.print(cnt);
    }
  }

  void PrintEvent(enum BUTTON button, EVENT event) {
    STDOUT.print("EVENT: ");
    if (button) {
      PrintButton(button);
      STDOUT.print("-");
    }
    PrintEvent(event);
    if (current_modifiers & ~button) {
      STDOUT.print(" mods ");
      PrintButton(current_modifiers);
    }
    if (IsOn()) STDOUT.print(" ON");
    STDOUT.print(" millis=");
    STDOUT.println(millis());
  }

  bool Parse(const char *cmd, const char* arg) override {

    if (!strcmp(cmd, "stealth")) {
      // stealthMode = !stealthMode;
      SetStealth(!stealthMode); 
      return true;
    }
    #ifdef X_MENUTEST
      // parse menu commands as well, if needed
      if (menu)
        if (menu->Parse(cmd, arg)) return true;   
    #endif  // X_MENUTEST  


    if (!strcmp(cmd, "on")) {
        On();      
      return true;
    }
    if (!strcmp(cmd, "off")) {
      Off();
      return true;
    }
    if (!strcmp(cmd, "get_on")) {
      STDOUT.println(IsOn());
      return true;
    }
    if (!strcmp(cmd, "clash")) {
      Clash(false, 10.0);
      return true;
    }
    if (!strcmp(cmd, "stab")) {
      Clash(true, 10.0);
      return true;
    }
    if (!strcmp(cmd, "force")) {
      SaberBase::DoForce();
      return true;
    }
    if (!strcmp(cmd, "blast")) {
      // Avoid the base and the very tip.
      // TODO: Make blast only appear on one blade!
      SaberBase::DoBlast();
      return true;
    }
    if (!strcmp(cmd, "lock") || !strcmp(cmd, "lockup")) {
      On();
      STDOUT.print("Lockup ");
      if (SaberBase::Lockup() == SaberBase::LOCKUP_NONE) {
        SaberBase::SetLockup(SaberBase::LOCKUP_NORMAL);
        SaberBase::DoBeginLockup();
        STDOUT.println("ON");
      } else {
        SaberBase::DoEndLockup();
        SaberBase::SetLockup(SaberBase::LOCKUP_NONE);
        STDOUT.println("OFF");
      }
      return true;
    }
    if (!strcmp(cmd, "drag")) {
      STDOUT.print("Drag ");
      if (SaberBase::Lockup() == SaberBase::LOCKUP_NONE) {
        SaberBase::SetLockup(SaberBase::LOCKUP_DRAG);
        SaberBase::DoBeginLockup();
        STDOUT.println("ON");
      } else {
        SaberBase::DoEndLockup();
        SaberBase::SetLockup(SaberBase::LOCKUP_NONE);
        STDOUT.println("OFF");
      }
      return true;
    }
    if (!strcmp(cmd, "lblock") || !strcmp(cmd, "lb")) {
      STDOUT.print("lblock ");
      if (SaberBase::Lockup() == SaberBase::LOCKUP_NONE) {
        SaberBase::SetLockup(SaberBase::LOCKUP_LIGHTNING_BLOCK);
        SaberBase::DoBeginLockup();
        STDOUT.println("ON");
      } else {
        SaberBase::DoEndLockup();
        SaberBase::SetLockup(SaberBase::LOCKUP_NONE);
        STDOUT.println("OFF");
      }
      return true;
    }
    if (!strcmp(cmd, "melt")) {
      STDOUT.print("melt ");
      if (SaberBase::Lockup() == SaberBase::LOCKUP_NONE) {
        SaberBase::SetLockup(SaberBase::LOCKUP_MELT);
        SaberBase::DoBeginLockup();
        STDOUT.println("ON");
      } else {
        SaberBase::DoEndLockup();
        SaberBase::SetLockup(SaberBase::LOCKUP_NONE);
        STDOUT.println("OFF");
      }
      return true;
    }

#ifdef ENABLE_AUDIO

// #ifndef DISABLE_DIAGNOSTIC_COMMANDS
#ifdef ENABLE_DIAGNOSE_COMMANDS
    if (!strcmp(cmd, "beep")) {
      beeper.Beep(0.5, 293.66 * 2);
      beeper.Beep(0.5, 329.33 * 2);
      beeper.Beep(0.5, 261.63 * 2);
      beeper.Beep(0.5, 130.81 * 2);
      beeper.Beep(1.0, 196.00 * 2);
      return true;
    }

  if (!strcmp(cmd, "play")) {
      if (!arg) {
        StartOrStopTrack();
        return true;
      }
      MountSDCard();
      EnableAmplifier();
      RefPtr<BufferedWavPlayer> player = GetFreeWavPlayer();
      if (player) {
        STDOUT.print("Playing ");
        STDOUT.println(arg);
        if (!player->PlayInCurrentDir(arg))
          player->Play(arg);
      } else {
        STDOUT.println("No available WAV players.");
      }
      
      return true;
    }





    if (!strcmp(cmd, "play_track")) {
      if (!arg) {
        StartOrStopTrack();
        return true;
      }
      if (track_player_) {
        track_player_->Stop();
        track_player_.Free();
      }
      MountSDCard();
      EnableAmplifier();
      track_player_ = GetFreeWavPlayer();
      if (track_player_) {
        STDOUT.print("Playing ");
        STDOUT.println(arg);
        if (!track_player_->PlayInCurrentDir(arg))
          track_player_->Play(arg);
      } else {
        STDOUT.println("No available WAV players.");
      }
      return true;
    }
    if (!strcmp(cmd, "stop_track")) {
      if (track_player_) {
        track_player_->Stop();
        track_player_.Free();
      }
      return true;
    }
    if (!strcmp(cmd, "get_track")) {
      if (track_player_) {
        STDOUT.println(track_player_->Filename());
      }
      return true;
    }
#endif
  
// #ifndef DISABLE_DIAGNOSTIC_COMMANDS
#ifdef ENABLE_DIAGNOSE_COMMANDS
    if (!strcmp(cmd, "volumes")) {
      for (size_t unit = 0; unit < NELEM(wav_players); unit++) {
        STDOUT.print(" Unit ");
        STDOUT.print(unit);
        STDOUT.print(" Volume ");
        STDOUT.println(wav_players[unit].volume());
      }
      return true;
    }
#endif // ENABLE_DIAGNOSE_COMMANDS

#ifdef ENABLE_DEVELOPER_COMMANDS
    if (!strcmp(cmd, "buffered")) {
      for (size_t unit = 0; unit < NELEM(wav_players); unit++) {
        STDOUT.print(" Unit ");
        STDOUT.print(unit);
        STDOUT.print(" Buffered: ");
        STDOUT.println(wav_players[unit].buffered());
      }
      return true;
    }
#endif // ENABLE_DEVELOPER_COMMANDS

#endif // enable sound

#ifdef ENABLE_DIAGNOSE_COMMANDS
    if (!strcmp(cmd, "cd")) {
      chdir(arg);
      SaberBase::DoNewFont();
      return true;
    }
#if 0
    if (!strcmp(cmd, "mkdir")) {
      SD.mkdir(arg);
      return true;
    }
#endif
    if (!strcmp(cmd, "pwd")) {
      for (const char* dir = current_directory; dir; dir = next_current_directory(dir)) {
        STDOUT.println(dir);
      }
      return true;
    }
#endif // ENABLE_DIAGNOSE_COMMANDS
    if (!strcmp(cmd, "n") || (!strcmp(cmd, "next") && arg && (!strcmp(arg, "preset") || !strcmp(arg, "pre")))) {
      // next_preset();
      next_preset_fast();
      return true;
    }
    if (!strcmp(cmd, "p") || (!strcmp(cmd, "prev") && arg && (!strcmp(arg, "preset") || !strcmp(arg, "pre")))) {
      // previous_preset();
      previous_preset_fast();
      return true;
    }
  

    if (!strcmp(cmd, "list_presets")) {
       list_presets();       
      return true;
    } 

    if (!strcmp(cmd, "list_profile")) {
       list_profile();       
      return true;
    } 

    
    
   #ifdef ENABLE_DIAGNOSE_COMMANDS 
    if (!strcmp(cmd, "show_current_preset")) {
        current_preset_->Print();
      return true;
    }

    // if (!strcmp(cmd, "reload_presets")) {
    //     STDOUT.Silent(true);
    //     bool retVal = LoadActivePresets(PROFILE_FILE, userProfile.apID);
    //     STDOUT.Silent(false);
    //     if (retVal) STDOUT.println("reload_presets-OK");
    //     else STDOUT.println("reload_presets-FAIL");

        
    //     // current_preset_->Print();
    //   return true;
    // }


    if (!strcmp(cmd, "tell_style")) {
      #define TELL_BLADE_STYLE(N) do {                                         \
            STDOUT.print("blade #"); STDOUT.print(N); STDOUT.print(" @ "); STDOUT.print((uint32_t)current_config->blade##N);   \
            STDOUT.print(" has style: "); STDOUT.print(current_preset_->bladeStyle[N-1]->name); STDOUT.print(" @ "); STDOUT.println((uint32_t)(current_config->blade##N->current_style()));  \
      } while (0);
      ONCEPERBLADE(TELL_BLADE_STYLE) 
      return true;
    }
   #endif // ENABLE_DIAGNOSE_COMMANDS

  

  

    if (!strcmp(cmd, "get_usersettings")) {            
      bool guarded=false;
      if (arg) {
        guarded = true;
        STDOUT.println("get_usersettings-START");
      }
      switch(*arg) {
        case 'V': // get regular volume
            STDOUT.println(userProfile.combatVolume);  break;            
        case 'v': // get stealth volume
            STDOUT.println(userProfile.stealthVolume);  break;
        case 'B': // get regular brightness
            STDOUT.println(userProfile.combatBrightness); break;
        case 'b': // get stealth brightness
            STDOUT.println(userProfile.stealthBrightness); break;
        case 's': // get sensitivity
              // STDOUT.println(userProfile.masterSensitivity); break;
              STDOUT.println(Sensitivity::master); break;
        case 'p': // get preset
            STDOUT.println(userProfile.preset); break;
        case 0: // get ALL
            STDOUT.print("  [V] Master volume = "); STDOUT.println(userProfile.combatVolume);
            STDOUT.print("  [v] Stealth volume = "); STDOUT.println(userProfile.stealthVolume);
            STDOUT.print("  [B] Master brightness = "); STDOUT.println(userProfile.combatBrightness);
            STDOUT.print("  [b] Stealth brightness = "); STDOUT.println(userProfile.stealthBrightness);
            STDOUT.print("  [s] Master sensitivity = "); STDOUT.println(Sensitivity::master); // STDOUT.println(userProfile.masterSensitivity);
            STDOUT.print("  [p] Preset = "); STDOUT.println(userProfile.preset);
            break;
      }
      if (guarded) STDOUT.println("get_usersettings-END");
      return true;

    }

    if (!strcmp(cmd, "set_usersettings")) {        
        uint16_t numVal = atoi(arg+2);    // numerical value, after character identifier and comma
        switch(*arg) {
           case 'V': // set volume
              if (numVal < MIN_MASTER_VOLUME) numVal = 0;
              userProfile.combatVolume = numVal;  
              if(!stealthMode) {  // apply volume if currently in use
                userProfile.masterVolume = numVal;
                dynamic_mixer.set_volume(VOLUME);
                #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
                if (!userProfile.masterVolume) {
                  SilentEnableAmplifier(false);
                  SilentEnableBooster(false);
                }
                else if (IsOn()) {
                  SilentEnableAmplifier(true);
                  SilentEnableBooster(true);
                }
                #endif
              }
              return true;
           case 'v': // set stealth volume
              if (numVal < MIN_MASTER_VOLUME) numVal = 0;
              if (numVal > userProfile.combatVolume) numVal = userProfile.combatVolume; // limit to regular volume
              userProfile.stealthVolume = numVal;  
              if(stealthMode) {  // apply volume if currently in use
                userProfile.masterVolume = numVal;
                dynamic_mixer.set_volume(VOLUME);
                #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
                if (!userProfile.masterVolume) {
                  SilentEnableAmplifier(false);
                  SilentEnableBooster(false);
                }
                else if (IsOn()) {
                  SilentEnableAmplifier(true);
                  SilentEnableBooster(true);
                }
                #endif
              }
              return true;
          case 'B': // set brightness
              if (numVal < MIN_MASTER_BRIGHTNESS) numVal = 0;
              userProfile.combatBrightness = numVal;
              if(!stealthMode) userProfile.masterBrightness = numVal; // apply brightness if currently in use
              return true;    
          case 'b': // set stealth brightness
              if (numVal < MIN_MASTER_BRIGHTNESS) numVal = 0;
              if (numVal > userProfile.combatBrightness) numVal = userProfile.combatBrightness; // limit to regular brightness
              userProfile.stealthBrightness = numVal;
              if(stealthMode) userProfile.masterBrightness = numVal; // apply brightness if currently in use
              return true;    
          case 's': // set master sensitivity
              // userProfile.masterSensitivity = numVal;
              if (numVal>255) numVal = 255;
              ApplySensitivities(numVal);       // apply new master sensitivity
              return true; 
          case 'p': // set preset
              if (numVal && numVal<=presets.size()) 
                  SetPreset(numVal, true);              
              return true;                         
        }
    }

 if (!strcmp(cmd, "get_sensitivity")) {            
      bool guarded=false;
      if (arg) {
        guarded = true;
        STDOUT.println("get_sensitivity-START");
      }
      switch(*arg) {
        case 's':   // get swing sensitivity
            STDOUT.println(userProfile.swingSensitivity.userSetting);  break;               
        case 'c':   // get clash sensitivity
            STDOUT.println(userProfile.clashSensitivity.userSetting);  break;               
        case 'b':   // get stab sensitivity
            STDOUT.println(userProfile.stabSensitivity.userSetting);  break;
        case 'k':   // get shake sensitivity
            STDOUT.println(userProfile.shakeSensitivity.userSetting);  break;
        case 't':   // get double-tap sensitivity
            STDOUT.println(userProfile.tapSensitivity.userSetting);  break;
        case 'w':   // get twist sensitivity
            STDOUT.println(userProfile.twistSensitivity.userSetting);  break;
        case 'm':   // get menu sensitivity
            STDOUT.println(userProfile.menuSensitivity.userSetting);  break;
        case 0: // get ALL     
          STDOUT.print("  MASTER = "); STDOUT.println(Sensitivity::master);
          STDOUT.print("  [s] Swing = "); STDOUT.print(userProfile.swingSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.swingSensitivity.ApplyMaster());
          STDOUT.print("  [c] Clash = "); STDOUT.print(userProfile.clashSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.clashSensitivity.ApplyMaster());
          STDOUT.print("  [b] Stab = "); STDOUT.print(userProfile.stabSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.stabSensitivity.ApplyMaster());
          STDOUT.print("  [k] Shake = "); STDOUT.print(userProfile.shakeSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.shakeSensitivity.ApplyMaster());
          STDOUT.print("  [t] Tap = "); STDOUT.print(userProfile.tapSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.tapSensitivity.ApplyMaster());
          STDOUT.print("  [w] Twist = "); STDOUT.print(userProfile.twistSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.twistSensitivity.ApplyMaster());
          STDOUT.print("  [m] Menu = "); STDOUT.print(userProfile.menuSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.menuSensitivity.ApplyMaster());
          break;         

      }
      if (guarded) STDOUT.println("get_sensitivity-END");
      return true;
    }

    if (!strcmp(cmd, "set_sensitivity")) {        
        uint16_t numVal = atoi(arg+2);    // numerical value, after character identifier and comma
        if (numVal>255) numVal = 255;
        switch(*arg) {
          case 's':   // set swing sensitivity
              userProfile.swingSensitivity.userSetting = numVal;
              userProfile.swingSensitivity.Set();
              STDOUT.print("Sensitivity set to "); STDOUT.print(userProfile.swingSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.swingSensitivity.ApplyMaster());
              STDOUT.print(" SwingSensitivity multiplier = "); STDOUT.println(userProfile.swingSensitivity.SwingSensitivity_multiplier);
              STDOUT.print(" SwingSharpness multiplier = "); STDOUT.println(userProfile.swingSensitivity.SwingSharpness_multiplier);
              STDOUT.print(" AccentSwingSpeedThreshold multiplier = "); STDOUT.println(userProfile.swingSensitivity.AccentSwingSpeedThreshold_multiplier);
              STDOUT.print(" MaxSwingVolume multiplier = "); STDOUT.println(userProfile.swingSensitivity.MaxSwingVolume_multiplier);
              smooth_swing_config.ApplySensitivity(&userProfile.swingSensitivity);  // scale parameters with sensitivity              
              return true;                             
          case 'c':   // set clash sensitivity
              userProfile.clashSensitivity.userSetting = numVal;
              userProfile.clashSensitivity.Set();
              STDOUT.print("Sensitivity set to "); STDOUT.print(userProfile.clashSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.clashSensitivity.ApplyMaster());
              STDOUT.print(" Clash threshold = "); STDOUT.println(CLASH_THRESHOLD_G);
              return true; 
          case 'b':   // set stab sensitivity
              userProfile.stabSensitivity.userSetting = numVal;
              userProfile.stabSensitivity.Set();
              STDOUT.print("Sensitivity set to "); STDOUT.print(userProfile.stabSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.stabSensitivity.ApplyMaster());
              STDOUT.print(" Stab threshold = "); STDOUT.println(userProfile.stabSensitivity.threshold);
              STDOUT.print(" Stab directionality = "); STDOUT.println(userProfile.stabSensitivity.dir);
              return true; 
          case 'k':   // set shake sensitivity
              userProfile.shakeSensitivity.userSetting = numVal;
              userProfile.shakeSensitivity.Set();
              STDOUT.print("Sensitivity set to "); STDOUT.print(userProfile.shakeSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.shakeSensitivity.ApplyMaster());
              STDOUT.print(" Shake threshold = "); STDOUT.println(userProfile.shakeSensitivity.threshold);
              STDOUT.print(" Shake period = "); STDOUT.println(userProfile.shakeSensitivity.maxPeriod);
              STDOUT.print(" Shake N preaks = "); STDOUT.println(userProfile.shakeSensitivity.nPeaks);
              STDOUT.print(" Shake reset time = "); STDOUT.println(userProfile.shakeSensitivity.resetTime);
              return true; 
          case 't':   // set tap sensitivity
              userProfile.tapSensitivity.userSetting = numVal;
              userProfile.tapSensitivity.Set();
              STDOUT.print("Sensitivity set to "); STDOUT.print(userProfile.swingSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.swingSensitivity.ApplyMaster());
              STDOUT.print(" Tap threshold = "); STDOUT.println(userProfile.tapSensitivity.threshold);
              // STDOUT.print(" Tap minTime = "); STDOUT.println(userProfile.tapSensitivity.minTime);
              STDOUT.print(" Tap minTime = "); STDOUT.println(DTAP_MINTIME);
              STDOUT.print(" Tap maxTime = "); STDOUT.println(userProfile.tapSensitivity.maxTime);
              return true; 
          case 'w':   // set twist sensitivity
              userProfile.twistSensitivity.userSetting = numVal;
              userProfile.twistSensitivity.Set();
              STDOUT.print("Sensitivity set to "); STDOUT.print(userProfile.twistSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.twistSensitivity.ApplyMaster());
              STDOUT.print(" Twist threshold = "); STDOUT.println(userProfile.twistSensitivity.threshold);
              STDOUT.print(" Twist directionality = "); STDOUT.println(userProfile.twistSensitivity.dir);
              STDOUT.print(" Twist minTime = "); STDOUT.println(userProfile.twistSensitivity.minTime);
              STDOUT.print(" Twist maxTime = "); STDOUT.println(userProfile.twistSensitivity.maxTime);

              return true; 
          case 'm':   // set menu sensitivity
              userProfile.menuSensitivity.userSetting = numVal;
              userProfile.menuSensitivity.Set();
              STDOUT.print("Sensitivity set to "); STDOUT.print(userProfile.menuSensitivity.userSetting); STDOUT.print(", rescaled by master to "); STDOUT.println(userProfile.menuSensitivity.ApplyMaster());
              STDOUT.print(" Theta threshold = "); STDOUT.println(userProfile.menuSensitivity.thetaThreshold);
              STDOUT.print(" Tick threshold = "); STDOUT.println(userProfile.menuSensitivity.tickThreshold);
              STDOUT.print(" Start scroll = "); STDOUT.println(userProfile.menuSensitivity.startScroll);
              STDOUT.print(" Max scroll = "); STDOUT.println(userProfile.menuSensitivity.maxScroll);
              STDOUT.print(" Stable time = "); STDOUT.println(userProfile.menuSensitivity.stableTime);
              return true;                                                                                     
        }
    }         





    if (!strcmp(cmd, "save_profile")) {     // save profile (including color variations) to profile.cod
        if (WriteUserProfile(PROFILE_FILE, 1))
          STDOUT.println("save_profile-OK");
        else
          STDOUT.println("save_profile-FAIL");
        return true;
    }


    if (!strcmp(cmd, "get_color")) {
        STDOUT.println("get_color-START");
        if(current_preset_)
          STDOUT.println(current_preset_->variation);
        STDOUT.println("get_color-END");
        return true;
    }

    if (!strcmp(cmd, "set_color")) {
        if (arg) {  // set color variation
          size_t variation = strtol(arg, NULL, 0);
          current_preset_->variation = variation;
          SaberBase::SetVariation(variation);
        }
        return true;
    }

   


      



  
    if (!strcmp(cmd, "list_styles")) {
      list_styles(NULL);
      return true;
    }

    if (!strcmp(cmd, "list_tracks")) {
      list_tracks(NULL);
      return true;
    }

    if (!strcmp(cmd, "list_fonts")) {
      list_fonts(NULL);  
      return true;
    }  



  return false;

  }

  void list_styles(FileReader* fileWriter)
  {   
      if(fileWriter)LOCK_SD(true);

      if(fileWriter)fileWriter->Write("list_styles-START\n");
      else STDOUT.println("list_styles-START");

      for (uint16_t i=0; i<styles.size(); i++) {
        if(fileWriter) {
          char buffer[5];
          fileWriter->Write(styles[i].name);
          fileWriter->Write(", "); 
          sprintf(buffer, "%d", styles[i].good4); 
          fileWriter->Write(buffer);
          fileWriter->Write("\n");
        } else {
          STDOUT.print(styles[i].name);
          STDOUT.print(", ");
          STDOUT.println(styles[i].good4);
        }
      }
      if(fileWriter)fileWriter->Write("list_styles-END\n");        
      else STDOUT.println("list_styles-END");
      // if(!fileWriter)STDOUT.println("NO FILE WRITER");
      if(fileWriter)LOCK_SD(false);
  }

  void list_fonts(FileReader* fileWriter)
  {
      LOCK_SD(true);
      if(fileWriter)fileWriter->Write("list_fonts-START\n");
      else STDOUT.println("list_fonts-START");

      for (LSFS::Iterator iter("/"); iter; ++iter) {
        if (iter.isdir()) {
          char fname[128];
          strcpy(fname, iter.name());
          strcat(fname, "/");
          char* fend = fname + strlen(fname);
          bool isfont = false;
          if (!isfont) {
            strcpy(fend, "hum.wav");
            isfont = LSFS::Exists(fname);
          }
          if (!isfont) {
            strcpy(fend, "hum01.wav");
            isfont = LSFS::Exists(fname);
          }
          if (!isfont) {
            strcpy(fend, "hum1.wav");
            isfont = LSFS::Exists(fname);
          }
          if (isfont) {
             if(fileWriter){fileWriter->Write(iter.name()); fileWriter->Write("\n");} 
             else STDOUT.println(iter.name());
          }
        }
      }
      if(fileWriter)fileWriter->Write("list_fonts-END\n");
      else STDOUT.println("list_fonts-END");
      LOCK_SD(false);

  }



  void list_tracks(FileReader* fileWriter)
  {
      // Tracks are must be in: tracks/*.wav or */tracks/*.wav
      LOCK_SD(true);
      if(fileWriter)fileWriter->Write("list_tracks-START\n");
      else STDOUT.println("list_tracks-START");
      ListTracks("tracks", fileWriter);
      for (LSFS::Iterator iter("/"); iter; ++iter) {
        if (iter.isdir()) {
          PathHelper path(iter.name(), "tracks");
          ListTracks(path, fileWriter);
        }
      }
      if(fileWriter)fileWriter->Write("list_tracks-END\n");
      else STDOUT.println("list_tracks-END");
      LOCK_SD(false);
  }


void list_presets(FileReader* fileWriter = 0) {
     if (fileWriter) {  // write on SD Card
        LOCK_SD(true);
        fileWriter->Write("list_presets-START\n"); 
        if (presets.size()) {
          char buffer[10];
          for (uint8_t i=0; i<presets.size(); i++) {
              fileWriter->Write(presets[i].name); 
              fileWriter->Write(", ");
              sprintf(buffer, "%d", presets[i].variation);
              fileWriter->Write(buffer);
              fileWriter->Write("\n");
          }
        }
        fileWriter->Write("list_presets-END\n");
        LOCK_SD(false);
     }
     else { // write to serial
        STDOUT.println("list_presets-START"); 
        if (presets.size())
          for (uint8_t i=0; i<presets.size(); i++) {
              STDOUT.print(presets[i].name); 
              STDOUT.print(", ");
              STDOUT.println(presets[i].variation);
          }
        STDOUT.println("list_presets-END");
     }      
}

void list_profile(FileReader* fileWriter = 0) {
     if (fileWriter) {  // write on SD Card
        char buffer[10];
        LOCK_SD(true);
        fileWriter->Write("list_profile-START\n"); 
        fileWriter->Write("ComVol, ");  sprintf(buffer, "%d",userProfile.combatVolume); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("StVol, ");  sprintf(buffer, "%d",userProfile.stealthVolume); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("ComBr, ");  sprintf(buffer, "%d",userProfile.combatBrightness); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("StBr, ");  sprintf(buffer, "%d",userProfile.stealthBrightness); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("Preset, ");  sprintf(buffer, "%d",userProfile.preset); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("MasterSense, ");  sprintf(buffer, "%d", Sensitivity::master); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("Swing, ");  sprintf(buffer, "%d",userProfile.swingSensitivity.userSetting); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("Clash, ");  sprintf(buffer, "%d",userProfile.clashSensitivity.userSetting); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("Stab, ");  sprintf(buffer, "%d",userProfile.stabSensitivity.userSetting); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("Shake, ");  sprintf(buffer, "%d",userProfile.shakeSensitivity.userSetting); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("Tap, ");  sprintf(buffer, "%d",userProfile.tapSensitivity.userSetting); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("Twist, ");  sprintf(buffer, "%d",userProfile.twistSensitivity.userSetting); fileWriter->Write(buffer); fileWriter->Write("\n");
        fileWriter->Write("Menu, ");  sprintf(buffer, "%d",userProfile.menuSensitivity.userSetting); fileWriter->Write(buffer); fileWriter->Write("\n");       
        fileWriter->Write("list_profile-END\n");
        LOCK_SD(false);
     }
     else { // write to serial
        STDOUT.println("list_profile-START"); 
        STDOUT.print("ComVol, "); STDOUT.println(userProfile.combatVolume);
        STDOUT.print("StVol, "); STDOUT.println(userProfile.stealthVolume);
        STDOUT.print("ComBr, "); STDOUT.println(userProfile.combatBrightness);
        STDOUT.print("StBr, "); STDOUT.println(userProfile.stealthBrightness);
        STDOUT.print("Preset, "); STDOUT.println(userProfile.preset);     
        STDOUT.print("MasterSense, "); STDOUT.println(Sensitivity::master); 
        STDOUT.print("Swing, "); STDOUT.println(userProfile.swingSensitivity.userSetting);
        STDOUT.print("Clash, "); STDOUT.println(userProfile.clashSensitivity.userSetting);
        STDOUT.print("Stab, "); STDOUT.println(userProfile.stabSensitivity.userSetting); 
        STDOUT.print("Shake, "); STDOUT.println(userProfile.shakeSensitivity.userSetting); 
        STDOUT.print("Tap, "); STDOUT.println(userProfile.tapSensitivity.userSetting); 
        STDOUT.print("Twist, "); STDOUT.println(userProfile.twistSensitivity.userSetting);
        STDOUT.print("Menu, "); STDOUT.println(userProfile.menuSensitivity.userSetting); 
        STDOUT.println("list_profile-END");
     }      
}

  

 void Help() override {
    #if defined(COMMANDS_HELP) 
        #if defined(X_MENUTEST)
          // parse menu commands as well, if needed
          if (menu) menu->Help();
        #endif
          STDOUT.println(" clash - trigger a clash");
          STDOUT.println(" on/off - turn saber on/off");
          STDOUT.println(" force - trigger a force push");
          STDOUT.println(" blast - trigger a blast");
          STDOUT.println(" stab - trigger a stab");
          STDOUT.println(" lock - begin/end lockup");
          STDOUT.println(" lblock/lb - begin/end lightning block");
          STDOUT.println(" melt - begin/end melt");
      #ifdef ENABLE_AUDIO
          STDOUT.println(" pwd - print current directory");
          STDOUT.println(" cd directory - change directory, and sound font");
          STDOUT.println(" play filename - play file");
          STDOUT.println(" next/prev font - walk through directories in alphabetical order");
          STDOUT.println(" next/prev pre[set] - walk through presets.");
          STDOUT.println(" beep - play a beep");
      #endif
    #endif
  }



bool PassEventToProp(enum BUTTON button, EVENT event) {
    #if defined(DIAGNOSE_EVENTS)
      PrintEvent(button, event);
    #endif

    #ifdef OSX_ENABLE_MTP
      if(!mtpUart->GetSession()) {
    #endif
        if (Event2(button, event, current_modifiers | (IsOn() ? MODE_ON : MODE_OFF))) {
          current_modifiers = 0;
          return true;
        }
        if (Event2(button, event,  MODE_ANY_BUTTON | (IsOn() ? MODE_ON : MODE_OFF))) {
          // Not matching modifiers, so no need to clear them.
          current_modifiers &= ~button;
          return true;
        }
        return false;
    #ifdef OSX_ENABLE_MTP
      } else return true;   // no prop events while transferring files
    #endif
}


virtual bool Event(enum BUTTON button, EVENT event) {    

    
    switch (event) {
      case EVENT_RELEASED:
        clash_pending_ = false;
      case EVENT_PRESSED:
        IgnoreClash(50); // ignore clashes to prevent buttons from causing clashes
        break;
    }

    // if (menu) return menu->Event(button, event);   // redirect event to TTMenu, if assigned to prop
    if (menu)   // redirect event to TTMenu, if assigned to prop
      if (menu->IsActive())
        if (menu->Event(button, event)) // release events to prop if the menu did not want them
            return true;   

    return PassEventToProp(button, event);
  }




  

  virtual bool Event2(enum BUTTON button, EVENT event, uint32_t modifiers) = 0;

protected: // was private:
  bool CommonIgnition() {
    if (IsOn()) return false;
    if (current_style() && current_style()->NoOnOff())
      return false;

#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
    RequestPower();     // get power for CPU
    EnableMotion();
#endif

    activated_ = millis();
    // STDOUT.println("Ignition.");
    MountSDCard();
    EnableAmplifier();
    SaberBase::RequestMotion();

    // Avoid clashes a little bit while turning on.
    // It might be a "clicky" power button...
    IgnoreClash(300);
    return true;
  }

public: // was protected
    Preset* current_preset_;
    volatile float accResultant = 0;
    bool stealthMode = false, setStealth = false;
  
    virtual void SetStealth(bool newStealthMode, uint8_t announce = true) {}

  LoopCounter accel_loop_counter_;
};

#endif
