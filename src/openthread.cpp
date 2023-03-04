/***************************************************************************
 * Copyright (C) 2023-, openlinyou, <linyouhappy@outlook.com>
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 ***************************************************************************/
#include "openthread.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <stdint.h>
#include <time.h>
#include <assert.h>

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#ifdef __cplusplus
extern "C" {
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h> 
#include <ws2tcpip.h>
#include <process.h>

#ifdef __cplusplus
}
#endif
#else 
 #include <unistd.h>
#include <sys/time.h>
#endif

namespace open
{

OpenThread& OpenThread::Msg::thread() const
{
    if (thread_) return *thread_;
    assert(false);
    static OpenThread emptyThread("NULL");
    return emptyThread;
}

const int OpenThread::Msg::pid() const
{ 
    if (thread_)
    {
        return thread_->pid_;
    }
    return -1; 
}
const std::string& OpenThread::Msg::name() const
{ 
    if (thread_)
    {
        return thread_->name_;
    }
    static const std::string empty;
    return empty;
}

// SafeQueue
OpenThread::SafeQueue::SafeQueue()
{
    head_.next_ = 0;
    tail_ = &head_;
    writeId_ = 0;
    readId_  = 0;
}

OpenThread::SafeQueue::~SafeQueue()
{
    clear();
    assert(vectCache_.empty());
}

void OpenThread::SafeQueue::clear()
{
    popAll();
    for (size_t i = 0; i < vectCache_.size(); i++)
    {
        if (vectCache_[i]->id_ != readId_)
        {
            assert(false);
        }
        ++readId_;
        delete vectCache_[i];
    }
    vectCache_.clear();
    readId_  = 0;
    writeId_ = 0;
}

void OpenThread::SafeQueue::popAll()
{
    //spinLock_.lock();
    Node* node = head_.next_;
    head_.next_ = 0;
    tail_ = &head_;
    //spinLock_.unlock();
    if (node)
    {
        assert(vectCache_.empty());
        Node* tmpNode = 0;
        while (node)
        {
            vectCache_.push_back(node);
            tmpNode = node->next_;
            node->next_ = 0;
            node = tmpNode;
        }
    }
}

void OpenThread::SafeQueue::popAll(std::queue<Node*>& queueCache)
{
    popAll();
    if (vectCache_.empty())
    {
        return;
    }
    for (size_t i = 0; i < vectCache_.size(); i++)
    {
        if (vectCache_[i]->id_ != readId_)
        {
            assert(false);
        }
        ++readId_;
        queueCache.push(vectCache_[i]);
    }
    vectCache_.clear();
}

void OpenThread::SafeQueue::push(OpenThread::Node* node)
{
    spinLock_.lock();
    node->id_ = writeId_++;
    tail_->next_ = node;
    tail_ = node;
    node->next_ = 0;
    spinLock_.unlock();
}

//OpenThread
OpenThread::OpenThread(const std::string& name)
    :state_(STOP),
    name_(name),
    pool_(0)
{
    cb_ = 0;
    pid_ = -1;
    leftCount_  = 0;
    totalCount_ = 0;
    cpuCost_   = 0;
    cpuStart_  = 0;

    isIdle_ = false;
    custom_ = 0;
    memset(&threadId_, 0, sizeof(threadId_));

    pthread_mutex_init(&mutex_, NULL);
    pthread_cond_init(&cond_, NULL);
}

OpenThread::OpenThread(const OpenThread&)
{
    assert(false);
    pool_ = 0;
    cb_ = 0;
    pid_ = -1;
    leftCount_ = 0;
    totalCount_ = 0;
    cpuCost_ = 0;
    cpuStart_ = 0;
    isIdle_ = false;
    custom_ = 0;
    memset(&threadId_, 0, sizeof(threadId_));

    pthread_mutex_init(&mutex_, NULL);
    pthread_cond_init(&cond_, NULL);
}


OpenThread::~OpenThread()
{
    assert(state_ == STOP);
    cb_ = 0;
    pid_ = -1;
    isIdle_ = false;
    pthread_mutex_destroy(&mutex_);
    pthread_cond_destroy(&cond_);
}

bool OpenThread::start(void (*cb)(const Msg&))
{
    if (!pool_)
    {
        assert(false);
        return false;
    }
    OpenThreadRef ref;
    ref.thread_ = pool_->thread(name_);
    if (!ref)
    {
        assert(false);
        return false;
    }
    if (ref.pid() != pid_ || ref.name() != name_)
    {
        assert(false);
        return false;
    }
    pthread_mutex_lock(&mutex_);
    if (!cb)
    {
        assert(false);
        pthread_mutex_unlock(&mutex_);
        return false;
    }
    assert(state_ == STOP);
    if (state_ != STOP)
    {
        assert(false);
        pthread_mutex_unlock(&mutex_);
        return false;
    }
    state_ = START;

    cb_ = cb;
    readId_ = 0;
    leftCount_ = 0;
    totalCount_ = 0;

    cpuCost_ = 0;
    cpuStart_ = 0;

    memset(&threadId_, 0, sizeof(threadId_));
    std::shared_ptr<OpenThread>* ptr = new std::shared_ptr<OpenThread>(ref.thread_);
    int ret = pthread_create(&threadId_, NULL, (void* (*)(void*))OpenThread::Run, ptr);
    if (ret != 0)
    {
        delete ptr;
        cb_ = 0;
        state_ = STOP;
        pthread_mutex_unlock(&mutex_);
        return false;
    }
    int count = 0;
    while (state_ != RUN)
    {
        OpenThread::Sleep(1);
        if (++count > 5000)
        {
            assert(false);
            cb_ = 0;
            state_ = STOP;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_);
    return true;
}

bool OpenThread::stop()
{
    //printf("OpenThread::stop==>>[%s]\n", name_.c_str());
    if (state_ != RUN) return false;
    Node* node = new Node;
    Msg& msg = node->msg_;
    msg.state_ = STOP;
    msg.thread_ = 0;
    queue_.push(node);
    //if (isIdle_)
    pthread_cond_signal(&cond_);
    return true;
}

bool OpenThread::send(const std::shared_ptr<void>& data)
{
    //printf("OpenThread::send==>>[%s]\n", name_.c_str());
    if (state_ != RUN) return false;
    Node* node = new Node;
    Msg& msg = node->msg_;
    msg.state_ = RUN;
    msg.data_  = data;
    msg.thread_ = 0;
    queue_.push(node);
    //if (isIdle_)
    pthread_cond_signal(&cond_);
    return true;
}

bool OpenThread::isCurrent()
{
    return pthread_equal(pthread_self(), threadId_);
}

void OpenThread::Run(void* arg)
{
    assert(arg);
    if (!arg) return;
    std::shared_ptr<OpenThread>* ptr = (std::shared_ptr<OpenThread>*)arg;
    if (!*ptr)
    {
        delete ptr;
        return;
    }
    pthread_setname_np((*ptr)->threadId_, (*ptr)->name_.c_str());
    (*ptr)->run();
    delete ptr;
}

bool OpenThread::hasMsg()
{
    assert(pthread_equal(pthread_self(), threadId_));
    if (!queueCache_.empty())
    {
        return true;
    }
    if (!queue_.empty())
    {
        queue_.popAll(queueCache_);
    }
    return !queueCache_.empty();
}

OpenThread::Node* OpenThread::popNode()
{
    if (!queue_.empty())
    {
        queue_.popAll(queueCache_);
    }
    Node* node = 0;
    if (!queueCache_.empty())
    {
        totalCount_++;
        leftCount_ = queueCache_.size();
        node = queueCache_.front();
        queueCache_.pop();
    }
    return node;
}

void OpenThread::run()
{
    assert(state_ == START);
    if (state_ != START) return;
    state_ = RUN;
    assert(pthread_equal(pthread_self(), threadId_));
    {
        Msg msg;
        msg.thread_ = this;
        msg.state_ = START;
        cb_(msg);
    }
    Node* node = 0;
    isIdle_ = false;
    bool isRunning = true;
    assert(queueCache_.empty());
    while (state_ == RUN)
    {
        while ((node = popNode()))
        {
            if (node->msg_.state_ == STOP)
            {
                node->msg_.thread_ = this;
                if (node->id_ != readId_)
                {
                    printf("OpenThread[%s] node->id_:%d, readId_:%d\n", name_.c_str(), node->id_, readId_);
                    assert(false);
                }
                ++readId_;
                cb_(node->msg_);
                delete node;
                isRunning = false;
                break;
            }
            if (profile_)
            {
                cpuStart_ = ThreadTime();
                node->msg_.thread_ = this;
                if (node->id_ != readId_)
                {
                    printf("OpenThread[%s] node->id_:%d, readId_:%d\n", name_.c_str(), node->id_, readId_);
                    assert(false);
                }
                ++readId_;
                cb_(node->msg_);
                cpuCost_ += ThreadTime() - cpuStart_;
            }
            else
            {
                node->msg_.thread_ = this;
                if (node->id_ != readId_)
                {
                    printf("OpenThread[%s] node->id_:%d, readId_:%d\n", name_.c_str(), node->id_, readId_);
                    assert(false);
                }
                ++readId_;
                cb_(node->msg_);
            }
            delete node;
        }
        if (!isRunning) break;

        isIdle_ = true;
        pthread_mutex_lock(&mutex_);
        pthread_cond_wait(&cond_, &mutex_);
        isIdle_ = false;
        pthread_mutex_unlock(&mutex_);
    }
    //printf("OpenThread[%s] exit\n", name_.c_str());
    state_ = STOP;
    pthread_mutex_lock(&mutex_);
    while (!queueCache_.empty())
    {
        node = queueCache_.front();
        queueCache_.pop();
        delete node;
    }
    queue_.clear();
    pthread_mutex_unlock(&mutex_);
    //printf("OpenThread[%s] exit===>>\n", name_.c_str());
}

// static method
OpenThreadPool OpenThread::DefaultPool_;
bool OpenThread::Init(size_t capacity, bool profile)
{
    return DefaultPool_.init(capacity, profile);
}

OpenThreadRef OpenThread::Create(const std::string& name)
{
    OpenThreadRef ref;
    ref.thread_ = DefaultPool_.create(name);
    return ref;
}

OpenThreadRef OpenThread::Create(const std::string& name, void (*cb)(const Msg&))
{
    OpenThreadRef ref;
    ref.thread_ = DefaultPool_.create(name, cb);
    return ref;
}

OpenThreadRef OpenThread::Thread(int pid)
{
    OpenThreadRef ref;
    ref.thread_ = DefaultPool_.thread(pid);
    return ref;
}

OpenThreadRef OpenThread::Thread(const std::string& name)
{
    OpenThreadRef ref;
    ref.thread_ = DefaultPool_.thread(name);
    return ref;
}

const std::string& OpenThread::ThreadName(int pid)
{
    return DefaultPool_.threadName(pid);
}

int OpenThread::ThreadId(const std::string& name)
{
    return DefaultPool_.threadId(name);
}

size_t OpenThread::GetThreadCapacity()
{
    return DefaultPool_.capacity();
}

size_t OpenThread::GetThreadSize()
{
    return DefaultPool_.size();
}

bool OpenThread::Send(int pid, const std::shared_ptr<void>& data)
{
    return  DefaultPool_.send(pid, data);
}

bool OpenThread::Send(const std::string& name, const std::shared_ptr<void>& data)
{
    return  DefaultPool_.send(name, data);
}

bool OpenThread::Send(const std::vector<int>& vectPid, const std::shared_ptr<void>& data)
{
    return  DefaultPool_.send(vectPid, data);
}

bool OpenThread::Send(const std::vector<std::string>& vectName, const std::shared_ptr<void>& data)
{
    return  DefaultPool_.send(vectName, data);
}

bool OpenThread::Stop(int pid)
{
    return  DefaultPool_.stop(pid);
}

bool OpenThread::Stop(const std::string& name)
{
    return  DefaultPool_.stop(name);
}

void OpenThread::StopAll()
{
    DefaultPool_.stopAll();
}

std::shared_ptr<OpenThread> OpenThread::GetThread(OpenThreadRef& ref)
{ 
    return ref.thread_; 
}

void OpenThread::ThreadJoin(OpenThreadRef& ref)
{
    DefaultPool_.threadJoin(ref.thread_);
}

void OpenThread::ThreadJoin(const std::vector<int>& vectPid)
{
    DefaultPool_.threadJoin(vectPid);
}

void OpenThread::ThreadJoin(const std::vector<std::string>& vectName)
{
    DefaultPool_.threadJoin(vectName);
}

void OpenThread::ThreadJoinAll()
{
    DefaultPool_.threadJoinAll();
}

//OpenThreader
void OpenThreader::start()
{
    auto threadRef = OpenThread::Thread(name_);
    assert(!threadRef);
    threadRef = OpenThread::Create(name_);
    assert(threadRef);
    pid_ = -1;
    if (threadRef)
    {
        thread_ = OpenThread::GetThread(threadRef);
        pid_ = thread_->pid();
        assert(!thread_->isRunning());
        thread_->setCustom(this);
        thread_->start(OpenThreader::Thread);
    }
}
void OpenThreader::stop()
{
    auto threadRef = OpenThread::Thread(name_);
    if (threadRef)
    {
        assert(threadRef == thread_);
        if (threadRef.isRunning())
        {
            threadRef.stop();
            threadRef.waitStop();
        }
    }
    else
        thread_.reset();
}

void OpenThreader::Thread(OpenThreadMsg& msg)
{
    OpenThreader* that = msg.custom<OpenThreader>();
    if (!that)
    {
        assert(false); return;
    }
    switch (msg.state_)
    {
    case OpenThread::RUN:
        that->onMsg(msg); return;
    case OpenThread::START:
        that->onStart(); return;
    case OpenThread::STOP:
        that->onStop(); return;
    default:
        assert(false); return;
    }
}

// SafeMap
OpenThreadPool::SafeMap::SafeMap()
    :capacity_(0)
{
}

OpenThreadPool::SafeMap::SafeMap(const SafeMap& that)
    :capacity_(0)
{
    assert(false);
}

OpenThreadPool::SafeMap::SafeMap(size_t capacity)
    :capacity_(0)
{
    setCapacity(capacity);
}

OpenThreadPool::SafeMap::~SafeMap()
{
}

void OpenThreadPool::SafeMap::setCapacity(size_t capacity)
{
    if (capacity_ > 0)
    {
        assert(false);
        return;
    }
    capacity_ = capacity;
    vectKeys_.resize(capacity, std::shared_ptr<const std::string>());
    vectValues_.resize(capacity, std::shared_ptr<OpenThread>());
}

bool OpenThreadPool::SafeMap::isFull()
{
    for (size_t i = 0; i < vectKeys_.size(); i++)
    {
        if (!vectKeys_[i]) return false;
    }
    return true;
}

size_t OpenThreadPool::SafeMap::size()
{
    for (size_t i = 0; i < vectKeys_.size(); i++)
    {
        if (!vectKeys_[i]) return i;
    }
    return vectKeys_.size();
}

std::shared_ptr<OpenThread> OpenThreadPool::SafeMap::operator[](size_t pid)
{
    assert(capacity_ == vectKeys_.size());
    assert(vectValues_.size() == vectKeys_.size());
    std::shared_ptr<OpenThread> ret;
    if (pid < vectValues_.size())
    {
        ret = vectValues_[pid];
    }
    if (ret)
    {
        assert(ret->pid() == pid);
    }
    return ret;
}

std::shared_ptr<OpenThread> OpenThreadPool::SafeMap::operator[](const std::string& name)
{
    assert(capacity_ == vectKeys_.size());
    assert(vectValues_.size() == vectKeys_.size());
    size_t i = 0;
    std::shared_ptr<const std::string> sptrKey;
    std::shared_ptr<OpenThread> ret;
    for (; i < vectKeys_.size(); i++)
    {
        sptrKey = vectKeys_[i];
        if (!sptrKey)  break;
        if (sptrKey->compare(name) == 0)
        {
            ret = vectValues_[i];
            if (ret)
            {
                assert(ret->pid() == i);
            }
            break;
        }
    }
    return ret;
}

int OpenThreadPool::SafeMap::set(std::shared_ptr<OpenThread>& value)
{
    if (!value) return -1;
    if (value->pid() != -1) return -1;
    assert(capacity_ == vectKeys_.size());
    assert(vectValues_.size() == vectKeys_.size());
    std::shared_ptr<const std::string> sptrKey;
    int pid = -1;
    std::string name = value->name();
    for (int i = 0; i < (int)vectKeys_.size(); i++)
    {
        sptrKey = vectKeys_[i];
        if (!sptrKey)
        {
            pid = i;
            vectValues_[pid] = value;
            vectKeys_[pid] = std::shared_ptr<const std::string>(new const std::string(name));
            break;
        }
        if (sptrKey->compare(name) == 0)
        {
            pid = i;
            vectValues_[pid] = value;
            break;
        }
    }
    if (pid != -1 && value)
    {
        value->pid_ = pid;
    }
    return pid;
}

void OpenThreadPool::SafeMap::clear()
{
    assert(capacity_ == vectKeys_.size());
    assert(vectValues_.size() == vectKeys_.size());
    for (size_t i = 0; i < vectKeys_.size(); ++i)
    {
        vectKeys_[i].reset();
        vectValues_[i].reset();
    }
}

// OpenThreadPool
OpenThreadPool::OpenThreadPool()
    :profile_(false),
    isInit_(false),
    isClearIng_(false)
{
    pthread_mutex_init(&mutex_, NULL);
    pthread_mutex_init(&mutex_close_, NULL);
}

OpenThreadPool::~OpenThreadPool()
{
    pthread_mutex_destroy(&mutex_);
    pthread_mutex_destroy(&mutex_close_);
}

void OpenThreadPool::lock()
{
    pthread_mutex_lock(&mutex_);
}

void OpenThreadPool::unlock()
{
    pthread_mutex_unlock(&mutex_);
}

bool OpenThreadPool::init(size_t capacity, bool profile)
{
    lock();
    if (isClearIng_)
    {
        unlock();
        return false;
    }
    if (isInit_)
    {
        unlock();
        assert(false);
        return true;
    }
    isInit_ = true;
    profile_ = profile;
    safeMap_.setCapacity(capacity);
    unlock();
    return true;
}

size_t OpenThreadPool::size()
{
    lock();
    if (isClearIng_)
    {
        unlock();
        return 0;
    }
    if (!isInit_)
    {
        unlock();
        if (!checkInit())
        {
            if (!init(256, true))
            {
                return 0;
            }
        }
        lock();
    }
    size_t size = safeMap_.size();
    unlock();
    return size;
}

size_t OpenThreadPool::capacity()
{
    lock();
    if (isClearIng_)
    {
        unlock();
        return 0;
    }
    if (!isInit_)
    {
        unlock();
        if (!checkInit())
        {
            if (!init(256, true))
            {
                return 0;
            }
        }
        lock();
    }
    size_t cap = safeMap_.capacity();
    unlock();
    return cap;
}

bool OpenThreadPool::checkInit()
{
    lock();
    if (isClearIng_)
    {
        unlock();
        return false;
    }
    if (!isInit_)
    {
        unlock();
        return false;
    }
    unlock();
    return true;
}

void OpenThreadPool::stopAll()
{
    if (isClearIng_)
    {
        return;
    }
    pthread_mutex_lock(&mutex_close_);
    isClearIng_ = true;
    if (!isInit_)
    {
        isClearIng_ = false;
        pthread_mutex_unlock(&mutex_close_);
        return;
    }
    std::shared_ptr<OpenThread> sptr;
    std::string name;
    for (size_t i = 0; i < safeMap_.capacity(); i++)
    {
        sptr = safeMap_[i];
        if (!sptr) continue;
        if (sptr->isRunning())
        {
            sptr->stop();
            int count = 1;
            while (sptr->isRunning())
            {
                OpenThread::Sleep(200);
                ++count;
                if (count == 5 || count == 10)
                {
                    if (sptr->isCurrent()) break;
                    sptr->stop();
                }
            }
        }
    }
    safeMap_.clear();
    isClearIng_ = false;
    pthread_mutex_unlock(&mutex_close_);
}

std::shared_ptr<OpenThread> OpenThreadPool::create(const std::string& name)
{
    std::shared_ptr<OpenThread> sptr;
    if (name.empty())
    {
        assert(false);
        return sptr;
    }
    if (!isInit_)
    {
        if (!checkInit())
        {
            if (!init(256, true))
            {
                return sptr;
            }
        }
    }
    sptr = safeMap_[name];
    if (!sptr)
    {
        lock();
        sptr = safeMap_[name];
        if (!sptr)
        {
            if (!safeMap_.isFull())
            {
                sptr = std::shared_ptr<OpenThread>(new OpenThread(name));
                sptr->pool_ = this;
                if (!safeMap_.set(sptr))
                {
                    unlock();
                    return sptr;
                }
            }
            sptr = safeMap_[name];
        }
        unlock();
    }
    else
    {
        assert(sptr->name() == name);
    }
    return sptr;
}

std::shared_ptr<OpenThread> OpenThreadPool::create(const std::string& name, void (*cb)(const OpenThread::Msg&))
{
    std::shared_ptr<OpenThread> sptr = create(name);
    if (sptr && cb)
    {
        if (!sptr->isRunning())
        {
            sptr->start(cb);
        }
        else
        {
            assert(sptr->cb_ == cb);
        }
    }
    return sptr;
}

std::shared_ptr<OpenThread> OpenThreadPool::thread(int pid)
{
    std::shared_ptr<OpenThread> sptr;
    if (pid < 0 || pid >= capacity())
    {
        return sptr;
    }
    if (!isInit_)
    {
        if (!checkInit()) return sptr;
    }
    sptr = safeMap_[pid];
    if (sptr)
    {
        assert(sptr->pid() == pid);
        assert(sptr->pool_ == this);
    }
    return sptr;
}

std::shared_ptr<OpenThread> OpenThreadPool::thread(const std::string& name)
{
    std::shared_ptr<OpenThread> sptr;
    if (!isInit_)
    {
        if (!checkInit()) return sptr;
    }
    sptr = safeMap_[name];
    if (sptr)
    {
        assert(sptr->name() == name);
        assert(sptr->pool_ == this);
    }
    return sptr;
}

const std::string& OpenThreadPool::threadName(int pid)
{
    static std::string emtpyString;
    if (pid < 0 || pid >= capacity())
    {
        return emtpyString;
    }
    if (!isInit_)
    {
        if (!checkInit()) return emtpyString;
    }
    std::shared_ptr<OpenThread> sptr = safeMap_[pid];
    if (sptr)
    {
        assert(sptr->pool_ == this);
        assert(sptr->pid() == pid);
        return sptr->name();
    }
    return emtpyString;
}

int OpenThreadPool::threadId(const std::string& name)
{
    if (!isInit_)
    {
        if (!checkInit()) return -1;
    }
    std::shared_ptr<OpenThread> sptr = safeMap_[name];
    if (sptr)
    {
        assert(sptr->pool_ == this);
        assert(sptr->name() == name);
        return sptr->pid();
    }
    return -1;
}

bool OpenThreadPool::send(int pid, const std::shared_ptr<void>& data)
{
    if (pid < 0 || pid >= capacity())
    {
        return false;
    }
    if (!isInit_)
    {
        if (!checkInit()) return false;
    }
    std::shared_ptr<OpenThread> sptr = safeMap_[pid];
    if (sptr && sptr->isRunning())
    {
        assert(sptr->pool_ == this);
        assert(sptr->pid() == pid);
        return sptr->send(data);
    }
    return false;
}

bool OpenThreadPool::send(const std::string& name, const std::shared_ptr<void>& data)
{
    if (!isInit_)
    {
        if (!checkInit()) return false;
    }
    std::shared_ptr<OpenThread> sptr = safeMap_[name];
    if (sptr && sptr->isRunning())
    {
        assert(sptr->pool_ == this);
        assert(sptr->name() == name);
        return sptr->send(data);
    }
    return false;
}

bool OpenThreadPool::send(const std::vector<int>& vectPid, const std::shared_ptr<void>& data)
{
    if (!isInit_)
    {
        if (!checkInit()) return false;
    }
    std::shared_ptr<OpenThread> sptr;
    bool ret = true;
    for (size_t i = 0; i < vectPid.size(); i++)
    {
        sptr = safeMap_[vectPid[i]];
        if (sptr && sptr->isRunning())
        {
            assert(sptr->pool_ == this);
            assert(sptr->pid() == vectPid[i]);
            ret = sptr->send(data) && ret;
        }
    }
    return ret;
}

bool OpenThreadPool::send(const std::vector<std::string>& vectName, const std::shared_ptr<void>& data)
{
    if (!isInit_)
    {
        if (!checkInit()) return false;
    }
    std::shared_ptr<OpenThread> sptr;
    bool ret = true;
    for (size_t i = 0; i < vectName.size(); i++)
    {
        sptr = safeMap_[vectName[i]];
        if (sptr && sptr->isRunning())
        {
            assert(sptr->pool_ == this);
            assert(sptr->name() == vectName[i]);
            ret = sptr->send(data) && ret;
        }
    }
    return ret;
}

bool OpenThreadPool::stop(int pid)
{
    if (pid < 0 || pid >= capacity())
    {
        return false;
    }
    if (!isInit_)
    {
        if (!checkInit()) return false;
    }
    std::shared_ptr<OpenThread> sptr = safeMap_[pid];
    if (sptr && sptr->isRunning())
    {
        assert(sptr->pool_ == this);
        assert(sptr->pid() == pid);
        sptr->stop();
    }
    return true;
}

bool OpenThreadPool::stop(const std::string& name)
{
    if (!isInit_)
    {
        if (!checkInit()) return false;
    }
    std::shared_ptr<OpenThread> sptr = safeMap_[name];
    if (sptr && sptr->isRunning())
    {
        assert(sptr->pool_ == this);
        assert(sptr->name() == name);
        sptr->stop();
    }
    return true;
}

void OpenThreadPool::threadJoin(const std::shared_ptr<OpenThread>& sptr)
{
    if (sptr && sptr->isRunning())
    {
        pthread_join(sptr->threadId_, NULL);
    }
}

void OpenThreadPool::threadJoin(const std::vector<int>& vectPid)
{
    if (!isInit_)
    {
        if (!checkInit()) return;
    }
    std::shared_ptr<OpenThread> sptr;
    for (size_t i = 0; i < vectPid.size(); i++)
    {
        sptr = safeMap_[vectPid[i]];
        if (!sptr) continue;
        if (sptr->isRunning())
        {
            assert(sptr->pool_ == this);
            assert(sptr->pid() == vectPid[i]);
            pthread_join(sptr->threadId_, NULL);
        }
    }
}

void OpenThreadPool::threadJoin(const std::vector<std::string>& vectName)
{
    if (!isInit_)
    {
        if (!checkInit()) return;
    }
    std::shared_ptr<OpenThread> sptr;
    for (size_t i = 0; i < vectName.size(); i++)
    {
        sptr = safeMap_[vectName[i]];
        if (!sptr) continue;
        if (sptr->isRunning())
        {
            assert(sptr->pool_ == this);
            assert(sptr->name() == vectName[i]);
            pthread_join(sptr->threadId_, NULL);
        }
    }
}
void OpenThreadPool::threadJoinAll()
{
    size_t size = safeMap_.capacity();
    std::shared_ptr<OpenThread> sptr;
    for (size_t i = 0; i < size; i++)
    {
        sptr = safeMap_[i];
        if (!sptr) continue;
        if (sptr->isRunning())
        {
            assert(sptr->pool_ == this);
            assert(sptr->pid() == i);
            pthread_join(sptr->threadId_, NULL);
        }
    }
}


// OpenThreadRef
bool OpenThreadRef::start(void (*cb)(OpenThreadMsg&))
{
    OpenThread* ptr = thread_.get();
    return ptr ? ptr->start(cb) : false;
}

bool OpenThreadRef::stop()
{
    OpenThread* ptr = thread_.get();
    return ptr ? ptr->stop(), true : false;
}

bool OpenThreadRef::send(const std::shared_ptr<void>& data)
{
    OpenThread* ptr = thread_.get();
    return ptr ? ptr->send(data) : false;
}

bool OpenThreadRef::isIdle()
{
    OpenThread* ptr = thread_.get();
    return ptr ? ptr->isIdle() : false;
}

bool OpenThreadRef::isRunning()
{
    OpenThread* ptr = thread_.get();
    return ptr ? ptr->isRunning() : false;
}

bool OpenThreadRef::waitStop(int64_t milliSecond)
{
    OpenThread* ptr = thread_.get();
    if (!ptr) return false;
    if (!ptr->isRunning())
    {
        return true;
    }
    if (ptr->isCurrent())
    {
        return false;
    }
    while (ptr->isRunning())
    {
        OpenThread::Sleep(milliSecond);
    }
    return true;
}

int OpenThreadRef::pid()
{
    OpenThread* ptr = thread_.get();
    return ptr ? ptr->pid() : -1;
}

const std::string& OpenThreadRef::name()
{
    static const std::string Empty;
    OpenThread* ptr = thread_.get();
    return ptr ? ptr->name() : Empty;
}


//OpenSync
OpenSync::OpenSyncRef::OpenSyncRef()
{
    isSleep_ = false;
    pthread_mutex_init(&mutex_, NULL);
    pthread_cond_init(&cond_, NULL);
}

OpenSync::OpenSyncRef::OpenSyncRef(const OpenSyncRef& that)
{
    assert(false);
    isSleep_ = that.isSleep_;
    pthread_mutex_init(&mutex_, NULL);
    pthread_cond_init(&cond_, NULL);
}

OpenSync::OpenSyncRef::~OpenSyncRef()
{
    pthread_mutex_destroy(&mutex_);
    pthread_cond_destroy(&cond_);
}

bool OpenSync::OpenSyncRef::await()
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

bool OpenSync::OpenSyncRef::wakeup()
{
    if (isSleep_)
    {
        isSleep_ = 0;
        pthread_cond_signal(&cond_);
        return true;
    }
    return false;
}

bool OpenSync::OpenSyncRef::wakeup(const std::shared_ptr<void>& data)
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


void OpenThread::Sleep(int64_t milliSecond)
{
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    ::Sleep((DWORD)milliSecond);
#else
    ::usleep(milliSecond * 1000);
#endif
}

int64_t OpenThread::MilliUnixtime()
{
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    int64_t ft = 0;
    ::GetSystemTimeAsFileTime((LPFILETIME)&ft);
    int64_t milliSecond = (ft / 10000000 - 11644473600LL) * 1000 + (ft / 10) % 1000000;
    return milliSecond;
#else
    struct timeval tv;
    ::gettimeofday(&tv, NULL);
    int64_t milliSecond = tv.tv_sec * 1000 + tv.tv_usec;
    return milliSecond;
#endif
}

int64_t OpenThread::ThreadTime()
{
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    int64_t ft = 0;
    ::GetSystemTimeAsFileTime((LPFILETIME)&ft);
    int64_t tt = (ft / 10000000 - 11644473600LL) * 1000000 + (ft / 10) % 1000000000;
#else
#define NANOSEC 1000000000
#define MICROSEC 1000000
#if  !defined(__APPLE__) || defined(AVAILABLE_MAC_OS_X_VERSION_10_12_AND_LATER)
    struct timespec ti;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ti);
    int64_t tt = ti.tv_sec * MICROSEC + ti.tv_nsec / (NANOSEC / MICROSEC);
#else
    struct task_thread_times_info aTaskInfo;
    mach_msg_type_number_t aTaskInfoCount = TASK_THREAD_TIMES_INFO_COUNT;
    if (KERN_SUCCESS != task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t)&aTaskInfo, &aTaskInfoCount)) {
        return 0;
    }
    int64_t tt = aTaskInfo.user_time.seconds * MICROSEC + aTaskInfo.user_time.microseconds;
#endif
#endif
    return tt;
}

};


#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#ifdef __cplusplus
extern "C" {
#endif

int pthread_mutex_lock(pthread_mutex_t* _mutex) 
{
    int rc = WaitForSingleObject(*_mutex, INFINITE);
    return rc == WAIT_OBJECT_0 ? 0 : rc;
}

int pthread_mutex_unlock(pthread_mutex_t* _mutex) 
{
    int rc = ReleaseMutex(*_mutex);
    return rc != 0 ? 0 : GetLastError();
}

int pthread_mutex_init(pthread_mutex_t* _mutex, void* ignoredAttr) 
{
    *_mutex = CreateMutex(NULL, FALSE, NULL);   
    return *_mutex == NULL ? GetLastError() : 0;
}

int pthread_mutex_destroy(pthread_mutex_t* _mutex)
{
    int rc = CloseHandle(*_mutex);
    return rc != 0 ? 0 : GetLastError();
}

typedef unsigned(__stdcall* routinefunc)(void*);
int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start_routine1) (void*), void* arg)
{
    int _intThreadId;
    routinefunc start_routine = (routinefunc)start_routine1;
    (*thread).thread_handle = (HANDLE)_beginthreadex(NULL, 0, start_routine, arg, 0, (unsigned int*)&_intThreadId);
    (*thread).thread_id = _intThreadId;
    return (*thread).thread_handle == 0 ? errno : 0;
}

int pthread_equal(pthread_t t1, pthread_t t2) 
{
    return ((t1.thread_id == t2.thread_id) ? 1 : 0);
}

pthread_t pthread_self() 
{
    pthread_t thread_self;
    thread_self.thread_id     = GetCurrentThreadId();
    thread_self.thread_handle = GetCurrentThread();
    return thread_self;
}

int pthread_join(pthread_t _thread, void** ignore)
{
    int rc = WaitForSingleObject(_thread.thread_handle, INFINITE);
    return rc == WAIT_OBJECT_0 ? 0 : rc;
}

int pthread_detach(pthread_t _thread)
{
    int rc = CloseHandle(_thread.thread_handle);
    return rc != 0 ? 0 : GetLastError();
}

void pthread_mutexattr_init(pthread_mutexattr_t* ignore) {}
void pthread_mutexattr_settype(pthread_mutexattr_t* ingore_attr, int ignore) {}
void pthread_mutexattr_destroy(pthread_mutexattr_t* ignore_attr) {}

int pthread_cond_init(pthread_cond_t* cv, const pthread_condattr_t* ignore)
{
    assert(sizeof(CRITICAL_SECTION) <= sizeof(cv->waiters_count_lock_));
    cv->waiters_count_ = 0;
    cv->was_broadcast_ = 0;
    cv->sema_ = CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
    if (cv->sema_ == NULL) return GetLastError();
    CRITICAL_SECTION* lock = (CRITICAL_SECTION*)cv->waiters_count_lock_;
    InitializeCriticalSection(lock);
    cv->waiters_done_ = CreateEvent(NULL, FALSE, FALSE, NULL);
    return (cv->waiters_done_ == NULL) ? GetLastError() : 0;
}

int pthread_cond_destroy(pthread_cond_t* cond)
{
    CloseHandle(cond->sema_);
    CRITICAL_SECTION* lock = (CRITICAL_SECTION*)cond->waiters_count_lock_;
    DeleteCriticalSection(lock);
    return (CloseHandle(cond->waiters_done_) == 0) ? GetLastError() : 0;
}

int pthread_cond_signal(pthread_cond_t* cv)
{
    CRITICAL_SECTION* lock = (CRITICAL_SECTION*)cv->waiters_count_lock_;
    EnterCriticalSection(lock);
    int have_waiters = cv->waiters_count_ > 0;
    LeaveCriticalSection(lock);
    if (!have_waiters) return 0;
    return ReleaseSemaphore(cv->sema_, 1, 0) == 0 ? GetLastError() : 0;
}

int pthread_cond_broadcast(pthread_cond_t* cv)
{
    int have_waiters = 0;
    CRITICAL_SECTION* lock = (CRITICAL_SECTION*)cv->waiters_count_lock_;
    EnterCriticalSection(lock);
    if (cv->waiters_count_ > 0) 
    {
        cv->was_broadcast_ = 1;
        have_waiters = 1;
    }
    if (have_waiters) 
    {
        ReleaseSemaphore(cv->sema_, cv->waiters_count_, 0);
        LeaveCriticalSection(lock);
        WaitForSingleObject(cv->waiters_done_, INFINITE);
        cv->was_broadcast_ = 0;
    }
    else
        LeaveCriticalSection(lock);
    return 0;
}

int pthread_cond_wait(pthread_cond_t* cv, pthread_mutex_t* external_mutex)
{
    int last_waiter = 0;
    CRITICAL_SECTION* lock = (CRITICAL_SECTION*)cv->waiters_count_lock_;
    EnterCriticalSection(lock);
    cv->waiters_count_++;
    LeaveCriticalSection(lock);
    SignalObjectAndWait(*external_mutex, cv->sema_, INFINITE, FALSE);
    EnterCriticalSection(lock);
    cv->waiters_count_--;

    last_waiter = cv->was_broadcast_ && cv->waiters_count_ == 0;
    LeaveCriticalSection(lock);
    if (last_waiter)
        SignalObjectAndWait(cv->waiters_done_, *external_mutex, INFINITE, FALSE);
    else
        WaitForSingleObject(*external_mutex, INFINITE);
    return 0;
}

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
    int result = 0;
    pthread_key_t* newkey = (pthread_key_t*)calloc(1, sizeof(pthread_key_t));
    if (newkey == NULL)
    {
        result = ENOMEM;
    }
    else if ((newkey->key = TlsAlloc()) == TLS_OUT_OF_INDEXES)
    {
        result = EAGAIN;
        free(newkey);
        newkey = NULL;
    }
    else if (destructor != NULL)
    {
        newkey->destructor = destructor;
    }
    key = newkey;
    return result;
}

int pthread_key_delete(pthread_key_t key)
{
    LPVOID lpvData = TlsGetValue(key.key);
    int rc = TlsFree(key.key);
    rc = (rc != 0) ? 0 : GetLastError();
    if (key.destructor != NULL && lpvData != 0) 
    {
        key.destructor(lpvData);
    }
    free(&key);
    return (rc);
}

void* pthread_getspecific(pthread_key_t key)
{
    LPVOID lpvData = TlsGetValue(key.key);
    if (lpvData == 0 && GetLastError() != ERROR_SUCCESS)
        return NULL;
    else
        return lpvData;
}

int pthread_setspecific(pthread_key_t key, const void* value)
{
    int rc = TlsSetValue(key.key, (LPVOID)value);
    return rc != 0 ? 0 : GetLastError();
}

int pthread_setname_np(pthread_t thread, const char* name)
{
    return 0;
}
int pthread_getname_np(pthread_t thread, char* name, size_t len)
{
    return 0;
}
#ifdef __cplusplus
}
#endif

#endif

