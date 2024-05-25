#ifndef PROPS_SABERPROP_H
#define PROPS_SABERPROP_H


#include "prop_base.h"

#ifdef OSX_ENABLE_MTP
  #include "../common/serial.h"
#endif 
#define PROP_TYPE SaberProp

#include "saberprop_menu.h"
#include "TTmenu.h"


// #define AUTOOFF_TIMEOUT 121000
#define AUTOOFF_TIMEOUT installConfig.APOtime
#define SILENT_STEALTH 0
#define ANNOUNCE_STEALTH 1
#define ANNOUNCE_LOWBAT 2
#define ANNOUNCE_TIMEOUT 3



// The Saber class implements the basic states and actions
// for the saber.
class SaberProp : public PROP_INHERIT_PREFIX PropBase 
{
private:
    RefPtr<BufferedWavPlayer> player;  // tra..la...la
    #ifdef ENABLE_LOGG_WAVE_OUTPUT
    bool offTriggered;
    #endif
public:
    // play special sounds for menu interaction 
    bool PlaySpecialSound(bool playedInMenu, Effect * soundEffect, int soundID = -1, uint16_t repeat = 0, float volume = 0)
    {   
        bool resultPlay;
        // STDOUT.print("Prin SS player state"); STDOUT.println(player ? "something": "null");
        if (!player) player = GetFreeWavPlayer();
        if (!player)  {
          STDOUT.println("SaberProp prop cannot get free player!");
          return false;
        }
        resultPlay =  player->PlayEffect(soundEffect, soundID, repeat, volume);
        // if(autofree) player.Free();
        // if(playedInMenu && userProfile.masterVolume < 5000)
        // {

        //   userProfile.masterVolume = 5000;
        //   dynamic_mixer.set_volume(VOLUME); // we need some volume to play volume tags 
        // }

        return resultPlay;
    }
    // Fade and stop a special sound played in loop 
    bool FadeAndStopSS(float fadeTime = 0.5)
    {
        if (!player) return false;   
        // player->set_fade_time(fadeTime);
        player->FadeAndStop();  // maybe just STOP iT an player->CLOSEFILES(); TEST 
        // player.Free();      // free player 
        return true;
    }

  SaberProp() : PropBase() 
  {    
    #ifdef ENABLE_LOGG_WAVE_OUTPUT
    offTriggered = false;
    #endif
    lastRecordedEvent = millis();
    myMenuActions = 0;
    shortcutPreset_actions.reset();
    MenuConfig_actions.reset();
    propMode = NO_MODE;
    lockupTime = 0;
  }
  const char* name() override { return "SaberProp"; }
  
  void Setup() 
  {
      // stealthOLDBrightness = userProfile.masterBrightness;    
      // stealthOLDVolume = userProfile.masterVolume;
      lbatOLDVolume = userProfile.masterVolume;
      emojiSounds.SetPersistence(true);       // those are persistent effect, so we need to scan manually
      emojiBackgrounds.SetPersistence(true); 
      menuSounds.SetPersistence(true); 
      Effect::ScanDirectory(XSOUNDS_PATH);
      
  }

  bool Event2(enum BUTTON button, EVENT event, uint32_t modifiers) override //__attribute__((optimize("Og"))) 
  {
    #ifdef OSX_ENABLE_MTP
    if(!mtpUart->GetSession()) {
    #endif
    // something is going on , needs power
    lastRecordedEvent = millis(); // something is happening , not necessary a fullfill condition 
    // if (event==EVENT_TAP) { // || button==BUTTON_POWER) {
      // STDOUT.print("[prop.Event2] button="); STDOUT.print(button); STDOUT.print(", event="); STDOUT.print(event); 
      // STDOUT.print(", modifiers="); STDOUT.print(modifiers); STDOUT.print(", current_modifiers="); STDOUT.println(current_modifiers);
    // }
    
    // Keep button state properly
    // TODO: scrap modifiers and duplicated event calls.
    if (button == BUTTON_POWER && event == EVENT_PRESSED) {
          buttonState = true;
          // STDOUT.println("Button pressed");
    }
    if (button == BUTTON_POWER && event == EVENT_RELEASED) {
          buttonState = false;
          // STDOUT.println("Button released");
    }

    switch (EVENTID(button, event, modifiers)) 
    { 
            
      // ------------------- PROP CONTROLS --------------------------------
      // Short click pressed while off , POWER ON 
      case EVENTID(BUTTON_POWER, EVENT_CLICK_SHORT, MODE_OFF):
          #ifdef DIAGNOSE_PROP
            STDOUT.print("SHORT CLICK detected! ");
          #endif
          if(!CheckCanStart()) {
              #ifdef DIAGNOSE_PROP
                STDOUT.println("Battery too low to turn ON.");
              #endif         
              return true;
          }
          // resetDefaults();          // reset variable to defaults
          #ifdef DIAGNOSE_PROP
            STDOUT.println("Turning ON.");
          #endif

          #ifdef ENABLE_LOGG_WAVE_OUTPUT
          logger.startLog();
          #endif         
          On();                     // Do ON, including reset and ignore clash
      return true;

      // Short click pressed while on , POWER OFF  
      case EVENTID(BUTTON_POWER, EVENT_CLICK_SHORT, MODE_ON):
          #ifdef DIAGNOSE_PROP
            STDOUT.println("SHORT CLICK detected! Turning OFF.");
          #endif           
          if( propMode== BLAST_MODE) {
              propMode = NO_MODE;            
              smooth_swing_v2.Pause(false);   // resume smoothswing
          }
          Off();                                              // do OFF 
          resetDefaults();
          #ifdef ENABLE_LOGG_WAVE_OUTPUT
          offTriggered=true;
          #endif
      return true;

      // BTN held long , either in OFF or ON , play track
      case EVENTID(BUTTON_POWER, EVENT_HELD_LONG, MODE_OFF):
          if(!CheckCanStart() && !track_player_) {  
            #ifdef DIAGNOSE_PROP
              STDOUT.println("LONG CLICK detected! Battery too low to turn on track.");
            #endif 
            return true;
          }
      case EVENTID(BUTTON_POWER, EVENT_HELD_LONG, MODE_ON):
          #ifdef DIAGNOSE_PROP
            STDOUT.print("LONG CLICK detected! Turn track ");
            if (track_player_)  STDOUT.println(" OFF");
            else STDOUT.println(" ON");
          #endif 
          if (!track_player_ && !current_preset_->track_index) 
            PlaySpecialSound(true, &menuSounds, notrack, 0, 1);   // trying to start non-existing track
          else 
            StartOrStopTrack();   // toggle track          
      return true;
      
      // CONFIG MENU -  Twist while btn pressed , either in OFF or ON , prepare menu enter  
      case EVENTID(BUTTON_NONE, EVENT_TWIST , BUTTON_POWER | MODE_OFF): 
          if(!CheckCanStart()) return true; 
          if(!MenuConfig_actions.enter) 
            MenuConfig_actions.triggerState = trgOff; // mark triger state only if we are not executiong other actions
      case EVENTID(BUTTON_NONE, EVENT_TWIST , BUTTON_POWER | MODE_ON):
          if(!menu && !myMenuActions && !propMode) 
          { 
            #ifdef DIAGNOSE_PROP
              STDOUT.println("Starting PROFILE menu.");
            #endif 
            if(!MenuConfig_actions.enter)
            { 
              MenuConfig_actions.enter = true;               // set enter preactions menus, PresetMenuCreator() will take cre of the rest
              if (track_player_) { // stop track if playing
                StartOrStopTrack(0); // stop track if playing
                MenuConfig_actions.resumeTrack = true; // start track after menu ends
                // STDOUT.println("[menudebug]: triggered Config menu while track is on");
              }
              if(MenuConfig_actions.triggerState == trgDef)  // only trigger state has default value ,
              { 
                MenuConfig_actions.triggerState = trgON;     //  mark trigger state as on                
                Off();
                hybrid_font.check_postoff_ = false; // disable postoff until the next Off()
              }
            }
          }
      return true;


      // COLOR shortcut - Scroll with button pressed
      case EVENTID(BUTTON_NONE, EVENT_SCROLL, BUTTON_POWER | MODE_ON):
          #ifdef DIAGNOSE_PROP
            STDOUT.print("PROP detected scrolling at ");  STDOUT.println(scroll_to_inject);
          #endif
          if (!CreateMenu(MnuConfig_Color, true, 0, 256, (uint16_t)(current_preset_->variation >> 7))) {
            #ifdef DIAGNOSE_PROP
              STDOUT.println("Failed to create color shortcut.");
            #endif
          }
          else {
            if (scroll_to_inject>0) {
                fusor.SetTheta(1);
                fusor.SetThetaPeak(0);  // might still be active from triggering scroll
                menu->StartScroll(scroll_to_inject);;              
            }
            else {
              fusor.SetTheta(-1);
              fusor.SetThetaPeak(0);  // might still be active from triggering scroll
              menu->StartScroll(-scroll_to_inject);
            }
            // menu->StartScroll(MENU_MAXSCROLL + MENU_STARTSCROLL);

          }
      return true;


       // PRESET shortcut - Second short click, either in OFF or ON , prepare menu enter 
      case EVENTID(BUTTON_POWER,  EVENT_SECOND_CLICK_SHORT, MODE_OFF):
          if(!CheckCanStart()) return true;
            if(presets.size() <= 1) {
              PlaySpecialSound(true, &emojiSounds, cancel, 0, 1);
              return true;
            } 
          if(!shortcutPreset_actions.enter) 
              shortcutPreset_actions.triggerState = trgOff; // mark triger state only if we are not executiong other actions
      case EVENTID(BUTTON_POWER,  EVENT_SECOND_CLICK_SHORT, MODE_ON):
          hybrid_font.check_postoff_ = false; // disable postoff until the next Off()
          if(!menu && !myMenuActions) {
            #ifdef DIAGNOSE_PROP
              STDOUT.println("Starting PRESET menu.");
            #endif           
            if(presets.size() <= 1) {
              PlaySpecialSound(true, &emojiSounds, cancel, 0, 1);
              return true;
            }
            if(!propMode && (millis() - menuDestroyedTimestamp > 1500) ) 
            {
              if(!shortcutPreset_actions.enter) 
              {
                shortcutPreset_actions.enter = true;
                if (!IsOn()) { 
                  shortcutPreset_actions.waitActions = true;    // wait to turn back on if menu was triggered while ON (saber went off before the second click)
                  shortcutPreset_actions.off = false;
                }
                else { 
                  shortcutPreset_actions.waitActions = false;
                  shortcutPreset_actions.off = true;        // turn back off when menu ends
                }
                shortcutPreset_actions.triggerState = trgOff; 
              }
            }
          }
      return true;


      // STEALTH / FULL POWER: tap with button pressed
      case EVENTID(BUTTON_NONE, EVENT_TAP,  MODE_OFF | BUTTON_POWER):   // toggle STEALTH while off
          if(!CheckCanStart()) return true;           
      case EVENTID(BUTTON_NONE, EVENT_TAP, MODE_ON | BUTTON_POWER): // toggle STEALTH while on
          if (!buttonState) return true;
          #ifdef DIAGNOSE_PROP
            STDOUT.print("TAP detected, set stealth mode to ");
            STDOUT.println(!stealthMode);
          #endif //  DIAGNOSE_PROP          
          SetStealth(!stealthMode);       
      return true;
      
      // ------------------- end prop controls --------------------------------


      // ---------------------- PROP EFFECTS --------------------------
      // TWIST - latch for lockup
      case EVENTID(BUTTON_NONE, EVENT_TWIST, MODE_ON):  
          if(!SaberBase::Lockup() && !propMode) { // latch for lockup if nothing else is happening
              #ifdef DIAGNOSE_PROP
                STDOUT.println("TWIST detected! LATCHED for LOCKUP.");
              #endif 
              // smooth_swing_v2.Pause(true);
              PlaySpecialSound(false, &emojiSounds, latchedON, 0, 1);            
              propMode = LATCH_MODE;
          }
          else if(propMode == LATCH_MODE) { // end pre-lockup latch if active
            #ifdef DIAGNOSE_PROP
              STDOUT.println("TWIST detected while latched for lockup! Turning latch OFF.");
            #endif           
            SaberBase::SetLockup(SaberBase::LOCKUP_NORMAL);   // temporary set lockup to normal, for the end lockup effect
            SaberBase::DoEndLockup();
            SaberBase::SetLockup(SaberBase::LOCKUP_NONE);
            propMode = NO_MODE;
          } 
      return true;

      // SHAKE - toggle blast mode
      case EVENTID(BUTTON_NONE, EVENT_SHAKE, MODE_ON):   
          if (propMode == BLAST_MODE) {   // end blast mode
            propMode = NO_MODE;            
            PlaySpecialSound(false, &emojiSounds, blastOFF, 0, 1); 
            smooth_swing_v2.Pause(false);   // resume smoothswing
            #ifdef DIAGNOSE_PROP
              STDOUT.println("SHAKE detected! Blast mode OFF.");
            #endif     
          }
          else if(!SaberBase::Lockup() && !propMode) {   // enter blast mode if nothing else is happening
            propMode= BLAST_MODE;
            PlaySpecialSound(false, &emojiSounds, blastON, 0, 1);
            smooth_swing_v2.Pause(true);  // pause smoothswing in blast mode
            #ifdef DIAGNOSE_PROP
              STDOUT.println("SHAKE detected! Will enter Blast mode.");
            #endif     

          }        
      return true;
      
      // Blast deflect 
      case EVENTID(BUTTON_NONE, EVENT_DEFLECT, MODE_ON):  // EVENT_THIRD_SAVED_CLICK_SHORT
          if (propMode == BLAST_MODE) {
            #ifdef DIAGNOSE_PROP
              STDOUT.println("BLAST DEFLECT detected!");
            #endif //  DIAGNOSE_PROP
            SaberBase::DoBlast();
          }
      return true;


      // CLASH trigger lockup or drag
      #define CLASH_VERTANGLE -0.7f    // vertical angle threshold to consider a clash as pointing down
      case EVENTID(BUTTON_NONE, EVENT_CLASH, MODE_ON):
          if (!SaberBase::Lockup()) { // nothing to do if already in lockup
            if(propMode == LATCH_MODE) {   
              #ifdef DIAGNOSE_PROP
                STDOUT.println("CLASH detected while latched! Turning Lockup ON.");
              #endif             
              SaberBase::SetLockup(SaberBase::LOCKUP_NORMAL);
              SaberBase::DoBeginLockup();
              IgnoreClash(300); 
              propMode = NO_MODE;
              lockupTime = millis();  // save time lockup was triggered, this clash might also contain a swing and we don't want to immediately end lockup
              return true;
            } 
            else if (propMode == NO_MODE && fusor.angle1() <= CLASH_VERTANGLE) {    // poiting down , do drag 
                SaberBase::SetLockup(SaberBase::LOCKUP_DRAG);
                SaberBase::DoBeginLockup();
                IgnoreClash(300); 
                lockupTime = millis();
                #ifdef DIAGNOSE_PROP
                  STDOUT.println("CLASH detected pointing down! Triggering Drag.");
                #endif              
                return true;              
            }
            else if (propMode == BLAST_MODE) return true;  // don't do clashes in blast mode
          }      
      break; // we need to return false so the clash event gets caught

      // SWING ends lockups 
      #define LOCKUP_MINTIME 300      // minimum time [ms] after a lockup when a swing can end it
      case EVENTID(BUTTON_NONE, EVENT_SWING, MODE_ON):
          if (!propMode && SaberBase::Lockup() && millis()-lockupTime > LOCKUP_MINTIME) { // end all lockups 
            SaberBase::DoEndLockup();   
            SaberBase::SetLockup(SaberBase::LOCKUP_NONE);
            #ifdef DIAGNOSE_PROP
              STDOUT.println("SWING detected in lockup/lightning/drag/melt, turning OFF.");
            #endif              
            IgnoreClash(300); // 
            lockupTime = 0; // reset lockup time
          }
          else if (propMode == BLAST_MODE) {
            #ifdef DIAGNOSE_PROP
              STDOUT.println("BLAST DEFLECT detected!");
            #endif //  DIAGNOSE_PROP
            SaberBase::DoBlast();
          }
      return true;
      // break; // we need to return false so the swing event gets caught
      
      //  STAB (no button)
      #define STAB_VERTANGLE 0.95f    // vertical angle threshold to consider a stab as pointing up
      case EVENTID(BUTTON_NONE, EVENT_STAB, MODE_ON):
          if (!SaberBase::Lockup() && !propMode) {  // no stab in lockup or blast mode
            if(fusor.angle1() >= STAB_VERTANGLE) { // poiting up do lightning 
              SaberBase::SetLockup(SaberBase::LOCKUP_LIGHTNING_BLOCK);            
              SaberBase::DoBeginLockup();
              lockupTime = millis();
              #ifdef DIAGNOSE_PROP
                STDOUT.println("STAB detected pointing up! Triggering Lightning.");
              #endif              
            }
            else {
              SaberBase::DoStab();
              #ifdef DIAGNOSE_PROP
                STDOUT.println("STAB detected!");
              #endif //  DIAGNOSE_PROP
            }
          }          
      return true;

      // MELT - screw gesture (stab & twist)
      case EVENTID(BUTTON_NONE, EVENT_SCREW, MODE_ON):
          if (!SaberBase::Lockup() && !propMode)  { // no melt in lockup or blast mode
              SaberBase::DoStab();
              SaberBase::SetLockup(SaberBase::LOCKUP_MELT);
              SaberBase::DoBeginLockup();
              lockupTime = millis();
              #ifdef DIAGNOSE_PROP
                STDOUT.println("SCREW detected! Triggering MELT.");
              #endif //  DIAGNOSE_PROP
          }
      return true;
      

      // ---------------------- end prop effects ---------------------------
    } // end big switch
    
    #ifdef OSX_ENABLE_MTP
    }
    #endif
    return false;
  }


  void Loop() override {
    // bool playerDestroyer;
    #ifdef OSX_ENABLE_MTP
      if(mtpUart->GetSession()) return;    // if media transfer file is active , no prop is running  
    #endif
    
        PropBase::Loop();   // run the prop loop     
        #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
          if (menu) RequestPower();
        #endif

          SaberBase::RequestMotion(); // request motion 
		      DetectTaps(1);               // run single-tap detection 
          DetectTwist();              // run twist detection  
          DetectSwing();              // run swing detection 
          DetectStabs();               // run stab and screw detection
          DetectDeflect();            // run deflect detection
		      DetectShake();              // run shake detection 
          DetectScroll();
          if (SaberBase::IsOn()) {
            // Auto low power:
            if (menu) lastRecordedEvent = millis();   // don't go into low power if menu is active
            if (!autoLowPower) {
              if (millis() - lastRecordedEvent > 3* (AUTOOFF_TIMEOUT/4) && !stealthMode) {
                  // enter auto low power after 75% of auto off time: force stealth mode if it was off
                  autoLowPower = true;
                  SetStealth(true, ANNOUNCE_TIMEOUT);
                  #ifdef DIAGNOSE_PROP
                    STDOUT.print(millis()); STDOUT.println(": ENTER auto low power");
                  #endif  
              }
            }
            else {
              if (millis() - lastRecordedEvent < 100) {
                  // exit auto low power, only if we previously forced stealth on
                  autoLowPower = false;
                  SetStealth(false);
                  #ifdef DIAGNOSE_PROP
                    STDOUT.print(millis()); STDOUT.println(": EXIT auto low power");
                  #endif
              }
            } 
            // Auto power off:
            if(SaberBase::IsOn() && (millis() - lastRecordedEvent > AUTOOFF_TIMEOUT)) {
              Off();
              if (autoLowPower) SetStealth(false, SILENT_STEALTH); // It went in stealth mode at auto power off, so we restore stealth=OFF for next start.
              autoLowPower = false;
              #ifdef DIAGNOSE_PROP
                STDOUT.print(millis()); STDOUT.println(": AUTO POWER OFF");
              #endif
              lastRecordedEvent = millis();
            }            
          }
          else {
              if (IgnitionGesture()) { // run ignition gesture detection, only when off
                #ifdef DIAGNOSE_PROP
                  STDOUT.println("Rise detected, turning ON");
                #endif
                if (CheckCanStart()) { // check if we can start
                  FastOn(); // turn on
                  lastRecordedEvent = millis();
                }
              }
          }
 
        if (PlayerDestroyer()) {    // just ended a sound and freed the prop player
            if(restoreSettingsLOWBAT) {  // restore volume after a failed attempt to power on; LowBat sound just ended.
              restoreSettingsLOWBAT = false;    // set by CheckCanStart
              #ifdef DIAGNOSE_PROP
                STDOUT.println("Player destroyed");
              #endif
              if (stealthMode) userProfile.masterVolume = userProfile.stealthVolume;  // restore volume according to stealth mode
              else userProfile.masterVolume = userProfile.combatVolume;
              dynamic_mixer.set_volume(VOLUME);
              #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
              SilentEnableAmplifier(false);      
              SilentEnableBooster(false);
              #endif
            }
        }
        MenuLoop();       // create/destroy menu actions
        StealthLoop();   // sweep volume and brightness if needed        
#ifdef ENABLE_LOGG_WAVE_OUTPUT
       if(offTriggered && !dac_OS.isOn()) {
        offTriggered = false;
        logger.stopLog(true);
       }

       if(logger.isSaveDone()) {
        logger.setSaveDone(false);
        PlaySpecialSound(false, &emojiSounds, okdone, 0, 1);
       }
#endif

  }
  /*  @brief  :
  *   @param  :
  *   @retval :
  */
  bool CheckCanStart() 
  { static uint32_t lastDenial=0;
    // float battLevel = battery_monitor.battery();
    // if (battLevel != 0) 
    // {
      // if(battLevel > TURN_ON_THRESH) return true;   // all good

    if (!battery_monitor.IsLow()) return true;      // check instant battery level
    #ifdef DIAGNOSE_PROP
      STDOUT.println("Battery too low to start");
    #endif
    if (millis() - lastDenial < 3000) return false;   // battery low but we recently announced, won't do it again
    // battery low and need to announce:
    lastDenial = millis();
    userProfile.masterVolume = 8000;    // set low volume to announce low battery
    dynamic_mixer.set_volume(VOLUME);
    restoreSettingsLOWBAT = true;
    #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
    SilentEnableAmplifier(true);      
    SilentEnableBooster(true);
    #endif
    PlaySpecialSound(true, &emojiSounds, lowbat, 0, 1);
    return false;

    // }
    // return false; 
  }
  /*  @brief  :
  *   @param  :
  *   @retval :
  */
  void CheckLowBattery() override 
  {
    // float battLevel = battery_monitor.battery();
    // if (battLevel != 0 && battLevel < TURN_OFF_THRESH) {  
    if (battery_monitor.low())    // check number of "low" flags
      if (SaberBase::IsOn()) {
          Off();
          STDOUT.println("Battery low, turning off.");
          PlaySpecialSound(true, &emojiSounds, lowbat, 0, 1);
      }
  }


// Apply and announce volume and brightness for the new stealth mode
uint16_t targetVol, targetBr;   // target volume and brightness, for sweep signalling of stealth modes
void SetStealth(bool newStealthMode, uint8_t announce = ANNOUNCE_STEALTH) override {
    if (announce) {   // announce: play special sound and sweep volume and brightness to new values
        #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
        SilentEnableAmplifier(true);  // ... just in case it was previously muted...
        SilentEnableBooster(true);
        #endif    
        float vol = 0.5;
        // // End lockup or drag, might have been triggered by the first tap (depends on sensitivities)
        // if (IsOn()) { 
        //   vol = 2;
        //   if (!propMode && SaberBase::Lockup()) { 
        //       SaberBase::SetLockup(SaberBase::LOCKUP_NONE);   // no end sound
        //       SaberBase::DoEndLockup();
        //   }
        // }
        // Announce new stealth mode
        if (newStealthMode) { // turn on STEALTH 
          switch(announce) {
            case ANNOUNCE_LOWBAT: PlaySpecialSound(false, &emojiSounds, lowbat, 0, vol); break;
            case ANNOUNCE_TIMEOUT: PlaySpecialSound(false, &emojiSounds, cancel, 0, vol); break;
            default: PlaySpecialSound(false, &emojiSounds, stealthON, 0, vol); break;
          }

          // if (announce==ANNOUNCE_STEALTH) PlaySpecialSound(false, &emojiSounds, stealthON, 0, vol);
          // else PlaySpecialSound(false, &emojiSounds, lowbat, 0, vol);
          // STDOUT.println("Stealth ON");
        }
        else {  // resume regular mode
          PlaySpecialSound(false, &emojiSounds, stealthOFF, 0, vol);
          // STDOUT.println("Stealth OFF");
        }
        // Set targets for sweep
        if (newStealthMode) {     
          targetVol = userProfile.stealthVolume;
          targetBr = userProfile.stealthBrightness;
        }
        else {
          targetVol = userProfile.combatVolume;
          targetBr = userProfile.combatBrightness;
        }
      StealthLoop(true); // start stealth sweeps

    }
    else {    // don't announce: just apply volume and brightness, immediately
      if (newStealthMode) { 
        userProfile.masterVolume = userProfile.stealthVolume;
        userProfile.masterBrightness = userProfile.stealthBrightness;
      }
      else {
        userProfile.masterVolume = userProfile.combatVolume;
        userProfile.masterBrightness = userProfile.combatBrightness;
      }
      // STDOUT.print("[SetStealth] Volume = "); STDOUT.print(userProfile.masterVolume); STDOUT.print(", brightness ="); STDOUT.println(userProfile.masterBrightness);          
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
      dynamic_mixer.set_volume(VOLUME);
      
    }
  stealthMode = newStealthMode;   // flag new mode


}

#define STEALTH_VOLSTEP 500   // about 1.3 seconds to sweep the entire volume range
#define STEALTH_BRSTEP 500   // about 1.3 seconds to sweep the entire brightness range
// Called by loop() to handle timed events for stealth signalling.
// targetStealth specifies whether to sweep towards stealth ON or OFF. startNow=true to trigger sweeping
void StealthLoop(bool startNow = false) {
    static uint32_t lastTimeServiced = 0;
    // 1. Check run conditions
    uint32_t timeNow = millis();
    if (startNow) lastTimeServiced = timeNow-11;    // enable sweep and run now. Hopefully not the first 10 [ms] since board started...
    if (!lastTimeServiced) return;                  // nothing to do, sweep is off
    if (timeNow - lastTimeServiced < 10) return;    // run once at 10 [ms]
    lastTimeServiced = timeNow;
        
    
    // 3. Sweep brightness
    int32_t tmp;    // allow wider range for preliminary calculation
    if (userProfile.masterBrightness > targetBr) {  // sweep down
      tmp = userProfile.masterBrightness - STEALTH_BRSTEP;
      if (tmp <= targetBr) userProfile.masterBrightness = targetBr;   // reached target brightness
      else userProfile.masterBrightness = tmp;          
    }
    else if (userProfile.masterBrightness < targetBr) { // sweep up
      tmp = userProfile.masterBrightness + STEALTH_BRSTEP;
      if (tmp >= targetBr) userProfile.masterBrightness = targetBr;   // reached target brightness
      else userProfile.masterBrightness = tmp;          
    } // no sweep if nothing must change

    // 4. Sweep volume
    if (userProfile.masterVolume > targetVol) {  // sweep down
      tmp = userProfile.masterVolume - STEALTH_VOLSTEP;
      if (tmp <= targetVol) userProfile.masterVolume = targetVol;   // reached target brightness
      else userProfile.masterVolume = tmp;          
    }
    else if (userProfile.masterVolume < targetVol) { // sweep up
      tmp = userProfile.masterVolume + STEALTH_VOLSTEP;
      if (tmp >= targetVol) userProfile.masterVolume = targetVol;   // reached target brightness
      else userProfile.masterVolume = tmp;          
    } // no sweep if nothing must change
    dynamic_mixer.set_volume(VOLUME);

    // 5. Check stop conditions
    if (userProfile.masterBrightness == targetBr && userProfile.masterVolume == targetVol) {
      lastTimeServiced = 0;   // stop sweeping if reached all targets
      #ifdef DIAGNOSE_PROP
        STDOUT.print("Sweeping volume to "); STDOUT.print(targetVol); 
        STDOUT.print(" and brightness to "); STDOUT.println(targetBr); 
      #endif
	    #if defined(SABERPROP) && defined(ARDUINO_ARCH_STM32L4) // STM Saberprop and UltraProffies
        if (!userProfile.masterVolume) { // mute
          SilentEnableAmplifier(false);         
          SilentEnableBooster(false);  
        }
      #endif 
      if (!IsOn()) {  // saber is off so we need to stop audio
        if (player) {
          player->FadeAndStop();         // stop player 
          player.Free();                // free wavplayer 
          if (hybrid_font.getHum()) hybrid_font.getHum()->SetFollowing(hybrid_font.getHum()); // restore looping for hum
        }
      }
    }
}



enum {
  NO_MODE = 0,
  BLAST_MODE,   // clashes = blast deflects
  LATCH_MODE,   // next clash triggers lockup 
} propMode;
uint32_t lockupTime;  // time when lockup was triggered

enum trgState
{
  trgDef = 0,   // trigger default 
  trgON,        // trigger from ON 
  trgOff,       // trigger from Off 

};

class MenuPreActions
{
  public: 
  bool enter;
  bool off;     // turn off when menu is destroyed
  bool resumeTrack;  // start track when exiting menu
  bool waitActions;
  int8_t triggerState;
  MenuPreActions(){ enter = false; waitActions = false; triggerState = trgDef;}
  void reset() { 
    off = false;
    enter = false; 
    waitActions = false; 
    triggerState = trgDef; 
    resumeTrack = false;
    } 
};
  /*  @brief  : Handels preset menu cration (we have to wait until on.off are over )              
  *   @param  : void 
  *   @retval : void 
  */
void PresetMenuCreator(MenuPreActions *preActions)
{

     if(!menu && !preActions->enter && preActions->off) {
        preActions->reset(); 
        Off(SILENT_OFF); 
     }

    // PRESET MENU HADLER CREATOR
    if(!menu && preActions->enter)
    {
      if(preActions->waitActions)   // we must wait specific action to complete before creating menu 
      { 
        if(( preActions->triggerState == trgOff)  
              &&  (hybrid_font.GetState() == HybridFont::STATE_OFF) && !GetNrOFPlayingPlayers(true)   ) // !hybrid_font.IsFontPlayersPlaying() // !hybrid_font.active_state()
        { //  STDOUT.println("from ON - waiting off success");
          preActions->waitActions = false;   // mark that we dont have to wait for specific state 
          // On();                   // trigger ON 
          FastOn();
        } 
        else if( (preActions->triggerState == trgON )
                && (hybrid_font.GetState() > HybridFont::STATE_OUT ) )
        {   // STDOUT.println("from OFF - waiting on success");
            preActions->waitActions = false; // mark that we dont gave to waot for specific state 
            Off();                // Trigger OFF
        }
        return;
      } 
      // Entered from  ON an off was required , so we were on so wait for off
      if( (( preActions->triggerState == trgOff) && (hybrid_font.GetState() > HybridFont::STATE_OUT) ) 
        || ( (preActions->triggerState == trgON ) && (hybrid_font.GetState() == HybridFont::STATE_OFF) && !GetNrOFPlayingPlayers(true) ) ) // !hybrid_font.IsFontPlayersPlaying()
      {
        preActions->enter = false;
        if(!CreateMenu(MnuShortcut_Preset, true, 1, presets.size(), userProfile.preset))
        {
          #ifdef DIAGNOSE_MENU
            STDOUT.println("Failed to create Preset Menu");
          #endif
        }

      }
    }

}

  /*  @brief  : Handels profile menu cration (we have to wait until on.off are over )
  *             run in loop to make the job                 
  *   @param  : void 
  *   @retval : void 
  */
void ConfigMenuCreator(MenuPreActions *preActions)
{
      // Profile Menu Creator 
    if(!menu && preActions->enter && (hybrid_font.GetState() == HybridFont::STATE_OFF ))   // (hybrid_font.GetState() == HybridFont::STATE_OFF )
    { 
      bool result;
      preActions->enter = false;
            
      // rolling menu between MnuConfig_Font and MnuConfig_Track, starting with MnuConfig_Font
      result = CreateMenu(Menu_Config, true, MnuConfig_Font, MnuConfig_Track, MnuConfig_Font);

      if(!result)
      {
          #ifdef DIAGNOSE_MENU  
            STDOUT.println("Failed to create Config Menu");
          #endif
      }
      // else STDOUT.println("[menudebug]: Created CONFIG menu starting with FONT submenu.");


    }
}

/*  @brief  : Check if a player is working and if is not free it 
*             We want not to keep the wavplayer streams busy and free as soon as posible 
*   @param  : void
*   @retval : true - player is free or its job was done and became free
*             false -player is still working 
*/
bool PlayerDestroyer()
{
    if(player)    // chek if we have a working player  
    {
      if(!player->isPlaying())  // check if it still has sound to play 
      {
        player->Stop();         // stop player 
        player->CloseFiles();   // close associated file 
        player.Free();          // free the spor from wavplayer 
        return true;  // player has become free 
      }
      return false; // player is still working 
    }
    return true; // we have to active player so is free 
}

// What happens when a menu is destroyed and it transitions to a new one.
void MenuTransition(MenuPreActions *preActions)
{
  if(menu && myMenuActions)
    { 
      if(myMenuActions->CanDestroy())
      { uint16_t  changeActions =myMenuActions->ChangeActions();
        // STDOUT.print("[menudebug]: Menu transition: "); STDOUT.println(changeActions);
        switch(changeActions)
        // switch(myMenuActions->ChangeActions())
        {
          // Tranzition out of CONFIG MENU
          case Menu_Config:
            // STDOUT.print("[menudebug]: Transition out of CONFIG menu - executing PreActions. ");
            if(preActions->triggerState == trgON) {  // we must restore config menu state 
              // On();  
              FastOn();
              #ifdef DIAGNOSE_MENU
                STDOUT.print("Turning on. "); 
              #endif
            } 
            else if (preActions->triggerState == trgOff) { 
              Off(); 
              #ifdef DIAGNOSE_MENU
                STDOUT.print("Turning off. "); 
              #endif
            } 
            if (preActions->resumeTrack) StartOrStopTrack(1);  // resume track if needed
            else StartOrStopTrack(0); // stop track if not needed, in case it was left playing by the track submenu
            #ifdef DIAGNOSE_MENU           
              if (preActions->resumeTrack) STDOUT.println("Starting track.");
              else STDOUT.println("Stopping track.");
            #endif
            preActions->reset();

          break;

          // Tranzition to CONFIG->COLOR MENU
          case MnuConfig_Color:
            if(CreateMenu(MnuConfig_Color, true, 0, 256, (uint16_t)(current_preset_->variation >> 7)))
            { 
              // On();  
              FastOn();
              // STDOUT.println("[menudebug]: Created COLOR submenu ");
              return;   // don't destroy submenu, still need it
            }
          break;
 

          // Tranzition to CONFIG->SENSITIVITY MENU
          case MnuConfig_Sensitivity:
            if(CreateMenu(MnuConfig_Sensitivity, false, 0, 32, Sensitivity::master / 8))
            { 
              // On(); 
              FastOn();
              userProfile.masterBrightness = Sensitivity::master << 8;     // signal current sensitivity with brightness & volume
              userProfile.masterVolume = Sensitivity::master << 8; 
              #ifdef DIAGNOSE_MENU              
                STDOUT.print("Sensitivity = "); STDOUT.print(Sensitivity::master); STDOUT.print(", setting volume to "); STDOUT.print(userProfile.masterVolume); 
                STDOUT.print(" and brightness to "); STDOUT.println(userProfile.masterBrightness);      
              #endif
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
              // STDOUT.println("[menudebug]: Created SENSITIVITY  submenu & running pre-actions");
              return;     // don't destroy submenu, still need it
            }
          break;

            // Tranzition to CONFIG->BRIGHTNESS MENU
            case MnuConfig_Brightness:
            { 
              // Segments seg(MIN_MASTER_BRIGHTNESS, 65535, 20); 
              Segments seg;
              if (stealthMode) seg.Set(MIN_MASTER_BRIGHTNESS, STEALTH_MAX_BRIGHTNESS, 20);         
              else seg.Set(userProfile.stealthBrightness, 65535, 20);
              
              if(CreateMenu(MnuConfig_Brightness, false, 0, 20, seg.valueToSegment(userProfile.masterBrightness)))
              {
                // On();
                FastOn();
                // STDOUT.println("[menudebug]: Created BRIGHTNESS  submenu");
                return;     // don't destroy submenu, still need it
              }
              break;
            }



            case MnuConfig_Volume:
            {
              // Segments seg(MIN_MASTER_VOLUME, 65535, 20); 
              Segments seg;
              if (stealthMode) seg.Set(MIN_MASTER_VOLUME, STEALTH_MAX_VOLUME, 20);
              else seg.Set(userProfile.stealthVolume, 65535, 20);
              if(CreateMenu(MnuConfig_Volume, false, 0, 20, seg.valueToSegment(userProfile.masterVolume)))
              {
                  // On();
                  FastOn();
                  // STDOUT.println("[menudebug]: Created VOLUME submenu ");          
                  return;     // don't destroy submenu, still need it
              }
            break;
            }

            // Tranzition to CONFIG->FONT MENU
            case MnuConfig_Font:
              if(CreateMenu(MnuConfig_Font, true, 1, fonts.count, (uint16_t)(current_preset_->font_index)))
              { 
                // STDOUT.println("[menudebug]: Created FONT submenu ");
                // On();  
                FastOn();
                SaberBase::DoNewFont();    // announce current font 
                return;     // don't destroy submenu, still need it
              }
            break;

            // Tranzition to CONFIG->TRACK MENU
            case MnuConfig_Track:
              if(CreateMenu(MnuConfig_Track, true, 0, tracks.count, (uint16_t)(current_preset_->track_index)))
              { 
                // On();  
                
                // STDOUT.println("[menudebug]: Created TRACK submenu ");
                return;     // don't destroy submenu, still need it
              }
            break;
             
            // Tranzition to CONFIG->PRESET MENU
            case MnuConfig_Preset:
              if(CreateMenu(MnuConfig_Preset, true, 1, presets.size(), userProfile.preset))
              { 
                // STDOUT.println("[menudebug]: Created PRESET submenu ");
                // On();  
                FastOn();
                return;     // don't destroy submenu, still need it
              }
            break;
            
        }
        DestroyMenu();
        lastRecordedEvent = millis(); // we have destroed a menu , so we just exit one , restore time
      } // if(myMenuActions->CanDestroy())
    } // if(menu && myMenuActions)
} // MenuTransition

  /*  @brief  : Handels the menu components like obj destruction and menu change obj , player freeing etc 
  *             run in loop to make the job                 
  *   @param  : void 
  *   @retval : void 
  */
  void MenuLoop()
  { 

    PresetMenuCreator(&shortcutPreset_actions);
    ConfigMenuCreator(&MenuConfig_actions);
    MenuTransition(&MenuConfig_actions);

  }
  /*  @brief : Create navigator for menu interaction
  *   @param : void 
  *   @retval:  -1 - fail to create  
  *             0 - an navigator already exists
  *             1 - navigator created succesfully 
  */
  int8_t CreateMenuNavigator()
  { 
    if(!menu) { // create navigator only if there is none active 
      menu = new TTMenu<uint16_t>(); 
      if(!menu) return -1; 
      return 1;
    }
    return 0;
  }
  /*  @brief : Create actions for menu navigator
  *   @param : type - type of actions  
  *   @retval:  -2 - failt to create , unknown actions 
  *             -1 - fail to create actions  
  *             0 - menu actions already exists   
  *             1 - actions created succesfully 
  */
  int8_t CreateMenuActions(SaberPropMenu type, uint16_t nrseg)
  {
      if(!myMenuActions) {
        switch(type) {
          // creating profile menu actions 
          case Menu_Config: myMenuActions = new menuConfig_t<SaberProp>(this); 
            // STDOUT.println("[menudebug]: CreateMenuActions created menuConfig_t object");
            break;
          // creating preset menu actions
          case MnuShortcut_Preset: myMenuActions = new shortcutPreset_t<SaberProp>(this);
            break;
          // creating color submenu actions 
          case MnuConfig_Color: myMenuActions = new submenuColor_t<SaberProp>(this, nrseg);
            break;
          case MnuConfig_Sensitivity: myMenuActions = new submenuSensitivity_t<SaberProp>(this);
            break;
          // creating brightness submenu actions 
          case MnuConfig_Brightness: myMenuActions = new submenuBrightness_t<SaberProp>(this, nrseg);
            break;
          // creating volume submenu actions 
          case MnuConfig_Volume: myMenuActions = new submenuVolume_t<SaberProp>(this, nrseg);
            break;
          // creating font submenu actions 
          case MnuConfig_Font: myMenuActions = new submenuFont_t<SaberProp>(this);
            break;
          // creating track submenu actions 
          case MnuConfig_Track: myMenuActions = new submenuTrack_t<SaberProp>(this);
            break;
          // creating preset submenu actions 
          case MnuConfig_Preset: myMenuActions = new submenuPreset_t<SaberProp>(this);
            // STDOUT.println("[menudebug]: CreateMenuActions created submenuPreset_t object");

            break;
          
          default:
            return -2;
        }

    

        if(!myMenuActions) return -1;
        return 1; // actions created succesfully 
      }
      return 0;
  }

  /*  @brief  : Handles the creation menu                
  *   @param  : void 
  *   @retval : void 
  */
  bool CreateMenu(SaberPropMenu menuType, bool rolls, uint16_t minT, uint16_t maxT, uint16_t initialValue)
  {   
      int8_t result;
      result = CreateMenuNavigator();
      if (result < 0) {  // failed to create menu navigator , stop here 
        STDOUT.println("Failed to create navigator");
        return false;
      } 
      if(!result) {  // a navigator already exists , perform a change actions 
        menu->ResetActions();  
      }

      result = CreateMenuActions(menuType, maxT-minT);
      if(!result) { // actions already exits , delete them and retry 
        DestroyActions();
        result = CreateMenuActions(menuType, maxT-minT);
      }
      if(result < 0) 
      { 
        STDOUT.println(" Failed to create Actions, destroy everything ");
        DestroyMenu();
        return false;
      }
      // try to link the created actions to the navigator 
      TTMenu<uint16_t>::xMenuActions *myActions;
      myActions = dynamic_cast<TTMenu<uint16_t>::xMenuActions *>(myMenuActions);
      if(!myActions) {
        STDOUT.println(" Fail setting menu actions ");
        DestroyMenu();
        return false;
      } 
      menu->setLimits(rolls, minT, maxT);                              
      menu->SetActions(myActions, initialValue);  // hijacks events *
      return true;
  }

  /*  @brief  : Destroy Menu Actions if any                
  *   @param  : void 
  *   @retval : void 
  */
  void DestroyActions()
  {
    if(myMenuActions) {
      #ifdef DIAGNOSE_MENU    
        STDOUT.println(" Deleting actions ");
      #endif
      delete myMenuActions;
      myMenuActions = 0;
    }
  }

  /*  @brief  : Destroy menu navigator and actions                 
  *   @param  : void 
  *   @retval : void 
  */
  void DestroyMenu()
  {
      if(menu) {
        menu->ResetActions();
        DestroyActions();
        #ifdef DIAGNOSE_MENU    
          STDOUT.println(" Deleting Menu ");
        #endif
        delete menu;
        menu = 0;
        menuDestroyedTimestamp = millis();
      }
  }


  void Help() override {
    PropBase::Help();
    // STDOUT.println(" usPresetMenu - enter ultrasaber Preset Menu ");
    // STDOUT.println(" usProfileMenu - enter ultrasaber Preset Menu ");

  }
  
   void On() override {
    if (!PropBase::CommonIgnition()) return;
    resetDefaults();          // reset prop variables to defaults
    IgnoreClash(600);         // loud poweron will trigger clash
    fusor.UpdateTheta(0);     // reset twist detector
    fusor.UpdateSlide(0);     // reset twist detector
    SaberBase::DoPreOn();
    on_pending_ = true;     // Hybrid font will call SaberBase::TurnOn() for us.
  }

   void FastOn() override {
      if (!PropBase::CommonIgnition()) return;
      resetDefaults();          // reset prop variables to defaults
      IgnoreClash(600);         // loud poweron will trigger clash
      fusor.UpdateTheta(0);     // reset twist detector
      fusor.UpdateSlide(0);     // reset twist detector      
      SaberBase::TurnOn();
      SaberBase::DoEffect(EFFECT_FAST_ON, 0);
  }



private:
  void resetDefaults()
  {
    buttonState = false;        // not pressed
    menuDestroyedTimestamp = 0;
    restoreSettingsLOWBAT = false;
    propMode = NO_MODE;
    lockupTime = 0;
    autoLowPower = false;
    SetStealth(stealthMode, SILENT_STEALTH);
    lastRecordedEvent = millis();
  }
  bool restoreSettingsLOWBAT = false;

  bool buttonState;
  uint16_t lbatOLDVolume;  
  uint32_t menuDestroyedTimestamp;
  // int8_t pointingAt = 0;
  uint32_t lastRecordedEvent;
  bool autoLowPower;  // true if in stealth mode before auto-power-off
  MenuPreActions shortcutPreset_actions;
  MenuPreActions MenuConfig_actions;
  menuInterface<SaberProp>* myMenuActions; // TODO: maybe static?
  public:
    // bool stealthMode = false, setStealth = false;
    // uint16_t stealthOLDBrightness , stealthOLDVolume;
};


#endif
