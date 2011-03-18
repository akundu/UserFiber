#ifndef __UF_CONF_H__
#define __UF_CONF_H__

#include <string>
#include <vector>
#include <list>
#include <ostream>

#include <iostream>
#include <ext/hash_map>

using namespace std;
namespace std { using namespace __gnu_cxx; }

namespace __gnu_cxx
{
    template<> struct hash< std::string >
    {
        size_t operator()( const std::string& x ) const
        {
            return hash< const char* >()( x.c_str() );
        }
    };
}

/** Base class for conf value type 
 *  The sole purpose of this class is to act as a base class for conf value types
 */
class ConfValueBase
{
public:
    virtual void dump(std::ostream &output)=0;
    virtual ~ConfValueBase() { }
    std::string type;
};

/** Helper for printing out vector
 */   
template <typename T>
std::ostream& operator <<  (std::ostream& output, const std::vector<T> &value)
{
    if(!value.size())
        return output;
    
    for(typename std::vector<T>::const_iterator it = value.begin(); it != value.end(); it++)
        output << *it << " ";
    return output;
}

/** Helper for printing out vector
 */   
template <typename T>
std::ostream& operator <<  (std::ostream& output, const std::list<T> &value)
{
    if(!value.size())
        return output;
    
    for(typename std::list<T>::const_iterator it = value.begin(); it != value.end(); it++)
        output << *it << " ";
    return output;
}

/** Template conf value 
 *  This covers all built in types.
 *  This is used by UFConf for string, int, bool and double
 *  For specialized conf value types, derive from ConfValueBase
 */
template <class T>
class ConfValue : public ConfValueBase {
public:
    T mElement;

    void dump(std::ostream& output) { output << mElement; }
    friend std::ostream& operator <<  (std::ostream& output, const ConfValue<T>& value)
    {
        output << value.mElement;
        return output;
    }
};

/** Get conf value from ConfValueBase
 *  This is a helper function which makes it easier to get conf values
 *  _testBool = &(((ConfValue<bool> *)conf_val)->mElement) will now become _testBool = confValueGet<bool>(conf_val)
 */
template <class T>
T* confValueGet(ConfValueBase *conf_val)
{
    return &(((ConfValue<T> *)conf_val)->mElement);
}

/// Forward declaration
class UFConfManager;

/** Holds config data for a given file
 *  The class has a parent conf variable.
 *  the get* functions look at _data. If the requested key is not found, they lookup the parent
 */
class UFConf
{
    friend class UFConfManager;
public:
    UFConf(const std::string &conf_file);
    void init();
    ConfValueBase *get(const std::string &key);
    int *getInt(const std::string &key);
    double *getDouble(const std::string &key);
    std::string *getString(const std::string &key);
    bool *getBool(const std::string &key);
    std::vector<std::string> *getStringVector(const std::string &key);

    void setInt(const std::string &key, const std::string &key_type, int value);
    void setDouble(const std::string &key, const std::string &key_type, double value);
    void setString(const std::string &key, const std::string &key_type, const std::string &value);
    void setBool(const std::string &key, const std::string &key_type, bool value);
    void setStringVector(const std::string &key, const std::string &key_type, const std::vector<std::string> &value);
    
    bool parse(const std::string &conf_file);
    virtual bool parseLine(const std::string &line);
    virtual void clear();
    virtual void cache(const std::string &conf_key, ConfValueBase *conf_val) { }
    virtual void afterParse() { }
    virtual ~UFConf() { }

    std::string conf_file;

    friend std::ostream& operator<<(std::ostream& output, const UFConf &conf);
    
protected:
    template<typename T>
    void _set(const std::string &conf_key, const std::string &conf_key_type, const T &value)
        {
            // Check if a value already exists for the key
            ConfValueBase *existingValue = get(conf_key);
            if(existingValue != NULL) {
                delete existingValue;
            }
            
            // Create new value. Set type. Copy value.
            ConfValue<T> *confValue = new ConfValue<T>;
            confValue->type = conf_key_type;
            confValue->mElement = value;
            _data[conf_key] = confValue;

            // call cache so that derived classes can cache whatever value they need to
            cache(conf_key, confValue);
        }

    template<typename T>
    T* _get(const std::string &key)
        {
            ConfValue<T> *confValue = (ConfValue<T> *)get(key);
            if(confValue != NULL) {
                return &confValue->mElement;
            }
            if(_parent == NULL)
                return NULL;
            return _parent->_get<T>(key);
        }
        
    void _setParent(UFConf *parent);
    
    UFConf *_parent;
    std::hash_map<std::string, ConfValueBase *> _data;
};

/** Factory to create conf objects
 *  This is needed because the cache method is virtual and needs to be called from the constructor
 */   
template< class T >
T* UFConfFactory(const std::string &conf_file)
{
    T* confObject = new T(conf_file);
    confObject->init();

    return confObject;
}

/** Manages config objects in the system 
 * 
 */
class UFConfManager
{
public:
    UFConfManager();
    void reload();
    UFConf* addChildConf(const std::string &conf_file, const std::string &parent_conf_file="/home/y/conf/UF/uf.conf");
    UFConf* addConf(const std::string &conf_file);
    bool addConf(UFConf *conf, const string &conf_file);
    bool addChildConf(UFConf *child_conf, const string &conf_file, const string &parent_conf_file="/home/y/conf/UF/uf.conf");
    UFConf* getConf(const std::string &conf_file);
    void dump();

    static long getRefreshTime();
    static void setRefreshTime(long);
    static UFConfManager* getConfManager();
    
    static pthread_key_t threadSpecificKey;
    
private:
    /// force children to pickup changes from a parent
    void _reloadChildren(UFConf *conf);

    /// add conf file to list of files being monitored for changes
    void _addWatch(const std::string &conf);

    /// conf object associated with a conf file
    std::hash_map<std::string, UFConf *> _configs;

    /// list of children for a given conf
    std::hash_map<std::string, vector<std::string> > _child_map;

    /// map of fds being monitored for changes to the filename
    std::hash_map<int, std::string> _watch_fd_map;
    int _notify_fd;

    /// Time between checking for conf file changes
    static long _refreshTime;

    static pthread_key_t _createThreadKey();
};

#endif
