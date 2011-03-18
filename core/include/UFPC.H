#ifndef UFPC_H
#define UFPC_H

#include <set>
#include <stack>
#include <deque>
#include <UF.H>
//#include <Factory.H>

struct UFMutex;
struct UFProducerData;
struct UFConsumer;
struct UFProducer;


struct UFConsumer
{
    friend class UFProducer;
    friend class UFJoinableProducer;
    friend class UFNonJoinableProducer;

    //user needs to call input->releaseMe after consuming the data
    UFProducerData* waitForData(UF* uf = 0, size_t* numRemaining = 0, TIME_IN_US timeToWait=0);
    bool hasData(UF* uf = 0);

    bool getNotifyOnExitOnly() const;
    UF* getUF() const;

    bool                            _requireLockToWaitForUpdate; //if the developer is aware that both the producer and all the consumers are going to run in the same thread - only then set this variable to false to gain some perf. benefits

    UFConsumer();
    virtual ~UFConsumer() { reset(); }
    void reset();

    virtual std::string getMyType() = 0;

    UFMutex                         _consumersProducerSetLock;
    bool                            _notifyOnExitOnly;


protected:
    UF*                             _currUF;
    std::string                     _myType;
    std::deque<UFProducerData*>     _queueOfDataToConsume;
    UFMutex                         _queueOfDataToConsumeLock;

    void clearDataToConsume();
};
inline UF* UFConsumer::getUF() const { return _currUF; }
inline bool UFConsumer::getNotifyOnExitOnly() const { return _notifyOnExitOnly; }


struct UFJoinableProducer;
struct UFJoinableConsumer : public UFConsumer
{
    friend class UFJoinableProducer;
    friend class UFProducer;
    UFJoinableConsumer(bool notifyOnExitOnly = false);
    ~UFJoinableConsumer() { resetMe(); };
    bool joinProducer(UFJoinableProducer* ufp);
    bool removeProducer(UFJoinableProducer* ufp);

    virtual std::string getMyType() { return "UFJoinableConsumer"; }

protected:
    std::deque<UFJoinableProducer*>         _consumersProducerSet;
    void resetMe();
};


struct UFNonJoinableProducer;
struct UFNonJoinableConsumer : public UFConsumer
{
    friend class UFNonJoinableProducer;
    friend class UFProducer;
    UFNonJoinableConsumer(UFNonJoinableProducer* ufp, bool notifyOnExitOnly = false);
    ~UFNonJoinableConsumer() { resetMe(); }
    void resetMe();

    virtual std::string getMyType() { return "UFNonJoinableConsumer"; }

protected:
    bool removeProducer();
    UFNonJoinableProducer*         _ufp;
};






struct UFDataObject;
struct UFProducer
{
    friend class UFProducerConsumerPair;
    friend class UFConsumer;
    UFProducer();
    virtual ~UFProducer() {}
    size_t produceData(UFDataObject* data, int ufpcCode, bool freeDataOnExit = true, UF* uf = 0);
    size_t produceData(UFProducerData* ufpd, UF* uf = 0);
    virtual size_t getConsumerCount() = 0;
    void reset();
    void init();
    bool                            _sendEOFAtEnd;
    bool                            _requireLockToUpdateConsumers;//if the developer is aware that both the producer and the consumers are going to run in the same thread - only then set this variable to false to gain some perf. benefits

protected:
    UFConsumer*                     _mostRecentConsumerAdded;
    UFMutex                         _producersConsumerSetLock; //needed when the consumers are adding or removing themselves from the consumerList
    bool                            _acceptNewConsumers;

    virtual bool addConsumer(UFConsumer* ufc) = 0;
    virtual bool removeConsumer(UFConsumer* ufc) = 0;
    virtual size_t updateConsumers(UFProducerData* ufpd, UF* uf) = 0;
    virtual void removeAllConsumers() = 0;

    UF*                             _uf;
};
inline UFProducer::UFProducer() { init(); }
inline void UFProducer::init() 
{ 
    _acceptNewConsumers = true; 
    _requireLockToUpdateConsumers = true; 
    _mostRecentConsumerAdded = 0;
    _sendEOFAtEnd = true;
}

struct UFJoinableProducer : public UFProducer
{
    UFJoinableProducer() {}
    ~UFJoinableProducer() { reset(); };
    bool addConsumer(UFConsumer* ufc);
    bool removeConsumer(UFConsumer* ufc);
    size_t getConsumerCount();

protected:
    std::deque<UFConsumer*>         _producersConsumerSet;
    size_t updateConsumers(UFProducerData* ufpd, UF* uf);
    void removeAllConsumers();
};
inline size_t UFJoinableProducer::getConsumerCount()
{
    if(!_requireLockToUpdateConsumers)
        return _producersConsumerSet.size();

    size_t count = 0;
    UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    _producersConsumerSetLock.lock(uf);    
    count = _producersConsumerSet.size();
    _producersConsumerSetLock.unlock(uf);

    return count;
}

struct UFNonJoinableProducer : public UFProducer
{
    friend class UFNonJoinableConsumer;
    UFNonJoinableProducer(UFNonJoinableConsumer* ufc) { if(ufc) _mostRecentConsumerAdded = ufc; }
    ~UFNonJoinableProducer() { reset(); }
    size_t getConsumerCount();

protected:
    UFConsumer*     _ufc;
    UFNonJoinableProducer() {}
    size_t updateConsumers(UFProducerData* ufpd, UF* uf);
    void removeAllConsumers();
    bool addConsumer(UFConsumer* ufc);
    bool removeConsumer(UFConsumer* ufc);
};
inline size_t UFNonJoinableProducer::getConsumerCount()
{
    if(!_requireLockToUpdateConsumers)
        return _mostRecentConsumerAdded ? 1 : 0;

    size_t count = 0;
    UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    _producersConsumerSetLock.lock(uf);    
    count = _mostRecentConsumerAdded ? 1 : 0;
    _producersConsumerSetLock.unlock(uf);

    return count;
}






struct UFDataObject
{
    UFDataObject();
    virtual ~UFDataObject();
};
inline UFDataObject::UFDataObject() {}
inline UFDataObject::~UFDataObject() {}

struct UFProducerData
{
    UFDataObject*                   _data;
    UFProducer*                     _producerWhichInserted;
    int                             _ufpcCode;
    bool                            _freeDataOnExit;
    bool                            _lockToUpdate;

    void addRef(size_t numToAdd = 1);
    void reduceRef();
    void reset();

    static UFProducerData* getObj();
    static void releaseObj(UFProducerData* obj);

    ~UFProducerData();
    UFProducerData() { reset(); }

protected:
    UFMutex                         _controlReferenceCount; //control the ref. count of this data
    size_t                          _referenceCount;
    static std::stack<UFProducerData*> _objList;
    static UFMutex                  _objListMutex;
};
inline void UFProducerData::reset()
{
    _referenceCount = 0; 
    _freeDataOnExit = true; 
    _lockToUpdate = true;
}

inline void UFProducerData::addRef(size_t numToAdd)
{
    if(!_lockToUpdate)
    {
        _referenceCount += numToAdd;
        return;
    }

    _controlReferenceCount.getSpinLock();
    _referenceCount += numToAdd;
    _controlReferenceCount.releaseSpinLock();
}

inline void UFProducerData::reduceRef()
{
    if(!_lockToUpdate)
    {
        --_referenceCount;
        return;
    }

    _controlReferenceCount.getSpinLock();
    --_referenceCount;
    _controlReferenceCount.releaseSpinLock();
}

inline UFProducerData* UFProducerData::getObj()
{
    return new UFProducerData();
}

inline void UFProducerData::releaseObj(UFProducerData* obj)
{
    if(!obj)
        return;

    if(obj->_lockToUpdate) obj->_controlReferenceCount.getSpinLock();
    if(!--obj->_referenceCount)
    {
        delete obj;
        return;
    }
    if(obj->_lockToUpdate) obj->_controlReferenceCount.releaseSpinLock();
}

inline UFProducerData::~UFProducerData()
{
    if(_freeDataOnExit && _data)
        delete (_data);
}



struct UFProducerConsumerPair
{
    UFProducerConsumerPair();
    ~UFProducerConsumerPair();
    UFNonJoinableConsumer* getConsumer() const { return _c; }
    UFNonJoinableProducer* getProducer() const { return _p; }

protected:
    UFNonJoinableConsumer*      _c;
    UFNonJoinableProducer*      _p;
};
inline UFProducerConsumerPair::UFProducerConsumerPair()
{
    _p = new UFNonJoinableProducer(0);
    _c = new UFNonJoinableConsumer(_p);
}
inline UFProducerConsumerPair::~UFProducerConsumerPair()
{
    if(_p)
    {
        _p->_requireLockToUpdateConsumers = false; //dont waste time on locking
        _p->_uf = 0; //the producer shouldnt be associated w/ any uf anymore
    }
    if(_c) 
    { 
        _c->_requireLockToWaitForUpdate = false; //dont waste time on locking
        delete _c; 
    }
    if(_p) delete _p; //the producer is killed after the consumer, so that the producer doesnt have to wait for the consumer to die and simply keep the uf alive
}

#endif
