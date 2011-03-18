#include <iostream>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string.h>

#include "UF.H"
#include "UFIO.H"
#include "UFIO.H"
#include "UFServer.H"
#include "UFStatSystem.H"
#include "UFConnectionPool.H"
#include "UFPC.H"

using namespace std;
bool runOS = true;

static uint32_t http_request;
struct ufTestHTTPServer : public UFServer
{
    ufTestHTTPServer(char* interfaceIP, unsigned int port)
    {
        _addressToBindTo = interfaceIP ? interfaceIP : "";
        _port = port;
    }
    void handleNewConnection(UFIO* ufio);
    void preAccept() { UFStatSystem::registerStat("http_request", &http_request, false); }
};


struct OSProducer : public UF
{
    OSProducer(const std::string& hostToConnect, const std::string& request)
    {
        _p = new UFProducer();
        _p->_requireLockToUpdateConsumers = false;
        _hostToConnect = hostToConnect;
        _request = request;
        _bailProducer = 0;
    }
    ~OSProducer() { }

    void runAsOS();
    void runAsStaticCache();

    static std::string data;

    void run()
    {
        if(runOS)
            runAsOS();
        else
            runAsStaticCache();
    }

    UF* createUF() { return new OSProducer(); }
    UFProducer* getProducer() const { return _p; }
    unsigned short int    _bailProducer;

protected:
    UFProducer* _p;
    std::string _hostToConnect;
    std::string _request;
    OSProducer() { _p = new UFProducer(); }
};
//std::string OSProducer::data = "HTTP/1.1 200 OK\r\nContent-Length: 1503\r\nAccept-Ranges: bytes\r\nConnection: Keep-Alive\r\n\r\n" + string(1500u, 'a') + "END";
std::string OSProducer::data = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nAccept-Ranges: bytes\r\nConnection: Keep-Alive\r\n\r\nhello";

unsigned long long int readTimeout = 0;
string hostToConnectTo = "localhost:8888";
void ufTestHTTPServer::handleNewConnection(UFIO* ufio)
{
    UFStatSystem::increment(http_request);
    if(!ufio)
        return;

    UFScheduler* ufs = UFScheduler::getUFScheduler();
    UF* uf = ufs->getRunningFiberOnThisThread();
    char buf[1024];
    int amtRead = 0;
    bool bail = false;
    string readData;
    unsigned static int counter = 0;


    //3. create the producer to OS
    OSProducer* pUF = new OSProducer(hostToConnectTo, readData);
    if(!pUF)
        return;
    UFConsumer* c = new UFConsumer();
    if(!c)
    {
        delete pUF;
        return;
    }
    c->_requireLockToWaitForUpdate = false;
    if(!c->joinProducer(pUF->getProducer()))
    {
        delete pUF;
        delete c;
        return;
    }
    ufs->addFiberToScheduler(pUF);


    while(!bail)
    {
        //1. read from the client
        amtRead = ufio->read(buf, 1023, readTimeout);
        if(amtRead > 0)
        {
            readData.append(buf, amtRead);
            if(readData.find("\r\n\r\n") == string::npos)
                continue;

            /*
            //2. check whether we should be closing the connection after the request is done
            if((readData.find("Connection: close") != string::npos) ||
               ((readData.find("HTTP/1.1") == string::npos) && (readData.find("Connection: Keep-Alive") == string::npos)))
                bail = true;
                */


            int amtWritten = 0;
            while(1)
            {
                UFProducerData* result = c->waitForData(uf);
                if(!result)
                {
                    bail = true;
                    break;
                }

                if(result->_ufpcCode == 0 /* the producer is done */)
                {
                    UFProducerData::releaseObj(result);
                    break;
                }
                else if(result->_ufpcCode == -1 /* the producer inserted an error */)
                {
                    UFProducerData::releaseObj(result);
                    bail = true;
                    break;
                }

                //write out to the client
                amtWritten = ufio->write((char*)result->_data, result->_size);
                if(amtWritten != (int) result->_size)
                {
                    cerr<<"couldnt write out data - wrote "<<amtWritten<<" instead of "<<result->_size<<" "<<strerror(ufio->getErrno())<<endl;
                    UFProducerData::releaseObj(result);
                    bail = true;
                    break;
                }
                UFProducerData::releaseObj(result);
            }

            pUF->_bailProducer = 2;
            ufs->addFiberToScheduler(pUF);

            readData.clear();
            counter++;
        }
        else if(amtRead <= 0)
            break;
    }
    delete c;

    pUF->_bailProducer = 1;
    ufs->addFiberToScheduler(pUF);

    /*
    if(counter > 300000)
        UFScheduler::getUFScheduler()->setExit(true);
        */
}


int main(int argc, char** argv)
{
    unsigned int numThreads = 8;
    unsigned int numProcesses = 1;
    unsigned short int port = 8080;
    if(argc > 1)
        numThreads = atoi(argv[1]);
    if(argc > 2)
        port = atoi(argv[2]);
    if(argc > 3)
        hostToConnectTo = argv[3];
    if(argc > 4)
        readTimeout = atoi(argv[4]);
    if(argc > 5)
        numProcesses = atoi(argv[5]);
    if(argc > 6)
        runOS = atoi(argv[6]);

    ufTestHTTPServer ufhttp(0, port);
    ufhttp.MAX_ACCEPT_THREADS_ALLOWED   = 1;
    ufhttp.MAX_THREADS_ALLOWED          = numThreads;
    ufhttp.MAX_PROCESSES_ALLOWED        = numProcesses;
    ufhttp.UF_STACK_SIZE                = 8192;

    ufhttp.run();

    return 0;
}

void OSProducer::runAsOS()
{
    bool releaseConn = false;
    UFConnectionPool* cpool = UFIOScheduler::getUFIOS()->getConnPool();
    UFIO* sufio = 0;

    bool alreadyWritten = false;
    unsigned int amtRead = 0;
    string dataReadFromOS;
    while(1)
    {
        if(!sufio)
        {
            sufio = cpool->getConnection(_hostToConnect, true);
            if(!sufio)
            {
                cerr<<"couldnt get connection to "<<_hostToConnect<<endl;
                delete _p;
                _p = 0;
                return;
            }
        }

        if(!alreadyWritten && sufio->write(_request.data(), _request.length()) != (int)_request.length())
        {
            cpool->releaseConnection(sufio, releaseConn);
            delete _p;
            _p = 0;
            return;
        }
        alreadyWritten = true;

        char* buf = (char*) malloc (1024);
        amtRead = sufio->read(buf, 1023, 0);
        if(amtRead == 0)
        {
            cpool->releaseConnection(sufio, false);
            sufio = 0;
            alreadyWritten = false;
            continue;
        }
        if(amtRead <= 0)
        {
            cerr<<"error on read "<<strerror(errno)<<" read "<<amtRead<<" prev read "<<dataReadFromOS.length()<<endl;
            free(buf);
            _p->produceData(0, 0, -1, false, this);
            break;
        }
        _p->produceData(buf, amtRead, 1, true, this);

        dataReadFromOS.append(buf, amtRead);
        if(dataReadFromOS.find("END") == string::npos)
            continue;
        releaseConn = true;
        break; //found the end string
    }
    cpool->releaseConnection(sufio, releaseConn);
    delete _p;
    _p = 0;
}

void OSProducer::runAsStaticCache()
{
    unsigned int amtToSendPerAttempt = 4096;
    unsigned int amtToSendThisTime = 0;
    unsigned int amtSent = 0;
    while(1)
    {
        while(amtSent < data.length())
        {
            amtToSendThisTime = (((data.length() - amtSent) > amtToSendPerAttempt) ? amtToSendPerAttempt : (data.length() - amtSent));
            /*
            char* buf = (char*) malloc (amtToSendThisTime);
            memcpy(buf, data.data() + amtSent, amtToSendThisTime);
            _p->produceData(buf, amtToSendThisTime, 1, true, this);
            */
            _p->produceData((void*)(data.data()+amtSent), amtToSendThisTime, 1, false, this);
            amtSent += amtToSendThisTime;
        }
        _p->produceData(0, 0, 0, false, this);
        amtSent = 0;

        this->block();
        if(_bailProducer == 1)
            break;
        _bailProducer = 0;
    }

    delete _p;
    _p = 0;
}
