#ifndef _FACTORY_H_
#define _FACTORY_H_

#include <pthread.h>
#include <map>
#include <iostream>
#include "ObjectPool.H"


template <typename _Type, typename _PoolType>
class Factory;

template<typename _Type>
class CrossThreadFactory;

//----------------------------------------------------------------------------
/**
 * Registry for the different types of factories
 *
 * Normally we would make the table thread safe when initializing it, but since we are only
 * using it in a static it will be called at static initializtion before any threads are created.
 */
class FactoryRegistry
{
public:
  FactoryRegistry() {
    if (_table == NULL) {
      _table = new std::map<const char*, void*>;
    }
  }

  void add(const char *name, void *factory) {
    (*_table).insert(make_pair(name, factory));
  }

  static void display() {
    if (_table != NULL) {
      for (std::map<const char*, void*>::iterator it = _table->begin(); it != _table->end(); ++it) {
        std::cout << "name: " << it->first << ", factory impl pointer: " << it->second << std::endl;
      }
    }
  }

private:
  static std::map<const char*, void*> *_table;
};


//----------------------------------------------------------------------------
/**
 * Factory implimentation class
 *
 * Hide the details of how the impl works incase to have to put the static in the .cc files
 * This can be removed if we don't have to add the static in the .cc files
 */
template <typename _Type, typename _PoolType>
class FactoryImpl
{
  FactoryImpl<_Type, _PoolType>() {
    _registry.add(__PRETTY_FUNCTION__, (void*)this);
  }

  friend class Factory<_Type, _PoolType>;
  friend class CrossThreadFactory<_Type>;
  _PoolType _pool;
  FactoryRegistry _registry;
};


//----------------------------------------------------------------------------
/**
 * Factory template class
 *
 * Uses a pool for each thread so there is no locking after the first alloc() on the thread
 */
template <typename _Type, typename _PoolType = ObjectPoolThreadMapped<_Type> >
class Factory
{
public:
  /**
   * Allocates an object
   *
   * @return Object pointer
   */
  static _Type* alloc() {
    return _impl._pool.alloc();
  }

  /**
   * Frees and object into the pool
   *
   * @param Object pointer
   */
  static void free(_Type *object) {
    _impl._pool.free(object);
  }

  /**
   * Set object recycler
   *
   * @param recycler New recycler object; default will be restored if set to 0
   */
  static void setRecycler(ObjectRecycler<_Type> *recycler) {
    _impl._pool.setRecycler(recycler);
  }

protected:
  static FactoryImpl<_Type, _PoolType> _impl;
};

/// The static for the different type of factories
template<typename _Type, typename _PoolType> FactoryImpl<_Type, _PoolType> Factory<_Type, _PoolType>::_impl;


//----------------------------------------------------------------------------
/**
 * Factory with a global pool
 */
template <typename _Type>
class FactoryGlobal : public Factory <_Type, ObjectPoolLocking<_Type> >
{
};


//----------------------------------------------------------------------------
/**
 * Factory with thread specific pools, but allowing object allocation
 * across threads
 */
template <typename _Type>
class CrossThreadFactory : public Factory<_Type, ObjectPoolThreadMapped<_Type, ObjectPoolLocking<_Type > > >
{
public:
  /**
   * Allocates an object from given thread's pool
   *
   * @param targetThread The thread whose pool to alloc object from
   * @return Object pointer
   */
  static _Type* alloc(const pthread_t &targetThread) {
    return Factory<_Type, ObjectPoolThreadMapped<_Type, ObjectPoolLocking<_Type > > >::_impl._pool.alloc(
        &targetThread);
  }
};


//----------------------------------------------------------------------------
/**
 * Auto pointer like factory that you don't have to remember to free
 */
template <typename _Type, typename _PoolType = ObjectPoolThreadMapped<_Type> >
class AutoFactoryObject
{
public:
  AutoFactoryObject() {
    _item = Factory<_Type, _PoolType>::alloc();
  }

  ~AutoFactoryObject() {
    Factory<_Type, _PoolType>::free(_item);
  }

  _Type& getRef() { return *_item; }

private:
  _Type* _item;
};


//----------------------------------------------------------------------------
/**
 * Auto pointer like factory that you don't have to remember to free
 */
template <typename _Type>
class AutoFactoryGlobalObject : public AutoFactoryObject<_Type, ObjectPoolLocking<_Type> >
{
};


#endif
