#include "UF.H"
#include <sys/time.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

int numThreads = 5;
int numFibers = 100;
unsigned int maxCount = 20000;
struct UFSleepTest : public UF
{
    UF* createUF() { return new UFSleepTest(); }

    void run()
    {
        unsigned int counter = 0;

        UFScheduler* this_thread_scheduler = UFScheduler::getUFScheduler(pthread_self());
        UF* this_user_fiber = this_thread_scheduler->getRunningFiberOnThisThread();

        while(counter < maxCount)
        {
          this_thread_scheduler->testingCondTimedWait.lock(this_user_fiber);
          if(counter != 0) {
              if(this_thread_scheduler->testingCondTimedWait.condTimedWait(this_user_fiber, 10000) != ETIMEDOUT) {
                  cerr << "condTimeWait successful" << endl;
                  this_user_fiber->usleep(10000);
                  this_thread_scheduler->testingCondTimedWait.signal();
              }
              else {
                  cerr << "condTimeWait timedout" << endl;
              }
          }
          else {
              this_thread_scheduler->testingCondTimedWait.signal();
          }
          counter++;
          this_thread_scheduler->testingCondTimedWait.unlock(this_user_fiber);
        }

        cerr << "Fiber : " << this_user_fiber << ", Count : " << counter << endl; 
        _parentScheduler->setExitJustMe(true);
    }
};

int main(int argc, char** argv)
{
    if(argc > 1)
        numThreads = atoi(argv[1]);
    if(argc > 2)
        numFibers = atoi(argv[2]);
    if(argc > 3)
        maxCount = atoi(argv[3]);

    for(int i = 0; i < numThreads; i++) {
        pthread_t thread;
        list<UF*>* ufList = new list<UF*>();
        for(int i = 0; i < numFibers; i++)
            ufList->push_back(new UFSleepTest());
        UFScheduler::ufCreateThread(&thread, ufList);

        void* status;
        pthread_join(thread, &status);
    }

    return 0;
}
