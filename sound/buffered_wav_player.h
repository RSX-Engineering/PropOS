#ifndef SOUND_BUFFERED_WAV_PLAYER_H
#define SOUND_BUFFERED_WAV_PLAYER_H

#include "playwav.h"
#include "volume_overlay.h"

class BufferedWavPlayer;
size_t WhatUnit(class BufferedWavPlayer* player);

#ifndef AUDIO_BUFFER_SIZE_BYTES
#define AUDIO_BUFFER_SIZE_BYTES 512
#endif


// Combines a WavPlayer and a BufferedAudioStream into a
// buffered wav player. When we start a new sample, we
// make sure to fill up the buffer before we start playing it.
// This minimizes latency while making sure to avoid any gaps.
class BufferedWavPlayer : public VolumeOverlay<BufferedAudioStream<AUDIO_BUFFER_SIZE_BYTES> > {
public:
  void Play(const char* filename) {
    MountSDCard();
    EnableAmplifier();
    pause_.set(true);
    clear();
    wav.Play(filename);
    SetStream(&wav);
    scheduleFillBuffer();
    pause_.set(false);
  }

  bool PlayInCurrentDir(const char* name) {
    #if defined(X_STATUS_REPORT_AUDIO)
    STDOUT << "Playing " << name << ", ";
    #endif
    for (const char* dir = current_directory; dir; dir = next_current_directory(dir)) {
      PathHelper full_name(dir, name);
      LOCK_SD(true);
      bool exists = LSFS::Exists(full_name);
      LOCK_SD(false);
      // Fill up audio buffers before we lock the SD again
      AudioStreamWork::scheduleFillBuffer();
      if (exists) {
	Play(full_name);
	return true;
      }
    }
    STDOUT << "(not found)\n";
    return false;
  }

  bool PlayInDir(const char* dir, const char* shortName, const char* ext) {
    PathHelper full_name(dir, shortName, ext);
    if (shortName) {  
      // #ifdef X_STATUS_REPORT_AUDIO
      //   STDOUT.print("[PlayInDir] Playing "); STDOUT.print(full_name); STDOUT.print(", ");
      // #endif
      LOCK_SD(true);
      bool exists = LSFS::Exists(full_name);
      LOCK_SD(false);
      // Fill up audio buffers before we lock the SD again
      AudioStreamWork::scheduleFillBuffer();
      if (exists) {
        Play(full_name);
        return true;
      }
    }
    STDOUT.print(full_name);
    STDOUT.println("(not found)");
    return false;
  }

// Looper task for repeating sounds at a time period longer than sound duration
// TODO: scrap this whole mess and put a propper wav player instead
class RepeatTask : public Looper {
public: 
  bool running;
  uint32_t longRepeatTime;
private: 
  uint32_t lastRepeatTime;
  BufferedWavPlayer* player;
  Effect* effect;
public:
  RepeatTask () : Looper(1000) { running = false; }  // schedule by looper at 1 ms
  
  const char* name() override { return "WAV Repeat Task"; }

  void Set(BufferedWavPlayer* player_, Effect* effect_, uint32_t repeatTime_) {
    player = player_;
    effect = effect_;
    if (!repeatTime_) repeatTime_ = 1;
    longRepeatTime = repeatTime_;
    lastRepeatTime = millis();    // more or less...
  }

  void Start() {  running = true; }

  void Stop() { running = false; }


  void Loop() override { 
    if (running) {
      uint32_t timeNow = millis();
      if (timeNow - lastRepeatTime >= longRepeatTime) {
          lastRepeatTime = timeNow;
          if (player)player->PlayOnce(effect);
      }
    }
  };

};

    RepeatTask* repeater;


  void UpdateSaberBaseSoundInfo() {
    SaberBase::sound_length = length();
    SaberBase::sound_number = current_file_id().GetFileNum();
  }

  void PlayOnce(Effect::FileID fileid, float start = 0.0) {  
    const Effect* effect = fileid.GetEffect();    
	MountSDCard();
    EnableAmplifier();
    set_volume_now(volume_target() * effect->GetVolume() / 100);
    // STDOUT << "unit = " << WhatUnit(this) << " vol = " << volume() << ", ";
    pause_.set(true);
    clear();
    ResetStopWhenZero();
    wav.PlayOnce(fileid, start);
    SetStream(&wav);
    // Fill up the buffer, if possible.
    while (!wav.eof() && space_available()) {
      scheduleFillBuffer();
    }
    pause_.set(false);
    if (SaberBase::sound_length == 0.0 && effect->GetFollowing() != effect) {
      UpdateSaberBaseSoundInfo();
    }
  }
  void PlayOnce(Effect* effect, float start = 0.0) override    // need to call this from volume_overlay
  {
    PlayOnce(effect->RandomFile(), start);
  }

  void Stop() override {
    // STDOUT.println("[BufferedWavPlayer.Stop]");
    pause_.set(true);
    wav.Stop();
    wav.Close();
    clear();
    PlayNext(0);
    repeatingEff = 0;   // signal nothing is repeating
  }


  void PlayNext(Effect* effect)  { wav.PlayNext(effect); }   // PlayLoop without the misleading name
  void QuickStop() { // For asynchronous replay...
    pause_.set(true);
    wav.Stop();
  }
  void ResetWav() override { wav.Reset();  }
  


  void CloseFiles() override { wav.Close(); }

  const char* Filename() const {
    return wav.Filename();
  }

  Effect::FileID current_file_id() const {
    return wav.current_file_id();
  }
  
  bool isPlaying() const {
    if (repeatingEff) return true;    // keep active if repeating
    return !pause_.get() && (wav.isPlaying() || buffered());
  }

  BufferedWavPlayer() : VolumeOverlay(),  pause_(true) { SetStream(&wav);  }
  

  
  // This makes a paused player report very little available space, which
  // means that it will be low priority for reading.
  size_t space_available() override {
    size_t ret = VolumeOverlay<BufferedAudioStream<AUDIO_BUFFER_SIZE_BYTES>>::space_available();
    if (pause_.get() && ret) ret = 2; // still slightly higher than FromFileStyle<>
    return ret;
  }

  int read(int16_t* dest, int to_read) override {
    if (pause_.get()) return 0;
    return VolumeOverlay<BufferedAudioStream<AUDIO_BUFFER_SIZE_BYTES> >::read(dest, to_read);
  }
  bool eof() const override {
    if (pause_.get()) return true;
    return VolumeOverlay<BufferedAudioStream<AUDIO_BUFFER_SIZE_BYTES> >::eof();
  }

  float length() const { return wav.length(); }
  float pos() const {
    return wav.pos() - buffered() * (1.0f / AUDIO_RATE);
  }


  const char* filename() const {
    return wav.filename();
  }

  void AddRef() { refs_++; }
  void SubRef() { refs_--; }
  bool Available() const { return refs_ == 0 && !isPlaying(); }
  uint32_t refs() const { return refs_; }

  void dump() {
    STDOUT << " pause=" << pause_.get()
	   << " buffered=" << buffered()
	   << " wav.isPlaying()=" << wav.isPlaying()
	   << "\n";
    wav.dump();
  }


    void Repeat(uint16_t msm1) { wav.SetRepeat(msm1); }

// soundID: -1 = random, 0 = same as before, >=1 = sound #'soundID'
// repeat: 0 = no repeat, 1 = loop, >1 = repeat at 'repeat' milliseconds
bool PlayEffect(Effect* soundEffect, int soundID=-1, uint16_t repeat=0, float volume=0) {
    // 1. Check we can play
    if (!soundEffect) return false;   // no effect
    int effFiles = soundEffect->files_found();
    if (!effFiles) return false; // no files

    // 2. Prepare data
    if (soundID < -1) soundID = -1;
    if (soundID > effFiles) soundID = effFiles;
    if (repeat>1) {
        repeat++;     // asjust 1ms if repeating at fixed time interval
        if (repeat<10) repeat = 10; // be reasonable!
    }

    // 3. Play sound at least once
    if (!pause_.get() && (wav.isPlaying() || buffered())) Stop(); // Stop whatever is playing 
    if (soundID>=1) soundEffect->Select(soundID-1);   // count from 1
    if (soundID==-1) soundEffect->Select(-1);         // random
    if (volume==0) reset_volume();     // restore volume, might have faded out previously
    else set_volume_now(volume);            // .. or set to specified value
    noInterrupts();
    // PlayOnce(soundEffect);
    
    // 3. Set repeat
    if (repeat) repeatingEff = soundEffect;  // store repeating effect for any type of repeat, so we can change rate later
    else repeatingEff = 0;
    if (repeater) repeater->Stop();           // stop long repeat task
    wav.delayedRepeat = 0;                    // assume no repeat
    wav.PlayNext(soundEffect);                // but assign an effect so SetRepeat knows what to work with
    if (!repeat || repeat==1) {  // No repeat or loop: apply now
        // if (repeater) repeater->Stop();        // stop repeat task, certainly not needed
        wav.SetRepeat(repeat);
    }
    else { // Timed repeat: apply after start reading the file, so we can know its length
        if (!repeater) repeater = new RepeatTask();       // prepare a repeat task now, just in case it's a long repeat  
        repeater->Set(this, soundEffect, repeat);
        soundEffect->SetFollowing(soundEffect);   // PlayOnce will set effect_ to soundEffect, the delayed SetRepeat will need it
        wav.delayedRepeat = repeat;               // loop() will call SetRepeat later, triggered by ISR. delayedRepeat holds the requested repeat time
        wav.longRepeat = &repeater->running;      // magic glue
    }
    interrupts();
    // if (!pause_ && (wav.isPlaying() || buffered())) FadeAndPlay(soundEffect); 
    // else PlayOnce(soundEffect);
    PlayOnce(soundEffect);

    return true;

  }

  // 0 = not repeating, 1 = short repeat, 2 = loop, 3 = long repeat
  // uint8_t Repeating() { return wav.Repeating(); }

  // 0: no repeat; 1: loop; >1: repeat at 'milli' milliseconds
  void ChangeRepeatTime(uint16_t milli) {
    // 1. Stop
    if (!repeatingEff) return;          // nothing to do, no repeat active 
    if (!milli) {
        // STDOUT.println("any repeat -> no repeat");
        // if (repeater) repeater->Stop();
        if (wav.longRepeat) *(wav.longRepeat) = 0;    //  stop repeater and prevent long repeat
        wav.shortRepeatTime = 0;  // prevent short repeat
        wav.effect_ = 0;          // prevent looping
        wav.longRepeat = 0;       // signal no repeating
        repeatingEff = 0;         
        return;
    }
    
    
    noInterrupts();
    uint32_t soundDuration = 1000 * length();
    // STDOUT.print("[ChangeRepeatTime] change to "); STDOUT.print(milli); STDOUT.print(", sound duration = "); STDOUT.print(soundDuration); STDOUT.print("[ms]: ");
    if (*(wav.longRepeat)) { // currently on long repeat 
        if (milli == 1 || milli == soundDuration) {
            // STDOUT.println("long -> loop");
            wav.effect_.set(repeatingEff);
            wav.effect_.get()->SetFollowing(repeatingEff);
            *(wav.longRepeat) = false ;    // stop RepeatTask
            // repeater->Stop();
        }
        else if (milli > soundDuration) {
            // STDOUT.println("long -> long");
            repeater->longRepeatTime = milli;
        }
        else {
            // STDOUT.println("long -> short");
            PlayEffect(repeatingEff, 0, milli);  // TODO: smooth transition
        }
    }
    else if (wav.shortRepeatTime) { // currently on short repeat
        if (milli == 1 || milli == soundDuration) {
            // STDOUT.println("short -> loop");
            wav.shortRepeatTime = 0;
        }
        else if (milli > soundDuration) {
            // STDOUT.println("short -> long");
            wav.shortRepeatTime = 0;    // stop short
            wav.effect_.get()->SetFollowing(0);
            wav.effect_.set(nullptr);            // prevent looping 
            if (!repeater) repeater = new RepeatTask();
            else repeater->Stop();
            repeater->Set(this, repeatingEff, milli);
            wav.longRepeat = &repeater->running;      // magic glue
            *(wav.longRepeat) = true;                 // also starts repeater
            // repeater->Start();
        }
        else {
            // STDOUT.println("short -> short");
            wav.shortRepeatTime = milli+1;
        }    
    }
    else { // currently looping
        if (milli == 1 || milli == soundDuration) {
            // STDOUT.println("loop -> loop, do nothing");
        }
        else if (milli > soundDuration) {
            // STDOUT.println("loop -> long");
            PlayEffect(repeatingEff, 0, milli);

        }
        else {
            // STDOUT.println("loop -> short");
            PlayEffect(repeatingEff, 0, milli);
        }
    }
    interrupts();
  }

  // Fade out volume over 'milli' [ms], then stop 
  void FadeAndStop(uint16_t speed = 125) {
    noInterrupts();
    if (repeater) repeater->Stop();
    wav.shortRepeatTime = 0;
    if(repeater)
    *(wav.longRepeat) = false;
    wav.PlayNext(0);  
    repeatingEff = 0;  
    set_speed(speed);
    VolumeOverlay::FadeAndStop();
    interrupts();
    // STDOUT.print(speed); STDOUT.print(": "); STDOUT.print(millis()); STDOUT.print("->");
  }


  private:
  Effect* repeatingEff;

  


private:
  uint32_t refs_ = 0;

  PlayWav wav;
  POAtomic<bool> pause_;
};



#endif
