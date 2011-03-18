#include "UFConf.H"
#include <iostream>

using std::cerr;
using std::endl;

int main(int argc, char **argv)
{
    if(argc < 3) 
    {
        cerr << "Usage : " << argv[0] << " <base-conf-file> <child-conf-file> [<conf-key>]*" << endl;
        return 0;
    }
    string base_conf_file = argv[1];
    string child_conf_file = argv[2];

    // Add base conf
    UFConfManager::addConf(base_conf_file);

    // Add child conf with base_conf as parent
    UFConfManager::addChildConf(child_conf_file, base_conf_file);

    // Get base conf and child conf
    UFConf* base_conf = UFConfManager::getConf(base_conf_file);
    UFConf* child_conf = UFConfManager::getConf(child_conf_file);
  
    // Go through rest of command line and print out config value as seen through the base and child configs
    for(int i = 3; i <argc; i++)
    {
        string key = argv[i];
        ConfValueBase *base_conf_val = base_conf->get(key);
        ConfValueBase *child_conf_val = child_conf->get(key);
        cerr << "Key=" << key << " Base=";
        if(base_conf_val)
            base_conf_val->dump(cerr);
        cerr << " Child=";
        if(child_conf_val)
            child_conf_val->dump(cerr);
        cerr << endl;
    }

    return 0;
}
