#include <stdlib.h>
#include "UFIO.H"
#include "UFServer.H"

unsigned long long int readTimeout = 0;
struct EchoServer : public UFServer
{
    EchoServer(char* interfaceIP, unsigned int port)
    {
        _addressToBindTo = interfaceIP ? interfaceIP : "";
        _port = port;
    }
    void handleNewConnection(UFIO* ufio)
    {
        if(!ufio) return;

        char buf[256];
        int amtRead = 0;
        while ( ((amtRead = ufio->read(buf, 255, readTimeout)) > 0) && (ufio->write(buf, amtRead) == amtRead) ) {}
    }
};

int main(int argc, char** argv)
{
    unsigned int numThreads = 8;
    unsigned short int port = 8080;
    unsigned int numProcesses = 1;
    if(argc > 1)
        numThreads = atoi(argv[1]);
    if(argc > 2)
        port = atoi(argv[2]);
    if(argc > 3)
        readTimeout = atoi(argv[3]);
    if(argc > 4)
        numProcesses = atoi(argv[4]);

    EchoServer ufecho(0, port);
    ufecho.MAX_ACCEPT_THREADS_ALLOWED   = 1;
    ufecho.MAX_THREADS_ALLOWED          = numThreads;
    ufecho.MAX_PROCESSES_ALLOWED        = numProcesses;
    ufecho.UF_STACK_SIZE                = 8192;

    ufecho.run();
    return 0;
}
