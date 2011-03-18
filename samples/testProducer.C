#include <iostream>
#include <stdlib.h>
#include "UFPC.H"
#include <stdio.h>

UFProducer* p = 0;
struct TestProducer : public UF
{
    void run()
    {
        p = new UFProducer();
        unsigned int numAdded = 0;
        UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
        for(;;)
        {
            int* i = (int*) malloc (sizeof (int));
            *i = numAdded++;
            p->produceData((void*)i, sizeof(numAdded), true, uf);
            printf("%lu added\n", (unsigned long int) ((uintptr_t)(void*)uf));
            uf->usleep(1000);
        }
        delete p;
        UFScheduler::getUFScheduler()->setExitJustMe();
    }
    UF* createUF() { return new TestProducer(); }
};

struct TestConsumer : public UF
{
    void run()
    {
        UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
        while(!p)
            uf->usleep(1000);

        UFConsumer* c = new UFConsumer();
        if(!c || !c->joinProducer(p))
        {
            cerr<<"couldnt setup consumer"<<endl;
            if (c) delete c;
            return;
        }
        for(;;)
        {
            UFProducerData* result = c->waitForData(uf);
            if(!result)
            {
                uf->usleep(1000);
                continue;
            }
            int code = result->_ufpcCode;
            UFProducerData::releaseObj(result);
            if(code == 0 /* INDICATES AN END */)
                break;
            printf("%lu cons\n", (unsigned long int) ((uintptr_t)(void*)uf));
        }
        c->removeProducer(p);
        delete c;
        UFScheduler::getUFScheduler()->setExitJustMe();
    }
    UF* createUF() { return new TestConsumer(); }
};

unsigned int numConsumersToStart = 3;
unsigned int numThreadsToStart = 2;
int main(int argc, char** argv)
{
    if(argc > 1)
        numThreadsToStart = atoi(argv[1]);
    if(argc > 2)
        numConsumersToStart = atoi(argv[2]);

    pthread_t tc[numThreadsToStart+1];
    list<UF*>* ufList = new list<UF*>();
    ufList->push_back(new TestProducer());
    UFScheduler::ufCreateThread(&tc[0], ufList);


    unsigned int threadStarted = 1;
    while(threadStarted <= numThreadsToStart)
    {
        list<UF*>* ufList2 = new list<UF*>();
        for(unsigned int consumersStarted = 0; consumersStarted < numConsumersToStart; ++consumersStarted)
            ufList2->push_back(new TestConsumer());
        UFScheduler::ufCreateThread(&tc[threadStarted], ufList2);
        threadStarted++;
    }


    void* status;
    for(unsigned int runningThreads = 0; runningThreads < numThreadsToStart+1; ++runningThreads)
        pthread_join(tc[runningThreads], &status);

    return 0;
}
