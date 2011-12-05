#ifndef MY_HOSTENT
#define MY_HOSTENT

#include<ares.h>

// for ares less then version 1.7.0
//#ifndef ares_addrttl
//#define ares_addrttl addrttl
//#define ares_addr6ttl addr6ttl
//#endif

class  UFHostEnt 
{
    public :
	UFHostEnt()   { aresttl_ = NULL; hostent_ = NULL ; nttl_ = 0 ; timestamp_ = 0; };

	~UFHostEnt () { ReleaseEntries();  };

	hostent* get_hostent()
	{
	    return hostent_;
	}

	void set_hostent(hostent* host)
	{
	    hostent_ = host;
	}

	ares_addrttl* get_aresttl()
	{
	    return aresttl_;
	}

	void set_aresttl(ares_addrttl* aresttl)
	{
	    aresttl_ = aresttl;
	}

	uint32_t get_nttl()
	{
	    return nttl_;
	}

	void set_nttl(uint32_t nttl)
	{
	    nttl_ = nttl;
	}

	double  get_timestamp()
	{
	    return timestamp_;
	}

	void set_timestamp(double  timestamp)
	{
	    timestamp_ = timestamp;
	}

	bool IsEntryExpired( double  current_timestamp, uint32_t ttl)
	{
	    if(((timestamp_ + ttl) - current_timestamp) <= 0)
		return true ; 
	    
	    return false;
	}

	unsigned long int GetUnExpiredEntry(double  current_timestamp)
	{
	    for(unsigned int i = 0 ; i < nttl_ ; i ++)
	    {
		if(IsEntryExpired(current_timestamp,aresttl_[i].ttl) == false)
		//if(IsEntryExpired(current_timestamp,1) == false)
		    return aresttl_[i].ipaddr.s_addr;
	    }
	    return 0;
	}

	unsigned long int GetFirstIP()
	{
	   if(aresttl_ !=NULL)
	   return aresttl_[0].ipaddr.s_addr;
	   else 
	       return 0;
	}
	void ReleaseEntries()
	{
	    if(hostent_ != NULL)
		ares_free_hostent(hostent_);
	    if(aresttl_ != NULL)
		free(aresttl_);
	}

	void lock(UF * uf)
	{
	    mutex_.lock(uf);
	}

	void unlock(UF *uf)
	{
	    mutex_.unlock(uf);
	}


    private:
        
        UFMutex mutex_; 
        struct hostent *hostent_;
        struct ares_addrttl *aresttl_;
        uint32_t nttl_;
        double  timestamp_;
};

#endif
