#ifndef SPMENU_H
#define SPMENU_H

#define MENU_MIN_VOLUME     6550    // 10%
#define MENU_MIN_BRIGHTNESS 6550    // 10%
#define TIMEOUT_MENU  60000      // auto-exit menu after 1 minute of inactivity
 

typedef enum SaberPropMenu
{
    Menu_Config = 0,
    MnuConfig_Font = 1,
    MnuConfig_Volume = 2,
    MnuConfig_Brightness = 3,
    MnuConfig_Color = 4,
    MnuConfig_Sensitivity = 5,
    MnuConfig_Preset = 6,
    MnuConfig_Track = 7,
    
    MnuShortcut_Preset = 100
};

// TODO work some more at this class 
// TODO: use RangeStats!
class Segments {
    public: 
    uint16_t iMin;
    uint16_t iMax;
    uint16_t nrMaxSegments;
    uint16_t sStep;
    Segments(){}
    Segments(uint16_t min, uint16_t max, uint16_t dmaxSeg) { Set(min, max, dmaxSeg); }
    
    void Set(uint16_t min, uint16_t max, uint16_t dmaxSeg)
    {   
        if(min > max) {
            iMin = max;
            iMax = min;
        } else {
            iMin = min;
            iMax = max;
        }
        nrMaxSegments = dmaxSeg;
        sStep = (max - min) / (nrMaxSegments - 1);
    }

    uint16_t segmentToValue(uint16_t segment)
    {
        if(segment > nrMaxSegments)segment = nrMaxSegments;
        if(segment)segment -=1; 
        return (segment * sStep) + iMin;
    }

    uint16_t valueToSegment(uint16_t value)
    {
        if(value > iMax) value = iMax;
        else if(value < iMin) value = iMin;
        return ((value - iMin) / (sStep ? sStep : 1))+1;
    }
    // testOnly delete memeber functions 
    void printAllSegments()
    {   
        for(uint32_t i= iMin ; i <= iMax; i+=sStep) {
            STDOUT.print("Values ");STDOUT.print(i);
            STDOUT.print(" Segment ");STDOUT.println(valueToSegment((uint16_t)i));
        }

    }
    void printAllValues()
    {
        for(uint16_t i= 0 ; i <= nrMaxSegments; i++) {
            STDOUT.print("Segment ");STDOUT.print(i);
            STDOUT.print(" Value ");STDOUT.println(segmentToValue(i));
        }
    } 
};

template<class propPrototype>
class menuInterface : public TTMenu<uint16_t>::xMenuActions{
  public:
    bool canBeDestroyed;
    propPrototype *workingProp;
    uint32_t lastTimeActive;
    uint16_t iVolume;     // initial volume 
    uint16_t iBrightness; // initial brightness
    menuInterface(propPrototype* prop) {
      lastTimeActive = millis();
      workingProp = prop;
      canBeDestroyed = false;
    }
    
    virtual ~menuInterface() = default;
    virtual bool CanDestroy() = 0;  
    virtual uint16_t ChangeActions() = 0;

    // Set volume and brightness to some minimums, if too low
    void InitMenuProfile() {
      iVolume = userProfile.masterVolume;     // store previous values, in case we need to restore at cancel
      iBrightness = userProfile.masterBrightness;    
      #ifdef DIAGNOSE_MENU
        STDOUT.print("[InitMenuProfile] saved volume="); STDOUT.print(iVolume); STDOUT.print(" and brightness="); STDOUT.println(iBrightness);
      #endif
      // rise volume if too low
      if (userProfile.masterVolume < MENU_MIN_VOLUME) {
        userProfile.masterVolume = MIN_MASTER_VOLUME;
        #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
        SilentEnableAmplifier(true);         // ... in case it was mute ...
        SilentEnableBooster(true);
        #endif  
        dynamic_mixer.set_volume(VOLUME);     // apply volume
      }
      // rise brightness if too low
      if (userProfile.masterBrightness < MENU_MIN_VOLUME) {
        userProfile.masterBrightness = MIN_MASTER_VOLUME;
      }
    }

    // Restore volume and brightness to either previous values (if canceled) or to stealth mode (if confirmed)
    void RestoreMenuProfile(bool toInitial = false) {
        if (toInitial) {    // restore to initial values (canceled)
          userProfile.masterVolume = iVolume;
          userProfile.masterBrightness = iBrightness;
          #ifdef DIAGNOSE_MENU
            // STDOUT.print("[RestoreMenuProfile] Restored volume="); STDOUT.print(iVolume); STDOUT.print(" and brightness="); STDOUT.println(iBrightness);          
          #endif
        }
        else {  // restore according to stealthMode (confirmed)
          #ifdef DIAGNOSE_MENU
            // STDOUT.print("[RestoreMenuProfile] Restored volume and brightness for stealth"); STDOUT.println(workingProp->stealthMode);
          #endif
          FixStealthMode();     // adjust brightness and volume, if needed
          workingProp->SetStealth(workingProp->stealthMode, false);
        }
        // apply volume
        #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
        if (!userProfile.masterVolume) { // mute
          SilentEnableAmplifier(false);         
          SilentEnableBooster(false);  
        }
        else { // unmute
          SilentEnableAmplifier(true);      
          SilentEnableBooster(true);  
        }
        #endif
        dynamic_mixer.set_volume(VOLUME);     // apply volume
    }

   
}; 
// clas handling the behavoir of change preset menu 
// It give behavoir of the 4 action of menu action Tick , StableState , Ok , Cancel
// Xmenu will call the action
template<class T> 
class shortcutPreset_t :  public menuInterface<T>{
    // behavoir of the tick action 
    public:
    shortcutPreset_t(T *prop) : menuInterface<T>(prop)
    { 
      if(track_player_)restoreTrack = true;
      else restoreTrack = false;
      savedPreset = userProfile.preset;
      // menuInterface<T>::InitMenuVolume();
      // menuInterface<T>::InitMenuBright();
      menuInterface<T>::InitMenuProfile();
      menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, annunciator, 0 , 1);
      SaberBase::DoNewFont();         // announce current font


    }

    uint16_t ChangeActions() override {return 0xFFFF;}  // no action
    bool CanDestroy() override 
    {
      if(!menuInterface<T>::canBeDestroyed && (millis() - menuInterface<T>::lastTimeActive >  TIMEOUT_MENU))   // check if timeout in menu has occuured 
      { 
        Cancel(0); 
        return true;
      }
      return menuInterface<T>::canBeDestroyed;
    }

    private:
    uint16_t savedPreset;
    bool restoreTrack;
    // const uint32_t TIMEOUT_MENU = 60000;    // auto-exit menu after 1 minute of inactivity
    
    void Tick(uint16_t currentValue) override 
    {   
      menuInterface<T>::lastTimeActive = millis(); 
    }
    // behavoir of the stable state 
    void StableState(uint16_t currentValue) override 
    { 
      menuInterface<T>::lastTimeActive = millis();
      menuInterface<T>::workingProp->FadeAndStopSS();
      if (currentValue && currentValue <= presets.size()) {
        
        if (track_player_) 
        {     // make sure track player is freed 
          track_player_->Stop();
          track_player_->CloseFiles();
          track_player_.Free();
        }
        #if defined(SABERPROP) && SABERPROP_VERSION == 'Z' // single-font        
          if (SaberBase::IsOn()) menuInterface<T>::workingProp->SetPresetFast(currentValue, false);
          else menuInterface<T>::workingProp->SetPreset(currentValue, false);
        #else // multi-font
          if (SaberBase::IsOn()) menuInterface<T>::workingProp->SetPresetFast(currentValue);
          else menuInterface<T>::workingProp->SetPreset(currentValue, true);        
        #endif
        if(restoreTrack)
        {
          track_player_ = GetFreeWavPlayer();
          if (track_player_) { 
            uint8_t track_index = menuInterface<T>::workingProp->current_preset_->track_index;
            if (track_index) {
              char track[MAX_TRACKLEN];
              if (tracks.GetString(track_index, track))
                track_player_->Play(track);
            }            
          
          
            // track_player_->Play(menuInterface<T>::workingProp->current_preset_->track);
          }
        }
      }
    }
    // behavoir of OK 
    void Ok(uint16_t currentValue) override 
    {
      // menuInterface<T>::RestoreVolume();    // restore volume and brightness 
      // menuInterface<T>::RestoreBright(); 
      menuInterface<T>::RestoreMenuProfile();
      if(savedPreset != userProfile.preset) 
      {
        // save selected profile to user profile 
        if (WriteUserProfile(PROFILE_FILE, 1)) STDOUT.println("save_profile-OK");
        else STDOUT.println("save_profile-FAIL");
      }
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
      menuInterface<T>::canBeDestroyed = true;

    }
    // behavoir of cancel 
    void Cancel(uint16_t currentValue) override 
    {   
      if(savedPreset != userProfile.preset) 
      {

        // restore old settings, silently 
        menuInterface<T>::workingProp->SetPreset(savedPreset, false);
        savedPreset = userProfile.preset; // in case we have a spurios cancel event 
      }
      // menuInterface<T>::RestoreVolume();        // restore volume and brightness 
      // menuInterface<T>::RestoreBright();
      menuInterface<T>::RestoreMenuProfile();
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0 , 1); 
      menuInterface<T>::canBeDestroyed = true;  // mark that menu can be destroy 

    }
};

// // clas handling the behavoir of profile menu 
// // It give behavoir of the 4 action of menu action Tick , StableState , Ok , Cancel
// // Xmenu will call the action
template<class T> 
class menuConfig_t : public menuInterface<T> {
private:
    uint16_t savedSelection;

public: 
    menuConfig_t(T* prop): menuInterface<T>(prop) {
      // STDOUT.println("[menudebug]: menuConfig_t constructor initializes object");
      savedSelection = 0;
      // menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, annunciator, 0 , 1);
      menuInterface<T>::InitMenuProfile();

      // if (installConfig.monochrome)
      //   menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, longVolume, 0 , 1);
      // else
      //   menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, longColor, 0 , 1);

      menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, longSoundfont, 0 , 1);


    }

    uint16_t ChangeActions() override {return savedSelection;}
    
    bool CanDestroy() override
    {
      if(!menuInterface<T>::canBeDestroyed && (millis() - menuInterface<T>::lastTimeActive >  TIMEOUT_MENU))   // check if timeout in menu has occuured 
      { 
        Cancel(0); 
        return true;
      }
      return menuInterface<T>::canBeDestroyed;
    }

private: 
    // const uint32_t TIMEOUT_MENU = 120000;
    // behavoir of tick 
    void Tick(uint16_t currentValue) override 
    {   
      menuInterface<T>::lastTimeActive = millis();

    }
    // behavoir of the stable state 
    void StableState(uint16_t currentValue) override 
    { 
      menuInterface<T>::lastTimeActive = millis();   
      switch(currentValue) {
          case MnuConfig_Color: menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, menuVoices_id::color, 0, 1); break;
          case MnuConfig_Sensitivity: menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, menuVoices_id::sensitivity, 0, 1); break;
          case MnuConfig_Brightness : menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, menuVoices_id::brightness, 0, 1); break;
          case MnuConfig_Volume: menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, menuVoices_id::volume, 0, 1); break;
          case MnuConfig_Font: menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, menuVoices_id::soundfont, 0, 1); break;
          case MnuConfig_Track: menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, menuVoices_id::soundtrack, 0, 1); break;
          case MnuConfig_Preset: menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, menuVoices_id::preset, 0, 1); break;
      }
      savedSelection = currentValue;
    }
    // behavoir of OK 
    void Ok(uint16_t currentValue) override 
    {
      menuInterface<T>::lastTimeActive = millis();   // not really neccesarry just to be sure a false positive timeout cancel is ocured 

      if(!savedSelection) savedSelection = currentValue; // move forward with default 
      // STDOUT.print("Current value = "); STDOUT.println(currentValue);
      // menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
      menuInterface<T>::canBeDestroyed = true;
      menuInterface<T>::RestoreMenuProfile();  
  

    }
    // behavoir of cancel 
    void Cancel(uint16_t currentValue) override 
    { 

      menuInterface<T>::RestoreMenuProfile();  
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0, 1);
      menuInterface<T>::canBeDestroyed = true;
      savedSelection = 0;

    }
};


template<class T> 
class submenuColor_t : public menuInterface<T> {
  public: 
    submenuColor_t(T* prop, uint16_t nrSeg): menuInterface<T>(prop) 
    { 
      // STDOUT.println("[menudebug]: submenuColor_t constructor initializes object");
      initialColor = menuInterface<T>::workingProp->current_preset_->variation;               // save inital color 
      // menuInterface<T>::InitMenuVolume();
      // menuInterface<T>::InitMenuBright();
      menuInterface<T>::InitMenuProfile();

    }
    uint16_t ChangeActions() override {return Menu_Config;}  // restore state before menu config 
    bool CanDestroy() override
    {
      if(!menuInterface<T>::canBeDestroyed && (millis() - menuInterface<T>::lastTimeActive >  TIMEOUT_MENU))   // check if timeout in menu has occuured 
      { 
        Cancel(0); 
        return true;
      }
      return menuInterface<T>::canBeDestroyed;
    }

private: 
    // const uint32_t TIMEOUT_MENU = 120000;
    uint32_t initialColor;

    void Tick(uint16_t currentValue) override 
    {   
      menuInterface<T>::lastTimeActive = millis();
      SaberBase::SetVariation(currentValue << 7, true);
    }
    // behavoir of the stable state 
    void StableState(uint16_t currentValue) override 
    { 
      menuInterface<T>::lastTimeActive = millis();   
    }
    // behavoir of OK 
    void Ok(uint16_t currentValue) override 
    {
      menuInterface<T>::lastTimeActive = millis();   // not really neccesarry just to be sure a false positive timeout cancel is ocured 

      menuInterface<T>::workingProp->current_preset_->variation = currentValue << 7;
      SaberBase::SetVariation(menuInterface<T>::workingProp->current_preset_->variation, true);
      if (!WriteUserProfile(PROFILE_FILE, 1)) STDOUT.println("Failed saving profile!");
      menuInterface<T>::RestoreMenuProfile();


      // menuInterface<T>::RestoreVolume();    // restore settings before settings 
      // menuInterface<T>::RestoreBright(); 
      // if(initialColor != menuInterface<T>::workingProp->current_preset_->variation)
      // {
      //  if (WriteUserProfile(PROFILE_FILE, 1)) STDOUT.println("save_profile-OK");
      //   else STDOUT.println("save_profile-FAIL");
      // }
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
      menuInterface<T>::canBeDestroyed = true;
      // menuInterface<T>::RestoreVolume();
      // menuInterface<T>::RestoreBright(); 
    }
    // behavoir of cancel 
    void Cancel(uint16_t currentValue) override 
    { 
      // menuInterface<T>::RestoreVolume();
      // menuInterface<T>::RestoreBright();
      SaberBase::SetVariation(initialColor, true);
      menuInterface<T>::RestoreMenuProfile(); 
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0 , 1);
      menuInterface<T>::canBeDestroyed = true;

    }
};

template<class T> 
class submenuBrightness_t : public menuInterface<T> {
  public: 
    // submenuBrightness_t(T* prop, uint16_t nrSeg): menuInterface<T>(prop) {
    //   initialBrighntness = userProfile.masterBrightness;
    //   seg = Segments(MIN_MASTER_BRIGHTNESS, 65535, nrSeg);
    //   // menuInterface<T>::InitMenuVolume();
    //   // menuInterface<T>::InitMenuBright(); 
    //   menuInterface<T>::InitMenuProfile();

    // }
    submenuBrightness_t(T* prop, uint16_t nrSeg): menuInterface<T>(prop) {
      // STDOUT.println("[menudebug]: submenuBrightness_t constructor initializes object");
      // initialBrighntness = userProfile.masterBrightness;
      if (prop->stealthMode) {
          seg = Segments(MIN_MASTER_BRIGHTNESS, STEALTH_MAX_BRIGHTNESS, nrSeg);    // 0 -> stealth MAX if in stealth mode
          // STDOUT.print("[menudebug]: Segment limits: "); STDOUT.print(MIN_MASTER_BRIGHTNESS); STDOUT.print(" -> "); STDOUT.println(STEALTH_MAX_BRIGHTNESS); 
      }
      else {
          seg = Segments(userProfile.stealthBrightness, 65535, nrSeg);    // stealth -> full if not in stealth mode
          // STDOUT.print("[menudebug]: Segment limits: "); STDOUT.print(userProfile.stealthBrightness); STDOUT.print(" -> "); STDOUT.println(65535); 
      }
      // menuInterface<T>::InitMenuVolume();
      // menuInterface<T>::InitMenuBright(); 
      menuInterface<T>::InitMenuProfile();

    }    
    uint16_t ChangeActions() override {return Menu_Config;} // restore state before menu config 
    bool CanDestroy() override
    {
      if(!menuInterface<T>::canBeDestroyed && (millis() - menuInterface<T>::lastTimeActive >  TIMEOUT_MENU))   // check if timeout in menu has occuured 
      { 
        Cancel(0); 
        return true;
      }
      return menuInterface<T>::canBeDestroyed;
    }

private: 
    // const uint32_t TIMEOUT_MENU = 120000;
    // uint16_t initialBrighntness;
    Segments seg; 
    // behavoir of tick 
    void Tick(uint16_t currentValue) override 
    {   
      menuInterface<T>::lastTimeActive = millis();
      uint16_t tmp = seg.segmentToValue(currentValue); 
      if (tmp <= MIN_MASTER_BRIGHTNESS) tmp=0;
      userProfile.masterBrightness = tmp; 
    }
    // behavoir of the stable state 
    void StableState(uint16_t currentValue) override 
    { 
      menuInterface<T>::lastTimeActive = millis();   
    }
    // behavoir of OK 
    void Ok(uint16_t currentValue) override 
    {
      STDOUT.print("[menuProfileBrightness.Ok] currentValue = "); STDOUT.print(currentValue); STDOUT.print(", saved brightness = "); STDOUT.println(userProfile.masterBrightness);      
      if (menuInterface<T>::workingProp->stealthMode) {  // save current as stealth
          userProfile.stealthBrightness = userProfile.masterBrightness;
      } else {
          userProfile.combatBrightness = userProfile.masterBrightness;
      }

      menuInterface<T>::RestoreMenuProfile();
      if (!WriteUserProfile(PROFILE_FILE, 1)) STDOUT.println("Failed saving profile!");
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
      menuInterface<T>::canBeDestroyed = true; 
      menuInterface<T>::lastTimeActive = millis();   // not really neccesarry just to be sure a false positive timeout cancel is ocured      
    }
    // behavoir of cancel 
    void Cancel(uint16_t currentValue) override 
    {   
      // if(initialBrighntness != userProfile.masterBrightness)
      //   userProfile.masterBrightness = initialBrighntness;
      STDOUT.print("[menuProfileBrightness.Cancel currentValue = "); STDOUT.println(currentValue);
      menuInterface<T>::RestoreMenuProfile(true);   // restore to initial, not current values
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0 , 1);
      menuInterface<T>::canBeDestroyed = true;
      // menuInterface<T>::RestoreVolume();
      // menuInterface<T>::RestoreBright();
    }
};


template<class T> 
class submenuVolume_t : public menuInterface<T> {
  public: 
    submenuVolume_t(T* prop, uint16_t nrSeg): menuInterface<T>(prop) {
      // initialVolume = userProfile.masterVolume;
      // seg = Segments(MIN_MASTER_VOLUME, 65535, nrSeg);
      // menuInterface<T>::InitMenuVolume();
      // menuInterface<T>::InitMenuBright(); 
      // STDOUT.println("[menudebug]: submenuVolume_t constructor initializes object");
      if (prop->stealthMode) {
          seg = Segments(MIN_MASTER_VOLUME, STEALTH_MAX_VOLUME, nrSeg);    // 0 -> stealth MAX if in stealth mode
          // STDOUT.print("[menudebug]: Segment limits: "); STDOUT.print(MIN_MASTER_VOLUME); STDOUT.print(" -> "); STDOUT.println(STEALTH_MAX_VOLUME); 
      }
      else {
          seg = Segments(userProfile.stealthVolume, 65535, nrSeg);    // stealth -> full if not in stealth mode
          // STDOUT.print("[menudebug]: Segment limits: "); STDOUT.print(userProfile.stealthVolume); STDOUT.print(" -> "); STDOUT.println(65535); 
      }
      menuInterface<T>::InitMenuProfile();

    }
    uint16_t ChangeActions() override {return Menu_Config;} // restore state before menu config 
    bool CanDestroy() override
    {
      if(!menuInterface<T>::canBeDestroyed && (millis() - menuInterface<T>::lastTimeActive >  TIMEOUT_MENU))   // check if timeout in menu has occuured 
      { 
        Cancel(0); 
        return true;
      }
      return menuInterface<T>::canBeDestroyed;
    }

private: 
    // const uint32_t TIMEOUT_MENU = 120000;
    // uint16_t initialVolume;
    Segments seg;
    // behavoir of tick 
    void Tick(uint16_t currentValue) override 
    {   
      menuInterface<T>::lastTimeActive = millis();
      uint16_t tmp = seg.segmentToValue(currentValue); 
      if (tmp<=MIN_MASTER_VOLUME) {
        tmp = 0;
        #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
        SilentEnableAmplifier(false);      
        SilentEnableBooster(false);
        #endif
      }
      else {
        #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
        SilentEnableAmplifier(true);      
        SilentEnableBooster(true);
        #endif
      }
      userProfile.masterVolume = tmp;
      dynamic_mixer.set_volume(VOLUME);

    }
    // behavoir of the stable state 
    void StableState(uint16_t currentValue) override 
    { 
      menuInterface<T>::lastTimeActive = millis();   
    }
    // behavoir of OK 
    void Ok(uint16_t currentValue) override 
    {
      // STDOUT.print("[menudebug]: menuProfileVolume.Ok sets currentValue = "); STDOUT.print(currentValue); STDOUT.print(", saved volume = "); STDOUT.println(userProfile.masterVolume);      
      if (menuInterface<T>::workingProp->stealthMode) {  // save current as stealth
          userProfile.stealthVolume = userProfile.masterVolume;
      } else {
         userProfile.combatVolume = userProfile.masterVolume;
      }


      menuInterface<T>::RestoreMenuProfile();   // also fix stealth mode
      if (!WriteUserProfile(PROFILE_FILE, 1)) STDOUT.println("Failed saving profile!");
      menuInterface<T>::lastTimeActive = millis();   // not really neccesarry just to be sure a false positive timeout cancel is ocured 
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
      menuInterface<T>::canBeDestroyed = true; 


    }
    // behavoir of cancel 
    void Cancel(uint16_t currentValue) override 
    {   
      // STDOUT.println(currentValue);
      // if(initialVolume != userProfile.masterVolume)
        // userProfile.masterVolume = initialVolume;
      STDOUT.print("[menuProfileVolume.Cancel] currentValue = "); STDOUT.println(currentValue);
      menuInterface<T>::RestoreMenuProfile(true); // restore to initial values
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0, 1);
      menuInterface<T>::canBeDestroyed = true;
      // menuInterface<T>::RestoreVolume();
      // menuInterface<T>::RestoreBright();
    }
};


template<class T> 
class submenuSensitivity_t : public menuInterface<T> {
  public: 
    submenuSensitivity_t(T* prop): menuInterface<T>(prop) 
    { 
      initialSens = Sensitivity::master;               // save inital sensitivity 
      menuInterface<T>::InitMenuProfile();     
      // STDOUT.println("[menudebug]: submenuSensitivity_t constructor initializes object");

    }
    uint16_t ChangeActions() override {return Menu_Config;}  // restore state before menu config 
    bool CanDestroy() override
    {
      if(!menuInterface<T>::canBeDestroyed && (millis() - menuInterface<T>::lastTimeActive >  TIMEOUT_MENU))   // check if timeout in menu has occuured 
      { 
        Cancel(0); 
        return true;
      }
      return menuInterface<T>::canBeDestroyed;
    }

private: 
    // const uint32_t TIMEOUT_MENU = 120000;
    uint32_t initialSens;

    void Tick(uint16_t currentValue) override 
    {   
      menuInterface<T>::lastTimeActive = millis();
      if (currentValue < 32) {
        userProfile.masterBrightness = currentValue * 2048;     // signal current sensitivity with brightness & volume
        userProfile.masterVolume = currentValue * 2048; 
      }
      else {
        userProfile.masterBrightness = 65535;     // signal current sensitivity with brightness & volume
        userProfile.masterVolume = 65535;         
      }
      dynamic_mixer.set_volume(VOLUME);
      #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
      if (userProfile.masterVolume<=MIN_MASTER_VOLUME) {
        SilentEnableAmplifier(false);      
        SilentEnableBooster(false);
      }
      else {
        SilentEnableAmplifier(true);      
        SilentEnableBooster(true);
      }
      #endif 
    }

    // behavoir of the stable state 
    void StableState(uint16_t currentValue) override 
    { if (currentValue==32) ApplySensitivities(255);  // max
      else ApplySensitivities(8 * currentValue);  // 0...248
      menuInterface<T>::lastTimeActive = millis();   
    }

    // behavoir of OK 
    void Ok(uint16_t currentValue) override 
    {
      menuInterface<T>::lastTimeActive = millis();   // not really neccesarry just to be sure a false positive timeout cancel is ocured 

      if (currentValue==32) ApplySensitivities(255);  // max
      else ApplySensitivities(8 * currentValue);  // 0...248
      if (!WriteUserProfile(PROFILE_FILE, 1)) STDOUT.println("Failed saving profile!");
      menuInterface<T>::RestoreMenuProfile(true);

      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
      menuInterface<T>::canBeDestroyed = true;

    }
    // behavoir of cancel 
    void Cancel(uint16_t currentValue) override 
    { 
      ApplySensitivities(initialSens);
      menuInterface<T>::RestoreMenuProfile(true); 
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0 , 1);
      menuInterface<T>::canBeDestroyed = true;

    }
};


template<class T> 
class submenuFont_t : public menuInterface<T> {
private:
    uint16_t savedFont;
public: 
    submenuFont_t(T* prop): menuInterface<T>(prop) 
    { 
        // Initialization specific to the font menu
        menuInterface<T>::InitMenuProfile();
      savedFont = menuInterface<T>::workingProp->current_preset_->font_index;
      // STDOUT.print("[menudebug]: submenuFont_t constructor initializes object, current font is "); STDOUT.println(savedFont);
        // Example: Play a sound or display the current font
    }

    uint16_t ChangeActions() override {
        // Define how font changes are handled
        return Menu_Config; // Return to the config menu after changing the font
    }

    bool CanDestroy() override {
        // Implement conditions under which this menu can be destroyed
        if(!menuInterface<T>::canBeDestroyed && (millis() - menuInterface<T>::lastTimeActive > TIMEOUT_MENU)) {
            Cancel(0); 
            return true;
        }
        return menuInterface<T>::canBeDestroyed;
    }

    void Tick(uint16_t currentValue) override {
        // Handle tick events, such as updating the display or previewing fonts
        menuInterface<T>::lastTimeActive = millis();
    }

    void StableState(uint16_t currentValue) override {
      menuInterface<T>::lastTimeActive = millis();
      menuInterface<T>::workingProp->SetFont(currentValue);
    }

    void Ok(uint16_t currentValue) override {
        // Confirm font selection and perform necessary actions
        menuInterface<T>::RestoreMenuProfile();
        
        if(savedFont != currentValue) 
        {
          // save selected profile to user profile 
          // STDOUT.print("[menudebug]: currentValue="); STDOUT.print(currentValue);
          // STDOUT.print(", font changed to "); STDOUT.print(menuInterface<T>::workingProp->current_preset_->font_index); STDOUT.println(", will save current preset.");
          if (menuInterface<T>::workingProp->current_preset_->Overwrite(PRESETS_FILE)) 
            STDOUT.println("save_preset-OK");
          else 
            STDOUT.println("save_preset-FAIL");
        }
        // Example: Save the selected font to the user profile
        menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
        menuInterface<T>::canBeDestroyed = true;

    }

    void Cancel(uint16_t currentValue) override {
        if(savedFont != menuInterface<T>::workingProp->current_preset_->font_index) {
          // savedFont = menuInterface<T>::workingProp->current_preset_->font_index;
          // STDOUT.print("[menudebug]: currentValue="); STDOUT.print(currentValue);
          // STDOUT.print(", font restored to "); STDOUT.print(savedFont); STDOUT.println(", will not save");
          menuInterface<T>::workingProp->SetFont(savedFont, true);    // silent restore initial font 
        }

        // Handle cancellation, such as reverting to the previous font
        menuInterface<T>::RestoreMenuProfile(true); // Restore to initial values
        menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0, 1);
        menuInterface<T>::canBeDestroyed = true;
    }
};



template<class T> 
class submenuTrack_t : public menuInterface<T> {
private:
    uint16_t savedTrack;
    // bool restoreTrack;
public: 
    submenuTrack_t(T* prop): menuInterface<T>(prop) 
    { 
        // Initialization specific to the font menu
        menuInterface<T>::InitMenuProfile();
        // Example: Play a sound or display the current font
      savedTrack = menuInterface<T>::workingProp->current_preset_->track_index;
      // if (track_player_)restoreTrack = true;
      // else restoreTrack = false;
      // STDOUT.println("[menudebug]: submenuTrack_t constructor initializes object");      
      StableState(savedTrack);    // init current menu option
    }

    uint16_t ChangeActions() override {
        // Define how font changes are handled
        return Menu_Config; // Return to the config menu after changing the font
    }

    bool CanDestroy() override {
        // Implement conditions under which this menu can be destroyed
        if(!menuInterface<T>::canBeDestroyed && (millis() - menuInterface<T>::lastTimeActive > TIMEOUT_MENU)) {
            Cancel(0); 
            return true;
        }
        return menuInterface<T>::canBeDestroyed;
    }

    void Tick(uint16_t currentValue) override {
        // Handle tick events, such as updating the display or previewing fonts
        menuInterface<T>::lastTimeActive = millis();
    }

    void StableState(uint16_t currentValue) override {
        // Handle stable state, such as finalizing font selection
        menuInterface<T>::lastTimeActive = millis();
        menuInterface<T>::workingProp->SetTrack(currentValue, true);  // play current track and force start                 
    }

    void Ok(uint16_t currentValue) override {
      menuInterface<T>::RestoreMenuProfile();        
      if(savedTrack != currentValue) 
      {
        if (menuInterface<T>::workingProp->current_preset_->Overwrite(PRESETS_FILE)) 
          STDOUT.println("save_preset-OK");
        else 
          STDOUT.println("save_preset-FAIL");
      }
      // if (!restoreTrack) menuInterface<T>::workingProp->StartOrStopTrack(0);    // stop track if it was not playing when entering menu
      // Example: Save the selected font to the user profile
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
      menuInterface<T>::canBeDestroyed = true;
    }

    void Cancel(uint16_t currentValue) override {
       if(savedTrack != menuInterface<T>::workingProp->current_preset_->font_index) {
          menuInterface<T>::workingProp->SetTrack(savedTrack, false, true);    // silent restore initial track 
        }      
        // if (!restoreTrack) menuInterface<T>::workingProp->StartOrStopTrack(0);    // stop track if it was not playing when entering menu
        // Handle cancellation, such as reverting to the previous font
        menuInterface<T>::RestoreMenuProfile(true); // Restore to initial values
        menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0, 1);
        menuInterface<T>::canBeDestroyed = true;
    }
};



template<class T> 
class submenuPreset_t : public menuInterface<T> {
  private:
    uint16_t savedPreset;
    bool restoreTrack;  
  public: 
    submenuPreset_t(T* prop): menuInterface<T>(prop) 
    { 
      // STDOUT.println("[menudebug]: submenuPreset_t constructor initializes object");
      // menuInterface<T>::InitMenuProfile();

      if(track_player_)restoreTrack = true;
      else restoreTrack = false;
      savedPreset = userProfile.preset;
      menuInterface<T>::InitMenuProfile();
      // menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, annunciator, 0 , 1);
      SaberBase::DoNewFont();         // announce current font


    }
    uint16_t ChangeActions() override {return Menu_Config;}  // restore state before menu config 
    bool CanDestroy() override
    {
      if(!menuInterface<T>::canBeDestroyed && (millis() - menuInterface<T>::lastTimeActive >  TIMEOUT_MENU))   // check if timeout in menu has occuured 
      { 
        Cancel(0); 
        return true;
      }
      return menuInterface<T>::canBeDestroyed;
    }

    void Tick(uint16_t currentValue) override 
    {   
        menuInterface<T>::lastTimeActive = millis(); 
    }
    // behavoir of the stable state 
    void StableState(uint16_t currentValue) override 
    { 
      menuInterface<T>::lastTimeActive = millis();
      menuInterface<T>::workingProp->FadeAndStopSS();
      if (currentValue && currentValue <= presets.size()) {
        
        if (track_player_) {     // stop and free track player
          track_player_->Stop();
          track_player_->CloseFiles();
          track_player_.Free();
        }
        #if defined(SABERPROP) && SABERPROP_VERSION == 'Z' // single-font        
          if (SaberBase::IsOn()) menuInterface<T>::workingProp->SetPresetFast(currentValue, false);
          else menuInterface<T>::workingProp->SetPreset(currentValue, false);
        #else // multi-font
          if (SaberBase::IsOn()) menuInterface<T>::workingProp->SetPresetFast(currentValue);
          else menuInterface<T>::workingProp->SetPreset(currentValue, true);        
        #endif
        uint8_t track_index = menuInterface<T>::workingProp->current_preset_->track_index;
        // if(restoreTrack && track_index) StartOrStopTrack(1);    
        if(restoreTrack && track_index) { // start track if needed
          char track[MAX_TRACKLEN];
          if (tracks.GetString(track_index, track)) {
            track_player_ = GetFreeWavPlayer();
            if (track_player_) track_player_->Play(track);
          }
        }         
      }
    }
    // behavoir of OK 
    void Ok(uint16_t currentValue) override 
    {
      menuInterface<T>::RestoreMenuProfile();
      if(savedPreset != userProfile.preset) 
      {
        // save selected preset to user profile 
        if (WriteUserProfile(PROFILE_FILE, 1)) STDOUT.println("save_profile-OK");
        else STDOUT.println("save_profile-FAIL");
      }
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
      menuInterface<T>::canBeDestroyed = true;
    }
    // behavoir of cancel 
    void Cancel(uint16_t currentValue) override 
    { 
      if(savedPreset != userProfile.preset) 
      { // restore old settings, silently 
        menuInterface<T>::workingProp->SetPreset(savedPreset, false);
        savedPreset = userProfile.preset; // in case we have a spurios cancel event 
      }

      menuInterface<T>::RestoreMenuProfile();
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0 , 1); 
      menuInterface<T>::canBeDestroyed = true;  // mark that menu can be destroyed

    }
};


// template<class T> 
// class submenuPreset_t : public menuInterface<T> {
// public: 
//     submenuPreset_t(T* prop): menuInterface<T>(prop) 
//     { 
//         // Initialization specific to the font menu
//         menuInterface<T>::InitMenuProfile();
//       STDOUT.println("[menudebug]: submenuPreset_t constructor initializes object");

//         // Example: Play a sound or display the current font
//     }

//     uint16_t ChangeActions() override {
//         // Define how font changes are handled
//         return Menu_Config; // Return to the config menu after changing the font
//     }

//     bool CanDestroy() override {
//         // Implement conditions under which this menu can be destroyed
//         if(!menuInterface<T>::canBeDestroyed && (millis() - menuInterface<T>::lastTimeActive > TIMEOUT_MENU)) {
//             Cancel(0); 
//             return true;
//         }
//         return menuInterface<T>::canBeDestroyed;
//     }

//     void Tick(uint16_t currentValue) override {
//         // Handle tick events, such as updating the display or previewing fonts
//         menuInterface<T>::lastTimeActive = millis();
//     }

//     void StableState(uint16_t currentValue) override {
//         // Handle stable state, such as finalizing font selection
//         menuInterface<T>::lastTimeActive = millis();
//     }

//     void Ok(uint16_t currentValue) override {
//         // Confirm font selection and perform necessary actions
//         menuInterface<T>::RestoreMenuProfile();
//         // Example: Save the selected font to the user profile
//         menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
//         menuInterface<T>::canBeDestroyed = true;
//     }

//     void Cancel(uint16_t currentValue) override {
//         // Handle cancellation, such as reverting to the previous font
//         menuInterface<T>::RestoreMenuProfile(true); // Restore to initial values
//         menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0, 1);
//         menuInterface<T>::canBeDestroyed = true;
//     }
// };

#endif // SPMENU_H