#include "UF.H"
#include <sys/time.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

unsigned int amtToSleep = 1000000;
unsigned int numTimes = 10;
struct UFSleepTest : public UF
{
    UF* createUF() { return new UFSleepTest(); }

    void run()
    {
        timeval now;
        gettimeofday(&now,NULL);

        unsigned long long int timeNow = now.tv_sec*1000000+now.tv_usec;
        unsigned int counter = 0;

        while(counter++ < numTimes)
        {
            usleep(amtToSleep);
            timeNow += amtToSleep;
            cerr<<"current time = "<<timeNow<<endl;
        }

        _parentScheduler->setExitJustMe(true);
    }
};

int main(int argc, char** argv)
{
    if(argc > 1)
        numTimes = atoi(argv[1]);
    if(argc > 2)
        amtToSleep = atoi(argv[2]);

    pthread_t thread;
    list<UF*>* ufList = new list<UF*>();
    ufList->push_back(new UFSleepTest());
    UFScheduler::ufCreateThread(&thread, ufList);

    void* status;
    pthread_join(thread, &status);

    return 0;
}
