#ifdef ENABLE_AUDIO

// DMA-driven audio output.
#ifdef ARDUINO_ARCH_ESP32   // ESP architecture
#define AUDIO_BUFFER_SIZE 128	
#else
#define AUDIO_BUFFER_SIZE 44
#endif
#define AUDIO_RATE 44100
#define NUM_WAV_PLAYERS 8



#include "talkie.h"

#include "click_avoider_lin.h"
#include "audiostream.h"
#ifdef ULTRAPROFFIE
  #include "../common/PowerManager.h" // need this before dynamic_mixer, but moving upwards will cause the linker to fail, so we'll just keep it messy like everything else
#endif

#include "dynamic_mixer.h"
#ifdef ARDUINO_ARCH_ESP32   // ESP architecture
#include "dac_os.h"
#else
#include "dac.h"
#endif
#include "beeper.h"




Beeper beeper;
Talkie talkie;

#include "buffered_audio_stream.h"

size_t WhatUnit(class BufferedWavPlayer* player);

#include "effect.h"
#include "buffered_wav_player.h"

BufferedWavPlayer wav_players[NUM_WAV_PLAYERS];
RefPtr<BufferedWavPlayer> track_player_;

RefPtr<BufferedWavPlayer> GetFreeWavPlayer()  {
  // Find a free wave playback unit.
  for (size_t unit = 0; unit < NELEM(wav_players); unit++) {
    if (wav_players[unit].Available()) {
      wav_players[unit].reset_volume();
      return RefPtr<BufferedWavPlayer>(wav_players + unit);
    }
  }
  return RefPtr<BufferedWavPlayer>();
}

RefPtr<BufferedWavPlayer> GetWavPlayerPlaying(Effect* effect) {
  for (size_t unit = 0; unit < NELEM(wav_players); unit++) {
    if (wav_players[unit].isPlaying() &&
	wav_players[unit].current_file_id().GetEffect() == effect) {
      return RefPtr<BufferedWavPlayer>(wav_players + unit);
    }
  }
  return RefPtr<BufferedWavPlayer>();
}

RefPtr<BufferedWavPlayer> RequireFreeWavPlayer()  {
  while (true) {
    RefPtr<BufferedWavPlayer> ret = GetFreeWavPlayer();
    if (ret) return ret;
    STDOUT.println("Failed to get hum player, trying again!");
    delay(100);
  }
}

size_t WhatUnit(class BufferedWavPlayer* player) {
  if (!player) return -1;
  return player - wav_players;
}

void SetupStandardAudioLow() {
//  audio_splicer.Deactivate();
  for (size_t i = 0; i < NELEM(wav_players); i++) {
    if (wav_players[i].refs() != 0) {
      STDOUT.println("WARNING, wav player still referenced!");
    }
    dynamic_mixer.streams_[i] = wav_players + i;
    wav_players[i].reset_volume();
  }
  dynamic_mixer.streams_[NELEM(wav_players)] = &beeper;
  dynamic_mixer.streams_[NELEM(wav_players)+1] = &talkie;
}

void SetupStandardAudio() {
#ifdef ARDUINO_ARCH_ESP32   // ESP architecture
  SetupStandardAudioLow();
  dac_OS.SetStream(&dynamic_mixer);
#else
  dac.SetStream(NULL);
  SetupStandardAudioLow();
  dac.SetStream(&dynamic_mixer);
#endif
}

uint8_t GetNrOFPlayingPlayers(bool excludeTrack = false)
{   
  uint8_t nrPlaying = 0;
  for (size_t i = 0; i < NELEM(wav_players); i++) {
    if (wav_players[i].isPlaying() ) {
      nrPlaying++;
    }
  }
  if(excludeTrack) {
    if(track_player_ && nrPlaying) nrPlaying--; // nr of player excluding track player 
  }

  return nrPlaying;
}
#ifdef ULTRAPROFFIE
  inline void EnableAmplifier() { 
    #ifdef ARDUINO_ARCH_ESP32   // ESP architecture
    dac_OS.RequestPower();
    #else
    dac.RequestPower();
    #endif 
    }

  bool SoundActive() {
    if (!dynamic_mixer.get_volume()) return false;    // muted
    for (size_t i = 0; i < NELEM(wav_players); i++)
      if (wav_players[i].isPlaying())
        return true;
    if (beeper.isPlaying()) return true;
    if (talkie.isPlaying()) return true;
    return false;
  } 
  const auto AmplifierIsActive = SoundActive;   

  // Stubs (for backward compatibility only)
  void EnableBooster() {}
  void SilentEnableBooster(bool on) {}
  void SilentEnableAmplifier(bool on) {}


#endif 


#include "../common/config_file.h"
#include "hybrid_font.h"

HybridFont hybrid_font;

#include "smooth_swing_config.h"
#include "smooth_swing_cfx_config.h"
#include "looped_swing_wrapper.h"
#include "smooth_swing_v2.h"

LoopedSwingWrapper looped_swing_wrapper;
SmoothSwingV2 smooth_swing_v2;

#else  // ENABLE_AUDIO

#define LOCK_SD(X) do { } while(0)
#include "../common/sd_card.h"

#endif  // ENABLE_AUDIO
