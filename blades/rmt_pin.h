#ifndef _RMT_PIN_H
#define _RMT_PIN_H


#include "Arduino.h"
#include "freertos/task.h"
#include <string.h>
#include <driver/rmt.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stddef.h>

// #define RMT_WITH_TASK

#ifdef RMT_WITH_TASK

#define LED_STRIP_TASK_SIZE             (2048*6)
#define LED_STRIP_TASK_PRIORITY         (configMAX_PRIORITIES - 2)  // (configMAX_PRIORITIES - 1)
#define LED_STRIP_REFRESH_PERIOD_MS     (10) // TODO: add as parameter to led_strip_init
#define LED_STRIP_NUM_RMT_ITEMS_PER_LED (24U) // Assumes 24 bit color for each led
// RMT Clock source is @ 80 MHz. Dividing it by 8 gives us 10 MHz frequency, or 100ns period.
#define LED_STRIP_RMT_CLK_DIV  8// (8)
/****************************
        WS2812 Timing
 ****************************/
#define LED_STRIP_RMT_TICKS_BIT_1_HIGH_WS2812 9 // 900ns (900ns +/- 150ns per datasheet)
#define LED_STRIP_RMT_TICKS_BIT_1_LOW_WS2812  3 // 300ns (350ns +/- 150ns per datasheet)
#define LED_STRIP_RMT_TICKS_BIT_0_HIGH_WS2812 3 // 300ns (350ns +/- 150ns per datasheet)
#define LED_STRIP_RMT_TICKS_BIT_0_LOW_WS2812  9 // 900ns (900ns +/- 150ns per datasheet)

#define LED_STRIP_LENGTH 144U
#define INSTALLED_BRIGHTNESS (uint32_t)(256 * 3*65535 * 0.5)

Color16* volatile color_buffer_ptr = color_buffer;


/**
 * rgb led type 
 */
enum rgb_led_type_t {
    RGB_LED_TYPE_WS2812 = 0,
    // TODO ADD LATER IF LEDS CHANGES
    RGB_LED_TYPE_MAX,
};

/**
 * RGB LED colors
 */
struct led_color_t {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct led_strip_t {
    enum rgb_led_type_t rgb_led_type;       // blade type     
    uint32_t led_strip_length;              // strip pixel led
    rmt_channel_t rmt_channel;              // RMT peripheral settings

    /*
     * Interrupt table is located in soc.h
     * As of 11/27/16, reccomended interrupts are:
     * 9, 12, 13, 17, 18, 19, 20, 21 or 23
     * Ensure that the same interrupt number isn't used twice
     * across all libraries
     */
    int rmt_interrupt_num;                  // rmt interrupt nr 
    gpio_num_t gpio;                        // Must be less than GPIO_NUM_33

    // Double buffering elements
    bool showing_buf_1;
    struct led_color_t *led_strip_buf_1;
    struct led_color_t *led_strip_buf_2; 

    SemaphoreHandle_t access_semaphore;
};


class RMTLedPinBase : public WS2811PIN {
public:
  RMTLedPinBase(int num_leds,Color8::Byteorder byteorder):  num_leds_(num_leds), byteorder_(byteorder)     // TODO add other channel and make safe mechanism 
  {
    led_strip_test_init(GPIO_NUM_11, RMT_CHANNEL_1, 18);
  }

  bool IsReadyForBeginFrame() { return true; }
  bool IsReadyForEndFrame() { return true; }
  
  Color16* BeginFrame() {
    //for (int i = Color8::num_bytes(byteorder_); i >= 0; i--) OutByte(0);
    // here should be the reset frame i belive 
    return color_buffer;
  }

    void EndFrame() __attribute__((optimize("Ofast")))
    {
        Color16* pos = color_buffer_ptr;
        if(num_leds_ >= LED_STRIP_LENGTH) num_leds_ = LED_STRIP_LENGTH;

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

            led_strip_set_pixel_rgb(&my_led_strip, index, color.r, color.g, color.b);
            pos++;
        }

        for (uint32_t index = num_leds_; index < LED_STRIP_LENGTH; index++) {
            led_strip_set_pixel_rgb(&my_led_strip, index, 0, 0, 0);
        }

        led_strip_show(&my_led_strip);
    }

    int num_leds() const override { return num_leds_; }
    Color8::Byteorder get_byteorder() const override { return byteorder_; }
    
    void Enable(bool on) override {

    }


 public:

    TaskHandle_t led_strip_task_handle;         // Task handle
    typedef void (*led_fill_rmt_items_fn)(struct led_color_t *led_strip_buf, rmt_item32_t *rmt_items, uint32_t led_strip_length); // Function pointer for generating waveforms based on different LED drivers
    
    struct led_color_t led_strip_buf_1[LED_STRIP_LENGTH];
    struct led_color_t led_strip_buf_2[LED_STRIP_LENGTH];
    struct led_strip_t my_led_strip;    

    /*  @brief  : 
    *   @param  :
    *   @retval :
    */
    static inline void led_strip_fill_item_level(rmt_item32_t* item, int high_ticks, int low_ticks)
    {
        item->level0 = 1;
        item->duration0 = high_ticks;
        item->level1 = 0;
        item->duration1 = low_ticks;
    }

    /*  @brief  :
    *   @param  :
    *   @retval :
    */
    static inline void led_strip_rmt_bit_1_ws2812(rmt_item32_t* item)
    {
        led_strip_fill_item_level(item, LED_STRIP_RMT_TICKS_BIT_1_HIGH_WS2812, LED_STRIP_RMT_TICKS_BIT_1_LOW_WS2812);
    }

    /*  @brief  :
    *   @param  :
    *   @retval :
    */
    static inline void led_strip_rmt_bit_0_ws2812(rmt_item32_t* item)
    {
        led_strip_fill_item_level(item, LED_STRIP_RMT_TICKS_BIT_0_HIGH_WS2812, LED_STRIP_RMT_TICKS_BIT_0_LOW_WS2812);
    }

    /*  @brief  :
    *   @param  :
    *   @retval :
    */
    static void led_strip_fill_rmt_items_ws2812(struct led_color_t *led_strip_buf, rmt_item32_t *rmt_items, uint32_t led_strip_length)
    {
        uint32_t rmt_items_index = 0;

        for (uint32_t led_index = 0; led_index < led_strip_length; led_index++) {
            struct led_color_t led_color = led_strip_buf[led_index];

            for (uint8_t bit = 8; bit != 0; bit--) {
                uint8_t bit_set = (led_color.green >> (bit - 1)) & 1;
                if(bit_set) {
                    led_strip_rmt_bit_1_ws2812(&(rmt_items[rmt_items_index]));
                } else {
                    led_strip_rmt_bit_0_ws2812(&(rmt_items[rmt_items_index]));
                }
                rmt_items_index++;
            }
            for (uint8_t bit = 8; bit != 0; bit--) {
                uint8_t bit_set = (led_color.red >> (bit - 1)) & 1;
                if(bit_set) {
                    led_strip_rmt_bit_1_ws2812(&(rmt_items[rmt_items_index]));
                } else {
                    led_strip_rmt_bit_0_ws2812(&(rmt_items[rmt_items_index]));
                }
                rmt_items_index++;
            }
            for (uint8_t bit = 8; bit != 0; bit--) {
                uint8_t bit_set = (led_color.blue >> (bit - 1)) & 1;
                if(bit_set) {
                    led_strip_rmt_bit_1_ws2812(&(rmt_items[rmt_items_index]));
                } else {
                    led_strip_rmt_bit_0_ws2812(&(rmt_items[rmt_items_index]));
                }
                rmt_items_index++;
            }
        }
    }

    /*  @brief  :
    *   @param  :
    *   @retval :
    */
    static void led_strip_task(void *arg)
    {
        struct led_strip_t *led_strip = (struct led_strip_t *)arg;
        led_fill_rmt_items_fn led_make_waveform = NULL;
        bool make_new_rmt_items = true;
        bool prev_showing_buf_1 = !led_strip->showing_buf_1;

        size_t num_items_malloc = (LED_STRIP_NUM_RMT_ITEMS_PER_LED * led_strip->led_strip_length);//  + 1;  // add one item for reset 
        rmt_item32_t *rmt_items = (rmt_item32_t*) malloc(sizeof(rmt_item32_t) * num_items_malloc);
        if (!rmt_items) {
            vTaskDelete(NULL);
        }

        // switch (led_strip->rgb_led_type) {
        //     case RGB_LED_TYPE_WS2812:
        //         led_make_waveform = led_strip_fill_rmt_items_ws2812; //(led_strip, rmt_items, led_strip->led_strip_length);
        //         break;

        //     default:
        //         // Will avoid keeping it point to NULL
        //         led_make_waveform = led_strip_fill_rmt_items_ws2812;
        //         break;
        // };

        for(;;) {
            rmt_wait_tx_done(led_strip->rmt_channel,LED_STRIP_REFRESH_PERIOD_MS / portTICK_PERIOD_MS);
            xSemaphoreTake(led_strip->access_semaphore, portMAX_DELAY);

            /*
            * If buf 1 was previously being shown and now buf 2 is being shown,
            * it should update the new rmt items array. If buf 2 was previous being shown
            * and now buf 1 is being shown, it should update the new rmt items array.
            * Otherwise, no need to update the array
            */
            if ((prev_showing_buf_1 == true) && (led_strip->showing_buf_1 == false)) {
                make_new_rmt_items = true;
            } else if ((prev_showing_buf_1 == false) && (led_strip->showing_buf_1 == true)) {
                make_new_rmt_items = true;
            } else {
                make_new_rmt_items = false;
            }

            if (make_new_rmt_items) {
                if (led_strip->showing_buf_1) {
                    led_strip_fill_rmt_items_ws2812(led_strip->led_strip_buf_1, rmt_items, led_strip->led_strip_length);
                } else {
                    led_strip_fill_rmt_items_ws2812(led_strip->led_strip_buf_2, rmt_items, led_strip->led_strip_length);
                }
            }

            rmt_write_items(led_strip->rmt_channel, rmt_items, num_items_malloc, false);
            prev_showing_buf_1 = led_strip->showing_buf_1;
            xSemaphoreGive(led_strip->access_semaphore);
            vTaskDelay(1 / portTICK_PERIOD_MS);    // LED_STRIP_REFRESH_PERIOD_MS
        }

        if (rmt_items) {
            free(rmt_items);
        }
        vTaskDelete(NULL);
    }

    /*  @brief  :
    *   @param  :
    *   @retval :
    */
    bool led_strip_init_rmt(struct led_strip_t *led_strip)
    {
        rmt_config_t rmt_cfg ; 
        rmt_cfg.rmt_mode = RMT_MODE_TX;
        rmt_cfg.channel = led_strip->rmt_channel;
        rmt_cfg.clk_div = LED_STRIP_RMT_CLK_DIV;
        rmt_cfg.gpio_num = led_strip->gpio;
        rmt_cfg.mem_block_num = 1;
        rmt_cfg.flags = 0;
        rmt_cfg.tx_config .loop_en = false;
        rmt_cfg.tx_config.carrier_freq_hz = 10;            // Not used, but has to be set to avoid divide by 0 err
        rmt_cfg.tx_config.carrier_duty_percent = 50;
        rmt_cfg.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
        rmt_cfg.tx_config.carrier_en = false;
        rmt_cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW; //RMT_IDLE_LEVEL_LOW;
        rmt_cfg.tx_config.idle_output_en = true;

        esp_err_t cfg_ok = rmt_config(&rmt_cfg);
        if (cfg_ok != ESP_OK) {
            return false;
        }
        esp_err_t install_ok = rmt_driver_install(rmt_cfg.channel, 0, ESP_INTR_FLAG_IRAM);
        if (install_ok != ESP_OK) {
            return false;
        }

        return true;
    }

    /*  @brief  :
    *   @param  :
    *   @retval :
    */
    bool led_strip_init(struct led_strip_t *led_strip, rmt_channel_t channelRMT)
    {
        
        if ((led_strip == NULL) ||
            (led_strip->rmt_channel == RMT_CHANNEL_MAX) ||
            (led_strip->gpio > GPIO_NUM_33) ||  // only inputs above 33
            (led_strip->led_strip_buf_1 == NULL) ||
            (led_strip->led_strip_buf_2 == NULL) ||
            (led_strip->led_strip_length == 0) ||
            (led_strip->access_semaphore == NULL)) {
            return false;
        }

        if(led_strip->led_strip_buf_1 == led_strip->led_strip_buf_2) {
            return false;
        }

        memset(led_strip->led_strip_buf_1, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);
        memset(led_strip->led_strip_buf_2, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);

        rmt_set_source_clk(channelRMT, RMT_BASECLK_APB);

        bool init_rmt = led_strip_init_rmt(led_strip);
        if (!init_rmt) {
            return false;
        }

        xSemaphoreGive(led_strip->access_semaphore);
        BaseType_t task_created = xTaskCreatePinnedToCore(led_strip_task,
                                                "led_strip_task",
                                                LED_STRIP_TASK_SIZE,
                                                led_strip,
                                                LED_STRIP_TASK_PRIORITY,
                                                &led_strip_task_handle,
                                                ARDUINO_RUNNING_CORE // ARDUINO_RUNNING_CORE
                                            );

        if (!task_created) {
            return false;
        }

        return true;
    }

    /*  @brief  :
    *   @param  :
    *   @retval :
    */
    bool led_strip_set_pixel_color(struct led_strip_t *led_strip, uint32_t pixel_num, struct led_color_t *color)
    {
        bool set_led_success = true;

        if ((!led_strip) || (!color) || (pixel_num > led_strip->led_strip_length)) {
            return false;
        }

        if (led_strip->showing_buf_1) {
            led_strip->led_strip_buf_2[pixel_num] = *color;
        } else {
            led_strip->led_strip_buf_1[pixel_num] = *color;
        }

        return set_led_success;
    }

    /*  @brief  :
    *   @param  :
    *   @retval :
    */
    bool led_strip_set_pixel_rgb(struct led_strip_t *led_strip, uint32_t pixel_num, uint8_t red, uint8_t green, uint8_t blue) 
    {
        bool set_led_success = true;

        if ((!led_strip) || (pixel_num > led_strip->led_strip_length)) {
            return false;
        }

        if (led_strip->showing_buf_1) {
            led_strip->led_strip_buf_2[pixel_num].red = red;
            led_strip->led_strip_buf_2[pixel_num].green = green;
            led_strip->led_strip_buf_2[pixel_num].blue = blue;
        } else {
            led_strip->led_strip_buf_1[pixel_num].red = red;
            led_strip->led_strip_buf_1[pixel_num].green = green;
            led_strip->led_strip_buf_1[pixel_num].blue = blue;
        }

        return set_led_success;
    }

    /*  @brief  :
    *   @param  :
    *   @retval :
    */
    bool led_strip_get_pixel_color(struct led_strip_t *led_strip, uint32_t pixel_num, struct led_color_t *color)
    {
        bool get_success = true;

        if ((!led_strip) ||
            (pixel_num > led_strip->led_strip_length) ||
            (!color)) {
            color = NULL;
            return false;
        }

        if (led_strip->showing_buf_1) {
            *color = led_strip->led_strip_buf_1[pixel_num];
        } else {
            *color = led_strip->led_strip_buf_2[pixel_num];
        }

        return get_success;
    }

    /*  @brief  :  Updates the led buffer to be shown
    *   @param  :
    *   @retval :
    */
    bool led_strip_show(struct led_strip_t *led_strip)
    {
        bool success = true;

        if (!led_strip) {
            return false;
        }

        xSemaphoreTake(led_strip->access_semaphore, portMAX_DELAY);
        if (led_strip->showing_buf_1) {
            led_strip->showing_buf_1 = false;
            memset(led_strip->led_strip_buf_1, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);
        } else {
            led_strip->showing_buf_1 = true;
            memset(led_strip->led_strip_buf_2, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);
        }
        xSemaphoreGive(led_strip->access_semaphore);

        return success;
    }

    /*  @brief  : Clears the LED strip
    *   @param  :
    *   @retval :
    */
    bool led_strip_clear(struct led_strip_t *led_strip)
    {
        bool success = true;

        if (!led_strip) {
            return false;
        }

        if (led_strip->showing_buf_1) {
            memset(led_strip->led_strip_buf_2, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);
        } else {
            memset(led_strip->led_strip_buf_1, 0, sizeof(struct led_color_t) * led_strip->led_strip_length);
        }

        return success;
    }
    public:
    /*  @param  :   
    *   @param  :   
    *   @retval :
    */
    void led_strip_test_init(gpio_num_t gpioNUM, rmt_channel_t channelRMT, int intNR)
    {
        my_led_strip.rgb_led_type = RGB_LED_TYPE_WS2812;
        my_led_strip.rmt_channel = channelRMT;
        my_led_strip.rmt_interrupt_num = intNR;
        my_led_strip.gpio = gpioNUM;
        my_led_strip.led_strip_buf_1 = led_strip_buf_1;
        my_led_strip.led_strip_buf_2 = led_strip_buf_2;
        my_led_strip.led_strip_length = LED_STRIP_LENGTH;

        my_led_strip.access_semaphore = xSemaphoreCreateBinary();
        my_led_strip.showing_buf_1 = true;

        bool led_init_ok = led_strip_init(&my_led_strip, channelRMT);
        assert(led_init_ok);
        rmt_source_clk_t clockSource;
        uint32_t clk_hz;
        uint8_t clk_div;

        rmt_set_source_clk(channelRMT, RMT_BASECLK_APB);
        
        rmt_get_source_clk(channelRMT, &clockSource);
        rmt_get_counter_clock(channelRMT, &clk_hz);
        rmt_get_clk_div(channelRMT, &clk_div);
        Serial.print("Clock SOURCE is "); Serial.println(clockSource);
        Serial.print("Counter div  is "); Serial.println(clk_div);
        Serial.print("Counter clok  is "); Serial.println(clk_hz);

    }

    /*  @param  :   
    *   @param  :   
    *   @retval :
    */
    void led_strip_test_set(led_color_t led_color, uint8_t nr)
    {
        struct led_color_t zero_led_color = {
            .red = 0,
            .green = 0,
            .blue = 0,
        };
        Serial.print("R = "); Serial.println(led_color.red);
        Serial.print("G = "); Serial.println(led_color.green);
        Serial.print("B = "); Serial.println(led_color.blue);
        Serial.print("NR = "); Serial.println(nr);
        
        if(nr >= LED_STRIP_LENGTH) nr = LED_STRIP_LENGTH;

        for (uint32_t index = 0; index < nr; index++) {
            led_strip_set_pixel_color(&my_led_strip, index, &led_color);
        }

        for (uint32_t index = nr; index < LED_STRIP_LENGTH; index++) {
            led_strip_set_pixel_color(&my_led_strip, index, &zero_led_color);
        }
        led_strip_show(&my_led_strip);
    }



private:
  uint8_t pin_;
  uint8_t clock_pin_;
  int num_leds_;
  uint32_t frequency_;
  Color8::Byteorder byteorder_;

};

template<int LEDS, int PIN, Color8::Byteorder BYTEORDER, int frequency=800000, int reset_us=300, int t0h=294, int t1h=892>
class RMTLedPin : public RMTLedPinBase {
public:
  RMTLedPin() : RMTLedPinBase(LEDS, BYTEORDER) {}
};

#else

#include "driver/rmt.h"

#define WS2812_T0H_NS (400)
#define WS2812_T0L_NS (850)
#define WS2812_T1H_NS (800)
#define WS2812_T1L_NS (450)

#define INSTALLED_BRIGHTNESS (uint32_t)(256 * 3*65535 * 0.5)

#define RMT_LL_HW_BASE  (&RMT)

static uint32_t t0h_ticks;
static uint32_t t1h_ticks;
static uint32_t t0l_ticks;
static uint32_t t1l_ticks;

static bool rmt_reserved_channels[RMT_CHANNEL_MAX];
// Color16* volatile color_buffer_ptr = color_buffer;

static void IRAM_ATTR ws2812_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
        size_t wanted_num, size_t *translated_size, size_t *item_num)
{
    if (src == NULL || dest == NULL) {
        *translated_size = 0;
        *item_num = 0;
        return;
    }
    const rmt_item32_t bit0 = {{{ t0h_ticks, 1, t0l_ticks, 0 }}}; //Logical 0
    const rmt_item32_t bit1 = {{{ t1h_ticks, 1, t1l_ticks, 0 }}}; //Logical 1
    size_t size = 0;
    size_t num = 0;
    uint8_t *psrc = (uint8_t *)src;
    rmt_item32_t *pdest = dest;
    while (size < src_size && num < wanted_num) {
        for (int i = 0; i < 8; i++) {
            // MSB first
            if (*psrc & (1 << (7 - i))) {
                pdest->val =  bit1.val;
            } else {
                pdest->val =  bit0.val;
            }
            num++;
            pdest++;
        }
        size++;
        psrc++;
    }
    *translated_size = size;
    *item_num = num;
}

template<Color8::Byteorder BYTEORDER>
class RMTLedPinBase : public WS2811PIN
{
    public:

    typedef enum pinType
    {
        rmt_blade,
        rmt_status
    };

    RMTLedPinBase(int LEDS, int PIN, int frequency, int reset_us, int t1h, int t0h)
    {
        channel = RMT_CHANNEL_MAX;
        pin = (gpio_num_t)PIN;
        num_leds_ = LEDS;
        typeRMT = rmt_blade;
        color_buffer_ptr = color_buffer;
        init = initRMT();
        colors = (led_color_t*)malloc(sizeof(led_color_t) * LEDS);
        if(!colors)
            init = false;
    }

    ~RMTLedPinBase()
    {
        deinit();
        if(colors)
            free(colors);
    }

    void setColorBfr(Color16* bfr)
    {
        if(bfr)
            color_buffer_ptr = bfr;
    }
    void setType(pinType type)
    {
        typeRMT = type;
    }

    bool IsReadyForBeginFrame() override {
        return true;
    }
    Color16 *BeginFrame() override {
        return color_buffer_ptr;
    }
    bool IsReadyForEndFrame() override {
        if(init) {
            rmt_channel_status_result_t channel_status;
            rmt_get_channel_status(&channel_status);
            if(channel_status.status[channel] == RMT_CHANNEL_IDLE) return true;
        }
        return false;
    }

    void WaitUntilReadyForEndFrame() override {
        if(init)
            while (!IsReadyForEndFrame());
    }

    void EndFrame() override {
        if(init)
        {
            Color16 *pos = color_buffer_ptr;
            switch(typeRMT)
            {
                case rmt_blade:
                    for (uint32_t i = 0; i < num_leds_; i++) 
                    {
                        uint32_t energyScale = pos->r + pos->g + pos->b; // 3 * 16-bit color
                        uint32_t color32;
                        Color8 color;
                        if (energyScale <= INSTALLED_BRIGHTNESS/256) {
                            // just scale color16 to color8, no additional scaling
                            ((colors + i)->red) = *(((uint8_t*)&(pos->r))+1);         // Take 8 msb
                            ((colors + i)->green) = *(((uint8_t*)&(pos->g))+1);         // Take 8 msb
                            ((colors + i)->blue) = *(((uint8_t*)&(pos->b))+1);         // Take 8 msb
                        } else {
                            energyScale = INSTALLED_BRIGHTNESS / energyScale;     // 256 * scale
                            color32 = pos->r * energyScale; // 65536 * scale * color8
                            ((colors + i)->red) = *(((uint8_t*)&color32)+2);    // Take 8 msb
                            color32 = pos->g * energyScale; // 65536 * scale * color8
                            ((colors + i)->green) = *(((uint8_t*)&color32)+2);    // Take 8 msb
                            color32 = pos->b * energyScale; // 65536 * scale * color8
                            ((colors + i)->blue) = *(((uint8_t*)&color32)+2);    // Take 8 msb
                            // STDOUT.print("energyScale = "); STDOUT.println(energyScale);
                        }
                        pos++;
                    }
                break;

                case rmt_status:
                    for(uint16_t i =0; i< num_leds_; i++)
                    {
                        ((colors + i)->red) = *(((uint8_t*)&(pos->r))+1);         // Take 8 msb
                        ((colors + i)->green) = *(((uint8_t*)&(pos->g))+1);         // Take 8 msb
                        ((colors + i)->blue) = *(((uint8_t*)&(pos->b))+1);         // Take 8 msb
                        pos++;
                    }
                break;
            }
        }
            rmt_write_sample(config.channel, (uint8_t*)colors, (size_t)(num_leds_*3), false);
    }


    int num_leds() const override { return num_leds_; }
    Color8::Byteorder get_byteorder() const override { return BYTEORDER; }

    void Enable(bool enable) override {

    }

    bool isInit()
    {
        return init;
    }

    bool initRMT()
    {
        esp_err_t res;
        // Reserve channel
        for (size_t i = 0; i < RMT_CHANNEL_MAX; i++) {
            if (!rmt_reserved_channels[i]) {
                rmt_reserved_channels[i] = true;
                channel = (rmt_channel_t)i;
             break;
            }
        }

        if (channel == RMT_CHANNEL_MAX) {
            // Ran out of channels!
            return false;
        }

        config.rmt_mode = RMT_MODE_TX;                     
        config.channel = channel;                       
        config.gpio_num = pin;                            
        config.clk_div = 2;                               
        config.mem_block_num = 1;                          
        config.flags = 0;                                                             
        config.tx_config.carrier_freq_hz = 38000;                
        config.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH; 
        config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;        
        config.tx_config.carrier_duty_percent = 33;              
        config.tx_config.carrier_en = false;                     
        config.tx_config.loop_en = false;                        
        config.tx_config.idle_output_en = true;         

        if(rmt_config(&config) != ESP_OK) return false;
        if(rmt_driver_install(config.channel, 0, 0) != ESP_OK) return false;

        // Convert NS timings to ticks
        uint32_t counter_clk_hz = 0;

        rmt_get_counter_clock(channel, &counter_clk_hz);
        // NS to tick converter
        float ratio = (float)counter_clk_hz / 1e9;

        t0h_ticks = (uint32_t)(ratio * WS2812_T0H_NS);
        t0l_ticks = (uint32_t)(ratio * WS2812_T0L_NS);
        t1h_ticks = (uint32_t)(ratio * WS2812_T1H_NS);
        t1l_ticks = (uint32_t)(ratio * WS2812_T1L_NS);

        // Initialize automatic timing translator
         if(rmt_translator_init(config.channel, ws2812_rmt_adapter) != ESP_OK) return false;
         return true;
    }

    void deinit()
    {
            // Free channel again
        rmt_driver_uninstall(config.channel);
        rmt_reserved_channels[channel] = false;

        gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
        init = false;
    }

    private:
    rmt_config_t config;
    rmt_channel_t channel;
    gpio_num_t pin;
    bool init;
    int num_leds_;
    pinType typeRMT;
    typedef struct  {
        uint8_t green;
        uint8_t red;
        uint8_t blue;
    } led_color_t;
    led_color_t *colors;
    Color16 *color_buffer_ptr;
};

#endif

#endif