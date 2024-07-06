#ifndef ESP_STATUSLEDS_H
#define ESP_STATUSLEDS_H

#include "looper.h"
#include "../blades/rmt_pin.h"

#define RGB_STATUSLED_PIXEL_NR 10   // one pixel is one WS RGB LED
#define RMT_STATUS_PIN 18           // PIN where we will sent the signal 

class LedStatusEffect
{
    public:
    virtual Color16 getColor() = 0;
    virtual bool active() = 0;
    virtual void setActive(bool ac) = 0;
    virtual void runEffect() = 0;
    virtual bool nUpdate() = 0;
};

class LedStatusBlink : public LedStatusEffect
{
    public:
    
    /*  @brief  : public constructor, set desired param and set working var to def 
    *   @param  : color - color we want to make the blink 
                  freqms - how many ms we want to stay on/off
                  repeat - how many blinks we wish to exec
    *   @retval : void 
    */  
    LedStatusBlink(Color16 color, uint32_t freqms, int16_t repeat)
    {
        active_ = false;
        sMillis = 0;
        color_ = color;
        sColor_ = color;
        freqms_ = freqms;
        nrTimes_ = repeat;
        state = false;
    }

    /*  @brief  : Return if blink is active     
    *   @param  : void 
    *   @retval : void 
    */  
    bool active() override
    {
        return active_;
    }

    /*  @brief  : Return if we need to update color (color changed )  
    *   @param  : void 
    *   @retval : void 
    */  
    bool nUpdate() override
    {
        return nUpdate_;
    }
    
    /*  @brief  : Activate/Desactivate blink eff
    *   @param  : ac - state we want to set on eff 
    *   @retval : void 
    */  
    void setActive(bool ac)
    {
        active_ = ac;
        if(ac) sMillis = millis();
        state = false;
    }

    /*  @brief  : Run blink effect and set color if changed     
    *   @param  : void 
    *   @retval : void 
    */  
    void runEffect() override
    {
        if(active_)
        {
            if(millis() - sMillis > freqms_)
            {
                sMillis = millis();
                state = !state;
                if(state) {
                    color_ = sColor_;
                } else {
                    color_ = Color16();
                    nrTimes_--;
                    if(!nrTimes_) active_ = false;
                }
                nUpdate_ = true;
            }
        }
    }

    /*  @brief  : Simple getter of color     
    *   @param  : void 
    *   @retval : void 
    */  
    Color16 getColor() override
    {
        return color_;
    }

    /*  @brief  : Changed the color the blink effect is exec    
    *   @param  : void 
    *   @retval : void 
    */  
    void changeColor(Color16 c)
    {
        sColor_ = c;
    }

    private:
    uint32_t sMillis;
    bool active_, state, nUpdate_;
    uint32_t freqms_; 
    int16_t nrTimes_;
    Color16 color_, sColor_;

    
};



// Status led class
class StatusLed
{   
    public:

    StatusLed *nextStatusLed;
    Color16 *colorBfr;
    uint8_t pixelNR, sPos;
    bool needUpdate;
    LedStatusEffect * seff;

    /*  @brief  : constructor of status led, that received how many leds we alloc and start index 
    *   @param  : pixelAlloc - how many pixel we alloc for the status led 
                  sIndex - start index in array
    *   @retval : void 
    */   
    StatusLed(uint8_t pixelAlloc, uint8_t sIndex) 
    {   // initialize buffer color
        pixelNR = 0;
        nextStatusLed = NULL;
        colorBfr = NULL;
        seff = NULL;
        if(pixelAlloc + sIndex > RGB_STATUSLED_PIXEL_NR) return;
        sPos = sIndex;
        needUpdate = false;
        colorBfr = (Color16 *)malloc(pixelAlloc * sizeof(Color16));
        if(colorBfr) 
            pixelNR = pixelAlloc;

    }
    
    /*  @brief  : Destructor , free alloc memeory and set def
    *   @param  : void 
    *   @retval : void 
    */
    ~StatusLed() 
    {
        pixelNR = 0;
        if(colorBfr)
            free(colorBfr);
    }

    /*  @brief  : set color of pixel
    *   @param  : c - color 
                  indx - index of pixel  
    *   @retval : true - success
                  false - failed 
    */   
    bool setPixelColor(Color16 c, uint8_t indx)
    {
        if(!colorBfr) return false;
        if(!pixelNR) return false;
        if(indx >= pixelNR) return false;

        (colorBfr+indx)->r = c.r;
        (colorBfr+indx)->g = c.g;
        (colorBfr+indx)->b = c.b;
        needUpdate = true;
        return true;
    }

    /*  @brief  : Fill Color 
    *   @param  : c - color 
                  nrLeds - nr of leds we want to apply color  
    *   @retval : true - success
                  false - failed 
    */  
    bool fillColor(Color16 c, uint8_t nrLeds)
    {
        if(nrLeds > pixelNR) nrLeds = pixelNR;

        for(uint8_t i=0; i < nrLeds; i++)
        {
            setPixelColor(c, i);
        }

        return true;
    }

    /*  @brief  : Set Color    
    *   @param  : c - color  
    *   @retval : void 
    */  
    void setColor(Color16 c)
    {
        fillColor(c, pixelNR);
    }

    /*  @brief  : Set desired effect  
    *   @param  : void 
    *   @retval : void 
    */  
    void setEffect(LedStatusEffect* eff)
    {   
        seff = eff;
    }

    /*  @brief  : Clear effect   
    *   @param  : void 
    *   @retval : void 
    */  
    void clearEffect()
    {
        if(seff)
            seff->setActive(false);
        seff = NULL;
    }

    /*  @brief  : Start or stop an effect     
    *   @param  : void 
    *   @retval : void 
    */  
    void startStopEffect(bool on)
    {
        if(seff)
            seff->setActive(on);
    }

    /*  @brief  : check if effect is active   
    *   @param  : void 
    *   @retval : bool  
    */  
    bool isEffectActive()
    {
        if(seff)
            return seff->active();
        
        return false;
    }

    /*  @brief  : run effect   
    *   @param  : void 
    *   @retval : void 
    */  
    void run()
    {
        if(seff){
            if(!seff->active()) return;
            seff->runEffect();
            if(seff->nUpdate()) setColor(seff->getColor());
        }
    }

};

// reserve a rmt channel for status led assuming that status leds are ws2812 leds 
// and sends encoded signal dor it 
class StatusLed_Manager : Looper
{
public:
    /*  @brief  : constructor that reserved a rmt channedl and pin for status leds 
    *   @param  : PIN on which we want to sent the signal 
    *   @retval : void 
    */
    StatusLed_Manager(int pin) : Looper(100)
    {
        statusLEDS = NULL;
        rmtStatus = new RMTLedPinBase<Color8::Byteorder::GRB>(RGB_STATUSLED_PIXEL_NR, pin, 80000, 1, 1, 1);
        rmtStatus->setColorBfr(bfrColor);
        rmtStatus->setType(RMTLedPinBase<Color8::Byteorder::GRB>::pinType::rmt_status);
    }
    /*  @brief  : Name in Looper
    *   @param  : void 
    *   @retval : string of const name  
    */
    const char* name() override {
        return "StatusLed_man";
    }

    /*  @brief  : Inital setup  
    *   @param  : void 
    *   @retval :   
    */
    void Setup() override {
    
    }

    /*  @brief  : Handles the signal sending on channel for status led  
    *   @param  : void 
    *   @retval : void 
    */
    void Loop() override  {
        // loop will be run every 100 us 
        if(rmtStatus->IsReadyForEndFrame()) {
            if(updateColor())
                rmtStatus->EndFrame();
        }
    }

    /*  @brief  : Update color    
    *   @param  : void 
    *   @retval : void 
    */  
    bool updateColor()
    {   
        bool update = false;
        for (StatusLed *l = statusLEDS; l; l = l->nextStatusLed) {
            l->run();
            if(l->needUpdate) {
                l->needUpdate = false;
                update = true;
                if(l->sPos + l->pixelNR <= RGB_STATUSLED_PIXEL_NR)
                {
                    for(uint8_t i = 0; i < l->pixelNR; i++)
                    {
                        (bfrColor + l->sPos + i)->r = ((l->colorBfr+i)->r);
                        (bfrColor + l->sPos + i)->g = ((l->colorBfr+i)->g);
                        (bfrColor + l->sPos + i)->b = ((l->colorBfr+i)->b);

                    }
                }
            }
        }
        return update;
    }

    /*  @brief  : Link status led     
    *   @param  : void 
    *   @retval : void 
    */  
    void linkLed(StatusLed *led)
    {
        led->nextStatusLed = statusLEDS;
        statusLEDS = led;
    }

    /*  @brief  : Unlink status led    
    *   @param  : void 
    *   @retval : void 
    */  
    void unlinkLed(StatusLed *led)
    {
        for (StatusLed** i = &statusLEDS; *i; i = &(*i)->nextStatusLed) {
            if (*i == led) {
                *i = led->nextStatusLed;
                return;
            }
        }
    }

    private:
    RMTLedPinBase<Color8::Byteorder::GRB> *rmtStatus;
    Color16 bfrColor[RGB_STATUSLED_PIXEL_NR];
    StatusLed *statusLEDS;

};


void esp_statusLedTest()
{
    StatusLed_Manager statusMANAGER = StatusLed_Manager(RMT_STATUS_PIN);
    Color16 color;
    StatusLed blueTooth(2, 0);
    StatusLed wifiTooth(1, 2);
    StatusLed chargerTooth(1,3);

    pinMode(GPIO_NUM_17, OUTPUT);
    digitalWrite(GPIO_NUM_17, 0);

    statusMANAGER.linkLed(&blueTooth);
    statusMANAGER.linkLed(&wifiTooth);
    statusMANAGER.linkLed(&chargerTooth);

    color.r = 65535;
    color.g = 0;
    color.b = 0;
    blueTooth.setColor(color);
    LedStatusBlink blinky(color, 500, 5);
   
    blueTooth.setEffect(&blinky);
    blueTooth.startStopEffect(true);

    color.r = 0;
    color.g = 65535;
    color.b = 0;
    LedStatusBlink blink2(color, 200, 15);
    wifiTooth.setColor(color);
    color.r = 0;
    color.g = 0;
    color.b = 65535;
    chargerTooth.setColor(color);

    while(1)
    {
        statusMANAGER.Loop();
        delay(100);
        if(!blueTooth.isEffectActive())
        {
            blueTooth.clearEffect();
            blueTooth.setEffect(&blink2);
            blueTooth.startStopEffect(true);
        }
    }
}

#endif