#ifndef UF_ARES_H
#define UF_ARES_H

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <ares.h>
#include "UFDNS.H"
#include "UFHostEnt.H"
#include <iostream>

using namespace std;

// for ares less then version 1.7.0
//#ifndef ares_addrttl
//#define ares_addrttl addrttl
//#define ares_addr6ttl addr6ttl
//#endif

typedef void (*my_sock_state_cb)(void *data, int s, int read, int write) ;


typedef hash_map< const char *, UFHostEnt * , hash<const char*> > HostentHashMap;

class UFAresUFIO : public UFIO
{
    public:
	UFAresUFIO() : myhostent_((UFHostEnt*)0x00000BAD) 
	{
	    _uf = UFScheduler::getUF();
	};
	
	~UFAresUFIO()
	{
	    destroy();
	    cerr<<"aresio destructor called"<<endl;
	};
        
	void set_myhostent(UFHostEnt *h) 
	{ 
	    myhostent_ = h; 
	};
	
	void Init(my_sock_state_cb callback, UFAresUFIO *aresio) 
	{ 
	    op_.sock_state_cb =callback ; 
	    op_.sock_state_cb_data = aresio;
	    ares_init_options(&ch_, &op_,ARES_OPT_SOCK_STATE_CB); 
	};
	
	UFHostEnt *get_myhostent()
	{
	    return myhostent_;
	};

	ares_channel get_channel()
	{
	    return ch_;
	};

	void set_complete(bool flag)
	{
	    complete_ = flag;
	}

	bool get_complete()
	{
	    return complete_;
	}
       
	void destroy()
	{
	    ares_destroy_options( &op_);
	    ares_destroy(ch_);
        }

    private:	
	ares_channel ch_;
	struct ares_options op_;
//	struct hostent *hostent_;
//	struct ares_addrttl *aresttl_;;
//	struct nttl_;
        UFHostEnt *myhostent_;
	bool complete_;

};



class UFAres : public UFDNS
{
    public:
	 UFAres()  { };
	 ~UFAres() { };
	 UFHostEnt* GetHostByName(const char *name, uint32_t timeout = 0);
	 hostent *GetHostByNameDebug(const char *name, uint32_t timeout = 0);
	 hostent *GetHostByAddr(uint32_t ip , uint32_t timeout = 0, uint32_t family = AF_INET) { return NULL; };
	 hostent *GetSrvByName(const char *name, uint32_t timeout = 0) {  return NULL; };
	 UFAresUFIO* GetNewAresUFIO();
	 void ReleaseAresUFIO( UFAresUFIO* ares);
	 UFHostEnt* GetCachedHostent(const char* name)
	 {
	     //cerr<<"size in get"<<host_map_.size()<<endl;
	     HostentHashMap::const_iterator index = host_map_.find(name);
	     if(index == host_map_.end())
		 return NULL;
	     else
		 return const_cast<UFHostEnt*>(index->second);
	 }
	 void InsertToCache(const char* name,UFHostEnt *host)
	 {
	     //cerr<<"size in inset"<<host_map_.size()<<endl;
	     host_map_[name] = host;
	     //cerr<<" after size in inset"<<host_map_.size()<<endl;
	 }


    private:
	 list<UFAresUFIO*> list_ares_ufio_;
	 HostentHashMap host_map_;
};

#endif
