#ifndef UF_LOG_H
#define UF_LOG_H

#include <stdio.h>

#define LOG_ERROR(fmt, args...) do {                                    \
    fprintf(stderr, "[%s:%d] [%s] ERROR: " fmt "\n", __FILE__, __LINE__, __FUNCTION__ , ##args ); \
    } while (0)

#ifdef DEBUG
#define LOG_DEBUG(fmt, args...) do {                                    \
    fprintf(stderr, "[%s:%d] [%s] DEBUG: " fmt "\n", __FILE__, __LINE__, __FUNCTION__ , ##args ); \
    } while (0)
#else
#define LOG_DEBUG(fmt, args...)
#endif

#include <string>

#include <boost/logging/format_fwd.hpp>
// Step 1: Optimize : use a cache string, to make formatting the message faster
BOOST_LOG_FORMAT_MSG( optimize::cache_string_one_str<> ) 

#include <boost/logging/format_ts.hpp>
#include <boost/logging/format/formatter/thread_id.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/xtime.hpp>
using namespace boost::logging;

/**
 *  Logger related functions go into this namespace
 *  set* functions should be called only before calling init()
 */
namespace Logger
{
    /**
     *  @param access log file
     *  sets the access log file
     */
    void setAccessLogFile(const std::string &log);

    /**
     *  @param writePeriodInMS flush interval for flush thread in milliseconds
     *  sets the interval at which log lines are written by the flush thread to the destinations
     */
    void setWritePeriod(int writePeriodInMS);

    /**
     *  @param queueSize max number of log lines that can be in the queue 
     *  sets the max number of log lines that can be in the flush queue.
     *  messages logged when the queue size reaches this number will be dropped!
     */
    void setMaxQueueSize(size_t queueSize);

    /**
     *  Initialize error and debug logs
     *  Enables only error log
     */
    void init();

    /**
     *  Enable debug log
     *  Debug logs are written to debug.out in the current directory by default
     *  This can be overridden by calling setDebugLogFile
     */
    void enableDebug();

    /**
     *  Disable debug log
     */
    void disableDebug();

// Step 3 : Specify your logging class(es)
    typedef boost::logging::logger_format_write< default_, default_, writer::threading::on_dedicated_thread > log_type;

// Step 4: declare which filters and loggers you'll use
    extern BOOST_DECLARE_LOG_FILTER(g_log_level, boost::logging::level::holder_tss_with_cache<30> ) 
    extern BOOST_DECLARE_LOG(g_log, log_type);
    extern BOOST_DECLARE_LOG(g_log_access, log_type);
};

// Step 5: define the macros through which you'll log
#define L_DEBUG  BOOST_LOG_USE_LOG_IF_LEVEL(Logger::g_log(), Logger::g_log_level(), debug ) << "D|[" << __FILE__ << ":" << __LINE__ << "] [" << __FUNCTION__ << "]|"
#define L_ERROR  BOOST_LOG_USE_LOG_IF_LEVEL(Logger::g_log(), Logger::g_log_level(), error ) << "E|[" << __FILE__ << ":" << __LINE__ << "] [" << __FUNCTION__ << "]|"
#define L_INFO   BOOST_LOG_USE_LOG_IF_LEVEL(Logger::g_log(), Logger::g_log_level(), info  ) << "I|[" << __FILE__ << ":" << __LINE__ << "] [" << __FUNCTION__ << "]|"
#define L_ACCESS BOOST_LOG_USE_LOG_IF_LEVEL(Logger::g_log_access(), Logger::g_log_level(), error ) << "A|[" << __FILE__ << ":" << __LINE__ << "] [" << __FUNCTION__ << "]|"

#endif // UF_LOG_H
