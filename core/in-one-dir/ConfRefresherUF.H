#ifndef __CONF_REFRESHER_UF__
#define __CONF_REFRESHER_UF__
#include <uf/core/UF.H>
#include <uf/core/UFConf.H>

struct ConfRefresherUF : public UF
{
    void run();

    ConfRefresherUF(bool registerMe = false)
    {
        if(registerMe)
            _myLoc = UFFactory::getInstance()->registerFunc((UF*)this);
    }
    UF* createUF() { return new ConfRefresherUF(); }
    static ConfRefresherUF* _self;
    static int _myLoc;
};

#endif
