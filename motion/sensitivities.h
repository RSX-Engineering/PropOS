#ifndef SENSITIVITIES_H
#define SENSITIVITIES_H

#include "../common/Utils.h"


// Base class for specific sensitivities
struct Sensitivity {
    static uint8_t master;                   // master sensitivity: adds/remove up to 25% of range to user setting. Applied by Set().
    uint8_t userSetting;                     // user sets a single 0...255 value for each motion sensitivity. This does not get altered by master sensitivity, only what Set() applies
    virtual void Set() = 0;                   // overload this to apply user setting to actual motion detection parameters. 'userSetting' must be previously defined

    // returns sensitivity after applying master to user setting
    uint8_t ApplyMaster() {  
        // uint16_t appliedSetting = userSetting + master/2 - 64;  // +/- 25%
        uint16_t appliedSetting = userSetting + master/4 - 32; // +/- 12.5%
        if (appliedSetting < 1) appliedSetting = 1;
        if (appliedSetting > 255) appliedSetting = 255;
        return (uint8_t) appliedSetting;
    }

};

// Clash sensitivity
#define CLASH_DEFAULT 128        // default clash sensitivity
#define CLASH_MIN_TH  18 // 20         // clash threshold at minimum sensitivity
#define CLASH_MAX_TH  2          // clash threshold at maximum sensitivity
#define CLASH_OFF_TH  1000       // clash threshold when detection is off
struct  ClashSensitivity : public Sensitivity {
    float clashThreshold;   // CLASH_THRESHOLD_G points here
    ClashSensitivity(uint8_t default_sens = CLASH_DEFAULT) { userSetting = default_sens; }    
    void Set() override {
      if (!userSetting) clashThreshold = CLASH_OFF_TH;    // disabled
      else {      
        uint8_t appliedSetting = ApplyMaster();
        if (!appliedSetting) appliedSetting = 1;
        RangeStats<uint8_t, 0> userRange(1, 255);
        clashThreshold = userRange.Rescale(appliedSetting, CLASH_MIN_TH, CLASH_MAX_TH);
      }
    }
};


// Swing sensitivity. Call smooth_swing_config.ApplySensitivity() to apply!
#define SWING_DEFAULT   150 //128     // default swing sensitivity
#define SWING_MIN_SWS   1.5 // 1.8     // multiplier for 'SwingSensitivity' at minimum sensitivity
#define SWING_MAX_SWS   0.5 // 0.2     // multiplier for 'SwingSensitivity' at maximum sensitivity
#define SWING_OFF_SWS   1000    // multiplier for 'SwingSensitivity' when detection is off
#define SWING_MIN_SHRP  1.3     // multiplier for 'SwingSharpness' at minimum sensitivity
#define SWING_MAX_SHRP  0.7     // multiplier for 'SwingSharpness' at maximum sensitivity
#define SWING_MIN_VOL  0.9 // 0.7      // multiplier for 'MaxSwingVolume' at minimum sensitivity
#define SWING_MAX_VOL  1.2 // 1.3      // multiplier for 'MaxSwingVolume' at maximum sensitivity
#define SWING_MIN_ACCTH  2       // multiplier for 'AccentSwingSpeedThreshold' at minimum sensitivity
#define SWING_MAX_ACCTH  0.5       // multiplier for 'AccentSwingSpeedThreshold' at maximum sensitivity
struct  SwSensitivity : public Sensitivity {
    float SwingSensitivity_multiplier;
    float SwingSharpness_multiplier;
    float AccentSwingSpeedThreshold_multiplier;
    float MaxSwingVolume_multiplier;
    SwSensitivity(uint8_t default_sens = SWING_DEFAULT) { userSetting = default_sens; }    
    void Set() override {
      if (!userSetting) {
        SwingSensitivity_multiplier = SWING_OFF_SWS;    // disable smooth swing
        AccentSwingSpeedThreshold_multiplier = 0;       // disable accent swing
      }
      else {
        RangeStats<uint8_t, 0> userRange(1, 255);
        uint8_t appliedSetting = ApplyMaster();
        if (!appliedSetting) appliedSetting = 1;
        SwingSensitivity_multiplier = userRange.Rescale(appliedSetting, SWING_MIN_SWS, SWING_MAX_SWS);
        SwingSharpness_multiplier = userRange.Rescale(appliedSetting, SWING_MIN_SHRP, SWING_MAX_SHRP);
        MaxSwingVolume_multiplier = userRange.Rescale(appliedSetting, SWING_MIN_VOL, SWING_MAX_VOL);
        AccentSwingSpeedThreshold_multiplier = userRange.Rescale(appliedSetting, SWING_MIN_ACCTH, SWING_MAX_ACCTH);         
      }
    }
};
// Stab sensitivity
#define STAB_DEFAULT 128      // default stab sensitivity
#define STAB_MIN_TH  8 // 10  // 5        // stab threshold at minimum sensitivity
#define STAB_MAX_TH  2        // stab threshold at maximum sensitivity
#define STAB_OFF_TH  1000     // stab threshold when detection is off
#define STAB_MIN_DIR   2.3       // stab directionality at minimum sensitivity
#define STAB_MAX_DIR   1.7       // stab directionality at maximum sensitivity
struct  StabSensitivity : public Sensitivity {
    float threshold;            // stab threshold = minimum XYZ acceleration: 
    float dir;
    StabSensitivity(uint8_t default_sens = STAB_DEFAULT) { userSetting = default_sens; }    
    void Set() override {
      if (!userSetting) threshold = STAB_OFF_TH;    // disabled
      else {
        RangeStats<uint8_t, 0> userRange(1, 255);
        uint8_t appliedSetting = ApplyMaster();
        if (!appliedSetting) appliedSetting = 1;
        threshold = userRange.Rescale(appliedSetting, STAB_MIN_TH, STAB_MAX_TH);
        dir = userRange.Rescale(appliedSetting, STAB_MIN_DIR, STAB_MAX_DIR);
      }
    }
};


#define SHAKE_DEFAULT   160 // 128       // default shake sensitivity
#define SHAKE_MIN_TH    900 // 1000      // shake threshold at minimum sensitivity
#define SHAKE_MAX_TH    100       // shake threshold at maximum sensitivity
#define SHAKE_OFF_TH    10000     // shake threshold when detection is off
#define SHAKE_MIN_PER   70 // 50        // shake maximum period at minimum sensitivity
#define SHAKE_MAX_PER   200 // 300       // shake maximum period at maximum sensitivity
#define SHAKE_MIN_NPK   20        // shake number of peaks at minimum sensitivity
#define SHAKE_MAX_NPK   6 // 8         // shake number of peaks at maximum sensitivity
#define SHAKE_MIN_RST   150 // 100       // shake reset time at minimum sensitivity
#define SHAKE_MAX_RST   400       // shake reset time at maximum sensitivity
// Shake sensitivity
struct  ShakeSensitivity : public Sensitivity {
    float threshold;
    uint16_t maxPeriod;
    uint16_t nPeaks;
    uint16_t resetTime;
    ShakeSensitivity(uint8_t default_sens = SHAKE_DEFAULT) { userSetting = default_sens; }    
    void Set() override {
      if (!userSetting) threshold = SHAKE_OFF_TH;    // disabled
      else {
        RangeStats<uint8_t, 0> userRange(1, 255); 
        uint8_t appliedSetting = ApplyMaster();
        if (!appliedSetting) appliedSetting = 1;
        threshold = userRange.Rescale(appliedSetting, SHAKE_MIN_TH, SHAKE_MAX_TH);
        maxPeriod = userRange.Rescale(appliedSetting, SHAKE_MIN_PER, SHAKE_MAX_PER);
        nPeaks = userRange.Rescale(appliedSetting, SHAKE_MIN_NPK, SHAKE_MAX_NPK);
        resetTime = userRange.Rescale(appliedSetting, SHAKE_MIN_RST, SHAKE_MAX_RST);
      }         
    }
};



#define TAP_DEFAULT     128    // default tap sensitivity
#define TAP_MIN_TH      50     // tap threshold at minimum sensitivity
#define TAP_MAX_TH      20     // tap threshold at maximum sensitivity
#define TAP_OFF_TH      1000   // tap threshold when detection is off
// #define TAP_MIN_MINTIME 250 // 150    // minimum time between taps at minimum sensitivity
// #define TAP_MAX_MINTIME SHAKE_MAX_PER+1 // 50     // minimum time between taps at maximum sensitivity - 
#define TAP_MIN_MAXTIME 350 // 300 // 250    // maximum time between taps at minimum sensitivity
#define TAP_MAX_MAXTIME 600    // maximum time between taps at maximum sensitivity
// Double-tap sensitivity
struct  TapSensitivity: public Sensitivity {
    float threshold;
    // uint16_t minTime;
    uint16_t maxTime;
    TapSensitivity(uint8_t default_sens = TAP_DEFAULT) { userSetting = default_sens;}    
    void Set() override {
      if (!userSetting) threshold = TAP_OFF_TH;    // disabled
      else {
        RangeStats<uint8_t, 0> userRange(1, 255); 
        uint8_t appliedSetting = ApplyMaster();
        if (!appliedSetting) appliedSetting = 1;
        threshold = userRange.Rescale(appliedSetting, TAP_MIN_TH, TAP_MAX_TH);
        // minTime = userRange.Rescale(userSetting, TAP_MIN_MINTIME, TAP_MAX_MINTIME);
        maxTime = userRange.Rescale(appliedSetting, TAP_MIN_MAXTIME, TAP_MAX_MAXTIME);
      }      
    }

};



#define TWIST_DEFAULT       128   // default tap sensitivity
#define TWIST_MIN_TH        600   // twist threshold at minimum sensitivity
#define TWIST_MAX_TH        100   // twist threshold at maximum sensitivity
#define TWIST_MIN_DIR       6    // twist directionality at minimum sensitivity
#define TWIST_MAX_DIR       1     // twist directionality at maximum sensitivity
#define TWIST_MIN_MINTIME   120   // minimum stroke duration at minimum sensitivity
#define TWIST_MAX_MINTIME   50    // minimum stroke duration at maximum sensitivity
#define TWIST_MIN_MAXTIME   240   // maximum stroke duration at minimum sensitivity
#define TWIST_MAX_MAXTIME   400   // maximum stroke duration at maximum sensitivity
// Twist sensitivity
struct  TwistSensitivity : public Sensitivity {
    float threshold;
    float dir;
    uint16_t minTime;
    uint16_t maxTime;
    TwistSensitivity(uint8_t default_sens = TWIST_DEFAULT) { userSetting = default_sens; }    
    void Set() override {
      RangeStats<uint8_t, 0> userRange(0, 255);     // core functionality, cannot be disabled
        uint8_t appliedSetting = ApplyMaster();
      threshold = userRange.Rescale(appliedSetting, TWIST_MIN_TH, TWIST_MAX_TH);
      dir = userRange.Rescale(appliedSetting, TWIST_MIN_DIR, TWIST_MAX_DIR);
      minTime = userRange.Rescale(appliedSetting, TWIST_MIN_MINTIME, TWIST_MAX_MINTIME);
      maxTime = userRange.Rescale(appliedSetting, TWIST_MIN_MAXTIME, TWIST_MAX_MAXTIME);
    }
};


#define MNU_DEFAULT     140     // default menu sensitivity
#define MNU_MIN_THTH    60 // 100 // 200     // theta threshold at minimum sensitivity
#define MNU_MAX_THTH    40 // 50      // theta threshold at maximum sensitivity
#define MNU_MIN_TKTH    50 // 70 // 80      // tick threshold at minimum sensitivity
#define MNU_MAX_TKTH    30 // 20 // 30      // tick threshold at maximum sensitivity
#define MNU_MIN_STARTS  1100 // 1200    // start scroll at minimum sensitivity
#define MNU_MAX_STARTS  400     // start scroll at maximum sensitivity
#define MNU_MIN_MAXS    1400 // 1600    // max scroll at minimum sensitivity
#define MNU_MAX_MAXS    800     // max scroll at maximum sensitivity
#define MNU_MIN_STABLE  400     // stable time at minimum sensitivity
#define MNU_MAX_STABLE  800     // stable time at maximum sensitivity
// Menu navigation sensitivity
struct  MenuSensitivity : public Sensitivity {
    float thetaThreshold;
    float tickThreshold;
    uint16_t startScroll;
    uint16_t maxScroll;
    uint16_t stableTime;
    MenuSensitivity(uint8_t default_sens = MNU_DEFAULT) { userSetting = default_sens; }    

    void Set() override {
      RangeStats<uint8_t, 0> userRange(0, 255);     // core functionality, cannot be disabled
      uint8_t appliedSetting = ApplyMaster();
      thetaThreshold = userRange.Rescale(appliedSetting, MNU_MIN_THTH, MNU_MAX_THTH);
      tickThreshold = userRange.Rescale(appliedSetting, MNU_MIN_TKTH, MNU_MAX_TKTH);
      startScroll = userRange.Rescale(appliedSetting, MNU_MIN_STARTS, MNU_MAX_STARTS);
      maxScroll = userRange.Rescale(appliedSetting, MNU_MIN_MAXS, MNU_MAX_MAXS);
      stableTime = userRange.Rescale(appliedSetting, MNU_MIN_STABLE, MNU_MAX_STABLE);
    }
};

#endif // SENSITIVITIES_H