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

using namespace std;

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

unsigned long long int readTimeout = 0;
string hostToConnectTo = "localhost:8888";
void ufTestHTTPServer::handleNewConnection(UFIO* ufio)
{
    UFStatSystem::increment(http_request);
    if(!ufio)
        return;

    UFConnectionPool* cpool = UFIOScheduler::getUFIOS()->getConnPool();
    UFIO *sufio = cpool->getConnection(hostToConnectTo.c_str(), true);
    if(!sufio)
    {
        cerr<<"didnt get connection to "<<hostToConnectTo<<endl;
        return;
    }

    char buf[1024];
    int amtRead = 0;
    bool bail = false;
    string readData;
    bool releaseConn = false;
    while(!bail)
    {
        //1. read from the client
        amtRead = ufio->read(buf, 1023, readTimeout);
        if(amtRead > 0)
        {
            readData.append(buf, amtRead);
            if(readData.find("\r\n\r\n") == string::npos)
                continue;

            //2. check whether we should be closing the connection after the request is done
            if((readData.find("Connection: close") != string::npos) ||
               ((readData.find("HTTP/1.1") == string::npos) && (readData.find("Connection: Keep-Alive") == string::npos)))
                bail = true;

            //3. write to the OS
            if(sufio->write(readData.data(), readData.length()) != (int)readData.length())
                break;
            readData.clear();

            //4. read from the OS
            string dataReadFromOS;
            while(1)
            {
                amtRead = sufio->read(buf, 1023, readTimeout);
                if(amtRead <= 0)
                {
                    bail = true;
                    break;
                }

                dataReadFromOS.append(buf, amtRead);
                if(dataReadFromOS.find("END") == string::npos)
                    continue;
                break; //found the end string
            }

            //5. write to the client what we've read from the OS
            if(ufio->write(dataReadFromOS.data(), dataReadFromOS.length()) != (int)dataReadFromOS.length())
                break;
        }
        else if(amtRead <= 0)
        {
            releaseConn = true;
            break;
        }
    }

    cpool->releaseConnection(sufio, releaseConn);
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

    ufTestHTTPServer ufhttp(0, port);
    ufhttp.MAX_ACCEPT_THREADS_ALLOWED   = 1;
    ufhttp.MAX_THREADS_ALLOWED          = numThreads;
    ufhttp.MAX_PROCESSES_ALLOWED        = numProcesses;
    ufhttp.UF_STACK_SIZE                = 8192;

    ufhttp.run();

    return 0;
}
