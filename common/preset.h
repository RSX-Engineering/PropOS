/*********************************************************
 *  XML PRESETS                                          *
 *  (C) Marius RANGU @ RSX Engineering                   *
 *  fileversion: v1.1 @ 2024/04;    license: GPL3        *
 *********************************************************
 
 ********************************************************/
 



#ifndef XPRESET_H
#define XPRESET_H


#define MAX_FONTLEN             16      // maximum length of a font name (including null terminator)
#define MAX_FONTS               100     // maximum number of fonts  
#define MAX_TRACKLEN            22      // maximum length of a track name (including null terminator)
#define MAX_TRACKS              100     // maximum number of tracks  
#define MAX_STYLES_PER_PRESET   6       // maximum number of styles should be maximum number of blades



#include <vector>
#include "CodReader.h"
#include "../styles/styles.h"


#define DEFAULT_FONT "Luke ROTJ"


StringVector fonts;     // list of sound fonts
StringVector tracks;     // list of sound tracks

class Preset{ 

public:
    uint16_t id;
    char name[16];
    // char font[16];
    // char track[22];
    uint8_t font_index;     // 1-based index in fonts:StringVector, 0 if no font assigned
    uint8_t track_index;     // 1-based index in tracks:StringVector, 0 if no track assigned
    uint32_t variation; // uint32 for no reason
    StyleDescriptor* bladeStyle[NUM_BLADES];

    Preset() {     // default constructor
        id = 0;
        strcpy(name, "uninitialized");
        font_index=0;
        track_index=0;

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
            char font[MAX_FONTLEN];
            char track[MAX_TRACKLEN];
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
        // strcpy(font, presetData.font);
        // strcpy(track, presetData.track);
        font_index = fonts.GetIndex(presetData.font);
        track_index = tracks.GetIndex(presetData.track);
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
        // strcpy(font, presetData.font);
        // strcpy(track, presetData.track);        
        font_index = fonts.GetIndex(presetData.font);
        track_index = tracks.GetIndex(presetData.track);
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

    // Overwrite current preset in the COD file - must previously exist!
    bool Overwrite(const char* filename) {
        CodReader reader;
        presetData_t presetData; 

        // STDOUT.print("Overwriting preset "); STDOUT.print(id); STDOUT.print(" in "); STDOUT.println(filename); 

        // 1. Open file for read and overwrite, then find the preset
        if (!reader.Open(filename, 1)) return false;               // File not found
        if (reader.FindEntry(id) != COD_ENTYPE_STRUCT) { reader.Close(); return false; }  // ID not found or wrong entry type
        if (reader.codProperties.structure.Handler != HANDLER_Preset) { reader.Close(); return false; }   // wrong handler

        // 2. Read preset data
        uint32_t numBytes = reader.ReadEntry(id, (void*)&presetData, sizeof(presetData));      // Attempt to read a structure with the requested ID. 
        if (numBytes != sizeof(presetData)) { reader.Close(); return false; }

        // 3. Overwrite preset data
        strcpy(presetData.name, name);
        // STDOUT.print("- Name: "); STDOUT.println(presetData.name);
        // strcpy(presetData.font, font);
        if (!fonts.GetString(font_index, presetData.font)) presetData.font[0] = 0;
        // STDOUT.print("- Font index: "); STDOUT.print(font_index); STDOUT.print(" = "); STDOUT.println(presetData.font);
        // strcpy(presetData.track, track);
        if (!tracks.GetString(track_index, presetData.track)) presetData.track[0] = 0;
        // STDOUT.print("- Track index: "); STDOUT.print(track_index); STDOUT.print(" = "); STDOUT.println(presetData.track);
        // Installed blades:
        for (uint8_t i=0; i<NUM_BLADES; i++)
            if (bladeStyle[i]) {
                strcpy(presetData.styleName[i], bladeStyle[i]->name);
                // STDOUT.print("- Blade "); STDOUT.print(i+1); STDOUT.print(" style: "); STDOUT.println(presetData.styleName[i]);
            }
            else { // unassigned style
                strcpy(presetData.styleName[i], "");
                // STDOUT.print("- Blade "); STDOUT.print(i+1); STDOUT.println(" has no style.");
            }
        // Uninstalled blades:
        if (NUM_BLADES < MAX_STYLES_PER_PRESET)
        for (uint8_t i=NUM_BLADES; i<MAX_STYLES_PER_PRESET; i++) {
            strcpy(presetData.styleName[i], "");
            // STDOUT.print("- Blade "); STDOUT.print(i+1); STDOUT.println(" has no style.");
        }


        // 4. Write back to file
        int32_t owrResult = reader.OverwriteEntry(id, (void*)&presetData, sizeof(presetData));
        reader.Close();
        // STDOUT.print("Overwrite result: "); STDOUT.println(owrResult);
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

    char default_font[MAX_FONTLEN] = "";

    // Check track and font, replace with defaults if fails
    bool CheckSounds() {
        bool success = true;        // assume success
        // 1. Check font
        char font[MAX_FONTLEN];
        font[0] = 0;    // make sure it's an empty font name
        if (font_index) fonts.GetString(font_index, font);    
        
        if (!font[0] || !LSFS::Exists(font)) { // font does not exist, attempt to use default                    
            if (!default_font[0]) {  // determine and store default font
                if (LSFS::Exists(DEFAULT_FONT)) strcpy(default_font, DEFAULT_FONT); // use specified default, if exists
                else if(fonts.count) fonts.GetString(1, default_font); // use first font in the list
            }
            if (default_font[0]) {      // use default font if possible
                #if defined(DIAGNOSE_BOOT) && SABERPROP_VERSION != 'Z'
                    STDOUT.println();
                    STDOUT.print(">>>>>>>>>> WARNING: Invalid font '"); STDOUT.print(font);
                    STDOUT.print("', using '"); STDOUT.print(default_font); STDOUT.print("' as default <<<<<<<<<<    ");
                #endif
                strcpy(font, default_font);  
            }
            else {  // no font found at all
                #ifdef DIAGNOSE_BOOT
                    STDOUT.println();
                    STDOUT.print(" >>>>>>>>>> ERROR: Invalid font '"); STDOUT.print(font);
                    STDOUT.println("' and no default font found!  <<<<<<<<<<    ");
                #endif
                success = false;

            }                      
        }

        // 2. Check track (if specified)
        char track[MAX_TRACKLEN];
        if (tracks.GetString(track_index, track)) { // track specified} 
            if (!LSFS::Exists(track)) { 
                // STDOUT.print("Preset "); STDOUT.print(name); STDOUT.print(" references invalid track '"); 
                // STDOUT.print(track); STDOUT.println("' - will use none.");
                #if defined(DIAGNOSE_BOOT) && SABERPROP_VERSION != 'Z'
                    STDOUT.print(">>>>>>>>>> WARNING invalid track '"); STDOUT.print(track);
                    STDOUT.print("'. No track assigned. <<<<<<<<<<    ");
                #endif
                track_index = 0;
                // success = false;
            }
        }

        return success;
    }

    void Print() {
        STDOUT.print("ID: "); STDOUT.println(id);
        STDOUT.print("Name: "); STDOUT.println(name);
        char font[MAX_FONTLEN];
        fonts.GetString(font_index, font);
        STDOUT.print("Font: "); STDOUT.println(font);
        STDOUT.print("Variation: "); STDOUT.println(variation);
        STDOUT.print("Track: "); 
        if (track_index) {
            char track[MAX_TRACKLEN];
            tracks.GetString(track_index, track);
            STDOUT.println(track); 
        } else STDOUT.println("<NONE>"); 
        for (uint8_t i=0; i<NUM_BLADES; i++) {
            STDOUT.print("Style"); STDOUT.print(i+1); STDOUT.print(": ");
            if (bladeStyle[i]) STDOUT.println(bladeStyle[i]->name);
            else STDOUT.println("<NONE>");        
        }        
    }


};

extern vector<Preset> presets;


// Scan the sound directory for fonts and store them as a compact list of strings
void ScanSoundFonts(bool report = true) {
    

    // 1. If presets are already set (rescanning), iterate through all presets and save their font names in a temporary StringVector
    StringVector tempPresetsFonts;
    if (!presets.empty()) {
        // STDOUT.print("ScanSoundFont found "); STDOUT.print(presets.size()); STDOUT.println(" presets:");
        size_t blockSize = 0;
        char fontNames[MAX_FONTS][MAX_FONTLEN]; // temporary storage for font names
        for (size_t i = 0; i < presets.size(); ++i) {
            char tempFontName[MAX_FONTLEN];
            if (fonts.GetString(presets[i].font_index, tempFontName)) {
                strcpy(fontNames[i], tempFontName);
                blockSize += strlen(tempFontName) + 1;
                // STDOUT.print("Preset ID="); STDOUT.print(presets[i].id); STDOUT.print(", font=");
                // STDOUT.print(tempFontName); STDOUT.print(", index= "); STDOUT.println(presets[i].font_index);
            } else {
                strcpy(fontNames[i], "*"); // mark no font
                blockSize += 2;
                // STDOUT.print("Preset ID="); STDOUT.print(presets[i].id); STDOUT.println(", no font.");
            }
        }
        char* tempBlock = new char[blockSize + 1]; // +1 for final null terminator
        char* currentPos = tempBlock;
        for (size_t i = 0; i < presets.size(); ++i) {
            strcpy(currentPos, fontNames[i]);
            currentPos += strlen(fontNames[i]) + 1;
        }
        tempBlock[blockSize++] = '\0'; // Final null terminator
        tempPresetsFonts.Assign(tempBlock, blockSize);
        blockSize = 0; // need those again...
    }


    #ifdef DIAGNOSE_BOOT
        if (report) STDOUT.print("* Scanning sound fonts ... ");
    #endif
    // 2. Scan and store in temporary RAM
    char fontNames[MAX_FONTS][MAX_FONTLEN]; // temporary storage for font names  
    uint8_t fontCount = 0;
    size_t blockSize = 0;
    LOCK_SD(true);
    for (LSFS::Iterator iter("/"); iter; ++iter) {
        if (iter.isdir()) {
            char fname[128];
            strcpy(fname, iter.name());
            strcat(fname, "/");
            char* fend = fname + strlen(fname);
            bool isfont = false;
            if (!isfont) {
                strcpy(fend, "hum.wav");
                isfont = LSFS::Exists(fname);
            }
            if (!isfont) {
                strcpy(fend, "hum01.wav");
                isfont = LSFS::Exists(fname);
            }
            if (!isfont) {
                strcpy(fend, "hum1.wav");
                isfont = LSFS::Exists(fname);
            }
            if (isfont && strlen(iter.name())<MAX_STRINGLEN) { // ignore fonts with long names
                // Copy font name to fontNames
                strcpy(fontNames[fontCount++], iter.name());
                blockSize += strlen(iter.name()) + 1; // Add the length of the font name plus the null terminator
                #ifdef DIAGNOSE_BOOT
                    if (report) {
                        STDOUT.print("|"); 
                        STDOUT.print(iter.name());
                    }
                #endif
            }
        }
    }
    LOCK_SD(false);

    // 3. Compact font names in a dinamically allocated contiguous memory block
    if (fontCount) {
        blockSize++; // Add one more byte for the final null terminator
        char* stringBlock = new char[blockSize];   // Allocate memory for stringMem based on the required size
        char* currentPos = stringBlock;              
        for (size_t i = 0; i < fontCount; ++i) {  // Copy font names from fontNames to stringBlock
            strcpy(currentPos, fontNames[i]); 
            currentPos += strlen(fontNames[i]) + 1; // Move pointer past the copied font name and null terminator
        }
        stringBlock[blockSize - 1] = '\0'; // Ensure the last byte is a null terminator, marking the end of the block

        if (fonts.Assign(stringBlock, blockSize) == fontCount) { // Assign the memory block to the StringVector 'fonts'
            #ifdef DIAGNOSE_BOOT
            if (report) {
                STDOUT.print("| Total=");
                STDOUT.println(fontCount); 
            }
            #endif  
        }
        else {
            delete[] stringBlock; // Free the memory block if it was not assigned to the StringVector
            #ifdef DIAGNOSE_BOOT
                if (report) STDOUT.println(">>>>>>>>>> ERROR: Could not assign font list.");
            #endif
        }
    }
    else {
        #ifdef DIAGNOSE_BOOT
            if (report) STDOUT.println(" NO FONT FOUND!");
        #endif
    }    

    // 4. Restore the font indexes for all the presets in 'presets', accounting for the new content of 'fonts'
    if (!presets.empty()) {
        // STDOUT.println("ScanSoundFonts restored fonts: ");
        char tempFontName[MAX_FONTLEN];
        for (size_t i = 0; i < presets.size(); ++i) {
            tempFontName[0] = 0;
            tempPresetsFonts.GetString(i + 1, tempFontName);
            if (tempFontName[0] != '*') { // preset had font
                presets[i].font_index = fonts.GetIndex(tempFontName);
                // STDOUT.print("Preset ID="); STDOUT.print(presets[i].id); STDOUT.print(", font=");
                // STDOUT.print(tempFontName); STDOUT.print(", index= "); STDOUT.println(presets[i].font_index);
            } else {
                presets[i].font_index = 0; // font not found, assign 0
                // STDOUT.print("Preset ID="); STDOUT.print(presets[i].id); STDOUT.println(", no font.");
            }
        }
    }

}   

// Scan the sound directory for tracks and store them as a compact list of strings
void ScanSoundTracks(bool report = true) {

    char trackNames[MAX_TRACKS][MAX_TRACKLEN]; // temporary storage for track names  
    uint8_t trackCount = 0;
    size_t blockSize = 0;

    // print the number of presets currently in the 'presets' vector

    // 1. If presets are already set (rescanning), iterate through all presets and save their tracks in a temporary StringVector
    StringVector tempPresetsTracks;
    if (presets.size()) {
        // STDOUT.print("ScanSoundTracks found "); STDOUT.print(presets.size()); STDOUT.println(" presets:");
        for (size_t i = 0; i < presets.size(); ++i) {
            char tempTrackName[MAX_TRACKLEN];
            if (tracks.GetString(presets[i].track_index, tempTrackName)) {
                strcpy(trackNames[i], tempTrackName);
                blockSize += strlen(tempTrackName) + 1;
                // STDOUT.print("Preset ID="); STDOUT.print(presets[i].id); STDOUT.print(", track="); 
                // STDOUT.print(tempTrackName); STDOUT.print(", index= "); STDOUT.println(presets[i].track_index);
            }
            else {
                strcpy(trackNames[i], "*"); // mark no track
                blockSize += 2;                
                // STDOUT.print("Preset ID="); STDOUT.print(presets[i].id); STDOUT.println(", no track."); 
            }

        }
        char* tempBlock = new char[blockSize + 1]; // +1 for final null terminator
        char* currentPos = tempBlock;
        for (size_t i = 0; i < presets.size(); ++i) {
            strcpy(currentPos, trackNames[i]);
            currentPos += strlen(trackNames[i]) + 1;
        }
        tempBlock[blockSize++] = '\0'; // Final null terminator
        tempPresetsTracks.Assign(tempBlock, blockSize);
        blockSize = 0;      // need those again...
        trackCount = 0;
    }

    #ifdef DIAGNOSE_BOOT
        if (report) STDOUT.print("* Scanning sound tracks ... ");
    #endif

    // 2. Scan and store in temporary RAM
    // 2.1 Search 'tracks' folder in root    
    LOCK_SD(true);
    if (LSFS::Exists("tracks")) {
        for (LSFS::Iterator i2("tracks"); i2; ++i2) {
            if (endswith(".wav", i2.name()) && i2.size() > 200000) {
                char nameandpath[2*MAX_TRACKLEN];
                strcpy(nameandpath, "tracks/");
                strcat(nameandpath, i2.name());
                size_t len = strlen(nameandpath);
                if (len < MAX_TRACKLEN) { // ignore tracks with long names
                    strcpy(trackNames[trackCount], nameandpath);    
                    blockSize += len + 1;           // Add the length of the track name plus the null terminator
                    #ifdef DIAGNOSE_BOOT
                        if (report) {
                            STDOUT.print("|"); 
                            STDOUT.print(trackNames[trackCount]);                        
                        }
                    #endif
                    trackCount++;   // don't do this earlier or strcpy will fail
                }                   
            }
        }        
    }                          
    // // 2.2 Search 'tracks' folder in subfolders
    // for (LSFS::Iterator iter("/"); iter; ++iter) {
    //     if (iter.isdir()) {
    //         PathHelper path(iter.name(), "tracks");
    //         if (LSFS::Exists(path)) {
    //             for (LSFS::Iterator i2(path); i2; ++i2) {
    //                 if (endswith(".wav", i2.name()) && i2.size() > 200000) {
    //                     STDOUT.print(path); STDOUT.print("/"); STDOUT.println(i2.name()); 
    //                     
    //                 }
    //             }
    //         }
    //     }
    // }
    LOCK_SD(false);

    // 3. Compact track names in a dinamically allocated contiguous memory block
    if (trackCount) {
        blockSize++; // Add one more byte for the final null terminator
        // STDOUT.print("Found "); STDOUT.print(trackCount); STDOUT.print(" tracks."); 
        // STDOUT.print("Total size: "); STDOUT.println(blockSize);        
		// STDOUT.flushTx();
		char* stringBlock = new char[blockSize];    // Allocate memory for stringMem based on the required size
                                                    
        char* currentPos = stringBlock;              
        for (size_t i = 0; i < trackCount; ++i) {  // Copy track names from trackNames to stringBlock
            strcpy(currentPos, trackNames[i]); 
            currentPos += strlen(trackNames[i]) + 1; // Move pointer past the copied track name and null terminator
        }
        stringBlock[blockSize - 1] = '\0'; // Ensure the last byte is a null terminator, marking the end of the block

        if (tracks.Assign(stringBlock, blockSize) == trackCount) { // Assign the memory block to the StringVector 'tracks'
            #ifdef DIAGNOSE_BOOT
                if (report) {
                    STDOUT.print("| Total="); 
                    STDOUT.println(trackCount);
                }
            #endif  
        }
        else {
            delete[] stringBlock; // Free the memory block if it was not assigned to the StringVector
            #ifdef DIAGNOSE_BOOT
                if (report) STDOUT.println(">>>>>>>>>> ERROR: Could not assign track list.");
            #endif
        }
    }
    else {
        #ifdef DIAGNOSE_BOOT
            if (report) STDOUT.println(" NO TRACK FOUND!");
        #endif
    }

    // 5. Restore the track indexes for all the presets in 'presets', accounting for the new content of 'tracks'
    if (!presets.empty()) {
        // STDOUT.println("ScanSoundTracks restored tracks: "); 
        char tempTrackName[MAX_TRACKLEN];
        // tempPresetsTracks.SetCurrent(1);
        // tempPresetsTracks.GetCurrent(tempTrackName); STDOUT.print("tempPresetsTracks starts with "); STDOUT.println(tempTrackName);
        for (size_t i = 0; i < presets.size(); ++i) {
            tempTrackName[0]=0;
            tempPresetsTracks.GetString(i+1, tempTrackName);
            // STDOUT.print("i="); STDOUT.print(i); STDOUT.print(", extracted track name: "); STDOUT.print(tempTrackName); STDOUT.print(" --- ");
            if (tempTrackName[0] != '*') {  // preset had track
                presets[i].track_index = tracks.GetIndex(tempTrackName);
                // STDOUT.print("Preset ID="); STDOUT.print(presets[i].id); STDOUT.print(", track="); 
                // STDOUT.print(tempTrackName); STDOUT.print(", index= "); STDOUT.println(presets[i].track_index);
            }
            else { 
                presets[i].track_index = 0; // track not found, assign 0
                // STDOUT.print("Preset ID="); STDOUT.print(presets[i].id); STDOUT.println(", no track."); 
            }
        }
    }




}






// Read list of active presets from profile.cod and populate the 'presets' vector
// __attribute__((optimize("Og")))
bool LoadActivePresets(const char* filename, uint16_t ID) {
    
    // fonts.ScanSoundFonts();     // scan memory for sound fonts and store them in compact form
    ScanSoundFonts();     // scan memory for sound fonts and store them in fonts:StringVector
    delay(1);  // need this for ESP (reason = ESP)
    ScanSoundTracks();      //  

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
                    char font[MAX_FONTLEN];
                    fonts.GetString(presets.back().font_index, font);
                    STDOUT.print(". Font: "); STDOUT.print(font);
                    STDOUT.print(". Track: "); 
                   
                    if (presets.back().track_index) {
                        char track[MAX_TRACKLEN];
                        tracks.GetString(presets.back().track_index, track);
                        STDOUT.print(track); 
                    } else STDOUT.print("<NONE>");               
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
            if (presets[0].font_index != presets[i].font_index) {
                SaberBase::monoFont = false;
                break;
            }           
    // STDOUT.print(" MONO-FONT = "); STDOUT.println(SaberBase::monoFont);

    // #ifdef DIAGNOSE_BOOT
    // #endif
    return success;

}




#endif // XPRESET_H



