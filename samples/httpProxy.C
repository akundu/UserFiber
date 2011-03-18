#include <iostream>
#include <stdlib.h>

#include "UFIO.H"
#include "UFServer.H"
#include "UFConnectionPool.H"
using namespace std;

struct HTTPProxy : public UFServer
{
    HTTPProxy(char* interfaceIP, unsigned int port)
    {
        _addressToBindTo = interfaceIP ? interfaceIP : "";
        _port = port;
    }
    void handleNewConnection(UFIO* ufio);
};

bool handleIO(UFIO* input, UFIO* output, char* buf, unsigned int maxLen)
{
    int amtRead = 0;
    if((amtRead = input->read(buf, maxLen)) > 0)
    {
        if(output->write(buf, amtRead) == amtRead)
            return true;
    }
    return false;
}

string hostToConnectTo = "localhost:8888";
unsigned long long int readTimeout = 0;
void HTTPProxy::handleNewConnection(UFIO* ufio)
{
    if(!ufio)
    {
        cerr<<"couldnt create UFIO object"<<endl;
        return;
    }

    UFConnectionPool* cpool = UFIOScheduler::getUFIOS()->getConnPool();
    UFIO *sufio = cpool->getConnection(hostToConnectTo.c_str(), true);
    if(!sufio)
    {
        cerr<<"didnt get connection"<<endl;
        return;
    }

    char buf[2048];
    list<UFIO*> ufioList;
    ufioList.push_back(ufio);
    ufioList.push_back(sufio);
    list<UFIO*>::iterator beg;
    while(1)
    {
        //poll to see which connection has some activity on it
        if(!UFIOScheduler::getUFIOS()->rpoll(ufioList, readTimeout))
        {
            cerr<<"error in getting poll"<<endl;
            break;
        }
        if(ufio->_markedActive) //client had some activity
        {
            if(!handleIO(ufio, sufio, buf, 2047))
                break;
        }
        if(sufio->_markedActive) //client had some activity
        {
            if(!handleIO(sufio, ufio, buf, 2047))
                break;
        }
    }

    cpool->releaseConnection(sufio, false);
}

int main(int argc, char** argv)
{
    unsigned int numThreads = 1;
    unsigned int numProcesses = 0;
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

    HTTPProxy ufecho(0, port);
    ufecho.MAX_ACCEPT_THREADS_ALLOWED   = 1;
    ufecho.MAX_THREADS_ALLOWED          = numThreads;
    ufecho.MAX_PROCESSES_ALLOWED        = numProcesses;

    ufecho.run();

    return 0;
}
