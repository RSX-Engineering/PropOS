/*********************************************************
 *  XML PRESETS                                          *
 *  (C) Marius RANGU @ RSX Engineering                   *
 *  fileversion: v1.0 @ 2022/10;    license: GPL3        *
 *********************************************************
 
 ********************************************************/
 



#ifndef XPRESET_H
#define XPRESET_H


#define MAX_ACTIVE_PRESETS      10  // maximum number of active presets 
#define MAX_STYLES_PER_PRESET   6   // maximum number of styles should be maximum number of blades

#include <vector>
#include "CodReader.h"
#include "../styles/styles.h"

// #define DEFAULT_FONT "default"
#define DEFAULT_FONT "Luke ROTJ"




class Preset{ 

public:
    uint16_t id;
    char name[16];
    char font[16];
    char track[22];
    uint32_t variation; // uint32 for no reason
    StyleDescriptor* bladeStyle[NUM_BLADES];

    Preset() {     // default constructor
        id = 0;
        strcpy(name, "uninitialized");
        strcpy(font, DEFAULT_FONT);
        strcpy(track, "");

        variation = 0;
        for (uint8_t i=0; i<NUM_BLADES; i++)
            bladeStyle[i] = 0;
    }    

    Preset(uint16_t id_) {  // id constructor
        Preset();
        id = id_;
    }
    
    // Data structure to read/write presets to COD files
    struct presetData_t {    // Preset data type to read from COD file
            char name[16];
            char font[16];
            char track[22];
            char styleName[MAX_STYLES_PER_PRESET][16];   // name of style for each blade
    } __attribute__((packed)); 

    // Read preset from .COD and assign blade styles
    bool Read(const char* filename, uint16_t ID) {
        CodReader reader;
        presetData_t presetData; 

        // STDOUT.print("[xPreset_Test()] Reading "); STDOUT.print(filename); STDOUT.print(", ID="); STDOUT.print(ID); STDOUT.print(": ");

        bool success = true;
        if (!reader.Open(filename)) return false;               // File not found
        if (reader.FindEntry(ID) != COD_ENTYPE_STRUCT) success = false;  // wrong entry type
        if (reader.codProperties.structure.Handler != HANDLER_Preset) success = false;   // wrong handler
        if (success) {
            uint32_t numBytes = reader.ReadEntry(ID, (void*)&presetData, sizeof(presetData));      // Attempt to read a structure with the requested ID. 
            if (numBytes != sizeof(presetData)) success = false;
            // STDOUT.print(" Read "); STDOUT.print(numBytes); STDOUT.print(" bytes, expected "); STDOUT.print(sizeof(presetData)); STDOUT.print(". ");
        }
        reader.Close();    
        if (!success) return false;

        strcpy(name, presetData.name);
        strcpy(font, presetData.font);
        strcpy(track, presetData.track);
        id = ID;
        variation = 0;      // variation is user-profile-data, not preset-data. 

        // // TODO: assign styles to blades based on what they're good for
        // for (uint8_t i=0; i<NUM_BLADES; i++)
        //     bladeStyle[i] = GetStyle(presetData.styleName[i]);

        if (!AssignStylesToBlades(&presetData)) { 
            // error assigning styles to blades

            return false;
        }

        return true;
    }

   // Same as Read() but file is already open and will not be closed
    bool FastRead(CodReader* file, uint16_t ID) {
        presetData_t presetData; 
        if (file->FindEntry(ID) != COD_ENTYPE_STRUCT) return false;  // wrong entry type
        if (file->codProperties.structure.Handler != HANDLER_Preset) return false;   // wrong handler
        uint32_t numBytes = file->ReadEntry(ID, (void*)&presetData, sizeof(presetData));      // Attempt to read a structure with the requested ID. 
        if (numBytes != sizeof(presetData)) return false;

        strcpy(name, presetData.name);
        strcpy(font, presetData.font);
        strcpy(track, presetData.track);        
        id = ID;
        variation = 0;      // variation is user-profile-data, not preset-data. 
        
        // // TODO: assign styles to blades based on what they're good for (AssignStyles)
        // for (uint8_t i=0; i<NUM_BLADES; i++)
        //     bladeStyle[i] = GetStyle(presetData.styleName[i]);

        if (!AssignStylesToBlades(&presetData)) { 
            // error assigning styles to blades
            return false;
        }

        // STDOUT.print("Preset "); STDOUT.print(id); STDOUT.print(" = ");
        // Print();
        return true;
    }


    // Assign preset styles to blades based on good4 flags
    // Rules of assignment:
    // ===================
    //      1. Each blade gets a style. If no style specified by preset can be assigned, that blade will get the default style.
    //      2. Each style can be assigned to any number of blades, including none.
    //      3. Each blade gets the first appropriate style with minimum blade assignments.
    bool AssignStylesToBlades(presetData_t* presetData) {
        
        if (!styles.size()) return false;   // critical error: no styles available

        // 1. Find installed blades
        BladeBase* installedBlades[NUM_BLADES];
        StyleHeart bladeStyleTypes[NUM_BLADES];
        for (uint8_t i=0; i<NUM_BLADES; i++) {  
            installedBlades[i] = BladeAddress(i+1);     // 0 if blade not installed
            // bladeStyleTypes[i] = installedBlades[i]->StylesAccepted();
            if (installedBlades[i]) {
                bladeStyleTypes[i] = installedBlades[i]->StylesAccepted();
                // STDOUT.print("[AssignStylesToBlades] blade at "); STDOUT.print((uint32_t)(installedBlades[i]));
                // STDOUT.print(" wants styles of type "); STDOUT.println(bladeStyleTypes[i]);
            }
        }

        // 2. Get styles and initialize usage counters
        StyleDescriptor* presetStyles[MAX_STYLES_PER_PRESET];   // pointers to specified styles
        uint8_t styleUsage[MAX_STYLES_PER_PRESET];               // keeps track of how many blades each style is assigned to
        for (uint8_t i=0; i<MAX_STYLES_PER_PRESET; i++) {
            presetStyles[i] = GetStyle(presetData->styleName[i]);
            if(!presetStyles[i]) {
                styleUsage[i] = 255;     // specified style not found or no style specidied
                if (*(presetData->styleName[i])) { // non-critical error:  style specified but not found
                    STDOUT.print(" >>>>>>>>>> WARNING: style '"); STDOUT.print(presetData->styleName[i]);  STDOUT.print("' not installed. Please upgrade firmware! <<<<<<<<<<   ");
                }
            }
            else {  
                styleUsage[i] = 0;      
                // STDOUT.print("[AssignStylesToBlades] found style "); STDOUT.print(presetData->styleName[i]); STDOUT.print(" at address "); STDOUT.println((uint32_t)(presetStyles[i]));
            }
        }

        // 3. Find a style for each installed blade
        for (uint8_t blade=0; blade<NUM_BLADES; blade++)
         if (installedBlades[blade]) {
            bool found = false;
            uint8_t targetUsage = 0;        // start by assuming each style is gonna be used for a single blade
            while (!found && targetUsage<=MAX_STYLES_PER_PRESET) {
                // 3.1 Seach for the first appropriate style with targetUsage
                for (uint8_t style=0; style<MAX_STYLES_PER_PRESET; style++) {
                    if (styleUsage[style] <= targetUsage)   // that eliminates styles not specified
                    if (presetStyles[style]->good4 & bladeStyleTypes[blade]) {
                        // 3.2 Found a good style: store style descriptor and stop searching
                        bladeStyle[blade] = presetStyles[style];
                        #ifdef DIAGNOSE_BOOT
                            STDOUT.print("Blade"); STDOUT.print(blade+1); STDOUT.print(" style: '");
                            STDOUT.print(bladeStyle[blade]->name); STDOUT.print("'.  ");
                        #endif
                        // STDOUT.print("[AssignStylesToBlades] Assigned style '"); STDOUT.print(bladeStyle[blade]->name);
                        // STDOUT.print("' to blade #"); STDOUT.println(blade+1);
                        found = true;
                        styleUsage[style] ++;
                        break;  // exit for
                    }
                }
                // 3.2 If not found, try again with increased target usage
                if (!found) targetUsage++;
            }
            // 3.3 If still not found: assign default style (first in vector that is good for, or first in vector regardless)
            if(!found) { 
                bladeStyle[blade] = GetDefaultStyle(bladeStyleTypes[blade]);
                if (!bladeStyle[blade]) {   // no style at all found!
                    #ifdef DIAGNOSE_BOOT
                        STDOUT.println("");
                        STDOUT.print(" >>>>>>>>>> ERROR: No good style found for blade"); STDOUT.print(blade+1);  
                        STDOUT.println(" and no default style is available. Please upgrade firmware! <<<<<<<<<<   ");
                    #endif              
                    return false;     
                }
                // STDOUT.print("[AssignStylesToBlades] Could not find an appropriate style for blade #"); STDOUT.print(blade+1);
                // STDOUT.print(", assigned default style '"); STDOUT.print(bladeStyle[blade]->name); STDOUT.println("'");
                #ifdef DIAGNOSE_BOOT
                    STDOUT.println("");
                    STDOUT.print(" >>>>>>>>>> WARNING: No good style found for blade"); STDOUT.print(blade+1);  STDOUT.print(", using '");
                    STDOUT.print(bladeStyle[blade]->name); STDOUT.print("' as default <<<<<<<<<<   ");
                #endif

            }

        }            
        return true;    // all the errors above are non-critical if at least the default style could be assigned
    }

    char default_font[16] = "";

    // Check track and font, replace with defaults if fails
    bool CheckSounds() {
        bool success = true;        // assume success
        // 1. Check font
        if (!LSFS::Exists(font)) { // font does not exist, attempt to use default
            if (LSFS::Exists(DEFAULT_FONT)) {  // default font exists: warning
                #if defined(DIAGNOSE_BOOT) && ULTRAPROFFIE_VERSION != 'Z'
                    STDOUT.println();
                    STDOUT.print(">>>>>>>>>> WARNING: Invalid font '"); STDOUT.print(font);
                    STDOUT.print("', using '"); STDOUT.print(DEFAULT_FONT); STDOUT.print("' as default <<<<<<<<<<    ");
                #endif
                strcpy(font, DEFAULT_FONT);  
                // STDOUT.print("Preset "); STDOUT.print(name); STDOUT.print(" references invalid font '"); 
                // STDOUT.print(font); STDOUT.println("' - will use default.");
            }
            else { // default font does not exist: assign ANY font
                if (!default_font[0]) { // scan for a valid font if didn't do this already                
                    for (LSFS::Iterator iter("/"); iter; ++iter) {
                        if (iter.isdir()) {
                            char fname[128];
                            strcpy(fname, iter.name());
                            strcat(fname, "/");
                            char* fend = fname + strlen(fname);
                            bool isfont = false;
                            strcpy(fend, "hum.wav");
                            isfont = LSFS::Exists(fname);
                            if (!isfont) {
                                strcpy(fend, "hum01.wav");
                                isfont = LSFS::Exists(fname);
                            }
                            if (!isfont) {
                                strcpy(fend, "hum1.wav");
                                isfont = LSFS::Exists(fname);
                            }
                            if (isfont) {
                                strcpy(default_font, iter.name());
                                break;                                
                            }
                        }
                    }
                }
                if (default_font[0]) {    // asign dynamic default
                    #if defined(DIAGNOSE_BOOT) && ULTRAPROFFIE_VERSION != 'Z'
                        STDOUT.println();
                        STDOUT.print(" >>>>>>>>>> WARNING: Invalid font '"); STDOUT.print(font); STDOUT.print("' and invalid default font '"); STDOUT.print(DEFAULT_FONT);
                        STDOUT.print("', using '"); STDOUT.print(default_font); STDOUT.println("' as first found <<<<<<<<<<    ");
                    #endif                    
                    strcpy(font, default_font);  

                }
                else {  // failed: no font found
                    #ifdef DIAGNOSE_BOOT
                        STDOUT.println();
                        STDOUT.print(" >>>>>>>>>> ERROR: Invalid font '"); STDOUT.print(font);
                        STDOUT.println("' and no other font found!  <<<<<<<<<<    ");
                    #endif
                    success = false;
                }

            }
        }
        // 2. Check track (if specified)
        if (track[0])
            if (!LSFS::Exists(track)) { 
                // STDOUT.print("Preset "); STDOUT.print(name); STDOUT.print(" references invalid track '"); 
                // STDOUT.print(track); STDOUT.println("' - will use none.");
                #if defined(DIAGNOSE_BOOT) && ULTRAPROFFIE_VERSION != 'Z'
                    STDOUT.print(">>>>>>>>>> WARNING invalid track '"); STDOUT.print(track);
                    STDOUT.print("'. No track assigned. <<<<<<<<<<    ");
                #endif
                strcpy(track, "");
                // success = false;
            }
       

        return success;
    }

    void Print() {
        STDOUT.print("ID: "); STDOUT.print(id);
        STDOUT.print(", Name: "); STDOUT.print(name);
        STDOUT.print(",  Font: "); STDOUT.print(font);
        STDOUT.print(",  Variation: "); STDOUT.print(variation);
        STDOUT.print(",  Track: "); 
        if (track[0]) STDOUT.print(track); else STDOUT.print("<NONE>"); 
        STDOUT.print(",  ");
        for (uint8_t i=0; i<NUM_BLADES; i++) {
            STDOUT.print("Style"); STDOUT.print(i+1); STDOUT.print(": ");
            if (bladeStyle[i]) STDOUT.print(bladeStyle[i]->name);
            else STDOUT.print("0");
            STDOUT.print(" ");
        }        
        STDOUT.println("");
    }


};

extern vector<Preset> presets;

// Read list of active presets from profile.cod and populate the 'presets' vector
// __attribute__((optimize("Og")))
bool LoadActivePresets(const char* filename, uint16_t ID) {
    // 0. Clear previous presets table
    presets.resize(0);                                          
    presets.shrink_to_fit();
    // 1. Find active presets table
    #ifdef DIAGNOSE_BOOT
        STDOUT.print("* Loading active presets table from "); STDOUT.print(filename); 
        STDOUT.print(", ID = "); STDOUT.print(ID); STDOUT.print(" ... ");
    #endif
    CodReader reader;
    bool success = true;    // assume success
    if (!reader.Open(filename)) return false;               // File not found
    if (reader.FindEntry(ID) != COD_ENTYPE_TABLE) success = false;  // wrong entry type
    if (reader.codProperties.structure.Handler != HANDLER_PresetList) success = false;   // wrong handler
    if (reader.codProperties.table.Rows != 2) success = false;  // incorect table format
    if (!reader.codProperties.table.Columns) success = false; // no active preset
    if (!success) { 
        #ifdef DIAGNOSE_BOOT
            STDOUT.println("Failed, could not find table");
        #endif
        reader.Close(); 
        return false; 
    }
    
    // 2. Read table into temporary vector
    vector<uint16_t> apData;    // active presets data
    apData.resize(2*reader.codProperties.table.Columns);    
    uint32_t nrOfBytes = reader.ReadEntry(ID, (void*)apData.data(), 4*reader.codProperties.table.Columns);   // Read 4 bytes for each preset
    reader.Close();
    if (nrOfBytes != 4*reader.codProperties.table.Columns) return false;
    #ifdef DIAGNOSE_BOOT
        STDOUT.print("found "); STDOUT.print(reader.codProperties.table.Columns); 
        STDOUT.println(" presets.");
    #endif
    // STDOUT.print("[LoadActivePresets] Read "); STDOUT.print(nrOfBytes/4); STDOUT.print(" presets from "); 
    // STDOUT.print(filename); STDOUT.print(", ID="); STDOUT.println(ID);
    
    // 3. Allocate RAM for 'presets' vector
    presets.resize(0);                                          
    presets.shrink_to_fit();
    presets.reserve(reader.codProperties.table.Columns);     
    
    // 4. Read each preset and populate the 'presets' vector with valid ones
    #ifdef DIAGNOSE_BOOT
        STDOUT.print("* Loading presets from file "); STDOUT.print(PRESETS_FILE); STDOUT.print(" ... ");
    #endif
    if (!reader.Open(PRESETS_FILE)) {
        #ifdef DIAGNOSE_BOOT
            STDOUT.println("Failed, could not find file.");
        #endif
        return false;               // File not found    
    }
    #ifdef DIAGNOSE_BOOT
        STDOUT.println();
    #endif
    uint16_t presetID, presetVar;
    for (uint8_t i=0; i<reader.codProperties.table.Columns; i++) {
        success = true;     // assume success
        presetID = *(apData.data() + i);
        presetVar = *(apData.data() + reader.codProperties.table.Columns + i);
        #ifdef DIAGNOSE_BOOT
            STDOUT.print("... Loading preset #"); STDOUT.print(i+1); STDOUT.print(", ID = "); 
            STDOUT.print(presetID); STDOUT.print(" ... ");
        #endif
        // 4.1 Read preset in the new element and check validity
        presets.emplace_back();   // Add new element at the end of the vector, assuming preset will be successfully assigned
        if (presets.back().FastRead(&reader, presetID)) {
            // 4.2 Set variation
            presets.back().variation = presetVar;
            // presets.back().Print();
            // 4.3 Check validity and fix is possible
            if (!presets.back().CheckSounds()) {
                success = false; // mark we found at least one error
                presets.pop_back(); // delete newly added preset
                // #ifdef DIAGNOSE_BOOT
                //     STDOUT.println("Failed, invalid preset."); 
                // #endif
            } else {
                #ifdef DIAGNOSE_BOOT
                    STDOUT.print("Color variation: "); STDOUT.print(presets.back().variation);
                    STDOUT.print(". Font: "); STDOUT.print(presets.back().font);
                    STDOUT.print(". Track: "); STDOUT.print(presets.back().track);                
                    STDOUT.print(". Preset name: '"); STDOUT.print(presets.back().name);
                    STDOUT.println("'.");
                #endif
            }
        }
        else { // failed, delete newly created vector element
            success = false;        // mark we found at least one error
            presets.pop_back();     // delete newly added preset
            // #ifdef DIAGNOSE_BOOT
            //     STDOUT.println("Failed, preset not found."); 
            // #endif
        }
    }
    
    reader.Close();
    
    // Check how many fonts are active
    SaberBase::monoFont = true;
    if (presets.size()>1)
        for (uint8_t i=1; i<presets.size(); i++) 
            if (strcmp(presets[0].font, presets[i].font)) {
                SaberBase::monoFont = false;
                break;
            }           
    // STDOUT.print(" MONO-FONT = "); STDOUT.println(SaberBase::monoFont);

    // #ifdef DIAGNOSE_BOOT
    // #endif
    return success;

}




#endif // XPRESET_H



