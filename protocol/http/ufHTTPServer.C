#include <iostream>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string.h>

#include "UF.H"
#include "UFIO.H"
#include "UFServer.H"
#include "UFStatSystem.H"

using namespace std;

static uint32_t http_request;
struct HTTPServer : public UFServer
{
    HTTPServer(char* interfaceIP, unsigned int port)
    {
        _addressToBindTo = interfaceIP ? interfaceIP : "";
        _port = port;
    }
    void handleNewConnection(UFIO* ufio);
    void preAccept() { UFStatSystem::registerStat("http_request", &http_request, false); }
};

const char* HTTP_ANSWER = "HTTP/1.0 200 OK\r\nConnection: keep-alive\r\nCache-Control: private, max-age=0\r\nContent-Type: text/html; charset=ISO-8859-1\r\nContent-Length: 5\r\n\r\nhello";
const unsigned int HTTP_ANSWER_LENGTH = strlen(HTTP_ANSWER);
unsigned int counter = 0;
unsigned long long int readTimeout = 0;
void HTTPServer::handleNewConnection(UFIO* ufio)
{
    UFStatSystem::increment(http_request);
    if(!ufio)
    {
        return;
    }

    char buf[256];
    int amtRead = 0;
    int amtWritten = 0;
    bool bail = false;
    size_t n = 0;
    string readData;
    while(!bail)
    {
        //3. setup for read
        amtRead = ufio->read(buf, 255, readTimeout);
        if(amtRead > 0)
        {
            readData.append(buf, amtRead);
            if(readData.find("\r\n\r\n") == string::npos)
                continue;

            //4. write what we've read
            int amtToWrite = HTTP_ANSWER_LENGTH; //amtRead
            char* bufToWrite = const_cast<char*>(HTTP_ANSWER); //buf
            amtWritten = 0;
            while(1)
            {
                n = ufio->write(bufToWrite+amtWritten, amtToWrite-amtWritten);
                if(n <= 0)
                {
                    if(errno == EINTR)
                        continue;
                    bail = true;
                    break;
                }
                amtWritten += n;
                if(amtWritten == amtToWrite)
                    break;
                else if(amtWritten > amtToWrite)
                {
                    bail = true;
                    break;
                }
            }

            readData.clear();
        }
        else if(amtRead <= 0)
            break;
    }
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
        readTimeout = atoi(argv[3]);
    if(argc > 4)
        numProcesses = atoi(argv[4]);

    cerr<<"setting readtimeout = "<<readTimeout<<endl;

    HTTPServer ufhttp(0, port);
    ufhttp.MAX_ACCEPT_THREADS_ALLOWED   = 1;
    ufhttp.MAX_THREADS_ALLOWED          = numThreads;
    ufhttp.MAX_PROCESSES_ALLOWED        = numProcesses;
    ufhttp.UF_STACK_SIZE                = 8192;

    ufhttp.run();

    return 0;
}
