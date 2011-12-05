#ifndef _OBJECTPOOL_H_
#define _OBJECTPOOL_H_

#include <stack>
#include <vector>
#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>

#define CACHE_LINE_SIZE 64
using namespace std;


//----------------------------------------------------------------------------
/**
 * Object wrapper for cache line alignment
 *
 * Aligns the object to the cache line.  Can greatly improve performace on multi-threaded applications and can prevent
 * false sharing.
 */
template <typename T>
struct CacheLineStorage
{
  T data __attribute__((aligned(CACHE_LINE_SIZE)));
  char pad[CACHE_LINE_SIZE > sizeof(T) ? CACHE_LINE_SIZE - sizeof(T) : 0];
};

/** Interface that all object recyclers should implement */
template<class T>
class ObjectRecycler
{
public:
  void recycle(T *obj) { if (obj) _recycle(obj); }
  virtual ~ObjectRecycler() { };
protected:
  virtual void _recycle(T *obj) = 0;
};

/** Default implementation that just uses ctor/dtor to recycle */
template<class T>
class DefaultObjectRecycler : public ObjectRecycler<T>
{
protected:
  void _recycle(T *obj) {
    obj->~T();
    new(obj) T;
  }
};

template<class T>
class ResettableObjectRecycler : public ObjectRecycler<T>
{
protected:
  void _recycle(T *obj) { obj->reset(); }
};

//----------------------------------------------------------------------------
/**
 * Object pool container
 *
 * Object pool that keeps allocated objects around for reuse.  Follows the Object pool design pattern.  This container doesn't
 * keep pointers to objects in use and only keeps a count of them, for performance reasons.
 */
template <class T>
class ObjectPool
{
public:
  ObjectPool(): _inUseCount(0), _maxPoolSize(1000), _optRecycler(0) {}

  /**
   * Acquire and object from the pool, if empty create a new object
   *
   * @return Pointer to a new or reused object
   */
  T* alloc();

  /**
   * Release the object into the pool.  If there are too many object allocated already then it will be freed.
   *
   * @param Pointer to an object that needs to go back into the pool
   */
  void free(T *object);

  /**
   * Sets the maximum size of the pool
   */
  void setMaxPoolSize(const uint32_t size) { _maxPoolSize = size; }

  /**
   * Fill up the pool with allocated objects
   */
  void preAlloc();

  /**
   * Set object recycler
   *
   * @param recycler New recycler object; default will be restored if set to 0
   */
  void setRecycler(ObjectRecycler<T> *recycler) { _optRecycler = recycler; }

  void display() {
    std::cout << "pool size : " << _maxPoolSize << std::endl
              << "in use    : " << _inUseCount << std::endl
              << "not used  : " << _notused.size() << std::endl;
  }

private:
  uint32_t _inUseCount;
  uint32_t _maxPoolSize;
  std::stack<T*> _notused;
  T _pristine;
  DefaultObjectRecycler<T> _defRecycler;
  ObjectRecycler<T> *_optRecycler;
};


//----------------------------------------------------------------------------
/**
 * Thread safe object pool container
 *
 * Uses a mutex to lock the container before modifying the contents.  The added mutex and locking is the only added functionality
 * over the base class.
 */
template <class T>
class ObjectPoolLocking : public ObjectPool<T>
{
public:
  ObjectPoolLocking(): ObjectPool<T>()
  {
    pthread_mutex_init(&_mutex, NULL);
  }

  /**
   * Acquire and object from the pool, if empty create a new object
   *
   * @return Pointer to a new or reused object
   */
  T* alloc()
  {
    assert(pthread_mutex_lock(&_mutex) == 0);
    T *object = ObjectPool<T>::alloc();
    assert(pthread_mutex_unlock(&_mutex) == 0);
    return object;
  }

  /**
   * Release the object into the pool.  If there are too many objects allocated already then it will be freed.
   *
   * @param Pointer to an object that needs to go back into the pool
   */
  void free(T *object)
  {
    assert(pthread_mutex_lock(&_mutex) == 0);
    ObjectPool<T>::free(object);
    assert(pthread_mutex_unlock(&_mutex) == 0);
  }

  /**
   * Sets the maximum size of the pool
   */
  void setMaxPoolSize(const uint32_t size)
  {
    assert(pthread_mutex_lock(&_mutex) == 0);
    ObjectPool<T>::setMaxPoolSize(size);
    assert(pthread_mutex_unlock(&_mutex) == 0);
  }

  void preAlloc() {
    assert(pthread_mutex_lock(&_mutex) == 0);
    ObjectPool<T>::preAlloc();
    assert(pthread_mutex_unlock(&_mutex) == 0);
  }

  void display() {
    assert(pthread_mutex_lock(&_mutex) == 0);
    ObjectPool<T>::display();
    assert(pthread_mutex_unlock(&_mutex) == 0);
  }

private:
  pthread_mutex_t _mutex;
};


//----------------------------------------------------------------------------
/**
 * Thread safe object pool container with multiple pools
 *
 * Creates multiple pools to reduce the lock contention and uses the thread
 * id and a hash to determine what pool to use.
 */
template <class T, class C = ObjectPoolLocking<T> >
class ObjectPoolPartitioned
{
public:
  ObjectPoolPartitioned(const uint32_t poolCount = 17): _poolCount(poolCount) {
    _pools = new CacheLineStorage<C>[poolCount];
  }
 
  virtual ~ObjectPoolPartitioned() {} 
  /**
   * Acquire and object from the pool, if empty create a new object
   *
   * @param tid If provided, will be used to identify "target" pool;
   *        else will use current thread to find pool
   * @return Pointer to a new or reused object
   */
  T* alloc(const pthread_t *tid = 0)
  {
    uint32_t poolIndex = poolLookup(tid);
    return _pools[poolIndex].data.alloc();
  }

  /**
   * Release the object into the pool.  If there are too many objects allocated already then it will be freed.
   *
   * @param Pointer to an object that needs to go back into the pool
   */
  void free(T *object) {
    uint32_t poolIndex = poolLookup(0);
    _pools[poolIndex].data.free(object);
  }

  /**
   * Sets the maximum size of the pools
   */
  void setMaxPoolSize(const uint32_t size) {
    for (uint32_t i = 0; i < _poolCount; ++i) {
      _pools[i].data.setMaxPoolSize(size);
    }
  }

  /**
   * Allocates enough objects to fill the pools
   */
  void preAlloc() {
    for (uint32_t i = 0; i < _poolCount; ++i) {
      _pools[i].data.preAlloc();
    }
  }

  /**
   * Set object recycler
   *
   * @param recycler New recycler object; default will be restored if set to 0
   */
  void setRecycler(ObjectRecycler<T> *recycler) {
    for (uint32_t i = 0; i < _poolCount; ++i) {
      _pools[i].data.setRecycler(recycler);
    }
  }

  void display() {
    for (uint32_t i = 0; i < _poolCount; ++i) {
      std::cout << "Structure information index: " << i << std::endl;
      _pools[i].display();
    }
  }

  /**
   * Gets the pool for the thread
   *
   * @param tid If provided, will be used to identify "target" pool; 
   *        else will use current thread's pool
   * @return Pointer to the pool for the thread
   */
  C* getPool(const pthread_t *tid = 0) {
    uint32_t poolIndex = poolLookup(tid);
    return &_pools[poolIndex].data;
  }

protected:
  /**
   * Simple hash to determine the pool to use
   */
  virtual uint32_t poolLookup(const pthread_t *tid) {
    pthread_t threadId = tid ? *tid : pthread_self();
    return static_cast<uint32_t>(threadId * (threadId + 3)) % _poolCount;
  }

  uint32_t _poolCount;
  CacheLineStorage<C> *_pools;    /// defaults to type ObjectPoolLocking<T>
};


//----------------------------------------------------------------------------
/**
 * Thread safe object pool container with multiple pools and minimal locking
 * 
 * Assigns a thread per pool and requires no locking after assigning threads
 * to a pool.  Must have the number of pools >= the number of threads.  Having
 * the number of pools equal to the number of threads would be ideal, not to
 * waist some memory.
 */
template <class T, class C = ObjectPool<T> >
class ObjectPoolThreadMapped : public ObjectPoolPartitioned<T, C>
{
public:
  ObjectPoolThreadMapped(const uint32_t poolCount = 32): ObjectPoolPartitioned<T, C>(poolCount) {
    _lookupIndex = new CacheLineStorage<pthread_t>[ObjectPoolPartitioned<T, C>::_poolCount];
    for (uint32_t i = 0; i < ObjectPoolPartitioned<T, C>::_poolCount; ++i) {
      _lookupIndex[i].data = 0;
    }

    pthread_mutex_init(&_lookupMutex, NULL);
  }

protected:

  /**
   * Array lookup to determine the pool to use
   */
  virtual uint32_t poolLookup(const pthread_t *tid);

  CacheLineStorage<pthread_t> *_lookupIndex;
  pthread_mutex_t _lookupMutex;
};


// inlined methods
//----------------------------------------------------------------------------
template <class T>
inline T* ObjectPool<T>::alloc()
{
  T *object = NULL;
  
  // check to see if we need to new a new object or get one from the stack
  if (_notused.size() == 0) {
    object = new T;
  } else {
    object = _notused.top();
    _notused.pop();
  }

  // keep track of how many objects are currently being used
  ++_inUseCount;
  return object;
}


//----------------------------------------------------------------------------
template <class T>
inline void ObjectPool<T>::free(T *object)
{
  // check to see if we have allocated too many already
  if (_notused.size() + _inUseCount <= _maxPoolSize) {
    // reset the object to an initial state
    _optRecycler ? _optRecycler->recycle(object) : _defRecycler.recycle(object);
    _notused.push(object);
  } else {
    delete object;
  }

  // check for underflow
  assert(_inUseCount > 0);
  --_inUseCount;
}

//----------------------------------------------------------------------------
template <typename T>
inline void ObjectPool<T>::preAlloc()
{
  uint32_t toAlloc = _maxPoolSize - _inUseCount;
  _inUseCount = _maxPoolSize;

  for (uint32_t i = 0; i < toAlloc; ++i) {
    free(new T);
  }
}

//----------------------------------------------------------------------------
template <class T, class C>
inline uint32_t ObjectPoolThreadMapped<T, C>::poolLookup(const pthread_t *tid)
{
  pthread_t threadId = tid ? *tid : pthread_self();
  for (uint32_t i = 0; i < ObjectPoolPartitioned<T, C>::_poolCount; ++i) {
    if (_lookupIndex[i].data == threadId) {
      return i;
    }
  }

  pthread_mutex_lock(&_lookupMutex);
  uint32_t i = 0;
  for (; i < ObjectPoolPartitioned<T, C>::_poolCount; ++i) {
    if (_lookupIndex[i].data == threadId) {
      pthread_mutex_unlock(&_lookupMutex);
      return i;
    } else if(_lookupIndex[i].data == 0) {
      _lookupIndex[i].data = threadId;
      pthread_mutex_unlock(&_lookupMutex);
      return i;
    }
  }

  abort(); // we should never reach this point
  return 0;
}
#endif
