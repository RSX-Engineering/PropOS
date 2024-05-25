#ifndef BLADES_STM32U5_WS2811_H
#define BLADES_STM32U5_WS2811_H
namespace {
const int timer_frequency = 80000000;  // 80Mhz
};

class WS2811Client {
public:
  virtual void done_callback() = 0;
  virtual int chunk_size() = 0;
  virtual int pin() const = 0;
  virtual int frequency() = 0;
  virtual int num_leds() = 0;
  virtual void read(uint8_t* dest) = 0;
  virtual uint8_t get_t0h() = 0;
  virtual uint8_t get_t1h() = 0;
  virtual void set01(uint8_t zero, uint8_t one) = 0;
  WS2811Client* volatile next_ws2811_client_ = nullptr;
};

#define PROFFIEOS_ASSERT(X) do {} while(0)

class WS2811Engine {
public:
  virtual void queue(WS2811Client* client) = 0;
  virtual void kick() = 0;
};

volatile bool ws2811_dma_done = true;

class NoInterruptScope {
public:
  NoInterruptScope() { noInterrupts(); }
  ~NoInterruptScope() { interrupts(); }
};

// Position where we are reading colors from the color buffer.
Color16* volatile color_buffer_ptr = color_buffer;
uint32_t color_buffer_size = 0;


// TIM_HandleTypeDef htim1;
// DMA_HandleTypeDef handle_GPDMA1_Channel15;

  class WS2811EngineSTM32L4 : public WS2811Engine {
  public:

    static const int instance = 2;    // static const
    #define WS2811_TIMER_INSTANCE TIMER_INSTANCE_TIM15

    void kick() override {
      if (!ws2811_dma_done) return;
      if (!client_) return;
      PROFFIEOS_ASSERT(color_buffer_size);
      show();
    }

    void queue(WS2811Client* client) override {
      client->next_ws2811_client_ = nullptr;
      PROFFIEOS_ASSERT(color_buffer_size);
      noInterrupts();
      if (!client_) {
        last_client_ = client_ = client;
      } else {
        PROFFIEOS_ASSERT(client_ != client);
        PROFFIEOS_ASSERT(last_client_ != client);
        last_client_->next_ws2811_client_ = client;
        last_client_ = client;
      }
      interrupts();
      kick();
    }

    void gdma_ll_init(void)
    {
      __HAL_RCC_GPDMA1_CLK_ENABLE();
      HAL_NVIC_SetPriority(GPDMA1_Channel15_IRQn, 0, 0);
      HAL_NVIC_EnableIRQ(GPDMA1_Channel15_IRQn);
    }

    void gpio_ll_init(TIM_HandleTypeDef* htim, uint32_t dpin)
    {
      GPIO_InitTypeDef GPIO_InitStruct = {0};
      if(htim->Instance==TIM1)
      {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        /**TIM1 GPIO Configuration
        PB14     ------> TIM1_CH2N
        */
       
        GPIO_InitStruct.Pin =  digitalPinToBitMask(dpin);
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
      }
    }

    /**
    * @brief TIM_PWM MSP Initialization
    * @param htim_pwm: TIM_PWM handle pointer
    * @retval None
    */
    void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* htim_pwm)
    {
      if(htim_pwm->Instance==TIM1)
      {
        /* Peripheral clock enable */
        __HAL_RCC_TIM1_CLK_ENABLE();

        /* TIM1 DMA Init */
        /* GPDMA1_REQUEST_TIM1_CH2 Init */
        dma_.Instance = GPDMA1_Channel15;
        dma_.Init.Request = GPDMA1_REQUEST_TIM1_CH2;
        dma_.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
        dma_.Init.Direction = DMA_PERIPH_TO_MEMORY;
        dma_.Init.SrcInc = DMA_SINC_FIXED;
        dma_.Init.DestInc = DMA_DINC_FIXED;
        dma_.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD;
        dma_.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
        dma_.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
        dma_.Init.SrcBurstLength = 1;
        dma_.Init.DestBurstLength = 1;
        dma_.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
        dma_.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
        dma_.Init.Mode = DMA_NORMAL;
        if (HAL_DMA_Init(&dma_) != HAL_OK)
        {
          Error_Handler();
        }

        __HAL_LINKDMA(htim_pwm, hdma[TIM_DMA_ID_CC2], dma_);

        if (HAL_DMA_ConfigChannelAttributes(&dma_, DMA_CHANNEL_NPRIV) != HAL_OK)
        {
          Error_Handler();
        }

        if (HAL_DMA_RegisterCallback(&dma_, HAL_DMA_XFER_CPLT_CB_ID, dma_refill_callback1) != HAL_OK)
        {
          Error_Handler();
        }
        if (HAL_DMA_RegisterCallback(&dma_, HAL_DMA_XFER_HALFCPLT_CB_ID, dma_done_callback) != HAL_OK)
        {
          Error_Handler();
        }
      }
    }

    void tim_ll_init(void)
    {
      TIM_MasterConfigTypeDef sMasterConfig = {0};
      TIM_OC_InitTypeDef sConfigOC = {0};
      TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

      timer_.Instance = TIM1;
      timer_.Init.Prescaler = 160-1;
      timer_.Init.CounterMode = TIM_COUNTERMODE_UP;
      timer_.Init.Period = 100;
      timer_.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
      timer_.Init.RepetitionCounter = 0;
      timer_.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
      if (HAL_TIM_PWM_Init(&timer_) != HAL_OK)
      {
        Error_Handler();
      }
      sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
      sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
      sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
      if (HAL_TIMEx_MasterConfigSynchronization(&timer_, &sMasterConfig) != HAL_OK)
      {
        Error_Handler();
      }
      sConfigOC.OCMode = TIM_OCMODE_PWM1;
      sConfigOC.Pulse = 0;
      sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
      sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
      sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
      sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
      sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
      if (HAL_TIM_PWM_ConfigChannel(&timer_, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
      {
        Error_Handler();
      }
      sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
      sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
      sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
      sBreakDeadTimeConfig.DeadTime = 0;
      sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
      sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
      sBreakDeadTimeConfig.BreakFilter = 0;
      sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
      sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
      sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
      sBreakDeadTimeConfig.Break2Filter = 0;
      sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
      sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
      if (HAL_TIMEx_ConfigBreakDeadTime(&timer_, &sBreakDeadTimeConfig) != HAL_OK)
      {
        Error_Handler();
      }
      // HAL_TIM_MspPostInit(&timer_);

  }

    
    WS2811EngineSTM32L4() {
      // Priority is 1 step above audio, because this code freezes if we
      // miss an interrupt. If we miss an audio interrupt, we just get a small glitch.
      gdma_ll_init();
      tim_ll_init();
      //stm32l4_gpio_pin_configure(GPIO_PIN_PB2, GPIO_MODE_OUTPUT | GPIO_OTYPE_PUSHPULL | GPIO_OSPEED_LOW | GPIO_PUPD_PULLDOWN);

    }

    ~WS2811EngineSTM32L4() {
      __HAL_RCC_TIM1_CLK_DISABLE();
      /* TIM1 DMA DeInit */
      HAL_DMA_DeInit(timer_.hdma[TIM_DMA_ID_CC2]);
      HAL_DMA_DeInit(timer_.hdma[TIM_DMA_ID_CC1]);
      HAL_DMA_DeInit(timer_.hdma[TIM_DMA_ID_CC3]);
    }
    
    static void flush_dma(DMA_HandleTypeDef *dma) {
      HAL_DMA_Abort(dma);
      HAL_DMA_DeInit(dma);
    }

    void DoRead(uint8_t* dest) {
      if (read_calls_) {
        read_calls_--;
        client_->read(dest);
      } else {
        memset(dest, 0, chunk_size_);
      }
    }

    void FillTo(uint8_t* end) {
      // Faster, but assumes unaligned memory access is ok.
      while (dest_ < end) {
        if (!chunk_bits_) {
          while (dest_ <= end - chunk_size_) {
            DoRead(dest_);
            dest_ += chunk_size_;
          }
          DoRead((uint8_t*)displayMemory);
          chunk_bits_ = chunk_size_;
        }
        int to_copy = std::min<int>(end - dest_, chunk_bits_);
        memcpy(dest_, displayMemory + chunk_size_ - chunk_bits_, to_copy);
        dest_ += to_copy;
        chunk_bits_ -= to_copy;
      }
    }

    void Fill(bool top) {
      if (dest_ == end_) dest_ = begin_;
      if (top) {
        FillTo(half_);
      } else {
        FillTo(end_);
      }
    }
    
    void show()  {
      WS2811Client* client = client_;
      PROFFIEOS_ASSERT(client);
      int pin = client->pin();
      int leds = client->num_leds();
      int frequency = client->frequency();
      while (!ws2811_dma_done) yield();

      chunk_size_ = client->chunk_size();
      int total_chunks = (sizeof(displayMemory) - 1) / chunk_size_ - 1;
      int bits = leds * chunk_size_;
      int chunks_per_interrupt = total_chunks / 2;
      bool CIRCULAR = total_chunks < leds;
      read_calls_ = leds;
      if (chunks_per_interrupt > 32 && CIRCULAR) chunks_per_interrupt = 32;
      bits_per_interrupt_ = chunks_per_interrupt * chunk_size_;

      // Divvy up the displayMemory into three regions:
      // 1: A chunk for reading data into (one chunk)  (displayMemory)
      // 2: upper dma buffer                           (begin_)
      // 3: lower dma buffer                           (half_)
      //                                               (end_)
      
      begin_ = (uint8_t*)displayMemory;
      begin_ += chunk_size_;
      dest_ = begin_;
      half_ = begin_ + chunks_per_interrupt * chunk_size_;
      end_ = half_ + chunks_per_interrupt * chunk_size_;
      chunk_bits_ = 0;
      sent_ = 0;

      // int pulse_len = timer_frequency / frequency;
      // int instance = g_APinDescription[pin].pwm_instance;
      // int divider = stm32l4_timer_clock(timer()) / timer_frequency;


      ws2811_dma_done = false;
      pin_ = pin;
      gpio_ll_init(&timer_, pin);
      currentContext = this;
      client->set01(client->get_t0h(), client->get_t1h());
      Fill(false);
      uint32_t channel = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(pin_), PinMap_PWM));
      HAL_TIM_PWM_Start_DMA(&timer_, channel, (uint32_t*)&begin_, bits + 1);

      
    }
    
    void DoRefill1() {
      sent_ += bits_per_interrupt_;
      if (sent_ > bits_to_send_) {
        DoDoneCB();
      } else {
        Fill(dma_.Instance->CTR1 >= (half_ - begin_));
      }
    }
    
    static void dma_refill_callback1(DMA_HandleTypeDef *hdma) {
      if(currentContext) ((WS2811EngineSTM32L4*)currentContext)->DoRefill1();

    }
    // static void dma_done_callback_ignore(void* context, uint32_t events) {}
    static void dma_done_callback(DMA_HandleTypeDef *hdma) {
      if(currentContext) ((WS2811EngineSTM32L4*)currentContext)->DoDoneCB();
    }
    void DoDoneCB() {
      // Set the pin to low, normal output mode. This will keep the pin low even if we
      // re-use the timer for another show() call.
      digitalWrite(pin_, LOW);
      // stm32l4_dma_disable(&dma3_);
      uint32_t channel = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(pin_), PinMap_PWM));
      HAL_TIM_PWM_Stop_DMA(&timer_, channel);
      ws2811_dma_done = true;
      client_->done_callback();
      noInterrupts();
      client_ = client_->next_ws2811_client_;
      PROFFIEOS_ASSERT( !client_ || color_buffer_size );
      interrupts();
      
      static_kick(this);
    }

    static void static_kick(void *context) {
      WS2811EngineSTM32L4* engine = ((WS2811EngineSTM32L4*)context);
      engine->kick();
    }

    private:
    static uint8_t* begin_;
    static uint8_t* half_;
    static uint8_t* end_; 
    static uint8_t* dest_;
    static WS2811Client* volatile client_;
    static WS2811Client* volatile last_client_;
    static uint32_t chunk_bits_;
    static uint32_t chunk_size_;
    static uint32_t bits_per_interrupt_;

    static uint32_t sent_;

    static uint32_t bits_to_send_;
    static uint32_t read_calls_;

    static int pin_;
    static volatile uint8_t bit_;
    static TIM_HandleTypeDef timer_;
    static DMA_HandleTypeDef dma_;
    static void *currentContext; 

  };

WS2811Client* volatile WS2811EngineSTM32L4::client_;
WS2811Client* volatile WS2811EngineSTM32L4::last_client_;
uint32_t WS2811EngineSTM32L4::chunk_bits_;
uint32_t WS2811EngineSTM32L4::chunk_size_;
uint32_t WS2811EngineSTM32L4::bits_per_interrupt_;
uint32_t WS2811EngineSTM32L4::sent_;
uint32_t WS2811EngineSTM32L4::bits_to_send_;
uint32_t WS2811EngineSTM32L4::read_calls_;
int WS2811EngineSTM32L4::pin_;
void* WS2811EngineSTM32L4::currentContext = nullptr;
uint8_t* WS2811EngineSTM32L4::begin_;
uint8_t* WS2811EngineSTM32L4::end_;
uint8_t* WS2811EngineSTM32L4::half_;
uint8_t* WS2811EngineSTM32L4::dest_;
TIM_HandleTypeDef WS2811EngineSTM32L4::timer_;
DMA_HandleTypeDef WS2811EngineSTM32L4::dma_;
volatile uint8_t WS2811EngineSTM32L4::bit_;

WS2811Engine* GetWS2811Engine(int pin) {
  static WS2811EngineSTM32L4 engine;
  if (pin < 0) return nullptr;
  return &engine;
}

// Usage:
// beginframe
// fill color buffer
// endframe
template<Color8::Byteorder BYTEORDER>
class WS2811PinBase : public WS2811Client, public WS2811PIN {
public:
  // Note, t0h_us/t1h_us are out of 1250 us, even if the frequency is
  // not 800kHz. If you're reading the datasheet for a 400Hz strip, you
  // would need to divide by two.
  WS2811PinBase(int num_leds,
		int8_t pin,
		int frequency,
		int reset_us,
		int t0h_us = 294,
		int t1h_us = 862) {
    pin_ = pin;
    engine_ = GetWS2811Engine(pin);
    num_leds_ = num_leds;
    frequency_ = frequency;
    reset_us_ = reset_us;
    int pulse_len = timer_frequency / frequency;
    t0h_ = pulse_len * t0h_us / 1250;
    t1h_ = pulse_len * t1h_us / 1250;
    // installedBrightness = 65535;      // default installed brightness = 50%, will be updated at install
    // setInstalledBrightness(0.7);
    // Serial.begin(115200);
    // Serial.print("[WS2811PinBase constructor]: "); Serial.print(num_leds); Serial.print(" LEDs at pin "); Serial.print(pin); Serial.print(", freq="); Serial.print(frequency);
    // Serial.print(", reset="); Serial.print(reset_us); Serial.print(", t0h="); Serial.print(t0h_us); Serial.print(", t1h="); Serial.print(t1h_us);
    // Serial.print(". Byteorder = "); Serial.println(BYTEORDER);
  }

  bool IsReadyForBeginFrame() override {
    if (num_leds_ > NELEM(color_buffer)) {
      STDOUT.print("Display memory is not big enough, increase maxLedsPerStrip!");
      return false;
    }
    return NELEM(color_buffer) - color_buffer_size >= num_leds_;
  }

  Color16* BeginFrame() override {
    while (!IsReadyForBeginFrame()) yield();

    noInterrupts();
    Color16 *ret = color_buffer_ptr + color_buffer_size;
    interrupts();

    if (ret >= color_buffer + NELEM(color_buffer)) ret -= NELEM(color_buffer);
    return ret;
  }

  bool IsReadyForEndFrame() override {
    return done_ && (micros() - done_time_us_) > reset_us_;
  }

  void EndFrame() override {
    color_buffer_size += num_leds_;
    // atomic_fetch_add(&color_buffer_size, num_leds_);
    if (!engine_) return;
    while (!IsReadyForEndFrame()) yield();
    frame_num_++;

    if (engine_) {
      done_ = false;
      engine_->queue(this);
    }

  }

  int num_leds() const override { return num_leds_; }
  Color8::Byteorder get_byteorder() const override { return BYTEORDER; }
  void Enable(bool on) override {
    pinMode(pin_, on ? OUTPUT : INPUT_ANALOG);
  }
  int pin() const override { return pin_; }

private:
  void done_callback() override {
    done_time_us_ = micros();
    done_ = true;
  }

  // #define INSTALLED_BRIGHTNESS installedBrightness
  #define INSTALLED_BRIGHTNESS (uint32_t)(256 * 3*65535 * 0.5)


  void read(uint8_t* dest) override __attribute__((optimize("Ofast"))) { // good
  // void read(uint8_t* dest) override __attribute__((optimize("O0"))) { 
    PROFFIEOS_ASSERT(color_buffer_size);
    Color16* pos = color_buffer_ptr;
    uint32_t* output = (uint32_t*) dest;
      // Color8 color = pos->dither(frame_num_, pos - color_buffer);   // this is preposterous!    
      
      // Current-saturating brightness
      uint32_t energyScale = pos->r + pos->g + pos->b; // 3 * 16-bit color
      uint32_t color32;
      Color8 color;
      if (energyScale <= INSTALLED_BRIGHTNESS/256) {
          // just scale color16 to color8, no additional scaling
          color.r = *(((uint8_t*)&(pos->r))+1);         // Take 8 msb
          color.g = *(((uint8_t*)&(pos->g))+1);         // Take 8 msb
          color.b = *(((uint8_t*)&(pos->b))+1);         // Take 8 msb
      } else {
        energyScale = INSTALLED_BRIGHTNESS / energyScale;     // 256 * scale
        color32 = pos->r * energyScale; // 65536 * scale * color8
        color.r = *(((uint8_t*)&color32)+2);    // Take 8 msb
        color32 = pos->g * energyScale; // 65536 * scale * color8
        color.g = *(((uint8_t*)&color32)+2);    // Take 8 msb
        color32 = pos->b * energyScale; // 65536 * scale * color8
        color.b = *(((uint8_t*)&color32)+2);    // Take 8 msb
        // STDOUT.print("energyScale = "); STDOUT.println(energyScale);

      }

    uint32_t tmp;
    if (Color8::inline_num_bytes(BYTEORDER) == 4) {
      tmp = GETBYTE<BYTEORDER, 3>(color) * 0x8040201U;
      *(output++) = zero4X_ ^ ((tmp >> 7) & 0x01010101U) * one_minus_zero_;
      *(output++) = zero4X_ ^ ((tmp >> 3) & 0x01010101U) * one_minus_zero_;
    }
    tmp = GETBYTE<BYTEORDER, 2>(color) * 0x8040201U;
    *(output++) = zero4X_ ^ ((tmp >> 7) & 0x01010101U) * one_minus_zero_;
    *(output++) = zero4X_ ^ ((tmp >> 3) & 0x01010101U) * one_minus_zero_;
    tmp = GETBYTE<BYTEORDER, 1>(color) * 0x8040201U;
    *(output++) = zero4X_ ^ ((tmp >> 7) & 0x01010101U) * one_minus_zero_;
    *(output++) = zero4X_ ^ ((tmp >> 3) & 0x01010101U) * one_minus_zero_;
    tmp = GETBYTE<BYTEORDER, 0>(color) * 0x8040201U;
    *(output++) = zero4X_ ^ ((tmp >> 7) & 0x01010101U) * one_minus_zero_;
    *(output++) = zero4X_ ^ ((tmp >> 3) & 0x01010101U) * one_minus_zero_;

    pos++;
    if (pos == color_buffer + NELEM(color_buffer)) pos = color_buffer;
    color_buffer_size -= 1;
    // atomic_fetch_sub(&color_buffer_size, 1);
    color_buffer_ptr = pos;
  }

  int chunk_size() override {
    return Color8::num_bytes(BYTEORDER) * 8;
  }
  
  int frequency() override { return frequency_; }
  int num_leds() override { return num_leds_; }

  void set01(uint8_t zero, uint8_t one) override {
    zero4X_ = zero * 0x01010101;
    one_minus_zero_ = one ^ zero;
  }

  uint8_t get_t0h() override { return t0h_; }
  uint8_t get_t1h() override { return t1h_; }


  WS2811Engine* engine_;
  int8_t pin_;
  uint8_t frame_num_ = 0;
  uint16_t num_leds_;
  int frequency_;
  uint32_t reset_us_;

  // These are in timer units, not us
  uint8_t t0h_;
  uint8_t t1h_;

  uint32_t one_minus_zero_;
  uint32_t zero4X_;

  volatile bool done_ = true;
  volatile uint32_t done_time_us_ = 0;
};

template<int LEDS, int PIN, Color8::Byteorder BYTEORDER, int frequency=800000, int reset_us=300, int t0h=294, int t1h=892>
class WS2811Pin : public WS2811PinBase<BYTEORDER> {
public:
  WS2811Pin() : WS2811PinBase<BYTEORDER>(LEDS, PIN, frequency, reset_us, t0h, t1h) { }
};

#endif

