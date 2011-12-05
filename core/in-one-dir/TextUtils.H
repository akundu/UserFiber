#ifndef TEXT_UTILS_H
#define TEXT_UTILS_H

#include <string>

namespace TextUtils 
{

    /** returns true if character is a blank (space or TAB) */
    inline bool isBlank(char ch)
    {
        return ((ch == ' ') || (ch == '\t'));
    }

    /** Chops trailing whitespace */
    inline void chopTrailingWs(std::string &str)
    {
        size_t origSize, newSize;
        origSize = str.size();
        for (newSize = origSize; newSize && isBlank(str[newSize - 1]); --newSize);
        if (newSize != origSize)
        {
            str.resize(newSize);
        }
    }

    extern const std::string EMPTY_STRING;
    extern const char *LINE_TERMINATOR;
    extern const size_t LINE_TERMINATOR_SIZE;

    /**
     * @brief converts a string into an unsigned integer
     * @param ptr string containing numerical data
     * @param data output argument where converted number will be stored
     * @param base radix to use for conversion
     * @return true if string contained valid unsigned integer
     */
    bool parseUInt(const char *ptr, unsigned int &data, int base = 10);
    
    /**
     * @brief converts a string into an unsigned short integer
     * @param ptr string containing numerical data
     * @param data output argument where converted number will be stored
     * @param base radix to use for conversion
     * @return true if string contained valid unsigned short integer
     */
    bool parseUShort(const char *ptr, unsigned short &data, int base = 10);

    /**
     * @brief converts a string into an unsigned floating point number
     * @param ptr string containing numerical data
     * @param data output argument where converted number will be stored
     * @return true if string contained valid unsigned floating point number
     */
    bool parseUFloat(const char *ptr, float &data);

};


#endif // TEXT_UTILS_H
