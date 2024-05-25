#ifndef ESP_STATUSLEDS_H
#define ESP_STATUSLEDS_H

#include "looper.h"
#include "../blades/rmt_pin.h"

#define RGB_STATUSLED_PIXEL_NR 4   // one pixel is one WS RGB LED
#define RMT_STATUS_PIN 18           // PIN where we will sent the signal 




// Status led class
class StatusLed
{   
protected:
    uint8_t pendingStatus;  // status to be set on next call to run()
public:
    bool transparent;       // first non-transparent LED linked will set the color (unless 0, 0, 0) - independently for each pixel
    StatusLed *nextStatusLed;
    uint8_t status;     // current status - 0 = inactive, 1... = enum
    Color16 *colorBfr;
    uint8_t pixelNR, sPos;
    /*  @brief  : constructor of status led, that received how many leds we alloc and start index 
    *   @param  : pixelAlloc - how many pixel we alloc for the status led 
                  sIndex - start index in array
    *   @retval : void 
    */   
    StatusLed(uint8_t pixelAlloc, uint8_t sIndex) 
    {   // initialize buffer color
        pixelNR = 0;    // remains 0 if alloc failed
        nextStatusLed = NULL;
        colorBfr = NULL;
        status = 0;           // inactive
        pendingStatus = 0;    // no pending status
        transparent = true; 
        if (sIndex >= RGB_STATUSLED_PIXEL_NR) sIndex = RGB_STATUSLED_PIXEL_NR - 1;  // max index        
        if(pixelAlloc + sIndex > RGB_STATUSLED_PIXEL_NR) pixelAlloc = RGB_STATUSLED_PIXEL_NR - sIndex; // max nr of pixel
        sPos = sIndex;
        colorBfr = (Color16 *)malloc(pixelAlloc * sizeof(Color16));
        if(colorBfr) {
            pixelNR = pixelAlloc;
            for(uint8_t i = 0; i < pixelNR; i++) {  // clear color buffer
                (colorBfr + i)->r = 0;  
                (colorBfr + i)->g = 0;
                (colorBfr + i)->b = 0;
            }
        }
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
        // needUpdate = true;
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


    bool isOff() {  // checks if there's any non-0 color on any LED
        if (!pixelNR) return true;     // no pixel assigned
        for(uint8_t i = 0; i < pixelNR; i++) {
            if ((colorBfr + i)->r || (colorBfr + i)->g || (colorBfr + i)->b) return false;
        }
        return true;
    }

    // Change status on next call to run()
    virtual void setStatus(uint8_t newStatus) {
        pendingStatus = newStatus;
    }

    // Run effect, returns true if needs update
    // Override this in derived classes to implement custom effects
    virtual bool run()   {
        return false;
    }


};

// RTS interpolators for dynamic effects on status LEDs
uint8_t statusledrts_Fade[] = { 0, 1, 5, 255, 255, 0 }; // keep max for 1 timescale, fade to 0 in 4 timescales
uint8_t statusledrts_Pulse50[] = { 0, 1, 2, 3, 5, 0, 255, 255, 0, 0 }; // pulse for 5 timescales, with 50% duty cycle
uint8_t statusledrts_Pulse[] = { 0, 2, 3, 10, 0, 255, 255, 0 }; // asymmetrical pulse for 10 timescales
uint8_t statusledrts_Flash[] = { 0, 1, 2, 0, 255, 0}; // simple flash 

// Status led class to signal communication
class StatusLed_Comm : public StatusLed
{

RTS<uint8_t, uint8_t> rts[2];  // RTS interpolators for dynamic effects

public:
    // Communication states
    enum StatusLed_commStates : uint8_t
    {
        COMMSTATE_OFF = 0,
        COMMSTATE_BROADCASTING,
        COMMSTATE_CONNECTED,
        COMMSTATE_DISCONNECTED,
        COMMSTATE_WIFISEARCH,
        COMMSTATE_WIFICONNECTED
    };

    StatusLed_Comm(uint8_t pixelNR, uint8_t pixelStart) : StatusLed(pixelNR, pixelStart) {  }

    bool run() override
    {   
        // 1. Change status if needed
        if (pendingStatus != status)  {
            switch (pendingStatus) {
                
                case COMMSTATE_OFF:
                    fillColor(Color16(0, 0, 0), pixelNR);   // all off
                    status = COMMSTATE_OFF;
                    return true;    // update

                case COMMSTATE_WIFISEARCH:
                    if (status != COMMSTATE_DISCONNECTED) {    // wait for the disconnected effect to end
                        rts[0].Free(); rts[0].Init(statusledrts_Pulse, sizeof(statusledrts_Pulse));                
                        rts[0].Start(200, 0, 0);       // run in loop 
                        status = COMMSTATE_WIFISEARCH;
                        return true;    // update                
                    }
                    break;
                
                case COMMSTATE_BROADCASTING:
                    if (status != COMMSTATE_DISCONNECTED) {    // wait for the disconnected effect to end
                        rts[0].Free(); rts[0].Init(statusledrts_Pulse, sizeof(statusledrts_Pulse));                
                        rts[0].Start(200, 0, 0);       // run in loop 
                        status = COMMSTATE_BROADCASTING;
                        return true;    // update                
                    }
                    break;

                case COMMSTATE_CONNECTED:
                    rts[0].Free(); rts[0].Init(statusledrts_Fade, sizeof(statusledrts_Fade));                
                    rts[0].Start(700, 1, 0);       // run once
                    status = COMMSTATE_CONNECTED;
                    return true;    // update

                case COMMSTATE_WIFICONNECTED:
                    rts[0].Free(); rts[0].Init(statusledrts_Fade, sizeof(statusledrts_Fade));                
                    rts[0].Start(700, 1, 0);       // run once
                    status = COMMSTATE_WIFICONNECTED;
                    return true;    // update                       

                case COMMSTATE_DISCONNECTED:
                    // Blue control
                    rts[0].Free(); rts[0].Init(statusledrts_Fade, sizeof(statusledrts_Fade));
                    rts[0].Start(80, 1, 0);       // run once with timescale 80 ms and no delay      
                     // Red control
                    rts[1].Free(); rts[1].Init(statusledrts_Pulse50, sizeof(statusledrts_Pulse50));
                    rts[1].Start(200, 1, 0);       // run once with timescale 200 ms and no delay      
                    status = COMMSTATE_DISCONNECTED;                              
                    break;
               
                default:    // invalid pending status
                    pendingStatus = status; // revert to current status

            }
        } 

        // 2. Run dynamic effect
        switch(status) {
            case COMMSTATE_BROADCASTING: 
            {   uint16_t colint = (uint16_t)rts[0].Get() << 8;
                setColor(Color16(0, 0, colint));    // apply RTS on blue
                return true;    // update
            }

            case COMMSTATE_WIFISEARCH:
           {    uint16_t colint = (uint16_t)rts[0].Get() << 8;
                setColor(Color16(colint, 0, colint));    // apply RTS on blue
                return true;    // update
            }

            case COMMSTATE_CONNECTED:
            {
                uint16_t green = (uint16_t)rts[0].Get() << 8;
                setPixelColor(Color16(0, green, 65535), 1);    // Cyan -> Blue on middle
                uint16_t blue = green;
                if (blue < 10000) blue = 10000;   
                setPixelColor(Color16(0, green, blue), 0);    // Cyan -> dim Blue on sides
                setPixelColor(Color16(0, green, blue), 2);
                return true;    // update                    
            }

            case COMMSTATE_WIFICONNECTED:
            {
                uint16_t color = (uint16_t)rts[0].Get() << 8;
                if (color < 10000) color = 10000;   
                setPixelColor(Color16(color, 0, color), 1); 
                setPixelColor(Color16(color, 0, color), 0);    // Cyan -> dim Blue on sides
                setPixelColor(Color16(color, 0, color), 2);
                return true;    // update                    
            }

            case COMMSTATE_DISCONNECTED:
                if (rts[0].state == interpolator_running || rts[1].state == interpolator_running)   {               
                    uint16_t blue = (uint16_t)rts[0].Get() << 8;
                    uint16_t red = (uint16_t)rts[1].Get() << 8;
                    setColor(Color16(red, 0, blue));  
                    return true;    // update
                }
                else {
                    status = COMMSTATE_OFF; // if not pending broadcasting, revert to OFF
                    if (pendingStatus != COMMSTATE_BROADCASTING) pendingStatus = status; 
                    setColor(Color16(0, 0, 0));   
                    return true;
                }

            default:
                break;
  
        }
        return false;
    }
    

}StatusLed_comm(RGB_STATUSLED_PIXEL_NR, 0);  // PIXEL_NR leds starting from 1

// Status led class to signal battery status
class StatusLed_Bat : public StatusLed
{
    RTS<uint8_t, uint8_t> rts[2];   // RTS interpolators for dynamic effects
    TF<float, float> batScale;      // transfer function to set color based on battery level
    uint32_t lastFlashTime;         // last time flashed, for repetitions on lowbat and charging
    uint8_t lowbat_green;           // determined by battery charging state, sets flashing color from orange to red

public:
    // Battery states
    enum StatusLed_batStates : uint8_t
    {
        BATSTATE_OFF = 0,
        BATSTATE_NOBAT,
        BATSTATE_LOWBAT,
        BATSTATE_CHARGING,
        BATSTATE_CHARGED,
    };

    StatusLed_Bat(uint8_t pixelNR, uint8_t pixelStart) : StatusLed(pixelNR, pixelStart) { 
        transparent = false; // overwrite color, don't mix it. Need to keep color pure, to signal battery level
        lowbat_green = 0;   // default flash color for LOWBAT is red (in case batPercent is never set)
     }

    // Change status on next call to run()
    void setStatus(uint8_t newStatus, int8_t batPercent=-1) {
        if (batPercent >= 0) {  // apply battery percent if specified; rough translation orange(42%) -> red(<10%)
            if (batPercent <= 10) lowbat_green = 0; // red
            else if (batPercent > 42 && newStatus ==BATSTATE_LOWBAT) {    // switch to OFF if battery is charged more than 42%
                pendingStatus = BATSTATE_OFF;
                STDOUT.print("batPercent: "); STDOUT.print(batPercent); STDOUT.println(" > 42, switching to OFF");
                return;
            }
            else lowbat_green = (batPercent-10) << 2;    
            STDOUT.print("batPercent: "); STDOUT.print(batPercent); STDOUT.print(" lowbat_green: "); STDOUT.println(lowbat_green);
        }
        pendingStatus = newStatus; // change status otherwise
    }

    bool run() override
    {   
        // 1. Change status if needed
        if (pendingStatus != status)  {
            switch (pendingStatus) {
                
                case BATSTATE_OFF:
                    fillColor(Color16(0, 0, 0), pixelNR);   // all off
                    status = BATSTATE_OFF;
                    return true;    // update

                case BATSTATE_NOBAT:
                    rts[0].Free(); rts[0].Init(statusledrts_Pulse50, sizeof(statusledrts_Pulse50));
                    rts[0].Start(40, 3, 0);     // fast pulse, run 3 times
                    status = BATSTATE_NOBAT;
                    return true;    // update

                case BATSTATE_LOWBAT:
                    rts[0].Free(); rts[0].Init(statusledrts_Flash, sizeof(statusledrts_Flash));
                    rts[0].Start(40, 1, 0);     // simple flash, run once
                    rts[1].Free(); rts[1].Init(statusledrts_Flash, sizeof(statusledrts_Flash));
                    rts[1].Start(40, 1, 50);    // same flash with a small delay
                    status = BATSTATE_LOWBAT;
                    break;

                case BATSTATE_CHARGING:
                    rts[0].Free(); rts[0].Init(statusledrts_Flash, sizeof(statusledrts_Flash));
                    rts[0].Start(2000, 0, 0);     // slow pulse, run in loop
                    status = BATSTATE_CHARGING;
                    return true;    // update
                
                case BATSTATE_CHARGED:
                    rts[0].Free(); rts[0].Init(statusledrts_Fade, sizeof(statusledrts_Fade));
                    rts[0].Start(500, 1, 0);     
                    // rts[0].Free(); rts[0].Init(statusledrts_Pulse, sizeof(statusledrts_Pulse));
                    // rts[0].Start(100, 1, 0);    
                    setPixelColor(Color16(0, 65535, 0), 1);  // keep max green on middle pixel
                    status = BATSTATE_CHARGED;
                    return true;    // update

                default:    // invalid pending status
                    pendingStatus = status; // revert to current status
            }
        } 

        // 2. Run dynamic effect
        switch(status) {
            case BATSTATE_NOBAT:
                if( rts[0].state == interpolator_running) {
                    uint16_t colint = (uint16_t)rts[0].Get() << 8;
                    setColor(Color16(colint, 0, 0));    // flash red 3 times
                    return true;    // update
                }
                else {  // revert to off
                    status = BATSTATE_OFF; 
                    pendingStatus = BATSTATE_OFF; 
                    setColor(Color16(0, 0, 0));   
                    return true;                                        
                }

            case BATSTATE_LOWBAT:
                if (rts[0].state == interpolator_running || rts[1].state == interpolator_running) {
                    uint16_t colint = (uint16_t)rts[0].Get();
                    setPixelColor(Color16(colint<<8, colint*lowbat_green, 0), 0);    // flash on one side, full red + green based on battery level
                    colint = (uint16_t)rts[1].Get();
                    setPixelColor(Color16(colint<<8, colint*lowbat_green, 0), 2);    // ...then on second side with a small delay
                    lastFlashTime = millis();
                    return true;    // update
                }
                else {
                    // if (millis() - lastFlashTime > 4999) { // flash again after a long delay
                    if (millis() - lastFlashTime > 2999) { // flash again after a long delay
                        status = BATSTATE_OFF; 
                        pendingStatus = BATSTATE_LOWBAT;    // restart LOWBAT 
                        return false;
                    }
                }
                break;
            
            case BATSTATE_CHARGING:
                {   uint16_t colint = (uint16_t)rts[0].Get() << 8;
                    if (!colint) colint = 1; // avoid 0 because it will loose transparency
                    setPixelColor(Color16(0, colint, 0), 1);    // pulse green on middle pixel, the others will remain transparent
                    return true;    // update
                }

            case BATSTATE_CHARGED:
                if( rts[0].state == interpolator_running) {
                    uint16_t colint = (uint16_t)rts[0].Get() << 8;
                    setPixelColor(Color16(0, colint, 0), 0);    // fade on side pixels
                    setPixelColor(Color16(0, colint, 0), 2);  
                    return true;    // update
                }
                return false;    // fade ended, no need to update
            default:
                break;
        }
        return false;
    }
}StatusLed_bat(3, 0);  // 3 leds starting from 1

// reserve a rmt channel for status led assuming that status leds are ws2812 leds 
// and sends encoded signal dor it 
// class StatusLed_Manager : Looper
class StatusLed_Manager : Looper, CommandParser, PowerSubscriber
{
public:

    void PwrOn_Callback() override 
    {
        STDOUT.println(" status+ ");
        if(!rmtStatus)
        {
            rmtStatus = new RMTLedPinBase<Color8::Byteorder::GRB>(RGB_STATUSLED_PIXEL_NR, pin_, 80000, 1, 1, 1);
            if (!rmtStatus) return;     // failed to allocate RAM for RMT channel
            rmtStatus->setColorBfr(pixelsBuffer);
            rmtStatus->setType(RMTLedPinBase<Color8::Byteorder::GRB>::pinType::rmt_status);
        }

    }         
    void PwrOff_Callback() override 
    { 
        STDOUT.println(" status- ");
        if(rmtStatus)
        {
            delete rmtStatus;
            rmtStatus = NULL;
        }
    }
    /*  @brief  : constructor that reserved a rmt channedl and pin for status leds 
    *   @param  : PIN on which we want to sent the signal 
    *   @retval : void 
    */
    // StatusLed_Manager(int pin) : Looper(10000)
    StatusLed_Manager(int pin) : Looper(10000), CommandParser() , PowerSubscriber(pwr4_CPU)
    {
        statusLEDS = NULL;
        pin_ = pin;
        rmtStatus = new RMTLedPinBase<Color8::Byteorder::GRB>(RGB_STATUSLED_PIXEL_NR, pin, 80000, 1, 1, 1);
        if (!rmtStatus) return;     // failed to allocate RAM for RMT channel
        rmtStatus->setColorBfr(pixelsBuffer);
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
        linkLed(&StatusLed_bat);  // link status led to manager (1st beause of transparency)    
        linkLed(&StatusLed_comm);  // link status led to manager
    }

    // "status_comm 0-3"
    // "status_bat 0-4"
    bool Parse( const char *cmd, const char *arg) override {
        if (!strcmp(cmd, "status_comm")) {
           uint8_t newStatus = atoi(arg);
           if (newStatus > 3) {
                STDOUT.println("Invalid status, range is 0-3.");
           } 
           else {
                StatusLed_comm.setStatus(newStatus);
           }
           return true;    

        }

        if (!strcmp(cmd, "status_bat")) {
           uint16_t newStatus = atoi(arg);
           if (newStatus == 2) newStatus = 200;     // default to 0% battery level, if not specified
           if (newStatus > 4) {
                if (newStatus>=200 && newStatus<=299) {                    
                    StatusLed_bat.setStatus(2, newStatus-200);  // set battery level
                }
                else STDOUT.println("Invalid status, range is 0-4 & 200-299.");
           } 
           else {
                StatusLed_bat.setStatus(newStatus);                
           }
           return true;    

        }

        return false;
    }

    void Help() override {}


    /*  @brief  : Handles the signal sending on channel for status led  
    *   @param  : void 
    *   @retval : void 
    */
    void Loop() override  {
        // loop will be run every 10 ms
        if(!rmtStatus) return;
        if(rmtStatus->IsReadyForEndFrame()) {
            if(updateColor())
                rmtStatus->EndFrame();
        }
    }


    /*  @brief  : Update color    
    *   @param  : void 
    *   @retval : void 
    */  
    bool updateColor() {   
        if (!statusLEDS) return false;  // no leds
        bool needsUpdate = false;
        for (StatusLed *l = statusLEDS; l; l = l->nextStatusLed) {
            if (l->run()) needsUpdate = true;   // run effect and check if needs update. New colors are stored in l->colorBfr                
        }

        if (needsUpdate) { // if any status LED needs update, mix colors to pixelsBuffer
            
            // 1. Clear all colors
            for(uint8_t i = 0; i <RGB_STATUSLED_PIXEL_NR; i++) {
                (pixelsBuffer + i)->r = 0;
                (pixelsBuffer + i)->g = 0;
                (pixelsBuffer + i)->b = 0;
            }
                        
            // 2. Mix or overwrite
            Color16* pixelColor;   
            Color16* statusLED_color;
            bool overwritten;
            for (StatusLed *l = statusLEDS; l; l = l->nextStatusLed) { 
                overwritten = false;
                for(uint8_t i = 0; i < l->pixelNR; i++) {
                    pixelColor = pixelsBuffer + l->sPos + i;   
                    statusLED_color = l->colorBfr+i;                    
                    if (!l->transparent && (statusLED_color->r || statusLED_color->g || statusLED_color->b)) { // overwrite
                        overwritten = true;     // won't check other LEDs once overwritten
                        pixelColor->r = statusLED_color->r;
                        pixelColor->g = statusLED_color->g;
                        pixelColor->b = statusLED_color->b;
                    }
                    else {  // mix colors
                        if (pixelColor->r < statusLED_color->r) pixelColor->r = statusLED_color->r;
                        if (pixelColor->g < statusLED_color->g) pixelColor->g = statusLED_color->g;
                        if (pixelColor->b < statusLED_color->b) pixelColor->b = statusLED_color->b;                    
                    }
                }
                if (overwritten) break; // no need to check other LEDs
            }

        }                
        return needsUpdate;
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
    RMTLedPinBase<Color8::Byteorder::GRB> *rmtStatus = NULL;
    Color16 pixelsBuffer[RGB_STATUSLED_PIXEL_NR];
    StatusLed *statusLEDS;
    int pin_;

} StatusLed_manager(RMT_STATUS_PIN);



#endif