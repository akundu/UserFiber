#include <UFAres.H>
#include <stdio.h>

using namespace std;

static void printHost(struct hostent* host , struct ares_addrttl *ttls = 0, int nttl = 0)
{
  //  return;
        int i;
	for(i = 0 ;  host->h_addr_list[i] != NULL; i++)
	{
	   printf("%d.%d.%d.%d\n",(unsigned char)host->h_addr_list[i][0],(unsigned char)host->h_addr_list[i][1],(unsigned char)host->h_addr_list[i][2],(unsigned char)host->h_addr_list[i][3]);
	}
	for(i = 0; host->h_aliases[i]!=NULL;i++)
	   printf("%s\n",host->h_aliases[i]);

        for(i = 0 ; i < nttl ; i++)
	{
	   char *name = inet_ntoa(ttls[i].ipaddr);
	   printf("IP is %s, TTL is %d\n",name,ttls[i].ttl);
	}
	printf("----------------------------------------------------------------------------------\n");
}

static double GetCurrentTime()
{
    timeval now;
    gettimeofday(&now,NULL);
    return((double)(now.tv_sec +(now.tv_usec/1000000.0)));
}
static void mycallback(void *arg, int status,
	               int timeouts, struct hostent *host)
{
	UFAresUFIO *ares = (UFAresUFIO*) arg;
        if(!host)
	{
	    ares->set_myhostent(NULL);
	    return;
	}

	printHost(host);
        UFHostEnt *myhostent = ares->get_myhostent();
	myhostent->set_hostent(host);
	myhostent->set_nttl(0);
	myhostent->set_aresttl(NULL);
	myhostent->set_timestamp(0);
	ares->set_myhostent(myhostent);
}

static void arescallback(void *arg, int status, int  timeouts, unsigned char *abuf, int alen)
{
    UFAresUFIO *ares = (UFAresUFIO*) arg;
    struct hostent *host = NULL;
    struct ares_addrttl *ttls = (ares_addrttl*) malloc(sizeof(ares_addrttl) ); ;
    int nttl = 1;
    UFHostEnt *myhostent;
    if(status == ARES_SUCCESS)
    {
	 status = ares_parse_a_reply(abuf, alen, &host, ttls,&nttl);
	if(!host)
	{
	    ares->set_myhostent(NULL);
	    return;
	}
	printHost(host, ttls,nttl);
        myhostent = ares->get_myhostent();	
	myhostent->set_hostent(host);
	if(nttl>0)
	    myhostent->set_nttl(nttl);
	else
	    myhostent->set_nttl(0);
	myhostent->set_aresttl(ttls);
	myhostent->set_timestamp(GetCurrentTime());
	ares->set_complete(true);
    }
    else
	cerr<<"failure"<<endl;


}
static void mycall(void *data, int fd, int read, int write)
{
	UFAresUFIO *ufio = (UFAresUFIO*) data;
	if(read ==0  && write == 0)
	{
	    ufio->setFd(-1);
	}
	else
	    ufio->setFd(fd);
	if(read)
	{  
	    UFIOScheduler::getUFIOS()->setupForWrite(ufio);
	}
}

UFAresUFIO* UFAres::GetNewAresUFIO()
{
    UFAresUFIO *aresio;
    if( list_ares_ufio_.empty() == true)
    {
	aresio = new UFAresUFIO();
	aresio->Init(mycall,aresio);
    }
    else
    {
	aresio = list_ares_ufio_.front();
	aresio->setFd(-1);
        aresio->set_myhostent((UFHostEnt*)0x00000BAD);
	list_ares_ufio_.pop_front();
	aresio->setUF(UFScheduler::getUF());
    }
    return aresio;
}

void UFAres::ReleaseAresUFIO(UFAresUFIO *aresio)
{
    if(aresio)
    {
	aresio->set_complete(false);
	//aresio->destroy();
	list_ares_ufio_.push_back(aresio);
    }
    else
	cerr<<"did not get valid UFAresUFIO"<<endl;

}


UFHostEnt* UFAres::GetHostByName(const char *name, uint32_t timeout)
{
    UFHostEnt* myhostent_ = GetCachedHostent(name);
    if(myhostent_ == NULL)
    {
	myhostent_ = new UFHostEnt;
	InsertToCache(name,myhostent_);
    }
    myhostent_->lock(UFScheduler::getUF());
    unsigned long int ip = 0;
    if(myhostent_ != NULL)
    {
	ip = myhostent_->GetUnExpiredEntry(GetCurrentTime());
    }
    if(ip != 0)
    {
            myhostent_->unlock(UFScheduler::getUF());
	    return myhostent_;
    }
    else
    {
	myhostent_->ReleaseEntries();

	UFAresUFIO *aresio = GetNewAresUFIO();
	aresio->set_myhostent(myhostent_);
	
	ares_search(aresio->get_channel(),name,1, 1,arescallback,aresio);
	 // process fd did not call mycallback yet 

	while ((aresio->get_complete() == false) && (aresio->getFd()!= -1))
	{
	    UFIOScheduler::getUFIOS()->setupForRead(aresio);
	        // woken up by epoll again, process them 
            ares_process_fd(aresio->get_channel(), aresio->getFd(), aresio->getFd());
	}

    
        // out of loop , either we got hostent or timed out
        myhostent_ = aresio->get_myhostent(); 
        ReleaseAresUFIO(aresio);
        myhostent_->unlock(UFScheduler::getUF());
        return myhostent_;
	
    }
}

struct hostent* UFAres::GetHostByNameDebug(const char *name, uint32_t timeout )
{
    UFAresUFIO *aresio = GetNewAresUFIO();
    aresio->Init(mycall,aresio);
    ares_gethostbyname(aresio->get_channel(),name,AF_INET,mycallback,aresio);  
    UFHostEnt *myhostent_ = aresio->get_myhostent(); 
    if(myhostent_ == (UFHostEnt*)0x00000BAD && aresio->getFd() != -1)
    {
	UFIOScheduler::getUFIOS()->setupForRead(aresio);
    }
    else
    {
	return myhostent_->get_hostent();
    }
    // First time no need to block , blocked by setup for read or write
    ares_process_fd(aresio->get_channel(), aresio->getFd(), aresio->getFd());
    
    //process fd called mycallback and hostent is not null
    myhostent_ = aresio->get_myhostent(); 
    ReleaseAresUFIO(aresio);
    if(myhostent_ && myhostent_ != (UFHostEnt*)0x00000BAD) 
    {
	return myhostent_->get_hostent();
    }
    else 
    {
	// process fd did not call mycallback yet 
	while (aresio->get_myhostent() == (UFHostEnt*)0x00000BAD)
	{
            UFIOScheduler::getUFIOS()->setupForRead(aresio);
	    // woken up by epoll again 
            ares_process_fd(aresio->get_channel(), aresio->getFd(), aresio->getFd());
	}

    }
    
    // out of loop , either we got hostent or timed out
    myhostent_ = aresio->get_myhostent(); 
    ReleaseAresUFIO(aresio);
    return myhostent_->get_hostent();
	
}

   

