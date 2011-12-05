#ifndef _UF_HASH_MAP_H_
#define _UF_HASH_MAP_H_
#include <iostream>
/*
    support for string as the key in hash_map
*/
#include <ext/hash_map>

using namespace std;
namespace std { using namespace __gnu_cxx; }

namespace __gnu_cxx
{
    template<> struct hash< std::string >
    {
        size_t operator()( const std::string& x ) const
        {
            return hash< const char* >()( x.c_str() );
        }
    };
}

#endif
