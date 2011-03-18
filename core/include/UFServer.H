#ifndef UFSERVER_H
#define UFSERVER_H

#include <pthread.h>
#include <map>
#include <vector>
#include <list>

#include <UF.H>
#include <UFIO.H>

typedef std::map<std::string, std::vector<pthread_t>* > StringThreadMapping;
struct UFServerThreadChooser;
class UFServer
{
public:
    UFServer();
    virtual ~UFServer() {}

    //handle the newly created connection
    virtual void handleNewConnection(UFIO* acceptedConn) = 0;

    //setters and getters
    unsigned int MAX_ACCEPT_THREADS_ALLOWED;
    unsigned int MAX_THREADS_ALLOWED;
    unsigned int MAX_PROCESSES_ALLOWED;
    unsigned int getProcessCount() const { return _childProcesses.size(); }
    unsigned int UF_STACK_SIZE;
    const char* getBindingInterface() const { return _addressToBindTo.c_str() ; }

    struct ListenSocket
    {
        unsigned short int port;
        int fd;
        ListenSocket(unsigned short int p = 0, int f = -1) : port(p), fd(f) { };
    };
    typedef std::list<ListenSocket> ListenSocketList;

    const ListenSocketList &getListenSockets() const { return _listenSockets; }

    //functions that are allowed to be modified by the 
    //inherited class to provide customizable functionalities 
    virtual void preForkRun() {}
    virtual void preBetweenFork() {}
    virtual void postBetweenFork(int childPid) {}
    virtual void postForkPreRun() {}
    virtual void preThreadRun() {}
    virtual void postThreadRun() {}
    virtual void preThreadCreation() {}
    virtual void postThreadCreation() {}
    virtual void preAccept() {}



    /* TODO:
    virtual void parentChildDeathHandler(int childPid) {}
    */
    UFServerThreadChooser*          _threadChooser;

    StringThreadMapping* getThreadList() { return &_threadList; };
    std::vector<pthread_t>* getThreadType(const std::string& type);
    void addThread(const std::string& type, UFScheduler* ufScheduler, pthread_t tid=0);
    void run();

protected:
    void reset();

    std::string                     _addressToBindTo;
    ListenSocketList                _listenSockets;
    unsigned int                    _listenBackLog;

    time_t                          _creationTime;

    std::map<int, time_t>           _childProcesses;
    StringThreadMapping             _threadList;

    //TODO: bool setupParentSignals();
    //TODO: bool setupChildrenSignals();
    //TODO: bool setupMonitoringThread();

    void startThreads();

    void _addListenPort(unsigned short int port)
    {
        _listenSockets.push_back(ListenSocket(port));
    }

private:
    //start processing
};



struct UFServerThreadChooser : public UFIOAcceptThreadChooser
{
    UFServerThreadChooser() { }

    std::pair<UFScheduler*, pthread_t> pickThread(int listeningFd);
    void add(UFScheduler* ufs, pthread_t tid);

protected:
    std::vector<std::pair<UFScheduler*, pthread_t> > _threadList;
};

inline void UFServerThreadChooser::add(UFScheduler* ufs, pthread_t tid)
{
    _threadList.push_back(std::make_pair(ufs, tid));
}

inline std::pair<UFScheduler*, pthread_t> UFServerThreadChooser::pickThread(int listeningFd)
{
    static unsigned int lastLocUsed = 0;
    if(!_threadList.size())
    {
        std::cerr<<"there has to be some fabric to hand the request to"<<std::endl;
        exit(1);
    }

    return _threadList[lastLocUsed++%(_threadList.size())];
}

#endif
