/********************************************************************
 *  STYLES LIBRARY                                                  *
 *  (C) Marius RANGU @ RSX Engineering                              *
 *  fileversion: v1.0 @ 2022/10;    license: GPL3                   *
 ********************************************************************
 
 ********************************************************************/

#ifndef STYLE_LIB__H
#define STYLE_LIB__H


#include <vector>
#include "style_ptr.h"
#include "blade_style.h"

 

class StyleDescriptor {
public:    
    StyleAllocator stylePtr;    // get crazy here!
    const char* name;           // up to 15 characters for CODv0.2
    StyleHeart good4;             // flags for types of usage the style is appropriate for
    
    // default constructor
    StyleDescriptor() { 
        stylePtr=0; 
        name = "none"; 
        good4 = _4nothing; }    

    // constructor to allow vector initialization
    StyleDescriptor(StyleAllocator styleptr, const char* name_, StyleHeart goodFor) {
        stylePtr = styleptr;
        name = name_;       // up to 15 characters for CODv0.2
        good4 = goodFor;
    }
};

#define GOOD4(X) static_cast<StyleHeart>(X)
vector<StyleDescriptor> styles {  
 
	// STYLES FOR ANALOG BLADES
	// ------------------------
	
	// Simple 
	{   StyleNormalPtr<RED, WHITE, 300, 800>(),  
     "Preset Simple", 
      GOOD4(StyleHeart::_4analog) },    

	// Fast pulse
	{  StyleNormalPtr<Pulsing<Red,Rgb16<44108,0,0>,800>,White,300,800,White,Yellow>(),
     "Fast Pulse",
    GOOD4(StyleHeart::_4analog) },

	// Morph for analog (in-hilt LED) blades
	{   StylePtr<InOutHelper<Layers<TransitionLoop<BLUE,TrConcat<TrConcat<TrFade<1500>,RED,TrConcat<TrFade<1500>,RED,TrFade<1500>>>,BLUE,TrFade<1500>>>,SimpleClashL<Sequence<WHITE,LightPink,20,4,0b1010100000000000>,200,EFFECT_CLASH,Int<0>>,LockupL<AudioFlickerL<WHITE>,AudioFlickerL<Magenta>,Int<32767>,Int<32767>,Int<32767>>,BlastL<WHITE,200,1>>,300,800>>(),
		"Morph",
		GOOD4(StyleHeart::_4analog) },     
    
    // Pulse for analog (in-hilt LED) blades
	{  	StylePtr<InOutHelper<Layers<TransitionLoop<Tomato,TrConcat<TrFade<800>,OrangeRed,TrConcat<TrFade<500>,Red,TrFade<500>>>>,SimpleClashL<Sequence<White,LightPink,20,4,0b1010100000000000>,200,EFFECT_CLASH,Int<0>>,LockupL<AudioFlickerL<White>,AudioFlickerL<Magenta>,Int<32767>,Int<32767>,Int<32767>>,BlastL<White,200,1>>,300,800>>(),
		"Real Pulse",
		GOOD4(StyleHeart::_4analog) },
  
    // Shimmer for analog (in-hilt LED) blades
	{   StylePtr<InOutHelper<Layers<Strobe<Rgb<255,0,0>,Rgb<200,0,0>,16,20>,SimpleClashL<Sequence<WHITE,LightPink,20,4,0b1010100000000000>,200,EFFECT_CLASH,Int<0>>,LockupL<AudioFlickerL<WHITE>,AudioFlickerL<Rgb<255,127,127>>,Int<32767>,Int<32767>,Int<32767>>,BlastL<WHITE,200,1>>,300,800>>(),
		"Shimmer",
		GOOD4(StyleHeart::_4analog) },
  
		
	#if defined(SABERPROP) && SABERPROP_VERSION == 'Z'
    // AudioShimmer II  for analog (in-hilt LED) blades
        {   StylePtr<InOutHelper<Layers<AudioFlicker<Rgb<255,0,0>,Rgb<127,0,0>>,
      SimpleClashL<Sequence<White,LightPink,20,4,0b1010100000000000>,200,EFFECT_CLASH,Int<0>>,
      LockupL<AudioFlickerL<White>,AudioFlickerL<Rgb<255,100,100>>,Int<32767>,Int<32767>,Int<32767>>,
      BlastL<White,200,1>>,300,800>>(),
        //  "AudioShimmer II",
		"Audio Shimmer",
          GOOD4(StyleHeart::_4analog) }, 
	#else 
	    // AudioShimmer for analog white blade only (in-hilt LED) blades
	{   StylePtr<InOutHelper<Layers<AudioFlicker<Rgb<100,255,75>,Rgb<50,125,35>>,SimpleClashL<Sequence<White,LightPink,20,4,0b1010100000000000>,200,EFFECT_CLASH,Int<0>>,LockupL<AudioFlickerL<White>,AudioFlickerL<Rgb<255,100,100>>,Int<32767>,Int<32767>,Int<32767>>,BlastL<White,200,1>>,300,800>>(),
		"Audio Shimmer",
		GOOD4(StyleHeart::_4analog) },
	
	#endif  

	
	// STYLES FOR PIXEL BLADES
	// -----------------------
	
    // Gradient
	{    StyleNormalPtr<Gradient<RED, BLUE>, Gradient<CYAN, YELLOW>, 300, 800>(), 
        "Gradient",
        GOOD4(StyleHeart::_4pixel) },

    // UltraSaber
	{   StylePtr<Layers<RotateColorsX<Variation,Red>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Orange,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Rgb<255,200,0>,DarkOrange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>>>(), 
		"UltraSaber",
        GOOD4(StyleHeart::_4pixel) },
    
    // Rainbow Custom
	{   StyleRainbowPtr<300,800>(),
       "Rainbow",
        GOOD4(StyleHeart::_4pixel) },
    
    // Saw Blade Fett263
	{   StylePtr<Layers<Pulsing<Stripes<3000,-3000,RotateColorsX<Variation,Blue>,RotateColorsX<Variation,Rgb<0,0,60>>,RotateColorsX<Variation,Rgb<50,50,255>>,RotateColorsX<Variation,Rgb<0,0,128>>>,Stripes<3000,3000,RotateColorsX<Variation,Blue>,RotateColorsX<Variation,Rgb<0,0,60>>,RotateColorsX<Variation,Rgb<50,50,255>>,RotateColorsX<Variation,Rgb<0,0,128>>>,3000>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Red,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Red,Orange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrJoin<TrWipe<800>,TrWipeIn<800>>,TrJoin<TrWipe<1200>,TrWipeIn<1200>>,Black>>>(),
       "Saw Blade",
       GOOD4(StyleHeart::_4pixel) },
    
	// Power Surge By Fett263
	{   StylePtr<Layers<Layers<AudioFlicker<RotateColorsX<Variation,Rgb<90,65,0>>,RotateColorsX<Variation,Rgb<120,90,0>>>,TransitionLoopL<TrWaveX<HumpFlickerL<RotateColorsX<Variation,Orange>,40>,Int<250>,Int<100>,Int<200>,Int<0>>>,TransitionLoopL<TrWaveX<HumpFlickerL<RotateColorsX<Variation,Rgb<180,130,0>>,40>,Int<350>,Int<100>,Int<300>,Int<0>>>>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Red,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Red,Orange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>>>(),
       "Power Surge",
       GOOD4(StyleHeart::_4pixel) },
    
	// Hump Flicker By Fett263
	{   StylePtr<Layers<HumpFlicker<RotateColorsX<Variation,Rgb<95,0,210>>,RotateColorsX<Variation,Rgb<42,0,110>>,50>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Red,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Red,Orange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>>>(),
       "Hump Flicker",
       GOOD4(StyleHeart::_4pixel) },
    
        // Omni Color By Fett263
	{    StylePtr<Layers<Mix<Sin<Scale<TwistAngle<1>,Int<1>,Int<8>>>,ColorChange<TrInstant,Red>,Green,Blue,Red>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Orange,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Rgb<255,200,0>,DarkOrange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>>>(),
		"Omni Color",
		GOOD4(StyleHeart::_4analog | StyleHeart::_4pixel) },
    
	// Thunder Storm By Fett263
	{   StylePtr<Layers<TransitionLoop<RotateColorsX<Variation,Blue>,TrConcat<TrBoing<500,3>,Layers<Stripes<10000,100,RotateColorsX<Variation,Rgb<0,0,128>>,RotateColorsX<Variation,Rgb<0,0,30>>,RotateColorsX<Variation,Rgb<0,0,200>>>,AlphaL<Stripes<8000,-200,RotateColorsX<Variation,Blue>,RotateColorsX<Variation,Rgb<0,0,60>>>,Int<16384>>>,TrDelayX<Scale<SlowNoise<Int<3000>>,Int<100>,Int<2000>>>>>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,TransitionLoopL<TrConcat<TrWipeX<Scale<SlowNoise<Int<2500>>,Int<150>,Int<300>>>,StrobeL<RandomPerLEDFlickerL<Rgb<175,175,175>>,Int<50>,Int<125>>,TrDelayX<Scale<SlowNoise<Int<1200>>,Int<300>,Int<80>>>,AlphaL<White,Int<0>>,TrDelay<300>>>,AlphaL<White,SmoothStep<Scale<SlowNoise<Int<2000>>,Int<1000>,Int<4000>>,Int<-4000>>>,SparkleL<Black,100,600>,ResponsiveStabL<Red,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Red,Orange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrConcat<TrWipeIn<200>,RandomBlink<30000,White,Black>,TrWipeIn<200>,Mix<SmoothStep<Scale<SlowNoise<Int<30000>>,Int<1000>,Int<6000>>,Int<-3000>>,Black,White>,TrDelay<400>,Mix<SmoothStep<Int<4000>,Int<-3000>>,Black,White>,TrWipe<300>>,TrWipeIn<500>,Black>>>(),
		"Thunder Storm",  
		GOOD4(StyleHeart::_4pixel) },
    
	// Lava Lamp Tie-Dye by Fett263
	{   StylePtr<Layers<StripesX<Sin<Int<4>,Int<3000>,Int<6000>>,Scale<TwistAngle<>,Int<-50>,Int<-100>>,StripesX<Sin<Int<3>,Int<1000>,Int<3000>>,Scale<TwistAngle<>,Int<25>,Int<80>>,Pulsing<RotateColorsX<Sin<Int<10>>,Rgb<255,50,50>>,RotateColorsX<Sin<Int<12>>,Rgb<200,0,0>>,3000>,Mix<Sin<Int<2>>,RotateColorsX<Sin<Int<12>>,Rgb<200,0,0>>,RotateColorsX<Sin<Scale<TwistAngle<>,Int<4>,Int<0>>>,Rgb<10,0,0>>>>,RotateColorsX<Sin<Scale<TwistAngle<>,Int<4>,Int<0>>>,Rgb<60,0,0>>,Pulsing<RotateColorsX<Sin<Scale<TwistAngle<>,Int<4>,Int<0>>>,Rgb<50,0,0>>,StripesX<Sin<Int<2>,Int<2000>,Int<4000>>,Sin<Int<2>,Int<25>,Int<75>>,Mix<Sin<Int<4>>,RotateColorsX<Sin<Int<4>>,Red>,RotateColorsX<Sin<Scale<TwistAngle<>,Int<14>,Int<2>>>,Rgb<255,50,50>>>,RotateColorsX<Sin<Scale<TwistAngle<>,Int<4>,Int<0>>>,Rgb<96,0,0>>>,2000>,Pulsing<RotateColorsX<Sin<Scale<TwistAngle<>,Int<10>,Int<6>>>,Rgb<128,0,0>>,RotateColorsX<Sin<Scale<TwistAngle<>,Int<4>,Int<10>>>,Rgb<50,0,0>>,3000>>,AlphaL<StripesX<Sin<Int<8>,Int<3000>,Int<6000>>,Scale<TwistAngle<>,Int<60>,Int<140>>,StripesX<Sin<Int<6>,Int<1000>,Int<3000>>,Scale<TwistAngle<>,Int<-25>,Int<-80>>,Pulsing<Mix<Sin<Int<4>>,RotateColorsX<Sin<Int<8>>,Red>,RotateColorsX<Sin<Int<6>>,Rgb<255,50,50>>>,RotateColorsX<Sin<Scale<TwistAngle<>,Int<4>,Int<0>>>,Rgb<20,0,0>>,4000>,Mix<Sin<Int<2>>,RotateColorsX<Sin<Int<8>>,Rgb<255,50,50>>,Black>>,RotateColorsX<Sin<Int<12>>,Rgb<60,0,0>>,Pulsing<RotateColorsX<Sin<Int<4>>,Rgb<50,0,0>>,StripesX<Sin<Int<2>,Int<2000>,Int<4000>>,Sin<Int<2>,Int<-25>,Int<-75>>,Mix<Sin<Int<4>>,RotateColorsX<Sin<Int<5>>,Red>,RotateColorsX<Sin<Int<16>>,Rgb<255,50,50>>>,RotateColorsX<Sin<Int<10>>,Rgb<96,0,0>>>,2000>,Pulsing<RotateColorsX<Sin<Int<9>>,Rgb<128,0,0>>,Black,3000>>,Int<10000>>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Orange,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Rgb<255,200,0>,DarkOrange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>>>(),
		"Lava Lamp",
		GOOD4(StyleHeart::_4pixel) },
    
	// Split Blade By Fett263
	{   StylePtr<Layers<Mix<SmoothStep<Scale<TwistAngle<1>,Int<6000>,Int<24000>>,Int<10000>>,RotateColorsX<Variation,Red>,Stripes<5000,-1500,RotateColorsX<Variation,DarkOrange>,RotateColorsX<Variation,Rgb<60,17,0>>,RotateColorsX<Variation,Rgb<255,140,40>>,RotateColorsX<Variation,Rgb<128,34,0>>>>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Red,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Red,Orange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>>>(),
		"Split Blade",
		GOOD4(StyleHeart::_4pixel) },
  
	// Smoke Blade By Fett263
	{   StylePtr<Layers<StripesX<Sin<Int<12>,Int<3000>,Int<7000>>,Scale<SwingSpeed<100>,Int<75>,Int<125>>,StripesX<Sin<Int<10>,Int<1000>,Int<3000>>,Scale<SwingSpeed<100>,Int<75>,Int<100>>,Pulsing<RotateColorsX<Variation,Rgb<100,100,150>>,RotateColorsX<Variation,Rgb<5,5,20>>,1200>,Mix<SwingSpeed<200>,RotateColorsX<Variation,Rgb<45,45,70>>,Black>>,RotateColorsX<Variation,Rgb<20,20,30>>,Pulsing<RotateColorsX<Variation,Rgb<25,25,40>>,StripesX<Sin<Int<10>,Int<2000>,Int<3000>>,Sin<Int<10>,Int<75>,Int<100>>,RotateColorsX<Variation,Rgb<100,100,150>>,RotateColorsX<Variation,Rgb<40,40,60>>>,2000>,Pulsing<RotateColorsX<Variation,Rgb<50,50,75>>,RotateColorsX<Variation,Rgb<2,2,5>>,3000>>,AlphaL<StyleFire<RotateColorsX<Variation,Rgb<100,100,150>>,RotateColorsX<Variation,Rgb<1,1,2>>,0,1,FireConfig<10,2000,2>,FireConfig<10,2000,2>,FireConfig<10,2000,2>,FireConfig<0,0,25>>,Int<10000>>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Red,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Red,Orange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>,TransitionEffectL<TrConcat<TrBoing<3000,5>,AlphaL<BrownNoiseFlickerL<Rgb<120,120,165>,Int<100>>,Bump<Int<0>,Int<4000>>>,TrBoing<3000,3>,Black,TrBoing<1400,3>,AlphaL<BrownNoiseFlickerL<Rgb<120,120,165>,Int<200>>,Bump<Int<0>,Int<6000>>>,TrBoing<550,3>>,EFFECT_PREON>>>(),
		"Smoke Blade",
		GOOD4(StyleHeart::_4pixel) },

  // The rest won't fit with both diagnose and developer ON   
#if !defined(ENABLE_DEVELOPER_MODE) || !defined(ENABLE_DIAGNOSE_MODE)
       
	// Audio Flicker By Fett263
	{   StylePtr<Layers<AudioFlicker<RotateColorsX<Variation,Rgb<100,100,150>>,RotateColorsX<Variation,Rgb<50,50,75>>>,LockupTrL<Layers<AlphaL<AudioFlickerL<Rgb<255,240,80>>,Bump<Sin<Int<10>,Int<10000>,Int<20000>>>>,AlphaL<LemonChiffon,Bump<Sin<Int<10>,Int<10000>,Int<20000>>>>>,TrConcat<TrInstant,AlphaL<TransitionEffect<Rgb<255,180,50>,LemonChiffon,TrInstant,TrFade<200>,EFFECT_LOCKUP_BEGIN>,Bump<Sin<Int<10>,Int<10000>,Int<20000>>,Int<20000>>>,TrFade<400>>,TrConcat<TrInstant,AlphaL<Mix<SmoothStep<Sin<Int<10>,Int<10000>,Int<20000>>,Int<1000>>,Stripes<1500,2000,TransitionEffect<Moccasin,Rgb<255,150,0>,TrInstant,TrFade<200>,EFFECT_LOCKUP_BEGIN>,Rgb<100,100,150>>,Stripes<1500,-2500,TransitionEffect<Moccasin,Rgb<255,150,0>,TrInstant,TrFade<200>,EFFECT_LOCKUP_BEGIN>,Rgb<100,100,150>>>,Int<18000>>,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Red,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<Blue,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,LocalizedClashL<CYAN>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Red,Orange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>>>(),
		"Audio Flicker",
		GOOD4(StyleHeart::_4pixel) },      

    // Stable to Unstable Swing by Fett 263
	{   StylePtr<Layers<Mix<SwingSpeed<100>,AudioFlicker<RotateColorsX<Variation,Red>,RotateColorsX<Variation,Rgb<128,0,0>>>,StyleFire<BrownNoiseFlicker<RotateColorsX<Variation,Red>,RandomPerLEDFlicker<RotateColorsX<Variation,Rgb<25,0,0>>,RotateColorsX<Variation,Rgb<60,0,0>>>,300>,RotateColorsX<Variation,Rgb<80,0,0>>,0,6,FireConfig<10,1000,2>,FireConfig<10,1000,2>,FireConfig<10,1000,2>,FireConfig<10,1000,2>>>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Orange,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Rgb<255,200,0>,DarkOrange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>>>(),
		"Stable2Unstable",
		GOOD4(StyleHeart::_4pixel) },

    // Rolling Pulse by Fett263
	{   StylePtr<Layers<Stripes<5000,-1500,RotateColorsX<Variation,Green>,RotateColorsX<Variation,Rgb<0,60,0>>,RotateColorsX<Variation,Rgb<50,255,50>>,RotateColorsX<Variation,Rgb<0,128,0>>>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Red,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Red,Orange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrJoinR<TrWipe<500>,TrWipeIn<500>>,Black>>>(),
		"Rolling Pulse",
		GOOD4(StyleHeart::_4pixel) },
    
    // Surging Pulse By Fett263
	{  	StylePtr<Layers<Stripes<6000,-3000,RotateColorsX<Variation,Rgb<128,0,0>>,RotateColorsX<Variation,Red>,RotateColorsX<Variation,Rgb<60,0,0>>>,LockupTrL<Layers<AlphaL<AudioFlickerL<White>,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Scale<SwingSpeed<100>,Int<14000>,Int<18000>>>>,AlphaL<White,Bump<Scale<BladeAngle<>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,Int<10000>>>>,TrConcat<TrInstant,White,TrFade<400>>,TrConcat<TrInstant,White,TrFade<400>>,SaberBase::LOCKUP_NORMAL>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Orange,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Int<20000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Rgb<255,200,0>,DarkOrange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>>>(),
		"Surging Pulse",
		GOOD4(StyleHeart::_4pixel) },
   
	// Unstable by Fett263
	{   StylePtr<Layers<Stripes<2000,-2500,RotateColorsX<Variation,DeepSkyBlue>,RandomPerLEDFlicker<RotateColorsX<Variation,Rgb<0,26,60>>,Black>,BrownNoiseFlicker<RotateColorsX<Variation,DeepSkyBlue>,RotateColorsX<Variation,Rgb<0,13,30>>,200>,RandomPerLEDFlicker<RotateColorsX<Variation,Rgb<0,25,80>>,RotateColorsX<Variation,Rgb<0,13,30>>>>,TransitionLoopL<TrConcat<TrWaveX<RandomFlicker<RotateColorsX<Variation,DeepSkyBlue>,BrownNoiseFlicker<RotateColorsX<Variation,Rgb<0,25,80>>,Black,300>>,Int<400>,Int<100>,Int<200>,Int<0>>,AlphaL<Red,Int<0>>,TrDelayX<Scale<SlowNoise<Int<1500>>,Int<200>,Int<1200>>>>>,AlphaL<Stripes<2500,-3000,RotateColorsX<Variation,DeepSkyBlue>,RotateColorsX<Variation,Rgb<0,26,60>>,Pulsing<RotateColorsX<Variation,Rgb<0,13,30>>,Black,800>>,SwingSpeed<400>>,TransitionEffectL<TrConcat<TrInstant,BrownNoiseFlickerL<AlphaL<White,Int<16000>>,Int<50>>,TrSmoothFade<600>>,EFFECT_LOCKUP_END>,ResponsiveLockupL<Strobe<White,BrownNoiseFlicker<White,Red,300>,50,1>,TrConcat<TrInstant,White,TrFade<400>>,TrFade<100>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>,Scale<SwingSpeed<100>,Int<10000>,Int<14000>>>,ResponsiveLightningBlockL<Strobe<White,AudioFlicker<White,Blue>,50,1>,TrConcat<TrInstant,AlphaL<White,Bump<Int<12000>,Int<18000>>>,TrFade<200>>,TrConcat<TrInstant,HumpFlickerL<AlphaL<White,Int<16000>>,30>,TrSmoothFade<600>>>,ResponsiveStabL<Red,TrWipeIn<600>,TrWipe<600>>,ResponsiveBlastL<White,Int<400>,Scale<SwingSpeed<200>,Int<100>,Int<400>>,Int<400>>,ResponsiveClashL<White,TrInstant,TrFade<400>,Scale<BladeAngle<0,16000>,Int<4000>,Int<26000>>,Int<6000>>,LockupTrL<AlphaL<BrownNoiseFlickerL<White,Int<300>>,SmoothStep<Int<30000>,Int<5000>>>,TrWipeIn<400>,TrFade<300>,SaberBase::LOCKUP_DRAG>,LockupTrL<AlphaL<Mix<TwistAngle<>,Red,Orange>,SmoothStep<Int<28000>,Int<5000>>>,TrWipeIn<600>,TrFade<300>,SaberBase::LOCKUP_MELT>,InOutTrL<TrWipe<300>,TrWipeIn<500>,Black>>>(),
		"Anger",
		GOOD4(StyleHeart::_4pixel) },
#endif
  
};






// Get pointer to a style in the 'styles' vector - by name
StyleDescriptor* GetStyle(const char* styleName) {
    // STDOUT.print ("Finding style "); STDOUT.println(styleName);
    if (!styles.size()) return 0;       // no styles available
    if (!styleName) return 0;           // no style specified
    for (uint8_t i=0; i<styles.size(); i++) 
        if (!strcmp(styles[i].name, styleName)) { 
            // STDOUT.print(" - Found style "); STDOUT.print(styles[i].name); 
            // STDOUT.print(" on position "); STDOUT.println(i); 
            return styles.data()+i;
        }
    // STDOUT.print("Could not find style"); STDOUT.println(styleName);
    return 0;
}

// Get pointer to first style good for something
StyleDescriptor* GetDefaultStyle(StyleHeart good4_) {
    if (!styles.size()) return 0;
    for (uint8_t i=0; i<styles.size(); i++) 
        if (styles[i].good4 & good4_) { // 1st style good for is the default
            return styles.data()+i;
        }
    return 0;
}



#endif  // STYLE_LIB__H
