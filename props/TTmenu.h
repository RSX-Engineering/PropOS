/****************************************************************
 *  TWIST & TICK MENU NAVIGATION                                *
 *  (C) Marius RANGU @ RSX Engineering                          *
 *  fileversion: v1.0 @ 2022/10;    license: GPL3               *
 ****************************************************************
 * START    TTMenu<uint16_t> myMenu;                             *
 *          myMenu.SetLimits(...);                              *
 *          myMenu.SetActions(defaultState);  // hijacks events *
 * CHANGE   myMenu.ResetActions();   // releases events         *
 *          myMenu.SetLimits(...);                              *
 *          myMenu.SetActions(newState); // hijacks events      *
 * STOP     myMenu.ResetActions();   // releases events         *
 ***************************************************************/
 


// #define X_MENUTEST



#ifndef TTMENU_H
#define TTMENU_H

#include "../common/Interpolator.h"


// Menu  sound effects
#define XSOUNDS_PATH        "_osx_/ss"     // common path for emoji sounds
Effect emojiSounds("emoji");    // emoji sounds
Effect emojiBackgrounds("emback"); // also an emoji sound, but need it on a different effect to play on background
Effect menuSounds("voice");       // menu labels

enum emojiSounds_id {     
    tickUP = 1,             // tick-by-tick
    tickDN = 2,
    tickEND = 3,            // end of navigation options
    scrollUP = 4,           // tick while scrolling
    scrollDN = 5,
    stopUP = 6,             // stop scrolling up with reverse motion
    stopDN = 7,             
    ack = 8,                // acknowledge
    cancel = 9,
    stealthON = 10,         // entering stealth (mute) mode
    stealthOFF = 11,        
    error = 12,             // critical error
    warning = 13,           // non-critical error
    lowbat = 14,            // low battery power off   
    connected = 15,         // established computer connection 
    disconnected = 16,      // break computer connection
    blastON = 17,           // 3-5 seconds 
    blastOFF = 18,          // shor max 1 s
    latchedON = 19,         // distinctive sound 
    boot = 20,              // boot confirmation
    menu = 21,              // menu annunciator

    emojiMAX = menu         // update if needed!
};

enum emojiBackgrounds_id {     
    skip = 1,               // menu navigation temporary disabled 
    busy = 2,               // working on something
    

    backMAX = busy         // update if needed!
};

enum menuVoices_id {    
    annunciator = 1,        // menu annunciator 
    color = 2,              // 'color' spoken
    brightness = 3,         // 'brightness' spoken
    volume = 4,             // 'volume' spoken
    sensitivity = 5,        // 'sensitivity' spoken
    longColor = 6,          // 'color' spoken, pre-mixed with menu annunciator
    longVolume = 7,         // 'volume' spoken, pre-mixed with menu annunciator

    menuMAX = longVolume   // update if needed!
};

// Menu navigator parameters
#define MENU_PERIOD         10              // Menu Loop() period [ms]. Need it constant for scroll timing
#define MENU_TICKTH         userProfile.menuSensitivity.tickThreshold             // Theta threshold for ticks
#define MENU_SKIPTH         MENU_TICKTH/2             // Theta threshold for skipping
#define MENU_STARTSCROLL    userProfile.menuSensitivity.startScroll             // Gyro threshold to trigger scrolling
#define MENU_STOPSCROLL     MENU_STARTSCROLL-200             // Gyro threshold to stop scrolling: 600
#define MENU_SCROLLPAUSE    500             // Milliseconds to pause scroll controls after a registered action, so same motion won't trigger two actions
#define MENU_MAXSCROLL      userProfile.menuSensitivity.maxScroll            // Maximum scroll index. See scrollIndex. 
#define MENU_MAXSCROLLTIME  5000            // Maximum scroll duration [ms] - at MENU_MAXSCROLL
#define MENU_STABLETIME     userProfile.menuSensitivity.stableTime             // Time [ms] tick must stay unchanged to consider it stable
#define MENU_ENDREMINDER    1500            // Minimum time between successive 'endTick' sounds 

// helpers
#define TPS2PER(X) 1000/X       // Period in [ms] for X [ticks per second] (for transfer function)
#define MENU_MINSCROLLTICK  0.015            // minimum time to play a tick before allowing to be interrupted [s] - relevant only for scroll 
#define MENU_TPSREFRESHTIME  20              // refresh time [ms] for scrolling feedback (tps will not change continuously, noisy and pointless)
#define SILENT_TICK true

template <typename TYPE>
int8_t signof(TYPE value) { return (value >= 0 ? (value == 0 ? 0 : 1) : -1); }

// main class:
template <class TYPE>
class TTMenu : public Looper {
private:
    TYPE tick;                  // current position in menu
    TYPE stableTick;            // current stable position in menu (needs to remain unchanged MENU_STABLETIME [ms] to become stable)
    TYPE minTick, maxTick;     // tick limits - defaults to TYPE limits
    bool rolling;                   // true if goes around min <-> max
    bool skipping;                  // true while rotating with button pressed
    bool buttonState;               // current state of button, to control skipping. Updated by Event()
    int16_t scrollIndex;           // This is an abstract number that |decrements| one unit every menu iteration, until it gets to 0. Zero means no scroll.
                                    // It is set by a scroll twist at a value proportional with the motion's "strength", up to MENU_MAXSCROLL.
                                    // It goes through a transfer function to get scrollTickPeriod. While scrolling, a tick is inserted every scrollTickPeriod [ms]
    uint32_t timeNow;               // current millis
    bool delayedNav;                // true since SetActions, until still for STABLETIME [ms]
    int16_t scrollTickPeriod;       // Sets when to insert ticks while scrolling
    uint32_t lastTickTime;          // Time when a tick was last inserted
    uint32_t lastScrollUpdateTime; // Time when the scrolling rate was last updated
    uint32_t lastScrollTime;        // Keep track of scroll action timing, to allow a dead time of MENU_SCROLLPAUSE milliseconds after a scroll action
    uint32_t lastEndTick;           // Time when 'tickEND' was played last time 
    scalableTF<float, float> scrollTF;  // scroll transfer function. 
    #define MENU_MAXSCROLLTPS   60             // Maximum rate of ticks per second inserted by scroll
    #define MENU_MINSCROLLTPS   3               // Minimum rate of ticks per second inserted by scroll
    float TFdata[8] = {         0,                         0.2,           0.8,            1,         // X = normalized time remaining (decreasing to 0 while scroll progresses)
                        TPS2PER(MENU_MINSCROLLTPS),    TPS2PER(10),   TPS2PER(40),   TPS2PER(MENU_MAXSCROLLTPS)   };     // Y = scrollTickPeriod [ms], specified as TPS2PER(ticks_per_second)
        //  MENU_MINSCROLLTPS = 2                        MENU_MAXSCROLLTPS = 50



    RefPtr<BufferedWavPlayer> player1;  // main player for menu sounds 
    RefPtr<BufferedWavPlayer> player2;  // overlay player for skip hum

public:
    TTMenu (TYPE initialTick = 0) : Looper(1000*MENU_PERIOD) { 
        tick = initialTick; 
        stableTick = tick;
        // rolling = true;
        rolling = false;
        skipping = false;       // not skipping
        buttonState = false;
        scrollIndex = 0;       // not scrolling
        scrollTickPeriod = 0;   
        lastScrollTime = 0;   
        lastEndTick = 0;  
        minTick = std::numeric_limits<TYPE>::min();     
        maxTick = std::numeric_limits<TYPE>::max();
        // minTick = 0;  
        // maxTick = 200;
        if (scrollTF.Init(TFdata, NELEM(TFdata))) {
            // scrollTF.Rescale(0.5, 2);
            scrollTF.Start();                    
        }
        actions = 0;
        
        // emojiSounds.reset(); emojiSounds.SetPersistence(true);       // those are persistent effect, so we need to scan manually
        // emojiBackgrounds.reset(); emojiBackgrounds.SetPersistence(true); 
        // menuSounds.reset(); menuSounds.SetPersistence(true); 
        // Effect::ScanDirectory(EMOJISOUNDS_PATH);    // TODO: move this to prop 
        // Effect::ScanDirectory(MENUVOICES_PATH);

    }            

    const char* name() override { return "TTMenu"; } 
    
    void setLimits(bool rolls, TYPE minT, TYPE maxT) {
        rolling = rolls;
        minTick = minT; 
        if (tick < minT) tick = minT;
        maxTick = maxT;
        if (tick > maxT) tick = maxT;
    }


// Base class for menu actions. Inherit, override and SetActions() to enable the menu
class xMenuActions {
public:
    virtual ~xMenuActions() = default;
    virtual void Tick(TYPE currentValue) {}
    virtual void StableState(TYPE currentValue) {}
    virtual void Ok(TYPE currentValue)  = 0;
    virtual void Cancel(TYPE currentValue) = 0;
    
};



private:    
    xMenuActions* actions;      // Defines behaviour. Menu disabled if actions not defined

public:
bool IsActive() { return actions; }

// Assign actions and enable menu
bool SetActions(xMenuActions* actions_, TYPE initialValue)  {     
    // 1. Check
    if (!actions_) return false;             // nothing to assign
    if (initialValue < minTick) initialValue = minTick; // bring initial value in valid range
	if (initialValue > maxTick) initialValue = maxTick; 
    if (actions) ResetActions();             // unassigned previous actions

    // 2. Assign players
    if (player1) {
        player1->Stop();
        player1->CloseFiles();
    }
    else {
        player1 = GetFreeWavPlayer();
        player1->set_fade_time(0.003);
    }
    
    if (player2) {
        player2->Stop();
        player2->CloseFiles();
    }
    else {
        player2 = GetFreeWavPlayer();
        player2->set_fade_time(0.003);
    }     

    if (!player2) 
        if (player1) STDOUT.println("TTMenu not fully functional - WAV player 2 not available"); // we'll have to manage without the background player
        else  { 
            STDOUT.println("Cannot start TTMenu - WAV player 1 not available");
            return false;       // cannot assign player
        }

    // 3. Assign actions and set value
    actions = actions_;     // takes over events
    tick = initialValue;
    stableTick = tick;
    delayedNav = true;
    lastTickTime = millis();        
    STDOUT.println ("Menu started.");
    return true;
}

// Un-assign actions and disable menu
void ResetActions() {
    // 1. Release WAV players
    if (player2) {
        if (player2->isPlaying()) player2->FadeAndStop();
        player2.Free();
    }
    if (player1) {
        if (player1->isPlaying()) player1->FadeAndStop();
        player1.Free();
    }
    // 2. Release actions
    actions = 0;        // releases events
    STDOUT.println ("Menu ended.");
}


private:    
// NAV HELPERS:
    // Initiate scrolling
    void StartScroll(uint32_t strength) {
        if (!player1) return; // won't scrolling without sound
        scrollIndex = (strength - MENU_STARTSCROLL);            // alwasys positive, peak is unsigned
        if (scrollIndex > MENU_MAXSCROLL) scrollIndex = MENU_MAXSCROLL;   // saturate
        // STDOUT.print("[TTMenu.StartScroll] initial, un-processed scrollIndex = "); STDOUT.println(scrollIndex);

        // 1. Set TF's output range
        uint32_t tmp = MENU_MAXSCROLL * TPS2PER(MENU_MAXSCROLLTPS);
        tmp /= scrollIndex;
        scrollTF.SetYrange(TPS2PER(MENU_MINSCROLLTPS), tmp);

        // 2. Set TF's input range and scale scrollIndex
        tmp = scrollIndex * MENU_MAXSCROLLTIME;
        tmp /= (MENU_MAXSCROLL * MENU_PERIOD);  // = scrollIndex rescaled to the desired time scale (keep it for later)
        scrollTF.SetXrange(0, tmp);     
        scrollTickPeriod = scrollTF.Get(scrollIndex);     // initialize tick period     

        // 3. Update scrollIndex
        scrollIndex = tmp;
        if (fusor.theta()>0) {
            if (InsertTick(1, SILENT_TICK))         // insert first scrolling tick (silent)
                player1->PlayEffect(&emojiSounds, emojiSounds_id::scrollUP, 2*MENU_TPSREFRESHTIME);  // start playing ticks (repeat rate will be updated after MENU_TPSREFRESHTIME [ms])
            else {  // already at the end so won't start scrolling
                player1->PlayEffect(&emojiSounds, emojiSounds_id::tickEND);  // play end sound a single time
                scrollIndex = 0;    // disable scrolling
            }
        }
        else {  
            if (InsertTick(-1, SILENT_TICK))  {           // insert first scrolling tick (silent)
                player1->PlayEffect(&emojiSounds, emojiSounds_id::scrollDN, 2*MENU_TPSREFRESHTIME);  // start playing ticks (repeat rate will be updated after MENU_TPSREFRESHTIME [ms])
                scrollIndex = - scrollIndex;          // add sign. scrollIndex now ranges [-MENU_MAXSCROLLTIME, MENU_MAXSCROLLTIME]
            }
            else {  // already at the end so won't start scrolling
                player1->PlayEffect(&emojiSounds, emojiSounds_id::tickEND);  // play end sound a single time
                scrollIndex = 0;    // disable scrolling
            }
        }
        lastScrollTime = timeNow;           // suspend scroll control for MENU_SCROLLPAUSE [ms]
        lastTickTime = timeNow;             // keep track of tick insertion time 
        lastScrollUpdateTime = timeNow;
        // STDOUT.print("[TTMenu.StartScroll]  After processing: scrollIndex = "); STDOUT.print(scrollIndex); STDOUT.print(", scrollTickPeriod = "); STDOUT.println(scrollTickPeriod);

    }

    // Increment tick if sign > 0. Decrement tick if sign < 0. Returns false if at end (tick not changed)
    bool InsertTick(int16_t sign, bool silent=false) {
        int soundToPlay = 0;
        bool retVal = true;         // gets false if at end so no tick was added
        if (sign > 0)  {   // up tick
            // insert tick
            soundToPlay = emojiSounds_id::tickUP;   // assume normal tick
            if (tick == maxTick-1 && !rolling) { // just hit the limit. 
                tick++;
                // soundToPlay = emojiSounds_id::endUP;   // end tick
                // STDOUT.print("Tick hit MAX limit = "); STDOUT.println(maxTick); // STDOUT.print(" @ "); STDOUT.println(timeNow);   
                // return;  
            }
            else if (tick == maxTick) {
                // if(!rolling) return;       // do nothing if at limit and not rolling
                if(!rolling) { 
                    retVal = false;             // signal no tick was inserted
                    if (timeNow - lastEndTick >= MENU_ENDREMINDER) {
                        soundToPlay = emojiSounds_id::tickEND;   // do not change tick if at limit and not rolling, just play end sound            
                        lastEndTick = timeNow;
                        // STDOUT.println("Tick already at MAX, will not increment");
                    }
                    else soundToPlay = 0;       // don't play end tick too often
                }
                else tick = minTick;            // roll over
            } 
            else { 
                tick++;
                // STDOUT.print("Tick++ = "); STDOUT.println(tick); 
            }

            // insert sound
            if (!silent && soundToPlay) {   // scrolls are played by repeat, so insert tick should be silent
                player1->PlayEffect(&emojiSounds, soundToPlay);  // play at full volume if player2 also playing (effect's volume defaults to 0.5, so 2*0.5=1)
            }

        }
        if (sign < 0) {   // down tick
            // insert tick
            soundToPlay = emojiSounds_id::tickDN;   // assume normal tick
            if (tick == minTick+1 && !rolling) { // just hit the limit. 
                tick--;
                // STDOUT.print("Tick hit MIN limit = "); STDOUT.println(minTick); 
                // soundToPlay = emojiSounds_id::endDN;   // end tick
                // return;    
            }
            else if (tick == minTick) {
                // if(!rolling) return;       // do nothing if at limit and not rolling
                if(!rolling) { 
                    retVal = false;             // signal no tick was inserted
                    if (timeNow - lastEndTick >= MENU_ENDREMINDER) {
                        soundToPlay = emojiSounds_id::tickEND;   // do not change tick if at limit and not rolling, just play end sound            
                        lastEndTick = timeNow;
                        // STDOUT.println("Tick already at MIN, will not decrement");
                    }
                    else soundToPlay = 0;       // don't play end tick too often
                }
                else tick = maxTick;            // roll over
            } 
            else { 
                tick--;
                // STDOUT.print("Tick-- = "); STDOUT.println(tick); 
            }
            // insert sound
            if (!silent && soundToPlay) {   // scrolls are played by repeat, so insert tick should be silent
                player1->PlayEffect(&emojiSounds, soundToPlay, 0, 1);  
            }

        }
    if (retVal) {
        lastTickTime = timeNow;
        // actions->currentValue = tick;
        actions->Tick(tick);
    }
    return retVal;
    
    }

    

public:

    void Setup() override  {    // temp, for debug only

    }


    // Menu navigation
    void Loop() override  {// __attribute__((optimize("Og"))) {
        #if defined(X_BROADCAST) && defined(BROADCAST_THETA)
            int32_t thetapeak = fusor.thetaPeak();  // debug only
        #endif
        if (!actions) return;       // won't work without defined behaviour
        if (!player1) return;       // won't work without the main sounds player (will work without the background player though...)
        timeNow = millis();   

        // 1. Delay navigation until still for STABLETIME [ms]
        if (delayedNav) {
            if (abs(fusor.theta()) >= MENU_TICKTH) lastTickTime = timeNow;      // while in delayed nav, lastTickTime holds the time of the last registered motion
            if (timeNow - lastTickTime >= MENU_STABLETIME) delayedNav = false;
        }
        if (delayedNav) return; 

        // STDOUT.print(".");
        // STDOUT.print(fusor.theta()); STDOUT.print("-");
        // 1. Check stable state
        if (stableTick != tick)     // tick changed since last reported
            if (timeNow - lastTickTime >= MENU_STABLETIME) {   // enough time has passed to consider latest tick stable
                stableTick = tick;
                // actions->currentValue = tick;
                actions->StableState(tick);
            }

        // 2.  Maintain scrolling, if active (including remote at fixed rate)
        if (scrollIndex) { // scroll active!
            // 2.1 Insert tick if needed
            if (scrollTickPeriod && timeNow - lastTickTime >= scrollTickPeriod) {
                bool notAtEnd = InsertTick(scrollIndex, SILENT_TICK);       // here scrollIndex just sets the sign
                if (notAtEnd) {
                    lastTickTime = timeNow;
                    if (scrollTickPeriod) scrollTickPeriod = scrollTF.Get(abs(scrollIndex));     // update tick period
                    if (timeNow - lastScrollUpdateTime > MENU_TPSREFRESHTIME ) {
                        player1->ChangeRepeatTime(scrollTickPeriod);
                        lastScrollUpdateTime = timeNow;
                    }
                    // STDOUT.print(timeNow); STDOUT.print(":"); STDOUT.print(scrollIndex); STDOUT.print("-"); STDOUT.print(scrollTickPeriod); STDOUT.print("="); STDOUT.println(tick);
                }
                else { // reached end while scrolling
                    player1->ChangeRepeatTime(0);       // stop repeating ticks
                    // player1->Stop();        
                    player1->PlayEffect(&emojiSounds, emojiSounds_id::tickEND);      // play end emoji a single time
                    scrollIndex = 0;                // stop scrolling
                    STDOUT.println("Reached END while scrolling");
                }
            }
            // 2.2 Advance scrolling
            if (scrollIndex) {
                if (scrollIndex>0) scrollIndex--;                     // keep scrolling
                else scrollIndex++; 
                if (!scrollIndex) { 
                    player1->ChangeRepeatTime(0);           // stop repeating ticks
                    STDOUT.print("END scrolling @ "); STDOUT.print(timeNow);  STDOUT.print(", tick = ");  STDOUT.println(tick); 
                }
            }
        }     
    // #endif // X_MENUTEST

        // 3. Check skipping
        if (buttonState && !skipping) 
            if (abs(fusor.theta()) >= MENU_SKIPTH) {
                skipping = true;
               // STDOUT.println("[TTMenu.Event] start skipping"); 
                // PlaySoundEffect(player2, &emojiBackgrounds, 1, 0, 0.2);   // start skiphum in loop
                // float vol = 0.5;
                // if (IsOn()) vol = 1.5;
                if(player2) player2->PlayEffect(&emojiBackgrounds, emojiBackgrounds_id::skip, 1, 0.5);  // play the skiphum in loop

            }
        
        // If theta accumulated enough to register a tick (except if skipping):     
        // 4. Check scroll controls
        if ((abs(fusor.theta()) >= MENU_TICKTH) && !skipping) {     
            if (scrollIndex) {   // scroll active
                // 4.1. Check stop scroll
                if ((fusor.thetaPeak() > MENU_STOPSCROLL) && signof<int32_t>(fusor.theta()) != (signof<int16_t>(scrollIndex)) ) {
                    scrollIndex = 0;                   // stop scrolling
                    lastScrollTime = timeNow;           // suspend scroll control for MENU_SCROLLPAUSE [ms]
                    scrollTickPeriod = 0;
                    if (fusor.theta() > 0)              
                        player1->PlayEffect(&emojiSounds,  emojiSounds_id::stopDN);     // stoping DOWN scrolling with up motion
                    else
                        player1->PlayEffect(&emojiSounds,  emojiSounds_id::stopUP);     // stoping UP scrolling with down motion
                    STDOUT.print("STOP scrolling. @ "); STDOUT.print(timeNow); STDOUT.print(", tick = ");  STDOUT.println(tick);  
                    fusor.SetTheta(0);   
                }
                // 4.2 Check restart scroll  
                if ((fusor.thetaPeak() > MENU_STARTSCROLL) && (signof<int32_t>(fusor.theta()) == signof<int16_t>(scrollIndex)) && (timeNow-lastScrollTime >= MENU_SCROLLPAUSE)) {
                    // Valid start scroll conditions:
                    if (fusor.thetaPeak() - MENU_STARTSCROLL > abs(scrollIndex)) {
                        StartScroll(fusor.thetaPeak());
                        STDOUT.print("RESTART scrolling @ "); STDOUT.print(timeNow);  STDOUT.print(", strength = ");  STDOUT.print(fusor.thetaPeak());   STDOUT.print(", tick = ");  STDOUT.println(tick);   
                    }   
                    else { // valid start scroll but would result in smaller scrollIndex than current, so we'll disregard it
                        lastScrollTime = timeNow;           // suspend scroll control for MENU_SCROLLPAUSE [ms]
                        STDOUT.print("-> ignoring valid scroll start, strength = "); STDOUT.print(fusor.thetaPeak());  STDOUT.print(" @ "); STDOUT.println(timeNow);     
                    }
                    fusor.SetTheta(0);   
                }          
                return;                
            }
            else { // not scrolling
                // 4.3 Check start scroll
                if (fusor.thetaPeak() > MENU_STARTSCROLL && timeNow-lastScrollTime >= MENU_SCROLLPAUSE) {
                    StartScroll(fusor.thetaPeak());
                    STDOUT.print("START scrolling @ "); STDOUT.print(timeNow);  STDOUT.print(", strength = ");  STDOUT.print(fusor.thetaPeak());   STDOUT.print(", tick = ");  STDOUT.println(tick);    
                    return;
                }
                // 4. Tick-by-tick navigation
                if (fusor.theta() > 0) InsertTick(1);   // Increment tick            
                else InsertTick(-1);                    // Decrement tick
                fusor.SetTheta(0);                      // Reset theta once we registered a tick
            }
        }
        
    #if defined(X_BROADCAST) && defined(BROADCAST_THETA)
        struct {
            int16_t up1 = 0;
            int16_t up2 = 0;
            int16_t up3 = 0;
            int16_t down1 = 0;
            int16_t down2 = 0;
            int16_t down3 = 0;
        } ch1_broadcast;
        ch1_broadcast.up1 = (int16_t)(fusor.gyro().x);
        ch1_broadcast.up2 = (int16_t)(fusor.theta()); 
        ch1_broadcast.up3 = (int16_t)(thetapeak);         
        ch1_broadcast.down1 = (int16_t)(tick);
        ch1_broadcast.down2 = (int16_t)(scrollIndex);     
        ch1_broadcast.down3 = (int16_t)(scrollTickPeriod);
        STDOUT.Broadcast(1, &ch1_broadcast, sizeof(ch1_broadcast));
    #endif // X_BROADCAST

    }

  bool Event(enum BUTTON button, EVENT event) {
    if (!actions) 
        return false;       // release events, menu is disabled without actions. Button actions are still possible without player1 though...

    if ((button & BUTTON::BUTTON_POWER)) { // button changed
        int cnt = 0;
        uint32_t eventID;
        if (event >= EVENT_FIRST_PRESSED && event <= EVENT_FOURTH_CLICK_LONG) {
            cnt = (event - EVENT_PRESSED) / (EVENT_SECOND_PRESSED - EVENT_FIRST_PRESSED);
            eventID = event - (EVENT_SECOND_PRESSED - EVENT_FIRST_PRESSED) * cnt;   
        }
        switch (eventID) {
            case EVENT_PRESSED:     // update buttonState
                buttonState = true; 
            break;

            case EVENT_RELEASED:    // stop skipping
                buttonState = false;
                if (skipping) {
                    skipping = false;
                    fusor.SetTheta(0);   // Reset theta when done skipping
                    if(player2)player2->FadeAndStop(); // stop skiphum with long fade (the only kind that actually works)
                    //STDOUT.println("[TTMenu.Event] stop skipping"); 
                }
            break;

            case EVENT_CLICK_SHORT: // confirm
                // STDOUT.println("[TTMenu.Event] CONFIRMED"); 
                // actions->currentValue = tick;
                if(player2) {
                    player2->Stop();
                    player2->CloseFiles();
                    player2.Free();
                }

                actions->Ok(tick);
                // tick = maxTick;     // debug only
            break;

            case EVENT_HELD_LONG:   // cancel
                if (!skipping) {
                    // tick = minTick;         // debug only
                    // STDOUT.println("[TTMenu.Event] CANCELED"); 
                    // actions->currentValue = tick;
                    if(player2) {
                        player2->Stop();
                        player2->CloseFiles();
                        player2.Free();
                    }
                    actions->Cancel(tick);
                }
            break;
        }


    }
    return true;
  }



 #if defined(X_MENUTEST) && defined(ENABLE_DEVELOPER_MODE)

    bool Parse(const char* cmd, const char* arg)  { 
        if (!strcmp(cmd, "tick")) {
            if (!strcmp(arg, "down")) InsertTick(-1);
            else InsertTick(1);
            return true;
        }

        if (!strcmp(cmd, "scroll")) {
            int16_t numVal = atoi(arg);    // numerical value, after character identifier and comma        
            if (!numVal) return true;
            if (numVal > MENU_MAXSCROLL) numVal = MENU_MAXSCROLL;     
            if (numVal < -MENU_MAXSCROLL) numVal = -MENU_MAXSCROLL;    
            if (numVal>0) fusor.SetTheta(MENU_TICKTH);
            else fusor.SetTheta(-MENU_TICKTH);
            StartScroll(MENU_STARTSCROLL+abs(numVal));

            STDOUT.print("START scrolling, scrollIndex = "); STDOUT.print(scrollIndex);  STDOUT.print(" @ "); STDOUT.println(millis());     
            return true;
        }

        if (!strcmp(cmd, "skip")) {
            if (!skipping) {     // simulate start skip and let Loop() do the rest
                buttonState = true;    
                fusor.SetTheta(MENU_SKIPTH + 1);
            }
            else {   // simulate stop skip and let Loop() do the rest
                Event(BUTTON_POWER, EVENT_FIRST_RELEASED);
            }
            Loop();
            return true;
        }
        
        if (!strcmp(cmd, "emoji")) {
            if (!arg) { // stop both players
                player1->FadeAndStop();
                if(player2)player2->FadeAndStop();
            }
            else {  // start one of the players
                MountSDCard();
                EnableAmplifier();
                // uint16_t repeat = atoi(arg);
                // uint16_t soundID = atoi(arg+2);
                char argh[20];
                strcpy(argh, arg);
                char* token = strtok(argh, ",");
                int16_t soundID = atoi(token);
                token = strtok(NULL, ",");
                uint16_t repeat = atoi(token);
                if (repeat > 10000) repeat = 10000;
                if (soundID>0)  { // play emoji
                    STDOUT.print("Play emoji sound #"); STDOUT.print(soundID); STDOUT.print(" on player1, repeat = "); STDOUT.print(repeat);
                    // PlaySoundEffect(player1, &emojiSounds, soundID, repeat);
                    if (player1->PlayEffect(&emojiSounds, soundID, repeat)) STDOUT.println(".");
                    else STDOUT.println(" - fail, could not find file. ");

                }
                else { // play emoji background
                    soundID = -soundID;         // it was negative just to signal background
                    STDOUT.print("Play emoji background #"); STDOUT.print(soundID); STDOUT.print(" on player2, repeat = "); STDOUT.print(repeat);
                    // PlaySoundEffect(player2, &emojiBackgrounds, 1, repeat, 0.2);
                    if(player2){
                    if (player2->PlayEffect(&emojiBackgrounds, soundID, repeat, 0.2)) STDOUT.println(".");
                    else STDOUT.println(" - fail, could not find file. ");
                    } else {
                        STDOUT.println(" - palyer 2 not available ");
                    }

                }
            }
            return true;           
        }



        if (!strcmp(cmd, "voice")) {
            if (!arg) { // stop both players
                player1->FadeAndStop();
            }
            else {  // start one of the players
                MountSDCard();
                EnableAmplifier();
                // uint16_t repeat = false;
                // uint16_t soundID = atoi(arg+2);
                char argh[20];
                strcpy(argh, arg);
                char* token = strtok(argh, ",");
                uint16_t soundID = atoi(token);
                token = strtok(NULL, ",");
                uint16_t repeat = atoi(token);   
                if (repeat > 10000) repeat = 10000;
                STDOUT.print("Play menu sound #"); STDOUT.print(soundID); STDOUT.print(" on player1, repeat = "); STDOUT.print(repeat);            
                if (player1->PlayEffect(&menuSounds, soundID, repeat)) STDOUT.println(".");
                else STDOUT.println(" - fail, could not find file. ");


            }
            return true;           
        }

        return false;
    }



    void Help() { 
        #if defined(COMMANDS_HELP) 
        STDOUT.println(" tick up/dn - trigger a tick");
        STDOUT.print(" scroll <strength> - trigger scroll at strength [-"); STDOUT.print(MENU_MAXSCROLL); STDOUT.print("..."); STDOUT.print(MENU_MAXSCROLL); STDOUT.println("]");
        STDOUT.println(" skip - toggles skipping");
        STDOUT.print(" emoji [<id>, <loop>] - stops/[plays] an emoji sound/background identified by 'id' = -"); STDOUT.print(emojiBackgrounds_id::backMAX);
                                                    STDOUT.print("..."); STDOUT.print(emojiSounds_id::emojiMAX); STDOUT.println(".'loop' = 0...10000 [ms]");
        STDOUT.print(" voice [<id>, <loop>] - stops/[plays] a voice sound identified by 'id' = 1..."); STDOUT.print(menuVoices_id::menuMAX); STDOUT.println(". 'loop' = 0...10000 [ms]");
        #endif

    }

#endif // X_MENUTEST

// 




};


// class testMenu_t : public TTMenu<uint16_t>::xMenuActions {
//     void Tick() override {   STDOUT.print("[testMenu.Tick()] currentValue = "); STDOUT.println(currentValue); }
//     void StableState() override {    STDOUT.print("[testMenu.StableState()] currentValue = "); STDOUT.println(currentValue); }
//     void Ok() override {   STDOUT.print("[testMenu.Ok()] currentValue = "); STDOUT.println(currentValue);}
//     void Cancel() override {   STDOUT.print("[testMenu.Cancel()] currentValue = "); STDOUT.println(currentValue);}
// } testMenu;         // test menu object



#endif // TTMENU_H



