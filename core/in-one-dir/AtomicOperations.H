#ifndef ATOMIC_OPERATIONS_H
#define ATOMIC_OPERATIONS_H

#include <stdint.h>

inline size_t atomic_increment(size_t* old, int value)
{
    return __sync_fetch_and_add(old, 1);
}


#endif
