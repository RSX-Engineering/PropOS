#ifndef SOUND_DAC_U5_H
#define SOUND_DAC_U5_H

// #include "filter.h"

#include <Arduino.h>
#include "dynamic_mixer.h"

// TODO u5: add code

#define CHANNELS 2
// #define I2S_NUM_FRAMES_TO_SEND 512
typedef struct
{
  uint16_t left;
  uint16_t right;
}I2S_Frame_t;

bool SoundActive();         // defined later in sound.h
void SetupStandardAudio();  // defined later in sound.h


class LS_DAC : CommandParser, Looper, public PowerSubscriber {
    public:
      LS_DAC() : CommandParser(), Looper(), PowerSubscriber(pwr4_Amplif) {
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

      /**
        * @brief DAC1 Initialization Function
        * @param None
        * @retval None
        */
      void dac_ll_init(void)
      {
        DAC_ChannelConfTypeDef sConfig = {0};
        DAC_AutonomousModeConfTypeDef sAutonomousMode = {0};
        /** DAC Initialization
        */
        hdac1.Instance = DAC1;
        if (HAL_DAC_Init(&hdac1) != HAL_OK)
        {
          Error_Handler();
        }

        /** DAC channel OUT1 config
        */
        sConfig.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_DISABLE;
        sConfig.DAC_DMADoubleDataMode = DISABLE;
        sConfig.DAC_SignedFormat = DISABLE;
        sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
        sConfig.DAC_Trigger = DAC_TRIGGER_T1_TRGO;
        sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
        sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
        sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
        if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
        {
          Error_Handler();
        }

        /** Configure Autonomous Mode
        */
        sAutonomousMode.AutonomousModeState = DAC_AUTONOMOUS_MODE_DISABLE;
        if (HAL_DACEx_SetConfigAutonomousMode(&hdac1, &sAutonomousMode) != HAL_OK)
        {
          Error_Handler();
        }
        /* DAC calibration */
        if (HAL_DACEx_SelfCalibrate(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
        {
          Error_Handler();
        }
      }

      /**
        * @brief GPDMA1 Initialization Function
        * @param None
        * @retval None
        */
      void gdma_ll_init(void)
      {
        /* Peripheral clock enable */
        __HAL_RCC_GPDMA1_CLK_ENABLE();

        /* GPDMA1 interrupt Init */
        HAL_NVIC_SetPriority(GPDMA1_Channel10_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(GPDMA1_Channel10_IRQn);

        handle_GPDMA1_Channel10.Instance = GPDMA1_Channel10;
        handle_GPDMA1_Channel10.InitLinkedList.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
        handle_GPDMA1_Channel10.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
        handle_GPDMA1_Channel10.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT1;
        handle_GPDMA1_Channel10.InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
        handle_GPDMA1_Channel10.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
        if (HAL_DMAEx_List_Init(&handle_GPDMA1_Channel10) != HAL_OK)
        {
          Error_Handler();
        }
        if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel10, DMA_CHANNEL_NPRIV) != HAL_OK)
        {
          Error_Handler();
        }

        if (HAL_DMA_RegisterCallback(&handle_GPDMA1_Channel10, HAL_DMA_XFER_CPLT_CB_ID, isr) != HAL_OK)
        {
          Error_Handler();
        }

      }

      /**
      * @brief TIM1 Initialization Function
      * @param None
      * @retval None
      */
      void tim2_ll_init(void)
      {
        TIM_SlaveConfigTypeDef sSlaveConfig = {0};
        TIM_MasterConfigTypeDef sMasterConfig = {0};

        htim2.Instance = TIM2;
        htim2.Init.Prescaler = 0;
        htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
        htim2.Init.Period = 3628;
        htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        htim2.Init.RepetitionCounter = 0;
        htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
        {
          Error_Handler();
        }
        sSlaveConfig.SlaveMode = TIM_SLAVEMODE_DISABLE;
        sSlaveConfig.InputTrigger = TIM_TS_ITR1;
        if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
        {
          Error_Handler();
        }
        sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
        sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
        {
          Error_Handler();
        }
        /* TIM signal generation to trigger the DAC */
        if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
        {
          Error_Handler();
        }
        
      }

      /**
      * @brief Implemented HAL_DAC_MspInit -> it will no longer executed it weak form
      * This function configures the hardware resources used in this example
      * @param hdac: DAC handle pointer
      * @retval None
      */
      void HAL_DAC_MspInit(DAC_HandleTypeDef* hdac)
      {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
        if(hdac->Instance==DAC1)
        {

        /** Initializes the peripherals clock
        */
          PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADCDAC|RCC_PERIPHCLK_DAC1;
          PeriphClkInit.AdcDacClockSelection = RCC_ADCDACCLKSOURCE_HSI;
          PeriphClkInit.Dac1ClockSelection = RCC_DAC1CLKSOURCE_LSI;
          if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
          {
            Error_Handler();
          }

          /* Peripheral clock enable */
          __HAL_RCC_DAC1_CLK_ENABLE();

          __HAL_RCC_GPIOA_CLK_ENABLE();
          /**DAC1 GPIO Configuration
          PA4     ------> DAC1_OUT1
          */
          GPIO_InitStruct.Pin = GPIO_PIN_4;
          GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
          GPIO_InitStruct.Pull = GPIO_NOPULL;
          HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        }

      }


      void Setup() override {
        if (!needs_setup_) return;
        dac_ll_init();
        gdma_ll_init();
        tim2_ll_init();

      }

      void begin()
      {
        if (on_) return;
        on_ = true;
        Setup();    
        // TODO u5: add code

      }

      void end()
      {
        if (!on_) return;
        on_ = false;
        // Gpio Deinit
        HAL_DMA_Abort(&handle_GPDMA1_Channel10);
        __HAL_RCC_GPDMA1_CLK_DISABLE();
        HAL_TIM_Base_DeInit(&htim2);
        HAL_TIM_Base_Stop(&htim2);
        __HAL_RCC_DAC1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_4);
      }

      // void unistall()
      // {
      //   // TODO u5: add code 
      //   needs_setup_ = true;
      // }

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

 static void isr(DMA_HandleTypeDef *hdma)
 {
    int16_t *dest;
    uint32_t saddr = current_position();
    if (saddr < (uint32_t)dac_dma_buffer + sizeof(dac_dma_buffer) / 2) {
      // DMA is transmitting the first half of the buffer
      // so we must fill the second half
      dest = (int16_t *)&dac_dma_buffer[AUDIO_BUFFER_SIZE*CHANNELS];

    } else {
      // DMA is transmitting the second half of the buffer
      // so we must fill the first half
      dest = (int16_t *)dac_dma_buffer;
    }  
    
    int16_t data[AUDIO_BUFFER_SIZE]; 
    int n = 0;
    if (stream_) {
      n = dynamic_mixer.read(data, AUDIO_BUFFER_SIZE);
    }
    while (n < AUDIO_BUFFER_SIZE) data[n++] = 0;

    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
      int16_t sample = data[i];

      *(dest++) = (((uint16_t)sample) + 32768);   // was  32767

    }
    

  }

      // static void isr2(void* arg, unsigned long int event) {}

      bool on_ = false;
      static bool needs_setup_;
      static bool createTask_;
      static ProffieOSAudioStream * volatile stream_;
      // static I2S_Frame_t frames[AUDIO_BUFFER_SIZE];
      static DAC_HandleTypeDef hdac1;
      static DMA_HandleTypeDef handle_GPDMA1_Channel10;
      static TIM_HandleTypeDef htim2;
      static uint16_t dac_dma_buffer[AUDIO_BUFFER_SIZE*CHANNELS];
      // static uint16_t dac_dma_buffer2[AUDIO_BUFFER_SIZE*2*CHANNELS];

    };

    ProffieOSAudioStream * volatile LS_DAC::stream_ = nullptr;
    bool LS_DAC::needs_setup_ = true;
    DAC_HandleTypeDef LS_DAC::hdac1;
    DMA_HandleTypeDef LS_DAC::handle_GPDMA1_Channel10;
    TIM_HandleTypeDef LS_DAC::htim2;
    //__attribute__((aligned(32))) I2S_Frame_t LS_DAC::frames[AUDIO_BUFFER_SIZE];
    __attribute__((aligned(32))) uint16_t LS_DAC::dac_dma_buffer[AUDIO_BUFFER_SIZE*CHANNELS];
    // __attribute__((aligned(32))) uint16_t LS_DAC::dac_dma_buffer2[AUDIO_BUFFER_SIZE*2*CHANNELS];



LS_DAC dac;

// void xDestroyDAComponents()
// {
//   dac.unistall();
// }

#endif
