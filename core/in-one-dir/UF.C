#include "UF.H"

#include <string.h>
#include <iostream>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include "UFSwapContext.H"

using namespace std;

#if __WORDSIZE == 64
static void runFiber(unsigned int lo, unsigned int hi)
#else
static void runFiber(void* args)
#endif
{
#if __WORDSIZE == 64
    UF* uf = (UF*)((((unsigned long)hi)<<32)+(unsigned long)lo);
#else
    if(!args)
        return;
    UF* uf = (UF*)args;
#endif
    uf->run();
    uf->_status = COMPLETED;
}

///////////////UF/////////////////////
UFFactory* UFFactory::_instance = 0;
unsigned int UF::DEFAULT_STACK_SIZE = 4*4096;
UFId UF::_globalId = 0;

UF::UF()
{ 
    reset();
    _myId = ++_globalId;  //TODO: make atomic
    setup();
}

static int getPageSize()
{
    int pageSize = sysconf(_SC_PAGE_SIZE);
    if(pageSize == -1)
    {
        cerr<<"couldnt get sysconf for pageSize "<<strerror(errno)<<endl;
        abort();
    }
    return pageSize;
}

static size_t pageSize = getPageSize();
bool UF::USE_MEMALIGN = false;
UF::~UF()
{
    if(_UFObjectCreatedStack && _ptrToStack)
    {
        if(UF::USE_MEMALIGN) //unprotect the data
        {
            if (mprotect((char*)_ptrToStack, pageSize, PROT_READ|PROT_WRITE|PROT_EXEC) == -1) //the front-end portion of the stack
            {
                cerr<<"couldnt mprotect location "<<strerror(errno)<<endl;
                abort();
            }
            if (mprotect((char*)_ptrToStack+pageSize+_UFContext.uc_stack.ss_size, pageSize, PROT_READ|PROT_WRITE|PROT_EXEC) == -1) //the back-end portion of the stack
            {
                cerr<<"couldnt mprotect location "<<strerror(errno)<<endl;
                abort();
            }
        }
        free(_ptrToStack);
    }
}

bool UF::setup(void* stackPtr, size_t stackSize)
{
    if(!stackPtr || !stackSize)
    {
        if(!UF::USE_MEMALIGN)
        {
            _UFContext.uc_stack.ss_size = (stackSize) ? stackSize : UF::DEFAULT_STACK_SIZE;
            _UFContext.uc_stack.ss_sp = _ptrToStack = (void*) malloc (_UFContext.uc_stack.ss_size);
        }
        else
        {
            size_t sizeToAllocate = (int)(UF::DEFAULT_STACK_SIZE/pageSize);
            if(!sizeToAllocate)
            {
                cerr<<"sizeToAllocate has to be larger than a pageSize which is "<<pageSize<<endl;
                abort();
            }
            sizeToAllocate *= pageSize;
            _UFContext.uc_stack.ss_size = sizeToAllocate;
            sizeToAllocate += 2*pageSize; //add the front and back boundaries to the stack
            if((errno = posix_memalign(&_ptrToStack, pageSize, sizeToAllocate)))
            {
                cerr<<"couldnt allocate space using posix_memalign "<<strerror(errno)<<endl;
                abort();
            }
            _UFContext.uc_stack.ss_sp = (char*)_ptrToStack + pageSize;
            if (mprotect((char*)_ptrToStack, pageSize, PROT_NONE) == -1) //the front-end portion of the stack
            {
                cerr<<"couldnt mprotect location "<<strerror(errno)<<endl;
                abort();
            }
            if (mprotect((char*)_ptrToStack+pageSize+_UFContext.uc_stack.ss_size, pageSize, PROT_NONE) == -1) //the back-end portion of the stack
            {
                cerr<<"couldnt mprotect location "<<strerror(errno)<<endl;
                abort();
            }
        }
        _UFObjectCreatedStack = true;
    }
    else
    {
        _UFContext.uc_stack.ss_size = stackSize;
        _UFContext.uc_stack.ss_sp = _ptrToStack = stackPtr;
        _UFObjectCreatedStack = false;
    }
    _UFContext.uc_stack.ss_flags = 0;

    return true;
}






///////////////UFScheduler/////////////////////
ThreadUFSchedulerMap UFScheduler::_threadUFSchedulerMap;
pthread_mutex_t UFScheduler::_mutexToCheckFiberSchedulerMap = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t getThreadKey()
{
    if(pthread_key_create(&UFScheduler::_specific_key, 0) != 0)
    {
        cerr<<"couldnt create thread specific key "<<strerror(errno)<<endl;
        exit(1);
    }
    return UFScheduler::_specific_key;
}
pthread_key_t UFScheduler::_specific_key = getThreadKey();
UFScheduler::UFScheduler()
{
    _bailWhenNoActiveUFs = false;
    _earliestWakeUpFromSleep = 0;
    _exitJustMe = false;
    _specific = 0;
    _currentFiber = 0;

    if(_inThreadedMode)
    {
        pthread_mutex_init(&_mutexToNominateToActiveList, NULL);
        pthread_cond_init(&_condToNominateToActiveList, NULL);
    }


    //check that there are no other schedulers already running in this thread
    if(_inThreadedMode)
    {
        pthread_t currThreadId = pthread_self();

        pthread_mutex_lock(&_mutexToCheckFiberSchedulerMap);
        if(_threadUFSchedulerMap.find(currThreadId) != _threadUFSchedulerMap.end())
        {
            cerr<<"cannot have more than one scheduler per thread"<<endl;
            exit(1);
        }
        _threadUFSchedulerMap[currThreadId] = this;
        pthread_mutex_unlock(&_mutexToCheckFiberSchedulerMap);
    }
    else
    {
        if(_threadUFSchedulerMap.find(0) != _threadUFSchedulerMap.end())
        {
            cerr<<"cannot have more than one scheduler per thread"<<endl;
            exit(1);
        }

        //for non-threaded mode we consider the pthread_t id to be 0
        _threadUFSchedulerMap[0] = this;
    }

    _tid = (_inThreadedMode) ? pthread_self() : 0;
    _notifyFunc = 0;
    _notifyArgs = 0;

    pthread_setspecific(_specific_key, this);
    _amtToSleep = 0;
    _runCounter = 1;
}

UFScheduler::~UFScheduler() 
{ 
    //remove the UFScheduler associated w/ this thread
    pthread_mutex_lock(&_mutexToCheckFiberSchedulerMap);
    ThreadUFSchedulerMap::iterator index = _threadUFSchedulerMap.find(pthread_self());
    if(index != _threadUFSchedulerMap.end())
        _threadUFSchedulerMap.erase(index);
    pthread_mutex_unlock(&_mutexToCheckFiberSchedulerMap);

    /*pthread_key_delete(_specific_key);*/ 
}

bool UFScheduler::addFiberToSelf(UF* uf)
{
    if(!uf)
        return false;
    if(uf->_status == WAITING_TO_RUN || 
       uf->_status == YIELDED) //UF is already in the queue
        return true;
    uf->_status = WAITING_TO_RUN;
    if(uf->getParentScheduler()) //probably putting back an existing uf into the active list
    {
        if(uf->getParentScheduler() == this) //check that we're scheduling for the same thread
        {
            _activeRunningList.push_front(uf);
            return true;
        }
        else
        {
            cerr<<uf<<" uf is not part of scheduler, "<<this<<" its part of "<<uf->getParentScheduler()<<endl;
            abort(); //TODO: remove the abort
            return false;
        }
    }

    //create a new context
    uf->_parentScheduler = this;
    uf->_UFContext.uc_link = &_mainContext;

    getcontext(&(uf->_UFContext));
    errno = 0;

#if __WORDSIZE == 64
    makecontext(&(uf->_UFContext), (void (*)(void)) runFiber, 2, (int)(ptrdiff_t)uf, (int)((ptrdiff_t)uf>>32));
#else
    makecontext(&(uf->_UFContext), (void (*)(void)) runFiber, 1, (void*)uf);
#endif
    if(errno != 0)
    {
        cerr<<"error while trying to run makecontext"<<endl;
        return false;
    }
    _activeRunningList.push_front(uf);
    return true;
}

bool UFScheduler::addFiberToAnotherThread(const list<UF*>& ufList, pthread_t tid)
{
    if(ufList.empty())
        return false;

    //find the other thread -- 
    //TODO: have to lock before looking at this map - 
    //since it could be changed if more threads are added later - not possible in the test that is being run (since the threads are created before hand)
    UF* uf = 0;
    list<UF*>::const_iterator beg = ufList.begin();
    list<UF*>::const_iterator ending = ufList.end();
    ThreadUFSchedulerMap::iterator index = _threadUFSchedulerMap.find(tid);
    if(index == _threadUFSchedulerMap.end())
    {
        cerr<<"couldnt find the scheduler associated with "<<tid<<" for uf = "<<*beg<<endl;
        ThreadUFSchedulerMap::iterator beg = _threadUFSchedulerMap.begin();
        return false;
    }

    UFScheduler* ufs = index->second;
    pthread_mutex_lock(&(ufs->_mutexToNominateToActiveList));
    for(; beg != ending; ++beg)
    {
        uf = *beg;
        ufs->_nominateToAddToActiveRunningList.push_back(uf);
    }
    pthread_cond_signal(&(ufs->_condToNominateToActiveList));
    pthread_mutex_unlock(&(ufs->_mutexToNominateToActiveList));
    ufs->notifyUF();
    return true;
}

bool UFScheduler::addFiberToScheduler(UF* uf, pthread_t tid)
{
    if(!uf)
    {
        cerr<<"null uf provided to scheduler"<<endl;
        return false;
    }

    //adding to the same scheduler and as a result thread as the current job
    if(!tid || (tid == pthread_self()))
        return addFiberToSelf(uf);
    else //adding to some other threads' scheduler
    {
        list<UF*> l;
        l.push_back(uf);
        return addFiberToAnotherThread(l, tid);
    }
}

bool UFScheduler::addFiberToScheduler(const list<UF*>& ufList, pthread_t tid)
{
    if(ufList.empty())
        return true;

    //adding to the same scheduler and as a result thread as the current job
    if(!tid || (tid == pthread_self()))
    {
        list<UF*>::const_iterator beg = ufList.begin();
        list<UF*>::const_iterator ending = ufList.end();
        for(; beg != ending; ++beg)
        {
            if(addFiberToSelf(*beg))
                continue;
            else
                return false;
        }
    }
    else //adding to some other threads' scheduler
        return addFiberToAnotherThread(ufList, tid);
    return true;
}

void UFScheduler::notifyUF()
{
    if(_notifyFunc)
        _notifyFunc(_notifyArgs);
}


bool UFScheduler::_exit = false;
const unsigned int DEFAULT_SLEEP_IN_USEC = 1000000;
void UFScheduler::runScheduler()
{
    errno = 0;

    _amtToSleep = DEFAULT_SLEEP_IN_USEC;
    bool ranGetTimeOfDay = false;

    struct timeval now;
    struct timeval start,finish;
    gettimeofday(&start, 0);
    TIME_IN_US timeNow = 0;

    MapTimeUF::iterator slBeg;
    bool waiting = false;
    while(!shouldExit())
    {
        if(_bailWhenNoActiveUFs && _activeRunningList.empty())
            break;

        ++_runCounter;
        while(!_activeRunningList.empty())
        {
            if(shouldExit())
                break;

            UF* uf = _activeRunningList.front();
            if(uf->_status == YIELDED &&
               uf->_lastRun == _runCounter) //we have looped back
                break;
            //printf("%lu - running uf %lu on iter %llu\n", pthread_self(), (uintptr_t)uf, _runCounter);
            _activeRunningList.pop_front();
            uf->_lastRun = _runCounter;
            uf->_status = RUNNING;
            _currentFiber = uf;
#if __WORDSIZE == 64
            uf_swapcontext(&_mainContext, &(uf->_UFContext));
#else
            swapcontext(&_mainContext, &(uf->_UFContext));
#endif
            _currentFiber = 0;

            if(uf->_status == BLOCKED)
                continue;
            else if(uf->_status == COMPLETED) 
            {
                if(uf->_myFactory)
                    uf->_myFactory->releaseUF(uf);
                else
                    delete uf;
                continue;
            }
            uf->_status = YIELDED;
            _activeRunningList.push_back(uf);
        }


        //check the sleep queue
        ranGetTimeOfDay = false;
        _amtToSleep = DEFAULT_SLEEP_IN_USEC;

        //check if some other thread has nominated some user fiber to be
        //added to this thread's list -
        //can happen in the foll. situations
        //1. the main thread is adding a new user fiber
        //2. some fiber has requested to move to another thread
        if(!_nominateToAddToActiveRunningList.empty() /*TODO: take this out later w/ the atomic size count*/ &&
           _inThreadedMode)

        {
            _amtToSleep = 0; //since we're adding new ufs to the list we dont need to sleep
            //TODO: do atomic comparison to see if there is anything in 
            //_nominateToAddToActiveRunningList before getting the lock
            pthread_mutex_lock(&_mutexToNominateToActiveList);
            do
            {
                UF* uf = _nominateToAddToActiveRunningList.front();
                if(uf->getParentScheduler())
                {
                    if(uf->_status != WAITING_TO_RUN && uf->_status != YIELDED)
                    {
                        uf->_status = WAITING_TO_RUN;
                        _activeRunningList.push_front(uf);
                    }
                }
                else //adding a new fiber
                    addFiberToScheduler(uf, 0);
                _nominateToAddToActiveRunningList.pop_front();
            }while(!_nominateToAddToActiveRunningList.empty());
            pthread_mutex_unlock(&_mutexToNominateToActiveList);
        }


        //pick up the fibers that may have completed sleeping
        //look into the sleep list;
        //printf("%u %u tnc = %llu %llu\n", (unsigned int)pthread_self(), _sleepList.size(), _earliestWakeUpFromSleep, _earliestWakeUpFromSleep-timeNow);
        if(!_sleepList.empty())
        {
            gettimeofday(&now, 0);
            ranGetTimeOfDay = true;
            timeNow = timeInUS(now);
            if(timeNow >= _earliestWakeUpFromSleep) //dont go into this queue unless the time seen the last time has passed
            {
                for(slBeg = _sleepList.begin(); slBeg != _sleepList.end(); )
                {
                    //1. see if anyone has crossed the sleep timer - add them to the active list
                    if(slBeg->first <= timeNow) //sleep time is over
                    {
                        UFWaitInfo *ufwi = slBeg->second;
                        ufwi->_ctrl.getSpinLock();
                        ufwi->_sleeping = false;
                        if(ufwi->_uf)
                        {
                            if(ufwi->_uf->_status != WAITING_TO_RUN && ufwi->_uf->_status != YIELDED)
                            {
                                ufwi->_uf->_status = WAITING_TO_RUN;
                                _activeRunningList.push_front(ufwi->_uf);
                                ufwi->_uf = NULL;
                            }
                        }
                        waiting = ufwi->_waiting;
                        ufwi->_ctrl.releaseSpinLock();
                        if(!waiting) //since the uf is not being waited upon release it (the sleeping part has already been done)
                            releaseWaitInfo(*ufwi);
                        
                        _sleepList.erase(slBeg);
                        slBeg = _sleepList.begin();
                        continue;
                    }
                    else
                    {
                        if(_amtToSleep) //since the nominate system might have turned off the sleep - we dont activate it again
                            _amtToSleep = slBeg->first-timeNow; 
                        _earliestWakeUpFromSleep = slBeg->first;
                        break;
                    }
                    ++slBeg;
                }
            }
        }

        //see if there is anything to do or is it just sleeping time now
        if(!_notifyFunc && _activeRunningList.empty() && !shouldExit())
        {
            if(_inThreadedMode) //go to conditional wait (in threaded mode)
            {
                struct timespec ts;
                int nSecToIncrement = (int)(_amtToSleep/1000000);
                TIME_IN_US nUSecToIncrement = (TIME_IN_US)(_amtToSleep%1000000);
                if(!ranGetTimeOfDay)
                    gettimeofday(&now, 0);
                ts.tv_sec = now.tv_sec + nSecToIncrement;
                ts.tv_nsec = (now.tv_usec + nUSecToIncrement)*1000; //put in nsec

                pthread_mutex_lock(&_mutexToNominateToActiveList);
                if(_nominateToAddToActiveRunningList.empty())
                    pthread_cond_timedwait(&_condToNominateToActiveList, &_mutexToNominateToActiveList, &ts);
                pthread_mutex_unlock(&_mutexToNominateToActiveList);
            }
            else //sleep in non-threaded mode
                usleep(_amtToSleep);
        }
    }
    gettimeofday(&finish, 0);

    TIME_IN_US diff = (finish.tv_sec-start.tv_sec)*1000000 + (finish.tv_usec - start.tv_usec);
    cerr<<pthread_self()<<" time taken in this thread = "<<diff<<"us"<<endl;
}


bool UFScheduler::_inThreadedMode = true;
UFScheduler* UFScheduler::getUFScheduler(pthread_t tid)
{
    if(!tid || tid == pthread_self())
        return (UFScheduler*)pthread_getspecific(_specific_key);

    pthread_mutex_lock(&_mutexToCheckFiberSchedulerMap);
    ThreadUFSchedulerMap::const_iterator index = _threadUFSchedulerMap.find(tid);
    if(index == _threadUFSchedulerMap.end())
    {
        pthread_mutex_unlock(&_mutexToCheckFiberSchedulerMap);
        return 0;
    }
    pthread_mutex_unlock(&_mutexToCheckFiberSchedulerMap);

    return const_cast<UFScheduler*>(index->second);
}

UF* UFScheduler::getUF(pthread_t tid)
{
    return const_cast<UF*>(getUFScheduler(tid)->getRunningFiberOnThisThread());
}

UFFactory::UFFactory()
{
    _size = 0;
    _capacity = 0;
    _objMapping = 0;
}

int UFFactory::registerFunc(UF* uf)
{
    //not making this code thread safe - since this should only happen at init time
    if(_size == _capacity)
    {
        _capacity  = _capacity ? _capacity : 5 /*start w/ 5 slots*/;
        _capacity *= 2; //double each time
        UF** tmpObjMapping = (UF**) malloc (sizeof(UF*)*_capacity);

        for(unsigned int i = 0; i < _size; ++i)
            tmpObjMapping[i] = _objMapping[i];
        if(_objMapping)
            free(_objMapping);

        _objMapping = tmpObjMapping;
    }

    _objMapping[_size] = uf;
    return _size++;
}

static const UF* READ_LOCK_UF = (UF*)0x1;
static const unsigned short int MAX_LOCK_FETCH_TRY = 50;
bool UFMutex::lock(UF* uf, bool readOnlyLock)
{
    if(!uf)
        return false;

    unsigned short int numLockRetryAttempts = 0;
    do
    {
        getSpinLock();
        if(readOnlyLock)
        {
            if(_ufCurrentlyOwningLock == READ_LOCK_UF)
            {
                ++_currentReadLockCount;
                releaseSpinLock();
                return true;
            }
            else if(_ufCurrentlyOwningLock != 0) //some other uf owns the lock - cant get a read lock - should revert to a regular lock
                readOnlyLock = false;
        }
        if(!_ufCurrentlyOwningLock || (_ufCurrentlyOwningLock == uf)) //no one owns the lock right now or its this uf itself
        {
            if(_beFair && _ufThatShouldGetLock == uf) //this uf should be the one getting the lock
                _ufThatShouldGetLock = 0;

            if(!_beFair || _ufThatShouldGetLock == 0) //the lock hasnt been assigned to some uf
            {
                if(readOnlyLock) _currentReadLockCount = 1;
                _ufCurrentlyOwningLock = (!readOnlyLock) ? uf : const_cast<UF*>(READ_LOCK_UF);
                releaseSpinLock();
                return true;
            }
        }

        if(numLockRetryAttempts)
        {
            _listOfClientsWaitingOnLock.push_front(uf); //put this uf into the top of the list - to be fair
            if(_beFair && numLockRetryAttempts > MAX_LOCK_FETCH_TRY && !_ufThatShouldGetLock)
            {
                _ufThatShouldGetLock = uf;
                _ufThatShouldGetLockHasGotLock = false;
            }
        }
        else //inserting into the list the first time
            _listOfClientsWaitingOnLock.push_back(uf);

        releaseSpinLock();
        uf->waitOnLock();
        ++numLockRetryAttempts;
    }while(1);

    return true;
}

bool UFMutex::unlock(UF* uf)
{
    if(!uf)
        return false;

    getSpinLock();
    if(_ufCurrentlyOwningLock == READ_LOCK_UF)
    {
        if(!_currentReadLockCount) //the count should be atleast 1
        {
            releaseSpinLock();
            cerr<<"the count on unlocking should be atleast 1"<<endl;
            abort();
            return false;
        }
        if(--_currentReadLockCount > 0) //there are more ufs in the read lock state
        {
            releaseSpinLock();
            return true;
        }
    }
    else if(_ufCurrentlyOwningLock != uf)
    {
        abort();
        releaseSpinLock();
        return false;
    }
    _ufCurrentlyOwningLock = 0;

    if(!_beFair || !_ufThatShouldGetLock) //no uf is forced to get the lock now
    {
        //pick the first uf from the list and give it the lock
        while(!_listOfClientsWaitingOnLock.empty())
        {
            UF* tmpUf = _listOfClientsWaitingOnLock.front();
            _listOfClientsWaitingOnLock.pop_front();
            if(tmpUf)
            {
                uf->getParentScheduler()->addFiberToScheduler(tmpUf, tmpUf->getParentScheduler()->_tid);
                break;
            }
        }
    }
    else
    {
        if(!_ufThatShouldGetLockHasGotLock) //the lock hasnt been given to the required uf yet
        {
            uf->getParentScheduler()->addFiberToScheduler(_ufThatShouldGetLock, _ufThatShouldGetLock->getParentScheduler()->_tid);
            _ufThatShouldGetLockHasGotLock = true;
        }
    }

    releaseSpinLock();
    return true;
}


bool UFMutex::tryLock(UF* uf, TIME_IN_US autoRetryIntervalInUS)
{
    while(1)
    {
        getSpinLock();
        if(!_ufCurrentlyOwningLock) //no one owns the lock right now
        {
            _ufCurrentlyOwningLock = uf;
            releaseSpinLock();
            return true;
        }

        releaseSpinLock();

        if(!autoRetryIntervalInUS)
            break;

        usleep(autoRetryIntervalInUS);
    }

    return false;
}


bool UFMutex::condWait(UF* uf)
{
    if(!uf)
        return false;
    
    //the object is already in the hash
    if(_listOfClientsWaitingOnCond.find(uf) == _listOfClientsWaitingOnCond.end())
    {
        UFWaitInfo *ufwi = uf->getParentScheduler()->getWaitInfo();
        ufwi->_uf = uf;
        ufwi->_waiting = true;
    
        _listOfClientsWaitingOnCond[uf] = ufwi;
    }

    unlock(uf);
    uf->waitOnLock(); //this fxn will cause the fxn to wait till a signal or broadcast has occurred
    lock(uf);

    return true;
}

void UFMutex::broadcast()
{
    if(_listOfClientsWaitingOnCond.empty())
        return;

    UFScheduler* ufs = UFScheduler::getUFScheduler();
    if(!ufs)
    {
        cerr<<"couldnt get scheduler on thread "<<pthread_self()<<endl;
        return;
    }

    //notify all the UFs waiting to wake up
    bool sleeping = false;
    for(UFWLHash::iterator beg = _listOfClientsWaitingOnCond.begin();
        beg != _listOfClientsWaitingOnCond.end(); ++beg)
    {
        // Get WaitInfo object
        UFWaitInfo *ufwi = beg->second;

        ufwi->_ctrl.getSpinLock();
        ufwi->_waiting = false; // Set _waiting to false, indicating that the UFWI has been removed from the cond queue

        // If uf is not NULL, schedule it and make sure no one else can schedule it again
        if(ufwi->_uf) 
        {
            ufs->addFiberToScheduler(ufwi->_uf, ufwi->_uf->getParentScheduler()->_tid);
            ufwi->_uf = NULL;
        }
        
        sleeping = ufwi->_sleeping;
        ufwi->_ctrl.releaseSpinLock();
        if(!sleeping) //sleep list has already run
            ufs->releaseWaitInfo(*ufwi);
    }
    _listOfClientsWaitingOnCond.clear();
}

void UFMutex::signal()
{
    if(_listOfClientsWaitingOnCond.empty())
        return;

    UFScheduler* ufs = UFScheduler::getUFScheduler();
    if(!ufs)
    {
        cerr<<"couldnt get scheduler"<<endl;
        return;
    }
    UF *uf_to_signal = NULL;
    bool sleeping = false;
    for(UFWLHash::iterator beg = _listOfClientsWaitingOnCond.begin(); beg != _listOfClientsWaitingOnCond.end();)
    {
        // Take first client off list
        UFWaitInfo *ufwi = beg->second;

        ufwi->_ctrl.getSpinLock();
        ufwi->_waiting = false; // Set _waiting to false, indicating that the UFWI has been removed from the cond queue
        
        if(ufwi->_uf)
        {
            uf_to_signal = ufwi->_uf; // Store UF to signal
            ufwi->_uf = NULL; // Clear UF. This ensures that no one else can schedule the UF.
        }

        sleeping = ufwi->_sleeping;
        ufwi->_ctrl.releaseSpinLock();
        if(!sleeping) //sleep list has already run
            ufs->releaseWaitInfo(*ufwi);

        // If a UF was found to signal, break out
        _listOfClientsWaitingOnCond.erase(beg);
        if(uf_to_signal)
            break;
        beg = _listOfClientsWaitingOnCond.begin();
    }

    if(uf_to_signal)
        ufs->addFiberToScheduler(uf_to_signal, uf_to_signal->getParentScheduler()->_tid);
}

bool UFMutex::condTimedWait(UF* uf, TIME_IN_US sleepAmtInUs)
{
    bool result = false;
    if(!uf)
        return result;

    // Wrap uf in UFWait structure before pushing to wait and sleep queues
    UFWaitInfo *ufwi = UFScheduler::getUFScheduler()->getWaitInfo();
    ufwi->_uf = uf;
    ufwi->_waiting = true;
    ufwi->_sleeping = true;
    
    // Add to waiting queue
    _listOfClientsWaitingOnCond[uf] = ufwi;
    unlock(uf);
    
    // Add to sleep queue
    struct timeval now;
    gettimeofday(&now, 0);
    TIME_IN_US timeNow = timeInUS(now);
    ufwi->_sleeping = true;
    unsigned long long int totalTime = timeNow+sleepAmtInUs;
    if( (unsigned long long int) uf->getParentScheduler()->_earliestWakeUpFromSleep > totalTime || uf->getParentScheduler()->_earliestWakeUpFromSleep)
        uf->getParentScheduler()->_earliestWakeUpFromSleep = totalTime;
    uf->getParentScheduler()->_sleepList.insert(std::make_pair((timeNow+sleepAmtInUs), ufwi));

    uf->waitOnLock(); //this fxn will cause the fxn to wait till a signal, broadcast or timeout has occurred
    ufwi->_ctrl.getSpinLock();
    result = ufwi->_sleeping;
    ufwi->_ctrl.releaseSpinLock();

    lock(uf);
    return (result) ? true : false;//if result (ufwi->_sleeping) is not true, it must be that the sleep list activated this uf
}

void* setupThread(void* args)
{
    if(!args)
        return 0;

    UFList* ufsToStartWith = (UFList*) args;
    UFScheduler ufs;
    ufs.addFiberToScheduler(*ufsToStartWith, 0);
    delete ufsToStartWith;
    
    //run the scheduler
    ufs.runScheduler();

    return 0;
}

void UFScheduler::ufCreateThread(pthread_t* tid, list<UF*>* ufsToStartWith)
{
    //create the threads
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    if(pthread_create(tid, &attr, setupThread, (void*)ufsToStartWith) != 0)
    {
        cerr<<"couldnt create thread "<<strerror(errno)<<endl;
        exit(1);
    }
}

string getPrintableTime()
{
    char asctimeDate[32];
    asctimeDate[0] = '\0';
    time_t now = time(0);
    asctime_r(localtime(&now), asctimeDate);

    string response = asctimeDate;
    size_t loc = response.find('\n');
    if(loc != string::npos)
        response.replace(loc, 1, "");
    return response;
}
