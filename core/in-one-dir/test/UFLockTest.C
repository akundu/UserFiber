#include "UF.H"
#include "UFPC.H"
#include <iostream>
#include <stdio.h>
#include <string>
#include <map>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <list>

using namespace std;

size_t numConsumers = 0;

UFMutex mconsIncremented;
size_t consIncremented = 0;
struct Consumer : public UF
{
    UF *createUF() { return new Consumer(); }

    void run()
    {
        bool bail = false;
        mconsIncremented.lock(this);
        if(++consIncremented == numConsumers)
            bail = true;
        mconsIncremented.unlock(this);

        if(bail)
            UFScheduler::getUFScheduler()->setExit(true);
    }
};

int main(int argc, char** argv)
{
    size_t consumersPerThread = 1;
    size_t numThreads = 1;
    char optCh;
    while ((optCh = getopt(argc, argv, "n:c:")) != -1)
    {
        if (optCh == 'n')
            numThreads = atoi(optarg);
        else if (optCh == 'c')
            consumersPerThread = atoi(optarg);
    }
    cerr<<"consumersPerThread = "<<consumersPerThread<<endl;
    cerr<<"numThreads = "<<numThreads<<endl;
    numConsumers = consumersPerThread * numThreads;



    //start the consumer
    pthread_t* consumerThreads = new pthread_t[numThreads];
    for (size_t i = 0; i < numThreads; ++i)
    {
        list<UF *> *consumerUFs = new list<UF *>;
        for(size_t j=0; j<consumersPerThread; ++j)
            consumerUFs->push_back(new Consumer);
        UFScheduler::ufCreateThread(&consumerThreads[i], consumerUFs);
    }

    for (size_t i = 0; i < numThreads; ++i)
        pthread_join(consumerThreads[i], 0);

    return 0;
}
