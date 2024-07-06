#ifndef ULTRAMENU_H
#define ULTRAMENU_H

#define MENU_MIN_VOLUME     6550    // 10%
#define MENU_MIN_BRIGHTNESS 6550    // 10%
#define TIMEOUT_MENU  60000      // auto-exit menu after 1 minute of inactivity
 

typedef enum UltrasaberMenu
{
    xUltraMenu_Profile = 0,
    xUltraMenu_Preset = 1,
    xUltraMenu_Color = 2,
    xUltraMenu_Sensitivity = 3,
    xUltraMenu_Bright = 4,
    xUltraMenu_Volume = 5,
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
    virtual bool CanDestroy() = 0;  // TODO
    virtual uint16_t ChangeActions() = 0;

    // Set volume and brightness to some minimums, if too low
    void InitMenuProfile() {
      iVolume = userProfile.masterVolume;     // store previous values, in case we need to restore at cancel
      iBrightness = userProfile.masterBrightness;    
      STDOUT.print("[InitMenuProfile] saved volume="); STDOUT.print(iVolume); STDOUT.print(" and brightness="); STDOUT.println(iBrightness);
      // rise volume if too low
      if (userProfile.masterVolume < MENU_MIN_VOLUME) {
        userProfile.masterVolume = MIN_MASTER_VOLUME;
        #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
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
    void RestoreProfile(bool toInitial = false) {
        if (toInitial) {    // restore to initial values (canceled)
          userProfile.masterVolume = iVolume;
          userProfile.masterBrightness = iBrightness;
          STDOUT.print("[RestoreProfile] Restored volume="); STDOUT.print(iVolume); STDOUT.print(" and brightness="); STDOUT.println(iBrightness);          
        }
        else {  // restore according to stealthMode (confirmed)
          STDOUT.print("[RestoreProfile] Restored volume and brightness for stealth"); STDOUT.println(workingProp->stealthMode);
          FixStealthMode();     // adjust brightness and volume, if needed
          workingProp->SetStealth(workingProp->stealthMode, false);
        }
        // apply volume
        #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
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

    // void InitMenuVolume()
    // { 
    //   if(userProfile.masterVolume < MIN_MASTER_VOLUME) {
    //     SilentEnableAmplifier(true);         // ... in case it was mute ...
    //     SilentEnableBooster(true);        
    //     userProfile.masterVolume = MIN_MASTER_VOLUME;  // set a min volume to hear the menu 
    //     dynamic_mixer.set_volume(VOLUME);

    //   } 

    //   // if(workingProp->stealthMode) {
    //   //   SilentEnableAmplifier(true);         // enable booster 
    //   //   SilentEnableBooster(true);
    //   //   // ivolume = workingProp->stealthOLDVolume;  // save initial stealth volume 
    //   //   if(userProfile.masterVolume < MIN_MASTER_VOLUME ) 
    //   //   {
    //   //     userProfile.masterVolume = MIN_MASTER_VOLUME;  // set a min volume to hear the menu 
    //   //     dynamic_mixer.set_volume(VOLUME);
    //   //   } 
    //   // } 
    // }

    // void RestoreVolume()
    // {
    //   if(workingProp->stealthMode) {    // we are in stelatlh 
    //     // // SilentEnableAmplifier(false); // turn off booster to stealth mode 
    //     // // SilentEnableBooster(false);
    //     // if(userProfile.masterVolume != workingProp->stealthOLDVolume) // we set a new volume , set it 
    //     // {
    //     //   // workingProp->stealthOLDVolume = ivolume;
    //     //   userProfile.masterVolume = workingProp->stealthOLDVolume;
    //     //   //dynamic_mixer.set_volume(VOLUME);
    //     // }
    //     workingProp->setStealth = true;   // set stealth , because we still want okay/cancel  volume 
    //   }
    // }

    // void InitMenuBright()
    // { 
    //   if(userProfile.masterBrightness < MIN_MASTER_BRIGHTNESS) 
    //       userProfile.masterBrightness = MIN_MASTER_BRIGHTNESS;   // changed to min 
    //   // if(workingProp->stealthMode) {
    //   //   // ibrightness = workingProp->stealthOLDBrightness;
    //   //   if(userProfile.masterBrightness < MIN_MASTER_BRIGHTNESS) 
    //   //     userProfile.masterBrightness = MIN_MASTER_BRIGHTNESS;   // changed to min 
    //   // } 
    // }


    
    // void RestoreBright()
    // { 
    //   if(workingProp->stealthMode) {
    //     // if(userProfile.masterBrightness != workingProp->stealthOLDBrightness)
    //     // {
    //     //   userProfile.masterBrightness = workingProp->stealthOLDBrightness;
    //     // }
    //     workingProp->setStealth = true;
    //   }
    // }   
}; 
// clas handling the behavoir of change preset menu 
// It give behavoir of the 4 action of menu action Tick , StableState , Ok , Cancel
// Xmenu will call the action
template<class T> 
class menuPreset_t :  public menuInterface<T>{
    // behavoir of the tick action 
    public:
    menuPreset_t(T *prop) : menuInterface<T>(prop)
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
        #if defined(ULTRAPROFFIE) && ULTRAPROFFIE_VERSION == 'Z' // single-font        
          if (SaberBase::IsOn()) menuInterface<T>::workingProp->SetPresetFast(currentValue, false);
          else menuInterface<T>::workingProp->SetPreset(currentValue, false);
        #else // multi-font
          if (SaberBase::IsOn()) menuInterface<T>::workingProp->SetPresetFast(currentValue);
          else menuInterface<T>::workingProp->SetPreset(currentValue, true);        
        #endif
        if(restoreTrack)
        {
           track_player_ = GetFreeWavPlayer();
          if (track_player_) 
          track_player_->Play(menuInterface<T>::workingProp->current_preset_->track);
        }
      }
    }
    // behavoir of OK 
    void Ok(uint16_t currentValue) override 
    {
      // menuInterface<T>::RestoreVolume();    // restore volume and brightness 
      // menuInterface<T>::RestoreBright(); 
      menuInterface<T>::RestoreProfile();
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
      menuInterface<T>::RestoreProfile();
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0 , 1); 
      menuInterface<T>::canBeDestroyed = true;  // mark that menu can be destroy 

    }
};

// // clas handling the behavoir of profile menu 
// // It give behavoir of the 4 action of menu action Tick , StableState , Ok , Cancel
// // Xmenu will call the action
template<class T> 
class menuProfile_t : public menuInterface<T> {
private:
    uint16_t savedSelection;

public: 
    menuProfile_t(T* prop): menuInterface<T>(prop) {
      savedSelection = 0;
      // menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, annunciator, 0 , 1);
      menuInterface<T>::InitMenuProfile();

      if (installConfig.monochrome)
        menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, longVolume, 0 , 1);
      else
        menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, longColor, 0 , 1);
      // menuInterface<T>::InitMenuVolume();
      // menuInterface<T>::InitMenuBright();

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
          case xUltraMenu_Color: menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, menuVoices_id::color, 0, 1); break;
          case xUltraMenu_Sensitivity: menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, menuVoices_id::sensitivity, 0, 1); break;
          case xUltraMenu_Bright : menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, menuVoices_id::brightness, 0, 1); break;
          case xUltraMenu_Volume: menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, menuVoices_id::volume, 0, 1); break;
      }
      savedSelection = currentValue;
    }
    // behavoir of OK 
    void Ok(uint16_t currentValue) override 
    {
      menuInterface<T>::lastTimeActive = millis();   // not really neccesarry just to be sure a false positive timeout cancel is ocured 

      if(!savedSelection) savedSelection = currentValue; // move forward with default 
      // STDOUT.print("Current value = "); STDOUT.println(currentValue);
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
      menuInterface<T>::canBeDestroyed = true;
      menuInterface<T>::RestoreProfile();  
  


      // if(!savedSelection)
      // {
      //   menuInterface<T>::workingProp->PlaySpecialSound(true, &menuSounds, annunciator, 0 , 1);
      //   // savedSelection = 2; // choose color by default 
      //   // menuInterface<T>::workingProp->FadeAndStopSS(); 
      // } else {
      //   menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
      //   menuInterface<T>::canBeDestroyed = true;
      //   // menuInterface<T>::RestoreVolume();
      //   // menuInterface<T>::RestoreBright();
      // }

    }
    // behavoir of cancel 
    void Cancel(uint16_t currentValue) override 
    { 
      // menuInterface<T>::RestoreVolume();
      // menuInterface<T>::RestoreBright();
      menuInterface<T>::RestoreProfile();  
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0, 1);
      menuInterface<T>::canBeDestroyed = true;
      savedSelection = 0;

    }
};


template<class T> 
class menuProfileColor_t : public menuInterface<T> {
  public: 
    menuProfileColor_t(T* prop, uint16_t nrSeg): menuInterface<T>(prop) 
    { 
      initialColor = menuInterface<T>::workingProp->current_preset_->variation;               // save inital color 
      // menuInterface<T>::InitMenuVolume();
      // menuInterface<T>::InitMenuBright();
      menuInterface<T>::InitMenuProfile();

    }
    uint16_t ChangeActions() override {return xUltraMenu_Profile;}  // restore state before menu profile 
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
      SaberBase::SetVariation(currentValue << 7);
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
      SaberBase::SetVariation(menuInterface<T>::workingProp->current_preset_->variation);
      if (!WriteUserProfile(PROFILE_FILE, 1)) STDOUT.println("Failed saving profile!");
      menuInterface<T>::RestoreProfile();


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
      SaberBase::SetVariation(initialColor);
      menuInterface<T>::RestoreProfile(); 
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0 , 1);
      menuInterface<T>::canBeDestroyed = true;

    }
};

template<class T> 
class menuProfileBrightness_t : public menuInterface<T> {
  public: 
    // menuProfileBrightness_t(T* prop, uint16_t nrSeg): menuInterface<T>(prop) {
    //   initialBrighntness = userProfile.masterBrightness;
    //   seg = Segments(MIN_MASTER_BRIGHTNESS, 65535, nrSeg);
    //   // menuInterface<T>::InitMenuVolume();
    //   // menuInterface<T>::InitMenuBright(); 
    //   menuInterface<T>::InitMenuProfile();

    // }
    menuProfileBrightness_t(T* prop, uint16_t nrSeg): menuInterface<T>(prop) {
      // initialBrighntness = userProfile.masterBrightness;
      if (prop->stealthMode) {
          seg = Segments(MIN_MASTER_BRIGHTNESS, STEALTH_MAX_BRIGHTNESS, nrSeg);    // 0 -> stealth MAX if in stealth mode
          STDOUT.print("Segment limits: "); STDOUT.print(MIN_MASTER_BRIGHTNESS); STDOUT.print(" -> "); STDOUT.println(STEALTH_MAX_BRIGHTNESS); 
      }
      else {
          seg = Segments(userProfile.stealthBrightness, 65535, nrSeg);    // stealth -> full if not in stealth mode
          STDOUT.print("Segment limits: "); STDOUT.print(userProfile.stealthBrightness); STDOUT.print(" -> "); STDOUT.println(65535); 
      }
      // menuInterface<T>::InitMenuVolume();
      // menuInterface<T>::InitMenuBright(); 
      menuInterface<T>::InitMenuProfile();

    }    
    uint16_t ChangeActions() override {return xUltraMenu_Profile;} // restore state before menu profile 
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

      menuInterface<T>::RestoreProfile();
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
      menuInterface<T>::RestoreProfile(true);   // restore to initial, not current values
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0 , 1);
      menuInterface<T>::canBeDestroyed = true;
      // menuInterface<T>::RestoreVolume();
      // menuInterface<T>::RestoreBright();
    }
};


template<class T> 
class menuProfileVolume_t : public menuInterface<T> {
  public: 
    menuProfileVolume_t(T* prop, uint16_t nrSeg): menuInterface<T>(prop) {
      // initialVolume = userProfile.masterVolume;
      // seg = Segments(MIN_MASTER_VOLUME, 65535, nrSeg);
      // menuInterface<T>::InitMenuVolume();
      // menuInterface<T>::InitMenuBright(); 
      if (prop->stealthMode) {
          seg = Segments(MIN_MASTER_VOLUME, STEALTH_MAX_VOLUME, nrSeg);    // 0 -> stealth MAX if in stealth mode
          STDOUT.print("Segment limits: "); STDOUT.print(MIN_MASTER_VOLUME); STDOUT.print(" -> "); STDOUT.println(STEALTH_MAX_VOLUME); 
      }
      else {
          seg = Segments(userProfile.stealthVolume, 65535, nrSeg);    // stealth -> full if not in stealth mode
          STDOUT.print("Segment limits: "); STDOUT.print(userProfile.stealthVolume); STDOUT.print(" -> "); STDOUT.println(65535); 
      }
      menuInterface<T>::InitMenuProfile();

    }
    uint16_t ChangeActions() override {return xUltraMenu_Profile;} // restore state before menu profile 
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
        #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
        SilentEnableAmplifier(false);      
        SilentEnableBooster(false);
        #endif
      }
      else {
        #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
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
      STDOUT.print("[menuProfileVolume.Ok] currentValue = "); STDOUT.print(currentValue); STDOUT.print(", saved volume = "); STDOUT.println(userProfile.masterVolume);      
      if (menuInterface<T>::workingProp->stealthMode) {  // save current as stealth
          userProfile.stealthVolume = userProfile.masterVolume;
      } else {
         userProfile.combatVolume = userProfile.masterVolume;
      }


      menuInterface<T>::RestoreProfile();   // also fix stealth mode
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
      menuInterface<T>::RestoreProfile(true); // restore to initial values
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0, 1);
      menuInterface<T>::canBeDestroyed = true;
      // menuInterface<T>::RestoreVolume();
      // menuInterface<T>::RestoreBright();
    }
};


template<class T> 
class menuProfileSensitivity_t : public menuInterface<T> {
  public: 
    menuProfileSensitivity_t(T* prop): menuInterface<T>(prop) 
    { 
      initialSens = Sensitivity::master;               // save inital sensitivity 
      menuInterface<T>::InitMenuProfile();     

    }
    uint16_t ChangeActions() override {return xUltraMenu_Profile;}  // restore state before menu profile 
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
      #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
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
      menuInterface<T>::RestoreProfile(true);

      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, ack, 0, 1);
      menuInterface<T>::canBeDestroyed = true;

    }
    // behavoir of cancel 
    void Cancel(uint16_t currentValue) override 
    { 
      ApplySensitivities(initialSens);
      menuInterface<T>::RestoreProfile(true); 
      menuInterface<T>::workingProp->PlaySpecialSound(true, &emojiSounds, cancel, 0 , 1);
      menuInterface<T>::canBeDestroyed = true;

    }
};


#endif // ULTRAMENU_H