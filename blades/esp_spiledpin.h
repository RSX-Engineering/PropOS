#ifndef ESP_SPI_WS2812_H_
#define ESP_SPI_WS2812_H_

#include <stdio.h>
#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define LED_MAX_NBER_LEDS 144 //24*2
// #define LED_DMA_BUFFER_SIZE ((LED_MAX_NBER_LEDS * 16 * (24/4)))+1
#define LED_RESET_FRAME_INDEX 60
#define LED_DMA_BUFFER_SIZE LED_RESET_FRAME_INDEX + ((LED_MAX_NBER_LEDS * 16 * (24/4))) + 4

#define LED_PIN GPIO_NUM_11
#define INSTALLED_BRIGHTNESS (uint32_t)(256 * 3*65535 * 0.5)
#define SPI_STRIP_SPEED 3200000 // HZ 3.2 MHz

uint16_t dma16BitEncode[16] = {0x8888, 0x8C88, 0xC888, 0xCC88, 0x888C, 0x8C8C, 0xC88C, 0xCC8C, 0x88C8, 0x8CC8, 0xC8C8, 0xCCC8, 0x88CC, 0x8CCC, 0xC8CC, 0xCCCC};
Color16* volatile color_buffer_ptr = color_buffer;

template<Color8::Byteorder BYTEORDER>
class ESPILedPinBase : public WS2811PIN {
public:
    ESPILedPinBase(int LEDS, int PIN, int frequency, int reset_us, int t1h, int t0h) :
        num_leds_(LEDS),
        pin_(PIN),
        frequency_(frequency),
        reset_us_(reset_us),
        t1h_(t1h),
        t0h_(t0h)
    {
        inited = false;
        memset(&SPI_settings, 0, sizeof(STRIP_SPI_settings_t));
        memset(&trans_desc, 0, sizeof(trans_desc));
        transQueued = 0;
        ledstripDMAbuffer = NULL;
        initSPIws2812(PIN);
    }

    bool IsReadyForBeginFrame() { return true; }
    bool IsReadyForEndFrame() {
        esp_err_t ret;
        if(!transQueued) return true;
        spi_transaction_t *ret_trans;
        ret = spi_device_get_trans_result(SPI_settings.spi, &ret_trans, portMAX_DELAY);
        if(ret == ESP_OK)
        {
            transQueued = 0;
            return true;
        }
        return false; 
    }

    Color16* BeginFrame() {
    //for (int i = Color8::num_bytes(byteorder_); i >= 0; i--) OutByte(0);
    // here should be the reset frame i belive 
    return color_buffer;
    }

    void EndFrame() __attribute__((optimize("Ofast")))
    {
        if(transQueued) return;
        Color16* pos = color_buffer_ptr;
        if(num_leds_ >= LED_MAX_NBER_LEDS) num_leds_ = LED_MAX_NBER_LEDS;
        int n = LED_RESET_FRAME_INDEX;

        for (uint32_t index = 0; index < num_leds_; index++) 
        {
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
            // Encode color to strip byte color 
            //GREEN 
            ledstripDMAbuffer[n++] = dma16BitEncode[0x0f & (color.g >>4)];  // encode first 4 MSB of color to string encoding
            ledstripDMAbuffer[n++] = dma16BitEncode[0x0f & (color.g)];      // encode last MSB of color to string encoding        
            //RED
            ledstripDMAbuffer[n++] = dma16BitEncode[0x0f & (color.r >>4)];
            ledstripDMAbuffer[n++] = dma16BitEncode[0x0f & (color.r)];
            //BLUE
            ledstripDMAbuffer[n++] = dma16BitEncode[0x0f & (color.b >>4)];
            ledstripDMAbuffer[n++] = dma16BitEncode[0x0f & (color.b)];

            pos++;
        }
        // fill rest of dma buffer with zeros 
        for (uint32_t index = num_leds_; index < LED_MAX_NBER_LEDS; index++) {
            ledstripDMAbuffer[n++] = dma16BitEncode[0];
            ledstripDMAbuffer[n++] = dma16BitEncode[0];
        }

        led_strip_update();
    }

    int num_leds() const override { return num_leds_; }
    Color8::Byteorder get_byteorder() const override { return BYTEORDER; }

    void Enable(bool on) override {

    }

private:
    typedef struct {
        spi_host_device_t host;
        int dma_chan;
        spi_bus_config_t buscfg;
        spi_device_interface_config_t devcfg;
        spi_device_handle_t spi;
    } STRIP_SPI_settings_t;

    void led_strip_update() {
        esp_err_t ret;
        if(!inited || transQueued) return;

        memset(&trans_desc, 0, sizeof(spi_transaction_t));
        trans_desc.length = LED_DMA_BUFFER_SIZE * 8; //length is in bits
        trans_desc.tx_buffer = ledstripDMAbuffer;
       // ret = spi_device_transmit(SPI_settings.spi, &trans_desc);

        //ToDo: check if any spi transfers in flight
        ret = spi_device_queue_trans(SPI_settings.spi, &trans_desc, portMAX_DELAY);
        // if (ret != ESP_OK) return ret;
        transQueued = 1;

  
        // if (ret != ESP_OK) return ret;

        // ESP_ERROR_CHECK(err);
    }
    
    spi_host_device_t getFreeSPI()
    {
        if(!spiPeriphState[0])
            return SPI2_HOST;
        if(!spiPeriphState[1])
            return SPI3_HOST;
    }
    void setBusySPI(spi_host_device_t dev)
    {
        if(!dev) return;
        spiPeriphState[dev-1] = dev;
    }
    void initSPIws2812(int pinS) {
        esp_err_t err;
        
		SPI_settings.host = getFreeSPI();              // make a function to auto select available host
        if(!SPI_settings.host){
            SPI_settings.host = (spi_host_device_t)0;
            return;
        } 
		SPI_settings.dma_chan = SPI_DMA_CH_AUTO;

        SPI_settings.buscfg.mosi_io_num = pinS;
        SPI_settings.buscfg.miso_io_num = -1;      // not used
        SPI_settings.buscfg.sclk_io_num = -1;      // not used 
        SPI_settings.buscfg.quadwp_io_num = -1;    // not used
        SPI_settings.buscfg.quadhd_io_num = -1;    // not used
        SPI_settings.buscfg.data4_io_num = -1;     ///< GPIO pin for spi data4 signal in octal mode, or -1 if not used.
        SPI_settings.buscfg.data5_io_num = -1;     ///< GPIO pin for spi data5 signal in octal mode, or -1 if not used.
        SPI_settings.buscfg.data6_io_num = -1;     ///< GPIO pin for spi data6 signal in octal mode, or -1 if not used.
        SPI_settings.buscfg.data7_io_num = -1;     ///< GPIO pin for spi data7 signal in octal mode, or -1 if not used.
        SPI_settings.buscfg.max_transfer_sz = LED_DMA_BUFFER_SIZE;
        SPI_settings.buscfg.flags = 0;             ///< Abilities of bus to be checked by the driver. Or-ed value of ``SPICOMMON_BUSFLAG_*`` flags.
        SPI_settings.buscfg.intr_flags = 0; 

        SPI_settings.devcfg .command_bits = 0;
        SPI_settings.devcfg .address_bits = 0;
        SPI_settings.devcfg .dummy_bits = 0;
        SPI_settings.devcfg .mode = 0;                           //SPI mode 0
        SPI_settings.devcfg .clock_speed_hz = SPI_STRIP_SPEED; //3.2 * 1000 * 1000, //Clock out at 3.2 MHz
        SPI_settings.devcfg .input_delay_ns = -1;
        SPI_settings.devcfg .spics_io_num = -1; // CS pin
        SPI_settings.devcfg .queue_size = 1;    // Not sure if needed

        err = spi_bus_initialize(SPI_settings.host, &SPI_settings.buscfg, SPI_settings.dma_chan);
        if(err != ESP_OK)
            return;
        // ESP_ERROR_CHECK(err);
        err = spi_bus_add_device(SPI_settings.host, &SPI_settings.devcfg, &SPI_settings.spi);
        if(err != ESP_OK)
        {
            spi_bus_free(SPI_settings.host);
            return;
        }

        // ESP_ERROR_CHECK(err);
        // alloc memory for 
        ledstripDMAbuffer = (uint16_t*)heap_caps_malloc(LED_DMA_BUFFER_SIZE, MALLOC_CAP_DMA); // Critical to be DMA memory.
        if(ledstripDMAbuffer)
            memset(ledstripDMAbuffer, 0, LED_DMA_BUFFER_SIZE);
        else {
            spi_bus_free(SPI_settings.host);
            return;
        }
        setBusySPI(SPI_settings.host);
        inited = true;
    }

    uint8_t pin_;
    uint8_t clock_pin_;
    int num_leds_;
    int frequency_;
    int reset_us_;
    int t1h_;
    int t0h_;
    uint16_t* ledstripDMAbuffer;
    STRIP_SPI_settings_t SPI_settings;
    spi_transaction_t trans_desc;
    uint8_t transQueued;
    bool inited;
    static uint8_t spiPeriphState[2];
};
template<Color8::Byteorder BYTEORDER>
uint8_t ESPILedPinBase<BYTEORDER>::spiPeriphState[2] = {0,0};

template<int LEDS, int PIN, Color8::Byteorder BYTEORDER, int frequency=800000, int reset_us=300, int t1h=294, int t0h=892>
class ESPILedPin : public ESPILedPinBase<BYTEORDER> {
public:  
  ESPILedPin() : ESPILedPinBase<BYTEORDER>(LEDS, PIN, frequency, reset_us, t1h, t0h) {}
};

#endif /* MAIN_SPI_WS2812_H_ */