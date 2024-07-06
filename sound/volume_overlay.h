#ifndef SOUND_VOLUME_OVERLAY_H
#define SOUND_VOLUME_OVERLAY_H

const uint32_t kVolumeShift = 14;
const uint32_t kMaxVolume = 1 << kVolumeShift;
const uint32_t kDefaultVolume = kMaxVolume / 2;
// 1 / 500 second for to change the volume. (2ms)
const uint32_t kDefaultSpeed = 500 * kMaxVolume / AUDIO_RATE;


template<class T>
class VolumeOverlay : public T {
public:
  VolumeOverlay() : volume_(kMaxVolume / 100), stop_when_zero_(false) {
    volume_.set(kDefaultVolume);
    volume_.set_target(kDefaultVolume);
    volume_.set_speed(kDefaultSpeed);
  }




   int read(int16_t* data, int elements) override {
    
    elements = T::read(data, elements);
    if (volume_.isConstant()) {
      int32_t mult = volume_.value();
      if (mult == kMaxVolume) { 
        // Do nothing
      } 
      else if (mult == 0) {   // volume at 0
        if (stop_when_zero_.get()) {
          stop_when_zero_.set(false);
          volume_.set_speed(kDefaultSpeed);
          T::Stop();    // stop stream
          Stop();     // stop player 
          reset_volume(); // don't leave it at 0, we reuse players
        }
        if (restart_when_zero_) {
          volume_.set_speed(kDefaultSpeed);
          reset_volume(); // don't leave it at 0, we reuse players       
          if (nextVolume) set_volume_now(nextVolume);   // 
          ResetWav(); // Don't alter repeating data, might wanna repeat whatever is starting
          PlayOnce(restart_when_zero_);
          restart_when_zero_ = 0;
        }
        for (int i = 0; i < elements; i++) data[i] = 0; 
      } 
      else { // volume at target
        for (int i = 0; i < elements; i++) {
          data[i] = clamptoi16((data[i] * mult) >> kVolumeShift);
        }
      }
    } 
    else { // volume not at target
      for (int i = 0; i < elements; i++) {
        int32_t v = data[i] * (int16_t)volume_.value();
        v >>= kVolumeShift;
        data[i] = clamptoi16(v);
      }
        volume_.advance();    
    }
    return elements;
  }

  

 

  float volume() {
    return volume_.value() * (1.0f / (1 << kVolumeShift));
  }
  int volume_target() {
    return volume_.target_;
  }
  void set_volume(int vol) {
    volume_.set_target(vol);
  }
  void set_volume_now(int vol) {
    volume_.set(vol);
    volume_.set_target(vol);
  }
  void reset_volume() {
    set_volume_now((int)kDefaultVolume);
    volume_.set_speed(kDefaultSpeed);
    stop_when_zero_.set(false);
  }
  void set_volume(float vol) {
    set_volume((int)(kDefaultVolume * vol));
  }
  void set_volume_now(float vol) {
    set_volume_now((int)(kDefaultVolume * vol));
  }
  void set_speed(uint32_t speed) {
    volume_.set_speed(speed);
  }
  void set_fade_time(float t) {   
    set_speed(std::max<int>(1, (int)(kMaxVolume / t / AUDIO_RATE)));
  }
  float fade_speed() const {
    return (kMaxVolume / (float)volume_.speed_) / AUDIO_RATE;
  }
  bool isOff() const {
    return volume_.isConstant() && volume_.value() == 0;
  }

  void FadeAndStop() {
    volume_.set_target(0);
    stop_when_zero_.set(true);
  }

  private:
    // bool restart_when_zero_ = false;
    Effect* restart_when_zero_ = 0;     // effect to start when current one fades out
    float nextVolume = 0;     // volume to be restored when starting new file (after fade)
  public:
    void FadeAndPlay(Effect* effect, float nVol = 0) {  // Fade out then restore volume and play whatever's in the pipeline
      volume_.set_target(0);
      restart_when_zero_ = effect;
      nextVolume = nVol;
    }

  
  virtual void ResetWav() {}  // Overloaded by BufferedWavPlayer to signal PlayWav that current sound ended at the end of fadeout
                              // It's all spaghetti anyway, so why not...
  virtual void Stop() {}      // Stop player (overloaded by BufferedWavPlayer)

  virtual void PlayOnce(Effect* effect, float start = 0.0) {}



  void ResetStopWhenZero() {
    stop_when_zero_.set(false);
  }

private:
  ClickAvoiderLin volume_;
  POAtomic<bool> stop_when_zero_;
};

#endif
