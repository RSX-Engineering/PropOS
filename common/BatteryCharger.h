#ifndef COMMON_CHARGESTATUS_LED_H
#define COMMON_CHARGESTATUS_LED_H

    #if defined(ULTRAPROFFIE) && defined(ARDUINO_ARCH_STM32L4) && defined(ULTRAPROFFIE_CHARGER)

    #include "looper.h"

    class BatteryCharger : public Looper, public CommandParser, public PowerSubscriber {
    public:
    void PwrOn_Callback() override { 
    #ifdef DIAGNOSE_POWER
      STDOUT.println(" cpu+ "); 
    #endif
    }
    void PwrOff_Callback() override { 
    #ifdef DIAGNOSE_POWER
      STDOUT.println(" cpu- "); 
    #endif
    }
    bool HoldPower() override {  // Return true to pause power subscriber timeout
        if (charging) return true;
        return false;
    }

    const char* name() override { return "Battery Charger"; }

    /* @brief : Constructor, initializing data members to default values 
    *  @param : void
    *  @retval: void
    */
    BatteryCharger() : PowerSubscriber(pwr4_CPU){
        ignited = false;
        charging = false;
        lastChargeState = false;
        mAmpLimit = 0;    // charger disabled by default
    }
    /*  @brief  :
    *   @param  : 
    *   @retval : 
    */
    void Setup() {
        pinMode(chargeDetectPin, INPUT);     // INPUT_PULLUP
        pinMode(chargeCurrentPin, OUTPUT);     // OUTPUT PUSH/PULL        
        stm32l4_gpio_pin_configure(g_APinDescription[chargeEnablePin].pin, (GPIO_PUPD_PULLDOWN | GPIO_OSPEED_MEDIUM | GPIO_OTYPE_PUSHPULL | GPIO_MODE_OUTPUT));
        digitalWrite(chargeEnablePin, 0);      // defaults to disabled, until install
        digitalWrite(chargeCurrentPin, 0);  // defaults to 0.85A, untill install
        // STDOUT.println("Battery Charger");
    }
    /*  @brief  :
    *   @param  : 
    *   @retval : 
    */
    void Loop() override {
        uint32_t now_millis = millis();
        if (now_millis - last_millis_ < 5) return;
        last_millis_ = now_millis;

        // 1. Turn charger on/off as saber goes on/off
        // TODO: keep charger off while the power off effect is still running
        ignited = SaberBase::IsOn();
        if (mAmpLimit) {
            if (ignited) digitalWrite(chargeEnablePin, 0);
            else digitalWrite(chargeEnablePin, 1);
        }
        
        // 2. Report change of state
        charging = !digitalRead(chargeDetectPin);
        if (lastChargeState != charging)
        {
            lastChargeState = charging;
            #if defined(DIAGNOSE_POWER)
            if(lastChargeState)
                STDOUT.println("==Charging==");
            else
                STDOUT.println("==NOT Charging==");
            #endif
        }
       
     
    }

    /*  @brief  :
    *   @param  : 
    *   @retval : 
    */
    bool SetChargeLimit(uint16_t mAmpLimit_)
    {
        switch (mAmpLimit_) {
            case 0:     // disable 
                digitalWrite(chargeEnablePin, 0);      // disable
                this->mAmpLimit = 0;
                return true;
            case 850:   // enable at 0.85A
                digitalWrite(chargeCurrentPin, 0);  // 0.85A
                delay(1);       // wait 1[ms] to settle
                digitalWrite(chargeEnablePin, 1);      // enable
                this->mAmpLimit = 850;
                return true;
            case 1000:  // enable at 1.0A
                digitalWrite(chargeCurrentPin, 1);  // 1.0A
                delay(1);       // wait 1[ms] to settle
                digitalWrite(chargeEnablePin, 1);      // enable
                this->mAmpLimit = 1000;
            return true;
            default: return false;
        }

    }

        /*  @brief  :
    *   @param  : 
    *   @retval : 
    */
    uint16_t GetChargeLimit()
    {
        return this->mAmpLimit;
    }
    /*  @brief  :
    *   @param  : 
    *   @retval : 
    */
    virtual bool Parse(const char* cmd, const char* arg) {
        if (!strcmp(cmd, "charger")) {
            // 1. Report current state
            STDOUT.print("Charger currently set at "); STDOUT.print(mAmpLimit); STDOUT.print(" milliAmps.   ");
            if(charging) 
                STDOUT.println("==Charging==");
            else
                STDOUT.println("==NOT Charging==");

            // 2. Change state, if requested
            if (!strcmp(arg, "0")) { SetChargeLimit(0); STDOUT.println("Charger is now disabled. "); }
            if (!strcmp(arg, "850")) { SetChargeLimit(850); STDOUT.println("Charger is now set at 850 milliAmps. "); }
            if (!strcmp(arg, "1000")) { SetChargeLimit(1000); STDOUT.println("Charger is now set at 1000 milliAmps. "); }

            return true;
        }
        return false;
    }

    /*  @brief  :
    *   @param  : 
    *   @retval : 
    */
    virtual void Help() {
        #if defined(COMMANDS_HELP) 
        STDOUT.println(" charger [0/850/1000] Report charger state. Optionally set charger maximum current, in milliAmps");
        #endif
    }
    

    private:
    bool ignited, charging, lastChargeState;
    uint32_t last_millis_;
    uint16_t mAmpLimit;
    };

    BatteryCharger charger;

    uint16_t xChargerGetLimit()
    {
        return charger.GetChargeLimit();
    }
    #endif 

#endif
