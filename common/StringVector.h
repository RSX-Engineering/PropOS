/********************************************************************
 *  STRING VECTOR                                                   *
 *  (C) Marius RANGU @ RSX Engineering                              *
 *  fileversion: v1.0 @ 2024/02;    license: GPL3                   *
 ********************************************************************
 *  Access a contiguous memory block as a vector of strings         *   
 ********************************************************************/


#ifndef STRING_VECTOR__H
#define STRING_VECTOR__H


#define MAX_STRINGS 100     // maximum number of strings in vector
#define MAX_STRINGLEN 32    // maximum string length, including null terminator. 
                            // This is used just to prevent long searches in case of memory corruption, make sure it covers all use cases


class StringVector {
private:
    char* stringMem;  // Contiguous memory block containing all strings. 
    size_t size;        // size of the memory block
    char* current;      // current position in the memory block (for iterative access)


public:
    _readonly_(uint8_t, count);  // number of strings in the list: read as 'count', write (privately) as 'p_count'

    // Constructor - initialize the list with no strings and an empty stringMem
    StringVector() {  stringMem = 0; Clear(); }
    ~StringVector() { if (stringMem) delete[] stringMem; }

    void Clear() {
        if (stringMem) delete[] stringMem;
        stringMem = nullptr;
        p_count = 0;
        size = 0;
        current = nullptr;
    }

    // Assigns a contiguous memory block to the vector, containing null-terminated strings.
    // Returns the number of strings in the block.
    // If the data is invalid (consecutive null terminators or missing block terminator), returns 0.
    uint8_t Assign(char* data, size_t dataSize) {

        if (stringMem) {     // If the vector is already initialized, clear the existing data

            Clear();
        }

        if (dataSize < 3) { 
            // STDOUT.println("Data is invalid (too short to contain at least one string and a block terminator)"); 
            return 0; }// Data is invalid (too short to contain at least one string and a block terminator}
        // Validate the data for consecutive null terminators before the end of the block
        for (size_t i = 0; i < dataSize - 2; i++) { // Exclude the last character for this check
            if (data[i] == '\0' && data[i + 1] == '\0') { 
                // STDOUT.println("Data is invalid due to consecutive null terminators"); 
                return 0; }// Data is invalid due to consecutive null terminators}
        }

        // If the last 2 characters before the end of the block are not null terminators, also consider it invalid
        if (data[dataSize - 2] != '\0' || data[dataSize - 1] != '\0') {
            // STDOUT.println("Data is invalid (missing block terminator)"); 
            return 0;}

        stringMem = data;
        size = dataSize;
        p_count = 0; // Reset the count

        // Count the number of null-terminated strings in the data, excluding the block terminator
        for (size_t i = 0; i < dataSize - 1; ++i) { // Exclude the last null terminator from the count
            if (data[i] == '\0') p_count++;
        }

        // Set the current pointer to the start of the memory block
        current = stringMem;
        // STDOUT.print("Assigned "); STDOUT.print(count); STDOUT.print(" strings using ");  STDOUT.print(size); STDOUT.println(" bytes");   
        return count; // Data assigned successfully
    }

   
    // Copies the 'index'-th null-terminated string from the contiguous memory block to the specified destination.
    // index is 1-based !!!
    // Returns the number of characters copied (excluding the null terminator), or 0 if there are less than n strings or the n-th string exceeds MAX_STRINGLEN characters.
    uint8_t GetString(uint8_t index, char* dest) {
        // Check if stringMem points to the end of the memory block
        char* src = stringMem;
        if (!src || *src == '\0') { 
            return 0; // No strings to extract
        }
        if (!index) return 0;   // Index out of bounds (1-based index
        index %= count+1;       // Wrap around if index exceeds the number of strings 

        uint8_t currentString = 1; // Start with the first string

        // Loop until we find the start of the n-th string or reach the end of the memory block
        while (currentString < index && *src != '\0') {
            // Skip over the current string
            while (*src != '\0') {
                src++;
            }
            src++; // Move past the null terminator of the current string
            currentString++;
        }

        // If we reached the end of the memory block without finding n strings, return 0
        if (*src == '\0' || currentString < index) {
            return 0;
        }

        // Now src points to the start of the n-th string or the end of the block
        uint8_t char_counter = 0;
        while (*src != '\0' && char_counter < (MAX_STRINGLEN - 1)) {
            *dest++ = *src++; // Copy character from src to dest
            char_counter++;
        }

        // If we stopped because we reached MAX_STRINGLEN - 1 characters and the next character is not '\0',
        // it means the source string exceeds our limit for copying, so we return 0.
        if (char_counter == (MAX_STRINGLEN - 1) && *src != '\0') {
            return 0; // The string exceeds MAX_STRINGLEN - 1 characters, return 0
        }
        
        *dest = '\0'; // Null-terminate the destination string

        return char_counter; // Return the number of characters copied (excluding the null terminator)
    }

    // Searches for a string in the memory block. If found, returns the 1-based index. If not found, returns 0.
    uint8_t GetIndex(const char* string) {
        if (!stringMem || !string || *string == '\0') {
            return 0; // Early return if the stringMem is not initialized or string is invalid
        }

        char* pos = stringMem;
        uint8_t index = 1; // Start with the first string (1-based index)

        while (*pos != '\0') { // Loop until the end of the memory block
            if (strcmp(pos, string) == 0) {
                return index; // Found the string, return its index
            }

            // Move to the next string in the memory block
            while (*pos != '\0') {
                pos++;
            }
            pos++; // Skip the null terminator to move to the start of the next string

            index++; // Increment the index for the next string
        }

        return 0; // String not found
    }

    // Copies current null-terminated string from the contiguous memory block to the specified destination.
    // 'Current' starts at 1 when scanning and is updated by SetCurrent, GetNext and GetPrev.
    // Returns the number of characters copied (excluding the null terminator), or 0 if the string exceeds MAX_STRINGLEN characters.
    uint8_t GetCurrent(char* dest) {
        
        if (!current || *current == '\0' || !stringMem) return 0; // Vector or current string not initialized
        
        char* src = current;        
        uint8_t char_counter = 0;

        while (*src != '\0' && char_counter < MAX_STRINGLEN-1) {
            *dest++ = *src++; // Copy character from src to dest
            char_counter++;
        }

        if (char_counter == MAX_STRINGLEN-1 && *src != '\0') {
            // The string exceeds MAX_STRINGLEN characters, return 0
            return 0;
        }

        *dest = '\0'; // Null-terminate the destination string
        return char_counter; // Return the number of characters copied (excluding the null terminator)
    }


    // Returns the 1-based index of the current string. If not set or there are no strings, returns 0.
    uint8_t GetCurrentIndex() {
        if (!current || *current == '\0' || !stringMem) return 0; // Vector or current string not initialized

        char* pos = stringMem;
        uint8_t index = 1; // Start with the first string (1-based index)

        while (pos != current) { // Loop until pos matches the current string's position
            if (*pos == '\0') {
                return 0; // Reached the end of the memory block without finding the current string, should not happen
            }

            // Move to the next string in the memory block
            while (*pos != '\0') {
                pos++;
            }
            pos++; // Skip the null terminator to move to the start of the next string

            index++; // Increment the index for the next string
        }

        return index; // Found the current string, return its index
    }

    // Sets next string as current and copies it from the contiguous memory block to the specified destination.
    // 'Current' starts at 1 when scanning and is updated by SetCurrent, GetNext and GetPrev.
    // Returns the number of characters copied (including the null terminator), or 0 if the string exceeds MAX_STRINGLEN characters.
    uint8_t GetNext(char* dest) {
        if (count==1) return GetCurrent(dest);
        if (!current || *current == '\0' || !stringMem) return 0; // Vector or current string not initialized

        // Move past the current string to find the start of the next string
        char* nextPos = current;
        while (*nextPos != '\0') {
            nextPos++;
        }
        // Move past the null terminator of the current string
        nextPos++;

        // Check if we've reached the end of the block, meaning this is the last string
        if (*nextPos == '\0') {
            // Wrap around to the first string
            nextPos = stringMem; // Reset to the start of the block
        }

        // Copy the next (or first) string to the destination
        char* src = nextPos; // Start copying from the new position
        uint8_t char_counter = 0; // Count the number of characters copied

        while (*src != '\0' && char_counter < (MAX_STRINGLEN - 1)) {
            *dest++ = *src++;
            char_counter++;
        }
        
        if (char_counter == MAX_STRINGLEN-1 && *src != '\0') {
            // The string exceeds MAX_STRINGLEN characters, return 0
            return 0;
        }
        *dest = '\0'; // Null-terminate the destination string

        // Update current to point to the next (or first) string, making it the new 'current'
        current = nextPos;

        return char_counter; // Return the number of characters copied (excluding the null terminator)
    }

    // Sets previous string as current and copies it from the contiguous memory block to the specified destination.
    // 'Current' starts at 1 when scanning and is updated by SetCurrent, GetNext and GetPrev.
    // Returns the number of characters copied (excluding the null terminator), or 0 if the string exceeds MAX_STRINGLEN characters.
    uint8_t GetPrev(char* dest) {
        if (count == 1) return GetCurrent(dest);
        if (!current || !stringMem) {
            // If the list is uninitialized or empty, indicate failure/no fonts
            return 0;
        }

        char* prevPos = current;
        if (current == stringMem) {
            // Wrap around to the last string in the block
            prevPos = stringMem + size - 3; // Position before the null terminator of the last string (account for the extra null terminator at the end of the block !)
            while (prevPos > stringMem && *(prevPos - 1) != '\0') prevPos--; // Move to the start of the last string
        } else {
            // Move back to find the start of the previous string
            prevPos--; // Move back from the current start position
            if (*prevPos == '\0') prevPos--; // If directly at a null terminator, move back one more
            while (prevPos > stringMem && *(prevPos - 1) != '\0') prevPos--; // Find the start of the previous string
        }


        // Copy the previous (or last) string to the destination
        char* src = prevPos; // Start copying from the new position
        uint8_t char_counter = 0; // Count the number of characters copied

        while (*src != '\0' && char_counter < (MAX_STRINGLEN - 1)) {
            *dest++ = *src++;
            char_counter++;
        }

        if (char_counter == MAX_STRINGLEN-1 && *src != '\0') {
            // The string exceeds MAX_STRINGLEN characters, return 0
            return 0;
        }
        *dest = '\0'; // Null-terminate the destination string

        // Update current to point to the previous (or last) string, making it the new 'current'
        current = prevPos;

        return char_counter; // Return the number of characters copied (excluding the null terminator)
    }

    // Set the current string as the 'index'-th  in the list (1-based index).
    // Returns false if the string was not found
    bool SetCurrent(uint8_t index) {
        if (index == 0 || index > count || !stringMem  || *stringMem == '\0') {
            return false; // Index out of bounds or stringMem is empty
        }

        char* pos = stringMem;
        uint8_t currentString = 1; // Start with the first string

        // Loop until we find the start of the index-th string or reach the end of the memory block
        while (currentString < index && *pos != '\0') {
            // Skip over the current string
            while (*pos != '\0') {
                pos++;
            }
            pos++; // Move past the null terminator of the current string
            currentString++;
        }

        // If we reached the end of the memory block without finding index strings, return false
        if (*pos == '\0' || currentString < index) {
            return false;
        }

        // Now pos points to the start of the index-th string
        current = pos; // Set current to the start of the found string
        return true;
    }

    bool SetCurrent(const char* string) {
        if (!stringMem || *stringMem == '\0' || !string || *string == '\0') {
            return false; // stringMem is empty or string is invalid
        }

        char* pos = stringMem;
        while (*pos != '\0') { // Loop through all strings in the memory block
            if (strcmp(pos, string) == 0) { // Compare current string with the target 
                current = pos; // If found, set current to the start of the found string
                return true;
            }
            while (*pos != '\0') { // Move to the end of the current string
                pos++;
            }
            pos++; // Move past the null terminator to the start of the next string
        }

        return false; // String not found
    }

};





#endif // STRING_VECTOR__H