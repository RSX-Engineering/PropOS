#ifndef COMMON_LOOPER_H
#define COMMON_LOOPER_H

// Helper class for classses that needs to be called back from the Loop()
// function. Also provides a Setup() function.
class Looper;
Looper* loopers = NULL;
Looper* hf_loopers = NULL;
class Looper {
public:
  void Link() {
    scheduled_time_ = 0;
    next_looper_ = loopers;
    loopers = this;
  }
  void Link(uint32_t schmicros) {
    scheduled_time_ = schmicros;
    next_looper_ = loopers;
    loopers = this;
  }
  void Unlink() {
    scheduled_time_ = 0;
    for (Looper** i = &loopers; *i; i = &(*i)->next_looper_) {
      if (*i == this) {
        *i = next_looper_;
        return;
      }
    }
  }
  Looper() { Link();  }   // link at highest priority
  Looper(uint32_t schmicros) { Link(schmicros);  } // link and schedule at callmicros [us]
  explicit Looper(NoLink _) { }
  explicit Looper(HFLink _) { Link(); } // High-Frequency Loopers are discarded
  ~Looper() { Unlink(); }
  static void DoLoop() {
    uint32_t microsNow = micros();       // current time;
    for (Looper *l = loopers; l; l = l->next_looper_) {
      if (microsNow - l->cpu_probe_.micros >= l->scheduled_time_) {
        ScopedCycleCounter cc(l->cpu_probe_);     // updates .cpu_probe_.micros (and CPU probe if enabled)
        l->Loop();                                // run .Loop() for this task
        microsNow = micros();                     // update current loop time (changed if .Loop() run)
      }
    }
    // char * stack_ptr = (char*)__get_MSP();
    // STDOUT.print(" * Temporary RAM (stack): "); STDOUT.println((uint32_t)(0x2000C000) - (uint32_t)(stack_ptr) );

  }
  
  static void DoSetup() {
    for (Looper *l = loopers; l; l = l->next_looper_) {
      l->Setup();
    }
  }
  static void DoProbe(DoWhatToProbe what) {
    for (Looper *l = loopers; l; l = l->next_looper_) 
      if (what==DoWhatToProbe::reset_probe)
        l->cpu_probe_.Reset();        
      else {
        l->cpu_probe_.Print(what);    
        STDOUT.print(" ");        
        STDOUT.println(l->name());
      }
  }
protected:
  virtual const char* name() = 0;
  virtual void Loop() = 0;
  virtual void Setup() {}
private:
  CPUprobe cpu_probe_;        // X_PROBECPU defined: monitors execution time, call frequency and cpu usage
  uint32_t scheduled_time_; // call period, in microsecond. .Loop() will be called on time intervals >= scheduled_time_
  Looper* next_looper_;
};

#endif
