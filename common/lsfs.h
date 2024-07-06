#ifndef COMMON_LSFS_H
#define COMMON_LSFS_H

// Filesystem abstractions

struct PathHelper {
  void Append(const char* name) {
    if (strlen(path_) && path_[strlen(path_)-1] != '/') {
      strcat(path_, "/");
    }
    strcat(path_, name);
  }
  void Append(const char* name, const char *ext) {
    if (strlen(path_) && path_[strlen(path_)-1] != '/') {
      strcat(path_, "/");
    }
    strcat(path_, name);
    strcat(path_, ".");
    strcat(path_, ext);
  }
  void Set(const char* path) {
#ifdef ARDUINO_ARCH_ESP32   // ESP architecture
    strcpy(path_, path ? "/" : "");
    strcpy(&path_[1], path ? path : "");
#else
    strcpy(path_, path ? path : "");
#endif
  }
  void Set(const char* path, const char* p2) {
    Set(path);
    Append(p2);
  }
  void Set(const char* path, const char* p2, const char* ext) {
    Set(path);
    Append(p2, ext);
  }
  explicit PathHelper(const char* path) {
    Set(path);
  }
  explicit PathHelper(const char* path, const char* p2) {
    Set(path, p2);
  }
  explicit PathHelper(const char* path, const char* p2, const char* ext) {
    Set(path, p2, ext);
  }
  bool IsRoot() const {
    if (path_[0] == 0) return true;
    if (path_[0] == '/' && path_[1] == 0) return true;
    return false;
  }
  bool Dirname() {
    char *p = strrchr(path_, '/');
    if (p) {
      *p = 0;
      return !IsRoot();
    }
    return false;
  }
  void UndoDirname() {
    path_[strlen(path_)] = '/';
  }
  operator const char*() const { return path_; }
#ifdef F_MAXPATH  
  char path_[F_MAXPATH];
#else
  char path_[128];
#endif
};


#if defined(PROFFIE_TEST)

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include "linked_ptr.h"
// Posix file primitives

class DoCloseFile {
public:
  static void Free(FILE* f) { if(f) fclose(f); }
};

class DoCloseDir {
public:
  static void Free(DIR* dir) { if(dir) closedir(dir); }
};

class File {
public:
  File() : file_() {}
  File(FILE* f) : file_(f) {}
  operator bool() const { return !!file_; }
  void close() { file_ = NULL; }
  int read(uint8_t *dest, size_t bytes) {
    return fread(dest, 1, bytes, file_.get());
  }
  int write(const uint8_t *dest, size_t bytes) {
    return fwrite(dest, 1, bytes, file_.get());
  }
  void seek(size_t pos) {
    fseek(file_.get(), pos, SEEK_SET);
  }
  uint32_t position() {
    return ftell(file_.get());
  }
  uint32_t available() {
    long pos = position();
    fseek(file_.get(), 0, SEEK_END);
    long end = position();
    seek(pos);
    return end - pos;
  }
  uint32_t size() {
    long pos = position();
    fseek(file_.get(), 0, SEEK_END);
    long end = position();
    seek(pos);
    return end;
  }
  int peek() {
    long pos = position();
    uint8_t ret;
    read(&ret, 1);
    seek(pos);
    return ret;
  }
  LinkedPtr<FILE, DoCloseFile> file_;
};


class LSFS {
public:
  typedef File FILE;
  static bool Begin() { return true; }
  static bool End() { return true; }
  static bool Exists(const char* path) {
    struct stat s;
    return stat(path, &s) == 0;
  }
  static bool Remove(const char* path) {
    return unlink(path) == 0;
  }
  static File Open(const char* path) {
    return fopen(path, "r");
  }
  static File OpenRW(const char* path) {
    File ret = fopen(path, "r+");
    if (ret) return ret;
    return OpenForWrite(path);
  }
  static File OpenFast(const char* path) {
    return fopen(path, "r");
  }
  static File OpenForWrite(const char* path) {
    return fopen(path, "wct");
  }
  class Iterator {
  public:
    explicit Iterator(const char* dirname) {
      dir_ = opendir(dirname);
      entry_ = readdir(dir_.get());
    }
    explicit Iterator(Iterator& other) {
      if (other.dir_) {
	dir_ = fdopendir(openat(dirfd(other.dir_.get()),
				other.entry_->d_name,
				O_RDONLY));
	entry_ = readdir(dir_.get());
      }
    }
    void operator++() {
      entry_ = readdir(dir_.get());
    }
    bool isdir() {
      struct stat s;
      if (!strcmp(entry_->d_name, ".")) return false;
      if (!strcmp(entry_->d_name, "..")) return false;
      if (fstatat(dirfd(dir_.get()), entry_->d_name, &s, 0) != 0) return false;
      return S_ISDIR(s.st_mode);
    }
    operator bool() { return !!entry_; }
    // bool isdir() { return f_.isDirectory(); }
    const char* name() { return entry_->d_name; }
    // size_t size() { return f_.size(); }
    
  private:
    LinkedPtr<DIR, DoCloseDir> dir_;
    dirent* entry_;
  };
};

#elif defined(ARDUINO_ARCH_STM32L4) // STM Architecture
   
#include <FS.h>

// Workaround for badly named variable
#define private c_private
#include <dosfs_core.h>
#undef private

class LSFS {
public:
  typedef File FILE;
  static bool IsMounted() {
    return mounted_;
  }
  static bool CanMount() {
    // dosfs_volume_t *volume = DOSFS_DEFAULT_VOLUME();
    dosfs_device_t *device = DOSFS_VOLUME_DEVICE(volume);
    if (device->lock & (DOSFS_DEVICE_LOCK_VOLUME |
			DOSFS_DEVICE_LOCK_SCSI |
			DOSFS_DEVICE_LOCK_MEDIUM |
			DOSFS_DEVICE_LOCK_INIT)) return false;
    return true;
  }
  static void WhyBusy(char *tmp) {
    *tmp = 0;
    // dosfs_volume_t *volume = DOSFS_DEFAULT_VOLUME();
    dosfs_device_t *device = DOSFS_VOLUME_DEVICE(volume);
    if (device->lock & DOSFS_DEVICE_LOCK_VOLUME)
      strcat(tmp, " volume");
    if (device->lock & DOSFS_DEVICE_LOCK_SCSI)
      strcat(tmp, " scsi");
    if (device->lock & DOSFS_DEVICE_LOCK_MEDIUM)
      strcat(tmp, " medium");
    if (device->lock & DOSFS_DEVICE_LOCK_INIT)
      strcat(tmp, " init");
  }
  // This function waits until the volume is mounted.
  static bool Begin() {
    if (mounted_) return true;
    if (!DOSFS.begin()) return false;
    if (!DOSFS.check()) {
      DOSFS.end();
      return false;
    }
    return mounted_ = true;
  }
  static void End() {
    if (!mounted_) return;
    DOSFS.end();
    mounted_ = false;
  }
  static bool Exists(const char* path) {
    if (!mounted_) return false;
    return DOSFS.exists(path);
  }
  static bool Remove(const char* path) {
    if (!mounted_) return false;
    return DOSFS.remove(path);
  }
  static File Open(const char* path) {
    if (!mounted_) return File();
    return DOSFS.open(path, "r");
  }
  static File OpenRW(const char* path) {
    if (!mounted_) return File();
    File f = DOSFS.open(path, "r+");
    if (!f) f = OpenForWrite(path);
    return f;
  }
  static File OpenFast(const char* path) {
    if (!mounted_) return File();
    return DOSFS.open(path, "r");
  }
  static void mkdir(PathHelper& p) {
    if (!mounted_) return;
    if (p.Dirname()) {
      mkdir(p);
      p.UndoDirname();
    }
    DOSFS.mkdir(p);
  }
  
  static bool RenamePath(const char* pathFrom, const char* pathTo)
  {
    return DOSFS.rename(pathFrom, pathTo);
  }
  
  
  static File OpenForOverWrite(const char* path) {
    if (!mounted_) return File();
    return DOSFS.open(path, "r+");
  }
  // #endif
  static File OpenForWrite(const char* path) {
    if (!mounted_) return File();
    File f = DOSFS.open(path, "w");
    if (!f) {
      PathHelper tmp(path);
      if (tmp.Dirname()) {
	mkdir(tmp);
	f = DOSFS.open(path, "w");
      }
    }
    return f;
  }
  class Iterator {
  public:
    explicit Iterator(const char* path) {

      strcpy(_path, path);

      if (path[strlen(path)-1] != '/')  
        strcat(_path, "/");

      PathHelper filename(_path, "*.*");
      if (!mounted_ || f_findfirst(filename, &_find) != F_NO_ERROR) {
        _find.find_clsno = 0x0fffffff;
      }
    }
    explicit Iterator(Iterator& other) {
      strcpy(_path, other._path);
      strcat(_path, other.name());

      if (_path[strlen(_path)-1] != '/')  
        strcat(_path, "/");

      PathHelper filename(_path, "*.*");
      if (!mounted_ || f_findfirst(filename, &_find) != F_NO_ERROR) {
        _find.find_clsno = 0x0fffffff;
      }
    }

    void operator++() {
      if (!mounted_ || f_findnext(&_find) != F_NO_ERROR) {
        _find.find_clsno = 0x0fffffff;
      }
    }
    operator bool() { return _find.find_clsno != 0x0fffffff; }
    bool isdir() { return _find.attr & F_ATTR_DIR; }
    const char* name() { return _find.filename; }
    size_t size() { return _find.filesize; }
  #ifdef OSX_ENABLE_MTP
	char attr() {return _find.attr; }
  #endif
    
  private:
    char _path[F_MAXPATH];
    F_FIND _find;
  };
private:
  static bool mounted_;
};

bool LSFS::mounted_ = false;


#elif defined(ARDUINO_ARCH_ESP32) // ESP architecture
#define PROFFIE_ESP_SDMMC
#include "FS.h"
#if defined(PROFFIE_ESP_SDMMC)
#include "SD_MMC.h"
#define DOSFS SD_MMC
#define DOSFS_SDMMC_CLK   38
#define DOSFS_SDMMC_CMD   37
#define DOSFS_SDMMC_D0    36
#define DOSFS_SDMMC_D1    35
#define DOSFS_SDMMC_D2    34
#define DOSFS_SDMMC_D3    33
#elif defined(PROFFIE_ESP_SDSPI)
#include "SD.h"
#include "SPI.h"

#define SDSPI_VSPI_SCK   18
#define SDSPI_VSPI_MISO  19
#define SDSPI_VSPI_MOSI  23
#define SDSPI_VSPI_CS    5

SPIClass sdspi_periph = SPIClass(FSPI);
#define DOSFS SD

#else
#error "Invalid an SD interface "
#endif

class LSFS {
public:
  typedef File FILE;
  static bool IsMounted() {
    return mounted_;
  }
  static bool CanMount() {
    // TODO add logic to check if we can mount
    return true;
  }
  static void WhyBusy(char *tmp) {
    *tmp = 0;
    // // dosfs_volume_t *volume = DOSFS_DEFAULT_VOLUME();
    // dosfs_device_t *device = DOSFS_VOLUME_DEVICE(volume);
    // if (device->lock & DOSFS_DEVICE_LOCK_VOLUME)
    //   strcat(tmp, " volume");
    // if (device->lock & DOSFS_DEVICE_LOCK_SCSI)
    //   strcat(tmp, " scsi");
    // if (device->lock & DOSFS_DEVICE_LOCK_MEDIUM)
    //   strcat(tmp, " medium");
    // if (device->lock & DOSFS_DEVICE_LOCK_INIT)
    //   strcat(tmp, " init");

    // TODO add logic to find out why is busy 
  }
  // This function waits until the volume is mounted.
  static bool Begin() {
    if (mounted_) return true; 
    #if defined(PROFFIE_ESP_SDMMC)
    DOSFS.setPins(DOSFS_SDMMC_CLK, DOSFS_SDMMC_CMD, DOSFS_SDMMC_D0, DOSFS_SDMMC_D1, DOSFS_SDMMC_D2, DOSFS_SDMMC_D3);
    if (!DOSFS.begin("/sdcard", false, false, SDMMC_FREQ_HIGHSPEED, 16)) return false; //20000    SDMMC_FREQ_HIGHSPEED SDMMC_FREQ_DEFAULT
    #elif defined(PROFFIE_ESP_SDSPI)
    sdspi_periph.begin(SDSPI_VSPI_SCK, SDSPI_VSPI_MISO, SDSPI_VSPI_MOSI, SDSPI_VSPI_CS);
    if (!DOSFS.begin(SDSPI_VSPI_CS, sdspi_periph, 25000000, "/sdcard")) return false;
    #endif
    // if (!DOSFS.check()) {   //TODO add logic to make a sd check 
    //   DOSFS.end();
    //   return false;
    // }
    return mounted_ = true;
  }

  static void correctPath(char* destPath, const char* sourcePath)  __attribute__((optimize("Ofast")))
  {
    strcpy(destPath, "/");
    strcpy((destPath + 1), sourcePath);
  }
  static void End() {
    if (!mounted_) return;
    DOSFS.end();
    #if defined(PROFFIE_ESP_SDSPI)
      sdspi_periph.end();
    #endif
    mounted_ = false;
  }
  static bool Exists(const char* path) {
    if (!mounted_) return false;
    correctPath(_correctedPath, path);
    return DOSFS.exists(_correctedPath);   // was path 
  }
  static bool Remove(const char* path) {
    if (!mounted_) return false;
    correctPath(_correctedPath, path);
    return DOSFS.remove(_correctedPath);
  }
  static File Open(const char* path)  __attribute__((optimize("Ofast"))) {
    if (!mounted_) return File();
    openTime = micros();
    correctPath(_correctedPath, path);
    File f = DOSFS.open(_correctedPath, "r");
    openTime = micros() - openTime;
    return f;
  }

  static File OpenRW(const char* path) {
    if (!mounted_) return File();
    File f = DOSFS.open(path, "r+");
    if (!f) f = OpenForWrite(path);
    return f;
  }

  static File OpenFast(const char* path)  __attribute__((optimize("Ofast"))) {
    if (!mounted_) return File();
    openTime = micros();
    correctPath(_correctedPath, path);
    File f = DOSFS.open(_correctedPath, "r");
    openTime = micros() - openTime;
    return f;
  }

  static bool RenamePath(const char* pathFrom, const char* pathTo)
  {
    return DOSFS.rename(pathFrom, pathTo);
  }


  static void mkdir(PathHelper& p) {
    if (!mounted_) return;
    if (p.Dirname()) {
      mkdir(p);
      p.UndoDirname();
    }
    DOSFS.mkdir(p);
  }

  static File OpenForOverWrite(const char* path) {
    if (!mounted_) return File();
    correctPath(_correctedPath, path);
    File f = DOSFS.open(_correctedPath, "r+");
    return f;
  }

  static File OpenForWrite(const char* path) {
    if (!mounted_) return File();
    correctPath(_correctedPath, path);
    File f = DOSFS.open(_correctedPath, "w");
    if (!f) {
      PathHelper tmp(_correctedPath);
      if (tmp.Dirname()) {
	mkdir(tmp);
	f = DOSFS.open(_correctedPath, "w");
      }
    }
    
    return f;
  }

  static void PrintLastOpenFileTime()
  {
    STDOUT.print("Last open file time ");STDOUT.print(openTime); STDOUT.println(" us");
  }

  class Iterator {
  public:
    explicit Iterator(const char* path) {

      strcpy(_path, path);

      if (!strlen(path) && (path[strlen(path)-1] != '/') )   //  (strlen(path) == 1) && 
        strcat(_path, "/");
      
      //Serial.print("path Len "); Serial.println(strlen(path));

      // PathHelper filename(_path, "*.*");
      PathHelper filename(_path);
      if (!mounted_ || ( !(_find = DOSFS.open(filename))) ) {
        find_clsno = 0x0fffffff;
      } else {
        _ffind = File();
      }
      
    }
    explicit Iterator(Iterator& other) {
      strcpy(_path, other._path);
      strcat(_path, other.name());

      if (!strlen(_path) && (_path[strlen(_path)-1] != '/') )   //  (strlen(path) == 1) && 
        strcat(_path, "/");

      // PathHelper filename(_path, "*.*");
      PathHelper filename(_path);
      if (!mounted_ || ( !(_find = DOSFS.open(filename)))) {
        find_clsno = 0x0fffffff;
      } else {
        _ffind = File();
      }
    }

    void operator++() {
      if(_find)
        _ffind = _find.openNextFile();
      if (!mounted_ || (!(_ffind)) ) {
        find_clsno = 0x0fffffff;
      }
    }
    operator bool() { return find_clsno != 0x0fffffff; }
    bool isdir() {  if(_ffind) return _ffind.isDirectory(); 
                    if(_find)return _find.isDirectory(); 
                    return false; 
                 }

    const char* name() { 
                  //if(!_find)return NULL;
                  if(_ffind) return _ffind.name(); 
                  if(_find) return _find.name(); 
                  return nullptr; 
                  }

    size_t size() { 
                  if(_ffind) return _ffind.size(); 
                  if(_find)return _find.size(); 
                  return 0; 
                  }
  #ifdef OSX_ENABLE_MTP
	char attr() {
            // return _find.attr;
            char attr = 0; 
            if(isdir()) attr = 0x10;
            else attr = 0x20;
            return attr;
        }
  #endif
    
  private:
    char _path[128];
    File _find;
    File _ffind;
    uint32_t find_clsno;
  };
private:
  static bool mounted_;
  static char _correctedPath[128];
  static uint32_t openTime;
};

bool LSFS::mounted_ = false;
char LSFS::_correctedPath[128];
uint32_t LSFS::openTime = 0;


#endif // PROFFIE_TEST 

#endif
