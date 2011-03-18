#include <UFConf.H>
#include <UFIO.H>
#include <fstream>
#include <iostream>
#include <sstream>
#include <errno.h>
#include <string.h>

#include <fcntl.h>
#include <sys/inotify.h>

UFConf::UFConf(const string &conf) : conf_file(conf), _parent(NULL)
{

}

void UFConf::init()
{
    // Parse default config
    string conf_file_default = conf_file + ".default";
    parse(conf_file_default);
    
    // Parse overrides
    parse(conf_file);
/*
    string *conf_file_parent = getString("parent");
    if(conf_file_parent != NULL) 
    {
        _setParent(getConf(*conf_file_parent));
    }
*/
}

/** set string value in conf
 *  Converts the string value that is passed in to a ConfValue and stores it in the conf hash
 */
void UFConf::setString(const string &key, const string &type, const string &value)
{
    _set<string>(key, type, value);
}

/** set int value in conf
 *  Converts the int value that is passed in to a ConfValue and stores it in the conf hash
 */
void UFConf::setInt(const string &key, const string &type, int value)
{
    _set<int>(key, type, value);
}

/** set bool value in conf
 *  Converts the bool value that is passed in to a ConfValue and stores it in the conf hash
 */
void UFConf::setBool(const string &key, const string &type, bool value)
{
    _set<bool>(key, type, value);
}

/** set double value in conf
 *  Converts double value that is passed in to a ConfValue and stores it in the conf hash
 */
void UFConf::setDouble(const string &key, const string &type, double value)
{
    _set<double>(key, type, value);
}

/** set vector<string> value in conf
 *  Converts the vector<string> value that is passed in to a ConfValue and stores it in the conf hash
 */
void UFConf::setStringVector(const string &key, const string &type, const vector<string> &value)
{
    _set< vector<string> >(key, type, value);
}

/** Get the string value associated with the key
 *  Looks at local conf hash for the key
 *  If key is not found in the local conf, forwards request to parent conf
 *  If key is not found in either local or parent conf, NULL is returned
 */
string *UFConf::getString(const string &key)
{
    return _get<string>(key);
}

/** Get the int value associated with the key
 *  Looks at local conf hash for the key
 *  If key is not found in the local conf, forwards request to parent conf
 *  If key is not found in either local or parent conf, NULL is returned
 */
int *UFConf::getInt(const string &key)
{
    return _get<int>(key);
}

/** Get the bool value associated the with key
 *  Looks at local conf hash for the key
 *  If key is not found in the local conf, forwards request to parent conf
 *  If key is not found in either local or parent conf, NULL is returned
 */
bool *UFConf::getBool(const string &key)
{
    return _get<bool>(key);
}

/** Get the double value associated with the key
 *  Looks at local conf hash for the key
 *  If key is not found in the local conf, forwards request to parent conf
 *  If key is not found in either local or parent conf, NULL is returned
 */
double *UFConf::getDouble(const string &key)
{
    return _get<double>(key);
}

/** Get the vector<string> value associated with the key
 *  Looks at local conf hash for the key
 *  If key is not found in the local conf, forwards request to parent conf
 *  If key is not found in either local or parent conf, NULL is returned
 */
vector<string> *UFConf::getStringVector(const string &key)
{
    return _get< vector<string> >(key);
}

/** get value associated with the key that is passed in
 *  Looks at local conf hash for the key
 *  If key is not found in the local conf, forwards request to parent conf
 *  If key is not found in either local or parent conf, NULL is returned
 */
ConfValueBase *UFConf::get(const string &key)
{
    hash_map<string, ConfValueBase *>::iterator it = _data.find(key);
    if(it != _data.end())
        return it->second;
    if(_parent == NULL)
        return NULL;
    return _parent->get(key);
}

/** Set parent config
 *  Also caches any config parameters that the child may be interested in
 */
void UFConf::_setParent(UFConf *parent)
{
    _parent = parent;
    for(hash_map<string, ConfValueBase *>::iterator it = parent->_data.begin();
        it != parent->_data.end(); it++)
    {
        cache(it->first, it->second);
    }
}

/** Parse config file and store in conf hash
 *  calls parseLine for each line in the conf file   
 */
bool UFConf::parse(const std::string &conf_file)
{
    ifstream infile;
    infile.open(conf_file.c_str());
    if(!infile.is_open()) 
        return false; // Could not open file
    string line;
    bool ok = true;
    while(getline(infile, line))
    {
        // Ignore lines starting with '#'
        if(line[0] != '#')
            ok &= parseLine(line);
    }
    infile.close();
    afterParse();
    return ok;
}

/** Parses config line
 *  Looks for config values of type STRING, INT, DOUBLE and BOOL
 *  Skips over lines beginning with '#'
 */
bool UFConf::parseLine(const std::string &line)
{
    istringstream instream;
    instream.str(line);  // Use s as source of input.
    string conf_key, conf_key_type;
    if (instream >> conf_key >> conf_key_type) 
    {
        // get type from config file, read into corresponding value and store  
        string string_value;
        int int_value;
        double double_value;
        bool bool_value;
        if(conf_key_type == "STRING")
        {
            string string_val_temp;
            while(instream >> string_val_temp) 
            {
                    if(string_value.length())
                        string_value += " ";
                    string_value += string_val_temp;
            }
            setString(conf_key, conf_key_type, string_value);
        }
        if(conf_key_type == "INT")
        {
            if(instream >> int_value)
                setInt(conf_key, conf_key_type, int_value);
        }
        if(conf_key_type == "DOUBLE")
        {
            if(instream >> double_value)
                setDouble(conf_key, conf_key_type, double_value);
        }
        if(conf_key_type == "BOOL")
        {
            if(instream >> bool_value)
                setBool(conf_key, conf_key_type, bool_value);
        }
    }
    return true;
}

void UFConf::clear()
{
    for(std::hash_map<std::string, ConfValueBase *>::iterator it = _data.begin(); it != _data.end(); it++)
    {
        if(it->second != NULL)
            delete it->second;
    }
    _data.clear();
}

/**
 *  Dump out config
 */
ostream& operator<<(ostream& output, const UFConf &conf)
{

    for(std::hash_map<std::string, ConfValueBase *>::const_iterator it = conf._data.begin();
        it != conf._data.end();
        it++)
    {
        cerr << it->first << " ";
        it->second->dump(cerr);
        cerr << endl;
    }
    return output;
}

UFConfManager::UFConfManager() : _notify_fd(-1)
{
    _notify_fd = inotify_init();
    if(_notify_fd >= 0)
    {
        int flags = ::fcntl(_notify_fd, F_GETFL, 0);
        if(flags != -1)
            ::fcntl(_notify_fd, F_SETFL, flags | O_NONBLOCK);
    }
}

// Default refresh time is 30 mins
long UFConfManager::_refreshTime = 30000000;
long UFConfManager::getRefreshTime()
{
    return _refreshTime;
}

void UFConfManager::setRefreshTime(long rtime)
{
    _refreshTime = rtime;
}

pthread_key_t UFConfManager::_createThreadKey()
{
    if(pthread_key_create(&UFConfManager::threadSpecificKey, 0) != 0)
    {
        cerr<<"couldnt create thread specific key "<<strerror(errno)<<endl;
        exit(1);
    }
    return UFConfManager::threadSpecificKey;
}
pthread_key_t UFConfManager::threadSpecificKey = UFConfManager::_createThreadKey();

UFConfManager* UFConfManager::getConfManager()
{
    void * ret = pthread_getspecific(threadSpecificKey);
    return (UFConfManager*) ret;
}

// FIXME : Random number picked from example code
const int BUFSZ = 1024;

void UFConfManager::reload()
{
#ifdef __linux__
    // Wrap inotify fd in UFIO
    UFIO* ufio = new UFIO(UFScheduler::getUF());
    if(!ufio || !ufio->setFd(_notify_fd, false/*has already been made non-blocking*/))
    {
        cerr<<getPrintableTime()<<" "<<getpid()<<":couldnt setup conf refresher" << endl;
        return;
    }

    // Check for changes from inotify
    while(1)
    {
        char buf[BUFSZ];
        int n = ufio->read(buf, sizeof(buf));
        if (n <= 0) 
        {
            continue;
        }
    
        // Go over changes
        // Collect confs that were changed
        vector<UFConf *> files_changed;
        int i = 0;
        while (i < n) 
        {
            struct inotify_event *ev;
            ev = (struct inotify_event *) &buf[i];
    
            if (ev && ev->wd && (ev->mask & IN_MODIFY))
            {
                // Check if the file that was modified is in the conf system
                std::hash_map<std::string, UFConf *>::iterator it = _configs.find(_watch_fd_map[ev->wd]);
                if(it == _configs.end())
                    continue; // This should never happen
    
                // cerr <<getPrintableTime()<<" "<<getpid()<< "Reloading conf : " << it->first << endl;
    
                // Conf found in system. Reparse.
                it->second->clear();
                it->second->parse(it->first);
    
                files_changed.push_back(it->second);
            }
            i += sizeof(struct inotify_event) + ev->len;
        }
   
       
        // cerr <<getPrintableTime()<<" "<<getpid()<< "file_changed size : " << files_changed.size() << endl;
        for(vector<UFConf *>::iterator it = files_changed.begin(); it != files_changed.end(); it++)
        {
            _reloadChildren(*it);
        }
    }
#else
    // system does not support inotify.
    // brute force. 
    // reparse all configs.
    for(std::hash_map<std::string, UFConf *>::iterator it = _configs.begin(); it != _configs.end(); it++)
    {
        it->second->clear();
        it->second->parse(it->first);
    }

    // reload children for every conf object
    for(std::hash_map<std::string, UFConf *>::iterator it = _configs.begin(); it != _configs.end(); it++)
    {
        _reloadChildren(it->second);
    }
#endif
}

void UFConfManager::_reloadChildren(UFConf *parent_conf)
{
    // Get children
    hash_map< string, vector<string> >::iterator child_map_it = _child_map.find(parent_conf->conf_file);
    if(child_map_it == _child_map.end())
        return; // Leaf node

    vector<string> &children = child_map_it->second;
    // Go over children and make them pickup changes from parent
    for(vector<string>::iterator vit = children.begin(); vit != children.end(); vit++)
    {
        std::hash_map<std::string, UFConf *>::iterator conf_map_it = _configs.find(*vit);
        if(conf_map_it == _configs.end())
            continue;
        conf_map_it->second->_setParent(parent_conf);

        // Recursively reload children
        _reloadChildren(conf_map_it->second);
    }
}

/** Add new child conf
 *  Create new conf and set parent to conf object corresponding to the parent_conf that is passed in
 */
UFConf* UFConfManager::addChildConf(const string &conf_file, const string &parent_conf_file)
{
    hash_map<string, UFConf*>::iterator it = _configs.find(conf_file);
    if(it != _configs.end())
    {
        // Conf already exists
        return it->second;
    }

    // Create conf
    UFConf *conf_created = addConf(conf_file);

    // Check if parent config was created
    it = _configs.find(parent_conf_file);
    if(it == _configs.end())
    {
        cerr << "Warning : Parent config was not created. Not setting parent conf" << endl;
    }
    else 
    {
        // Set parent conf
        if(conf_created != NULL)
        {
            conf_created->_setParent(it->second);
            _child_map[parent_conf_file].push_back(conf_file);
        }
    }
    return conf_created;
}

/** Add new conf
 *  Creates new conf, sets parent if 'parent' key is present and stores in the conf in the config system
 */
UFConf* UFConfManager::addConf(const string &conf_file)
{
    hash_map<string, UFConf*>::iterator it = _configs.find(conf_file);
    if(it != _configs.end())
    {
        // Conf already exists
        return it->second;
    }

    // Create new UFConf
    UFConf *conf = UFConfFactory<UFConf>(conf_file);
    
    // Store in conf map
    _configs[conf_file] = conf;

    // inotify watch file for changes
    _addWatch(conf_file);
    return conf;
}

/** Add new conf
 *  Uses the conf that is passed in
 */
bool UFConfManager::addConf(UFConf *conf, const string &conf_file)
{
    hash_map<string, UFConf*>::iterator it = _configs.find(conf_file);
    if(it != _configs.end())
    {
        // Conf already exists
        return false;
    }

    // Store in conf map
    _configs[conf_file] = conf;

    // inotify watch file for changes
    _addWatch(conf_file);
    return true;
}

/** Add new child conf
 *  Add conf object to manager and set parent
 */
bool UFConfManager::addChildConf(UFConf *child_conf, const string &conf_file, const string &parent_conf_file)
{
    if(!addConf(child_conf, conf_file))
        return false;
    _child_map[parent_conf_file].push_back(conf_file);
    child_conf->_setParent(getConf(parent_conf_file));
    return true;
}

/**
 *  Get config object pointer associated with the conf file that is passed in
 */
UFConf* UFConfManager::getConf(const string &conf_file)
{
    hash_map<string, UFConf *>::iterator it = _configs.find(conf_file);
    if(it == _configs.end())
        return NULL; // config was not created
    return it->second;
}

/**
 *  Print out all configs in the system to cerr
 */
void UFConfManager::dump()
{
    for(hash_map<string, UFConf *>::iterator it = _configs.begin();
        it != _configs.end();
        it++)
    {
        cerr << "=============CONF " << it->first << " STARTS" << endl;
        cerr << *(it->second);
        cerr << "=============CONF " << it->first << " ENDS" << endl;
    }
}

void UFConfManager::_addWatch(const string &conf_file)
{
    if(_notify_fd < 0)
    {
        cerr << "UFConfManager::_addWatch could not add watch on " << conf_file << endl;
        return;
    }

    int wd = inotify_add_watch(_notify_fd, conf_file.c_str(), IN_MODIFY);
    if(wd < 0)
        cerr << "UFConfManager::_addWatch error adding watch on " << conf_file << endl;

    _watch_fd_map[wd] = conf_file;
    // cerr << "UFConfManager::_addWatch watch on " << conf_file << endl;
}
