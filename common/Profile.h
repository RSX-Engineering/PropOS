/********************************************************************
 *  USER PROFILE MANAGEMENT                                         *
 *  (C) Marius RANGU @ RSX Engineering                              *
 *  fileversion: v1.0 @ 2022/10;    license: GPL3                   *
 ********************************************************************
 
 ********************************************************************/
 



#ifndef XPROFILE_H
#define XPROFILE_H


#define MIN_MASTER_VOLUME       1600    // minimum allowed master volume, below that is mute ~ 2.5%
#define MIN_MASTER_BRIGHTNESS   1600    // minimum allowed master brightness, below that is off: ~ 2.5%
#define STEALTH_MAX_VOLUME      21000   // ~32%
#define STEALTH_MAX_BRIGHTNESS  21000   // ~32%


// Data structure to read/write profile to COD files
struct profileData_t {
    uint16_t volume;
    uint16_t stealthVol;
    uint16_t brightness;
    uint16_t stealthBr;
    uint8_t masterSens;
    uint8_t swingSens;
    uint8_t clashSens;
    uint8_t stabSens;
    uint8_t shakeSens;
    uint8_t tapSens;
    uint8_t twistSens;
    uint8_t menuSens;
    uint16_t activePresets_id;
    uint8_t currentPreset_index;
} __attribute__((packed)); 




// If needed, adjusts regular / stealth volume & brightness to follow policy. Changes 'userProfile'!
void FixStealthMode() {
    // 1. Stealth is below limits
    if (userProfile.stealthVolume > STEALTH_MAX_VOLUME) userProfile.stealthVolume = STEALTH_MAX_VOLUME;
    if (userProfile.stealthBrightness > STEALTH_MAX_BRIGHTNESS) userProfile.stealthBrightness = STEALTH_MAX_BRIGHTNESS;
    // 2. Regular is above stealth
    if (userProfile.combatVolume < userProfile.stealthVolume) userProfile.combatVolume = userProfile.stealthVolume;
    if (userProfile.combatBrightness < userProfile.stealthBrightness) userProfile.combatBrightness = userProfile.stealthBrightness;
    // 3. Muted / off if too low
    if (userProfile.combatVolume <= MIN_MASTER_VOLUME) userProfile.combatVolume = 0;
    if (userProfile.stealthVolume <= MIN_MASTER_VOLUME) userProfile.stealthVolume = 0;
    if (userProfile.combatBrightness <= MIN_MASTER_BRIGHTNESS) userProfile.combatBrightness = 0;
    if (userProfile.stealthBrightness <= MIN_MASTER_BRIGHTNESS) userProfile.stealthBrightness = 0;
    // STDOUT.print("[FixStealthMode] Volume = "); STDOUT.print(userProfile.combatVolume); STDOUT.print(", stealth volume ="); STDOUT.println(userProfile.stealthVolume);          
    // STDOUT.print("[FixStealthMode] Brightness = "); STDOUT.print(userProfile.combatBrightness); STDOUT.print(", stealth brightness ="); STDOUT.println(userProfile.stealthBrightness);          
}

// Apply all motion sensitivities (including master)       
void ApplySensitivities(uint8_t masterSensitivity) {
    Sensitivity::master = masterSensitivity;
    userProfile.swingSensitivity.Set();
    smooth_swing_config.ApplySensitivity(&userProfile.swingSensitivity);  // scale parameters with sensitivity              
    userProfile.clashSensitivity.Set();
    userProfile.stabSensitivity.Set();
    userProfile.shakeSensitivity.Set();
    userProfile.tapSensitivity.Set();
    userProfile.twistSensitivity.Set();
    userProfile.menuSensitivity.Set();
}

// Read profile from COD file, return 0 if failed or ID of active presets table if success
// __attribute__((optimize("Og"))) 
uint16_t ReadProfile(const char* filename, uint16_t ID) {
    CodReader reader;
    profileData_t profileData; 
    // STDOUT.print("[xProfile.Read] Reading "); STDOUT.print(filename); STDOUT.print(", ID="); STDOUT.print(ID); STDOUT.print(": ");

    // 1. Read profile from COD file
    bool success = true;
    if (!reader.Open(filename)) { 
         #ifdef DIAGNOSE_BOOT
            STDOUT.print("FAILED TO SET PROFILE: failed to read profile data from ");
            STDOUT.print(filename); STDOUT.print(", ID = "); STDOUT.println(ID);
        #endif
        return false;               // File not found
    }
    #ifdef DIAGNOSE_BOOT
        STDOUT.print("* Reading user profile from "); STDOUT.print(filename); 
        STDOUT.print(", ID = "); STDOUT.print(ID); STDOUT.print(" ... ");
    #endif    
    if (reader.FindEntry(ID) != COD_ENTYPE_STRUCT) success = false;  // wrong entry type
    if (reader.codProperties.structure.Handler != HANDLER_Profile) success = false;   // wrong handler
    if (success) {
        uint32_t numBytes = reader.ReadEntry(ID, (void*)&profileData, sizeof(profileData));      // Attempt to read a structure with the requested ID. 
        if (numBytes != sizeof(profileData)) success = false;
        // if (success) {
        //     STDOUT.print(" Read "); STDOUT.print(numBytes); STDOUT.print(" bytes, expected "); STDOUT.print(sizeof(profileData)); STDOUT.print(". ");
        //     STDOUT.print("vol="); STDOUT.print(profileData.volume); STDOUT.print(", br="); STDOUT.print(profileData.brightness); STDOUT.print(", sens="); STDOUT.print(profileData.sensitivity);
        //     STDOUT.print(", presID="); STDOUT.print(profileData.activePresets_id); STDOUT.print(", curPres="); STDOUT.println(profileData.currentPreset_index);
        // }
    }
    reader.Close();    
    if (!success) { 
         #ifdef DIAGNOSE_BOOT
            STDOUT.println("Failed, could not read profile.");
        #endif
        return false;               // File not found
    }
    #ifdef DIAGNOSE_BOOT
        STDOUT.println();
    #endif   

    // 2. Set brightnes and volume
    userProfile.combatVolume = profileData.volume;                 // set volumes            
    userProfile.stealthVolume = profileData.stealthVol;
    userProfile.combatBrightness = profileData.brightness;         // set brightness
    userProfile.stealthBrightness = profileData.stealthBr;          
    FixStealthMode();       // adjust if needed
    userProfile.masterBrightness = userProfile.combatBrightness;   
    userProfile.masterVolume = userProfile.combatVolume;
    dynamic_mixer.set_volume(VOLUME);   // apply regular volume
    userProfile.swingSensitivity.userSetting = profileData.swingSens;
    userProfile.clashSensitivity.userSetting = profileData.clashSens;
    userProfile.stabSensitivity.userSetting = profileData.stabSens;
    userProfile.shakeSensitivity.userSetting = profileData.shakeSens;
    userProfile.tapSensitivity.userSetting = profileData.tapSens;
    userProfile.twistSensitivity.userSetting = profileData.twistSens;
    userProfile.menuSensitivity.userSetting = profileData.menuSens;
    ApplySensitivities(profileData.masterSens);           
    userProfile.preset = profileData.currentPreset_index;
    if (!userProfile.preset) userProfile.preset = 1;     // preset=0 means preset error, should not be specified like that in COD


    #ifdef DIAGNOSE_BOOT
        STDOUT.print(" ... Setting master volume to "); STDOUT.print(userProfile.masterVolume);
        STDOUT.print(" and stealth volume to "); STDOUT.println(userProfile.stealthVolume);
        STDOUT.print(" ... Setting master brightness to "); STDOUT.print(userProfile.masterBrightness);
        STDOUT.print(" and stealth brightness to "); STDOUT.println(userProfile.stealthBrightness);
        STDOUT.print(" ... Setting master sensitivity to "); STDOUT.println(Sensitivity::master); // STDOUT.println(userProfile.masterSensitivity);
        STDOUT.println(" ... Setting motion sensitivities: "); 
        STDOUT.print("     [swing]  "); STDOUT.println(userProfile.swingSensitivity.userSetting); 
        STDOUT.print("     [clash]  "); STDOUT.println(userProfile.clashSensitivity.userSetting); 
        STDOUT.print("     [stab]   "); STDOUT.println(userProfile.stabSensitivity.userSetting); 
        STDOUT.print("     [shake]  "); STDOUT.println(userProfile.shakeSensitivity.userSetting); 
        STDOUT.print("     [tap]    "); STDOUT.println(userProfile.tapSensitivity.userSetting); 
        STDOUT.print("     [twist]  "); STDOUT.println(userProfile.twistSensitivity.userSetting); 
        STDOUT.print("     [menu]   "); STDOUT.println(userProfile.menuSensitivity.userSetting); 
      if (!profileData.activePresets_id) {
        STDOUT.print("* Loading active presets table from "); STDOUT.print(PRESETS_FILE); 
        STDOUT.println(", ID = 0 ... FAILED, invalid ID"); 
      }
    #endif // DIAGNOSE_BOOT
    return profileData.activePresets_id;
}

// Write user profile and color variations to profile.cod
// __attribute__((optimize("Og")))
bool WriteUserProfile(const char* filename, uint16_t ID) {
    // STDOUT.print("NOT Writing profile to file "); STDOUT.print(filename); STDOUT.print(", ID="); STDOUT.println(ID);
    CodReader reader;
    profileData_t profileData;

    // 1. Prepare data structure for profile
    profileData.volume = userProfile.combatVolume;                     // get current volume & brightness
    profileData.brightness = userProfile.combatBrightness;
    profileData.stealthVol = userProfile.stealthVolume;
    profileData.stealthBr = userProfile.stealthBrightness;
    profileData.masterSens = Sensitivity::master;             // get current sensitivities
    profileData.swingSens = userProfile.swingSensitivity.userSetting;   
    profileData.clashSens = userProfile.clashSensitivity.userSetting;   
    profileData.stabSens = userProfile.stabSensitivity.userSetting;   
    profileData.shakeSens = userProfile.shakeSensitivity.userSetting;   
    profileData.tapSens = userProfile.tapSensitivity.userSetting;   
    profileData.twistSens = userProfile.twistSensitivity.userSetting;   
    profileData.menuSens = userProfile.menuSensitivity.userSetting;   
    profileData.activePresets_id = userProfile.apID;            // get presets
    profileData.currentPreset_index = userProfile.preset;

    // 2. Overwrite profile entry in COD
    if (!reader.Open(filename, true)) return false;     // open for overwrite
    int32_t retVal = reader.OverwriteEntry(ID, &profileData, sizeof(profileData));
    if (retVal != sizeof(profileData)) {
        // STDOUT.print("[WriteUserProfile] Error writing struct, error="); STDOUT.println(retVal);
        reader.Close();
        return false;       // could not overwrite
    }
    // 3. Prepare active presets table
    retVal = reader.FindEntry(userProfile.apID); // this sets the codProperties so we can get the real table size - we might have fewer presets in RAM, if some were not good
    if (retVal != COD_ENTYPE_TABLE) { reader.Close(); return false; }   // active presets table not found
    uint8_t tableSize = 2*reader.codProperties.table.Columns;
    vector<uint16_t> apData(tableSize, 0);    // active presets data - initialized everyting with 0
    for (uint8_t i=0; i<presets.size(); i++) {
        apData[i] = presets[i].id;
        apData[tableSize/2+i] = presets[i].variation;
    }

    // 4. Overwrite active presets entry in COD
    retVal = reader.OverwriteEntry(userProfile.apID, apData.data(), 2*tableSize);

    // STDOUT.println("[WriteUserProfile] will write active presets table:");
    // for (uint8_t i=0; i<apData.size(); i++) {
    //     STDOUT.print(apData[i]); STDOUT.print(" ");
    // }
    // STDOUT.println("");        

    reader.Close();
    if (retVal != 2*tableSize) return false;

    return true;
} 




// Read and set user profile from profile.cod; load active presets
bool SetUserProfile(const char* filename) {
    #ifdef DIAGNOSE_BOOT
        STDOUT.println("");
        STDOUT.println("Starting xProfile ................................."); 
    #endif
    uint16_t retVal = ReadProfile(filename, 1);     // retVal = ID of active presets
    bool success = false; 
    if (retVal) {
        userProfile.apID = retVal;      // store ID of active presets table, in case we need to overwrite        
        if (LoadActivePresets(filename, retVal)) success = true;
        else if (!presets.size()) userProfile.preset = 0;        // preset error if no valid preset (fatal error!!!)
        #ifdef DIAGNOSE_BOOT
            STDOUT.print("... Setting current preset to #"); STDOUT.println(userProfile.preset);
        #endif
    }
    #ifdef DIAGNOSE_BOOT
        STDOUT.print("xProfile ended ........................."); 
        if (success) STDOUT.println(" SUCCESS."); 
        else STDOUT.println(" FAILED!"); 
        STDOUT.println("");
    #endif 
    return success;

}


   

#endif // XPROFILE_H