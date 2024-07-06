#ifndef SOUND_DAC_H
#define SOUND_DAC_H

// #include "filter.h"

#if defined(__IMXRT1062__)
void set_audioClock(int nfact, int32_t nmult, uint32_t ndiv,  bool force = false); // sets PLL4
#endif

#ifdef USE_I2S



#define CHANNELS 1


void my_stm32l4_system_saiclk_configure_22579200()
{
  RCC->PLLSAI1CFGR &= ~RCC_PLLSAI1CFGR_PLLSAI1PEN;
  RCC->CR &= ~RCC_CR_PLLSAI1ON;

  /* Wait till the PLLSAI1 is turned off */
  while (RCC->CR & RCC_CR_PLLSAI1RDY) {}

  RCC->PLLSAI1CFGR = ((11 << 27) | (62 << 8) | RCC_PLLSAI1CFGR_PLLSAI1PEN);
  RCC->CR |= RCC_CR_PLLSAI1ON;

  /* Wait till the PLLSAI1 is ready */
  while (!(RCC->CR & RCC_CR_PLLSAI1RDY)){}
}



#else   // USE_I2S
#define CHANNELS 1
#endif  // USE_I2S

#define PDB_CONFIG (PDB_SC_TRGSEL(15) | PDB_SC_PDBEN | PDB_SC_CONT | PDB_SC_PDBIE | PDB_SC_DMAEN)


#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
#include "stm32l4_timer.h"
#include "stm32l4_dac.h"
#include "stm32l4_dma.h"
#include "stm32l4_gpio.h"

stm32l4_timer_t stm32l4_dac1Timer;
stm32l4_dac_t stm32l4_dac1;
uint32_t timPeriphClock;
// stm32l4_dma_t stm32l4_dac1DMA;


#endif


#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
bool SoundActive();         // defined later in sound.h
void SetupStandardAudio();  // defined later in sound.h
class LS_DAC : CommandParser, Looper, public PowerSubscriber {
public:
  LS_DAC() : CommandParser(), Looper(), PowerSubscriber(pwr4_Booster | pwr4_Amplif) {}
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

#else 
class LS_DAC : CommandParser, Looper {
public:
  void Loop() override {}
#endif 

  virtual const char* name() { return "DAC"; }
  void Setup() override {
    if (!needs_setup_) return;
    needs_setup_ = false;
	
  #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
    bool result;
    unsigned int priority = STM32L4_SAI_IRQ_PRIORITY;
    // uint32_t timPeriphClock;
    // uint32_t dmaOption = 0;
    // 1. Create instance of timer6 , with desired priority
    result = stm32l4_timer_create(&stm32l4_dac1Timer, TIMER_INSTANCE_TIM6, priority, 0);  // prio was 2 
    if(result && stm32l4_dac1Timer.state == TIMER_STATE_INIT) // if created OK proceed configureing it
    {
        timPeriphClock = stm32l4_timer_clock(&stm32l4_dac1Timer);  // get clock peripheral
        result = stm32l4_timer_enable(&stm32l4_dac1Timer, 0, 1814, 0, NULL, NULL, TIMER_OPTION_COUNT_UP);  //  | TIMER_EVENT_PERIOD   // 1300 // 1814
        stm32l4_dac1Timer.TIM->CR2 = 0X20; // The update event is selected as a trigger output (TRGO). 
        // stm32l4_timer_start(&stm32l4_dac1Timer, 0); // start the timer 
    } // end creation of TIMER6 for triggering DAC conversion 

        // 0 AMP enable GPIO // toDo Move from here 
    stm32l4_gpio_pin_configure(GPIO_PIN_PB4, GPIO_MODE_OUTPUT | GPIO_OTYPE_PUSHPULL | GPIO_OSPEED_LOW | GPIO_PUPD_PULLDOWN);
    //stm32l4_gpio_pin_write(GPIO_PIN_PB4, 0); // RESET FOr NOW 
    // 1. GPIO INit - PA4 , prepare pin for DAC1 USAGE 
    stm32l4_gpio_pin_configure(GPIO_PIN_PA4, GPIO_MODE_ANALOG | GPIO_OTYPE_PUSHPULL | GPIO_OSPEED_LOW | GPIO_PUPD_NONE); 
    // 2. DMA1 Channel3  preparing
    stm32l4_dma_create(&dma, DMA_CHANNEL_DMA1_CH3_DAC1, priority);
    stm32l4_dma_enable(&dma, isr, NULL);
    // stm32l4_dac1DMA.DMA->CCR = DMA_CCR_PL_1 | DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0 | DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_CIRC;


    // 3. DAC1 Initialization
    stm32l4_dac_create(&stm32l4_dac1, DAC_INSTANCE_DAC, priority, 0);
    stm32l4_dac_enable(&stm32l4_dac1, 0, NULL, NULL, 0);
    stm32l4_dac_channel(&stm32l4_dac1, DAC_CHANNEL_1, 0, DAC_CONTROL_EXTERNAL);     // make calibration   DAC_CONTROL_INTERNAL
    stm32l4_dac1.DACx->CR |= DAC_CR_TEN1;     // and DAC channel1 DMA Underrun Interrupt enabled
    stm32l4_dac1.DACx->CR &= ~DAC_CR_EN1;     // Disable for now , further config folllow 

    stm32l4_dac1.DACx->CR |= DAC_CR_DMAEN1 | DAC_CR_DMAUDRIE1; // enable underrrun IT and DMA ENABLE

  #else // end ULTRAPROFFIE_DAC
    #if PROFFIEBOARD_VERSION == 3
        stm32l4_dma_create(&dma, DMA_CHANNEL_DMA2_CH1_SAI1_A, STM32L4_SAI_IRQ_PRIORITY);
    #else
        stm32l4_dma_create(&dma, DMA_CHANNEL_DMA2_CH6_SAI1_A, STM32L4_SAI_IRQ_PRIORITY);
    #endif
    #if defined(ENABLE_I2S_OUT) || defined(ENABLE_SPDIF_OUT)
        stm32l4_dma_create(&dma2, DMA_CHANNEL_DMA2_CH2_SAI1_B, STM32L4_SAI_IRQ_PRIORITY);
        // stm32l4_dma_create(&dma2, DMA_CHANNEL_DMA2_CH7_SAI1_B, STM32L4_SAI_IRQ_PRIORITY);
    #endif
  #endif // end else  ULTRAPROFFIE_DAC



#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
    SetupStandardAudio();   // was in amplifier.Setup()
#endif

  }

  void begin() {
    if (on_) return;
    on_ = true;
    Setup();

    memset(dac_dma_buffer, 0, sizeof(dac_dma_buffer));
#if defined(ENABLE_SPDIF_OUT) || defined(ENABLE_I2S_OUT)
    memset(dac_dma_buffer2, 0, sizeof(dac_dma_buffer2));
#endif
#ifdef FILTER_CUTOFF_FREQUENCY
    filter_.clear();
#endif

  #if defined(ULTRAPROFFIE_DAC) && defined(ARDUINO_ARCH_STM32L4)  // STM UltraProffies
  uint32_t dmaOption = 0;
  dmaOption = DMA_OPTION_MEMORY_TO_PERIPHERAL | DMA_OPTION_PERIPHERAL_DATA_SIZE_16 | DMA_OPTION_PRIORITY_HIGH | DMA_OPTION_MEMORY_DATA_SIZE_16 \
                  | DMA_OPTION_CIRCULAR | DMA_OPTION_MEMORY_DATA_INCREMENT;
  dmaOption |= DMA_OPTION_EVENT_TRANSFER_HALF | DMA_OPTION_EVENT_TRANSFER_DONE | DMA_OPTION_EVENT_TRANSFER_ERROR;  // | DMA_OPTION_EVENT_TRANSFER_HALF

  stm32l4_dma_start(&dma, ((uint32_t)(stm32l4_dac1.DACx))+12, (uint32_t)(dac_dma_buffer), AUDIO_BUFFER_SIZE * 2, dmaOption);  // was +8
  // DMA1->ISR |= DMA_ISR_GIF3 | DMA_ISR_TCIF3 | DMA_ISR_HTIF3;
  // stm32l4_dma_poll(&stm32l4_dac1DMA);
  stm32l4_timer_start(&stm32l4_dac1Timer, 0); // start the timer 
  stm32l4_dac1.DACx->CR |= DAC_CR_EN1; // enable underrrun IT and DMA ENABLE

#else // end ULTRAPROFFIE_DAC
      stm32l4_system_periph_enable(SYSTEM_PERIPH_SAI1);
      stm32l4_dma_enable(&dma, &isr, 0);
      SAI_Block_TypeDef *SAIx = SAI1_Block_A;
      SAIx->CR1 = SAI_xCR1_DS_2 | SAI_xCR1_MONO | SAI_xCR1_CKSTR
  #ifdef ENABLE_SPDIF_OUT
      | (1 << SAI_xCR1_MCKDIV_Pos)
  #endif
      ;
      SAIx->FRCR = (31 << SAI_xFRCR_FRL_Pos) | (15 << SAI_xFRCR_FSALL_Pos) | SAI_xFRCR_FSDEF | SAI_xFRCR_FSOFF;
      SAIx->SLOTR = SAI_xSLOTR_NBSLOT_0 | (0x0003 << SAI_xSLOTR_SLOTEN_Pos) | SAI_xSLOTR_SLOTSZ_0;

  #ifdef ENABLE_I2S_OUT
      stm32l4_dma_enable(&dma2, &isr2, 0);
      SAI_Block_TypeDef *SAI2 = SAI1_Block_B;
      SAI2->CR1 = SAI_xCR1_DS_2 | SAI_xCR1_MONO | SAI_xCR1_CKSTR;
      SAI2->FRCR = (31 << SAI_xFRCR_FRL_Pos) | (15 << SAI_xFRCR_FSALL_Pos) | SAI_xFRCR_FSDEF | SAI_xFRCR_FSOFF;
      SAI2->SLOTR = SAI_xSLOTR_NBSLOT_0 | (0x0003 << SAI_xSLOTR_SLOTEN_Pos) | SAI_xSLOTR_SLOTSZ_0;
  #endif

  #ifdef ENABLE_SPDIF_OUT
      stm32l4_dma_enable(&dma2, &isr2, 0);
      SAI_Block_TypeDef *SAI2 = SAI1_Block_B;
      SAI2->CR1 = SAI_xCR1_PRTCFG_0;
  #endif

      stm32l4_system_saiclk_configure(SYSTEM_SAICLK_11289600);
  #ifdef ENABLE_SPDIF_OUT
      my_stm32l4_system_saiclk_configure_22579200();
  #endif
      extern const stm32l4_sai_pins_t g_SAIPins;
      stm32l4_gpio_pin_configure(g_SAIPins.sck, (GPIO_PUPD_NONE | GPIO_OSPEED_HIGH | GPIO_OTYPE_PUSHPULL | GPIO_MODE_ALTERNATE));
      stm32l4_gpio_pin_configure(g_SAIPins.fs, (GPIO_PUPD_NONE | GPIO_OSPEED_HIGH | GPIO_OTYPE_PUSHPULL | GPIO_MODE_ALTERNATE));
      stm32l4_gpio_pin_configure(g_SAIPins.sd, (GPIO_PUPD_NONE | GPIO_OSPEED_HIGH | GPIO_OTYPE_PUSHPULL | GPIO_MODE_ALTERNATE));
      stm32l4_dma_start(&dma, (uint32_t)&SAIx->DR, (uint32_t)dac_dma_buffer, AUDIO_BUFFER_SIZE * 2,
                        DMA_OPTION_EVENT_TRANSFER_DONE |
                        DMA_OPTION_EVENT_TRANSFER_HALF |
                        DMA_OPTION_MEMORY_TO_PERIPHERAL |
                        DMA_OPTION_PERIPHERAL_DATA_SIZE_32 |
                        DMA_OPTION_MEMORY_DATA_SIZE_16 |
                        DMA_OPTION_MEMORY_DATA_INCREMENT |
                        DMA_OPTION_PRIORITY_HIGH |
                        DMA_OPTION_CIRCULAR);
      SAIx->CR1 |= SAI_xCR1_DMAEN;

  #define SAIB_SCK g_SAIPins.sck
  #define SAIB_FS g_SAIPins.fs
  #define SAIB_SD g_SAIPins.sd
      
  #if PROFFIEBOARD_VERSION < 3
  #undef SAIB_FS
  #define SAIB_FS GPIO_PIN_PA4_SAI1_FS_B
  #endif    

  #ifdef ENABLE_I2S_OUT
      // Neopixel data 3 pin is SCK
      stm32l4_gpio_pin_configure(SAIB_SCK, (GPIO_PUPD_NONE | GPIO_OSPEED_HIGH | GPIO_OTYPE_PUSHPULL | GPIO_MODE_ALTERNATE));

      // Neopixel data 4 is FS, it is also possible to use GPIO_PIN_PB6_SAI1_FS_B, which is the power button pin.
      stm32l4_gpio_pin_configure(SAIB_FS, (GPIO_PUPD_NONE | GPIO_OSPEED_HIGH | GPIO_OTYPE_PUSHPULL | GPIO_MODE_ALTERNATE));

      // aux/Button2 button pin is DATA
      stm32l4_gpio_pin_configure(SAIB_SD, (GPIO_PUPD_NONE | GPIO_OSPEED_HIGH | GPIO_OTYPE_PUSHPULL | GPIO_MODE_ALTERNATE));
      stm32l4_dma_start(&dma2, (uint32_t)&SAI2->DR, (uint32_t)dac_dma_buffer2, AUDIO_BUFFER_SIZE * 2,
                        DMA_OPTION_EVENT_TRANSFER_DONE |
                        DMA_OPTION_EVENT_TRANSFER_HALF |
                        DMA_OPTION_MEMORY_TO_PERIPHERAL |
                        DMA_OPTION_PERIPHERAL_DATA_SIZE_32 |
                        DMA_OPTION_MEMORY_DATA_SIZE_16 |
                        DMA_OPTION_MEMORY_DATA_INCREMENT |
                        DMA_OPTION_PRIORITY_HIGH |
                        DMA_OPTION_CIRCULAR);
      SAI2->CR1 |= SAI_xCR1_DMAEN;
  #endif

  #ifdef ENABLE_SPDIF_OUT
      // aux button pin becomes S/PDIF out
      stm32l4_gpio_pin_configure(SAIB_SD, (GPIO_PUPD_NONE | GPIO_OSPEED_HIGH | GPIO_OTYPE_PUSHPULL | GPIO_MODE_ALTERNATE));
      stm32l4_dma_start(&dma2, (uint32_t)&SAI2->DR, (uint32_t)dac_dma_buffer2, AUDIO_BUFFER_SIZE * 2 * 2,
                        DMA_OPTION_EVENT_TRANSFER_DONE |
                        DMA_OPTION_EVENT_TRANSFER_HALF |
                        DMA_OPTION_MEMORY_TO_PERIPHERAL |
                        DMA_OPTION_PERIPHERAL_DATA_SIZE_32 |
                        DMA_OPTION_MEMORY_DATA_SIZE_32 |
                        DMA_OPTION_MEMORY_DATA_INCREMENT |
                        DMA_OPTION_PRIORITY_HIGH |
                        DMA_OPTION_CIRCULAR);
      SAI2->CR1 |= SAI_xCR1_DMAEN;
  #endif

      noInterrupts();
      if (!(SAIx->CR1 & SAI_xCR1_SAIEN))
      {
        SAIx->CR2 = SAI_xCR2_FTH_1;
        SAIx->CR1 |= SAI_xCR1_SAIEN;
      }
  #if defined(ENABLE_I2S_OUT) || defined(ENABLE_SPDIF_OUT)
      if (!(SAI2->CR1 & SAI_xCR1_SAIEN))
      {
        SAI2->CR2 = SAI_xCR2_FTH_1;
        SAI2->CR1 |= SAI_xCR1_SAIEN;
      }
  #endif // I2S || SPDIF
      interrupts();
  
#endif // end else ULTRAPROFFIE_DAC


  }

  void end() {
    if (!on_) return;
    on_ = false;
#if defined(ULTRAPROFFIE_DAC) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
    stm32l4_dac1.DACx->CR &= ~DAC_CR_EN1;     // Disable DAC output
    //stm32l4_gpio_pin_write(GPIO_PIN_PB4, 0);  // Disable amp TODO , remove from here maybe 
    stm32l4_timer_stop(&stm32l4_dac1Timer);   // start the timer 
    stm32l4_dma_stop(&dma);       // stop DMA
#else 
      SAI_Block_TypeDef *SAIx = SAI1_Block_A;

      SAIx->CR1 &=~ SAI_xCR1_SAIEN;
      // Poll until actually off
      while (SAIx->CR1 & SAI_xCR1_SAIEN);

  #if defined(ENABLE_I2S_OUT) || defined(ENABLE_SPDIF_OUT)
      SAI_Block_TypeDef *SAI2 = SAI1_Block_B;

      SAI2->CR1 &=~ SAI_xCR1_SAIEN;
      // Poll until actually off
      while (SAI2->CR1 & SAI_xCR1_SAIEN);
  #endif

      stm32l4_system_saiclk_configure(SYSTEM_SAICLK_NONE);

      stm32l4_dma_disable(&dma);
  #if defined(ENABLE_I2S_OUT) || defined(ENABLE_SPDIF_OUT)
      stm32l4_dma_disable(&dma2);
  #endif
      stm32l4_system_periph_disable(SYSTEM_PERIPH_SAI1);
      extern const stm32l4_sai_pins_t g_SAIPins;
      stm32l4_gpio_pin_configure(g_SAIPins.sck, (GPIO_PUPD_NONE | GPIO_MODE_ANALOG));
      stm32l4_gpio_pin_configure(g_SAIPins.fs, (GPIO_PUPD_NONE | GPIO_MODE_ANALOG));
      stm32l4_gpio_pin_configure(g_SAIPins.sd, (GPIO_PUPD_NONE | GPIO_MODE_ANALOG));

  #ifdef ENABLE_I2S_OUT
      stm32l4_gpio_pin_configure(GPIO_PIN_PB3_SAI1_SCK_B, (GPIO_PUPD_NONE | GPIO_MODE_ANALOG));
      stm32l4_gpio_pin_configure(GPIO_PIN_PA4_SAI1_FS_B, (GPIO_PUPD_NONE | GPIO_MODE_ANALOG));
      stm32l4_gpio_pin_configure(GPIO_PIN_PB5_SAI1_SD_B, (GPIO_PUPD_NONE | GPIO_MODE_ANALOG));
  #endif

  #ifdef ENABLE_SPDIF_OUT
      stm32l4_gpio_pin_configure(GPIO_PIN_PB5_SAI1_SD_B,  (GPIO_PUPD_NONE | GPIO_MODE_ANALOG));
  #endif

  
#endif 
  }

  bool Parse(const char* cmd, const char* arg) override {
#ifndef DISABLE_DIAGNOSTIC_COMMANDS
    if (!strcmp(cmd, "dacbuf")) {
      SAI_Block_TypeDef *SAIx = SAI1_Block_A;
      STDOUT.print("STATUS: ");
      STDOUT.print(SAIx->SR, HEX);
      STDOUT.print(" CR1: ");
      STDOUT.print(SAIx->CR1, HEX);
      STDOUT.print(" CR2: ");
      STDOUT.println(SAIx->CR2, HEX);
      STDOUT.print("Current position: ");
      STDOUT.println(((uint16_t*)current_position()) - dac_dma_buffer);
      for (size_t i = 0; i < NELEM(dac_dma_buffer); i++) {
        STDOUT.print(dac_dma_buffer[i]);
        if ((i & 0xf) == 0xf)
          STDOUT.println("");
        else
          STDOUT.print(" ");
      }
      STDOUT.println("");
      return true;
    }
#endif
    return false;
  }

  bool isSilent() {
     for (size_t i = 0; i < NELEM(dac_dma_buffer); i++)
       if (dac_dma_buffer[i] != dac_dma_buffer[0])
         return false;
     return true;
  }

  void Help() override {
    #if defined(COMMANDS_HELP) 
    STDOUT.println(" dacbuf - print the current contents of the dac buffer");
    #endif
  }

  // TODO: Replace with enable/disable
  void SetStream(class ProffieOSAudioStream* stream) {
    stream_ = stream;
  }

private:
  static uint32_t current_position() {
  return (uint32_t)(dac_dma_buffer + stm32l4_dma_count(&dma));
  }
  // Interrupt handler.
  // Fills the dma buffer with new sample data.
  static void isr(void* arg, unsigned long int event)

  {
    ScopedCycleCounter cc(audio_dma_interrupt_cycles);
    int16_t *dest;
    uint32_t saddr = current_position();
#ifdef ENABLE_SPDIF_OUT
    uint32_t *secondary;
#endif
#ifdef ENABLE_I2S_OUT
    uint16_t *secondary;
#endif

    if (saddr < (uint32_t)dac_dma_buffer + sizeof(dac_dma_buffer) / 2) {
      // DMA is transmitting the first half of the buffer
      // so we must fill the second half
      dest = (int16_t *)&dac_dma_buffer[AUDIO_BUFFER_SIZE*CHANNELS];
#if defined(ENABLE_SPDIF_OUT) || defined(ENABLE_I2S_OUT)
      secondary = dac_dma_buffer2 + AUDIO_BUFFER_SIZE * 2;
#endif
    } else {
      // DMA is transmitting the second half of the buffer
      // so we must fill the first half
      dest = (int16_t *)dac_dma_buffer;
#if defined(ENABLE_SPDIF_OUT) || defined(ENABLE_I2S_OUT)
      secondary = dac_dma_buffer2;
#endif
    }
#ifdef __IMXRT1062__
    int16_t *clear_cache = dest;
#endif

#ifndef LINE_OUT_VOLUME
// LINE_OUT_VOLUME can be defined as dynamic_mixer.get_volume() to follow normal volume.
#define LINE_OUT_VOLUME 2000
#endif      
    
#if defined(ENABLE_SPDIF_OUT) || defined(ENABLE_I2S_OUT) || defined(FILTER_CUTOFF_FREQUENCY)
#define DAC_GET_FLOATS
#endif

#ifdef DAC_GET_FLOATS
    float data[AUDIO_BUFFER_SIZE];
#else    
    int16_t data[AUDIO_BUFFER_SIZE];
#endif    
    int n = 0;
    if (stream_) {
      n = dynamic_mixer.read(data, AUDIO_BUFFER_SIZE);
    }
    while (n < AUDIO_BUFFER_SIZE) data[n++] = 0;

#if defined(ENABLE_SPDIF_OUT) || defined(ENABLE_I2S_OUT)
    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
#ifdef ENABLE_I2S_OUT
      *(secondary++) = clamptoi16(data[i] * (LINE_OUT_VOLUME));
#endif
#ifdef ENABLE_SPDIF_OUT
      int32_t sample24 = clamptoi24(data[i] * (LINE_OUT_VOLUME * 256));
      *(secondary++) = sample24 & 0xFFFFFF;
      *(secondary++) = sample24 & 0xFFFFFF;
#endif
    }
#endif  // GET_FLOATS

#ifdef FILTER_CUTOFF_FREQUENCY
    // Run the filter
    static_assert(AUDIO_BUFFER_SIZE % 4 == 0);
    for (int i = 0; i < AUDIO_BUFFER_SIZE; i+=4) {
      filter_.Run4(data + i);
    }
#endif    
    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
#ifdef DAC_GET_FLOATS
      int16_t sample = clamptoi16(data[i] * dynamic_mixer.get_volume());
#else   // GET_FLOATS
      int16_t sample = data[i];
#endif  // GET_FLOATS

#ifdef USE_I2S
        *(dest++) = sample;
  #if CHANNELS == 2
        // Duplicate sample to left and right channel.
        *(dest++) = sample;
  #endif
#elif defined(ULTRAPROFFIE_DAC)

    *(dest++) = (((uint16_t)sample) + 32768);   // was  32767

#else // I2S
      // For Teensy DAC
      *(dest++) = (((uint16_t)sample) + 32768) >> 4;
#endif
    }
    
#ifdef __IMXRT1062__
    arm_dcache_flush_delete(clear_cache, sizeof(dac_dma_buffer)/2);
#endif
  }

  static void isr2(void* arg, unsigned long int event) {}

  bool on_ = false;
  bool needs_setup_ = true;
  DMAMEM static uint16_t dac_dma_buffer[AUDIO_BUFFER_SIZE*2*CHANNELS];
#ifdef ENABLE_SPDIF_OUT
  DMAMEM static uint32_t dac_dma_buffer2[AUDIO_BUFFER_SIZE*2*2];
#endif
#ifdef ENABLE_I2S_OUT
  DMAMEM static uint16_t dac_dma_buffer2[AUDIO_BUFFER_SIZE*2*CHANNELS];
#endif
  static ProffieOSAudioStream * volatile stream_;
  static DMAChannel dma;
#if defined(ENABLE_I2S_OUT) || defined(ENABLE_SPDIF_OUT)
  static DMAChannel dma2;
#endif
#ifdef FILTER_CUTOFF_FREQUENCY
  static Filter::Biquad<
    Filter::Bilinear<
    Filter::BLT<
      Filter::ConvertToHighPass<
	Filter::ButterWorthProtoType<FILTER_ORDER>, FILTER_CUTOFF_FREQUENCY, AUDIO_RATE>>>> filter_;
#endif  
};


DMAChannel LS_DAC::dma;
#if defined(ENABLE_I2S_OUT) || defined(ENABLE_SPDIF_OUT)
DMAChannel LS_DAC::dma2;
#endif


ProffieOSAudioStream * volatile LS_DAC::stream_ = nullptr;
DMAMEM __attribute__((aligned(32))) uint16_t LS_DAC::dac_dma_buffer[AUDIO_BUFFER_SIZE*2*CHANNELS];
#ifdef ENABLE_SPDIF_OUT
DMAMEM __attribute__((aligned(32))) uint32_t LS_DAC::dac_dma_buffer2[AUDIO_BUFFER_SIZE*2*2];
#endif
#ifdef ENABLE_I2S_OUT
DMAMEM __attribute__((aligned(32))) uint16_t LS_DAC::dac_dma_buffer2[AUDIO_BUFFER_SIZE*2*CHANNELS];
#endif
#ifdef FILTER_CUTOFF_FREQUENCY
Filter::Biquad<
  Filter::Bilinear<
  Filter::BLT<
    Filter::ConvertToHighPass<
      Filter::ButterWorthProtoType<FILTER_ORDER>, FILTER_CUTOFF_FREQUENCY, AUDIO_RATE>>>> LS_DAC::filter_;
#endif  

LS_DAC dac;


#endif
