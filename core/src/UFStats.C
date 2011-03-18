#include <UFStats.H>
#include <UFStatSystem.H>

uint32_t UFStats::currentConnections;
uint32_t UFStats::connectionsHandled;
uint32_t UFStats::txnSuccess;
uint32_t UFStats::txnFail;
uint32_t UFStats::txnReject;
uint32_t UFStats::bytesRead;
uint32_t UFStats::bytesWritten;

namespace UFStats
{
    void registerStats(bool lock_needed)
    {
        UFStatSystem::registerStat("connections.current", &currentConnections, lock_needed);
        UFStatSystem::registerStat("connections.handled", &connectionsHandled, lock_needed);
        UFStatSystem::registerStat("txn.success", &txnSuccess, lock_needed);
        UFStatSystem::registerStat("txn.fail", &txnFail, lock_needed);
        UFStatSystem::registerStat("txn.reject", &txnReject, lock_needed);
        UFStatSystem::registerStat("bytes.read", &bytesRead, lock_needed);
        UFStatSystem::registerStat("bytes.written", &bytesWritten, lock_needed);
    }
}
