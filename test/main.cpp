#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <map>
#include <unordered_map>
#include "openthread.h"

using namespace open;

// Test0
void Test0Thread(OpenThreadMsg& msg)
{
    if (msg.state_ == OpenThread::START)
    {
        printf("Hello OpenThread\n");
        OpenThread::Sleep(1000);
        msg.thread().stop();
    }
}
void Test0()
{
    // create and start thread
    auto thread = OpenThread::Create("Test0Thread", Test0Thread);
    // wait stop
    OpenThread::ThreadJoin(thread);
    printf("Complete Test0\n\n");
}

// Test1
struct Test1Data
{
    std::string data_;
    ~Test1Data()
    {
        printf("Test1:~Test1Data\n");
    }
};
void Test1Thread(OpenThreadMsg& msg);
void Test1()
{
    // create and start thread
    auto threadRef = OpenThread::Create("Test1Thread");
    threadRef.start(Test1Thread);

    // send msg to thread
    auto msg = OpenThread::MakeShared<OpenSync>();
    threadRef.send(msg);
    const Test1Data* data = msg->sleepBack<Test1Data>();
    if (data)
    {
        printf("Test1====>>:%s\n", data->data_.c_str());
    }
    // or 
    //auto msg = std::make_shared<OpenSyncRef>();
    //threadRef.send(msg);
    // stop thread
    threadRef.stop();

    // wait stop
    OpenThread::ThreadJoin(threadRef);
    printf("Complete Test1\n\n");
    OpenThread::Sleep(2000);
}

//Test2
void Test2ThreadDog(OpenThreadMsg& msg);
void Test2ThreadCat(OpenThreadMsg& msg);
void Test2()
{
    // dog and cat
    auto dog = OpenThread::Create("dog", Test2ThreadDog);
    auto cat = OpenThread::Create("cat", Test2ThreadCat);

    // send dog
    auto data = OpenThread::MakeShared<std::string>();
    data->assign("Hello dog! Catch cat!");
    if (!dog.send(data))
    {
        printf("Test2Thread send failed\n");
    }
    // wait thread stop
    OpenThread::ThreadJoin({ "dog", "dog" });
    printf("Complete Test2\n\n");
}

// Test3
void Test3Thread1(OpenThreadMsg& msg)
{
}
void Test3Thread2(OpenThreadMsg& msg)
{
}
void Test3()
{
    OpenThread::StopAll();
    size_t capacity = OpenThread::GetThreadCapacity();
    for (size_t pid = 0; pid < capacity; pid++)
    {
        auto threadRef = OpenThread::Thread("Thread_"+std::to_string(pid));
        assert(!threadRef);
    }
    assert(OpenThread::GetThreadSize() == 0);
    auto data = OpenThread::MakeShared<std::string>();
    data->assign("sendMsg");
    std::string name;
    for (int pid = 0; pid < capacity; pid++)
    {
        name = "Thread_" + std::to_string(pid);
        auto threadRef = OpenThread::Create(name, Test3Thread1);
        assert(threadRef && threadRef.pid() == pid && threadRef.name() == name);
        threadRef.send(data);
        OpenThread::Send(pid, data);
        OpenThread::Send(name, data);
        printf("Test3 create %s\n", name.c_str());
    }
    assert(OpenThread::GetThreadSize() == capacity);
    for (size_t pid = 0; pid < capacity; pid++)
    {
        name = "Thread_" + std::to_string(pid);
        auto threadRef = OpenThread::Thread(name);
        assert(threadRef && threadRef.name() == name);
        threadRef.stop();
    }
    printf("Test3 do stop\n");
    OpenThread::ThreadJoinAll();
    printf("Test3 finish waitStop\n");

    // create again
    for (size_t pid = 0; pid < capacity; pid++)
    {
        name = "Thread_" + std::to_string(pid);
        auto threadRef = OpenThread::Create(name, Test3Thread2);
        assert(threadRef && threadRef.pid() == pid && threadRef.name() == name);
    }
    printf("Test3 finish create again\n");
    auto threadRef = OpenThread::Create("over_boundary");
    assert(!threadRef);
    OpenThread::StopAll();
    printf("Complete Test3\n\n");
}

struct ProtoBuffer
{
    virtual ~ProtoBuffer() {}
};

class Data
{
    ProtoBuffer* proto_;
public:
    Data() :proto_(0), srcPid_(-1) {}
    Data(int pid, const std::string& name, const std::string& key, ProtoBuffer* proto)
        :srcPid_(pid), srcName_(name), rpc_(key), proto_(proto) {}
    ~Data()
    {
        if (proto_) delete proto_;
        proto_ = 0;
    }
    int srcPid_;
    std::string rpc_;
    std::string srcName_;
    template <class T>
    const T& proto() const 
    {
        T* p = dynamic_cast<T*>(proto_);
        if (p) return *p;
        static T Empty_;
        return Empty_;
    }
};

class Worker;
typedef void(Worker::*Handle)(const Data&);
struct Rpc
{
    Handle handle_;
};
class Worker
{
public:
    Worker(const std::string& name)
        :name_(name)
        ,sessionId_(0)
    {
        mapKeyFunc_["do_start"] = { (Handle)&Worker::do_start };
    }
    ~Worker()
    {
        stop();
    }
    void start()
    {
        auto threadRef = OpenThread::Thread(name_);
        assert(!threadRef);
        threadRef = OpenThread::Create(name_);
        assert(threadRef);
        if (threadRef)
        {
            thread_ = OpenThread::GetThread(threadRef);
            assert(!thread_->isRunning());
            thread_->setCustom(this);
            thread_->start(Worker::Thread);
        }
    }
    void do_start(const Data& data)
    {
        onStart();
    }
    virtual void onInit()
    {
    }
    virtual void onStart()
    {
    }
    virtual void onMsg(const Data& data)
    {
        printf("[%s]receive<<=[%s] key:%s\n", name_.c_str(), data.srcName_.c_str(), data.rpc_.c_str());
        auto iter = mapKeyFunc_.find(data.rpc_);
        if (iter != mapKeyFunc_.end())
        {
            auto& rpc = iter->second;
            if (rpc.handle_)
            {
                (this->*rpc.handle_)(data);
                return;
            }
        }
        printf("[%s]no implement key:%s\n", name_.c_str(), data.rpc_.c_str());
    }
    // proto will be delete
    bool send(int sid, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>[%s] key:%s\n", name_.c_str(), WorkerName(sid).c_str(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(sid, data);
        //assert(ret);
        return ret;
    }
    // proto will be delete
    bool send(const std::string& name, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>[%s] key:%s\n", name_.c_str(), name.c_str(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(name, data);
        //assert(ret);
        return ret;
    }
    bool send(std::vector<int>& vectSid, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>size[%d] key:%s\n", name_.c_str(), (int)vectSid.size(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(vectSid, data);
        //assert(ret);
        return ret;
    }
    bool send(std::vector<std::string>& vectName, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>size[%d] key:%s\n", name_.c_str(), (int)vectName.size(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(vectName, data);
        //assert(ret);
        return ret;
    }
    void stop()
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
        {
            thread_.reset();
        }
    }
    virtual void onStop()
    {
    }
    static void Thread(OpenThreadMsg& msg)
    {
        Worker* that = msg.custom<Worker>();
        if (!that)
        {
            assert(false); return;
        }
        switch (msg.state_)
        {
        case OpenThread::RUN: 
            break;
        case OpenThread::START:
            that->onInit(); return;
        case OpenThread::STOP:
            that->onStop(); return;
        default:
            assert(false); return;
        }
        const Data* data = msg.data<Data>();
        if (data) that->onMsg(*data);
    }
    static bool Send(std::vector<std::string>& vectName, const std::string& key, ProtoBuffer* proto)
    {
        printf("Send=>size[%d] key:%s\n", (int)vectName.size(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(-1, "Global", key, proto));
        bool ret = OpenThread::Send(vectName, data);
        assert(ret);
        return ret;
    }

    const std::string name_;
    static inline int WorkerId(const std::string& name) { return OpenThread::ThreadId(name); }
    static inline const std::string& WorkerName(int pid) { return OpenThread::ThreadName(pid); }
protected:
    void sendLoop(const std::string& key)
    {
        auto proto = new ProtoBuffer;
        send(pid(), key, proto);
    }
    int pid()
    {
        OpenThread* p = thread_.get();
        return p ? p->pid() : -1;
    }
    bool canLoop()
    {
        OpenThread* p = thread_.get();
        return p ? (p->isRunning() && !p->hasMsg()) : false;
    }
    int sessionId_;
    std::shared_ptr<OpenThread> thread_;
    std::unordered_map<std::string, Rpc> mapKeyFunc_;
};

struct TimerEventMsg :public ProtoBuffer
{
    int workerId_;
    int64_t deadline_;
};

struct TimerInfoMsg :public ProtoBuffer
{
    TimerInfoMsg() 
        :workerId_(0), leftCount_(0), cpuCost_(0), dataTime_(0) {}
    int workerId_;
    size_t leftCount_;
    int64_t cpuCost_;
    int64_t dataTime_;
};

class Inspector:public Worker
{
    std::unordered_map<std::string, TimerInfoMsg> mapTimerInfo_;
    std::vector<int> vectQueryId;
public:
    Inspector(const std::string& name):Worker(name)
    {
        mapKeyFunc_["start_inspect"] = { (Handle)&Inspector::start_inspect };
        mapKeyFunc_["query_timer_info"] = { (Handle)&Inspector::query_timer_info };
        mapKeyFunc_["return_timer_info"] = { (Handle)&Inspector::return_timer_info };
    }
    virtual void onStart()
    {
    }
    void start_inspect(const Data& data)
    {
        std::vector<int> vectPid;
        vectPid.reserve(mapTimerInfo_.size());
        for (auto iter = mapTimerInfo_.begin(); iter != mapTimerInfo_.end(); iter++)
        {
            if(iter->second.workerId_ >= 0)
                vectPid.push_back(iter->second.workerId_);
        }
        auto proto = new ProtoBuffer;
        send(vectPid, "get_timer_info", proto);
    }

    void return_timer_info(const Data& data)
    {
        auto& proto = data.proto<TimerInfoMsg>();
        auto& timerInfo = mapTimerInfo_[data.srcName_];
        timerInfo = proto;
        if (!vectQueryId.empty())
        {
            auto data = new TimerInfoMsg;
            *data = proto;
            send(vectQueryId, "query_timer_info", data);
            vectQueryId.clear();
        }
    }
    void query_timer_info(const Data& data)
    {
        TimerInfoMsg* tmpInfo = 0;
        auto curTime = OpenThread::MilliUnixtime();
        for (auto iter = mapTimerInfo_.begin(); iter != mapTimerInfo_.end(); iter++)
        {
            auto& info = iter->second;
            if (curTime > info.dataTime_ + 10000) continue;
            if (tmpInfo)
            {
                if (tmpInfo->leftCount_ > info.leftCount_ || tmpInfo->cpuCost_ > info.cpuCost_)
                    tmpInfo = &info;
            }
            else
            {
                tmpInfo = &info;
            }
        }
        if (!tmpInfo)
        {
            vectQueryId.push_back(data.srcPid_);
            sendLoop("start_inspect");
        }
        else
        {
            auto proto = new TimerInfoMsg;
            *proto = *tmpInfo;
            send(data.srcPid_, "query_timer_info", proto);
        }
    }
};


class Timer:public Worker
{
    int inspectorId_;
    std::multimap<int64_t, int> mapTimerEvent;
public:
    Timer(const std::string& name):Worker(name) 
    {
        inspectorId_ = -1;
        mapKeyFunc_["start_timer"] = { (Handle)&Timer::start_timer };
        mapKeyFunc_["get_timer_info"] = { (Handle)&Timer::get_timer_info };
        mapKeyFunc_["request_timer"] = { (Handle)&Timer::request_timer };
    }
    virtual void onStart()
    {
        sendLoop("start_timer");
    }
    void start_timer(const Data& data)
    {
        int64_t curTime = 0;
        while (canLoop())
        {
            if (!mapTimerEvent.empty())
            {
                curTime = OpenThread::MilliUnixtime();
                while (!mapTimerEvent.empty())
                {
                    auto iter = mapTimerEvent.begin();
                    if (curTime > iter->first)
                    {
                        auto proto = new TimerEventMsg;
                        proto->workerId_ = pid();
                        proto->deadline_ = curTime;
                        send(iter->second, "return_timer", proto);

                        mapTimerEvent.erase(iter);
                    }
                    else
                    {
                        break;
                    }
                }
            }
            if (inspectorId_ < 0)
            {
                inspectorId_ = WorkerId("Inspector");
                if (inspectorId_ >= 0)
                {
                    auto proto = new TimerInfoMsg;
                    proto->workerId_ = pid();
                    proto->dataTime_ = OpenThread::MilliUnixtime();
                    proto->cpuCost_ = thread_->cpuCost();
                    proto->leftCount_ = thread_->leftCount();
                    send(inspectorId_, "return_timer_info", proto);
                }
            }
            OpenThread::Sleep(10);
        }
    }
    // provide timer info
    void get_timer_info(const Data& data)
    {
        auto proto = new TimerInfoMsg;
        proto->workerId_  = pid();
        proto->dataTime_  = OpenThread::MilliUnixtime();
        proto->cpuCost_   = thread_->cpuCost();
        proto->leftCount_ = thread_->leftCount();
        send(data.srcPid_, "return_timer_info", proto);
        sendLoop("start_timer");
    }
    void request_timer(const Data& data)
    {
        auto& proto = data.proto<TimerEventMsg>();
        mapTimerEvent.insert({ proto.deadline_, data.srcPid_ });
        sendLoop("start_timer");
    }
};

class Server:public Worker
{
    int inspectorId_;
    int collect_;
public:
    Server(const std::string& name)
        :Worker(name) 
        ,inspectorId_(-1)
    {
        collect_ = 0;
        mapKeyFunc_["start_work"] = { (Handle)&Server::start_work };
        mapKeyFunc_["query_timer_info"] = { (Handle)&Server::query_timer_info };
        mapKeyFunc_["return_timer"] = { (Handle)&Server::return_timer };
    }
    int inspectorId()
    {
        if (inspectorId_ < 0)
        {
            inspectorId_ = WorkerId("Inspector");
        }
        return inspectorId_;
    }
    virtual void onStart()
    {
        sendLoop("start_work");
    }
    void start_work(const Data& data)
    {
        while (true)
        {
            int wid = inspectorId();
            if (wid >= 0)
            {
                auto proto = new ProtoBuffer;
                send(wid, "query_timer_info", proto);
                break;
            }
            OpenThread::Sleep(1000);
        }
    }
    void query_timer_info(const Data& data)
    {
        auto& proto = data.proto<TimerInfoMsg>();
        if (proto.workerId_ > 0)
        {
            auto data = new TimerEventMsg;
            int64_t curTime = OpenThread::MilliUnixtime();
            data->deadline_ = curTime + curTime % 2000;
            if (data->deadline_ > curTime + 2000)
            {
                data->deadline_ = curTime;
            }
            send(proto.workerId_, "request_timer", data);
        }
        else
        {
            sendLoop("start_work");
        }
    }
    void return_timer(const Data& data)
    {
        if (collect_++ > 100)
        {
            OpenThread::StopAll();
            return;
        }
        sendLoop("start_work");
    }
    
};

void Test4()
{
    OpenThread::StopAll();
    std::vector<Worker*> vectWorker =
    {
        new Inspector("Inspector"),
        new Timer("timer1"),
        new Timer("timer2"),
        new Server("server1"),
        new Server("server2")
    };
    std::vector<std::string> vectName;
    for (size_t i = 0; i < vectWorker.size(); i++)
    {
        vectName.push_back(vectWorker[i]->name_);
        vectWorker[i]->start();
    }
    // all working, send "start" msg;
    Worker::Send(vectName, "do_start", NULL);

    OpenThread::ThreadJoinAll();
    for (size_t i = 0; i < vectWorker.size(); i++)
    {
        delete vectWorker[i];
    }
    vectWorker.clear();
    printf("Complete Test4\n\n");

}

int main()
{
    Test0();
    Test1();
    Test2();
    Test3();
    Test4();
    printf("Complete All\n\n");
    return getchar();
}

// Test1
void Test1Thread(OpenThreadMsg& msg)
{
    if (msg.state_ == OpenThread::START)
    {
        printf("Test1Thread[%s] START\n", msg.name().c_str());
        OpenThread::Sleep(1000);
    }
    else if (msg.state_ == OpenThread::RUN)
    {
        // recevie msg
        OpenSync* data = (OpenSync*)msg.data<OpenSync>();
        if (data)
        {
            auto sptr = std::shared_ptr<Test1Data>(new Test1Data);
            sptr->data_.assign("Test1Thread come back!");
            data->wakeup(sptr);
        }
        OpenThread::Sleep(1000);
    }
    else if (msg.state_ == OpenThread::STOP)
    {
        printf("Test1Thread[%s] STOP\n", msg.name().c_str());
        OpenThread::Sleep(1000);
    }
}

// Test2
void Test2ThreadDog(OpenThreadMsg& msg)
{
    assert(msg.name() == "dog");
    switch (msg.state_)
    {
    case OpenThread::START:
        printf("Test2ThreadDog[%s] START\n", msg.name().c_str());
        break;
    case OpenThread::RUN: {
        const std::string* data = msg.data<std::string>();
        if (!data) break;
        printf("Test2ThreadDog[%s] MSG:%s\n", msg.name().c_str(), data->c_str());
        if (*data == "Hello dog! Catch cat!")
        {
            auto data = OpenThread::MakeShared<std::string>();
            data->assign("Hello cat! Catch you!");
            auto cat = OpenThread::Thread("cat");
            if (cat && !cat.send(data))
            {
                printf("Test2ThreadDog[%s] send failed\n", msg.name().c_str());
            }
        }
        else if (*data == "Bang dog!")
        {
            // stop cat
            auto cat = OpenThread::Thread("cat");
            cat.stop();
        }
        else
        {
            assert(false);
        }
        break;
    }
    case OpenThread::STOP:
        printf("Test2ThreadDog[%s] STOP\n", msg.name().c_str());
        break;
    default:
        break;
    }
}

void Test2ThreadCat(OpenThreadMsg& msg)
{
    assert(msg.name() == "cat");
    switch (msg.state_)
    {
    case OpenThread::START:
        printf("Test2ThreadCat[%s] START\n", msg.name().c_str());
        break;
    case OpenThread::RUN: {
        const std::string* data = msg.data<std::string>();
        if (!data) break;
        printf("Test2ThreadCat[%s] MSG:%s\n", msg.name().c_str(), data->c_str());

        if (*data == "Hello cat! Catch you!")
        {
            auto data = OpenThread::MakeShared<std::string>();
            data->assign("Bang dog!");
            if (!OpenThread::Send("dog", data))
            {
                printf("Test2ThreadCat[%s] send failed\n", msg.name().c_str());
            }
        }
        break;
    }
    case OpenThread::STOP:
        printf("Test2ThreadCat[%s] STOP\n", msg.name().c_str());
        // counter dog
        OpenThread::Stop("dog");
        break;
    default:
        break;
    }
}