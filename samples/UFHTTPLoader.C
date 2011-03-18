#include <stdio.h>
#include <algorithm>
#include <errno.h>
#include <set>
#include <time.h>
#include <sstream>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <string>
#include <stdio.h>

#include <ufcore/UF.H>
#include <ufcore/UFIO.H>
#include <ufcore/UFServer.H>
#include <ufcore/UFConnectionPool.H>
#include <vector>

using namespace std;

unsigned int NUM_REQUESTS_TO_RUN                        = 0;
unsigned short int NUM_THREADS                          = 1;
unsigned int NUM_USER_FIBERS_ALLOWED_TO_RUN             = 1;
unsigned int NUM_CONNECTIONS_PER_FIBER                  = 1;
unsigned int NUM_REQUESTS_PER_FIBER                     = 1;
unsigned int RATE_TO_CREATE_FIBERS                      = 0;
int RATE_TO_CREATE_FIBERS_TIME                          = -1;
unsigned short int THREAD_COMPLETION_PERCENT_TO_BAIL_ON = 100;

struct ResponseInfoObject
{
    ResponseInfoObject()
    {
        total_time                  = 0;
        connect_time                = 0;
        num_attempt                 = 0;
        num_success                 = 0;
        total_success_time          = 0;

        thread_create_error         = 0;
        socket_creation_error       = 0;
        error_response              = 0;
        invalid_response            = 0;
        connect_error               = 0;
        connect_success             = 0;
        write_error                 = 0;
        read_error                  = 0;
        num_user_fibers_running     = 0;
    }

    unsigned long long int total_time;
    unsigned long long int connect_time;
    unsigned int num_attempt;
    unsigned int num_success;
    unsigned long long int total_success_time;

    unsigned int thread_create_error;
    unsigned int socket_creation_error;
    unsigned int error_response;
    unsigned int invalid_response;
    unsigned int connect_success;
    unsigned int connect_error;
    unsigned int write_error;
    unsigned int read_error;
    unsigned int num_user_fibers_running;
    vector <unsigned long long int> results;
};
ResponseInfoObject overallInfo;
pthread_mutex_t overallInfoTrackMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_key_t threadUpdateOverallInfo;

TIME_IN_US GET_RESPONSE_TIMEOUT = -1;
string DOUBLE_NEWLINE = "\r\n\r\n";
unsigned int DOUBLE_NEWLINE_LENGTH = DOUBLE_NEWLINE.length();
bool readData(UFIO* ufio, bool& connClosed)
{
    string result;
    char buf[4096];
    unsigned int searchStartPos;
    size_t endOfHeaders = string::npos;
    unsigned int contentLength = 0;
    bool okToExitToEnd = false;
    while(1)
    {
        int num_bytes_read = ufio->read(buf, 4095, GET_RESPONSE_TIMEOUT);
        if(num_bytes_read <= 0)
        {
            if(okToExitToEnd && (num_bytes_read == 0))
            {
                cerr<<"okToExitToEnd = "<<okToExitToEnd<<endl;
                connClosed = true;
                return true;
            }
            cerr<<"bytes read = "<<num_bytes_read<<" with errno = "<<strerror(ufio->getErrno())<<endl;
            return false;
        }
        /*
        else
            cerr<<"read"<<string(buf, num_bytes_read)<<endl;
            */
        result.append(buf, num_bytes_read);



        if(endOfHeaders == string::npos)
        {
            if(result.length() > (num_bytes_read + DOUBLE_NEWLINE.length()))
               searchStartPos = result.length() - (num_bytes_read + DOUBLE_NEWLINE.length());
            else
                searchStartPos = 0;
            endOfHeaders = result.find(DOUBLE_NEWLINE, searchStartPos);
            if(endOfHeaders != string::npos)
            {
                //search for the content length;
                bool foundContentLength = false;
                const char* indexOfCL = strstr(result.c_str(), "Content-Length: ");
                if(indexOfCL)
                {
                    sscanf(indexOfCL, "Content-Length: %d", &contentLength);
                    if(!contentLength)
                        cerr<<"found content length but not bytes = "<<result.c_str()+(indexOfCL-result.data())<<endl;
                    else
                        foundContentLength = true;
                }
                else
                {
                    indexOfCL = strstr(result.c_str(), "Content-Length:");
                    if(indexOfCL)
                    {
                        sscanf(indexOfCL, "Content-Length:%d", &contentLength);
                        if(!contentLength)
                            cerr<<"found content length but not bytes = "<<result.c_str()+(indexOfCL-result.data())<<endl;
                        else
                            foundContentLength = true;
                    }
                }
                if(!foundContentLength)
                    okToExitToEnd = true;
            }
        }
        if(!okToExitToEnd &&
           (result.length() > (endOfHeaders + DOUBLE_NEWLINE_LENGTH + contentLength))) //dont support pipelining yet
        {
            cerr<<"read more than supposed to"<<endl;
            cerr<<"read "<<result;
            return false;
        }
        else if(result.length() == endOfHeaders + DOUBLE_NEWLINE_LENGTH + contentLength)
            return true;
    }

    return false;
}


unsigned int SLEEP_BETWEEN_CONN_SETUP = 0;
TIME_IN_US CONNECT_AND_REQUEST_TIMEOUT = -1;
bool writeData(UFIO* ufio, const string& data)
{
    int amt_written = ufio->write(data.data(), data.length(), CONNECT_AND_REQUEST_TIMEOUT);
    if(amt_written <= 0)
        cerr<<"write failed "<<ufio->getErrno()<<endl;
    return ((amt_written == (int)data.length()) ? true : false);
}

string remote_addr = "";
int sockBuf = 0;
UFIO* getConn(ResponseInfoObject* rIO)
{
    UFConnectionPool* cpool = UFIOScheduler::getUFIOS()->getConnPool();
    if(!cpool)
        return 0;

    struct timeval start,finish;
    gettimeofday(&start, 0);
    UFIO* ufio = cpool->getConnection(remote_addr, true, CONNECT_AND_REQUEST_TIMEOUT);
    if(!ufio)
    {
        if(random()%100 < 10)
            cerr<<"connect error: "<<strerror(errno)<<endl;
        rIO->connect_error++;
        return 0;
    }
    gettimeofday(&finish, 0);
    rIO->connect_success++;
    rIO->connect_time += ((finish.tv_sec - start.tv_sec) * 1000000) + (finish.tv_usec - start.tv_usec);


    if(sockBuf)
    {
        setsockopt(ufio->getFd(), SOL_SOCKET, SO_RCVBUF, (char*) &sockBuf, sizeof( sockBuf ));
        setsockopt(ufio->getFd(), SOL_SOCKET, SO_SNDBUF, (char*) &sockBuf, sizeof( sockBuf ));
    }

    return ufio;
}

string host_header = "";
bool GENERATE_RANDOM_STRING = false;
string HTTP_BASE_REQ_STRING = "/index.html";
string MSG_STRING = "";
unsigned int INTER_SEND_SLEEP = 0;
unsigned int global_counter = 0;



unsigned long long int GENERATE_RANDOM_STRING_CONSTRAINT = 2000000000;
bool sleepShouldBeRandom = true;
void run_handler()
{
    ResponseInfoObject* rIO = (ResponseInfoObject*)pthread_getspecific(threadUpdateOverallInfo);
    if(!rIO)
        return;

    //create the socket to build the connection on
    struct timeval start,finish;
    UFIO* ufio = getConn(rIO);
    if(!ufio)
        return;

    //do the requests
    unsigned int num_requests_run = 0;
    while(num_requests_run++ < NUM_REQUESTS_PER_FIBER)
    {
        if(INTER_SEND_SLEEP)
        {
            if(sleepShouldBeRandom)
                UF::gusleep((random()%INTER_SEND_SLEEP)*1000);
            else
                UF::gusleep(INTER_SEND_SLEEP*1000);
        }

        rIO->num_attempt++;

        if(GENERATE_RANDOM_STRING)
        {
            stringstream ss;
            ss<<"GET "<<"/test"<<random()%10<<"/test"<<random()%10<<"/index.html/"<<(random()%GENERATE_RANDOM_STRING_CONSTRAINT)<<" HTTP/1.0\r\nHost: "<<host_header<<"\r\nConnection: Keep-Alive\r\n\r\n";
            gettimeofday(&start, 0);
            if(!writeData(ufio, ss.str()))
                goto run_handler_done;
        }
        else
        {
            if(!MSG_STRING.length())
                MSG_STRING = "GET " + HTTP_BASE_REQ_STRING + " HTTP/1.0\r\nHost: " + host_header + "\r\nConnection: Keep-Alive\r\n\r\n";
            gettimeofday(&start, 0);
            if(!writeData(ufio, MSG_STRING))
            {
                if(random()%100 < 10)
                    cerr<<"error on write = "<<strerror(errno)<<endl;
                rIO->write_error++;
                goto run_handler_done;
            }
        }


        bool connClosed = false;
        if(!readData(ufio, connClosed))
        {
            if(random()%100 < 10)
                cerr<<"bailing since read data failed "<<strerror(errno)<<endl;
            rIO->read_error++;
            goto run_handler_done;
        }

        gettimeofday(&finish, 0);
        //save the diff_time
        unsigned long long int diff_time = ((finish.tv_sec - start.tv_sec) * 1000000) + (finish.tv_usec - start.tv_usec);
        rIO->results.push_back(diff_time);
        rIO->total_success_time += diff_time;


        rIO->num_success++;

        if(connClosed)
        {
            UFIOScheduler::getUFIOS()->getConnPool()->releaseConnection(ufio, false);
            ufio = getConn(rIO);
            if(!ufio)
                return;
        }
    }

run_handler_done:
    UFIOScheduler::getUFIOS()->getConnPool()->releaseConnection(ufio, false);
}


struct ClientUF : public UF
{
    void run();
    UF* createUF() { return new ClientUF(); }
};

void ClientUF::run()
{
    ResponseInfoObject* rIO = (ResponseInfoObject*)pthread_getspecific(threadUpdateOverallInfo);
    if(!rIO)
        return;
    unsigned int num_requests_run = 0;
    while(num_requests_run++ < NUM_CONNECTIONS_PER_FIBER)
    {
        //wait if told to do so
        if(SLEEP_BETWEEN_CONN_SETUP)
        {
            if(sleepShouldBeRandom)
                UF::gusleep((random()%SLEEP_BETWEEN_CONN_SETUP)*1000);
            else
                UF::gusleep(SLEEP_BETWEEN_CONN_SETUP*1000);
        }

        //now start sending the data
        run_handler();
    }

    rIO->num_user_fibers_running--;
    return;
}

struct SetupClientUF : public UF
{
    void run();
    UF* createUF() { return new SetupClientUF(); }
};

void SetupClientUF::run()
{
    ResponseInfoObject rIO;
    pthread_setspecific(threadUpdateOverallInfo, &rIO);

    UFScheduler* ufs = UFScheduler::getUFScheduler();
    if(!ufs)
        return;

    unsigned short int num_contiguous_thread_creation_failures = 0;
    struct timeval start,finish;
    gettimeofday(&start, 0);


    while(rIO.num_user_fibers_running < NUM_USER_FIBERS_ALLOWED_TO_RUN)
    {
        if (!ufs->addFiberToScheduler(new ClientUF()))
        {
            rIO.thread_create_error++;
            cerr<<"thread_create error with errno = "<<strerror(errno)<<endl;
                                                  //only 5 contiguous failues allowed at a time
            if (num_contiguous_thread_creation_failures++ == 5)
            {
                cerr<<"had "<<num_contiguous_thread_creation_failures<<" failures in creating threads - so bailing"<<endl;
                break;
            }
        }
        else
        {
            rIO.num_user_fibers_running++;
            num_contiguous_thread_creation_failures = 0;
        }
    }

    //create the RATE_TO_CREATE_FIBERS based threads
    if(RATE_TO_CREATE_FIBERS)
    {
        time_t start_time = time(0);
        num_contiguous_thread_creation_failures = 0;
        bool breakFromRateCreationThreads = false;
        unsigned int totalCountRate = 0;
        while(!breakFromRateCreationThreads)
        {
            struct timeval start_rate,finish_rate;
            gettimeofday(&start_rate, 0);
            cerr<<time(0)<<": "<<getpid()<<" opening "<<RATE_TO_CREATE_FIBERS<<" connections with current count = "<<totalCountRate<<" and num active threads = "<<rIO.num_user_fibers_running<<endl;
            for(unsigned int i = 0; i < RATE_TO_CREATE_FIBERS; ++i)
            {
                if (!ufs->addFiberToScheduler(new ClientUF()))
                {
                    rIO.thread_create_error++;
                    cerr<<"thread_create error with errno = "<<strerror(errno)<<endl;
                                                      //only 5 contiguous failues allowed at a time
                    if (num_contiguous_thread_creation_failures++ == 5)
                    {
                        cerr<<"had "<<num_contiguous_thread_creation_failures<<" failures in creating threads - so bailing"<<endl;
                        breakFromRateCreationThreads = true;
                        break;
                    }
                }
                else
                    rIO.num_user_fibers_running++;

                UF::gusleep((int)(1000000/RATE_TO_CREATE_FIBERS));

                totalCountRate++;
            }
            gettimeofday(&finish_rate, 0);

            if((RATE_TO_CREATE_FIBERS_TIME != -1) && (RATE_TO_CREATE_FIBERS_TIME + start_time) < time(0))
                break;
        }
    }
    else
    {
        while(1)
        {
            if(!rIO.num_user_fibers_running)
                break;

            UF::gusleep(5000000);
            unsigned short int threadCompletionPercent = (rIO.num_attempt*100)/NUM_REQUESTS_TO_RUN;
            cerr <<pthread_self()<<": completed "<<rIO.num_attempt<<"/"<<NUM_REQUESTS_TO_RUN<<" ("<<threadCompletionPercent<<"%)"<<endl;

            if(threadCompletionPercent > THREAD_COMPLETION_PERCENT_TO_BAIL_ON)
                break;
        }
    }


    gettimeofday(&finish, 0);
    rIO.total_time += (finish.tv_sec-start.tv_sec)*1000000 + (finish.tv_usec-start.tv_usec);


    //TODO:add the total info into the total pool
    pthread_mutex_lock(&overallInfoTrackMutex);
    if(rIO.total_time > overallInfo.total_time)
        overallInfo.total_time = rIO.total_time;
    overallInfo.connect_time += rIO.connect_time;
    overallInfo.num_attempt += rIO.num_attempt;
    overallInfo.num_success += rIO.num_success;
    overallInfo.total_success_time += rIO.total_success_time;
    overallInfo.thread_create_error += rIO.thread_create_error;
    overallInfo.socket_creation_error += rIO.socket_creation_error;
    overallInfo.error_response += rIO.error_response;
    overallInfo.invalid_response += rIO.invalid_response;
    overallInfo.connect_error += rIO.connect_error;
    overallInfo.connect_success += rIO.connect_success;
    overallInfo.write_error += rIO.write_error;
    overallInfo.read_error += rIO.read_error;
    overallInfo.num_user_fibers_running += rIO.num_user_fibers_running;
    for(vector<unsigned long long int>::iterator beg = rIO.results.begin(); beg != rIO.results.end(); ++beg)
        overallInfo.results.push_back(*beg);
    pthread_mutex_unlock(&overallInfoTrackMutex);


    ufs->setExitJustMe(true);
}

void printResults()
{
    unsigned int num_fail = overallInfo.read_error+overallInfo.write_error+overallInfo.connect_error+overallInfo.error_response+overallInfo.invalid_response;
    unsigned int total_should_run = NUM_REQUESTS_PER_FIBER*NUM_CONNECTIONS_PER_FIBER*NUM_USER_FIBERS_ALLOWED_TO_RUN*NUM_THREADS;
    double avg_req_sec = (overallInfo.total_time) ? overallInfo.num_success/((double)overallInfo.total_time/1000000) : 0;
    double avg_tm_suc_req = (overallInfo.num_success) ? (double) (overallInfo.total_success_time) / (double) overallInfo.num_success : 0;
    double avg_suc_req_sec = (overallInfo.total_success_time) ? overallInfo.num_success/((double)overallInfo.total_success_time/1000000) : 0;


    float estimatedSuccess = (total_should_run) ? (overallInfo.num_success*100/total_should_run) : 0;
    float successPercent = (overallInfo.num_success+num_fail) ? (overallInfo.num_success*100/(overallInfo.num_success+num_fail)) : 0;
    cout<<"estimated success % = "<<estimatedSuccess
        <<" , real success % = "<<successPercent
        <<" , avg tm/suc req = "<<avg_tm_suc_req<<" us"
        <<" , avg succ req/sec = "<< avg_suc_req_sec
        <<" , avg req/sec = "<< avg_req_sec
        <<" , tot time = " <<overallInfo.total_time<<" us"
        <<" , tot shd run = "<<total_should_run
        <<" , num att = "<<overallInfo.num_attempt
        <<" , suc cnt = "<<overallInfo.num_success
        <<" , estimated fail cnt = "<<total_should_run-overallInfo.num_success
        <<" , real fail cnt = "<<num_fail
        <<" , thrd_creat_err = "<<overallInfo.thread_create_error
        <<" , sokt_creat_err = "<<overallInfo.socket_creation_error
        <<" , connect_error = "<<overallInfo.connect_error
        <<" , err_resp = "<<overallInfo.error_response
        <<" , invalid_resp = "<<overallInfo.invalid_response
        <<" , read_err = "<<overallInfo.read_error
        <<" , write_err = "<<overallInfo.write_error<<endl;

    if(!overallInfo.results.size())
        return;

    sort (overallInfo.results.begin(), overallInfo.results.end());
    unsigned int lastDump = 0;
    unsigned currLocation = 0;
    int counter = 0;
    cout<<"percentile breakdown w/ size = "<<overallInfo.results.size()<<endl;
    unsigned int lastValue = *(overallInfo.results.begin());
    for (vector<unsigned long long int>::iterator it=overallInfo.results.begin();
         it!=overallInfo.results.end(); 
         ++it)
    {
        currLocation = 100*counter++/overallInfo.results.size();
        if(lastDump == currLocation)
            continue;
        if((currLocation % 10 ) == 0 || (currLocation) >= 95)
        {
            cout<<" "<<currLocation<<"%"<<" <= "<< *it<<"us"<<endl;
            lastDump = currLocation;
        }
        lastValue = *it;
    }
    cout<<"100%"<<" <= "<< lastValue <<"us"<<endl;

    cout<<"min = "<<*(overallInfo.results.begin())<<"us"<<endl;
    cout<<"max = "<<lastValue<<"us"<<endl;
}

void print_info()
{
    cerr<<"connecting to               = "<<remote_addr<<endl;
    cerr<<"NUM_USER_FIBERS_ALLOWED_TO_RUN  = "<<NUM_USER_FIBERS_ALLOWED_TO_RUN<<endl;
    cerr<<"NUM_CONNECTIONS_PER_FIBER  = "<<NUM_CONNECTIONS_PER_FIBER<<endl;
    cerr<<"NUM_REQUESTS_PER_FIBER = "<<NUM_REQUESTS_PER_FIBER<<endl;
    cerr<<"CONNECT_AND_REQUEST_TIMEOUT = "<<CONNECT_AND_REQUEST_TIMEOUT<<endl;
    cerr<<"GET_RESPONSE_TIMEOUT        = "<<GET_RESPONSE_TIMEOUT<<endl;
    cerr<<"INTER_SEND_SLEEP            = "<<INTER_SEND_SLEEP<<endl;
    cerr<<"SLEEP_BETWEEN_CONN_SETUP    = "<<SLEEP_BETWEEN_CONN_SETUP<<endl;
    cerr<<"THREAD_COMPLETION_PERCENT_TO_BAIL_ON = "<<THREAD_COMPLETION_PERCENT_TO_BAIL_ON<<endl;

}

void print_usage()
{
    cerr<<"StHTTPLoader: "<<endl
        <<"\t[-f <num_threads>]"<<endl
        <<"\t[-H <host>]"<<endl
        <<"\t[-o <host header>]"<<endl
        <<"\t[-P <port to connect to> (80)]"<<endl
        <<"\t[-a <rate_to_create_conns/sec>]"<<endl
        <<"\t[-A <how long to run rate request [-1]"<<endl
        <<"\t[-t <num_user_threads_to_start>]"<<endl
        <<"\t[-C <NUM_CONNECTIONS_PER_FIBER>]"<<endl
        <<"\t[-R <NUM_REQUESTS_PER_FIBER>]"<<endl
        <<"\t[-c <CONNECT_AND_REQUEST_TIMEOUT in ms (1000)]"<<endl
        <<"\t[-d <GET_RESPONSE_TIMEOUT in ms        (1000)>]"<<endl
        <<"\t[-s INTER_SEND_SLEEP (in msec)]"<<endl
        <<"\t[-r request string (get ip:1.2.3.4)]"<<endl
        <<"\t[-e END STRING (END)]"<<endl
        <<"\t[-S SLEEP_BETWEEN_CONN_SETUP (in msec)]"<<endl
        <<"\t[-Z set the sleep to be random"<<endl
        <<"\t[-b thread completion percent at which to bail"<<endl
        <<"\t[-m generate random url"<<endl
        <<"\t[-M constraint on the number of urls to generate"<<endl
        <<"\t[-U sock buf sizes for snd + recv [default = 0/none]"<<endl
        <<"\t[-? this help screen]"<<endl
        <<"\t[-h this help screen]"<<endl;
    exit(0);
}

int main(int argc, char** argv)
{
    unsigned long long int DELAY_BETWEEN_STARTING_THREADS_IN_US = 0;
    if(pthread_key_create(&threadUpdateOverallInfo, 0) != 0)
    {
        cerr<<"couldnt create key for threadUpdateOverallInfo "<<strerror(errno)<<endl;
        return 1;
    }

    string rem_port = "80";
    string rem_addr = "127.0.0.1";
    char ch;
	while ((ch = getopt(argc, argv, "M:Z:U:x:m:o:A:a:b:r:S:t:H:P:R:C:f:c:d:s:?h")) != -1) 
    {
		switch (ch) 
        {
            case'x':
                DELAY_BETWEEN_STARTING_THREADS_IN_US = atoi(optarg);
                break;
            case 'Z':
                sleepShouldBeRandom = atoi(optarg);
                break;
            case 'm':
                GENERATE_RANDOM_STRING = atoi(optarg);
                break;
            case 'M':
                GENERATE_RANDOM_STRING_CONSTRAINT = atoi(optarg);
                break;
            case 'o':
                host_header = optarg;
                break;
            case 'P':
                rem_port = optarg;
                break;
		    case 'H':
                rem_addr = optarg;
			    break;
		    case 't':
			    NUM_USER_FIBERS_ALLOWED_TO_RUN = atoi(optarg);
			    break;
            case 'a':
                RATE_TO_CREATE_FIBERS = atoi(optarg);
                break;
            case 'A':
                RATE_TO_CREATE_FIBERS_TIME = atoi(optarg);
                break;
		    case 'C':
                NUM_CONNECTIONS_PER_FIBER = atoi(optarg);
			    break;
		    case 'R':
                NUM_REQUESTS_PER_FIBER = atoi(optarg);
			    break;
		    case 'f':
			    NUM_THREADS = atoi(optarg);
			    break;
            case 'S':
                SLEEP_BETWEEN_CONN_SETUP = atoi(optarg)*1000;
                if(SLEEP_BETWEEN_CONN_SETUP < 0)
                    SLEEP_BETWEEN_CONN_SETUP = 0;
                break;
            case 'r':
                HTTP_BASE_REQ_STRING = optarg;
                break;
            case 'c':
                CONNECT_AND_REQUEST_TIMEOUT = (atoi(optarg) > 0) ? atoi(optarg)*1000 : -1;
                break;
            case 'd':
                GET_RESPONSE_TIMEOUT = (atoi(optarg) > 0) ? atoi(optarg)*1000 : -1;
                break;
            case 's':
                INTER_SEND_SLEEP = atoi(optarg)*1000;
                break;
		    case 'b':
                THREAD_COMPLETION_PERCENT_TO_BAIL_ON = atoi(optarg);
                break;
		    case 'U':
                sockBuf = atoi(optarg);
                break;
		    case 'h':
		    case '?':
		    default:
			    print_usage();
			    break;
		}
	}

    if(!host_header.length())
        host_header = rem_addr;


    remote_addr = rem_addr + ":" + rem_port;
    print_info();

    NUM_REQUESTS_TO_RUN = NUM_REQUESTS_PER_FIBER*NUM_CONNECTIONS_PER_FIBER*NUM_USER_FIBERS_ALLOWED_TO_RUN;

    //create the threads
    pthread_t* thread = new pthread_t[NUM_THREADS];
    unsigned int i = 0;
    for(; i<NUM_THREADS; i++)
    {
        list<UF*>* ufList = new list<UF*>();
        ufList->push_back(new SetupClientUF());
        UFIO::ufCreateThreadWithIO(&thread[i], ufList);
        if(DELAY_BETWEEN_STARTING_THREADS_IN_US)
        {
            cerr<<"sleeping for "<<DELAY_BETWEEN_STARTING_THREADS_IN_US<<endl;
            usleep(DELAY_BETWEEN_STARTING_THREADS_IN_US);
        }
    }


    //wait for kids
    void* status;
    for(i=0; i<NUM_THREADS; i++)
        pthread_join(thread[i], &status);
    delete [] thread;


    printResults();
}
