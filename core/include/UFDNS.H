#ifndef UF_DNS_H
#define UF_DNS_H

#include <UF.H>
#include <UFIO.H>
class UFHostEnt;
class UFDNS : public UFIO
{
   public:
       UFDNS()
       {
	   _uf = UFScheduler::getUF();
	   timeout_msec_ = 5000;
	   tries_ = 4;
	   port_ = 53;
	   tcp_=false;
	  
       }
       virtual ~UFDNS() { };
       
       virtual struct hostent *GetHostByNameDebug(const char *name, uint32_t timeout = 0) = 0 ;
       virtual UFHostEnt* GetHostByName(const char *name, uint32_t timeout = 0) = 0;
       virtual struct hostent *GetHostByAddr(uint32_t ip , uint32_t timeout = 0, uint32_t family = AF_INET) = 0 ;
       virtual struct hostent *GetSrvByName(const char *name, uint32_t = 0) = 0 ;
       
   protected:
       uint32_t timeout_msec_;
       uint32_t tries_;
       uint32_t port_;
       bool     tcp_;
       
       
};

#endif
