#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include "UF.H"


struct SleepUF : public UF
{
    void run()
    {
        UF* uf = UFScheduler::getUFScheduler()->getRunningFiberOnThisThread();
        while(1)
        {
            printf("%lu r\n", (unsigned long int) ((uintptr_t)(void*)uf));
            uf->usleep(100);
        }
    }
    UF* createUF() { return new SleepUF(); }
};

unsigned int numConsumersToStart = 3;
unsigned int numThreadsToStart = 2;
int main(int argc, char** argv)
{
    setbuf(stdout, 0);

    if(argc > 1)
        numThreadsToStart = atoi(argv[1]);
    if(argc > 2)
        numConsumersToStart = atoi(argv[2]);

    pthread_t tc[numThreadsToStart];
    unsigned int threadStarted = 0;
    while(threadStarted < numThreadsToStart)
    {
        list<UF*>* ufList2 = new list<UF*>();
        for(unsigned int consumersStarted = 0; consumersStarted < numConsumersToStart; ++consumersStarted)
            ufList2->push_back(new SleepUF());
        UFScheduler::ufCreateThread(&tc[threadStarted], ufList2);
        threadStarted++;
    }


    void* status;
    for(unsigned int runningThreads = 0; runningThreads < numThreadsToStart; ++runningThreads)
        pthread_join(tc[runningThreads], &status);

    return 0;
}
