#ifndef IO_USER_THREADS_H
#define IO_USER_THREADS_H

#include <string>
#include <ext/hash_map>
#include <stdint.h>
#include <UF.H>

namespace std { using namespace __gnu_cxx; }




struct UFIOAcceptThreadChooser
{
    virtual std::pair<UFScheduler*,pthread_t> pickThread(int listeningFd) = 0;
    virtual ~UFIOAcceptThreadChooser() {}
};


struct UFIO;
struct UFSleepInfo
{
    UFSleepInfo() { _ufio = 0; }
    UFIO*           _ufio;
};


struct UFIOAcceptArgs
{
    UFIOAcceptArgs() { args = 0; ufio = 0; }
    void* args;
    UFIO* ufio;
};




struct UFIOScheduler;
struct UFIO
{
    friend class UFIOScheduler;
    friend class EpollUFIOScheduler;
    UFIO(UF* uf, int fd = -1);
    ~UFIO();
    bool isSetup(bool makeNonBlocking = true);


    static int setupConnectionToAccept(
                    const char* interface_addr, 
                    unsigned short int port, 
                    unsigned short int backlog = 16000, 
                    bool makeSockNonBlocking = true);
    //the fd must have been created using socket + bind + listen 
    //before passing into this function. It also has to be marked as non-blocking
    //call setupConnectionToAccept first - that will prepare the socket to accept conns
    //the fxn will not return
    //TODO: create ipv6 accept model
    void accept(UFIOAcceptThreadChooser* ufiotChooser,
                unsigned short int ufLocation,
                unsigned short int port,
                void* startingArgs,
                void* stackPtr = 0,
                unsigned int stackSize = 0);

    //The fxn will call isSetup which will make the connection request non-blocking and setup the socket
    //TODO: support ipv6 connect
    bool connect(const struct sockaddr *addr, socklen_t addrlen, TIME_IN_US timeout);

    /**
     * @brief read until delimiter is encountered or n-1 bytes has been read
     * @param buf the output buffer. The buffer is null terminated if readLine was successful.
     * @param n the size of the buffer pointed to by buf in bytes
     * @return number of bytes read, not including the delimiter on success. -1 on error, 
     *         getErrno() will return the underlying error.
     */
    ssize_t readLine(char* buf, size_t n, char delim='\n');
    /**
     * @brief read until delimiter is encountered or n-1 bytes has been read
     * @param out the output string
     * @param n readLine will attempt to read at most n-1 bytes
     * @return number of bytes read, not including the delimiter on success or -1 on error. 
     *         getErrno() will return the underlying error.
     */
    ssize_t readLine(std::string &out, size_t n, char delim='\n');

    /**
     * @brief attempt to read nbytes from the underlying fd
     * @param buf the output buffer
     * @param timeout the timeout in microseconds. 0 indicates a non-blocking call, -1 is no timeout
     * @returns number of bytes read, or -1 on error. getErrno() will return the underlying error
     * 
     * since this is most likely going to be run on an edge trigger system
     * reads should be done in such a manner so that if the read call
     * returns back nbyte, then a following read call
     * should be made to ensure that nothing is left in the network buffer
     * since in edge trigger no more events will be generated for data that was
     * already seen earlier as being in the network. Therefore always make 
     * another call to this fxn if the return was == to nbyte
     */
    ssize_t read(void *buf, size_t nbyte, TIME_IN_US timeout = -1);
    ssize_t write(const void *buf, size_t nbyte, TIME_IN_US timeout = -1);
    ssize_t writev(const struct iovec *iov, int iov_size, TIME_IN_US timeout = -1);
    int sendto(const char *msg, 
               size_t len,
               const struct sockaddr *to, 
               socklen_t tolen, 
               TIME_IN_US timeout);
    int sendmsg(const struct msghdr *msg, 
                int flags,
                TIME_IN_US timeout);
    int recvfrom(char *buf, 
                 size_t len, 
                 struct sockaddr *from,
                 socklen_t *fromlen, 
                 TIME_IN_US timeout);
    int recvmsg(struct msghdr *msg, 
                int flags,
                TIME_IN_US timeout);
    
    bool close();

    bool setFd(int fd, bool makeNonBlocking = true);
    void setUF(UF* uf);
    void setUFIOScheduler(UFIOScheduler* ufios);

    unsigned int getErrno() const;
    int getFd() const;
    UF* getUF() const;
    const std::string& getRemoteIP() const;
    unsigned int getRemotePort() const;
    UFIOScheduler* getUFIOScheduler() const;

    static void ufCreateThreadWithIO(pthread_t* tid, UFList* ufsToStartWith);

    UFSleepInfo*                _sleepInfo;
    bool                        _markedActive;
    bool                        _active;

    static int                  RECV_SOCK_BUF;
    static int                  SEND_SOCK_BUF;

protected:
    int                         _fd;
    unsigned int                _errno;
    UF*                         _uf;
    UFIOScheduler*              _ufios;
    char*                       _readLineBuf;
    size_t                      _readLineBufPos;
    size_t                      _readLineBufSize;

    UFIO() { reset(); }
    void reset();

    std::string                 _remoteIP;
    unsigned int                _remotePort;

    int                         _lastEpollFlag;
};
inline unsigned int UFIO::getErrno() const { return _errno; }
inline int UFIO::getFd() const { return _fd; }
inline UF* UFIO::getUF() const { return _uf; }
inline void UFIO::setUF(UF* uf) { _uf = uf; }
inline UFIOScheduler* UFIO::getUFIOScheduler() const { return _ufios; }
inline void UFIO::setUFIOScheduler(UFIOScheduler* ufios) { _ufios = ufios; }
inline const std::string& UFIO::getRemoteIP() const { return _remoteIP; };
inline unsigned int UFIO::getRemotePort() const { return _remotePort; };



struct UFIOScheduler;
//typedef std::map<pthread_t, UFIOScheduler*> ThreadFiberIOSchedulerMap;
typedef std::hash_map<pthread_t, UFIOScheduler*, std::hash<uintptr_t> > ThreadFiberIOSchedulerMap;

struct UFConnectionPool;
struct UFIOScheduler
{
    UFIOScheduler();
    virtual ~UFIOScheduler();

    virtual bool setupForConnect(UFIO* ufio, TIME_IN_US to = -1) = 0;
    virtual bool setupForAccept(UFIO* ufio, TIME_IN_US to = -1) = 0;
    virtual bool setupForRead(UFIO* ufio, TIME_IN_US to = -1) = 0;
    virtual bool setupForWrite(UFIO* ufio, TIME_IN_US to = -1) = 0;
    virtual bool closeConnection(UFIO* ufio) = 0;
    //TODO: support regular poll behavior
    virtual bool rpoll(std::list<UFIO*>& ufioList, TIME_IN_US to = -1) = 0;

    virtual bool isSetup() { return false; }
    virtual void waitForEvents(TIME_IN_US timeToWait) = 0;

    static UFIOScheduler* getUFIOS(pthread_t tid = 0);
    static ThreadFiberIOSchedulerMap    _tfiosscheduler;
    static pthread_key_t                _keyToIdentifySchedulerOnThread;

    // connection pool
    UFConnectionPool* getConnPool() const;

protected:
    UFConnectionPool*           _connPool;
};
inline UFConnectionPool* UFIOScheduler::getConnPool() const { return _connPool; }


#define MAX_FDS_FOR_EPOLL 128*1024-1
//typedef map<int, UFIO*> IntUFIOMap;
typedef std::hash_map<int, UFIO*, std::hash<int> > IntUFIOMap;
typedef std::multimap<TIME_IN_US, UFSleepInfo*> MapTimeUFIO;
struct EpollUFIOScheduler : public UFIOScheduler
{
    EpollUFIOScheduler(
           UF* uf /*the uf that the scheduler will run on to handle this UFIO*/, 
           unsigned int maxFds = MAX_FDS_FOR_EPOLL);
    ~EpollUFIOScheduler();
    bool isSetup(); //call after the c'tor to verify that the structure is correctly setup

    //inputInfo is the flags info (its an int*) for this addition
    bool setupForConnect(UFIO* ufio, TIME_IN_US to = -1);
    bool setupForAccept(UFIO* ufio, TIME_IN_US to = -1);
    bool setupForRead(UFIO* ufio, TIME_IN_US to = -1);
    bool setupForWrite(UFIO* ufio, TIME_IN_US to = -1);
    bool closeConnection(UFIO* ufio);
    bool rpoll(std::list<UFIO*>& ufioList, TIME_IN_US to = -1);


    void waitForEvents(TIME_IN_US timeToWait = 0);

    bool                            _interruptedByEventFd;

protected:
    UF*                             _uf;
    UFScheduler*                    _ufs;
    int                             _epollFd;
    unsigned int                    _maxFds;
    struct epoll_event*             _epollEventStruct;
    IntUFIOMap                      _intUFIOMap;
    bool                            _alreadySetup;


    MapTimeUFIO                     _sleepList;
    TIME_IN_US          _earliestWakeUpFromSleep;

    bool addToScheduler(UFIO* ufio, 
                        void* inputInfo /*flags to identify how ot add*/, 
                        TIME_IN_US to = -1,
                        bool wait = true, 
                        bool runEpollCtl = false);


    std::deque<UFSleepInfo*>        _availableSleepInfo;
    UFSleepInfo* getSleepInfo();
    void releaseSleepInfo(UFSleepInfo& ufsi);
};

inline UFSleepInfo* EpollUFIOScheduler::getSleepInfo()
{
    if(!_availableSleepInfo.empty())
    {
        UFSleepInfo* ufsi = _availableSleepInfo.front();
        _availableSleepInfo.pop_front();
        return ufsi;
    }

    return new UFSleepInfo();
}

inline void EpollUFIOScheduler::releaseSleepInfo(UFSleepInfo& ufsi)
{
    _availableSleepInfo.push_back(&ufsi);
}

struct IORunner : public UF
{
    void run();
    IORunner(bool registerMe = false)
    {
        if(registerMe)
            _myLoc = UFFactory::getInstance()->registerFunc((UF*)this);
    }
    UF* createUF() { return new IORunner(); }
    static IORunner* _self;
    static int _myLoc;
};

#endif
