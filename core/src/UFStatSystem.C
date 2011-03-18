#include <string.h>
#include <stdio.h>
#include <UFStatSystem.H>
#include <UF.H>
#include <UFIO.H>
#include <UFServer.H>
#include <iostream>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

using namespace std;
UFServer *UFStatSystem::server;

std::map<std::string, uint32_t> UFStatSystem::stat_name_to_num;

Stat::Stat(std::string _name, long long _value) :
  name(_name), value(_value)
{

}

std::vector< Stat > UFStatSystem::global_stats;
uint32_t UFStatSystem::MAX_STATS_ALLOWED = 500000;
uint32_t UFStatSystem::NUM_STATS_ESTIMATE = 5000;
static UFMutex statsMutex;

void UFStatSystem::incrementGlobal(uint32_t stat_num, long long stat_val)
{
    if(stat_num >= global_stats.size()) {
        return;
    }
    global_stats[stat_num].value += stat_val;
}

bool UFStatSystem::increment(uint32_t stat_num, long long stat_val)
{
    if(stat_num >= MAX_STATS_ALLOWED) {
        return false;
    }
  
    // Increment stat in this thread.
    // If resize is required, take the thread's stats_lock
    
    UFScheduler* running_thread_scheduler = UFScheduler::getUFScheduler(pthread_self());
    UF* running_user_fiber = running_thread_scheduler->getRunningFiberOnThisThread();

    if(running_thread_scheduler->_stats.size() < (unsigned int)stat_num+1) {
        running_thread_scheduler->_stats_lock.lock(running_user_fiber);
        uint32_t stat_vec_size = ( (NUM_STATS_ESTIMATE >= stat_num+1) ? NUM_STATS_ESTIMATE : stat_num+1);
        running_thread_scheduler->_stats.resize(stat_vec_size, 0);
        running_thread_scheduler->_stats_lock.unlock(running_user_fiber);
    }
    
    running_thread_scheduler->_stats[stat_num] += stat_val;
    return true;
}

bool UFStatSystem::increment(const char *stat_name, long long stat_val)
{
    uint32_t stat_num;
    if(!getStatNum(stat_name, stat_num)) {
        return false;
    }
    return increment(stat_num, stat_val);
}

bool UFStatSystem::get(uint32_t stat_num, long long *stat_val)
{
    // Get stat lock
    UFScheduler* running_thread_scheduler = UFScheduler::getUFScheduler(pthread_self());
    UF* running_user_fiber = running_thread_scheduler->getRunningFiberOnThisThread();
    statsMutex.lock(running_user_fiber);

    // Check if stat_num is valid
    if(stat_num >= global_stats.size()) {
        statsMutex.unlock(running_user_fiber);
        return false;
    }

    // Get stat value from global map
    *stat_val = global_stats[stat_num].value;
    statsMutex.unlock(running_user_fiber);
    return true;
}

bool UFStatSystem::get(const char *stat_name, long long *stat_val)
{
    // Get value of stat with name == stat_name
    // Returns value from the global stats map. does not walk threads
    uint32_t stat_num;
    if(!getStatNum(stat_name, stat_num)) {
        return false;
    }
    return get(stat_num, stat_val);
}

bool UFStatSystem::get_current(uint32_t stat_num, long long *stat_val)
{
    // Returns current value of stat. Walks all threads

    UFScheduler* running_thread_scheduler = UFScheduler::getUFScheduler(pthread_self());
    UF* running_user_fiber = running_thread_scheduler->getRunningFiberOnThisThread();

    *stat_val = 0;
    // Collect stat from all threads

    StringThreadMapping * all_threads = server->getThreadList();

    for(std::map<std::string, std::vector<pthread_t>* >::const_iterator map_it = all_threads->begin();
        map_it != all_threads->end();
        map_it++) {
        for(std::vector<pthread_t>::const_iterator thread_it = map_it->second->begin();
            thread_it != map_it->second->end(); thread_it++) {
            UFScheduler* this_thread_scheduler = UFScheduler::getUFScheduler(*thread_it);
            this_thread_scheduler->_stats_lock.lock(running_user_fiber);
            if(this_thread_scheduler->_stats.size() > stat_num) { 
                *stat_val += this_thread_scheduler->_stats[stat_num];
            }
            this_thread_scheduler->_stats_lock.unlock(running_user_fiber);
        }
    }
    return true;
}

bool UFStatSystem::get_current(const char *stat_name, long long *stat_val)
{
    uint32_t stat_num;
    if(!getStatNum(stat_name, stat_num))
        return false;
    return get_current(stat_num, stat_val);
}

bool UFStatSystem::registerStat(const char *stat_name, uint32_t *stat_num, bool lock_needed)
{
    if(!stat_num ) {
        return false;
    }

    // Get stat lock
    UFScheduler* running_thread_scheduler = NULL;
    UF* running_user_fiber = NULL;
    if(lock_needed) {
        running_thread_scheduler = UFScheduler::getUFScheduler(pthread_self());
        running_user_fiber = running_thread_scheduler->getRunningFiberOnThisThread();
        statsMutex.lock(running_user_fiber);
    }

    // Check if stat is already registered
    std::map<std::string, uint32_t>::const_iterator stat_name_it = stat_name_to_num.find(stat_name);
    if(stat_name_it != stat_name_to_num.end()) {
        *stat_num = stat_name_it->second;
        if(lock_needed)
            statsMutex.unlock(running_user_fiber);
        return true;
    }

    // Check to see if limit for max allowed stats was hit
    if(global_stats.size() == MAX_STATS_ALLOWED) {
        if(lock_needed)
          statsMutex.unlock(running_user_fiber);
        *stat_num = MAX_STATS_ALLOWED;
        return false;
    }

    // Regiter new stat. Store mapping from stat_num to name
    global_stats.push_back(Stat(stat_name, 0));
    *stat_num = global_stats.size() - 1;
    stat_name_to_num[stat_name] = *stat_num;

    // Release stat lock
    if(lock_needed)
        statsMutex.unlock(running_user_fiber);
    return true;
}

void UFStatSystem::setMaxStatsAllowed(uint32_t max_stats_allowed)
{
    MAX_STATS_ALLOWED = max_stats_allowed;
}

void UFStatSystem::setNumStatsEstimate(uint32_t num_stats_estimate)
{
    if(num_stats_estimate < MAX_STATS_ALLOWED)
        NUM_STATS_ESTIMATE = num_stats_estimate;
    else
        NUM_STATS_ESTIMATE = MAX_STATS_ALLOWED;
}

void UFStatSystem::setStatCommandPort(int port)
{
    UFStatCollector::setStatCommandPort(port);
}

void UFStatSystem::setReadTimeout(int secs, long usecs)
{
    UFStatCollector::setReadTimeout(secs, usecs);
}

void UFStatSystem::init(UFServer* ufs)
{
    // Store server pointer
    server = ufs;
    UFStatCollector::init(ufs);
}

void UFStatSystem::clear()
{
    for(std::vector< Stat >::iterator it = UFStatSystem::global_stats.begin();
            it != UFStatSystem::global_stats.end(); it++) {
        it->value = 0;
    }
}

void UFStatSystem::collect()
{
    UFScheduler* stat_thread_scheduler = UFScheduler::getUFScheduler(pthread_self());
    UF* stat_user_fiber = stat_thread_scheduler->getRunningFiberOnThisThread();
    statsMutex.lock(stat_user_fiber);
    UFStatSystem::clear();

    StringThreadMapping * all_threads = server->getThreadList();

    for(std::map<std::string, std::vector<pthread_t>* >::const_iterator map_it = all_threads->begin();
        map_it != all_threads->end();
        map_it++) {
        for(std::vector<pthread_t>::const_iterator thread_it = map_it->second->begin();
            thread_it != map_it->second->end(); thread_it++) {
            UFScheduler* this_thread_scheduler = UFScheduler::getUFScheduler(*thread_it);
            // Lock thread stats to prevent resizing on increment
            this_thread_scheduler->_stats_lock.lock(stat_user_fiber);
            int i = 0;
            for(std::vector<long long>::iterator it = this_thread_scheduler->_stats.begin();
                it != this_thread_scheduler->_stats.end(); it++, i++) {
                if(*it != 0) {
                    incrementGlobal(i, *it);
                }
            }
            // Release thread stats lock
            this_thread_scheduler->_stats_lock.unlock(stat_user_fiber);
        }
    }
    statsMutex.unlock(stat_user_fiber);
}

bool UFStatSystem::getStatNum(const char *stat_name, uint32_t &stat_num)
{
    UFScheduler* running_thread_scheduler = UFScheduler::getUFScheduler(pthread_self());
    UF* running_user_fiber = running_thread_scheduler->getRunningFiberOnThisThread();
    statsMutex.lock(running_user_fiber);

    // Get stat num and release lock
    std::map<std::string, uint32_t>::const_iterator stat_name_it = stat_name_to_num.find(stat_name);
    if(stat_name_it == stat_name_to_num.end()) {
        statsMutex.unlock(running_user_fiber);
        return false;
    }

    stat_num = stat_name_it->second;
    statsMutex.unlock(running_user_fiber);
    return true;
}

struct StatThreadChooser : public UFIOAcceptThreadChooser
{
    static pair<UFScheduler*, pthread_t> _accept_thread;
    pair<UFScheduler*, pthread_t> pickThread(int listeningFd)
    {
        return _accept_thread;
    }
};
pair<UFScheduler*, pthread_t> StatThreadChooser::_accept_thread;

static int MAX_STAT_NAME_LENGTH = 512;
int UFStatCollector::_statCommandPort = 8091;
time_t UFStatCollector::_startTime = time(NULL);
int UFStatCollector::_readTimeout = 600;
long UFStatCollector::_readTimeoutUSecs = 0;

void UFStatCollector::setStatCommandPort(int port)
{
    _statCommandPort = port;
}

void UFStatCollector::setReadTimeout(int secs, long usecs)
{
    _readTimeout = secs;
    _readTimeoutUSecs = usecs;
}

void UFStatCollector::init(UFServer* ufs)
{
    pthread_t stat_thread;
    pthread_create(&stat_thread, NULL, scheduler, ufs);
}

int CollectorRunner::_myLoc = -1;
CollectorRunner* CollectorRunner::_self = new CollectorRunner(true);
void CollectorRunner::run()
{
    UF* uf = UFScheduler::getUF();
    UFScheduler* ufs = uf->getParentScheduler();

    while(1) 
    {
        uf->usleep(60000000); //60 secs
        UFStatSystem::collect();
    }
    ufs->setExit();
}


//---------------------------------------------------------------------
// Handles a command port client connection
struct StatCommandProcessing : public UF
{
    void run();
    StatCommandProcessing(bool registerMe = false)
    {
        if(registerMe)
            _myLoc = UFFactory::getInstance()->registerFunc((UF*)this);
    }
    UF* createUF() { return new StatCommandProcessing(); }
    static StatCommandProcessing* _self;
    static int _myLoc;
};
int StatCommandProcessing::_myLoc = -1;
StatCommandProcessing* StatCommandProcessing::_self = new StatCommandProcessing(true);
void StatCommandProcessing::run()
{
    if (!_startingArgs)
        return;
    
    //1. create the new UFIO from the new fd
    UFIOAcceptArgs* fiberStartingArgs = (UFIOAcceptArgs*) _startingArgs;
    UFIO* ufio = fiberStartingArgs->ufio;
    
    static const char cmdUnrec[] = "Unrecognized command.\r\n";
    static const char cmdHelp[] = "Valid commands are: \r\n"
        "  stats - Print stats which have been collected.\r\n"
        "  stats_current - Print stats after forcing a collect\r\n"
        "  stat (<stat_name> )* - Print values for stats that are specified. Does not collect\r\n"
        "  stat_current (<stat_name> )* - Print values for stats that are specified after collecting from all threads\r\n"
        "  help - Prints this message.\r\n"
        "  quit - Close this connection.\r\n"
        ;
    int readbytes;
    char readbuf[1024];
    std::string readData;
    
    while(1) 
    {
        if((readbytes = ufio->read(readbuf, 1023, 60000000)) <= 0)
            break;

        readData.append(readbuf, readbytes);

        if(readData.find("\r\n") == string::npos)
           continue;
           
        if(readData.find("stats_current") != string::npos) 
        {
            // Force a collect before printing out the stats
            UFStatSystem::collect();
            std::stringstream printbuf;
            UFStatCollector::printStats(printbuf);
            if (ufio->write(printbuf.str().data(), printbuf.str().length()) == -1)
                //failed write, break to close connection
                break;
        }
        else if (readData.find("stats") != string::npos) 
        {
            std::stringstream printbuf;
            UFStatCollector::printStats(printbuf);
            if (ufio->write(printbuf.str().data(), printbuf.str().length()) == -1)
                    //failed write, break to close connection
                break;
        }
        else if (readData.find("stat ") != string::npos || readData.find("stat_current ") != string::npos) 
        {
            std::vector<std::string> stats;
            char stat_name[MAX_STAT_NAME_LENGTH];
            bzero(stat_name, MAX_STAT_NAME_LENGTH);
            int next;
            char *start = (char *)readData.c_str();

            // determine if collection has to be forced or not
            bool get_current = false;
            if(readData.find("stat ") != string::npos)
                start += strlen("stat ");
            else 
            {
                start += strlen("stat_current ");
                get_current = true;
            }
            
            while(sscanf(start, "%s%n", stat_name, &next) == 1) 
            {
                // Prefix support
                char *prefix_end = strchr(start, '*');
                if(prefix_end != NULL) 
                {
                    std::string prefix;
                    prefix.assign(start, prefix_end-start);
                    // Get all stats with the prefix
                    UFStatCollector::getStatsWithPrefix(prefix, stats);
                }
                else
                    stats.push_back(stat_name);
                bzero(stat_name, MAX_STAT_NAME_LENGTH);
                start+=next;
            }
            std::stringstream printbuf;
            
            UFStatCollector::printStats(stats, printbuf, get_current);
            if (ufio->write(printbuf.str().data(), printbuf.str().length()) == -1)
                //failed write, break to close connection
                break;
        }
        else if (readData.find("help") != string::npos) 
        {
            if (ufio->write(cmdHelp, sizeof(cmdHelp)-1) == -1)
                //failed write, break to close connection
                break;
        }
        else if (readData.find("quit") != string::npos) 
            break;
        else 
        {
            if ((ufio->write(cmdUnrec, sizeof(cmdUnrec)-1) == -1) ||
                (ufio->write(cmdHelp, sizeof(cmdHelp)-1) == -1))
                //failed write, break to close connection
                break;
        }
        readData.clear();
    } // END while loop
    
    delete ufio;
    delete fiberStartingArgs;
}

//----------------------------------------------------------------------
// Creates a socket for command port and listens for connection requests
int StatCommandListenerRun::_myLoc = -1;
StatCommandListenerRun* StatCommandListenerRun::_self = new StatCommandListenerRun(true);
void StatCommandListenerRun::run()
{
    int fd;
    unsigned int counter = 0;
    do
    {
        UFStatCollector::_statCommandPort += counter;
        fd = UFIO::setupConnectionToAccept(0, UFStatCollector::_statCommandPort, 16000);
        if(fd < 0)
        {
            cerr<<"couldnt setup accept thread for stat port "<<strerror(errno)<<endl;
            if(counter++ == 50) //try upto 50 times
                return;
            continue;
        }
        else
            break;
    } while(1);
    cerr<<"setup stat command port at "<<UFStatCollector::_statCommandPort;


    UFIO* ufio = new UFIO(UFScheduler::getUF());
    if(!ufio)
    {
        cerr<<"couldnt setup accept thread"<<endl;
        ::close(fd);
        return;
    }
    ufio->setFd(fd, false);

    StatThreadChooser ufiotChooser;
    ufio->accept(&ufiotChooser, StatCommandProcessing::_myLoc, UFStatCollector::_statCommandPort, 0, 0);
}

void* UFStatCollector::scheduler(void *args)
{
    if(!args)
        return 0;

    cerr<<getPrintableTime()<<" "<<getpid()<<": created stats thread (with I/O) - "<<pthread_self()<<endl;
    // add jobs to scheduler
    UFScheduler ufs;

    //insertion has to be done in a LIFO (stack) manner
    // stat collector
    ufs.addFiberToScheduler(new CollectorRunner());
    // stat command listener port
    ufs.addFiberToScheduler(new StatCommandListenerRun());
    ((UFServer*) args)->addThread("STAT_COLLECTOR", &ufs);
    // io scheduler
    ufs.addFiberToScheduler(new IORunner());
    
    // set thread for stat command listener to run on
    StatThreadChooser::_accept_thread = make_pair(&ufs, pthread_self());
    
    ufs.runScheduler();
    return 0;
}

//----------------------------------------------------------
void UFStatCollector::printStats(std::stringstream &printbuf) 
{
    printbuf <<  "Cache stats: \n"
                "-----------------------------------------------------------------------------\n";
    printbuf << "TIME " << _startTime <<"\n";

    UFScheduler* running_thread_scheduler = UFScheduler::getUFScheduler(pthread_self());
    UF* running_user_fiber = running_thread_scheduler->getRunningFiberOnThisThread();
    statsMutex.lock(running_user_fiber);
  
    for(std::vector< Stat >::const_iterator it = UFStatSystem::global_stats.begin();
        it != UFStatSystem::global_stats.end(); it++) 
        printbuf << "STAT " << it->name << " " << it->value << "\n";
    statsMutex.unlock(running_user_fiber);

    printbuf << "END\n";
}

void UFStatCollector::printStat(const char *stat_name, std::stringstream &printbuf, bool current) 
{
    long long stat_val = 0;
    bool stat_get_status;
    if(current)
        stat_get_status = UFStatSystem::get_current(stat_name, &stat_val);
    else
        stat_get_status = UFStatSystem::get(stat_name, &stat_val);
    
    //if(stat_get_status && stat_val != 0) {
    if(stat_get_status)
        printbuf << "STAT " << stat_name << " " << stat_val << "\n";
}

void UFStatCollector::printStats(const std::vector<std::string> &stat_names, std::stringstream &printbuf, bool current)
{
    printbuf << "TIME " << _startTime <<"\n";
    for(std::vector<std::string>::const_iterator it = stat_names.begin();
        it != stat_names.end();
        it++)
        printStat(it->c_str(), printbuf, current);
   printbuf << "END\n";
}

void
UFStatCollector::getStatsWithPrefix(const std::string &stat_prefix, std::vector<std::string> &stat_names)
{
    UFScheduler* running_thread_scheduler = UFScheduler::getUFScheduler(pthread_self());
    UF* running_user_fiber = running_thread_scheduler->getRunningFiberOnThisThread();
    statsMutex.lock(running_user_fiber);
    // Get all stats which start with stat_prefix
    for(std::vector< Stat >::const_iterator it = UFStatSystem::global_stats.begin();
        it != UFStatSystem::global_stats.end(); it++) 
    {
        size_t found = it->name.find(stat_prefix);
        if(!found)
            stat_names.push_back(it->name);
    }
    statsMutex.unlock(running_user_fiber);
}
