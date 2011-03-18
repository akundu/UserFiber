#ifndef UFCONNECTIONPOOL_H
#define UFCONNECTIONPOOL_H

#include <time.h>

#include <string>
#include <vector>
#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <UF.H>

class UFConnGroupInfo;
class UFConnIPInfo;
class UFIO;
class UFConnectionPoolImpl;
struct UFConnectionPool
{
    UFConnectionPool();

    bool addGroup(UFConnGroupInfo* groupInfo);
    UFConnGroupInfo* removeGroup(const std::string& groupName);
    UFIO* getConnection(const std::string& groupName, bool waitForConnection = true, TIME_IN_US connectTimeout = -1);
    void releaseConnection(UFIO* ufIO, bool connOk = true);
    void clearUnusedConnections(TIME_IN_US lastUsedTimeDiff = 300000000 /*300 secs*/, unsigned long long int coverListTime = 60*1000*1000);

    ///how long to timeout an ip that we cant connect to (is not responding)
    void setTimeToTimeoutIPAfterFailure(TIME_IN_US timeout);
    TIME_IN_US getTimeToTimeoutIPAfterFailure();
    void setMaxSimulConnsPerHost(int input);
    int getMaxSimulConnsPerHost();
    double getGroupAvailability(const std::string& name) const;

    std::string fillInfo(std::string& data, bool detailed = false) const;

protected:
    UFConnectionPoolImpl*           _impl;
};

struct UFConnectionPoolCleaner : public UF
{
    void run();
    UFConnectionPoolCleaner (bool registerMe = false);
    UF* createUF() { return new UFConnectionPoolCleaner(); }
};
inline UFConnectionPoolCleaner::UFConnectionPoolCleaner (bool registerMe) { /*if(registerMe) _myLoc = UFFactory::getInstance()->registerFunc((UF*)this);*/ }


#endif
