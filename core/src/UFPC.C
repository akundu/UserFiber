#include <UFPC.H>
#include <stdio.h>

using namespace std;

size_t UFProducer::produceData(UFDataObject* data, int ufpcCode, bool freeDataOnExit, UF* uf)
{
    if(!uf)
        uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    _uf = uf;

    //create the UFProducerData structure
    UFProducerData* ufpd = UFProducerData::getObj();
    ufpd->_data = data;
    ufpd->_freeDataOnExit = freeDataOnExit;
    ufpd->_producerWhichInserted = this;
    ufpd->_ufpcCode = ufpcCode;
    ufpd->_lockToUpdate = _requireLockToUpdateConsumers;

    size_t numConsUpdated = updateConsumers(ufpd, uf);
    if(numConsUpdated)
        return numConsUpdated;

    delete ufpd;
    return 0;
}

size_t UFProducer::produceData(UFProducerData* ufpd, UF* uf)
{
    if(!uf)
        uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    _uf = uf;

    return updateConsumers(ufpd, uf);
}

size_t UFJoinableProducer::updateConsumers(UFProducerData* ufpd, UF* uf)
{
    size_t consumerCount = getConsumerCount();
    if(!consumerCount)
        return 0;

    //increase the reference count
    ufpd->addRef(consumerCount);

    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.lock(uf);
    switch(consumerCount)
    {
        case 1: //optimize there being only one consumer
            {
                _mostRecentConsumerAdded->_queueOfDataToConsume.push_back(ufpd);
                if(!_mostRecentConsumerAdded->getNotifyOnExitOnly() || (ufpd->_ufpcCode == 0)) //signal if necessary
                {
                    if(_requireLockToUpdateConsumers)
                        _mostRecentConsumerAdded->_queueOfDataToConsumeLock.signal();
                    else //wake up from a block
                        UFScheduler::getUFScheduler()->addFiberToScheduler(_mostRecentConsumerAdded->getUF());
                }
                break;
            }
        default:
            {
                //for each of the consumers add it to their queue
                for(deque<UFConsumer*>::iterator beg = _producersConsumerSet.begin();
                    beg != _producersConsumerSet.end(); ++beg)
                {
                    UFConsumer* ufc = *beg;
                    //add to the consumer's queue
                    if(_requireLockToUpdateConsumers)
                    {
                        ufc->_queueOfDataToConsumeLock.lock(uf);
                        ufc->_queueOfDataToConsume.push_back(ufpd);
                        if(!ufc->getNotifyOnExitOnly() || (ufpd->_ufpcCode == 0))
                            ufc->_queueOfDataToConsumeLock.signal();
                        ufc->_queueOfDataToConsumeLock.unlock(uf);
                    }
                    else
                    {
                        ufc->_queueOfDataToConsume.push_back(ufpd);
                        if(!ufc->getNotifyOnExitOnly() || (ufpd->_ufpcCode == 0))
                            UFScheduler::getUFScheduler()->addFiberToScheduler(ufc->getUF());
                    }
                }
                break;
            }
    }
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.unlock(uf);

    return consumerCount;
}

size_t UFNonJoinableProducer::updateConsumers(UFProducerData* ufpd, UF* uf)
{
    size_t count = 0;
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.lock(uf);
    if(_mostRecentConsumerAdded)
    {
        //increase the reference count
        ufpd->addRef((count = 1));

        _mostRecentConsumerAdded->_queueOfDataToConsume.push_back(ufpd);
        if(!_mostRecentConsumerAdded->getNotifyOnExitOnly() || (ufpd->_ufpcCode == 0)) //signal if necessary
        {
            if(_requireLockToUpdateConsumers)
                _mostRecentConsumerAdded->_queueOfDataToConsumeLock.signal();
            else //wake up from a block
                UFScheduler::getUFScheduler()->addFiberToScheduler(_mostRecentConsumerAdded->getUF());
        }
    }
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.unlock(uf);

    return count;
}

UFConsumer::UFConsumer()
{ 
    reset();
}

void UFConsumer::reset()
{
    _currUF = 0; 
    _requireLockToWaitForUpdate = true;
}

UFJoinableConsumer::UFJoinableConsumer(bool notifyOnExitOnly)
{
    _notifyOnExitOnly = notifyOnExitOnly;
}

void UFConsumer::clearDataToConsume()
{
    if(!_queueOfDataToConsume.empty())
    {
        deque<UFProducerData*>::iterator beg = _queueOfDataToConsume.begin();
        for(; beg != _queueOfDataToConsume.end(); ++beg)
            UFProducerData::releaseObj((*beg));
    }
}

UFProducerData* UFConsumer::waitForData(UF* uf, size_t* numRemaining, TIME_IN_US timeToWait)
{
    if(numRemaining)
        *numRemaining = 0;

    if(!uf)
        uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    _currUF = uf;

    if(_requireLockToWaitForUpdate) _queueOfDataToConsumeLock.lock(uf);
    while(_queueOfDataToConsume.empty())
    {
        if(_requireLockToWaitForUpdate) 
        {
            if(!timeToWait)
                _queueOfDataToConsumeLock.condWait(uf);
            else
            {
                if(!_queueOfDataToConsumeLock.condTimedWait(uf, timeToWait))
                {
                    _queueOfDataToConsumeLock.unlock(uf); //release the lock gotten earlier
                    return 0;
                }
            }
        }
        else 
            uf->block(); //wait for the producer to wake up the consumer
    }

    //read the first element from the queue
    UFProducerData* result = _queueOfDataToConsume.front();
    _queueOfDataToConsume.pop_front();
    if(numRemaining)
        *numRemaining = _queueOfDataToConsume.size();
    if(_requireLockToWaitForUpdate) _queueOfDataToConsumeLock.unlock(uf); //release the lock gotten earlier

    return result;
}

bool UFConsumer::hasData(UF* uf)
{
    if(!_requireLockToWaitForUpdate)
        return !(_queueOfDataToConsume.empty());

    if(!uf)
        uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    bool result;
    _queueOfDataToConsumeLock.lock(uf);
    result = !(_queueOfDataToConsume.empty());
    _queueOfDataToConsumeLock.unlock(uf); //release the lock gotten earlier

    return result;
}

bool UFJoinableConsumer::joinProducer(UFJoinableProducer* ufp)
{
    if(!ufp)
        return false;

    if(_requireLockToWaitForUpdate)
    {
        _consumersProducerSetLock.getSpinLock();
        _consumersProducerSet.push_back(ufp);
        _consumersProducerSetLock.releaseSpinLock();
    }
    else
        _consumersProducerSet.push_back(ufp);

    //notify the producer that we're adding this consumer
    if(!ufp->addConsumer(this))
        return false;

    return true;
}

bool UFNonJoinableConsumer::removeProducer()
{
    if(!_ufp || !_ufp->removeConsumer(this))
        return false;
    _ufp = 0; //no need to lock - since we cant add any other producer w/ this type of consumer
    return true;
}

UFNonJoinableConsumer::UFNonJoinableConsumer(UFNonJoinableProducer* ufp, bool notifyOnExitOnly)
{
    _ufp = ufp;
    _ufp->addConsumer(this);
    _notifyOnExitOnly = notifyOnExitOnly;
}

void UFNonJoinableConsumer::resetMe()
{
    removeProducer();
    clearDataToConsume();
}

void UFJoinableConsumer::resetMe()
{
    //1. notify all the producers on exit
    for(deque<UFJoinableProducer*>::iterator beg = _consumersProducerSet.begin(); beg != _consumersProducerSet.end(); )
    {
        removeProducer(*beg);
        beg = _consumersProducerSet.begin();
    }

    //2. clear out all the remaining entries in the queue
    clearDataToConsume();
}

static bool eraseProducer(UFJoinableProducer* ufp, std::deque<UFJoinableProducer*>& coll)
{
    for(deque<UFJoinableProducer*>::iterator beg = coll.begin();
        beg != coll.end();
        ++beg)
    {
        if(*beg == ufp)
        {
            coll.erase(beg);
            return true;
        }
    }
    return false;
}

bool UFJoinableConsumer::removeProducer(UFJoinableProducer* ufp)
{
    //notifying producer on exit
    if(!ufp || !ufp->removeConsumer(this))
        return false;

    if(_requireLockToWaitForUpdate)
    {
        _consumersProducerSetLock.getSpinLock();
        //_consumersProducerSet.erase(ufp);
        eraseProducer(ufp, _consumersProducerSet);
        _consumersProducerSetLock.releaseSpinLock();
    }
    else
        eraseProducer(ufp, _consumersProducerSet);
        //_consumersProducerSet.erase(ufp);

    return true;
}

void UFProducer::reset()
{
    if(!getConsumerCount()) //dont do anything if there are no consumers remaining
        return;

    //add the EOF indicator
    if(_sendEOFAtEnd)
        produceData(0, 0, 0/*exit*/, 0/*freeDataOnExit*/); //notify the consumers that the producer is bailing

    //have to wait for all the consumers to acknowledge my death
    removeAllConsumers();
}

bool UFJoinableProducer::addConsumer(UFConsumer* ufc)
{
    UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.lock(uf);
    if(!_acceptNewConsumers)
    {
        if(_requireLockToUpdateConsumers) _producersConsumerSetLock.unlock(uf);
        return false;
    }
    _producersConsumerSet.push_back(ufc); //check insertion
    _mostRecentConsumerAdded = ufc;
    _mostRecentConsumerAdded->_currUF = uf;
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.unlock(uf);
    return true;
}

void UFJoinableProducer::removeAllConsumers()
{
    UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.lock(uf);
    _acceptNewConsumers = false;
    while(!_producersConsumerSet.empty())
    {
        if(_requireLockToUpdateConsumers) _producersConsumerSetLock.condTimedWait(uf, 1000000);
        else 
        {
            _uf = uf;
            _uf->block();
        }
    }
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.unlock(uf);
}

bool UFJoinableProducer::removeConsumer(UFConsumer* ufc)
{
    UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.lock(uf);
    std::deque<UFConsumer*>::iterator beg = _producersConsumerSet.begin();
    for(;beg != _producersConsumerSet.end(); ++beg) //TODO: optimize later - use set if there are too many consumers
    {
        if(*beg == ufc)
        {
            _producersConsumerSet.erase(beg);
            break;
        }
    }
    //_producersConsumerSet.erase(ufc);
    if(_requireLockToUpdateConsumers)  //notify the uf listening for this producer that another consumer bailed
        _producersConsumerSetLock.signal();
    else
    {
        if(_uf)
            UFScheduler::getUFScheduler()->addFiberToScheduler(_uf);
    }
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.unlock(uf);
    return true;
}

bool UFNonJoinableProducer::addConsumer(UFConsumer* ufc)
{
    if(!ufc || _mostRecentConsumerAdded)
        return false;

    if(!_requireLockToUpdateConsumers)
    {
        _mostRecentConsumerAdded = ufc;
        return true;
    }

    UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    _producersConsumerSetLock.lock(uf);
    _mostRecentConsumerAdded = ufc;
    _producersConsumerSetLock.unlock(uf);
    return true;
}

bool UFNonJoinableProducer::removeConsumer(UFConsumer* ufc)
{
    UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.lock(uf);
    _acceptNewConsumers = false; //can only remove consumer on exit
    if(!_mostRecentConsumerAdded)
    {
        if(_requireLockToUpdateConsumers) _producersConsumerSetLock.unlock(uf);
        return true;
    }

    _mostRecentConsumerAdded = 0;
    if(_requireLockToUpdateConsumers)  //notify the uf listening for this producer that another consumer bailed - needed if the producer is waiting for the consumers to die before being able to die on its own
        _producersConsumerSetLock.signal();
    else
    {
        if(_uf)
            UFScheduler::getUFScheduler()->addFiberToScheduler(_uf);
    }
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.unlock(uf);

    return true;
}

void UFNonJoinableProducer::removeAllConsumers()
{
    UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.lock(uf);
    _acceptNewConsumers = false; //can only remove consumer on exit
    while(_mostRecentConsumerAdded)
    {
        if(_requireLockToUpdateConsumers) _producersConsumerSetLock.condTimedWait(uf, 1000000);
        else 
        {
            _uf = uf;
            _uf->block();
        }
    }
    if(_requireLockToUpdateConsumers) _producersConsumerSetLock.unlock(uf);
}

stack<UFProducerData*> UFProducerData::_objList;
UFMutex UFProducerData::_objListMutex;

