#ifndef SOUND_DAC_H
#define SOUND_DAC_H

// #include "filter.h"

#include <Arduino.h>
#include "driver/i2s.h"
#include <math.h>
#include <FS.h>

#ifdef ENABLE_LOGG_WAVE_OUTPUT
#include "../common/ramLogger.h"
#endif

#define CHANNELS 2
// #define I2S_NUM_FRAMES_TO_SEND 512
typedef struct
{
  uint16_t left;
  uint16_t right;
}I2S_Frame_t;

#define LS_DAC_I2S_MUTEX_PROTECT

#ifdef LS_DAC_I2S_MUTEX_PROTECT
#include "freertos/semphr.h"
#define LS_DAC_I2S_MUTEX_LOCK()    do {} while (xSemaphoreTake(i2s_mutex, portMAX_DELAY) != pdPASS)    
#define LS_DAC_I2S_MUTEX_UNLOCK()  xSemaphoreGive(i2s_mutex)
#endif
bool SoundActive();         // defined later in sound.h
void SetupStandardAudio();  // defined later in sound.h


class LS_DAC : CommandParser, Looper, public PowerSubscriber {
    public:
      LS_DAC() : CommandParser(), Looper(), PowerSubscriber(pwr4_Amplif) {
        #ifdef LS_DAC_I2S_MUTEX_PROTECT
        i2s_mutex = xSemaphoreCreateMutex(); // Create i2s mutex
        #endif
      } // booster and amplifier are enabled by the same signal with I2S DAC/amplifier, so there's no distinct power domain for booster
      void PwrOn_Callback() override  { 
        begin();      // setup and start peripheral
        #ifdef DIAGNOSE_POWER
          STDOUT.println(" dac+ ");  
        #endif
      }         
      void PwrOff_Callback() override { 
        end();      // de-init peripheral
        #ifdef DIAGNOSE_POWER
          STDOUT.println(" dac- "); 
        #endif
      }     
      void Loop() override { if (SoundActive()) RequestPower(); }

      virtual const char* name() { return "DAC"; }

      static void i2sWriterTask(void *param) __attribute__((optimize("Ofast")))
      {
        LS_DAC *output = (LS_DAC *)param;
        int availableBytes = 0;
        int buffer_position = 0;
        // Frame_t *frames = (Frame_t *)malloc(sizeof(Frame_t) * AUDIO_BUFFER_SIZE);
        int16_t data[AUDIO_BUFFER_SIZE];
        while (true)
        {
          // wait for some data to be requested
          i2s_event_t evt;
          if (xQueueReceive(output->m_i2sQueue, &evt, portMAX_DELAY) == pdPASS)
          {
              if (evt.type == I2S_EVENT_TX_DONE)
              {
                  size_t bytesWritten = 0;
                  do
                  {
                      if (availableBytes == 0)
                      {
                          // get some frames from the wave file - a frame consists of a 16 bit left and right sample
                          //output->m_sample_generator->getFrames(frames, I2S_NUM_FRAMES_TO_SEND);
                          // how maby bytes do we now have to send
                          int n = 0;
                          if (stream_) {
                            n = dynamic_mixer.read(data, AUDIO_BUFFER_SIZE);
                            // if(n < AUDIO_BUFFER_SIZE)
                            // {
                            //     //  STDOUT.print("r ");STDOUT.println(n);
                            // } 
                          } else {
                            //STDOUT.println("NO STReAM AVAILABLE ");
                          }
                          while (n < AUDIO_BUFFER_SIZE) data[n++] = 0;

                          uint16_t *dest = dac_dma_buffer;
                          for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
                            // frames[i].left = data[i];
                            // frames[i].right = data[i];
                            *(dest++) = data[i];
                            *(dest++) = data[i];
                            #ifdef ENABLE_LOGG_WAVE_OUTPUT
                            logger.log(data[i]);
                            #endif
                            // STDOUT.print("L = ");STDOUT.println(data[i]); // STDOUT.println("");
                          }
                          availableBytes = n * sizeof(uint32_t);
                          // reset the buffer position back to the start
                          buffer_position = 0;
                      }
                      // do we have something to write?
                      if (availableBytes > 0)
                      {
                          // write data to the i2s peripheral
                          i2s_write(output->m_i2sPort, buffer_position + (uint8_t *)dac_dma_buffer,
                                  availableBytes, &bytesWritten, portMAX_DELAY);
                          availableBytes -= bytesWritten;
                          buffer_position += bytesWritten;
                      }

                      // LS_DAC_I2S_MUTEX_LOCK();
                      // if(!createTask_)
                      // {
                      //   bytesWritten = 0;
                      //   i2s_stop(I2S_NUM_0);
                      // }
                      // LS_DAC_I2S_MUTEX_UNLOCK();

                  } while (bytesWritten > 0); // bytesWritten
              } else {
                STDOUT.print("EVT type: "); STDOUT.println(evt.type);
              }
          }
          // LS_DAC_I2S_MUTEX_LOCK();
          // if(!createTask_)
          // {
          //   i2s_driver_uninstall(I2S_NUM_0);
          //   needs_setup_ = true;
          //   createTask_ = false;
          //   xQueueReset(output->m_i2sQueue);
          //   LS_DAC_I2S_MUTEX_UNLOCK();
          //   break;
          // }
          // LS_DAC_I2S_MUTEX_UNLOCK();
            // vTaskDelay(1);
        }
        // vTaskDelete(NULL);

      }


      void Setup() override {
        if (!needs_setup_) return;
        LS_DAC_I2S_MUTEX_LOCK();
         
         i2s_pin_config_t i2sPins = {
          .bck_io_num = bclkPin, //GPIO_NUM_5,
          .ws_io_num = lrclkPin, //GPIO_NUM_6,
          .data_out_num = txd0Pin,// GPIO_NUM_4,
          .data_in_num = -1
          };

         i2s_config_t i2sConfig = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = AUDIO_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),    // I2S_COMM_FORMAT_STAND_I2S
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL6, // ESP_INTR_FLAG_LEVEL6, //ESP_INTR_FLAG_NMI,
            .dma_buf_count = 8,
            .dma_buf_len = 64}; // was 64    // 1024

        m_i2sPort = I2S_NUM_0;
        //install and start i2s driver
        if( i2s_driver_install(m_i2sPort, &i2sConfig, 4, &m_i2sQueue) != ESP_OK)
        {
          STDOUT.println("Failed driver install !");
          needs_setup_ = true;
          return;
        }
 
        // set up the i2s pins
        if(i2s_set_pin(m_i2sPort, &i2sPins) != ESP_OK)
        {
          STDOUT.println("Failed set pin !");
          needs_setup_ = true;
          return;
        }
        // clear the DMA buffers
        if(i2s_zero_dma_buffer(m_i2sPort) != ESP_OK)
        {
          STDOUT.println("Failed zero dma !");
          needs_setup_ = true;
          return;
        }
        needs_setup_ = false;
        // start a task to write samples to the i2s peripheral
        if(!createTask_) {
          if(xTaskCreatePinnedToCore(i2sWriterTask, "i2s Writer Task", 4096*3, this, configMAX_PRIORITIES - 1, &m_i2sWriterTaskHandle, ARDUINO_RUNNING_CORE) == pdPASS)   // ARDUINO_RUNNING_CORE
              createTask_ = true;
          else 
            STDOUT.println("Failed to create audio task!");
        }
        LS_DAC_I2S_MUTEX_UNLOCK();
      }

      void begin()
      {
        if (on_) return;
        on_ = true;
        Setup();
        i2s_start(I2S_NUM_0);

      }

      void end()
      {
        if (!on_) return;
        on_ = false;
        i2s_stop(I2S_NUM_0);
        // LS_DAC_I2S_MUTEX_LOCK();
        // createTask_ = false;
        // LS_DAC_I2S_MUTEX_UNLOCK();
        // TODO , find a way to end it 
      }

      #ifdef ENABLE_LOGG_WAVE_OUTPUT
      bool isOn() {
        return on_;
      }
      #endif

      void unistall()
      {
        i2s_driver_uninstall(I2S_NUM_0);
        if(createTask_){
          vTaskSuspend(m_i2sWriterTaskHandle);
          vTaskDelete(m_i2sWriterTaskHandle);
          xQueueReset(m_i2sQueue);
        }
        needs_setup_ = true;
        createTask_ = false;
      }

      bool Parse(const char* cmd, const char* arg) override 
      {
        return false;
      }

      bool isSilent() 
      {
     for (size_t i = 0; i < NELEM(dac_dma_buffer); i++)
       if (dac_dma_buffer[i] != dac_dma_buffer[0])
         return false;
      // for (size_t i = 0; i < NELEM(frames); i++)
      //   if (frames[i].left != frames[0].left )
      //     return false;
      return true;
      }

      void Help() override 
      {
       #if defined(COMMANDS_HELP) 
        STDOUT.println(" dacbuf - print the current contents of the dac buffer");
        STDOUT.println("play_dac - play a file from dac cmd");
       #endif
      }

      // TODO: Replace with enable/disable
      void SetStream(class ProffieOSAudioStream* stream) {
        stream_ = stream;
      }
    private:

      static uint32_t current_position() 
      {
        return 0;
      }

      // static void isr(void* arg, unsigned long int event)
      // {
        
      // }

      // static void isr2(void* arg, unsigned long int event) {}

      bool on_ = false;
      static bool needs_setup_;
      static bool createTask_;
      static ProffieOSAudioStream * volatile stream_;
      #ifdef LS_DAC_I2S_MUTEX_PROTECT
      static SemaphoreHandle_t i2s_mutex;
      #endif
      // static I2S_Frame_t frames[AUDIO_BUFFER_SIZE];

      static uint16_t dac_dma_buffer[AUDIO_BUFFER_SIZE*CHANNELS];
      // static uint16_t dac_dma_buffer2[AUDIO_BUFFER_SIZE*2*CHANNELS];
          // I2S write task
      TaskHandle_t m_i2sWriterTaskHandle;
      // i2s writer queue
      QueueHandle_t m_i2sQueue;
      // i2s port
      i2s_port_t m_i2sPort;

    };

    ProffieOSAudioStream * volatile LS_DAC::stream_ = nullptr;
    bool LS_DAC::createTask_ = false;
    bool LS_DAC::needs_setup_ = true;
    #ifdef LS_DAC_I2S_MUTEX_PROTECT
    SemaphoreHandle_t LS_DAC::i2s_mutex = NULL;
    #endif
    //__attribute__((aligned(32))) I2S_Frame_t LS_DAC::frames[AUDIO_BUFFER_SIZE];
    __attribute__((aligned(32))) uint16_t LS_DAC::dac_dma_buffer[AUDIO_BUFFER_SIZE*CHANNELS];
    // __attribute__((aligned(32))) uint16_t LS_DAC::dac_dma_buffer2[AUDIO_BUFFER_SIZE*2*CHANNELS];



LS_DAC dac_OS;

void xDestroyDAComponents()
{
  dac_OS.unistall();
}

#endif
