#ifndef COMMON_STDOUT_H
#define COMMON_STDOUT_H


#ifdef PROFFIE_TEST
struct Print {
  void print(const char* s) { write((const uint8_t*)s, strlen(s)); }
  void print(float v) {
    char tmp[64];
    sprintf(tmp, "%f", v);
    print(tmp);
  }
  void print(int v, int base) {
    char tmp[64];
    sprintf(tmp, "%d", v);
    print(tmp);
  }
  void write(char s) { write( (uint8_t) s); }
  template<class T>
  void println(T s) { print(s); write('\n'); }
  template<class T>
  void println(T s, int base) { print(s); write('\n'); }
  virtual size_t write(uint8_t s) { putchar(s); return 1; }
  virtual size_t write(const uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) write(buffer[i]);
    return size;
  }

};
#endif

extern Print* default_output;
extern Print* stdout_output;

template<typename T, typename X = void> struct PrintHelper {
  static void out(Print& p, T& x) { p.print(x); }
};

template<typename T> 
struct PrintHelper<T, decltype(((T*)0)->printTo(*(Print*)0))> {
  static void out(Print& p, T& x) { x.printTo(p); }
};


#include "SerialStub.h"
class ConsoleHelper : public Print {
private:
  bool broadcast = false;    // true if broadcasting binary
  uint32_t lastMillis;
public:
  size_t write(uint8_t b) override {
    size_t ret = stdout_output->write(b);
    return ret;
  }
  size_t write(const uint8_t *buffer, size_t size) override {
    size_t ret = stdout_output->write(buffer, size);
    return ret;
  }

 #ifdef ULTRAPROFFIE
      void flushTx()
      {
        Serial.flush();
      }
      void flushRx()
      {
        uint32_t timeStamp = millis();
        while(Serial.available() || (millis() - timeStamp < 10) )
            Serial.read();
      }

  #endif 

  void Silent(bool newState) {
    if (newState) { // 
      default_output = &EmptySerial;                
      stdout_output = &EmptySerial; 
    }
    else {
      default_output = &Serial;    // Resume normal STDOUT printing            
      stdout_output = &Serial;
    }
  }
#ifdef X_BROADCAST
  void StartBroadcast() {
      Serial.println(" Start broadcasting...");
      Serial.flush();
      Serial.begin(SERIAL_BIN_BAUD);    // bin speed for broadcasting
      default_output = &EmptySerial;                
      stdout_output = &EmptySerial; 
      broadcast = true;   
      lastMillis = millis();
  }

  void StopBroadcast() {
      Serial.flush();
      Serial.begin(SERIAL_ASCII_BAUD);  // ascii speed for regular STDOUT
      default_output = &Serial;    // Resume normal STDOUT printing            
      stdout_output = &Serial;   
      broadcast = false;    
  }

  bool Broadcasting() {   return broadcast;   }
  
  size_t Broadcast(uint8_t channel, void* buffer, uint16_t size) { //  __attribute__((optimize("Og")))  {
    if (!Broadcasting()) return 0;          // silence, we're NOT broadcasting
    if (!channel) return 0;                 // Don't broadcast on channel 0
    uint32_t timeNow = millis();
    // uint32_t timeNow = 10*millis(); timeNow = timeNow / 7;    // adjust for systick bug
    if (timeNow-lastMillis < 10) return 0;  // Limit frequency to 100 Hz   // TODO: set frequency per channel
    // if (timeNow-lastMillis < 5) return 0;  // Limit frequency to 200 Hz
    lastMillis = timeNow;
    #ifdef OBSIDIANFORMAT 
      size_t ret = Serial.write((uint8_t*)buffer, size);
      ret += Serial.write((uint8_t*)(&timeNow), 4);

    // size_t ret = Serial.println(timeNow);
    #else
      size_t ret = Serial.write(channel);   
      ret += Serial.write((uint8_t*)(&timeNow), 4);
      ret += Serial.write((uint8_t*)(&size), 2);
      ret += Serial.write((uint8_t*)buffer, size);
    #endif
    return ret;
  }
#endif // X_BROADCAST

  template<typename T>
  ConsoleHelper& operator<<(T v) {
    PrintHelper<T>::out(*this, v);
    return *this;
  }



};

template<typename T>
Print& operator<<(Print& p, T v) {
  PrintHelper<const T>::out(p, v);
  return p;
}

class CatchCommandOutput : public Print {
 public:
  CatchCommandOutput(int target_line, char* buf, int bufsize) :
    target_line_(target_line),
    buf_(buf),
    bufsize_(bufsize)  {
  }
  size_t write(uint8_t b) override {
    if (b == '\n') { line_++; return 1; }
    if (b == '\r') return 1;	  
    if (line_ == target_line_) {
      if (bufsize_ > 1) {
	*buf_ = b;
	buf_++;
	*buf_ = 0;
	bufsize_--;
      }
    }
    return 1;
  }
 int num_lines() { return line_ - 1; } // assume last line is empty
private:
 int target_line_ = 0;
 int line_ = 1;

 char* buf_;
 size_t bufsize_;
};

template<size_t MAXLINE>
class GetNextLineCommandOutput : public Print {
 public:
  GetNextLineCommandOutput(const char* previous, char* target_buf, bool reverse) :
    previous_(previous),
    buf_(target_buf),
    reverse_(reverse) {
  }

  bool compare(const char* a, const char *b) const {
    return reverse_ ? (strcmp(a, b) > 0) : (strcmp(a, b) < 0);
  }
  size_t write(uint8_t b) override {
    if (b == '\n') {
      if (pos_) {
	line_[pos_] = 0;
	if (previous_ == nullptr || compare(previous_, line_)) {
	  if (!found_ || compare(line_, buf_)) {
	    strcpy(buf_, line_);
	    found_ = true;
	  }
	}
	pos_ = 0;
      }
      return 1;
    }
    if (b == '\r') return 1;
    if (pos_ <  MAXLINE - 1) line_[pos_++] = b;
    return 1;
  }
  bool found() const { return found_; }
private:
  bool reverse_;
  bool found_ = false;
  size_t pos_ = 0;
  char* buf_;
  const char* previous_;
  char line_[MAXLINE];
};

extern ConsoleHelper STDOUT;

#define DEFINE_COMMON_STDOUT_GLOBALS            \
Print* default_output = &Serial;                \
Print* stdout_output = &Serial;                 \
ConsoleHelper STDOUT
#define STDERR (*default_output)

#endif
