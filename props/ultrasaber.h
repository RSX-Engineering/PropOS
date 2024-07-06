/*
How buttons behave when using this prop file:

On/Off -                  Zero buttons saber = Twist (2 directional, like revving a motorcycle)
                          ** Note that the motion has to be done long enough to count, so a very quick flick of the wrist will not work.
                          1 button saber = Click to turn the saber on or off.
                          2 button saber = Click POW
                          ** Note, if #define DUAL_POWER_BUTTONS is added to config file,  Clicking either POW or AUX will power on.
                             Also note that POW and AUX become swapped while the saber is on if AUX used to power on.
Turn On muted -           Double-click POW button
Next preset -             Zero button saber = Point up and shake
                          1 or 2 button saber = Hold POW button and hit the blade while saber is off.
Previous Preset -         Hold AUX button and click the POW button while saber is off.
Clash -                   Hit the blade while saber is on.
Lockup -                  Hold either POW or AUX, then trigger a clash. Release button to end.
Drag -                    Hold either POW or AUX, then trigger a clash while pointing down. Release button to end.
Melt -                    Hold either POW or AUX and stab something.
Force Lightning Block -   Click AUX while holding POW.
Force -                   Long-click POW button.
Start Soundtrack -        Long-click the POW button while blade is off.
Blaster block -           Short-click AUX button.
Enter/Exit Color Change - 1 button saber = Hold button and Twist.
                          2 button saber = Hold Aux and click POW while on.
** Note Color Change only works with ProffieOS 3.x and above)
*/

#ifndef PROPS_ULTRASABER_H
#define PROPS_ULTRASABER_H


#include "prop_base.h"

#ifdef OSX_ENABLE_MTP
  #include "../common/serial.h"
#endif 
#define PROP_TYPE Ultrasaber

#include "ultrasaber_menu.h"

// #define AUTOOFF_TIMEOUT 121000
#define AUTOOFF_TIMEOUT installConfig.APOtime
#define SILENT_STEALTH 0
#define ANNOUNCE_STEALTH 1
#define ANNOUNCE_LOWBAT 2
#define ANNOUNCE_TIMEOUT 3


// The Saber class implements the basic states and actions
// for the saber.
class Ultrasaber : public PROP_INHERIT_PREFIX PropBase 
{
private:
    RefPtr<BufferedWavPlayer> player;  // tra..la...la
public:
    // play special sounds for menu interaction 
    bool PlaySpecialSound(bool playedInMenu, Effect * soundEffect, int soundID = -1, uint16_t repeat = 0, float volume = 0)
    {   
        bool resultPlay;
        // STDOUT.print("Prin SS player state"); STDOUT.println(player ? "something": "null");
        if (!player) player = GetFreeWavPlayer();
        if (!player)  {
          STDOUT.println("Ultrasaber prop cannot get free player!");
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

  Ultrasaber() : PropBase() 
  {    
    lastRecordedEvent = millis();
    myMenuActions = 0;
    presetMenuPreActions.reset();
    profileMenuPreActions.reset();
    specialMODE.reset();

    // player = 0;
  }
  const char* name() override { return "Ultrasaber"; }
  
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
    // if (event==EVENT_2TAP) { // || button==BUTTON_POWER) {
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
      // ---------------------------------------------------------------------------
      // ----------------- POINTING DETECTIONS ------------------------------------
      // BTN IS BEING PRESSED SO GET POINtING , it might come a drag or lockup 
      // case EVENTID(BUTTON_POWER, EVENT_PRESSED, MODE_ON):
      // { 
      //   // if power button is pressed , saber on 
      //   if (accel_.x < -0.25) pointingAt = -1;    // pointing down 
      //   else if(accel_.x > 0.60) pointingAt = 1;  // pointing up 
      //   else pointingAt = 0;                      // bweteen up and down
      //   return true;
      // }
      // ---------------- END OF POITING DETECTIONS --------------------------------
      // ---------------------------------------------------------------------------

      // ---------------------------------------------------------------------------
      // ------------------- PROP ACTIONS ALWAYS ON --------------------------------

      // Short click pressed while off , POWER ON 
      case EVENTID(BUTTON_POWER, EVENT_CLICK_SHORT, MODE_OFF):
      {
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
        On();                     // Do ON, including reset and ignore clash
        // IgnoreClash(600);         // loud poweron will trigger clash
        return true;
      }
      // Short click pressed while on , POWER OFF  
      case EVENTID(BUTTON_POWER, EVENT_CLICK_SHORT, MODE_ON):
      { 
        static uint32_t lastBlast=0;
        if(specialMODE.smode == BLAST_MODE) {  // if blast mode active do blast 
            uint32_t timeNow = millis();
            if (timeNow - lastBlast > 500) {
              #ifdef DIAGNOSE_PROP
                  STDOUT.println("Blast!");
              #endif 
              SaberBase::DoBlast();               
              lastBlast - timeNow;
            }
            IgnoreClash(600);    // loud blast sounds will trigger clashes
        }
        // else if (!SaberBase::Lockup() && !specialMODE.smode) { // if no lock is active and not in lightning mode 
        else { 
          #ifdef DIAGNOSE_PROP
            STDOUT.println("SHORT CLICK detected! Turning OFF.");
          #endif           
          Off();                                              // do OFF 
          resetDefaults();
        }
        return true;
      }
      // BTN held long , either in OFF or ON , play track
      case EVENTID(BUTTON_POWER, EVENT_HELD_LONG, MODE_OFF):
        if(!CheckCanStart() && !track_player_) {  
          #ifdef DIAGNOSE_PROP
            STDOUT.println("LONG CLICK detected! Battery too low to turn on track.");
          #endif 
          return true;
        }
      case EVENTID(BUTTON_POWER, EVENT_HELD_LONG, MODE_ON):
      {
        if(!SaberBase::Lockup() && !specialMODE.source && !specialMODE.smode)
        {
          #ifdef DIAGNOSE_PROP
            STDOUT.print("LONG CLICK detected! Turn track ");
            if (track_player_)  STDOUT.println(" OFF");
            else STDOUT.println(" ON");
          #endif 
          StartOrStopTrack();
          
        }
        return true;
      }
      //PROFILE MENU -  Twist while btn pressed , either in OFF or ON , prepare menu enter  
      case EVENTID(BUTTON_NONE, EVENT_TWIST , BUTTON_POWER | MODE_OFF): 
      {
        if(!CheckCanStart()) return true; 
        if(!profileMenuPreActions.enter) 
          profileMenuPreActions.triggerState = trgOff; // mark triger state only if we are not executiong other actions
      }
      case EVENTID(BUTTON_NONE, EVENT_TWIST , BUTTON_POWER | MODE_ON):
      {
        if(!menu && !myMenuActions && !specialMODE.smode) 
        { 
          #ifdef DIAGNOSE_PROP
            STDOUT.println("Starting PROFILE menu.");
          #endif 
          if(!profileMenuPreActions.enter)
          { 
            profileMenuPreActions.enter = true;               // set enter preactions menus, MenuPresetHandlerCreator() will take cre of the rest
            if(profileMenuPreActions.triggerState == trgDef)  // only trigger state has default value ,
              { 
                profileMenuPreActions.triggerState = trgON;     //  mark trigger state as on
                Off();
                hybrid_font.check_postoff_ = false; // disable postoff until the next Off()
              }
          }
        }
        return true;

      }

       // PRESET MENU - Second short click pressed , either in oFF or ON , prepare menu enter 
      case EVENTID(BUTTON_POWER,  EVENT_SECOND_CLICK_SHORT, MODE_OFF):
      {
       if(!CheckCanStart()) return true;
        if(presets.size() <= 1) {
          PlaySpecialSound(true, &emojiSounds, cancel, 0, 1);
          return true;
        } 
       if(!presetMenuPreActions.enter) 
          presetMenuPreActions.triggerState = trgOff; // mark triger state only if we are not executiong other actions
      }
      case EVENTID(BUTTON_POWER,  EVENT_SECOND_CLICK_SHORT, MODE_ON):
      {   
          hybrid_font.check_postoff_ = false; // disable postoff until the next Off()
        if(!menu && !myMenuActions) {
          #ifdef DIAGNOSE_PROP
            STDOUT.println("Starting PRESET menu.");
          #endif           
          if(presets.size() <= 1) {
            PlaySpecialSound(true, &emojiSounds, cancel, 0, 1);
            return true;
          }
          if(!specialMODE.smode && (millis() - menuDestroyedTimestamp > 1500) ) 
          {
            if(!presetMenuPreActions.enter) 
            {
              presetMenuPreActions.enter = true;
              if (!IsOn()) { 
                presetMenuPreActions.waitActions = true;    // wait to turn back on if menu was triggered while ON (saber went off before the second click)
                presetMenuPreActions.off = false;
              }
              else { 
                presetMenuPreActions.waitActions = false;
                presetMenuPreActions.off = true;        // turn back off when menu ends
              }
              presetMenuPreActions.triggerState = trgOff; 
            }
          }
        }
        return true;
      }
      case EVENTID(BUTTON_NONE, EVENT_2TAP,  MODE_OFF | BUTTON_POWER):   // toggle STEALTH while off
          if(!CheckCanStart()) return true; 
          
      case EVENTID(BUTTON_NONE, EVENT_2TAP, MODE_ON): // toggle STEALTH while on
      case EVENTID(BUTTON_NONE, EVENT_2TAP, MODE_ON | BUTTON_POWER): // toggle STEALTH while on
      // {   if (!buttonState || specialMODE.smode) return true;
      {   if (!buttonState) return true;
          // STDOUT.print("PROP DOUBLE TAP - button = "); STDOUT.println(buttonState);
          // stealthMode = !stealthMode;
          #ifdef DIAGNOSE_PROP
            STDOUT.print("DOUBLE TAP detected, set stealth mode to ");
            STDOUT.println(!stealthMode);
          #endif //  DIAGNOSE_PROP          
          SetStealth(!stealthMode); 
       
            return true;
      }


      // ---------------------------------------------------------------------------
      // ------------------- END OF PROP ACTIONS -----------------------------------

      // ---------------------------------------------------------------------------
      // ---------------------- PROP MODES BLAST & LATCHED --------------------------

      // LATCHED ON - shake while btn , mark latched on 
      case EVENTID(BUTTON_NONE, EVENT_SHAKE, MODE_ON | BUTTON_POWER):  
      { 
        if(!SaberBase::Lockup() && (specialMODE.smode != BLAST_MODE)) 
        { 
            #ifdef DIAGNOSE_PROP
              STDOUT.println("SHAKE detected! LATCHED for lockup.");
            #endif 
            // smooth_swing_v2.Pause(true);
            if(!specialMODE.smode) {
              PlaySpecialSound(false, &emojiSounds, latchedON, 0, 1);
            }
            specialMODE.source = shakeLatch;
            return true;
        }
        // we did not fullfill the condition to enter LATCHED , maybe we are in lockup or blast mode 
        specialMODE.source = shakeDef; // set shake source to default not to mistreat end shake 
        return true;
      }
      // BLAST ON - shake detected  . blast mode  
      case EVENTID(BUTTON_NONE, EVENT_SHAKE, MODE_ON):   
      {
         if(!SaberBase::Lockup() && (specialMODE.smode != LATCH_MODE)) 
         {
          // STDOUT.println("Shake!");
          smooth_swing_v2.Pause(true);
          if(specialMODE.smode == BLAST_MODE) {
            PlaySpecialSound(false, &emojiSounds, blastOFF, 0, 1); 
            #ifdef DIAGNOSE_PROP
              STDOUT.println("SHAKE detected! Blast mode OFF.");
            #endif             
          }
          else {  // we are about to enter blast mode 
            PlaySpecialSound(false, &emojiSounds, blastON, 0, 1);
            specialMODE.persistent = true;
            #ifdef DIAGNOSE_PROP
              STDOUT.println("SHAKE detected! Will enter Blast mode.");
            #endif             
          }
          specialMODE.source = shakeBlast;
          return true;
         } else if(specialMODE.smode == LATCH_MODE) 
         {
            // if(SaberBase::Lockup() == SaberBase::LOCKUP_LIGHTNING_BLOCK) {
              #ifdef DIAGNOSE_PROP
                STDOUT.println("SHAKE detected! Latched lockup disabled.");
              #endif               
              SaberBase::SetLockup(SaberBase::LOCKUP_NORMAL);        // do end lockup                    
              SaberBase::DoEndLockup();
              SaberBase::SetLockup(SaberBase::LOCKUP_NONE);
            // }
            specialMODE.source = shakeLatch;
            return true;
         }
         specialMODE.source = shakeDef; // set shake source to default not to mistreat end shake 
        return true;
      } 
      // CLATCH AND BLAST MODE --- detect and end chake 
      case EVENTID(BUTTON_NONE, EVENT_ENDSHAKE, MODE_ON):  
      { 
        switch(specialMODE.source)    // check if we have a valid shake source 
        {
          case shakeBlast:    // we are coming from start of blast
          {
            if(specialMODE.persistent) // we have to check blast charging 
            {
              if(!player) specialMODE.persistent = false;  // we asume that pleyer destroyer freed the player and it sound stoped
              else if(player->isPlaying()) 
              {
                float totalSec = player->length();
                float currentSec = player->pos();
                float procentPlayed;
                if(!totalSec) {                   // if lenght is 0 , it shouldnt 
                  if(!currentSec)currentSec = 1;  // something is wrong we end the skake and we didnt played at all
                  totalSec = currentSec * 2; // avoid division by zero if case , we dou
                }
                procentPlayed = currentSec * 100;
                procentPlayed /= totalSec;

                // STDOUT.print("Pr played ");  STDOUT.println(procentPlayed);

                if(procentPlayed > 70) specialMODE.persistent =false;
              }
            }

            if(!specialMODE.smode && !specialMODE.persistent) // we dind have any previous mode set latch 
            {
                specialMODE.smode = BLAST_MODE;
                specialMODE.source = shakeDef; // reset shake source
                #ifdef DIAGNOSE_PROP
                  STDOUT.println("Blast mode ON");
                #endif                 
            }
            else {
              specialMODE.reset();
            }

            FadeAndStopSS();                // fade and stop active sound 
            smooth_swing_v2.Pause(false);   // resume smoothswing
          }
          break;

          case shakeLatch:
          {
            if(!specialMODE.smode) // we dind have any previous mode set latch 
            {
              specialMODE.source = shakeDef; // reset shake source 
              specialMODE.smode = LATCH_MODE;
              SaberBase::DoEndLockup();
              SaberBase::SetLockup(SaberBase::LOCKUP_NONE);
              
            }
            else if(specialMODE.smode == LATCH_MODE) {
              specialMODE.reset();
            }
            FadeAndStopSS();
            smooth_swing_v2.Pause(false);

            break;
          }
        }

        return true;
      }
      // LAtCHED MODE trigger  
      case EVENTID(BUTTON_NONE, EVENT_CLASH, MODE_ON):
      {
        if(specialMODE.smode == LATCH_MODE)
        {   
            // STDOUT.println("START LIGHTING");
            #ifdef DIAGNOSE_PROP
              STDOUT.println("CLASH detected while latched! Turning Lockup ON.");
            #endif             
            SaberBase::SetLockup(SaberBase::LOCKUP_LIGHTNING_BLOCK);
            SaberBase::DoBeginLockup();
            lockUpLightTimestamp = millis();
            IgnoreClash(300); // todO 
            return true;
        }
        break; // we need to return false to be cathced by the prop clash 
      }
      // end lockup lighting
      case EVENTID(BUTTON_NONE, EVENT_SWING, MODE_ON):
      { 
        if(specialMODE.smode == LATCH_MODE && 
            SaberBase::Lockup() == SaberBase::LOCKUP_LIGHTNING_BLOCK && (millis() - lockUpLightTimestamp > 200))
        {
          #ifdef DIAGNOSE_PROP
            STDOUT.println("SWING detected while in latched lockup! Turning Lockup OFF.");
          #endif           
          SaberBase::DoEndLockup();
          SaberBase::SetLockup(SaberBase::LOCKUP_NONE);
          specialMODE.reset();
          return true;
        }
        break; // we need to return false to be cathced by the prop swing?
      }

      // ------------------------------------------------------------------
      // ---------------------- END PROP MODES BLAST & LATCHED ------------

      // -------------------------------------------------------------------  
      // -------------------- PROP EFFECTS --------------------------------
      // TWIST , no btn do force 
      case EVENTID(BUTTON_NONE, EVENT_TWIST, MODE_ON):  // EVENT_THIRD_SAVED_CLICK_SHORT
      { 
        #ifdef DIAGNOSE_PROP
          STDOUT.println("TWIST detected!");
        #endif //  DIAGNOSE_PROP
        SaberBase::DoForce();
        return true;
      } 
      // LOCKUP and DRAG - clash with BTN 
      case EVENTID(BUTTON_NONE, EVENT_CLASH, MODE_ON | BUTTON_POWER):
      {
        if (!SaberBase::Lockup()  && !specialMODE.smode) {           
          if (fusor.angle1() <= -0.75) {     // poiting down , do drag 
            SaberBase::SetLockup(SaberBase::LOCKUP_DRAG);
            SaberBase::DoBeginLockup();
            #ifdef DIAGNOSE_PROP
              STDOUT.println("CLASH detected pointing up! Triggering Drag.");
            #endif              
            return true;
          } else if(fusor.angle1() >= 0.75) { // poiting up do lockup 
            SaberBase::SetLockup(SaberBase::LOCKUP_NORMAL);
            SaberBase::DoBeginLockup();
            #ifdef DIAGNOSE_PROP
              STDOUT.println("CLASH detected pointing down! Triggering Lockup.");
            #endif              
            return true;
          }
        }
        break;
      }

      // Horizontal STAB (no button)
      case EVENTID(BUTTON_NONE, EVENT_STAB, MODE_ON):
      { 
        // MAKE MELT if blaster Mode off and Latching Lockup Off
        if (!SaberBase::Lockup() && !specialMODE.smode)  {
              SaberBase::DoStab();
              #ifdef DIAGNOSE_PROP
                STDOUT.println("STAB detected!");
              #endif //  DIAGNOSE_PROP
          return true;
        }
        break;
      }


      // MELT - make stab gesture with btn
      case EVENTID(BUTTON_NONE, EVENT_STAB, MODE_ON | BUTTON_POWER):
      { 
        // MAKE MELT if blaster Mode off and Latching Lockup Off
        if (!SaberBase::Lockup() && !specialMODE.smode)  {
          if (abs(fusor.angle1()) <= 0.25) {
            SaberBase::DoStab();
            SaberBase::SetLockup(SaberBase::LOCKUP_MELT);
            SaberBase::DoBeginLockup();
            #ifdef DIAGNOSE_PROP
              STDOUT.println("Horizontal STAB detected! Triggering Melt.");
            #endif //  DIAGNOSE_PROP
          }
          return true;
        }
        break;
      }
    

      // Events that needs to be handled regardless of what other buttons
      // are pressed.
      case EVENTID(BUTTON_POWER, EVENT_RELEASED, MODE_ANY_BUTTON | MODE_ON):
      {
        if (!specialMODE.smode && SaberBase::Lockup()) {
          // SaberBase::DoEndLockup();
          SaberBase::SetLockup(SaberBase::LOCKUP_NONE);
          SaberBase::DoEndLockup();     // no end lockup, only if latched
          IgnoreClash(300); // todO 
          return true;
        }
        break;
      }
      // ---------------------- END PROP EFFECTS ---------------------------
    } // switch
    
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
        #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
          if (menu) RequestPower();
        #endif

          SaberBase::RequestMotion(); // request motion 
		      Detect2Tap(accResultant);               // run double-tap detection 
          DetectTwist();              // run twist detection  
          DetectSwing();              // run swing detection 
		      DetectShake();              // run shake detection 
          if (SaberBase::IsOn()) {
            // Auto low power:
            if (menu) lastRecordedEvent = millis();   // don't go into low power if menu is active
            if (!autoLowPower) {
              if (millis() - lastRecordedEvent > 3* (AUTOOFF_TIMEOUT/4) && !stealthMode) {
                  // enter auto low power after 75% of auto off time: force stealth mode if it was off
                  autoLowPower = true;
                  SetStealth(true, ANNOUNCE_TIMEOUT);
                  STDOUT.print(millis()); STDOUT.println(": ENTER auto low power");
              }
            }
            else {
              if (millis() - lastRecordedEvent < 100) {
                  // exit auto low power, only if we previously forced stealth on
                  autoLowPower = false;
                  SetStealth(false);
                  STDOUT.print(millis()); STDOUT.println(": EXIT auto low power");
              }
            } 
            // Auto power off:
            if(SaberBase::IsOn() && (millis() - lastRecordedEvent > AUTOOFF_TIMEOUT)) {
              Off();
              if (autoLowPower) SetStealth(false, SILENT_STEALTH); // It went in stealth mode at auto power off, so we restore stealth=OFF for next start.
              autoLowPower = false;
              STDOUT.print(millis()); STDOUT.println(": AUTO POWER OFF");
              lastRecordedEvent = millis();
            }            
          }
 
        if (PlayerDestroyer()) {    // just ended a sound and freed the prop player
            if(restoreSettingsLOWBAT) {  // restore volume after a failed attempt to power on; LowBat sound just ended.
              restoreSettingsLOWBAT = false;    // set by CheckCanStart
              STDOUT.println("Player destroyed");
              if (stealthMode) userProfile.masterVolume = userProfile.stealthVolume;  // restore volume according to stealth mode
              else userProfile.masterVolume = userProfile.combatVolume;
              dynamic_mixer.set_volume(VOLUME);
              #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
              SilentEnableAmplifier(false);      
              SilentEnableBooster(false);
              #endif
            }
        }
        MenuLoop();       // create/destroy menu actions
        StealthLoop();   // sweep volume and brightness if needed        


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
    STDOUT.println("Battery too low to start");
    if (millis() - lastDenial < 3000) return false;   // battery low but we recently announced, won't do it again
    // battery low and need to announce:
    lastDenial = millis();
    userProfile.masterVolume = 8000;    // set low volume to announce low battery
    dynamic_mixer.set_volume(VOLUME);
    restoreSettingsLOWBAT = true;
    #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
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

// void SetStealth(bool on)
// {

//     SilentEnableAmplifier(true);
//     SilentEnableBooster(true);

//   if(on) {
//     stealthOLDBrightness = userProfile.masterBrightness;
//     stealthOLDVolume = userProfile.masterVolume;
//     // userProfile.masterBrightness = stealthOLDBrightness >> 1;
//     userProfile.masterBrightness = userProfile.stealthBrightness;
//     userProfile.masterVolume = userProfile.stealthVolume;
//     dynamic_mixer.set_volume(VOLUME);
    
//     // if (!userProfile.masterVolume) {
//     //     SilentEnableAmplifier(false);		// silent OFF 
//     //     SilentEnableBooster(false);
//     // }
//     PlaySpecialSound(true, &emojiSounds, stealthON, 0 ,1);   
//     STDOUT.println("Stealth ON");
//   } 
//   else {
//     userProfile.masterBrightness = stealthOLDBrightness;
//     userProfile.masterVolume = stealthOLDVolume;
//     dynamic_mixer.set_volume(VOLUME);
//     // SilentEnableAmplifier(true);
//     // SilentEnableBooster(true);
//     PlaySpecialSound(true, &emojiSounds, stealthOFF, 0 ,1);    
//     STDOUT.println("Stealth OFF");
//   }
     
// }

// Apply and announce volume and brightness for the new stealth mode
uint16_t targetVol, targetBr;   // target volume and brightness, for sweep signalling of stealth modes
void SetStealth(bool newStealthMode, uint8_t announce = ANNOUNCE_STEALTH) override {
    if (announce) {   // announce: play special sound and sweep volume and brightness to new values
        #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
        SilentEnableAmplifier(true);  // ... just in case it was previously muted...
        SilentEnableBooster(true);
        #endif    
        float vol = 0.5;
        // End lockup or drag, might have been triggered by the first tap (depends on sensitivities)
        if (IsOn()) { 
          vol = 2;
          if (!specialMODE.smode && SaberBase::Lockup()) { 
              SaberBase::SetLockup(SaberBase::LOCKUP_NONE);   // no end sound
              SaberBase::DoEndLockup();
          }
        }
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
      STDOUT.print("Sweeping volume to "); STDOUT.print(targetVol); 
      STDOUT.print(" and brightness to "); STDOUT.println(targetBr); 
	    #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
        if (!userProfile.masterVolume) { // mute
          SilentEnableAmplifier(false);         
          SilentEnableBooster(false);  
        }
      #endif 
      if (!IsOn()) {  // saber is off so we need to stop audio
        if (player) {
          player->FadeAndStop();         // stop player 
          player.Free();                // free wavplayer 
          hybrid_font.getHum()->SetFollowing(hybrid_font.getHum()); // restore looping for hum
        }
      }
    }
}

enum shakeSource
{
  shakeDef = 0,
  shakeBlast = 1,
  shakeLatch = 2
};

enum SpecialMode
{
  NO_MODE = 0,
  BLAST_MODE,
  LATCH_MODE
};

class SMode
{ 
  public:

  shakeSource source;
  bool persistent;
  SpecialMode smode;
  SMode() { reset();}
  void reset()
  {
    source = shakeDef; persistent = false; smode = NO_MODE;
  }
};

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
  bool waitActions;
  int8_t triggerState;
  MenuPreActions(){ enter = false; waitActions = false; triggerState = trgDef;}
  void reset() { 
    off = false;
    enter = false; 
    waitActions = false; 
    triggerState = trgDef; 
    } 
};
  /*  @brief  : Handels preset menu cration (we have to wait until on.off are over )              
  *   @param  : void 
  *   @retval : void 
  */
void MenuPresetHandlerCreator(MenuPreActions *psPreActions)
{

     if(!menu && !psPreActions->enter && psPreActions->off) {
        psPreActions->reset(); 
        Off(SILENT_OFF); 
     }

    // PRESET MENU HADLER CREATOR
    if(!menu && psPreActions->enter)
    {
      if(psPreActions->waitActions)   // we must wait specific action to complete before creating menu 
      { 
        if(( psPreActions->triggerState == trgOff)  
              &&  (hybrid_font.GetState() == HybridFont::STATE_OFF) && !GetNrOFPlayingPlayers(true)   ) // !hybrid_font.IsFontPlayersPlaying() // !hybrid_font.active_state()
        { //  STDOUT.println("from ON - waiting off success");
          psPreActions->waitActions = false;   // mark that we dont have to wait for specific state 
          On();                   // trigger ON 
        } 
        else if( (psPreActions->triggerState == trgON )
                && (hybrid_font.GetState() > HybridFont::STATE_OUT ) )
        {   // STDOUT.println("from OFF - waiting on success");
            psPreActions->waitActions = false; // mark that we dont gave to waot for specific state 
            Off();                // Trigger OFF
        }
        return;
      } 
      // Entered from  ON an off was required , so we were on so wait for off
      if( (( psPreActions->triggerState == trgOff) && (hybrid_font.GetState() > HybridFont::STATE_OUT) ) 
        || ( (psPreActions->triggerState == trgON ) && (hybrid_font.GetState() == HybridFont::STATE_OFF) && !GetNrOFPlayingPlayers(true) ) ) // !hybrid_font.IsFontPlayersPlaying()
      {
        psPreActions->enter = false;
        if(!CreateUltraMenu(xUltraMenu_Preset, true, 1, presets.size(), userProfile.preset))
        {
          STDOUT.println("Failed to create Preset Menu");
        }
        // psPreActions->reset();    // preactions reset menu was created succesfully !
      }
    }

}

  /*  @brief  : Handels profile menu cration (we have to wait until on.off are over )
  *             run in loop to make the job                 
  *   @param  : void 
  *   @retval : void 
  */
void MenuProfileHandlerCreator(MenuPreActions *pfPreActions)
{
      // Profile Menu Creator 
    if(!menu && pfPreActions->enter && (hybrid_font.GetState() == HybridFont::STATE_OFF ))   // (hybrid_font.GetState() == HybridFont::STATE_OFF )
    { 
      bool result;
      pfPreActions->enter = false;
    if(!installConfig.monochrome) // we dont have a monochrome menu so create a color menu
        // result = CreateUltraMenu(xUltraMenu_Profile, true, color, volume, brightness);
        result = CreateUltraMenu(xUltraMenu_Profile, true, xUltraMenu_Color, xUltraMenu_Volume, xUltraMenu_Color);
      else 
        result = CreateUltraMenu(xUltraMenu_Profile, true, xUltraMenu_Sensitivity, xUltraMenu_Volume, xUltraMenu_Volume);
      if(!result)
      {
          STDOUT.println("Failed to create Profile Menu");
      }
      // pfPreActions->reset();
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

void MenuChangerDestroyer(MenuPreActions *mPreActions)
{
  if(menu && myMenuActions)
    {
      if(myMenuActions->CanDestroy())
      { 
        switch(myMenuActions->ChangeActions())
        {
          case xUltraMenu_Profile:
              if(mPreActions->triggerState == trgON) On();  // silent on ?               // we must restore profile menu state 
              else if (mPreActions->triggerState == trgOff) Off();
              mPreActions->reset();
          break;

          case xUltraMenu_Color:
          {
            if(CreateUltraMenu(xUltraMenu_Color, true, 0, 256, (uint16_t)(current_preset_->variation >> 7)))
            { 
              On();  return;
            }
            break;
          }  
        case xUltraMenu_Sensitivity:
          {
            if(CreateUltraMenu(xUltraMenu_Sensitivity, false, 0, 32, Sensitivity::master / 8))
            { 
              On(); 
              userProfile.masterBrightness = Sensitivity::master << 8;     // signal current sensitivity with brightness & volume
              userProfile.masterVolume = Sensitivity::master << 8; 
              STDOUT.print("Sensitivity = "); STDOUT.print(Sensitivity::master); STDOUT.print(", setting volume to "); STDOUT.print(userProfile.masterVolume); 
              STDOUT.print(" and brightness to "); STDOUT.println(userProfile.masterBrightness);      
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
              return;
            }
            break;
          }  

          case xUltraMenu_Bright:
          { 
            // Segments seg(MIN_MASTER_BRIGHTNESS, 65535, 20); 
            Segments seg;
            if (stealthMode) seg.Set(MIN_MASTER_BRIGHTNESS, STEALTH_MAX_BRIGHTNESS, 20);         
            else seg.Set(userProfile.stealthBrightness, 65535, 20);
            
            if(CreateUltraMenu(xUltraMenu_Bright, false, 0, 20, seg.valueToSegment(userProfile.masterBrightness)))
            {
              On();
              return;
            }
            break;
          }

          case xUltraMenu_Volume:
          {
            // Segments seg(MIN_MASTER_VOLUME, 65535, 20); 
            Segments seg;
            if (stealthMode) seg.Set(MIN_MASTER_VOLUME, STEALTH_MAX_VOLUME, 20);
            else seg.Set(userProfile.stealthVolume, 65535, 20);
            if(CreateUltraMenu(xUltraMenu_Volume, false, 0, 20, seg.valueToSegment(userProfile.masterVolume)))
            {
                On();
                return;
            }
            break;
          }

        }
        DestroyMenu();
        lastRecordedEvent = millis(); // we have destroed a menu , so we just exit one , restore time
      }
    }
}

  /*  @brief  : Handels the menu components like obj destruction and menu change obj , player freeing etc 
  *             run in loop to make the job                 
  *   @param  : void 
  *   @retval : void 
  */
  void MenuLoop()
  { 

    MenuPresetHandlerCreator(&presetMenuPreActions);
    MenuProfileHandlerCreator(&profileMenuPreActions);
    MenuChangerDestroyer(&profileMenuPreActions);

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
  int8_t CreateMenuActions(UltrasaberMenu type, uint16_t nrseg)
  {
      if(!myMenuActions) {
        switch(type) {
          // creating profile menu actions 
          case xUltraMenu_Profile: myMenuActions = new menuProfile_t<Ultrasaber>(this); 
            break;
          // creating preset menu actions
          case xUltraMenu_Preset: myMenuActions = new menuPreset_t<Ultrasaber>(this);
            break;
          // creating color menu actions 
          case xUltraMenu_Color: myMenuActions = new menuProfileColor_t<Ultrasaber>(this, nrseg);
            break;
          case xUltraMenu_Sensitivity: myMenuActions = new menuProfileSensitivity_t<Ultrasaber>(this);
            break;
          // creating brightness menu actions 
          case xUltraMenu_Bright: myMenuActions = new menuProfileBrightness_t<Ultrasaber>(this, nrseg);
            break;
          // creating volume menu actions 
          case xUltraMenu_Volume: myMenuActions = new menuProfileVolume_t<Ultrasaber>(this, nrseg);
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
  bool CreateUltraMenu(UltrasaberMenu menuType, bool rolls, uint16_t minT, uint16_t maxT, uint16_t initialValue)
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
        STDOUT.println(" Fail setting actions ");
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
      STDOUT.println(" Deleting actions ");
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
        STDOUT.println(" Deleting Menu ");
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
    SaberBase::DoPreOn();
    on_pending_ = true;
    // Hybrid font will call SaberBase::TurnOn() for us.
  }

private:
  void resetDefaults()
  {
    buttonState = false;        // not pressed
    announceStealth = 0;
    // pointingAt = 0;             // used to determine where the tip of saber is pointing 
    lockUpLightTimestamp = 0;
    menuDestroyedTimestamp = 0;
    announceStealth = false;
    restoreSettingsLOWBAT = false;
    specialMODE.reset();
    autoLowPower = false;
    SetStealth(stealthMode, SILENT_STEALTH);
    lastRecordedEvent = millis();
  }
  bool restoreSettingsLOWBAT = false;
  uint32_t announceStealth = 0;   // time when to announce change of stealth mode

  bool buttonState;
  uint32_t lockUpLightTimestamp;
  uint16_t lbatOLDVolume;  
  uint32_t menuDestroyedTimestamp;
  // int8_t pointingAt = 0;
  uint32_t lastRecordedEvent;
  bool autoLowPower;  // true if in stealth mode before auto-power-off
  SMode specialMODE;
  MenuPreActions presetMenuPreActions;
  MenuPreActions profileMenuPreActions;
  menuInterface<Ultrasaber>* myMenuActions; // TODO: maybe static?
  public:
    // bool stealthMode = false, setStealth = false;
    // uint16_t stealthOLDBrightness , stealthOLDVolume;
};


#endif
