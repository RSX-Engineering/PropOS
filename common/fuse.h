#ifndef COMMON_FUSE_H
#define COMMON_FUSE_H

#include "vec3.h"
#include "quat.h"
#include "saber_base.h"
#include "extrapolator.h"
#include "box_filter.h"

// #define FUSE_SPEED

#ifndef ACCEL_MEASUREMENTS_PER_SECOND
#define ACCEL_MEASUREMENTS_PER_SECOND 800
#endif
#ifndef GYRO_MEASUREMENTS_PER_SECOND
#define GYRO_MEASUREMENTS_PER_SECOND 800
#endif

#if 1 // def DEBUG

#if 0
static bool my_isnan(float val) {
  if (val < 10000000 && val > -1000000) return false;
  return true;
}
#else
static bool my_isnan(float val) {
  // Returns true for +/- infinit as well.
  union { uint32_t i; float v; } tmp;
  tmp.v = val;
  return (tmp.i & 0x7f800000UL) == 0x7f800000UL;
}
#endif

static bool my_isnan(Vec3 v) {
  return my_isnan(v.x) || my_isnan(v.y) || my_isnan(v.z);
}
static bool my_isnan(Quat q) {
  return my_isnan(q.w_) || my_isnan(q.v_);
}

int nan_count = 0;

#define CHECK_NAN(X) do {                       \
 if (nan_count < 20 && my_isnan((X))) {         \
    nan_count++;                                \
    STDOUT << "NAN " << #X << " = " << (X) << " LINE " << __LINE__ << "\n"; \
  }                                             \
}while(0)

#else

#define CHECK_NAN(X) do { } while(0)

#endif

float exp2_fast(float x) {
  int i = floorf(x);
  x -= i;
  return ldexpf(-8.24264/(x-3.41421)-1.41421356237, i);
}


class Fusor : public Looper {
public:
  Fusor() :
#ifdef FUSE_SPEED
  speed_(0.0),
#endif
    down_(0.0), last_micros_(0) {
  }
  const char* name() override { return "Fusor"; }
  void DoMotion(const Vec3& gyro, bool clear) {
    CHECK_NAN(gyro);
    if (clear) {
      gyro_extrapolator_.clear(gyro);
      gyro_clash_filter_.clear(gyro);
    } else {
      gyro_extrapolator_.push(gyro);
      gyro_clash_filter_.push(gyro);
    }
  }
  void DoAccel(const Vec3& accel, bool clear) {
    CHECK_NAN(accel);
    if (clear) {
      accel_extrapolator_.clear(accel);
      accel_clash_filter_.clear(accel);
      down_ = accel;
      last_clear_ = micros();
    } else {
      accel_extrapolator_.push(accel);
      accel_clash_filter_.push(accel);
    }
  }

#ifndef GYRO_STABILIZATION_TIME_MS
#define GYRO_STABILIZATION_TIME_MS 64
#endif

  void Loop() override {
    uint32_t last_accel = accel_extrapolator_.last_time();
    uint32_t last_gyro = gyro_extrapolator_.last_time();
    uint32_t now = micros();
    if (!accel_extrapolator_.ready() ||
	!gyro_extrapolator_.ready() ||
	now - last_accel > 200000 ||
	now - last_gyro > 200000 ||
	now - last_clear_ < GYRO_STABILIZATION_TIME_MS * 1000) {
      gyro_ = Vec3(0.0f);
      accel_ = Vec3(0.0f);
      swing_speed_ = 0.0f;
      return;
    }

    float delta_t = (now - last_micros_) / 1000000.0;
    last_micros_ = now;
    // Update last_clear_ so we won't have wrap-around issues.
    last_clear_ = now - GYRO_STABILIZATION_TIME_MS * 1000;
    CHECK_NAN(delta_t);

    accel_ = accel_extrapolator_.get(now);
    CHECK_NAN(accel_);
    gyro_ = gyro_extrapolator_.get(now);
    CHECK_NAN(gyro_);
    swing_speed_ = -1.0f;
    angle1_ = 1000.0f;
    angle2_ = 1000.0f;

    
    Quat rotation = Quat(1.0, gyro_ * -(std::min(delta_t, 0.01f) * M_PI / 180.0 / 2.0)).normalize();
    CHECK_NAN(rotation);
    down_ = rotation.rotate_normalized(down_);
    CHECK_NAN(down_);

#ifdef FUSE_SPEED
    speed_ = rotation.rotate_normalized(speed_);
//    speed_ = rotation * speed_;
    CHECK_NAN(speed_);
#endif

    #define G_constant 9.80665

    float wGyro = 1.0;
    CHECK_NAN(wGyro);
    // High gyro speed means trust acceleration less.
    wGyro += gyro_.len() / 100.0;
    CHECK_NAN(wGyro);

    // If acceleration is not near "down", don't trust it.
    //wGyro += (accel_ - down_).len2();

    // If acceleration is not 1.0G, don't trust it.
    wGyro += fabsf(accel_.len() - 1.0f) * 50.0;

    // If acceleration is changing rapidly, don't trust it.
    wGyro += accel_extrapolator_.slope().len() * 1000;
    CHECK_NAN(wGyro);

    mss_ = (accel_ - down_) * G_constant; // change unit from G to m/s/s
    CHECK_NAN(mss_);

#ifdef FUSE_SPEED
    speed_ += mss_ * std::min(delta_t, 0.01f);
    CHECK_NAN(speed_);

    // FIXME?
    // Speed higher than 100m/s? How about "no"
    if (speed_.len() > 100.0) {
      speed_ *= 100.0 / speed_.len();
    }

#if 1
    speed_ = speed_.MTZ(0.2 * delta_t);
#else
    float delta_factor = powf(0.75, delta_t);
    CHECK_NAN(delta_factor);
    speed_ = speed_ * delta_factor;
    CHECK_NAN(speed_.x);
#endif
#endif
    // goes towards 1.0 when moving.
    // float gyro_factor = powf(0.01, delta_t / wGyro);
    // float gyro_factor = expf(logf(0.01) * delta_t / wGyro);
    float gyro_factor = expf(logf(0.01) * delta_t / wGyro);
    CHECK_NAN(gyro_factor);
    down_ = down_ *  gyro_factor + accel_ * (1.0 - gyro_factor);
    // Might be a good idea to normalize down_, but then
    // down_ and accel_ might be slightly different length,
    // so we would need to use MTZ() on mss_.
    CHECK_NAN(down_.x);

    UpdateTheta(now);  // calculate twist_angle
    UpdateSlide(now); // calculate slide speed

  #if defined(X_BROADCAST) && defined(BROADCAST_MOTION)
      struct {
          int16_t acc_x100;     // 100 * X acceleration [g]
          int16_t acc_y100;     // 100 * Y acceleration [g]
          int16_t acc_z100;     // 100 * Z acceleration [g]
          int16_t gyro_x100;    // 100 * X angular velicity [dps]
          int16_t gyro_y100;    // 100 * Y angular velicity [dps]
          int16_t gyro_z100;    // 100 * Z angular velicity [dps]
      } ch1_broadcast;
      ch1_broadcast.acc_x100 = (int16_t)(100*accel_extrapolator_.get().x);
      ch1_broadcast.acc_y100 = (int16_t)(100*accel_extrapolator_.get().y);
      ch1_broadcast.acc_z100 = (int16_t)(100*accel_extrapolator_.get().z);
      ch1_broadcast.gyro_x100 = (int16_t)(100*gyro_extrapolator_.get().x);
      ch1_broadcast.gyro_y100 = (int16_t)(100*gyro_extrapolator_.get().y);
      ch1_broadcast.gyro_z100 = (int16_t)(100*gyro_extrapolator_.get().z);
      STDOUT.Broadcast(1, &ch1_broadcast, sizeof(ch1_broadcast));
  #endif // BROADCAST_MOTION
  } 

  // RELATIVE TWIST ANGLE (theta)
  #define THETA_SR      100    // twist detection sample rate [Hz] (never constant!)
  #define THETA_NOISETH userProfile.menuSensitivity.thetaThreshold    // angular threshold to begin integration
  #define THETA_STILLTH  10    // angular threshold for still (keep it low to allow small gaps)
  void UpdateTheta(uint32_t microsNow) {
    static uint32_t lastMicros = 0;     // keep constant rate
    if(!microsNow) {  // reset
      theta_ = 0;  
      theta_peak_ = 0;  
    }
    if ((microsNow - lastMicros) < 1000000/THETA_SR) return; // keep constant rate
    uint16_t absGyro = abs(gyro_.x);     
    if (absGyro >= THETA_NOISETH) { // integrate only if above noise threshold    
        float dT = microsNow - lastMicros; 
        dT /= 1000000;
        theta_ += gyro_.x * dT;    
        if (absGyro > theta_peak_) theta_peak_ = absGyro;
    }
    else if (absGyro < THETA_STILLTH) {
            theta_ = 0;   // reset while still
            theta_peak_ = 0;  
        }
    lastMicros = microsNow;
  }

// LONGITUDINAL SLIDE (stab) SPEED AND DISTANCE
#define SLIDE_THRES   2.5f         // Acceleration threshold to start integrating for slide
#define SLIDE_NOISETH 1.0f         // Noise threshold for zero-crossing and end of integration
#define SLIDE_MAXZERO 100000       // Maximum time [us] allowed for zero-crossing, otherwise it's not a slide
#define SLIDE_MINPEAK 50000        // Minimum time [us] for 1st peak, otherwise it's probably a clash
#define SLIDE_PAUSE   200000       // Pause [us] after integration ended
void UpdateSlide(uint32_t microsNow) {    
  static enum : int8_t {   
      Off = 0,          // not active 
      Peak1 = 1,         // Acceleration above threshold 
      Zero = 2,         // Zero crossing towards 2nd peak 
      Peak2 = 3,         // Zero crossed, opposite peak expected
      Pause = 4,         // Pause after integration ended
      CancelPause = -1  // Pause after cancel (different value just to distinguish it in broadcast)
  } slideState = Off;
    
  static uint32_t lastMicros = 0;   // keep constant rate
  static float slideSpeed = 0;      // speed = integrated acceleration
  static float slideDistance = 0;  // distance = integrated speed
  static uint32_t measuredTime = 0;
  static bool zeroSpeed = false;  // if true report speed as zero until it actually becomes zero, to prevent aftereffects


  if (!microsNow) { // reset
    slideSpeed = 0;
    slideSpeed_ = 0;
    slideDistance = 0;
    slideDistance_ = 0;
    slideState = Off;
    return;
  }

  // uint32_t microsNow = micros();
  if ((microsNow - lastMicros) < 1000) return; // keep constant rate

  // float mss = mss_.x;
  float dT = microsNow - lastMicros; 
  dT /= 1000000;

  switch (slideState) {
      case Off: // not active
          if (mss_.x >= SLIDE_THRES) { // above integration threshold, but only on forward slide
            if (abs(mss_.y)-abs(mss_.x) > SLIDE_NOISETH || abs(mss_.z)-abs(mss_.x) > SLIDE_NOISETH) {             
              // cancel if X acceleration is not clearly the highest, it's not a slide
              slideState = CancelPause;
              measuredTime = microsNow;              
            }
            else { // start integrating (next iteraion)
              slideState = Peak1;
              measuredTime = microsNow;     
            }
          }
      break;

      case Peak1: // acceleration above threshold, integrate
          slideSpeed += mss_.x * dT;  // integrate for speed
          slideDistance += slideSpeed_ * dT;  // integrate for distance
          if (mss_.x < SLIDE_NOISETH) {  // end of 1st peak            
            if (microsNow - measuredTime  < SLIDE_MINPEAK) {  // peak too short, it's probably noise
              slideState = CancelPause;
              slideSpeed = 0;
              slideDistance =0;  
              measuredTime = microsNow;
            }
            else { // switch to zero crossing state 
              slideState = Zero;    
              measuredTime = microsNow;  // start counting zero crossing time
            }      
          }         
      break;

      case Zero: // acceleration crossing zero towards the oposite peak        
          if ( mss_.x <= -SLIDE_NOISETH) {   
            // switch to 2nd peak state if above threshold on opposite side of zero
            slideState = Peak2;
            measuredTime = 0;
          }
          else if (microsNow - measuredTime > SLIDE_MAXZERO) {  // too long zero time, reset speed and pause
            slideState = CancelPause;
            slideSpeed = 0;
            slideDistance =0;  
            measuredTime = microsNow;
          }
          else {  // still zero crossing, integrate           
            slideSpeed += mss_.x * dT;  // integrate for speed
            slideDistance += slideSpeed_ * dT;  // integrate for distance
          }
      break;

      case Peak2: // second peak after zero crossing
          if (mss_.x < -SLIDE_NOISETH) {
            slideSpeed += mss_.x * dT; // continue integrating until below noise threshold again
            slideDistance += slideSpeed_ * dT;  // integrate for distance
            slideDistance_ = slideDistance;  // publish distance
          }
          else { // end of integration, pause for a while
            slideState = Pause;
            slideSpeed = 0;
            measuredTime = microsNow;
          }
      break;

      case Pause: // pause integration
      case CancelPause:
          slideDistance_ = slideDistance;  // publish distance
          if (microsNow - measuredTime > SLIDE_PAUSE) {  // pause ended, reset
            slideState = Off;
            measuredTime = 0;
            slideDistance = 0;
            slideDistance_ = 0;
          }
  
  }    
  lastMicros = microsNow;
  
  if (slideSpeed >= 0) slideSpeed_ = slideSpeed;
  else slideSpeed_ = 0;  // report zero speed;


  #if defined(X_BROADCAST) && defined(BROADCAST_SLIDE)
        struct {
            int16_t up1=0, up2=0, up3=0;
            int16_t down1=0, down2=0, down3=0;
        } ch1_broadcast;
        ch1_broadcast.up1 = (int16_t)(100*mss_.x);
        ch1_broadcast.up2 = (int16_t)(100*mss_.y);
        ch1_broadcast.up3 = (int16_t)(100*mss_.z);        
        ch1_broadcast.down1 = (int16_t)(slideState);
        ch1_broadcast.down2 = (int16_t)(100*slideSpeed);
        ch1_broadcast.down3 = (int16_t)(100*slideDistance);

        STDOUT.Broadcast(1, &ch1_broadcast, sizeof(ch1_broadcast));        
  #endif // X_BROADCAST   

  }

  bool freefall() const {
    // TODO: Cancel out centripital force?
    return accel_.len2() < 0.1;
  }

  // PI/2 = straight up, -PI/2 = straight down
  float angle1() {
    // use orientation!
    if (angle1_ == 1000.0f) {
      angle1_ = atan2f(down_.x, sqrtf(down_.z * down_.z + down_.y * down_.y));
    }
    return angle1_;
  }

  // Twist angle.
  // Note: Twist speed is simply gyro().z!
  float angle2() {
    if (angle2_ == 1000.0f) {
      angle2_ = atan2f(down_.y, down_.z);
    }
    return angle2_;
  }

  // 0 = up, +/-PI = down, PI/2 = left, -PI/2 = right
  float pov_angle() {
    return atan2f(down_.y, down_.x);
  }

  float swing_speed() {
    if (swing_speed_ < 0) {
      swing_speed_ = sqrtf(gyro_.z * gyro_.z + gyro_.y * gyro_.y);
      // swing_speed_ += 0.1 * ( sqrtf(gyro_.z * gyro_.z + gyro_.y * gyro_.y) - swing_speed_);   // moving average filter  
      if (nan_count < 20 && my_isnan(swing_speed_)) {
        nan_count++;
        STDOUT << "\nNAN swing_speed_ " << gyro_;
      }
    }
    return swing_speed_;
  }


  Vec3 gyro() { return gyro_; }    // degrees/s
  float theta() { return theta_; }    // twist relative angle
  uint32_t thetaPeak() { return theta_peak_; }  // maximum gyro during current theta integration
  float slideSpeed() { return slideSpeed_; }  // slide speed
  float slideDistance() { return slideDistance_; }  // slide distance
  void SetTheta(float t) { theta_ = t; }
  void SetThetaPeak(uint32_t tp) { theta_peak_ = tp; }
  Vec3 gyro_slope() {
    // degrees per second per second
    return gyro_extrapolator_.slope() * 1000000;
  }
  Vec3 accel() { return accel_; }  // G/s/s
  Vec3 mss() { return mss_; }      // m/s/s (acceleration - down vector)
  Vec3 down() { return down_; }    // G/s/s (length should be close to 1.0)

  // Meters per second per second
  Vec3 clash_mss() {
    return accel_clash_filter_.get() - down_;
  }

  // Meters per second per second
  float gyro_clash_value() {   
    // degrees per microsecond
    float v = (gyro_clash_filter_.get() - gyro_extrapolator_.get(micros())).len();
    // Translate into meters per second per second, assuming blade is one meter.
    return v / 9.81;
  }
  
#ifdef FUSE_SPEED
  Vec3 speed() { return speed_; }  // m/s
#endif

  // Acceleration into swing in radians per second per second
  float swing_accel() {
    Vec3 gyro_slope_ = gyro_slope();
    return sqrtf(gyro_slope_.z * gyro_slope_.z + gyro_slope_.y * gyro_slope_.y) * (M_PI / 180);
  }

  // Acceleration into twist (one direction) in degrees per second per second
  float twist_accel() {
    return gyro_slope().x;
  }

  void dump() {
    STDOUT << " Accel=" << accel_ << " ("<<  accel_.len() << ")"
	   << " Gyro=" << gyro_ << " (" << gyro_.len() << ")"
	   << " down=" << down_ << " (" << down_.len() << ")"
	   << " mss=" << mss_  << " (" << mss_.len() << ")"
	   << "\n";
    STDOUT << " ready=" << ready()
	   << " swing speed=" << swing_speed()
	   << " gyro slope=" << gyro_slope().len()
	   << " last_micros_ = " << last_micros_
	   << " now = " << micros()
	   << "\n";
    STDOUT << " acceleration extrapolator data:\n";
    accel_extrapolator_.dump();
    STDOUT << " gyro extrapolator data:\n";
    gyro_extrapolator_.dump();
  }

  bool ready() { return micros() - last_micros_ < 50000; }

private:
  uint32_t last_clear_ = 0;
  static const int filter_hz = 80;
  static const int clash_filter_hz = 1600;
  Extrapolator<Vec3, ACCEL_MEASUREMENTS_PER_SECOND/filter_hz> accel_extrapolator_;
  Extrapolator<Vec3, GYRO_MEASUREMENTS_PER_SECOND/filter_hz> gyro_extrapolator_;
  BoxFilter<Vec3, ACCEL_MEASUREMENTS_PER_SECOND/clash_filter_hz> accel_clash_filter_;
  BoxFilter<Vec3, ACCEL_MEASUREMENTS_PER_SECOND/clash_filter_hz> gyro_clash_filter_;

#ifdef FUSE_SPEED
  Vec3 speed_;
#endif
  Vec3 down_;
  Vec3 mss_;
  uint32_t last_micros_;
  Vec3 accel_;
  Vec3 gyro_;
  float swing_speed_;
  float angle1_;
  float angle2_;
  float theta_;            // twist relative angle
  float slideSpeed_;       // instantaneous slide speed
  float slideDistance_;     // slide distance:  - updated when slide is confirmed (might seem to start like a slide but proof to be vibrations)
                            //                  - cleared after post-detection pause
  uint16_t theta_peak_;    // maximum abs(gyro) during theta integration
    
};

Fusor fusor;

// For debugging
template<class T, int N>
class PeakPrinter {
public:
  void Add(T v, bool print) {
    for (int i = 0; i < N-1; i++) tmp_[i] = tmp_[i+1];
    tmp_[N-1] = v;
    if (print)  {
      HighPeak();
      LowPeak();
    }
  }
  void HighPeak() {
    for (int i = 0; i < N-1; i++) {
      if (tmp_[i] > tmp_[N/2]) return;
    }
    STDOUT << " HIGHPEAK: " << tmp_[N/2] << "\n";
  }
  void LowPeak() {
    for (int i = 0; i < N-1; i++) {
      if (tmp_[i] < tmp_[N/2]) return;
    }
    STDOUT << " LOWPEAK: " << tmp_[N/2] << "\n";
  }
private:
  T tmp_[N];
};

#endif
