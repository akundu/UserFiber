#ifndef UFCONNECTIONPOOLIMPL_H
#define UFCONNECTIONPOOLIMPL_H

#include <time.h>
#include <UF.H>
#include <UFConnectionPool.H>

#include <ext/hash_map>
using namespace std;        
namespace std { using namespace __gnu_cxx; }
struct VoidPtrHash 
{
    size_t operator()(void* p) const 
    {
        return reinterpret_cast< uintptr_t >(p);
    }
};

struct VoidPtrEqual 
{
    bool operator()(void* v1, void* v2) const
    {
        return (v1 == v2);
    }
};


struct UFIO;
struct UFConnIPInfo;
struct UFConnGroupInfo;

typedef std::map<std::string, UFConnGroupInfo*>                 GroupIPMap;
typedef std::map<std::string, UFConnIPInfo*>                    IPInfoStore;
typedef std::multimap<unsigned long long int, UFIO*>            UFIOIntMap;
typedef hash_map<void*, UFConnIPInfo*, VoidPtrHash, VoidPtrEqual> UFConnIPInfoMap;

const time_t DEFAULT_TIMEOUT_OF_IP_ON_FAILURE = 10;
struct UFConnectionPoolImpl
{
    UFConnectionPoolImpl();
    ~UFConnectionPoolImpl();

    // Call before thread creation
    static void init();
    
    bool addGroup(UFConnGroupInfo* stGroupInfo);
    void removeGroup(const std::string& name);
    UFIO* getConnection(const std::string& groupName, bool waitForConnection = true, TIME_IN_US connectTimeout = -1);
    void releaseConnection(UFIO* ufIO, bool connOk = true);
    UFConnGroupInfo* addGroupImplicit(const std::string& groupName);

    void clearUnusedConnections(TIME_IN_US lastUsedTimeDiff = 300000000 /*300 secs*/, unsigned long long int coverListTime = 60*1000*1000);

    void setMaxSimulConnsPerHost(int input);
    int getMaxSimulConnsPerHost();
    void setTimeToTimeoutIPAfterFailure(time_t input);
    time_t getTimeToTimeoutIPAfterFailure();

protected:
    GroupIPMap                  _groupIpMap;
    IPInfoStore                 _ipInfoStore;
    UFConnIPInfoMap             _ufConnIPInfoMap;
    int                         _maxSimulConnsPerHost;
    time_t                      _timeToTimeoutIPAfterFailure;  ///How long should we time out an IP if there is a failure

    UFConnIPInfo* getIPInfo(const std::string& name);
    bool createIPInfo(const std::string& groupName, UFConnGroupInfo* groupInfo, TIME_IN_US connectTimeout = -1);
};
inline void UFConnectionPoolImpl::setMaxSimulConnsPerHost(int input) { _maxSimulConnsPerHost = input; }
inline int UFConnectionPoolImpl::getMaxSimulConnsPerHost() { return _maxSimulConnsPerHost; }
inline void UFConnectionPoolImpl::setTimeToTimeoutIPAfterFailure(time_t input) { _timeToTimeoutIPAfterFailure = input; }
inline time_t UFConnectionPoolImpl::getTimeToTimeoutIPAfterFailure() { return _timeToTimeoutIPAfterFailure; }

struct UFConnIPInfo
{
    UFConnIPInfo(const std::string& ip, 
                       unsigned int port,
                       bool persistent = true, 
                       int maxSimultaneousConns = 0,
                       TIME_IN_US connectTimeout = 0,
                       TIME_IN_US timeToFailOutIPAfterFailureInSecs = 10);
    ~UFConnIPInfo() {} //TODO: remove all the conns w/in currently available and currently used

    const std::string&  getIP() const;
    struct sockaddr_in* getSin();
    unsigned int        getMaxSimultaneousConns() const;
    unsigned int        getInProcessCount() const;
    bool                getPersistent() const;
    unsigned int        getTimeToFailOutIPAfterFailureInSecs() const;
    TIME_IN_US          getConnectTimeout() const;
    time_t              getTimedOut() const;
    void                setTimedOut(time_t t);
    UFIOIntMap&         getCurrentlyAvailableConnections();
    UFMutex*            getMutexToCheckSomeConnection();

    time_t              getLastUsed() const;
    void                incInProcessCount(int numToIncrement = 1);
    UFIO*               getConnection(UFConnIPInfoMap& _UFConnIPInfoMap);
    UFIO*               createConnection();

    UFIOIntMap          _currentlyAvailableConnections;
    unsigned int        _currentlyUsedCount;

protected:
    std::string         _ip;
    unsigned int        _port;
    struct sockaddr_in  _sin;
    unsigned int        _maxSimultaneousConns;
    bool                _persistent; 

    unsigned int        _timeToFailOutIPAfterFailureInSecs; ///how many s to try before considering the connect a failure
    TIME_IN_US          _connectTimeout; ///how many ms to try before considering the connect a failure
    time_t              _timedOut;

    unsigned int        _inProcessCount;
    UFMutex             _someConnectionAvailable;
    time_t              _lastUsed;

};
inline time_t UFConnIPInfo::getLastUsed() const { return _lastUsed; }
inline struct sockaddr_in* UFConnIPInfo::getSin() { return &_sin; }
inline const std::string& UFConnIPInfo::getIP() const { return _ip; }
inline unsigned int UFConnIPInfo::getMaxSimultaneousConns() const { return _maxSimultaneousConns; }
inline unsigned int UFConnIPInfo::getInProcessCount() const { return _inProcessCount; }
inline void UFConnIPInfo::incInProcessCount(int numToIncrement) { _inProcessCount += numToIncrement; }
inline bool UFConnIPInfo::getPersistent() const { return _persistent; }
inline unsigned int UFConnIPInfo::getTimeToFailOutIPAfterFailureInSecs() const { return _timeToFailOutIPAfterFailureInSecs; }
inline TIME_IN_US UFConnIPInfo::getConnectTimeout() const { return _connectTimeout; }
inline time_t UFConnIPInfo::getTimedOut() const { return _timedOut; }
inline void UFConnIPInfo::setTimedOut(time_t t) { _timedOut = t; }
inline UFMutex* UFConnIPInfo::getMutexToCheckSomeConnection() { return &_someConnectionAvailable; }
inline UFIOIntMap& UFConnIPInfo::getCurrentlyAvailableConnections() { return _currentlyAvailableConnections; }




typedef std::vector<std::string>    UFConnIPInfoList;
struct UFConnGroupInfo
{
    UFConnGroupInfo(const std::string& name);
    ~UFConnGroupInfo();

    UFConnIPInfoList&           getIpInfoList();
    time_t                      getTimeToExpireAt() const;
    void                        setTimeToExpireAt(time_t input);
    std::string                 getName() const;

protected:
    std::string                 _name;
    UFConnIPInfoList            _ipInfoList;
    time_t                      _timeToExpireAt;
};
inline std::string UFConnGroupInfo::getName() const { return _name; }
inline time_t UFConnGroupInfo::getTimeToExpireAt() const { return _timeToExpireAt; }
inline void UFConnGroupInfo::setTimeToExpireAt(time_t input) { _timeToExpireAt = input; }
inline UFConnIPInfoList& UFConnGroupInfo::getIpInfoList() { return _ipInfoList; }



#endif
