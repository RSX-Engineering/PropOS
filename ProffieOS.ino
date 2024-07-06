/*
 ProffieOS: Control software for lightsabers and other props.
 http://fredrik.hubbe.net/lightsaber/teensy_saber.html
 Copyright (c) 2016-2019 Fredrik Hubinette
 Additional copyright holders listed inline below.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// **************************************************************************
// *   ProffieOSx Fork by RSX Engineering. Licensed under GPL v3 as above.  *
// **************************************************************************



#ifdef PROFFIEBOARD_VERSION
    #define PROFFIEBOARD
#else 
    #define ULTRAPROFFIE
    #ifdef PROFFIE_ESP
        #define ULTRAPROFFIE_VERSION 'P'    
    #else // PROFFIE_ESP
      #if HW_LETTER4 == 4
        #define ULTRAPROFFIE_VERSION 'L'
      #else
        #define ULTRAPROFFIE_VERSION 'Z'
      #endif 
    #endif // PROFFIE_ESP
#endif // PROFFIEBOARD_VERSION


/* TODO: refactor hardware switches in board package (and remove defines above):
    * ProffieBoard v2: PROFFIEBOARD, PROFFIEBOARD_VERSION = 2
    * ProffieBoard v3: PROFFIEBOARD, PROFFIEBOARD_VERSION = 3
    * UltraProffie Zero: ULTRAPROFFIE, ULTRAPROFFIE_VERSION = 'Z'
    * UltraProffie Lite: ULTRAPROFFIE, ULTRAPROFFIE_VERSION = 'L'
    * UltraProffie (ESP): ULTRAPROFFIE, ULTRAPROFFIE_VERSION = 'P'
*/

#define CONFIG_FILE "config/board_config.h"

#define CONFIG_TOP
#include CONFIG_FILE
#undef CONFIG_TOP


//
// OVERVIEW
//
// Here explain some general code concepts to make it easier
// to understand the code below.
//
// Most things start with the ProbBase class. Depending on the
// configuration, this class is extended by the Saber class,
// the Detonator class, or some other class. The extended class
// is instantiated as "prop", and is responsible for handling
// button clicks, clashes, swings and other events. These events
// are then send to all registered SaberBase classes.
///
// Generally speaking, there are usually two registered SaberBase
// classes listening for events. One for sound and one for
// the blade. Sound and blade effects are generally executed
// separately by separate clases.
//
// Blades are generally handled by one of the child-classes of
// BladeBase. These classes know how many LEDs the current
// blade has, and how to set those LEDs to a given color, but
// they don't actually decide what the blade should look like.
// Instead they just call the current BladeStyle class and
// asks it to set the colors. The BladeStyle classes don't
// need to know what kind of blade is attached, although
// some combinations of BladeBases and BladeStyles just don't
// make any sense.
//
// Sounds are also abstracted. It starts with scanning a directory
// on the SD card for files that match known patterns of file names.
// The Effect class is responsible for keeping track of all numbered
// files that for a particular filename prefix.
//
// Once the directory has been scanned, we'll decide how to play
// sounds. In the past, there was one class for handling NEC style
// fonts and another for handling Plecter style fonts. However,
// both of those have now been merged into the HybridFont class
// which can do both. It is also capable of doing some mix and matching,
// so you can use a plecter style hum together with a NEC style
// swing if you so desire. The HybridFont class inherit from
// SaberBase and listen on on/off/clash/etc. events, just like
// BladeBase classes do.
//
// HybridFont tells the audio subsystem
// to trigger and mix sounds as aproperiate. The sound subsystem
// starts with an DMA channel which feeds data to a digital-to-analog
// converter. Once the data buffer is half-gone, and interrupt is
// triggered in the DAC class, which tries to fill it up by
// reading data from a int16_t AudioStream. Generally, that data
// stream is hooked up to the AudioDynamicMixer class. This
// class is responsible for taking multiple audio inputs,
// summing them up and then adjusting the volume to minimize
// clipping.

// TODO LIST:
//   stab detect/effect
//
// Audio work items:
//   select clash from force
//   stab effect
// Blade stuff
//    better clash
// Allow several blades to share power pins.


// You can get better SD card performance by
// activating the  USE_TEENSY3_OPTIMIZED_CODE define
// in SD.h in the teensy library, however, my sd card
// did not work with that define.


#ifdef PROFFIEBOARD
  #include "common/crc.h"
#endif

#ifdef ARDUINO_ARCH_ESP32   // ESP architecture
  #include "common/espSTCRC.h"
#endif

#include <Arduino.h>

#ifdef ARDUINO_ARCH_STM32L4   // STM architecture

// This is a hack to let me access the internal stuff..
#define private public
#include <Wire.h>
#undef private

#include <FS.h>
#define digitalWriteFast digitalWrite
#include <stm32l4_wiring_private.h>
#include <stm32l4xx.h>
#include <armv7m.h>
#include <stm32l4_gpio.h>
#include <stm32l4_sai.h>
#include <stm32l4_dma.h>
#include <stm32l4_system.h>
#include <arm_math.h>
#include <STM32.h>
#define DMAChannel stm32l4_dma_t
#define DMAMEM
#define NVIC_SET_PRIORITY(X,Y) NVIC_SetPriority((X), (IRQn_Type)(Y))

#endif

#include <math.h>
#include <malloc.h>

#ifdef ENABLE_SERIALFLASH

// This is a hack to let me access the internal stuff..
#define private public
#define protected public

#include <SerialFlash.h>

#undef private
#undef protected
#endif



const char version[] = "v7.1x" OSX_SUBVERSION ;
const char install_time[] = __DATE__ " " __TIME__;

#include "common/common.h"
#include "common/state_machine.h"
#include "common/stdout.h"
#include "common/errors.h"  // Include just the base class, unless PROFFIEOS_DEFINE_FUNCTION_STAGE is defined (see EOF)



DEFINE_COMMON_STDOUT_GLOBALS;

void PrintQuotedValue(const char *name, const char* str) {
  STDOUT.print(name);
  STDOUT.write('=');
  if (str) {
    while (*str) {
      switch (*str) {
        case '\n':
          STDOUT.print("\\n");
          break;
        case '\t':
          STDOUT.print("\\t");
          break;
        case '\\':
          STDOUT.write('\\');
        default:
          STDOUT.write(*str);
      }
      ++str;
    }
  }
  STDOUT.println("");
}

#include "common/Probe.h"   // LoopCounter and ScopedCycleCounter redefined here
CPUprobe audio_dma_interrupt_cycles;
CPUprobe pixel_dma_interrupt_cycles;
CPUprobe motion_interrupt_cycles;
CPUprobe wav_interrupt_cycles;

#define NELEM(X) (sizeof(X)/sizeof((X)[0]))

#ifdef DOSFS_CONFIG_STARTUP_DELAY
#define PROFFIEOS_SD_STARTUP_DELAY DOSFS_CONFIG_STARTUP_DELAY
#else
#define PROFFIEOS_SD_STARTUP_DELAY 1000
#endif

#ifndef CONFIG_STARTUP_DELAY
#define CONFIG_STARTUP_DELAY 0
#endif

#if PROFFIEOS_SD_STARTUP_DELAY > CONFIG_STARTUP_DELAY
#define PROFFIEOS_STARTUP_DELAY PROFFIEOS_SD_STARTUP_DELAY
#else
#define PROFFIEOS_STARTUP_DELAY CONFIG_STARTUP_DELAY
#endif

#include "common/linked_list.h"
#include "common/looper.h"
#include "common/command_parser.h"


CommandParser* parsers = NULL;


#include "common/vec3.h"  
#include "common/quat.h"
#include "common/ref.h"
#include "common/events.h"
#include "common/saber_base.h"
#include "common/saber_base_passthrough.h"
SaberBase* saberbases = NULL;
SaberBase::LockupType SaberBase::lockup_ = SaberBase::LOCKUP_NONE;
bool SaberBase::on_ = false;
#if defined(PROFFIEBOARD) || ( defined(ULTRAPROFFIE) && ULTRAPROFFIE_VERSION == 'P')
  uint32_t SaberBase::last_motion_request_ = 0;
#endif 
uint32_t SaberBase::current_variation_ = 0;
float SaberBase::sound_length = 0.0;
int SaberBase::sound_number = -1;
float SaberBase::clash_strength_ = 0.0;
bool SaberBase::monoFont = true;  
uint8_t Sensitivity::master = 128;



#include "common/box_filter.h"

// Returns the decimals of a number, ie 12.2134 -> 0.2134
float fract(float x) { return x - floorf(x); }

// clamp(x, a, b) makes sure that x is between a and b.
float clamp(float x, float a, float b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}
float Fmod(float a, float b) {
  return a - floorf(a / b) * b;
}

int32_t clampi32(int32_t x, int32_t a, int32_t b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}
int16_t clamptoi16(int32_t x) {
  return clampi32(x, -32768, 32767);
}
int32_t clamptoi24(int32_t x) {
  return clampi32(x, -8388608, 8388607);
}

#include "common/sin_table.h"

void EnableBooster();
void EnableAmplifier();
#ifdef PROFFIEBOARD
  bool AmplifierIsActive();   // need this because there's no power manager (yet) for ProffieBoard
#endif
void MountSDCard();
const char* GetSaveDir();

#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
void SilentEnableBooster(bool on);
void SilentEnableAmplifier(bool on);
#endif 

#include "common/lsfs.h"
#include "common/strfun.h"


// Double-zero terminated array of search paths.
// No trailing slashes!
char current_directory[128];
const char* next_current_directory(const char* dir) {
  dir += strlen(dir);
  dir ++;
  if (!*dir) return NULL;
  return dir;
}
const char* last_current_directory() {
  const char* ret = current_directory;
  while (true) {
    const char* tmp = next_current_directory(ret);
    if (!tmp) return ret;
    ret = tmp;
  }
}

const char* previous_current_directory(const char* dir) {
  if (dir == current_directory) return nullptr;
  dir -= 2;
  while (true) {
    if (dir == current_directory) return current_directory;
    if (!*dir) return dir + 1;
    dir--;
  }
}

#ifdef ULTRAPROFFIE_CHARGER
uint16_t xChargerGetLimit();
#endif

#include "sound/sound.h"


#include "common/battery_monitor.h"
#include "common/BatteryCharger.h"


#include "common/color.h"
#include "common/range.h"
#include "common/fuse.h"

#include "blades/blade_base.h"
#include "blades/blade_wrapper.h"

class MicroEventTime {
  void SetToNow() { micros_ = micros(); millis_ = millis(); }
  uint32_t millis_since() { return millis() - millis_; }
  uint32_t micros_since() {
    if (millis_since() > (0xFFFF0000UL / 1000)) return 0xFFFFFFFFUL;
    return micros() - micros_;
  }
private:
  uint32_t millis_;
  uint32_t micros_;
};

template<class T, class U>
struct is_same_type { static const bool value = false; };

template<class T>
struct is_same_type<T, T> { static const bool value = true; };

// This really ought to be a typedef, but it causes problems I don't understand.
#define StyleAllocator class StyleFactory*

#include "styles/rgb.h"
#include "styles/rgb_arg.h"
#include "styles/charging.h"
#include "styles/fire.h"
#include "styles/sparkle.h"
#include "styles/gradient.h"
#include "styles/random_flicker.h"
#include "styles/random_per_led_flicker.h"
#include "styles/audio_flicker.h"
#include "styles/brown_noise_flicker.h"
#include "styles/hump_flicker.h"
#include "styles/rainbow.h"
#include "styles/color_cycle.h"
#include "styles/cylon.h"
#include "styles/ignition_delay.h"
#include "styles/retraction_delay.h"
#include "styles/pulsing.h"
#include "styles/blinking.h"
#include "styles/on_spark.h"
#include "styles/rgb_cycle.h"
#include "styles/clash.h"
#include "styles/lockup.h"  // Also does "drag"
#include "styles/blast.h"
#include "styles/strobe.h"
#include "styles/inout_helper.h"
#include "styles/inout_sparktip.h"
#include "styles/colors.h"
#include "styles/mix.h"
#include "styles/style_ptr.h"
#include "styles/file.h"
#include "styles/stripes.h"
#include "styles/random_blink.h"
#include "styles/sequence.h"
#include "styles/byteorder.h"
#include "styles/rotate_color.h"
#include "styles/colorchange.h"
#include "styles/transition_pulse.h"
#include "styles/transition_effect.h"
#include "styles/transition_loop.h"
#include "styles/effect_sequence.h"
#include "styles/color_select.h"
#include "styles/remap.h"
#include "styles/edit_mode.h"
#include "styles/pixelate.h"

// functions
#include "functions/ifon.h"
#include "functions/change_slowly.h"
#include "functions/int.h"
#include "functions/int_arg.h" 
#include "functions/int_select.h"
#include "functions/sin.h"
#include "functions/scale.h"
#include "functions/battery_level.h"
#include "functions/trigger.h"
#include "functions/bump.h"
#include "functions/smoothstep.h"
#include "functions/swing_speed.h"
#include "functions/sound_level.h"
#include "functions/blade_angle.h"
#include "functions/variation.h"
#include "functions/twist_angle.h"
#include "functions/layer_functions.h"
#include "functions/islessthan.h"
#include "functions/circular_section.h"
#include "functions/marble.h"
#include "functions/slice.h"
#include "functions/mult.h"
#include "functions/wavlen.h"
#include "functions/wavnum.h"
#include "functions/effect_position.h"
#include "functions/time_since_effect.h"
#include "functions/sum.h"
#include "functions/ramp.h"
#include "functions/center_dist.h"
#include "functions/linear_section.h"
#include "functions/hold_peak.h"
#include "functions/clash_impact.h"
#include "functions/effect_increment.h"
#include "functions/increment.h"
#include "functions/subtract.h"
#include "functions/divide.h"
#include "functions/isbetween.h"
#include "functions/clamp.h"
#include "functions/alt.h"
#include "functions/volume_level.h"
#include "functions/mod.h"

// transitions
#include "transitions/fade.h"
#include "transitions/join.h"
#include "transitions/concat.h"
#include "transitions/instant.h"
#include "transitions/delay.h"
#include "transitions/wipe.h"
#include "transitions/join.h"
#include "transitions/boing.h"
#include "transitions/random.h"
#include "transitions/wave.h"
#include "transitions/select.h"
#include "transitions/extend.h"
#include "transitions/center_wipe.h"
#include "transitions/sequence.h"
#include "transitions/blink.h"
#include "transitions/doeffect.h"
#include "transitions/loop.h"
#include "styles/legacy_styles.h"
//responsive styles

#include "styles/responsive_styles.h"

// #include "styles/pov.h"

class NoLED;

#include "blades/power_pin.h"
#include "blades/pwm_pin.h"

#include "blades/analogLED.h"

#include "blades/ws2811_blade.h"
#include "blades/simple_blade.h"


#include "common/malloc_helper.h"
#include "common/Preset.h"
#include "common/Profile.h"   
#include "common/blade_config.h"


extern BladeConfig blades[];
BladeConfig* current_config = blades;

// Get a address of a blade. bladeNo starts at 1 !!!
inline class BladeBase* BladeAddress(uint8_t bladeNo) {
    switch (bladeNo) {
        #if NUM_BLADES >= 1
            case 1: return current_config->blade1;
        #endif
         #if NUM_BLADES >= 2
            case 2: return current_config->blade2;
        #endif
        #if NUM_BLADES >= 3
            case 3: return current_config->blade3;
        #endif
        #if NUM_BLADES >= 4
            case 4: return current_config->blade4;
        #endif
        #if NUM_BLADES >= 5
            case 5: return current_config->blade5;
        #endif
        #if NUM_BLADES >= 6
            case 6: return current_config->blade6;
        #endif                                   
        default: return 0;
    }
}



class BladeBase* GetPrimaryBlade() {
#if NUM_BLADES == 0
  return nullptr;
#else  
  return current_config->blade1;
#endif  
}
const char* GetSaveDir() {
  if (!current_config) return "";
  if (!current_config->save_dir) return "";
  return current_config->save_dir;
}



#define CONFIG_PRESETS
#include CONFIG_FILE
#undef CONFIG_PRESETS

#define CONFIG_PROP
#include CONFIG_FILE
#undef CONFIG_PROP

#ifndef PROP_TYPE
  #ifdef PF_PROP_ULTRASABERS
    #include "props/ultrasaber.h"  
  #elif defined(PF_PROP_SABER)
    #include "props/saber.h"
  #endif

#endif // PROP_TYPE


PROP_TYPE prop;


#include "buttons/button.h"



uint32_t startup_AHB1ENR;
uint32_t startup_AHB2ENR;
uint32_t startup_AHB3ENR;
uint32_t startup_APB1ENR1;
uint32_t startup_APB1ENR2;
uint32_t startup_APB2ENR;
uint32_t startup_MODER[4];





#define CONFIG_BUTTONS
#include CONFIG_FILE
#undef CONFIG_BUTTONS


#include "common/Install.h"   // after all invocations of CONFIG_FILE

 




// #include "common/sd_test.h"

#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
  #include "common/HardwareID.h"
#endif

#ifdef OSX_ENABLE_MTP
  #include "common/serial.h"
#endif 



class I2CDevice;

#ifdef ARDUINO_ARCH_STM32L4   // STM architecture
  int	rand (void) { return millis(); }    // cheaper and randomer, since nothing is in sync
  // Need those to report RAM usage
  extern "C" char *sbrk(int i);
  extern uint32_t __StackLimit;
  extern uint32_t __StackTop;
  extern uint32_t __HeapLimit;
  extern uint32_t __HeapBase;
  extern uint32_t __data_start__;
  extern uint32_t __data_end__;
  extern uint32_t __bss_start__;
  extern uint32_t __bss_end__;  
#else
namespace FASTRandom{
int	rand (void) { return millis(); }    // cheaper and randomer, since nothing is in sync
int random(int x){ return (millis() & 0x7fffff) % x; }
}
using namespace FASTRandom;
#endif

class Commands : public CommandParser {
 public:
  enum PinType {
    PinTypeFloating,
    PinTypePulldown,
    PinTypeCap,
    PinTypeOther,
  };

  bool TestPin(int pin, PinType t) {
    int ret = 0;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delayMicroseconds(20);
    ret <<= 1;
    ret |= digitalRead(pin);

    digitalWrite(pin, HIGH);
    delayMicroseconds(20);
    ret <<= 1;
    ret |= digitalRead(pin);

    // Discharge time
    pinMode(pin, INPUT_PULLDOWN);
    uint32_t start = micros();
    uint32_t end;
    while (digitalRead(pin)) {
      end = micros();
      if (end - start > 32768) break; // 32 millis
    }
    ret <<= 16;
    ret |= (end - start);

    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(20);
    ret <<= 1;
    ret |= digitalRead(pin);
    pinMode(pin, INPUT);

    return ret;
  }
  bool Parse(const char* cmd, const char* e) override {
//#ifndef DISABLE_DIAGNOSTIC_COMMANDS
    if (!strcmp(cmd, "help")) {
      CommandParser::DoHelp();
      return true;
    }
//#endif

#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
  if(!strcmp(cmd, "board_info")) {
    STDOUT.println("board_info-START");
    STDOUT.println("HARDWARE:");
    PROFFIE_HDID.printHWSN(NULL);
    STDOUT.println("FIRMWARE:");
    STDOUT.print("ProffieOS "); STDOUT.println(version);
    STDOUT.print("Compiled: "); STDOUT.println(install_time);
    STDOUT.print("Build for: "); STDOUT.println(XSTR(HWVER));

    STDOUT.print("Prop: '"); STDOUT.print(prop.name()); STDOUT.println("'");
    STDOUT.print("Supported blades: "); STDOUT.println(NUM_BLADES);
    STDOUT.print("Diagnose: ");
    #ifdef PF_STATUS_REPORT_ON
      STDOUT.println("ON");
    #else
      STDOUT.println("OFF");
    #endif
    STDOUT.print("Developer mode: ");
    #ifdef ENABLE_DEVELOPER_MODE
      STDOUT.println("ON");
    #else
      STDOUT.println("OFF");
    #endif

    STDOUT.println("board_info-END");
    return true;
  }

#ifdef ENABLE_DEVELOPER_COMMANDS
    if (!strcmp(cmd, "whatison")) {
      bool on = false;
      SaberBase::DoIsOn(&on);
      STDOUT.print("Saber bases: ");
      STDOUT.println(on ? "On" : "Off");
      STDOUT.print("Beeper: ");
      STDOUT.println(beeper.isPlaying() ? "On" : "Off");
      STDOUT.print("Talker: ");
      STDOUT.println(talkie.isPlaying() ? "On" : "Off");
      for (size_t i = 0; i < NELEM(wav_players); i++) {
	       STDOUT << "Wav player " << i << ": "
                << (wav_players[i].isPlaying() ? "On" : "Off")
                << " (eof =  " << wav_players[i].eof()
                << " volume = " << wav_players[i].volume()
                << " refs = " << wav_players[i].refs()
                << " fade speed = " << wav_players[i].fade_speed()
                << " filename=" << wav_players[i].filename() << ")\n";
      }
      return true;
    }
#endif 


#endif


  if (!strcmp(cmd, "format")) {
    STDOUT.println("format-START");
    int res;
    #ifdef ARDUINO_ARCH_ESP32   // ESP architecture
    // TODO add DOSFS.format(); after adding it in lower package 
    #else
    res = f_format(0);        // perform format
    #endif
    if(res) {
      #ifdef ARDUINO_ARCH_ESP32   // ESP architecture
      // TODO add DOSFS.format(); after adding it in lower package 
      #else
      res = f_hardformat(0);
      #endif

      STDOUT.print("HARD format completed. Ans = ");
      STDOUT.println(res);
    } else {
      STDOUT.print("FAST format completed. Ans = ");
      STDOUT.println(res);
    }
    STDOUT.println("format-END");
    return true;
  
  }
  
#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
    if (!strcmp(cmd, "get_bor")) {
      STDOUT.println("get_bor-START");
      STDOUT.print("BOR Level =  "); STDOUT.println(stm32l4_bor_get());
      STDOUT.println("get_bor-END");
      return true;
    }
#endif

  #ifndef DISABLE_DIAGNOSTIC_COMMANDS
    if (!strcmp(cmd, "renamePath") && e) {
      STDOUT.println("rename path");
      { 
        char *String = (char*)e;
        char pathFrom[128];
        char pathTo[128];
        char *token;
        for(uint8_t i=0; i<128;i++)
        {
            pathFrom[i] = 0;
            pathTo[i] = 0;
        }
        LOCK_SD(true);
        token = strtok(String, " ");
        uint8_t i = 0;
        while( token != NULL ) {
          if(!i) {
            sprintf(pathFrom, "%s", token);
            STDOUT.print("PATH FROM "); STDOUT.println(pathFrom);
            i++;
          } else {
            sprintf(pathTo, "%s", token);
            STDOUT.print("PATH TO "); STDOUT.println(pathTo);
            i++;
            break;
          }
          token = strtok(NULL, " ");
        }
        if(i == 2) {
          STDOUT.print("SUCCESS PArSiNG");
          STDOUT.print("Renaming ");
          STDOUT.print("from ");
          STDOUT.print(pathFrom);
          STDOUT.print(" to ");
          STDOUT.println(pathTo);
          if(LSFS::Exists(pathFrom)) {
            STDOUT.print("Renaming result = "); STDOUT.println(LSFS::RenamePath(pathFrom, pathTo));
          } else {
              STDOUT.print("Path not exist");
          }

        }
        LOCK_SD(false);

      }

      return true;
    }   
  #endif // DISABLE_DIAGNOSTIC_COMMANDS


  #ifdef ENABLE_DIAGNOSE_COMMANDS
    if (!strcmp(cmd, "cod") && e) {
        int8_t res;
        bool result;
        uint32_t readCRC, calcCRC;
        CodReader codReaderObj;  
        result = codReaderObj.Open(e);
        if(result) 
        {
          calcCRC = codReaderObj.codInterpreter->GetFileCrC(&readCRC);
          STDOUT.print("CRC READ "); STDOUT.print(readCRC); STDOUT.print(" CRC CALC "); STDOUT.println(calcCRC);
          for(uint8_t i=1; i <= codReaderObj.headerNrEntries; i++ )
          {
              res = codReaderObj.GetEntry(i);
              if(res > 0)
              {   
                  if(res == COD_ENTYPE_TABLE) 
                  {   STDOUT.print("   "); STDOUT.print(codReaderObj.codProperties.table.ID); STDOUT.print(": table ");
                      STDOUT.print(codReaderObj.codProperties.table.Rows); STDOUT.print(" x "); STDOUT.print(codReaderObj.codProperties.table.Columns); 
                      STDOUT.print(" for handler "); STDOUT.println(codReaderObj.codProperties.table.Handler);
                      // STDOUT.print("==Entry TYPE=="); STDOUT.print(res); STDOUT.print(" "); STDOUT.println((char)res);
                      // STDOUT.print("Table ID = "); STDOUT.println(codReaderObj.codProperties.table.ID);
                      // STDOUT.print("Table SizeR  = "); STDOUT.println(codReaderObj.codProperties.table.Rows);
                      // STDOUT.print("Table SizeC = "); STDOUT.println(codReaderObj.codProperties.table.Columns); 
                      // STDOUT.print("Table dataType = "); STDOUT.println(codReaderObj.codProperties.table.DataType);
                      // STDOUT.print("Table Handler = "); STDOUT.println(codReaderObj.codProperties.table.Handler);

                  } else if(res == COD_ENTYPE_STRUCT) {
                      STDOUT.print("   "); STDOUT.print(codReaderObj.codProperties.structure.ID); STDOUT.print(": struct of ");
                      STDOUT.print(codReaderObj.codProperties.structure.Size); STDOUT.print(" bytes for handler  ");
                      STDOUT.println(codReaderObj.codProperties.structure.Handler);

                      // STDOUT.print("==Entry TYPE== "); STDOUT.print(res); STDOUT.print(" "); STDOUT.println((char)res);
                      // STDOUT.print("Struct ID = "); STDOUT.println(codReaderObj.codProperties.structure.ID);
                      // STDOUT.print("Struct Size = "); STDOUT.println(codReaderObj.codProperties.structure.Size);
                      // STDOUT.print("Struct Handler = "); STDOUT.println(codReaderObj.codProperties.structure.Handler);

                  } else {
                      STDOUT.print("==Entry TYPE=="); STDOUT.println(res);
                      STDOUT.print("Entry on nrCRT "); STDOUT.println(i);
                  }

              } 
              else if(res == 0 )
              {
                  STDOUT.print("Inconsistent number of entries, should be "); STDOUT.println(i);
              } else {
                  STDOUT.print("Error reading entry number "); STDOUT.println(i);
              }

          }
        }
        codReaderObj.Close();
        STDOUT.print("End of "); STDOUT.print(e); STDOUT.println(" --- ");

        return true;
      }
  #endif // ENABLE_DIAGNOSE_COMMANDS


  #if defined(X_PROBECPU) && defined(ENABLE_DEVELOPER_MODE) && defined(ARDUINO_ARCH_STM32L4)
      if (!strcmp(cmd, "malloc")) {
          struct mallinfo mi = mallinfo();
          STDOUT.print(" * Total non-mmapped bytes (arena): "); STDOUT.println(mi.arena);
          STDOUT.print(" * Number of free chunks (ordblks): "); STDOUT.println(mi.ordblks);
          STDOUT.print(" * Number of free fastbin blocks (smblks): "); STDOUT.println(mi.smblks);
          STDOUT.print(" * Number of mapped regions (hblks): "); STDOUT.println(mi.hblks);
          STDOUT.print(" * Bytes in mapped regions (hblkhd): "); STDOUT.println(mi.hblkhd);
          STDOUT.print(" * Max. total allocated space (usmblks): "); STDOUT.println(mi.usmblks);
          STDOUT.print(" * Free bytes held in fastbins (fsmblks): "); STDOUT.println(mi.fsmblks);
          STDOUT.print(" * Total allocated space (uordblks): "); STDOUT.println(mi.uordblks);
          STDOUT.print(" * Total free space (fordblks): "); STDOUT.println(mi.fordblks);
          STDOUT.print(" * Topmost releasable block (keepcost): "); STDOUT.println(mi.keepcost);
        return true;
      }
      if (!strcmp(cmd, "ram")) {
        char *heapend = (char*)sbrk(0);
        char * stack_ptr = (char*)__get_MSP();
        float percent, tmpPercent;

        STDOUT.print(" * Static RAM (data & bss): "); STDOUT.print((uint32_t)(&__HeapLimit) - (uint32_t)(&__data_start__) ); STDOUT.print(" bytes = "); 
        percent = 100.0f *  ( (uint32_t)(&__HeapLimit) - (uint32_t)(&__data_start__) ) / ( (uint32_t)(&__StackTop) - (uint32_t)(&__data_start__) );
        STDOUT.print(percent); STDOUT.println("%.");
        STDOUT.print(" * Dynamic RAM (heap): "); STDOUT.print((uint32_t)(heapend) - (uint32_t)(&__HeapLimit) ); STDOUT.print(" bytes = "); 
        tmpPercent = 100.0f *  ( (uint32_t)(heapend) - (uint32_t)(&__HeapLimit) ) / ( (uint32_t)(&__StackTop) - (uint32_t)(&__data_start__) );
        STDOUT.print(tmpPercent); STDOUT.println("%.");
        percent += tmpPercent;
        STDOUT.print(" * Temporary RAM (stack): "); STDOUT.print((uint32_t)(&__StackTop) - (uint32_t)(stack_ptr) ); STDOUT.print(" bytes = "); 
        tmpPercent = 100.0f *  ( (uint32_t)(&__StackTop) - (uint32_t)(stack_ptr) ) / ( (uint32_t)(&__StackTop) - (uint32_t)(&__data_start__) );
        STDOUT.print(tmpPercent); STDOUT.print("%. - Minimum is set to "); STDOUT.print(((uint32_t)(&__StackTop) - (uint32_t)(&__StackLimit) )); STDOUT.println(" bytes.");
        percent += tmpPercent;
        STDOUT.print("----- Total SRAM1 usage: "); STDOUT.print(percent); STDOUT.println("%.");
        return true;
      }


      if (!strcmp(cmd, "top")) {
        // 1. Enable cycle counter if it's not
        if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
          CoreDebug->DEMCR |= 1<<24; // DEMCR_TRCENA_Msk;
          DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
          STDOUT.println("Cycle counting enabled, top will work next time.");
          return true;
        }
        // 2. Report call frequencies
        STDOUT.println("");
        STDOUT.println("Call frequencies [Hz]:"); 
        audio_dma_interrupt_cycles.Print(DoWhatToProbe::print_frequency); STDOUT.println(" Audio DMA ISR");  // report frequency for ISRs
        wav_interrupt_cycles.Print(DoWhatToProbe::print_frequency); STDOUT.println(" WAV Reader ISR"); 
        pixel_dma_interrupt_cycles.Print(DoWhatToProbe::print_frequency); STDOUT.println(" Pixel ISR"); 
        motion_interrupt_cycles.Print(DoWhatToProbe::print_frequency); STDOUT.println(" Motion ISR"); 
        Looper::DoProbe(DoWhatToProbe::print_frequency);  // Call .Print() on all looper's probes                                                                                // report frequency for all loopers
        // 3. Report runtimes
        STDOUT.println("Execution times [us]:"); 
        audio_dma_interrupt_cycles.Print(DoWhatToProbe::print_duration); STDOUT.println(" Audio DMA ISR");  // report frequency for ISRs
        wav_interrupt_cycles.Print(DoWhatToProbe::print_duration); STDOUT.println(" WAV Reader ISR"); 
        pixel_dma_interrupt_cycles.Print(DoWhatToProbe::print_duration); STDOUT.println(" Pixel ISR"); 
        motion_interrupt_cycles.Print(DoWhatToProbe::print_duration); STDOUT.println(" Motion ISR"); 
        Looper::DoProbe(DoWhatToProbe::print_duration); 
        // 4. Report CPU usage
        STDOUT.println("CPU usage [%]:"); 
        audio_dma_interrupt_cycles.Print(DoWhatToProbe::print_cpu_usage); STDOUT.println(" Audio DMA ISR");  // report frequency for ISRs
        wav_interrupt_cycles.Print(DoWhatToProbe::print_cpu_usage); STDOUT.println(" WAV Reader ISR"); 
        pixel_dma_interrupt_cycles.Print(DoWhatToProbe::print_cpu_usage); STDOUT.println(" Pixel ISR"); 
        motion_interrupt_cycles.Print(DoWhatToProbe::print_cpu_usage); STDOUT.println(" Motion ISR"); 
        Looper::DoProbe(DoWhatToProbe::print_cpu_usage); 
        // 5. Report loop counters
        SaberBase::DoTop(0);  // original DoTop needed total cycles to make calculations, we send 0 to keep backward compatibility
        // 6. Reset probes
        noInterrupts();
        Looper::DoProbe(DoWhatToProbe::reset_probe); 
        audio_dma_interrupt_cycles.Reset();
        wav_interrupt_cycles.Reset();
        pixel_dma_interrupt_cycles.Reset();
        motion_interrupt_cycles.Reset();
        interrupts();
        STDOUT.println("-> CPU probes reset.");
        STDOUT.println("");
        return true;
      }

    if (!strcmp(cmd, "clk")) {
      if (e) {
        uint32_t c = atoi(e) * 1000000;
        stm32l4_system_sysclk_configure(c, c/2, c/2);
      }
      STDOUT.print("Clocks: hse=");
      STDOUT.print(stm32l4_system_hseclk());
      STDOUT.print(" lse=");
      STDOUT.print(stm32l4_system_lseclk());
      STDOUT.print(" sys=");
      STDOUT.print(stm32l4_system_sysclk());
      STDOUT.print(" f=");
      STDOUT.print(stm32l4_system_fclk());
      STDOUT.print(" h=");
      STDOUT.print(stm32l4_system_hclk());
      STDOUT.print(" p1=");
      STDOUT.print(stm32l4_system_pclk1());
      STDOUT.print(" p2=");
      STDOUT.print(stm32l4_system_pclk2());
      return true;
    }


    if (!strcmp(cmd, "dmamap")) {
      for (int channel = 0; channel < 16; channel++) {
        stm32l4_dma_t *dma = stm32l4_dma_get(channel);
        if (dma) {
          STDOUT.print(" DMA");
          STDOUT.print( 1 +(channel / 8) );
          STDOUT.print("_CH");
          STDOUT.print( channel % 8 );
          STDOUT.print(" = ");
          STDOUT.println(dma->channel >> 4, HEX);
        }
      }
      return true;
    }



  #endif // X_PROBECPU

  if (!strcmp(cmd, "alive")) {
    STDOUT.println("alive-START");
    if (prop.IsOn()) STDOUT.println("propstate: on");
    else STDOUT.println("propstate: off");
    STDOUT.print("battery: ");  battery_monitor.Print(); STDOUT.println("");
    STDOUT.println("alive-END");
    return true;
  }

  #ifdef X_BROADCAST // Enable binary streaming
    if (!strcmp(cmd, "broadcast")) {
        STDOUT.StartBroadcast();    // reset board to turn off, unless it gets stopped in code
    return true;
    }
  #endif // X_BROADCAST





#ifdef ENABLE_SD

  #ifdef ENABLE_DIAGNOSE_COMMANDS
      if (!strcmp(cmd, "dir")) {
        LOCK_SD(true);
        if (!e || LSFS::Exists(e)) {
          for (LSFS::Iterator dir(e ? e : ""); dir; ++dir) {
            STDOUT.print(dir.name());
            STDOUT.print(" ");
            STDOUT.println(dir.size());
          }
          STDOUT.println("Done listing files.");
        } else {
          STDOUT.println("No such directory.");
        }
        LOCK_SD(false);
        return true;
      }
  #endif 

  #ifdef ENABLE_DEVELOPER_COMMANDS
    if (!strcmp(cmd, "del") && e) {
      LOCK_SD(true);
      LSFS::Remove(e);
      LOCK_SD(false);
      return true;
    }
  #endif 

#endif  // ENABLE_SD


#ifdef ENABLE_AUDIO

  #ifdef ENABLE_DEVELOPER_COMMANDS
      if (!strcmp(cmd, "dumpwav")) {
        int16_t tmp[32];
        wav_players[0].Stop();
        wav_players[0].read(tmp, NELEM(tmp));
        wav_players[0].Play(e);
        for (int j = 0; j < 64; j++) {
          int k = wav_players[0].read(tmp, NELEM(tmp));
          for (int i = 0; i < k; i++) {
            STDOUT.print(tmp[i]);
            STDOUT.print(" ");
          }
          STDOUT.println("");
        }
        wav_players[0].Stop();
        return true;
      }
  #endif // ENABLE_DEVELOPER_COMMANDS

  #ifdef ENABLE_DEVELOPER_COMMANDS
      if (!strcmp(cmd, "dumpwavplayer")) {
        for (size_t i = 0; i < NELEM(wav_players); i++) {
          if (e && atoi(e) != i) continue;
          wav_players[i].dump();
        }
        return true;
      }



  #endif // ENABLE_DEVELOPER_COMMANDS

#endif // ENABLE_AUDIO


#ifdef ENABLE_DIAGNOSE_COMMANDS
  
  if (!strcmp(cmd, "effects")) {
      Effect::ShowAll();
      return true;
  }

#endif // ENABLE_DIAGNOSE_COMMANDS





#if defined(ENABLE_DEVELOPER_COMMANDS) && defined(ARDUINO_ARCH_STM32L4)

    if (!strcmp(cmd, "dumpfusor")) {
      fusor.dump();
      return true;
    }

    #if defined(ENABLE_MOTION) || defined(ENABLE_SSD1306) || defined(INCLUDE_SSD1306)
        if (!strcmp(cmd, "i2cstate")) {
          extern void DumpI2CState();
          DumpI2CState();
          SaberBase::DumpMotionRequest();
          return true;
        }
    #endif

    if (!strcmp(cmd, "portstates")) {
      GPIO_TypeDef *GPIO;
      for (int i = 0; i < 4; i++) {
        switch (i) {
          case 0:
            GPIO = (GPIO_TypeDef *)GPIOA_BASE;
            STDOUT.print("PORTA: ");
            break;
          case 1:
            GPIO = (GPIO_TypeDef *)GPIOB_BASE;
            STDOUT.print("PORTB: ");
            break;
          case 2:
            GPIO = (GPIO_TypeDef *)GPIOC_BASE;
            STDOUT.print("PORTC: ");
            break;
          case 3:
            GPIO = (GPIO_TypeDef *)GPIOH_BASE;
            STDOUT.print("PORTH: ");
            break;
        }
        for (int j = 15; j >= 0; j--) {
          uint32_t now = (GPIO->MODER >> (j * 2)) & 3;
          uint32_t saved = (startup_MODER[i] >> (j * 2)) & 3;
          STDOUT.print((now == saved ? "ioga" : "IOGA")[now]);
          if (!(j & 3)) STDOUT.print(" ");
        }
        STDOUT.print("  ");
        for (int j = 15; j >= 0; j--) {
          uint32_t now = (GPIO->PUPDR >> (j * 2)) & 3;
          STDOUT.print("-ud?"[now]);
          if (!(j & 3)) STDOUT.print(" ");
        }
        STDOUT.print("  ");
        for (int j = 15; j >= 0; j--) {
          uint32_t now = ((GPIO->IDR >> j) & 1) | (((GPIO->ODR >> j) & 1) << 1);
          STDOUT.print("lhLH"[now]);
          if (!(j & 3)) STDOUT.print(" ");
        }
        STDOUT.print("  ");
        for (int j = 15; j >= 0; j--) {
	  int afr = 0xf & (GPIO->AFR[j >> 3] >> ((j & 7) * 4));
          STDOUT.print("0123456789ABCDEF"[afr]);
          if (!(j & 3)) STDOUT.print(" ");
        }
        STDOUT.println("");
      }
      return true;
    }

    if (!strcmp(cmd, "whatispowered")) {
      STDOUT.print("ON: ");
      #define PRINTIFON(REG, BIT) do {                                        \
              if (RCC->REG & RCC_##REG##_##BIT##EN) {                         \
                STDOUT.print(" " #BIT);                                       \
                if (!(startup_##REG & RCC_##REG##_##BIT##EN)) STDOUT.print("+"); \
              }                                                               \
            } while(0)

            PRINTIFON(AHB1ENR,FLASH);
            PRINTIFON(AHB1ENR,DMA1);
            PRINTIFON(AHB1ENR,DMA2);
            PRINTIFON(AHB2ENR,GPIOA);
            PRINTIFON(AHB2ENR,GPIOB);
      #ifdef GPIOC_BASE
            PRINTIFON(AHB2ENR,GPIOC);
      #endif      
      #ifdef GPIOD_BASE
            PRINTIFON(AHB2ENR,GPIOD);
      #endif      
      #ifdef GPIOE_BASE
            PRINTIFON(AHB2ENR,GPIOE);
      #endif
      #if defined(STM32L476xx) || defined(STM32L496xx)
            PRINTIFON(AHB2ENR,GPIOF);
            PRINTIFON(AHB2ENR,GPIOG);
      #endif
            PRINTIFON(AHB2ENR,GPIOH);
      #if defined(STM32L496xx)
            PRINTIFON(AHB2ENR,GPIOI);
      #endif
            PRINTIFON(AHB2ENR,ADC);
            PRINTIFON(APB1ENR1,DAC1);
      #if defined(STM32L476xx) || defined(STM32L496xx)
            PRINTIFON(AHB2ENR,OTGFS);
      #elif !defined(STM32L431xx)
            PRINTIFON(APB1ENR1,USBFS);
      #endif
            PRINTIFON(APB2ENR,USART1);
            PRINTIFON(APB1ENR1,USART2);
      #if defined(STM32L433xx) || defined(STM32L476xx) || defined(STM32L496xx)
            PRINTIFON(APB1ENR1,USART3);
      #endif
      #if defined(STM32L476xx) || defined(STM32L496xx)
            PRINTIFON(APB1ENR1,UART4);
            PRINTIFON(APB1ENR1,UART5);
      #endif
            PRINTIFON(APB1ENR2,LPUART1);
            PRINTIFON(APB1ENR1,I2C1);
      #if defined(STM32L433xx) || defined(STM32L476xx) || defined(STM32L496xx)
            PRINTIFON(APB1ENR1,I2C2);
      #endif
            PRINTIFON(APB1ENR1,I2C3);
      #if defined(STM32L496xx)
            PRINTIFON(APB1ENR2,I2C4);
      #endif
            PRINTIFON(APB2ENR,SPI1);
      #if defined(STM32L433xx) || defined(STM32L476xx) || defined(STM32L496xx)
            PRINTIFON(APB1ENR1,SPI2);
      #endif
            PRINTIFON(APB1ENR1,SPI3);
            PRINTIFON(APB1ENR1,CAN1);
      #if defined(STM32L496xx)
            PRINTIFON(APB1ENR1,CAN2);
      #endif
            PRINTIFON(AHB3ENR,QSPI);
      #if defined(STM32L433xx) || defined(STM32L476xx) || defined(STM32L496xx)
            PRINTIFON(APB2ENR,SDMMC1);
      #endif
            PRINTIFON(APB2ENR,SAI1);
      #if defined(STM32L476xx) || defined(STM32L496xx)
            PRINTIFON(APB2ENR,SAI2);
            PRINTIFON(APB2ENR,DFSDM1);
      #endif
            PRINTIFON(APB2ENR,TIM1);
            PRINTIFON(APB1ENR1,TIM2);
      #ifdef TIM3_BASE
            PRINTIFON(APB1ENR1,TIM3);
      #endif
      #ifdef TIM4_BASE
            PRINTIFON(APB1ENR1,TIM4);
      #endif
      #ifdef TIM5_BASE
            PRINTIFON(APB1ENR1,TIM5);
      #endif
            PRINTIFON(APB1ENR1,TIM6);
      #ifdef TIM7_BASE      
            PRINTIFON(APB1ENR1,TIM7);
      #endif
      #ifdef TIM8_BASE      
            PRINTIFON(APB2ENR,TIM8);
      #endif
            PRINTIFON(APB2ENR,TIM15);
            PRINTIFON(APB2ENR,TIM16);
      #if defined(STM32L476xx) || defined(STM32L496xx)
            PRINTIFON(APB2ENR,TIM17);
      #endif
            PRINTIFON(APB1ENR1,LPTIM1);
            PRINTIFON(APB1ENR2,LPTIM2);

            // Not sure what CPUs implement these
            PRINTIFON(AHB1ENR, CRC);
            PRINTIFON(AHB1ENR, TSC);
            PRINTIFON(AHB2ENR, RNG);
      #ifdef LCD_BASE
            PRINTIFON(APB1ENR1, LCD);
      #endif
            PRINTIFON(APB1ENR1, RTCAPB);
            PRINTIFON(APB1ENR1, WWDG);
            PRINTIFON(APB1ENR1, CRS);
            PRINTIFON(APB1ENR1, CAN1);
            PRINTIFON(APB1ENR1, PWR);
            PRINTIFON(APB1ENR1, OPAMP);
      #ifdef SWPMI1_BASE
            PRINTIFON(APB1ENR2, SWPMI1);
      #endif
            PRINTIFON(APB2ENR, SYSCFG);
            PRINTIFON(APB2ENR, FW);
      #if !defined(STM32L431xx)
            STDOUT.println("");
            STDOUT.print("VBUS: ");
            STDOUT.println(stm32l4_gpio_pin_read(GPIO_PIN_PB2));
            STDOUT.print("USBD connected: ");
            STDOUT.println(USBD_Connected());
      #endif
            return true;
    }

#endif //  ENABLE_DEVELOPER_COMMANDS

    if (!strcmp(cmd, "reset")) {
    #if defined(ARDUINO_ARCH_ESP32)  // ESP architecture
      ESP.restart();
    #else
      STM32.reset();
    #endif 
      STDOUT.println("Reset failed.");
      return true;
    }

    return false;
  }

  void Help() override {
#if defined(COMMANDS_HELP) 
    STDOUT.println(" version - show software version");
    STDOUT.println(" reset - restart software");
  #ifndef DISABLE_DIAGNOSTIC_COMMANDS    
    STDOUT.println(" effects - list current effects");
  #endif    
  #ifdef ENABLE_SERIALFLASH
    STDOUT.println("Serial Flash memory management:");
    STDOUT.println("   ls, rm <file>, format, play <file>, effects");
    STDOUT.println("To upload files: tar cf - files | uuencode x >/dev/ttyACM0");
  #endif
  #if defined(ENABLE_SD) && !defined(DISABLE_DIAGNOSTIC_COMMANDS)
    STDOUT.println(" dir [directory] - list files on SD card.");
    STDOUT.println(" sdtest - benchmark SD card");
  #endif
  #ifdef X_PROBECPU
    STDOUT.println(" top - report CPU usage and execution times");
  #endif
  #ifdef X_BROADCAST
    STDOUT.println(" broadcast on/off - enable/disable binary broadcasting");
  #endif
  #ifndef DISABLE_DIAGNOSTIC_COMMANDS
    STDOUT.println(" cod <filename> - list all entries in a .COD file");
  #endif
#else // COMMANDS_HELP
  STDOUT.println("For help on ASCII commands, visit http://git.ultraproffie/ASCII_Commands.md");
#endif // COMMANDS_HELP
  }
};

StaticWrapper<Commands> commands;

#include "common/serial.h"


#if defined(ENABLE_MOTION) || defined(ENABLE_SSD1306) || defined(INCLUDE_SSD1306)
#include "common/i2cdevice.h"
I2CBus i2cbus;
#endif


#ifdef ENABLE_MOTION

#include "motion/motion_util.h"
#include "motion/lsm6ds3h.h"


#ifdef GYRO_CLASS
// Can also be gyro+accel.
StaticWrapper<GYRO_CLASS> gyroscope;
  #ifdef ULTRAPROFFIE
void EnableMotion() {gyroscope->enabled = true; }
void DisableMotion() {gyroscope->enabled = false; }
#else // Stubs for ProffieBoard
  void EnableMotion() { }
  void DisableMotion() { }
#endif
#ifdef ARDUINO_ARCH_STM32L4   // STM architecture
  void motion_irq() {  gyroscope->Poll(); }  
#endif
#endif

#ifdef ACCEL_CLASS
StaticWrapper<ACCEL_CLASS> accelerometer;
#endif

#endif   // ENABLE_MOTION

#ifdef ULTRAPROFFIE
  #include "common/sd_card.h"   // amplifier and booster replaced by power manager
#else 
  #include "sound/amplifier.h"
  #include "common/sd_card.h"
  #include "common/booster.h"
#endif 



#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
bool PublishContent(const char* filename)
{

  FileReader myfileReader;
  bool result;
  LOCK_SD(true);
  result = myfileReader.Create(filename);
  if(result)
  { 
    char buffer[10];
    myfileReader.Write("board_info-START\n");
    myfileReader.Write("HARDWARE:\n");
    #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
    PROFFIE_HDID.printHWSN(&myfileReader);
    #endif
    myfileReader.Write("FIRMWARE:\n");
    myfileReader.Write("ProffieOS "); myfileReader.Write(version); myfileReader.Write("\n");
    myfileReader.Write("Compiled: "); myfileReader.Write(install_time); myfileReader.Write("\n");
    myfileReader.Write("Build for: "); myfileReader.Write(XSTR(HWVER)); myfileReader.Write("\n");

    sprintf(buffer, "%d", NUM_BLADES);
    myfileReader.Write("Prop: '"); myfileReader.Write(prop.name()); myfileReader.Write("'\n");
    myfileReader.Write("Supported blades: "); myfileReader.Write(buffer); myfileReader.Write("\n");
    myfileReader.Write("Diagnose: ");   
    #ifdef PF_STATUS_REPORT_ON
      myfileReader.Write("ON\n");
    #else
      myfileReader.Write("OFF\n");
    #endif
    myfileReader.Write("Developer mode: ");
    #ifdef ENABLE_DEVELOPER_MODE
      myfileReader.Write("ON\n");
    #else
      myfileReader.Write("OFF\n");
    #endif
    myfileReader.Write("board_info-END\n");
    myfileReader.Write("\n");

    prop.list_styles(&myfileReader); myfileReader.Write("\n");
    prop.list_fonts(&myfileReader);  myfileReader.Write("\n");
    prop.list_tracks(&myfileReader); myfileReader.Write("\n");
    prop.list_presets(&myfileReader); myfileReader.Write("\n");
    prop.list_profile(&myfileReader); myfileReader.Write("\n");
    LOCK_SD(true);
    if (userProfile.preset) sprintf(buffer, "%d", presets[userProfile.preset-1].variation);
    else sprintf(buffer, "%d", 0);
    myfileReader.Write("get_color-START\n");
    myfileReader.Write(buffer);  myfileReader.Write("\n");
    myfileReader.Write("get_color-END\n");
    myfileReader.Close();
    LOCK_SD(false);
    return true;
  }
  LOCK_SD(false);
  return false;
  
}
#include "common/FwUpdate.h"

bool BootInSafeMode()
{
  pinMode(auxPin, INPUT_PULLUP);
  delay(10); // wait a bit before start to read
  // uint32_t timeStamp, lastDebounce;
  // bool state = false;
  // int reading, btnState = 1, lastBtnState =1; 
  // timeStamp = millis();
  // lastDebounce = timeStamp;
  // while((millis() - timeStamp) < 100) {  // wait 50 ms to detect 
  //   reading = digitalRead(auxPin);
  //   if(reading != lastBtnState) lastDebounce = millis();
  //   if(millis() - lastDebounce > 40) 
  //   {
  //     if(reading != btnState) 
  //       btnState = reading;
  //   }
  //   lastBtnState = reading;
  // }
  // // STDOUT.print("PinState: "); STDOUT.println(digitalRead(auxPin));
  // if(btnState == 0) {
  if (digitalRead(auxPin) == LOW) {
    STDOUT.println("BOOTING IN SAFE MODE - NO SOUND AND LIGHTS! ");
    beeper.Beep(0.5, 1000);
    beeper.Beep(0.5, 2000);
    beeper.Beep(0.5, 5000);
    return true;
  }
   pinMode(auxPin, INPUT);
  return false;
}
#endif

void setup() {
#ifdef ARDUINO_ARCH_STM32L4   // STM architecture
#define SAVE_RCC(X) startup_##X = RCC->X
  SAVE_RCC(AHB1ENR);
  SAVE_RCC(AHB2ENR);
  SAVE_RCC(AHB3ENR);
  SAVE_RCC(APB1ENR1);
  SAVE_RCC(APB1ENR2);
  SAVE_RCC(APB2ENR);
#define SAVE_MODER(PORT, X) startup_MODER[X] = ((GPIO_TypeDef *)GPIO##PORT##_BASE)->MODER
  SAVE_MODER(A, 0);
  SAVE_MODER(B, 1);
  SAVE_MODER(C, 2);
  SAVE_MODER(H, 3);

  // TODO enable/disable as needed
  pinMode(boosterPin, OUTPUT);
  digitalWrite(boosterPin, HIGH);
#endif


#ifdef OSX_ENABLE_MTP
Serial.begin(SERIAL_ASCII_BAUD);
#else 
Serial.begin(9600);
#endif

//  Serial.begin(921600);  // 921600
//  Serial.begin(9600);

#if defined(PROFFIEBOARD) && defined(ENABLE_DEVELOPER_MODE)
  // #ifdef X_WAIT_FOR_SERIAL
    { // wait until we can use serial (up to 5 seconds if USB is not connected)
      uint32_t started_ = millis();
      while ( !Serial && (millis()-started_<5000) );   
    }
    if (Serial) { Serial.println(""); Serial.println(""); Serial.println(""); Serial.println("- serial port alive -"); }
  // #endif
#endif 


#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
  #if HWL_CONCAT(MQUOATE, HW_PREFIX, MQUOATE) == 'L'
    CheckFwUpdate();  // todo
  #endif 
#endif
if (Serial) { 
  Serial.println(""); Serial.println("");
  STDOUT.print(" - Welcome to ProffieOS ");
  STDOUT.print(version);  STDOUT.println(" -");
  STDOUT.print("Prop: '"); STDOUT.print(prop.name()); STDOUT.print("' supports ");
  STDOUT.print(NUM_BLADES);
  STDOUT.print(" blades. Diagnose is ");
  #ifdef PF_STATUS_REPORT_ON
    STDOUT.print("ON");
  #else
    STDOUT.print("OFF");
  #endif
  STDOUT.print(". Developer mode is ");
  #ifdef ENABLE_DEVELOPER_MODE
    STDOUT.print("ON");
  #else
    STDOUT.print("OFF");
  #endif
  STDOUT.println(".");
  STDOUT.println("");
  }


  // Wait for all voltages to settle.
  // Accumulate some entrypy while we wait.
  uint32_t now = millis();

  while (millis() - now < PROFFIEOS_STARTUP_DELAY) {
#ifndef NO_BATTERY_MONITOR  
    srand((rand() * 917823) ^ LSAnalogRead(batteryLevelPin));
#endif


  }

  // 1. Init memory
#ifdef ENABLE_SERIALFLASH
  SerialFlashChip::begin(serialFlashSelectPin);
#endif
#ifdef ENABLE_SD
  bool sd_card_found = LSFS::Begin();
  if (!sd_card_found) {
    if (sdCardSelectPin >= 0 && sdCardSelectPin < 255) {
      #if defined(DIAGNOSE_STORAGE)
      STDOUT.println("No "STORAGE_RES" found.");
      #endif
      pinMode(sdCardSelectPin, OUTPUT);
      digitalWrite(sdCardSelectPin, 0);
      delayMicroseconds(2);
      pinMode(sdCardSelectPin, INPUT);
      delayMicroseconds(2);
      if (digitalRead(sdCardSelectPin) != HIGH) {
        #if defined(DIAGNOSE_STORAGE)
        STDOUT.println("SD select not pulled high!");
        #endif
      }
    }
#ifdef ARDUINO_ARCH_STM32L4   // STM architecture

    stm32l4_gpio_pin_configure(GPIO_PIN_PA5,   (GPIO_PUPD_PULLUP | GPIO_OSPEED_HIGH | GPIO_MODE_INPUT));
    delayMicroseconds(10);
    if (!stm32l4_gpio_pin_read(GPIO_PIN_PA5)) {
      STDOUT.println("SCK won't go high!");
    }
    stm32l4_gpio_pin_configure(GPIO_PIN_PA5,   (GPIO_PUPD_PULLDOWN | GPIO_OSPEED_HIGH | GPIO_MODE_INPUT));
    delayMicroseconds(10);
    if (stm32l4_gpio_pin_read(GPIO_PIN_PA5)) {
      STDOUT.println("SCK won't go low!");
    }
#endif 
  } else {
    #if defined(DIAGNOSE_STORAGE)
    STDOUT.println(STORAGE_RES" found.");
    #endif
  }
#endif // ENABLE_SD

#ifdef PROFFIEBOARD
  ProffieBoard_InitCRC();   // Ugly patch, we need to get all board packages under the same JSON to get rid of this...
#endif

  // 2. Install configuration
  #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
  bool skip = BootInSafeMode();
  #else 
  bool skip = 0;
  #endif

  if(!skip)	
  {
    #ifdef DIAGNOSE_BOOT
      Install(INSTALL_FILE);
      STDOUT.println("");
    #else
      STDOUT.print("Running Install ... ");
      if (Install(INSTALL_FILE)) STDOUT.println("Success.");
      else STDOUT.println("FAILED!");
    #endif
  }


  // 3. User Profile
  if(!skip) {
    #ifdef DIAGNOSE_BOOT
        SetUserProfile(PROFILE_FILE);
    #else // DIAGNOSE_BOOT
      STDOUT.print("Running xProfile ... ");
      if (SetUserProfile(PROFILE_FILE)) STDOUT.println("Success.");
      else STDOUT.println("FAILED!");
    #endif // DIAGNOSE_BOOT

    // 4. Publish content
    #if defined(ULTRAPROFFIE) && ULTRAPROFFIE_VERSION == 'L'  
      #ifdef DIAGNOSE_BOOT    
        STDOUT.print ("Publishing content in "); STDOUT.print(OFFLINE_FILE); STDOUT.print(" ........................ ");
        if (PublishContent(OFFLINE_FILE)) STDOUT.println("Success.");
        else STDOUT.println("FAILED!");
      #else
        STDOUT.print("Publishing content ... ");
        if (PublishContent(OFFLINE_FILE)) STDOUT.println("Success.");
        else STDOUT.println("FAILED!");
      #endif
    #endif // ULTRAPROFFIE_VERSION

    STDOUT.println("");
    prop.ActivateBlades();
    prop.SetPreset(userProfile.preset, false);
  }     
    
      

  // 5. Setup
  Looper::DoSetup();    


  // 6. Signal boot
  SaberBase::DoBoot();
#if defined(ENABLE_SD) && defined(ENABLE_AUDIO)
  if (!sd_card_found) ProffieOSErrors::sd_card_not_found();
#endif // ENABLE_AUDIO && ENABLE_SD
  
}




void loop() {
  Looper::DoLoop();
}

#ifdef ARDUINO_ARCH_ESP32   // ESP architecture
#include "common/BlServerTest.h"
void setup2()
{
   BlServer.Setup();
}

void loop2()
{
 BlServer.Loop();
}
#endif


#define CONFIG_BOTTOM
#include CONFIG_FILE
#undef CONFIG_BOTTOM

#define PROFFIEOS_DEFINE_FUNCTION_STAGE
#include "common/errors.h"

