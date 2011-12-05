#ifndef UF_COMPOSABLE_H
#define UF_COMPOSABLE_H

#include <string>

/** 
 * @brief Interface functions that should be implemented by classes
 * that can compose strings in a canonical format
 *
 * Classes that implement this interface are assumed to compose
 * strings in a standard format that can be used as-is for storage or
 * network transmission purposes. It's different from seralization as
 * serialization is meant for internal consumption and doesn't mandate
 * a canonical format.
 * 
 */
class Composable
{
public:

    /** 
     * @brief Composes a string using current state
     * @param str output argument where the data will be stored
     * @param clearStringFirst string argument will be cleared
     *        if set (defaults to false).
     */
    virtual bool compose(std::string &str, bool clearStringFirst = false) const
    {
        if (clearStringFirst)
        {
            str.clear();
        }
        return _determineSizeAndCompose(str);
    }
    
    /** 
     * @brief Composes a string using current state
     * @return str output argument where the data will be stored
     */
    virtual std::string compose() const
    {
        std::string str;
        _determineSizeAndCompose(str);
        return str;
    }

    /**
     * @brief Determine how much space would be needed to compose 
     * @return the size required or -1 if size cannot be determined
     */
    virtual int determineComposeSize() const { return -1; };

    virtual ~Composable() { };

protected:

    virtual bool _compose(std::string &str) const = 0;

    bool _determineSizeAndCompose(std::string &str) const
    {
        int reqSize = determineComposeSize();
        if (reqSize > 0)
        {
            str.reserve(str.size() + reqSize);
        }
        return _compose(str);
    }
};

#endif // UF_COMPOSABLE_H
