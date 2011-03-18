#ifndef _UF_STATS_H
#define _UF_STATS_H

#include <stdint.h>

namespace UFStats
{
    void registerStats(bool lock_needed = false);
    extern uint32_t currentConnections;
    extern uint32_t connectionsHandled;
    extern uint32_t txnSuccess;
    extern uint32_t txnFail;
    extern uint32_t txnReject;
    extern uint32_t bytesRead;
    extern uint32_t bytesWritten;
}

#endif
