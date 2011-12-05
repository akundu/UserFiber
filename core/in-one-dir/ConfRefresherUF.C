#include <uf/core/ConfRefresherUF.H>

int ConfRefresherUF::_myLoc = -1;
ConfRefresherUF* ConfRefresherUF::_self = new ConfRefresherUF(true);

void ConfRefresherUF::run()
{
    UFConfManager *confManager = (UFConfManager *)pthread_getspecific(UFConfManager::threadSpecificKey);
    if(confManager == NULL)
        return;
#ifdef __linux__
    // Use inotify. Just call conf manager reload
    confManager->reload();
#else
    long refresh_time = UFConfManager::getRefreshTime();
    if(!refresh_time)
        return;

    while(1) {
        UF* uf = UFScheduler::getUF();
        // Sleep between checks for conf file changes
        uf->usleep(refresh_time);
        
        // Ask conf manager to pick up changes
        confManager->reload();
    }
#endif
}
