#ifndef PROPS_PROP_BASE_H
#define PROPS_PROP_BASE_H

#ifndef PROP_INHERIT_PREFIX
#define PROP_INHERIT_PREFIX
#endif

#include "TTmenu.h"
#ifdef OSX_ENABLE_MTP
  #include "../common/serial.h"
#endif 


#define PREV_INDEXED (int16_t) -32768   // marker for "previous" font/track
#define NEXT_INDEXED (int16_t) 32767   // marker for "next" font/track
#define ENABLE_DIAGNOSE_COMMANDS



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
#ifdef SABERPROP
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
  #ifdef SABERPROP
    PropBase() : CommandParser(), Looper(), PowerSubscriber(pwr4_CPU) { menu=0; }
  #else 
    //TTMenu<uint16_t>* menu;    // if a menu is assigned to prop, events will be redirected to menu.Event()
    PropBase() : CommandParser() { menu=0; }
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

#ifdef SABERPROP
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
        #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
          SilentEnableAmplifier(false);
          SilentEnableBooster(false);
        #endif
        return true;
      }
    } else {
      if (muted_volume_) {
        dynamic_mixer.set_volume(muted_volume_);
        muted_volume_ = 0;
        #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
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


  virtual void FastOn() {
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

  virtual void Clash2(float strength) {
    SaberBase::SetClashStrength(strength);
    #ifdef DIAGNOSE_PROP
      STDOUT.println("CLASH detected!");
    #endif // DIAGNOSE_PROP
            
    if (Event(BUTTON_NONE, EVENT_CLASH)) {
      IgnoreClash(400);
    } 
    else {
      IgnoreClash(200);
      // Saber must be on and not in lockup mode for stab/clash.
      if (SaberBase::IsOn() && !SaberBase::Lockup()) {
          SaberBase::DoClash();
          // STDOUT.print("Orientation = "); STDOUT.println(fusor.angle1());
      }
    }
  }
  virtual void Clash(float strength) {   
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
      pending_clash_strength_ = strength;
      return;
    }
    Clash2(strength);
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
    return true;
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
      else {
        STDOUT.println("Nothing to change, no presets available.");
        return;
      }
    #endif
      
      SaberBase::SetVariation(current_preset_->variation, true); // update variation (including white)
      // chdir(current_preset_->font);                       // change font
      char font[MAX_FONTLEN];
      fonts.GetString(current_preset_->font_index, font);
      chdir(font);
      userProfile.preset = presetIndex+1;                 // set current preset in user profile

      bool restartTrack = false;
      if(track_player_) {
        track_player_->Stop();  
        // STDOUT.print("Track"); STDOUT.print(track_player_->Filename()); STDOUT.println(" stopped.");
        restartTrack = true;    
      }
      if (restartTrack) {
        StartOrStopTrack(1);  // start track (will reuse player if exists)
        // STDOUT.print("Restarting track..."); STDOUT.println(track_player_->Filename());
      }
      // else STDOUT.println("Track not playing.");

      if (current_preset_->font_index) fonts.SetCurrent(current_preset_->font_index);  // set current index in fonts (for next/prev)
      if (current_preset_->track_index) tracks.SetCurrent(current_preset_->track_index);  // set current index in tracks (for next/prev)


    }
  
  public:
    // preset_num starts at 1!
     void SetPreset(int preset_num, bool announce) {
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
	




// fontIndex starts at 1, PREV_INDEXED and NEXT_INDEXED are reserved
// fast = true => don't announce font name if saber is on, just change it
  void SetFont(int16_t fontIndex, bool fast = false) {
    if (!fonts.count) return;                  // no fonts indexed
    if (!fontIndex) return;                    // invalid index

    // 1. Get font name from index
    char fontName[MAX_FONTLEN];
    switch(fontIndex) {
      case PREV_INDEXED: 
          if (!fonts.GetPrev(fontName)) return;  // set previous as current
          fontIndex = fonts.GetCurrentIndex();  // get new index
          // STDOUT.print("SET Previous font: "); STDOUT.println(fontName);
          break;
      case NEXT_INDEXED: 
          if (!fonts.GetNext(fontName)) return;  // set next as current
          fontIndex = fonts.GetCurrentIndex();  // get new index
          // STDOUT.print("SET Next font: "); STDOUT.println(fontName);
          break;
      default: 
          if (!fonts.GetString(fontIndex, fontName)) return;  // invalid index
          fonts.SetCurrent(fontIndex);  // set new current font
          // STDOUT.print("SET New font: "); STDOUT.println(fontName);
          break;
    }

    // 2. Change font and announce
    bool on = BladeOff(true);   // silent off
    if (!chdir(fontName)) return;             // change directory
    // STDOUT.print("Font changed to '"); STDOUT.print(fontName); STDOUT.println("'.");
    if (on) {
        hybrid_font.silentOn = true;
        CommonIgnition();
        SaberBase::TurnOn();
        if (!fast) SaberBase::DoNewFont();    // announce new font 
    }
    else 
      SaberBase::DoNewFont();   // just announce new font if saber is off
    
    // 3. Update preset with new font index
    current_preset_->font_index = fontIndex;
    // STDOUT.print("New font index: "); STDOUT.println(current_preset_->font_index);

  }

  // trackIndex starts at 1, PREV_INDEXED and NEXT_INDEXED are reserved
  void SetTrack(int16_t trackIndex, bool forceStart = false,  bool fast = false) {
    if (!tracks.count) return;                  // no tracks indexed

    // 1. Update track index
    char trackName[MAX_TRACKLEN];   // we don't actually use it, just need it to update current track for next/prev
    switch(trackIndex) {
      case PREV_INDEXED: 
          if (!current_preset_->track_index) tracks.SetCurrent(1);  // if no track, set 1st as current for 'prev' 
          if (!tracks.GetPrev(trackName)) return;  // set previous as current
          trackIndex = tracks.GetCurrentIndex();  // get new index
          break;
      case NEXT_INDEXED: 
          if (!current_preset_->track_index) tracks.SetCurrent(tracks.count);  // if no track, set last as current for 'next'
          if (!tracks.GetNext(trackName)) return;  // set next as current
          trackIndex = tracks.GetCurrentIndex();  // get new index
          break;
      case 0:    // set no track: just announce and leave trackIndex as 0, nothing else to do        
          break;
      default: 
          if (!tracks.GetString(trackIndex, trackName)) return;  // invalid index
          tracks.SetCurrent(trackIndex);  // set new current track
          break;
    }

    // 2. Update preset with new track index
    current_preset_->track_index = trackIndex;

    // 3. Start or change track     
    if (trackIndex) { // track assigned
      if (track_player_) {  // track currently playing
        track_player_->Stop();  // stop current track
        StartOrStopTrack(1);  // start new track
      }
      else { // track currently not playing
        if (forceStart) StartOrStopTrack(1);  // start playing even if it was off, if forceStart = true
        else if (!fast) PlayTrackTag(2000);  // play just the tag if track was not playing
      }
    }
    else {  // no track assigned
      if (track_player_) track_player_->Stop();  // stop current track, if playing, but don't free it
      else track_player_ = GetFreeWavPlayer();
      if(track_player_)track_player_->PlayEffect(&menuSounds, notrack, 0, 1);          
    }

  }



  // Go to the next Preset.
   void next_preset() {
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
   void previous_preset() {
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

      // Detect clash
      if (v > CLASH_THRESHOLD_G + fusor.gyro().len() / 200.0) { 
        if (clash_pending1_) {
          pending_clash_strength1_ = std::max<float>(v, (float)pending_clash_strength1_);
        } else {
          clash_pending1_ = true;
          pending_clash_strength1_ = v;
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

// Detect start scroll (hard twist + SCROLL_STOPTIME[ms] still)
#define SCROLL_STOPTIME   100     // silence time after a twist stops, to register it as a scroll [ms]
#define SCROLL_ENDTWIST   0.5     // fraction of maxTheta below which a twist is considered either ended or changed direction
#define SCROLL_MINTWIST   MENU_STARTSCROLL/4  // minimum thetaPeak registered as a new twist, during stop time 

int32_t scroll_to_inject;  //  maximum thetaPeak registered during a scroll, to inject it. Sign holds theta sign.

// Detect the beginnign of a scrolling motion (and wait to make sure it's not a double-twist)
void DetectScroll() {
  static bool scrollState = false;     // current scrolling state (we're detecting start and end)
  static uint32_t maxTheta = 0;         // maximum (absolute theta) since scrolling started (we need to detect a change in direction))
  static uint32_t stopTime = 0;        // time since twisting motion stopped [ms]
  static uint32_t suspendTime = millis();     // time since detection was suspended, to prevent multi-twist being register as scroll [ms]
  float thetaNow = abs(fusor.theta());
#if defined(X_BROADCAST) && defined(BROADCAST_SCROLL)
  static bool eventState = false;
#endif

  if (suspendTime) {  // detection suspended
    if (millis() - suspendTime > MENU_SCROLLPAUSE) {
      suspendTime = 0;  // end suspension
      scroll_to_inject = 0; //
    }
  }
  else {  // detection active
      // thetaNow = abs(fusor.theta());
      if (scrollState) {  // curent registered state: scrolling
          if (thetaNow > maxTheta) maxTheta = thetaNow;  // still twisting in the same direction, store maximum theta 
          if (fusor.thetaPeak() > abs(scroll_to_inject)) { 
            int8_t thetaSign = scroll_to_inject < 0 ? -1 : 1;  // store theta sign
            scroll_to_inject = thetaSign * fusor.thetaPeak();  // store maximum thetaPeak and also the sign of the theta that triggered scrolling
          }
          else if (thetaNow < SCROLL_ENDTWIST*maxTheta) { // theta below a fraction of max, twist ended or changed direction
              // if (thetaNow < 0.1*maxTheta) { // motion ended, start measuring stop time
              if (!thetaNow) { // motion ended, start measuring stop time                 
                stopTime = millis();  // scroll detected, suspend detection for a while
              }
              else {
                  // motion changed direction, it's a second twist, not a scroll
                  suspendTime = millis();  // suspend detection for a while, to prevent multi-twist being register as scroll
              }
              scrollState = false;
              maxTheta = 0;
          }     
      }
      else {  // curent registered state: not scrolling 
          if (stopTime) { // we're measuring stop time after a scroll stop, to make sure another (reversed) twist won't start immediately
            if (fusor.thetaPeak() > SCROLL_MINTWIST) { // some twist detected during stop time:
              stopTime = 0;             // cancel counting stop time
              suspendTime = millis();   // suspend detection for a while, to prevent multi-twist being register as scroll
            }
            else if (millis() - stopTime >= SCROLL_STOPTIME) {  // stop time ended without any other twist detection
              stopTime = 0;             // cancel counting stop time
              suspendTime = millis();   // suspend detection for a while
              Event(BUTTON_NONE, EVENT_SCROLL);   // trigger event once, until theta gets below threshold                 
              #if defined(X_BROADCAST) && defined(BROADCAST_SCROLL)
                eventState = true;
              #endif  
            }          
          }
          else if (fusor.thetaPeak() > MENU_STARTSCROLL) {  // not measuring stop time, check if a new scroll is starting
            maxTheta = thetaNow;  // store theta when scrolling started
            scroll_to_inject = fusor.thetaPeak();  // store maximum thetaPeak
            if (fusor.theta() < 0) scroll_to_inject = -scroll_to_inject;  // store theta sign
            scrollState = true;   // start scrolling  
          }
      }
  
  }
  
 
    
  #if defined(X_BROADCAST) && defined(BROADCAST_SCROLL)
    struct {
        int16_t up1 = 0;
        int16_t up2 = 0;
        int16_t up3 = 0;
        int16_t down1 = 0;
        int16_t down2 = 0;
        int16_t down3 = 0;
    } ch1_broadcast;
    ch1_broadcast.up1 = (int16_t)(thetaNow);
    ch1_broadcast.up2 = (int16_t)(fusor.thetaPeak()); 
    if (scrollState) ch1_broadcast.up3 = (int16_t)(100);         
    if (stopTime) ch1_broadcast.down1 = (int16_t)(millis()-stopTime);
    // ch1_broadcast.down1 = (int16_t)(maxTheta); 
    // ch1_broadcast.down2 = (int16_t)(scroll_to_inject); 
    if (suspendTime) ch1_broadcast.down2 = (int16_t)(millis()-suspendTime);
    if (eventState) ch1_broadcast.down3 = (int16_t)(100);
    if (STDOUT.Broadcast(1, &ch1_broadcast, sizeof(ch1_broadcast)) && eventState)
        eventState = false;   // clear event state after broadcast once
  #endif // X_BROADCAST    

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


// Run detection of ignition gesture(s), return true if detected
#define IGNITION_SLIDETH 0.5    // minimum slide speed to consider it ignition gesture
#define IGNITION_THETATH 80     // minimum twist to consider it ignition gesture
#define IGNITION_TIMEWINDOW 200 // time window to detect ignition gesture [ms]
bool IgnitionGesture() {
  bool ignition = false;
  static uint32_t windowTime = 0;

  if (!windowTime) {  // no condition detected yet
      if (abs(fusor.slideSpeed()) > IGNITION_SLIDETH && fusor.slideDistance()) {
          windowTime = millis();  // slide confirmed, start time window to look for twist
      }
  }
  else {
    if (abs(fusor.theta()) > IGNITION_THETATH) {
        ignition = true;
        windowTime = 0;  // reset window
        // #ifdef DIAGNOSE_PROP
        //     STDOUT.println("IGNITION gesture detected!");
        // #endif
        return true;
    }
    if (millis() - windowTime > IGNITION_TIMEWINDOW) windowTime = 0;  // time window expired
  }
 



  // if (abs(fusor.theta()) > IGNITION_THETATH && abs(fusor.slideSpeed()) > IGNITION_SLIDETH && abs(fusor.slideDistance())) {
  //     // slide must be confirmed (by non-zero distance) during twist, to prevent false positives
  //     #ifdef DIAGNOSE_PROP
  //         STDOUT.println("IGNITION detected!");
  //     #endif
  //     ignition = true;
  //     // return true;
  // }

  #if defined(X_BROADCAST) && defined(BROADCAST_IGNITION)
        struct {
            int16_t up1=0, up2=0, up3=0;
            int16_t down1=0, down2=0, down3=0;
        } ch1_broadcast;
        ch1_broadcast.up1 = (int16_t)(100*fusor.theta());
        ch1_broadcast.up2 = (int16_t)(100*fusor.slideSpeed());
        ch1_broadcast.up3 = (int16_t)(100*fusor.slideDistance());
        if (windowTime) ch1_broadcast.down1 = (int16_t)(millis() - windowTime);
        if(ignition) ch1_broadcast.down2 = 10;

        STDOUT.Broadcast(1, &ch1_broadcast, sizeof(ch1_broadcast));        
  #endif // X_BROADCAST   

  return false;

  }



  #define TAP_ACCTH     userProfile.tapSensitivity.threshold      // acceleration threshold to consider it a clash
  #define TAP_CLASHDBT  60     // clash debounce time: time to ignore acceleration after triggering a clash [ms]
  // #define TAP_MINTIME   userProfile.tapSensitivity.minTime    // minimum time between clashes to call them taps [ms]
  #define TAP_MINTIME   SHAKE_MAX_PER+1    // minimum time between clashes to call them taps [ms]
  #define TAP_MAXTIME   userProfile.tapSensitivity.maxTime    // maximum time between clashes to call them taps [ms]
  #define TAP_QUIETTIME 500    // minimum time between successive double-taps [ms]



// Detect  nTaps taps
void DetectTaps(uint8_t nTaps)  {  // Got acceleration resultant from DoAccel()
  static bool clash=false;      // true if acceleration went over the thresold
  // static uint32_t lastChecked=0;// last time when detection was executed (once @ 10 ms)
  static int32_t lastClash=0;   // negative: time when we started ignoring clashes;  non-negative: time when last clash was detected
  static int16_t tapCounter;    // counts valid taps, with negative penalties
  
  #if defined(X_BROADCAST) && defined(BROADCAST_2TAP)
    static bool Detected2Tap = false;
  #endif

  float acc = fusor.accel().len2();   // len squared (no need for sqrt)
  // 0. Ignore if needed
  uint32_t timeNow = millis();
  // uint32_t dT = timeNow-lastChecked;   // time since last valid clash
  // if (dT < 2) return;
  // lastChecked = timeNow;

  if (lastClash<0) {  // ignore TAP_QUIETTIME [ms], since -lastClash
    if (timeNow+lastClash > TAP_QUIETTIME) {
        lastClash = 0; // stop ignoring
        #if defined(X_BROADCAST) && defined(BROADCAST_2TAP)
            Detected2Tap = false;   // broadcast end of ignore time
        #endif
    }
  }
  else {
    // 1. Detect clashes  
     uint32_t dT = timeNow-lastClash;   // time since last valid clash
    if (acc>TAP_ACCTH) { // over thresold
      if (!clash) { // clash event!
          clash=true;
          // 2. Discriminate between clashed and taps
          if (dT>TAP_QUIETTIME) 
              tapCounter = 1;        // this can only be the first tap, since it came after some long quiet time
          else if (dT<TAP_MINTIME) 
                  tapCounter = 3;     // clashes are too fast, forget about it! Need QUIETTIME silence to resume tap detection
               else if (dT<TAP_MAXTIME)   // valid tap, keep counting!
                  tapCounter++;              
          lastClash = timeNow;
          // STDOUT.println(tapCounter); 
      }
    }
    else { // below threshold
      if (clash && dT>TAP_CLASHDBT) 
        clash=false;
    }

    // 3. Decide on tap event
    if (timeNow-lastClash > TAP_CLASHDBT && tapCounter==nTaps) { // enough taps!
          Event(BUTTON_NONE, EVENT_TAP);   // trigger event
          lastClash = -timeNow;
          #if defined(X_BROADCAST) && defined(BROADCAST_2TAP)
            Detected2Tap = true;  // broadcast double-tap for QUIETTIME [ms]
          #endif
          tapCounter = 0;  
          // STDOUT.println("2xTAP detected! ");     
      }

    // 4. Reset if quiet
    if (tapCounter && timeNow-lastClash > TAP_MAXTIME)  tapCounter = 0;  

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




// #define STAB_TH     userProfile.stabSensitivity.threshold   // stab threshold = minimum XYZ acceleration: 
// #define STAB_DIR    userProfile.stabSensitivity.dir // 2         // stab directionality = ratio of X and YZ acceleration: 2.0f

// default
#define STAB_SPEEDTH 1.5f       // speed threshold
#define STAB_DISTH   0.2f       // distance threshold
#define STAB_SWINGSPEED  100    // maximum swing speed allowed during stab
#define SCREW_THETATH  800      // minimum theta peak to consider a detected stab as a screw
#define SCREW_SWINGSPEED  150   // maximum swing speed allowed during stab


void DetectStabs() {
  static float speedPeak = 0;   // peak slide speed

  float speed = fusor.slideSpeed();
  float distance = fusor.slideDistance();
  float swingSpeed = fusor.swing_speed();
  static enum : int8_t {
      Off = 0,
      Stab = 1,
      Screw = -1
  } stabEvent = Off;

  if (!speed && !distance) {
    speedPeak=0;                     // reset peak slide speed if no slide
    stabEvent = Off;
  }
  else if (speed > speedPeak) speedPeak = speed;  // store peak slide speed

  if (speed && !stabEvent) {  // only check stab if speed is non zero, otherwise we would trigger multiple events due to distance retention during post-slide pause
    if (speedPeak > STAB_SPEEDTH && distance > STAB_DISTH) {  // stab or screw, only on forward slide
      if (fusor.thetaPeak() >= SCREW_THETATH && swingSpeed < SCREW_SWINGSPEED) {  // screw detected
        stabEvent = Screw;
        Event(BUTTON_NONE, EVENT_SCREW); 
      }
      else if (swingSpeed < STAB_SWINGSPEED) {  // stab detected
        stabEvent = Stab;
        Event(BUTTON_NONE, EVENT_STAB);
      }      
      // fusor.UpdateTheta(0);     // reset twist and slide detectors, regardless which (if any) event was generated
      // fusor.UpdateSlide(0);
    }
  }

  #if defined(X_BROADCAST) && defined(BROADCAST_STAB)
        struct {
            int16_t up1=0, up2=0, up3=0;
            int16_t down1=0, down2=0, down3=0;
        } ch1_broadcast;
        ch1_broadcast.up1 = (int16_t)(100*fusor.slideSpeed());
        ch1_broadcast.up2 = (int16_t)(100*speedPeak);
        ch1_broadcast.up3 = (int16_t)(100*fusor.slideDistance());
        ch1_broadcast.down1 = (int16_t)(swingSpeed);                
        ch1_broadcast.down2 = (int16_t)(fusor.thetaPeak());                        
        if (stabEvent==Stab) ch1_broadcast.down3 = 1000;
        if (stabEvent==Screw) ch1_broadcast.down3 = 2000;


        STDOUT.Broadcast(1, &ch1_broadcast, sizeof(ch1_broadcast));        
  #endif // X_BROADCAST   
}


#define DEFLECT_NOISETH  5.0f   // noise threshold
#define DEFLECT_THRESHOLD 30.0f // threshold for deflect detection
void DetectDeflect() {
  
  static float accResultant = 0;    // resultant acceleration squared (no need for sqrt)
  static float peakG = 0;           // peak acceleratin resultant, during an event
  static uint32_t lastTime = 0;     // keep sampling rate constant, otherwise difference is not derivative
  bool deflectEvent = false;


  uint32_t timeNow = millis();
  if (timeNow - lastTime < 10) return;  // sample rate 100 Hz 
  lastTime = timeNow;


  accResultant = fusor.mss().len();     // current resultant of 3-axes accelerations  

  if (accResultant < DEFLECT_NOISETH) {  // below noise threshold, quiet or end of event
    if (peakG > DEFLECT_THRESHOLD) {  // that was the end of a deflect 
      Event(BUTTON_NONE, EVENT_DEFLECT);  // trigger event
      deflectEvent = true;
    }
    peakG = 0;  // reset peak derivative at the end of the event
  }
  else { // event in progress
    if (timeNow - last_clash_ < clash_timeout_) peakG = 0;  //  reset peak if clash is active
    else if (accResultant > peakG) peakG = accResultant;  // store peak acceleration during event
  }


  #if defined(X_BROADCAST) && defined(BROADCAST_DEFLECT)
        struct {
            int16_t up1=0, up2=0, up3=0;
            int16_t down1=0, down2=0, down3=0;
        } ch1_broadcast;
        ch1_broadcast.up1 = (int16_t)(100*accResultant);                
        ch1_broadcast.up2 = (int16_t)(100*peakG);                                         
        if (timeNow - last_clash_ < clash_timeout_) ch1_broadcast.down1 = 1;    // clash active
        if (deflectEvent) ch1_broadcast.down2 = 1;    // deflect event


        STDOUT.Broadcast(1, &ch1_broadcast, sizeof(ch1_broadcast));        
  #endif // X_BROADCAST   
}

  Vec3 accel_;

  // newState = 0, 1 or 255 to toggle
  void StartOrStopTrack(uint8_t newState = 255) { 
#ifdef ENABLE_AUDIO
    // stop track
    if ( newState==0 || ((newState==255 && track_player_)) ) {
      if (!track_player_) return;  // nothing to stop
      track_player_->Stop();
      track_player_->CloseFiles();
      track_player_.Free();
    }  
    
    // start track
    else if ( newState==1 || ((newState==255 && !track_player_)) ) {
      MountSDCard();
      EnableAmplifier();
      char track[MAX_TRACKLEN];
      if (tracks.GetString(current_preset_->track_index, track)) {
        if (!track_player_) track_player_ = GetFreeWavPlayer();
        if (track_player_) {
          track_player_->Play(track);
          tracks.SetCurrent(current_preset_->track_index);
        } 
      }      
    }
#endif
  }

  // Play first ms milliseconds from the track, then fade and stop (by looper)
  void PlayTrackTag(uint32_t ms=2000) {
#ifdef ENABLE_AUDIO
    if(track_player_) track_player_->Stop();  // stop current track, if playing, but don't free it
    StartOrStopTrack(1);  // start track (will reuse player if exists)
    millis_to_stop_track_ = millis() + ms;  // set time to stop track
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
  #if defined(PROFFIEBOARD) || ( defined(SABERPROP) && SABERPROP_VERSION == 'P')
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



  volatile float pending_clash_strength1_ = 0.0;

  uint32_t last_beep_;
  float current_tick_angle_ = 0.0;
  uint32_t millis_to_stop_track_ = 0;  // if set, track should fade out after this millis()

  void Loop() override {
    CallMotion();
    if (clash_pending1_) {
      clash_pending1_ = false;
      Clash(pending_clash_strength1_);
    }
    if (clash_pending_ && millis() - last_clash_ >= clash_timeout_) {
      clash_pending_ = false;
      Clash2(pending_clash_strength_);
    }
    CheckLowBattery();
#ifdef ENABLE_AUDIO
    if (millis_to_stop_track_ && track_player_) 
      if (millis() > millis_to_stop_track_) {
        track_player_->FadeAndStop();
        millis_to_stop_track_ = 0;
      }
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
    #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
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
      case EVENT_TAP: STDOUT.print("Double Tap"); break;
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
      // Clash(10.0);
      SaberBase::DoClash();
      return true;
    }
    if (!strcmp(cmd, "stab")) {
      // Clash(true, 10.0);
      SaberBase::DoStab();
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
  
    if (!strcmp(cmd, "n") || (!strcmp(cmd, "next") && arg)) {
      if (!strcmp(arg, "preset") || !strcmp(arg, "pre")) {
        next_preset_fast();
        return true;
      }
      if (!strcmp(arg, "font")) {
        SetFont(NEXT_INDEXED);
        return true;
      }
      if (!strcmp(arg, "track")) {
        SetTrack(NEXT_INDEXED);
        return true;
      }        
    }

    if (!strcmp(cmd, "p") || (!strcmp(cmd, "prev") && arg)) {
      if (!strcmp(arg, "preset") || !strcmp(arg, "pre")) {
        previous_preset_fast();
        return true;
      }
      if (!strcmp(arg, "font")) {
        SetFont(PREV_INDEXED);
        return true;
      }
      if (!strcmp(arg, "track")) {
        SetTrack(PREV_INDEXED);
        return true;
      }      
    }

    // set_font <fontname>
    if (!strcmp(cmd, "set_font")) {
      if(arg) {
        uint8_t fontIndex = fonts.GetIndex(arg);
        // if (!fontIndex) {
        //   ScanSoundFonts(); // font not indexed, rescan
        //   fontIndex = fonts.GetIndex(arg);  // try again
        // }
        if(fontIndex) {
          SetFont(fontIndex);
          STDOUT.println("set_font-OK");
        }          
        else STDOUT.println("set_font-FAIL"); // font not found even after re-scanning                
        return true;  // command served
      }
    }



    // set_font <fontname>
    if (!strcmp(cmd, "scan_fonts")) {
        ScanSoundFonts(false); // font not indexed, rescan
        STDOUT.println("scan_fonts-START");
        list_fonts(NULL, false);  // don't add guards
        STDOUT.println("scan_fonts-END");
        return true;  // command served
    }

    // track - report current state 
    // track <0/1> - set and report new state 
    if (!strcmp(cmd, "track")) {
      if (arg) {
        if (*arg == '0') StartOrStopTrack(0);
        else if (*arg == '1') StartOrStopTrack(1);
      }
      STDOUT.println("track-START");
      if (track_player_) STDOUT.println("on");
      else STDOUT.println("off");
      STDOUT.println("track-END");      
      return true;
    }

    // // tracktag <ms> - play the first <ms> milliseconds of the track (if exists)
    // if (!strcmp(cmd, "tracktag")) {
    //   PlayTrackTag(atoi(arg));
    //   return true;
    // }


 if (!strcmp(cmd, "scan_tracks")) {
      ScanSoundTracks(false); //rescan
      STDOUT.println("scan_tracks-START");
      list_tracks(NULL, false); // don't add guards
      STDOUT.println("scan_tracks-END");
      return true;  // command served      
    }


    // set_track <trackname>
    if (!strcmp(cmd, "set_track")) {
      uint8_t trackIndex;
      if(arg) { // specified track
        trackIndex = tracks.GetIndex(arg);   // argument should be track name
        // if (!trackIndex) {
        //   ScanSoundTracks(); // track not indexed, rescan
        //   trackIndex = tracks.GetIndex(arg);  // try again
        // }
        if(trackIndex) {
          SetTrack(trackIndex);
          STDOUT.println("set_track-OK");
        }          
        else STDOUT.println("set_track-FAIL"); // track not found even after re-scanning                       
      }
      else { // unspecified track, remove any
        SetTrack(0);
        STDOUT.println("set_track-OK");
      }
         
      return true;  // command served      
    }


    if (!strcmp(cmd, "get_preset")) {
        STDOUT.println("get_preset-START");
        current_preset_->Print();
        STDOUT.println("get_preset-END");
        return true;
    } 


    if (!strcmp(cmd, "save_preset")) {     // save current preset to presets.cod
        if (current_preset_->Overwrite(PRESETS_FILE))
          STDOUT.println("save_preset-OK");
        else
          STDOUT.println("save_preset-FAIL");
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
    // if (!strcmp(cmd, "show_current_preset") || !strcmp(cmd, "show_preset")) {
    //     current_preset_->Print();
    //   return true;
    // }

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
      if(arg) {
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
          case 'a':    
              STDOUT.print("  [V] Master volume = "); STDOUT.println(userProfile.combatVolume);
              STDOUT.print("  [v] Stealth volume = "); STDOUT.println(userProfile.stealthVolume);
              STDOUT.print("  [B] Master brightness = "); STDOUT.println(userProfile.combatBrightness);
              STDOUT.print("  [b] Stealth brightness = "); STDOUT.println(userProfile.stealthBrightness);
              STDOUT.print("  [s] Master sensitivity = "); STDOUT.println(Sensitivity::master); // STDOUT.println(userProfile.masterSensitivity);
              STDOUT.print("  [p] Preset = "); STDOUT.println(userProfile.preset);
              break;
        }
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
                #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
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
                #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
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
        case 'a':     
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
              STDOUT.print(" Tap minTime = "); STDOUT.println(TAP_MINTIME);
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
      #ifndef REPORT_RGB
        STDOUT.println("get_color-START");
      #endif
        if(current_preset_)
        #ifdef REPORT_RGB
          STDOUT.print(currentR); STDOUT.print(", "); STDOUT.print(currentG); STDOUT.print(", "); STDOUT.println(currentB);
        #else
          STDOUT.println(current_preset_->variation);
        #endif
      #ifndef REPORT_RGB
        STDOUT.println("get_color-END");
      #endif
        return true;
    }

    #ifdef REPORT_RGB
      if (!strcmp(cmd, "test_colorvariation")) {
        Color16 testColor(65535,0,0);
        for (uint16_t variation=0; variation<32768; variation+=328) {
            // testColor.rotate(rotationLUT.Get(variation), desatLUT.Get(variation));  // rotate and desaturate color
            SaberBase::SetVariation(variation, true);
            testColor.rotate(SaberBase::GetCurrentRotation(), SaberBase::GetCurrentDesaturation());
            STDOUT.print(variation); STDOUT.print(", "); STDOUT.print(currentR>>8); STDOUT.print(", "); 
            STDOUT.print(currentG>>8); STDOUT.print(", "); STDOUT.println(currentB>>8);
            // STDOUT.print("Rotated with "); STDOUT.print(rotationLUT.Get(variation)); STDOUT.print(" and desaturated with "); STDOUT.println(desatLUT.Get(variation));
        }
        // testColor.rotate(32765);  // last point
        SaberBase::SetVariation(32767, true);
        testColor.rotate(SaberBase::GetCurrentRotation(), SaberBase::GetCurrentDesaturation());
        STDOUT.print("32767, "); STDOUT.print(currentR>>8); STDOUT.print(", "); 
        STDOUT.print(currentG>>8); STDOUT.print(", "); STDOUT.println(currentB>>8);      
        return true;
      }
    #endif


    if (!strcmp(cmd, "set_color")) {
        if (arg) {  // set color variation
          size_t variation = strtol(arg, NULL, 0);
          current_preset_->variation = variation;
          SaberBase::SetVariation(variation, true);
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

  void list_fonts(FileReader* fileWriter, bool guarded = true)
  {
      if(fileWriter)LOCK_SD(true);
      if (guarded) {
        if(fileWriter)fileWriter->Write("list_fonts-START\n");
        else STDOUT.println("list_fonts-START");
      }
      if (fonts.count) {    // fonts should have been scanned by now
        uint8_t currentIndex =  fonts.GetCurrentIndex();   // store current font, we're gonnna read font names sequentially for faster access        
        fonts.SetCurrent(fonts.count);                    // set last font as current, so 'Next' will be the first font in list
        char fontname[MAX_FONTLEN];
        size_t len;
        for (uint8_t i=1; i<=fonts.count; i++) {
            fonts.GetNext(fontname);    // set next font as current and get its name
            if(fileWriter) { fileWriter->Write(fontname); fileWriter->Write("\n"); }
            else STDOUT.println(fontname);
        }

        fonts.SetCurrent(currentIndex);     // restore current font
      }
      
      if (guarded) {
        if(fileWriter)fileWriter->Write("list_fonts-END\n");
        else STDOUT.println("list_fonts-END");
      }
      if(fileWriter)LOCK_SD(false);


  }



  void list_tracks(FileReader* fileWriter=0, bool guarded = true)
  {
      // Tracks are must be in: tracks/*.wav or */tracks/*.wav
      if(fileWriter)LOCK_SD(true);
      if (guarded) {
        if(fileWriter)fileWriter->Write("list_tracks-START\n");
        else STDOUT.println("list_tracks-START");
      }
      
      if (tracks.count) {    // tracks should have been scanned by now
        uint8_t currentIndex =  tracks.GetCurrentIndex();   // store current track, we're gonnna read track names sequentially for faster access        
        tracks.SetCurrent(tracks.count);                    // set last track as current, so 'Next' will be the first  in list
        char trackname[MAX_TRACKLEN];
        size_t len;
        for (uint8_t i=1; i<=tracks.count; i++) {
            tracks.GetNext(trackname);    // set next track as current and get its name
            if(fileWriter) { fileWriter->Write(trackname); fileWriter->Write("\n"); }
            else STDOUT.println(trackname);
        }

        tracks.SetCurrent(currentIndex);     // restore current track
      
      }

      if (guarded) {
        if(fileWriter)fileWriter->Write("list_tracks-END\n");
        else STDOUT.println("list_tracks-END");
      }
      if(fileWriter)LOCK_SD(false);

  }


void list_presets(FileReader* fileWriter = 0) {
     if (fileWriter) {  // write on SD Card
      if(fileWriter)LOCK_SD(true);
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
      if(fileWriter)LOCK_SD(false);
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

#if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
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
    bool stealthMode = false, setStealth = false;
  
    virtual void SetStealth(bool newStealthMode, uint8_t announce = true) {}

  LoopCounter accel_loop_counter_;
};

#endif
