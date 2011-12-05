#ifndef STATIC_STRING_LOOKUP_H
#define STATIC_STRING_LOOKUP_H

#include <set>
#include <string>

#include <uf/util/CaseInsensitiveOrderer.H>

/** class that can be used as a case insensitive string lookup */
class StaticStringLookup
{
public:
    /** 
     * @brief populates the lookup table
     * @param strs is an array of string objects whose last element
     *             should be an empty string
     */
    StaticStringLookup(const std::string strs[])
    {
        for (int i = 0; strs[i].size(); ++i)
        {
            _strs.insert(strs[i]);
        }
    }
    /** returns true if given string is present in the loaded set */
    bool exists(const std::string &str) const
    {
        return (_strs.find(str) != _strs.end());
    }
private:
    std::set<std::string, CaseInsensitiveOrderer> _strs;
};

#endif // STATIC_STRING_LOOKUP_H
