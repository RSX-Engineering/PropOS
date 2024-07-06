#ifndef BLADES_PWM_PIN_H
#define BLADES_PWM_PIN_H


// First some abstractions for controlling PWM pin
#ifdef ARDUINO_ARCH_ESP32   // ESP architecture

  
  #include <stdio.h>
  #include "driver/ledc.h"
  #include "esp_err.h"

  #define LEDC_TIMER              LEDC_TIMER_0
  #define LEDC_MODE               LEDC_LOW_SPEED_MODE
  #define LEDC_OUTPUT_IO          (22) // Define the output GPIO
  #define LEDC_CHANNEL            LEDC_CHANNEL_0
  #define LEDC_DUTY_RES           LEDC_TIMER_14_BIT // Set duty resolution to 13 bits
  #define LEDC_DUTY               (16384) // Set duty to 50%. ((2 ** 15) - 1) * 50% = 16383
  #define LEDC_FREQUENCY          (813) // Frequency in Hertz. Set frequency at 150 kHz

  #define PROFFIE_ESP_MAX_ANALOG_CH 4
  static struct channelPinSelection
  {
    uint8_t channel[PROFFIE_ESP_MAX_ANALOG_CH]= {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t pin[PROFFIE_ESP_MAX_ANALOG_CH] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t state[PROFFIE_ESP_MAX_ANALOG_CH] = {0, 0, 0, 0};
    
  } activeChannels;

  static bool SetupTimer(ledc_timer_t timerInstance)
  {
    esp_err_t result;
    if(timerInstance >= LEDC_TIMER_MAX)
      return false;             // we dont have a valid timer instance

    ledc_timer_config_t ledc_timer;
    ledc_timer.speed_mode       = LEDC_MODE;
    ledc_timer.timer_num        = LEDC_TIMER;
    ledc_timer.duty_resolution  = LEDC_DUTY_RES;
    ledc_timer.freq_hz          = LEDC_FREQUENCY;  // Set output frequency at 5 kHz
    ledc_timer.clk_cfg          = LEDC_USE_APB_CLK;

    result = ledc_timer_config(&ledc_timer);
    if(result == ESP_OK)
      return true;

    return false;
  }

  static int8_t PinTochannel(uint32_t pin)
  {
     for(uint8_t i = 0; i < PROFFIE_ESP_MAX_ANALOG_CH; i++)
     {
      if(activeChannels.pin[i] == pin)
      { // check state 
        if(!activeChannels.state[i])
          return -1;
        return activeChannels.channel[i];
      }
     }

     return -1;
  }
  static int8_t FindFreeTimerChannel(uint32_t pin)
  {
    int8_t channelSelector = -1;
    for(uint8_t i = 0; i < PROFFIE_ESP_MAX_ANALOG_CH; i++)
    {
      if (activeChannels.channel[i] == 0xFF)  // check if channel is free  , 0 means free
      { // we found a free channel 
        channelSelector = i;
        activeChannels.channel[i] = channelSelector;
        activeChannels.pin[i] = pin;  
        // find if the pin is not asign to other channel
        for(uint8_t j = 0; j < PROFFIE_ESP_MAX_ANALOG_CH; j++)
        {
          if(activeChannels.pin[j] == pin && activeChannels.channel[j])
          {
            channelSelector = -1;         // we found that the pin is used by other valid channel
            activeChannels.channel[i] = 0xFF;
            activeChannels.pin[i] = 0xFF;  
          }  

        }
        break;
      }
    }
    return channelSelector;
  }

  void LSanalogWriteSetup(uint32_t pin) 
  { 
    // find channel 
    int8_t channelSelector;
    static bool isInit = false;
    ledc_channel_config_t ledc_channel;
    esp_err_t result;

    channelSelector = FindFreeTimerChannel(pin);
    if(channelSelector < 0)
      return;                 // could not find the channel 
    
    if(!isInit) 
      isInit = SetupTimer(LEDC_TIMER);
    
    ledc_channel.speed_mode     = LEDC_MODE;
    ledc_channel.channel        = (ledc_channel_t)channelSelector;
    ledc_channel.timer_sel      = LEDC_TIMER;
    ledc_channel.intr_type      = LEDC_INTR_DISABLE;
    ledc_channel.gpio_num       = pin;
    ledc_channel.duty           = 0; // Set duty to 0%
    ledc_channel.hpoint         = 0;

    result = ledc_channel_config(&ledc_channel); // channel 0 cfg
    if(result == ESP_OK)
      activeChannels.state[channelSelector] = 1; // channel as init 
    else {
        activeChannels.state[channelSelector] = 0;      // freee channel because it was not init OKAY , to default values
        activeChannels.channel[channelSelector] = 0xFF; // to default values
        activeChannels.pin[channelSelector] = 0xFF;     // cto default values
    }
    // nrActiveChannel++;
  }

  void LSanalogWriteTeardown(uint32_t pin) 
  {
    int8_t channelOFPin;
    channelOFPin = PinTochannel(pin);
    if(channelOFPin < 0)
      return;
    // ledc_stop(LEDC_MODE, );
    ledc_stop(LEDC_MODE, (ledc_channel_t)channelOFPin, 0);
    // TODO maybe add timer disable 
  }

  void LSanalogWrite(uint32_t pin, int value) 
  {
    int8_t channelOFPin;
    channelOFPin = PinTochannel(pin);
    if(channelOFPin < 0)
      return;
    value >>= 1;
    if (value < 0) value = 0;  if (value > 16383) value = 16383;
    //STDOUT.print("P: ");STDOUT.print(pin);STDOUT.print(" V: ");STDOUT.println(value);
    ledc_set_duty(LEDC_MODE, (ledc_channel_t)channelOFPin, value);
    ledc_update_duty(LEDC_MODE, (ledc_channel_t)channelOFPin);      // Update duty to apply the new value
  }
  

#else
#include <stm32l4_timer.h>

namespace {

static stm32l4_timer_t stm32l4_pwm[PWM_INSTANCE_COUNT];
static uint8_t timer_use_counts[PWM_INSTANCE_COUNT];

#define PWM_SYNC_INSTANCE 3  // TIM15

#if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) // STM UltraProffies
static uint8_t activeChannels[4];
#endif

void SetupTimer(uint32_t instance) {
#ifdef PROFFIEBOARD
  timer_use_counts[instance]++;
  if (stm32l4_pwm[instance].state == TIMER_STATE_NONE) {
    stm32l4_timer_create(&stm32l4_pwm[instance], g_PWMInstances[instance], 15, 0);
  }

  if (stm32l4_pwm[instance].state == TIMER_STATE_INIT) {
    // 813 Hz, 32768 steps
    uint32_t carrier = 26666666;
    uint32_t modulus = 32768;
    uint32_t divider = stm32l4_timer_clock(&stm32l4_pwm[instance]) / carrier;
    
    if (divider == 0) divider = 1;
    
    stm32l4_timer_enable(&stm32l4_pwm[instance], divider -1, modulus -1, 0, NULL, NULL, 0);
    stm32l4_timer_start(&stm32l4_pwm[instance], false);
    if (instance != PWM_SYNC_INSTANCE)  {
      SetupTimer(PWM_SYNC_INSTANCE);
      // TIM16 cannot be synchronized in hardware, so let's do the best we can.
      // We use TIM15 to synchromize with, because it is used for PWM on all proffieboards.
      volatile uint32_t* from = &stm32l4_pwm[PWM_SYNC_INSTANCE].TIM->CNT;
      volatile uint32_t* to = &stm32l4_pwm[instance].TIM->CNT;
      noInterrupts();
      *to = *from + 10;
      interrupts();
    } else {
      // avoid slow startup
      stm32l4_pwm[instance].TIM->CNT = 0;
    }
    // Buffer counters from now on.
    stm32l4_pwm[instance].TIM->CR1 |= TIM_CR1_ARPE;
  }
#else
  if(instance == TIMER_INSTANCE_TIM2) {
    if (stm32l4_pwm[instance].state == TIMER_STATE_NONE) {
        stm32l4_timer_create(&stm32l4_pwm[instance], g_PWMInstances[instance], 10, 0);
    }
    if (stm32l4_pwm[instance].state == TIMER_STATE_INIT) {
        uint32_t carrier = 26666666;
        uint32_t modulus = 32768;
        uint32_t divider = stm32l4_timer_clock(&stm32l4_pwm[instance]) / carrier;
    
        if (divider == 0) divider = 1;
        stm32l4_timer_enable(&stm32l4_pwm[instance], divider-1, modulus -1, TIMER_OPTION_COUNT_PRELOAD, NULL, NULL, 0);
        stm32l4_pwm[instance].TIM->EGR = TIM_EGR_UG;          // update enable 
        stm32l4_timer_start(&stm32l4_pwm[instance], false);

    }
  }
#endif
}

void TeardownTimer(uint32_t instance) {
#ifdef PROFFIEBOARD
  if (0 == --timer_use_counts[instance]) {
    stm32l4_timer_stop(&stm32l4_pwm[instance]);
    stm32l4_timer_disable(&stm32l4_pwm[instance]);
    if (instance != PWM_SYNC_INSTANCE) {
      TeardownTimer(PWM_SYNC_INSTANCE);
    }
  }
#else 
  if(activeChannels[0] == 0 && activeChannels[1] == 0 &&  activeChannels[2] == 0 && activeChannels[3] == 0)
  {
    stm32l4_timer_stop(&stm32l4_pwm[instance]);
    stm32l4_timer_disable(&stm32l4_pwm[instance]);
  }
#endif
}

void LSanalogWriteSetup(uint32_t pin) {
  // Handle the case the pin isn't usable as PIO
  if (pin >= NUM_TOTAL_PINS || g_APinDescription[pin].GPIO == NULL) {
    STDOUT.print("Analog Setup: NOT A PIN: ");
    STDOUT.println(pin);
    return;
  }
  
  if (!(g_APinDescription[pin].attr & PIN_ATTR_PWM)) {
    STDOUT.println("Analog Setup: Pin is not configured for PWM: ");
    STDOUT.println(pin);
    return;
  }
  uint32_t instance = g_APinDescription[pin].pwm_instance;
#ifdef PROFFIEBOARD
  SetupTimer(instance);
  stm32l4_timer_channel(&stm32l4_pwm[instance], g_APinDescription[pin].pwm_channel, 0, TIMER_CONTROL_PWM);
  stm32l4_gpio_pin_configure(g_APinDescription[pin].pin, (GPIO_PUPD_NONE | GPIO_OSPEED_HIGH | GPIO_OTYPE_PUSHPULL | GPIO_MODE_ALTERNATE));
#else
  SetupTimer(instance);
  stm32l4_timer_channel(&stm32l4_pwm[instance], g_APinDescription[pin].pwm_channel, 0, TIMER_CONTROL_PWM); 
  stm32l4_gpio_pin_configure(g_APinDescription[pin].pin, (GPIO_PUPD_PULLDOWN | GPIO_OSPEED_HIGH | GPIO_OTYPE_PUSHPULL | GPIO_MODE_ALTERNATE));
  activeChannels[g_APinDescription[pin].pwm_channel] = 1;
#endif
}

void LSanalogWriteTeardown(uint32_t pin) {
#ifdef PROFFIEBOARD
  pinMode(pin, INPUT_ANALOG);
  TeardownTimer(g_APinDescription[pin].pwm_instance);
#else 
  activeChannels[g_APinDescription[pin].pwm_channel] = 0;
  // pinMode(pin, INPUT_ANALOG);	// TODO was active , chek on power manager 
  TeardownTimer(g_APinDescription[pin].pwm_instance);

#endif
}

void LSanalogWrite(uint32_t pin, int value) {
  TIM_TypeDef* TIM = stm32l4_pwm[g_APinDescription[pin].pwm_instance].TIM;
  if (value < 0) value = 0;  
  if (value > 32767) value = 32767;
  // stm32l4_timer_channel(&stm32l4_pwm[instance], g_APinDescription[pin].pwm_channel, value, TIMER_CONTROL_PWM);
  switch (g_APinDescription[pin].pwm_channel) {
    case TIMER_CHANNEL_1: TIM->CCR1 = value; break;
    case TIMER_CHANNEL_2: TIM->CCR2 = value; break;
    case TIMER_CHANNEL_3: TIM->CCR3 = value; break;
    case TIMER_CHANNEL_4: TIM->CCR4 = value; break;
    case TIMER_CHANNEL_5: TIM->CCR5 = value; break;
    case TIMER_CHANNEL_6: TIM->CCR6 = value; break;
  }
}

};
#endif


#endif
