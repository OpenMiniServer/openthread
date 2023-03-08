/***************************************************************************
 * Copyright (C) 2023-, openlinyou, <linyouhappy@outlook.com>
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 ***************************************************************************/

#ifndef HEADER_OPEN_THREAD_H
#define HEADER_OPEN_THREAD_H

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <atomic>

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#ifdef __cplusplus
extern "C" {
#endif

typedef void* HANDLE;
typedef unsigned long DWORD;
typedef HANDLE pthread_mutex_t;
struct pthread_t_
{
  HANDLE thread_handle;
  DWORD  thread_id;
};

typedef struct pthread_t_ pthread_t;
typedef int pthread_mutexattr_t;       
typedef int pthread_condattr_t;        
typedef int pthread_attr_t; 
#define PTHREAD_MUTEX_RECURSIVE 0

typedef struct
{
    int waiters_count_;
    HANDLE sema_;
    HANDLE waiters_done_;
    size_t was_broadcast_;
    char waiters_count_lock_[256];
} pthread_cond_t;

struct pthread_key_t_
{
    DWORD key;
    void (*destructor) (void *);  
};
typedef struct pthread_key_t_ pthread_key_t;

#ifdef __cplusplus
}
#endif

#else
#include <pthread.h>
#endif

namespace open
{

class OpenThreadRef;
class OpenThreadPool;
class OpenThread
{
public:
    enum State
    {
        START,
        RUN,
        STOP
    };
    class Msg
    {
        OpenThread* thread_;
        std::shared_ptr<void> data_;
        Msg():thread_(0), data_(0), state_(START) {};
        Msg(const Msg&) :thread_(0), data_(0), state_(START){}
        void operator=(const Msg&) {}
    public:
        State state_;
        template <class T>
        inline const T* data() const { return dynamic_cast<const T*>((const T*)data_.get()); }
        template <class T>
        inline T* edit() const { return dynamic_cast<T*>((T*)data_.get()); }
        OpenThread& thread() const;
        const int pid() const;
        const std::string& name() const;
        template <class T>
        T* custom() const { return thread_ ? dynamic_cast<T*>((T*)thread_->custom_) : 0; }
        friend class OpenThread;
    };
    friend class Msg;

    ~OpenThread();
    bool start(void (*cb)(const Msg&));
    bool stop();
    bool send(const std::shared_ptr<void>& data);
    bool isCurrent();
    void waitIdle();

	inline bool isIdle() { return isIdle_; }
	inline bool isRunning() { return state_ == RUN; }
    inline State state() { return state_; }

    inline int pid() { return pid_; }
    inline const std::string& name() { return name_; }
    inline size_t& totalCount() { return totalCount_; }
    inline size_t& leftCount() { return leftCount_; }
    inline int64_t& cpuCost() { return cpuCost_; }
    inline int64_t& cpuStart() { return cpuStart_; }

    template <class T>
    static std::shared_ptr<T> MakeShared()
    { 
        return std::shared_ptr<T>(new T); 
    }
	static bool Init(size_t capacity = 256, bool profile = true);
    static OpenThreadRef Create(const std::string& name);
	static OpenThreadRef Create(const std::string& name, void (*cb)(const Msg&));
    static OpenThreadRef Thread(int pid);
    static OpenThreadRef Thread(const std::string& name);
    static const std::string& ThreadName(int pid);
    static int ThreadId(const std::string& name);
    static size_t GetThreadCapacity();
    static size_t GetThreadSize();

    static bool Send(int pid, const std::shared_ptr<void>& data);
    static bool Send(const std::string& name, const std::shared_ptr<void>& data);
    static bool Send(const std::vector<int>& vectPid, const std::shared_ptr<void>& data);
    static bool Send(const std::vector<std::string>& vectName, const std::shared_ptr<void>& data);
    static bool Stop(int pid);
    static bool Stop(const std::string& name);
    static void StopAll();
    static void ThreadJoin(OpenThreadRef& ref);
    static void ThreadJoin(const std::vector<int>& vectPid);
    static void ThreadJoin(const std::vector<std::string>& vectName);
    static void ThreadJoinAll();
    static std::shared_ptr<OpenThread> GetThread(OpenThreadRef& ref);

    static void Sleep(int64_t milliSecond);
    static int64_t ThreadTime();
    static int64_t MilliUnixtime();

    //Use with care
    bool hasMsg();
    inline void setCustom(void* custom) { custom_ = custom; }
private:
    void run();
    static void Run(void* arg);
    OpenThread(const std::string& name);
    OpenThread(const OpenThread&);
    void operator=(const OpenThread&) {}

    int pid_;
    volatile State state_;
    volatile bool isIdle_;
    void* custom_;

    const std::string name_;
    pthread_t threadId_;
    pthread_cond_t cond_;
    pthread_mutex_t mutex_;
    void (*cb_)(const Msg&);
    OpenThreadPool* pool_;

    bool profile_;
    size_t totalCount_;
    size_t leftCount_;
    int64_t cpuCost_;
    int64_t cpuStart_;
private:
    struct Node
    {
        unsigned int id_;
        Msg msg_;
        Node* next_;
        Node():id_(0), next_(0){}
    };
    class SpinLock
    {
    private:
        SpinLock(const SpinLock&) {};
        void operator=(const SpinLock) {};
    public:
        SpinLock() {};
        void lock() { while (flag_.test_and_set(std::memory_order_acquire)); }
        void unlock() { flag_.clear(std::memory_order_release); }
    private:
        std::atomic_flag flag_;
    };
    class SafeQueue
    {
        Node head_;
        Node* tail_;
        SpinLock spinLock_;
        unsigned int writeId_;
        unsigned int readId_;
        std::vector<Node*> vectCache_;
    public:
        SafeQueue();
        ~SafeQueue();
        void clear();
        void popAll();
        void popAll(std::queue<Node*>& queueCache);
        void push(Node* node);
        bool empty() { return !head_.next_; }
    };
    unsigned int readId_;
    SafeQueue queue_;

    Node* popNode();
    std::queue<Node*> queueCache_;
private:
    static OpenThreadPool DefaultPool_;
    friend class OpenThreadPool;
 public:
     static inline bool Send(const std::initializer_list<int>& list, const std::shared_ptr<void>& data) 
     { std::vector<int> v = list;return Send(v, data); }
     static inline bool Send(const std::initializer_list<std::string>& list, const std::shared_ptr<void>& data) 
     { std::vector<std::string> v = list;return Send(v, data); }
     static inline void ThreadJoin(const std::initializer_list<int>& list) 
     { std::vector<int> v = list;return ThreadJoin(v); }
     static inline void ThreadJoin(const std::initializer_list<std::string>& list) 
     { std::vector<std::string> v = list; return ThreadJoin(v); }
};
typedef const OpenThread::Msg OpenThreadMsg;

class OpenThreader
{
public:
    OpenThreader(const std::string& name) :name_(name), pid_(-1) {}
    virtual ~OpenThreader(){ stop(); }
    virtual bool start();
    virtual void stop();
    virtual void onStart() { }
    virtual void onMsg(OpenThreadMsg& msg) { }
    virtual void onStop() { }
    const inline int pid() { return pid_; }
    const std::string& name() { return name_; }
    static void Thread(OpenThreadMsg& msg);
    static inline int ThreadId(const std::string& name) { return OpenThread::ThreadId(name); }
    static inline const std::string& ThreadName(int pid) { return OpenThread::ThreadName(pid); }
protected:
    int pid_;
    const std::string name_;
    std::shared_ptr<OpenThread> thread_;
};

class OpenThreadPool
{
    class SafeMap
    {
        size_t capacity_;
        std::vector<std::shared_ptr<OpenThread>> vectValues_;
        std::vector<std::shared_ptr<const std::string>> vectKeys_;
        SafeMap(const SafeMap& that);
        void operator=(const SafeMap& that) {}
    public:
        SafeMap();
        SafeMap(size_t capacity);
        ~SafeMap();
        bool isFull();
        void clear();
        size_t size();
        size_t capacity() { return vectKeys_.size(); }
        void setCapacity(size_t capacity);
        std::shared_ptr<OpenThread> operator[](size_t pid);
        std::shared_ptr<OpenThread> operator[](const std::string& name);
        int set(std::shared_ptr<OpenThread>& value);
    };
public:
    OpenThreadPool();
    ~OpenThreadPool();

    void lock();
    void unlock();

    bool init(size_t capacity, bool profile = true);
    bool checkInit();
    void stopAll();
    size_t size();
    size_t capacity();

    std::shared_ptr<OpenThread> create(const std::string& name);
    std::shared_ptr<OpenThread> create(const std::string& name, void (*cb)(const OpenThread::Msg&));
    std::shared_ptr<OpenThread> thread(int pid);
    std::shared_ptr<OpenThread> thread(const std::string& name);
    const std::string& threadName(int pid);
    int threadId(const std::string& name);

    bool send(int pid, const std::shared_ptr<void>& data);
    bool send(const std::string& name, const std::shared_ptr<void>& data);
    bool send(const std::vector<int>& vectPid, const std::shared_ptr<void>& data);
    bool send(const std::vector<std::string>& vectName, const std::shared_ptr<void>& data);
    bool stop(int pid);
    bool stop(const std::string& name);
    void threadJoin(const std::shared_ptr<OpenThread>& ref);
    void threadJoin(const std::vector<int>& vectPid);
    void threadJoin(const std::vector<std::string>& vectName);
    void threadJoinAll();

private:
    bool profile_;
    SafeMap safeMap_;
    volatile bool isInit_;
    volatile bool isClearIng_;
    pthread_mutex_t mutex_;
    pthread_mutex_t mutex_close_;
};

class OpenThreadRef
{
    std::shared_ptr<OpenThread> thread_;
public:
    OpenThreadRef() {}
    OpenThreadRef(std::shared_ptr<OpenThread>& ref) :thread_(ref) {}
    OpenThreadRef(const OpenThreadRef& that) { thread_ = that.thread_; }
    void operator=(const OpenThreadRef& that) { thread_ = that.thread_; }
    bool operator==(const OpenThreadRef& that) { return thread_ == that.thread_; }
    bool operator==(const std::shared_ptr<OpenThread>& that) { return thread_ == that; }
    inline void waitIdle() { if (thread_) thread_->waitIdle(); }
    bool start(void (*cb)(OpenThreadMsg&));
    bool stop();
    bool send(const std::shared_ptr<void>& data);
    bool waitStop(int64_t milliSecond = 1);

    bool isIdle();
    bool isRunning();

    int pid();
    const std::string& name();
    operator bool() const { return !!thread_; }
    friend class OpenThread;
};

class OpenSync
{
    class OpenSyncRef
    {
        volatile bool isSleep_;
        pthread_cond_t cond_;
        pthread_mutex_t mutex_;
        OpenSyncRef();
        OpenSyncRef(const OpenSyncRef&);
        void operator=(const OpenSyncRef&) {}
    public:
        ~OpenSyncRef();
        bool await();
        bool wakeup();
        friend class OpenSync;
    };
    std::shared_ptr<OpenSyncRef> sync_;
public:
    OpenSync() { sync_ = std::shared_ptr<OpenSyncRef>(new OpenSyncRef); }
    OpenSync(const OpenSync& that) { sync_ = that.sync_; }
    void operator=(const OpenSync& that) { sync_ = that.sync_; }

    inline void await() { if (sync_) sync_->await(); }
    inline bool wakeup() { if (sync_) return sync_->wakeup(); return false; }
    operator bool() const { return !!sync_; }
};


};

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#ifdef __cplusplus
extern "C" {
#endif
// mutex
int pthread_mutex_lock(pthread_mutex_t* _mutex);
int pthread_mutex_unlock(pthread_mutex_t* _mutex);
int pthread_mutex_init(pthread_mutex_t* _mutex, void* ignoredAttr);
int pthread_mutex_destroy(pthread_mutex_t* _mutex);

// pthread
int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start_routine)(void*), void* arg);
int pthread_equal(pthread_t t1, pthread_t t2);
pthread_t pthread_self();
int pthread_join(pthread_t _thread, void** ignore);
int pthread_detach(pthread_t _thread);

void pthread_mutexattr_init(pthread_mutexattr_t* ignore);
void pthread_mutexattr_settype(pthread_mutexattr_t* ingore_attr, int ignore);
void pthread_mutexattr_destroy(pthread_mutexattr_t* ignore_attr);

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
int pthread_key_delete(pthread_key_t key);
void* pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* value);

int pthread_setname_np(pthread_t thread, const char* name);
int pthread_getname_np(pthread_t thread, char* name, size_t len);

// cond
int pthread_cond_init(pthread_cond_t* cv, const pthread_condattr_t* ignore);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_signal(pthread_cond_t* cv);
int pthread_cond_broadcast(pthread_cond_t* cv);
int pthread_cond_wait(pthread_cond_t* cv, pthread_mutex_t* external_mutex);
#ifdef __cplusplus
}
#endif

#endif

template <class SRC, class DST = void>
class OpenSyncReturn
{
    class OpenSyncRef
    {
        volatile bool isSleep_;
        pthread_cond_t cond_;
        pthread_mutex_t mutex_;
        std::shared_ptr<SRC> srcData_;
        std::shared_ptr<DST> destData_;
        OpenSyncRef()
        {
            isSleep_ = false;
            pthread_mutex_init(&mutex_, NULL);
            pthread_cond_init(&cond_, NULL);
        }
        OpenSyncRef(const OpenSyncRef&)
        {
            assert(false);
            isSleep_ = that.isSleep_;
            pthread_mutex_init(&mutex_, NULL);
            pthread_cond_init(&cond_, NULL);
        }
        void operator=(const OpenSyncRef&) {}
    public:
        ~OpenSyncRef()
        {
            pthread_mutex_destroy(&mutex_);
            pthread_cond_destroy(&cond_);
        }
        bool await()
        {
            if (!isSleep_)
            {
                isSleep_ = true;
                pthread_mutex_lock(&mutex_);
                pthread_cond_wait(&cond_, &mutex_);
                pthread_mutex_unlock(&mutex_);
                return true;
            }
            return false;
        }
        inline const std::shared_ptr<DST>& awaitReturn()
        {
            await();
            return destData_;
        }
        bool wakeup()
        {
            if (isSleep_)
            {
                isSleep_ = 0;
                pthread_cond_signal(&cond_);
                return true;
            }
            return false;
        }
        bool wakeup(const std::shared_ptr<DST>& data)
        {
            if (isSleep_)
            {
                isSleep_ = 0;
                destData_ = data;
                pthread_cond_signal(&cond_);
                return true;
            }
            return false;
        }
        friend class OpenSyncReturn;
    };
    std::shared_ptr<OpenSyncRef> sync_;
public:
    OpenSyncReturn() { sync_ = std::shared_ptr<OpenSyncRef>(new OpenSyncRef); }
    OpenSyncReturn(const OpenSyncReturn& that) { sync_ = that.sync_; }
    void operator=(const OpenSyncReturn& that) { sync_ = that.sync_; }

    void operator=(const std::shared_ptr<SRC>& data) { if (sync_) sync_->srcData_ = data; }
    void put(const std::shared_ptr<SRC>& data) { if (sync_) sync_->srcData_ = data; }
    inline const std::shared_ptr<SRC> get() const { if (sync_) return sync_->srcData_; return std::shared_ptr<SRC>(); }

    inline void await() { if (sync_) sync_->await(); }
    inline const std::shared_ptr<DST> awaitReturn() { if (sync_) return sync_->awaitReturn(); return std::shared_ptr<DST>(); }

    inline bool wakeup() { if (sync_) return sync_->wakeup(); return false; }
    inline bool wakeup(const std::shared_ptr<DST>& data) { if (sync_) return sync_->wakeup(data); return false; }

    operator bool() const { return !!sync_; }
};

#endif /* HEADER_OPEN_THREAD_H */
