/******************* _*_ Mode: C++; Indent: Inktomi4 _*_ *******************
 Stats

 ***************************************************************************/

#ifndef UF_STAT_SYSTEM_H
#define UF_STAT_SYSTEM_H

#include <map>
#include <string>
#include <vector>
#include <utility>
#include <sstream>
#include <UF.H>

struct Stat
{
    Stat(std::string _name, long long _value);
    std::string name;
    long long value;
};

class UFIO;
class UFServer;
class UFStatSystem
{
public:
    friend class CollectorRunner;
    friend class StatCommandProcessing;

    static bool increment(uint32_t stat_num, long long stat_val = 1);
    static bool increment(const char *stat_name, long long stat_val = 1);
    static bool get(uint32_t stat_num, long long *stat_val);
    static bool get(const char *stat_name, long long *stat_val);
    static bool get_current(uint32_t stat_num, long long *stat_val);
    static bool get_current(const char *stat_name, long long *stat_val);
    
    static bool registerStat(const char *stat_name, uint32_t *stat_num, bool lock_needed = true);
    static void setMaxStatsAllowed(uint32_t max_stats_allowed);
    static void setNumStatsEstimate(uint32_t num_stats_estimate);
    static void setStatCommandPort(int port);
    static void setReadTimeout(int secs = 1, long usecs = 0);
    static void init(UFServer* ufs);
    
private:
    // These must be called after acquiring the global stats lock
    // Since these are private, only methods in UFStatCollectorContinuation can call them
    static void incrementGlobal(uint32_t stat_num, long long stat_val = 1);
    static void clear();
    static void collect();

    static bool getStatNum(const char *stat_name, uint32_t &stat_num);
    static UFServer *server;
    static std::map<std::string, uint32_t> stat_name_to_num;
    static std::vector< Stat > global_stats;
    static uint32_t MAX_STATS_ALLOWED;
    static uint32_t NUM_STATS_ESTIMATE;

    friend class UFStatCollector;
};

struct CollectorRunner : public UF
{
    void run();
    CollectorRunner(bool registerMe = false)
    {
        if(registerMe)
            _myLoc = UFFactory::getInstance()->registerFunc((UF*)this);
    }
    UF* createUF() { return new CollectorRunner(); }
    static CollectorRunner* _self;
    static int _myLoc;
};

struct StatCommandListenerRun;
class UFStatCollector
{
public:
    friend class StatCommandListenerRun;
    friend class StatCommandProcessing;

    static void setStatCommandPort(int port);
    static void setReadTimeout(int secs = 1, long usecs = 0);
    static void init(UFServer* ufs);
    
private:
    static void *scheduler(void *args);
    static void *commandLoop(void *data);
    static void printStats(std::stringstream &printbuf);
    static void printStat(const char *stat_name, std::stringstream &printbuf, bool current = false);
    static void printStats(const std::vector<std::string> &stat_names, std::stringstream &printbuf, bool current = false);
    static void getStatsWithPrefix(const std::string &stat_prefix, std::vector<std::string> &stat_names);

    // member variables
    static int _statCommandPort;
    static time_t _startTime;
    static int _readTimeout;
    static long _readTimeoutUSecs;
};

struct StatCommandListenerRun : public UF
{
    void run();
    StatCommandListenerRun(bool registerMe = false)
    {
        if(registerMe)
            _myLoc = UFFactory::getInstance()->registerFunc((UF*)this);
    }
    UF* createUF() { return new StatCommandListenerRun(); }
    static StatCommandListenerRun* _self;
    static int _myLoc;
};

#endif // UF_STAT_SYSTEM

