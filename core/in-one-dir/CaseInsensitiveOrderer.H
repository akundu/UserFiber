#ifndef CASE_INSENSITIVE_ORDERER
#define CASE_INSENSITIVE_ORDERER

#include <string>

/** 
 * @brief Functor to perform case insensitive 'less than' comparison
 * of US-ASCII charset (only) strings
 */
class CaseInsensitiveOrderer
{
public:
    /** returns true if s1 is less than s2 */
    inline bool operator()(const std::string &s1, const std::string &s2) const
    {
        _init();
        size_t i = 0;
        while (true)
        {
            if (i == s1.size())
            {
                return (i < s2.size());
            }
            if (i == s2.size())
            {
                return false;
            }
            if (_lookup[static_cast<unsigned char>(s1[i])] < _lookup[static_cast<unsigned char>(s2[i])])
            {
                return true;
            }
            if (_lookup[static_cast<unsigned char>(s1[i])] > _lookup[static_cast<unsigned char>(s2[i])])
            {
                return false;
            }
            ++i;
        }
    }

    static bool compare(const std::string &s1, const std::string &s2)
    {
        return _compare(s1.data(), s1.size(), s2.data(), s2.size());
    }
    
    static bool compare(const char *s1, size_t s1Len, const char *s2, size_t s2Len)
    {
        if ((s1Len && !s1) || (s2Len && !s2))
        {
            return false;
        }
        return _compare(s1, s1Len, s2, s2Len);
    }
    
    static bool compare(const char *s1, const char *s2, size_t sLen)
    {
        if (sLen && (!s1 || !s2))
        {
            return false;
        }
        return _compare(s1, s2, sLen);
    }

    static bool compare(const std::string &s1, const char *s2, size_t s2Len)
    {
        if (s2Len && !s2)
        {
            return false;
        }
        return _compare(s1.data(), s1.size(), s2, s2Len);
    }

private:

    static bool _lookupInitialized;
    static char _lookup[256];

    static void _init()
    {
        if (!_lookupInitialized)
        {
            for (int i = 0; i < 255; ++i)
            {
                _lookup[i] = static_cast<char>(i);
            }
            for (int i = 'A'; i <= 'Z'; ++i)
            {
                _lookup[i] = 'a' + (i - 'A');
            }
            _lookupInitialized = true;
        }
    }

    static bool _compare(const char *s1, size_t s1Len, const char *s2, size_t s2Len)
    {
        if (s1Len != s2Len)
        {
            return false;
        }
        return _compare(s1, s2, s1Len);
    }

    static bool _compare(const char *s1, const char *s2, size_t sLen)
    {
        _init();
        for (size_t i = 0; i < sLen; ++i)
        {
            if (_lookup[static_cast<unsigned char>(s1[i])] != _lookup[static_cast<unsigned char>(s2[i])])
            {
                return false;
            }
        }
        return true;
    }
};

#endif // CASE_INSENSITIVE_ORDERER
