#include <errno.h>
#include <iostream>
#include <string.h>
#include "UF.H"
#include "UFIO.H"
#include "UFConnectionPool.H"

using namespace std;
struct TestConnPoolFetchUF : public UF
{
    void run();
    UF* createUF() { return new TestConnPoolFetchUF(); }
};

string hostToGet = "127.0.0.1:80";
unsigned int NUM_TO_GET = 10;
void TestConnPoolFetchUF::run()
{
    UFConnectionPool* cpool = UFIOScheduler::getUFIOS()->getConnPool();
    unsigned int counter = 0;
    while(counter++ < NUM_TO_GET)
    {
        cerr<<"getting connection"<<endl;
        UFIO* ufio = cpool->getConnection(hostToGet);
        if(!ufio)
        {
            cerr<<"couldnt get connection "<<strerror(errno)<<endl;
            break;
        }
        cerr<<"releasing connection to pool"<<endl;
        cpool->releaseConnection(ufio, true);
    }

    UFScheduler::getUFScheduler()->setExitJustMe(true);
}

int main(int argc, char** argv)
{
    unsigned int numThreadsToCreate = 1;
    unsigned int numUFsToCreate = 1;
    if(argc > 1)
        hostToGet = argv[1];
    if(argc > 2)
        numThreadsToCreate = atoi(argv[2]);
    else if(argc > 3)
        numUFsToCreate = atoi(argv[3]);

    //create the threads
    pthread_t* thread = new pthread_t[numThreadsToCreate];
    unsigned int i = 0;
    for(; i<numThreadsToCreate; i++)
    {
        list<UF*>* ufList = new list<UF*>();
        for(unsigned int j = 0; j < numUFsToCreate; ++j)
            ufList->push_back(new TestConnPoolFetchUF());
        UFIO::ufCreateThreadWithIO(&thread[i], ufList);
    }

    //wait for kids
    void* status;
    for(i=0; i<numThreadsToCreate; i++)
        pthread_join(thread[i], &status);
    delete [] thread;

    return 0;
}
