#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include "UF.H"


unsigned int value = 0;
UFMutex* mut = 0;
struct TestProducer : public UF
{
    void run()
    {
        UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
        mut = new UFMutex; 
        while(1)
        {
            mut->lock(uf);
            value++;
            mut->broadcast();
            mut->unlock(uf);
            printf("%lu add\n", (unsigned long int) ((uintptr_t)(void*)uf));
            uf->usleep(10);
        }
    }
    UF* createUF() { return new TestProducer(); }
};

struct ConsumerWithSignal : public UF
{
    void run()
    {
        UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
        while(!mut)
            uf->usleep(1000);

        unsigned int lastValue = 0;
        unsigned int tmpValue = 0;
        while(1)
        {
            mut->lock(uf);
            while(lastValue == value)
                mut->condWait(uf);
            tmpValue = value;
            mut->unlock(uf);
            printf("%lu cons %u to %u\n", (unsigned long int) ((uintptr_t)(void*)uf), lastValue, tmpValue);
            lastValue = tmpValue;
        }
    }
    UF* createUF() { return new ConsumerWithSignal(); }
};

struct ConsumerWithLockOnly : public UF
{
    void run()
    {
        UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
        while(!mut)
            uf->usleep(1000);

        unsigned int lastValue = 0;
        unsigned int tmpValue = 0;
        while(1)
        {
            mut->lock(uf);
            while(lastValue == value)
            {
                mut->unlock(uf);
                uf->usleep(10);
                mut->lock(uf);
            }
            tmpValue = value;
            mut->unlock(uf);
            printf("%lu cons %u to %u\n", (unsigned long int) ((uintptr_t)(void*)uf), lastValue, tmpValue);
            lastValue = tmpValue;
        }
    }
    UF* createUF() { return new ConsumerWithLockOnly(); }
};

unsigned int numConsumersToStart = 3;
unsigned int numThreadsToStart = 2;
bool useLockOnly = false;
int main(int argc, char** argv)
{
    if(argc > 1)
        numThreadsToStart = atoi(argv[1]);
    if(argc > 2)
        numConsumersToStart = atoi(argv[2]);
    if(argc > 3)
        useLockOnly = atoi(argv[3]);

    cerr<<"using "<<(useLockOnly ? "lock" : "signals")<<" for notification"<<endl;

    pthread_t tc[numThreadsToStart+1];
    list<UF*>* ufList = new list<UF*>();
    ufList->push_back(new TestProducer());
    UFScheduler::ufCreateThread(&tc[0], ufList);


    unsigned int threadStarted = 0;
    while(++threadStarted <= numThreadsToStart)
    {
        list<UF*>* ufList2 = new list<UF*>();
        for(unsigned int consumersStarted = 0; consumersStarted < numConsumersToStart; ++consumersStarted)
        {
            if(useLockOnly)
                ufList2->push_back(new ConsumerWithLockOnly());
            else
                ufList2->push_back(new ConsumerWithSignal());
        }
        UFScheduler::ufCreateThread(&tc[threadStarted], ufList2);
    }


    void* status;
    for(unsigned int runningThreads = 0; runningThreads < numThreadsToStart+1; ++runningThreads)
        pthread_join(tc[runningThreads], &status);

    return 0;
}
