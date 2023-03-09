# OpenThread
Cross-platform multi-threaded design!Three designed pattern: Await pattern, Worker pattern and Actor pattern.
OpenThread is the most comfortable cross-platform multi-threaded concurrent library. 

Using elegant methods to create threads, manage threads and communicate between threads to achieve multi-core concurrency. 

OpenThread has no dependencies and is designed for all platforms with only two source files, making it easy for beginners to play with C++ multi-threading development. 

**The OpenLinyou project designs a cross-platform server framework. Write code in VS or XCode and run it on Linux without any changes, even on Android and iOS.**
OpenLinyou：https://github.com/openlinyou

## Cross-platform support 
Designed for cross-platforms such as Windows and Linux.

## Compilation and execution
Please install the cmake tool and use it to build the project. It can be compiled and run on VS or Xcode. 

Source code: https://github.com/openlinyou/openthread
```
# Clone the project
git clone https://github.com/openlinyou/openthread
cd ./openthread
# Create a build project directory
mkdir build
cd build
# If it is win32, openthread.sln will appear in this directory. Click it to start VS for coding and debugging.
cmake ..
make
./helloworld
```

## All source files
+ src/openthread.h
+ src/openthread.cpp

## Technical Features
The technical features of OpenThread: 

1. Cross-platform design that provides a unified pthread interface for Linux. 

2. Thread pool management uses smart pointers and lock-free maps to achieve efficient access to thread objects.

3. Each thread has its own message queue. Messages are atomically locked when placed in the queue, while reading from the message queue is a lock-free operation. This ensures efficient exchange of information between threads. 

4. Thread interaction data is managed using smart pointers to achieve automated memory management without worrying about memory leaks.

5. Three major design patterns for multithreading: Await pattern, Worker pattern and Actor pattern.

## Three major design patterns for multithreading development

1. Await pattern. Two threads, one thread requests the other thread and blocks waiting; the other thread receives the request and returns data to wake up the first thread; the first thread wakes up and continues execution with the data.

2. Worker pattern. Suitable for clients, create a certain number of worker threads to form a factory and provide a unique interface service to the outside.

3. Actor pattern. Suitable for servers, one thread per Actor, different Actors are responsible for different functions.

## 1.Create Thread HelloWorld
```C++
// This function will be called when the child thread receives one of 
//three types of messages: thread start, exit and receive message.
void TestThread(OpenThreadMsg& msg)
{
    if (msg.state_ == OpenThread::START)
    {
        printf("Hello OpenThread\n");
        // Sleep for 1 second
        OpenThread::Sleep(1000);
        // Exit thread
        msg.thread().stop();
    }
}
int main()
{
    // Create a thread, name it and set the child thread's run function to TestThread
    auto thread = OpenThread::Create("Thread", TestThread);
    // Wait for the child thread to exit
    OpenThread::ThreadJoin(thread);
    printf("Pause\n");
    return getchar();
}
```

## 2.Await Model
Create an OpenSyncReturn object in the main thread, send it to the sub-thread and block waiting for the sub-thread to return. 

After receiving the message, the sub-thread sends a message to wake up and sends an OpenSync object to the main thread, waiting for the main thread to respond. 

After being awakened, the main thread receives the OpenSync object carried by the sub-thread message and wakes up the sub-thread.
```C++
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include "openthread.h"

using namespace open;

// Test1
struct TestData
{
    std::string data_;
};
struct Test1Data
{
    std::string data_;
    OpenSync openSync_;
    ~Test1Data()
    {
        printf("Test1:~Test1Data\n");
    }
};

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
        OpenSyncReturn<TestData, Test1Data>* data = msg.edit<OpenSyncReturn<TestData, Test1Data>>();
        if (data)
        {
            std::shared_ptr<TestData> str = data->get();
            if (str)
            {
                assert(str->data_ == "Waiting for you!");
            }
            auto sptr = std::shared_ptr<Test1Data>(new Test1Data);
            sptr->data_.assign("Of Course,I Still Love You!");
            data->wakeup(sptr);

            //wait receive
            sptr->openSync_.await();
        }
        OpenThread::Sleep(1000);
    }
    else if (msg.state_ == OpenThread::STOP)
    {
        printf("Test1Thread[%s] STOP\n", msg.name().c_str());
        OpenThread::Sleep(1000);
    }
}

int main()
{
    // create and start thread
    auto threadRef = OpenThread::Create("Test1Thread");
    threadRef.start(Test1Thread);

    // send msg to thread
    auto msg = std::shared_ptr<OpenSyncReturn<TestData, Test1Data>>(new OpenSyncReturn<TestData, Test1Data>);
    {
        auto data = std::shared_ptr<TestData>(new TestData);
        data->data_ = "Waiting for you!";
        msg->put(data);
    }
    threadRef.send(msg);
    auto ret = msg->awaitReturn();
    if (ret)
    {
        assert(ret->data_ == "Of Course,I Still Love You!");
        printf("Test1====>>:%s\n", ret->data_.c_str());

        //wake up wait.
        ret->openSync_.wakeup();
    }
    // stop thread
    threadRef.stop();

    // wait stop
    OpenThread::ThreadJoin(threadRef);
    printf("Pause\n");
    return getchar();
}
```

## 3.Communication between threads
Create sub-threads dog and cat respectively, and communicate with each other between sub-thread dog and sub-thread cat. 
This is a small story about a dog walking a cat.
```C++
#include <assert.h>
#include <stdio.h>
#include "openthread.h"
using namespace open;
//Sub-threads dog
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
        //Message from the main thread.
        if (*data == "Hello dog! Catch cat!")
        {
        	//Send a message to the cat sub-thread.
            auto data = OpenThread::MakeShared<std::string>();
            data->assign("Hello cat! Catch you!");
            auto cat = OpenThread::Thread("cat");
            if (cat && !cat.send(data))
            {
                printf("Test2ThreadDog[%s] send failed\n", msg.name().c_str());
            }
        }
        //Message from the cat sub-thread.
        else if (*data == "Bang dog!")
        {
        	//Close the cat sub-thread.
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
//Sub-threads cat
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
        //Message from the dog sub-thread.
        if (*data == "Hello cat! Catch you!")
        {
            auto data = OpenThread::MakeShared<std::string>();
            data->assign("Bang dog!");
            // Send a message to the dog sub-thread.
            if (!OpenThread::Send("dog", data))
            {
                printf("Test2ThreadCat[%s] send failed\n", msg.name().c_str());
            }
        }
        break;
    }
    case OpenThread::STOP:
        printf("Test2ThreadCat[%s] STOP\n", msg.name().c_str());
        // The dog thread closed the cat, and before closing, 
        // the cat thread also closed the dog thread and fought back.
        OpenThread::Stop("dog");
        break;
    default:
        break;
    }
}
int main()
{
    // Create sub-threads dog and cat
    auto dog = OpenThread::Create("dog", Test2ThreadDog);
    auto cat = OpenThread::Create("cat", Test2ThreadCat);
    // Send a message to the dog sub-thread
    auto data = OpenThread::MakeShared<std::string>();
    data->assign("Hello dog! Catch cat!");
    if (!dog.send(data))
    {
        printf("Test2Thread send failed\n");
    }
    // Wait for sub-threads to exit
    OpenThread::ThreadJoin({ "dog", "cat" });
    return getchar();
}
```

## 4.Batch creation and management of threads
Batch creation and management of threads When OpenThread starts, it will default to setting the maximum number of threads that can be created. 

After exceeding this number, it cannot be modified. So at program startup, you can use OpenThread::Init(256) to specify the maximum number of threads. 

The main goal of threads is to leverage multi-core performance. Creating too many threads will result in performance loss. 

It is best to have twice the number of CPU cores for the number of threads.

Try to avoid frequent creation and destruction of threads. To prevent confusion between threads, a thread pool OpenThreadPool was designed. 

You can configure dedicated thread pools for different businesses.
```C++
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include "openthread.h"
using namespace open;

void Test3Thread1(OpenThreadMsg& msg)
{
}
void Test3Thread2(OpenThreadMsg& msg)
{
}
void Test3()
{
	// Specify the maximum number of threads that can be created. This can only be modified at program startup.
    OpenThread::Init(256);
    size_t capacity = OpenThread::GetThreadCapacity();
    assert(capacity == 256)
    for (size_t pid = 0; pid < capacity; pid++)
    {
        // OpenThread::Thread queries the OpenThread thread object
        auto threadRef = OpenThread::Thread("Thread_" + std::to_string(pid));
        // Since no threads have been created yet, it is null
        assert(!threadRef);
    }
    // Total number of thread names. Thread names exist once specified.
    assert(OpenThread::GetThreadSize() == 0);
    
    // Create a smart pointer object to send to the sub-thread. The string "sendMsg"
    auto data = OpenThread::MakeShared<std::string>();
    data->assign("sendMsg");
    std::string name;
    for (int pid = 0; pid < capacity; pid++)
    {
        name = "Thread_" + std::to_string(pid);
        // OpenThread::Create creates a thread with a specified name. If a thread with that name already exists, it returns that thread.
        // Once successful, the thread will have a name. You can view it using top -Hp. Windows systems do not have thread names.
        auto threadRef = OpenThread::Create(name, Test3Thread1);
        assert(threadRef && threadRef.pid() == pid && threadRef.name() == name);
        //Three ways to send messages to sub-threads: via the thread object, via the thread ID (not the system thread ID but the array index ID), and via the thread name
        threadRef.send(data);
        OpenThread::Send(pid, data);
        OpenThread::Send(name, data);
        printf("Test3 create %s\n", name.c_str());
    }
    assert(OpenThread::GetThreadSize() == capacity);
    for (size_t pid = 0; pid < capacity; pid++)
    {
        name = "Thread_" + std::to_string(pid);
        // Query thread by thread name. Querying threads by thread name is less efficient than querying by thread id.
        auto threadRef = OpenThread::Thread(name);
        assert(threadRef && threadRef.name() == name);
        //Stop child thread
        threadRef.stop();
    }
    printf("Test3 do stop\n");
    //Wait for all child threads to close and exit
    OpenThread::ThreadJoinAll();
    printf("Test3 finish waitStop\n");
    // Create child threads again. Child thread names will always exist and occupy capacity.
    // Unless you call OpenThread::StopAll() to close and clean up all child threads and start over.
    for (size_t pid = 0; pid < capacity; pid++)
    {
        name = "Thread_" + std::to_string(pid);
        auto threadRef = OpenThread::Create(name, Test3Thread2);
        assert(threadRef && threadRef.pid() == pid && threadRef.name() == name);
    }
    printf("Test3 finish create again\n");
    // The number of child thread names exceeds the maximum capacity, so creating with "over_boundary" fails
    auto threadRef = OpenThread::Create("over_boundary");
    assert(!threadRef);
    // Close and exit all threads and clean up
    OpenThread::StopAll();
}
//线程池测试
void Test5Thread2(OpenThreadMsg& msg)
{
    if (msg.state_ == OpenThread::START)
    {
        printf("Test1Thread[%s] START\n", msg.name().c_str());
        OpenThread::Sleep(1000);
    }
    else if (msg.state_ == OpenThread::RUN)
    {
        // recevie msg
        printf("Test1Thread[%s] RUN\n", msg.name().c_str());
        OpenThread::Sleep(1000);
    }
    else if (msg.state_ == OpenThread::STOP)
    {
        printf("Test1Thread[%s] STOP\n", msg.name().c_str());
        OpenThread::Sleep(1000);
    }
}
//Thread pool test.
void Test5()
{
    //Create a new thread pool. 
    OpenThreadPool pool;
    pool.init(64);

    auto thread = pool.create("Independent");
    if (thread)
    {
        thread->start(Test5Thread2);
        thread->stop();
    }
    //Stop all threads in this thread pool.
    pool.stopAll();
    pool.threadJoinAll();
}
int main()
{
    Test3();
    Test5();
    printf("Pause\n");
    return getchar();
}
```

## 5.Actor Model
Use the Worker class to encapsulate OpenThread, one thread for one Worker business. 

Inspector (monitor), Timer (timer) and Server (server) inherit from Worker. 
Inspector is responsible for monitoring the running information of multiple Timers and performing load balancing. 

Timer provides timer services. When started, it registers with Inspector and provides running information. 

Server queries available Timers from Inspector and then requests timer services from this Timer.
```C++
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <map>
#include <unordered_map>
#include "openthread.h"

using namespace open;
//Assume it's a Google protobuf object
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
class Worker:public OpenThreader
{
public:
    Worker(const std::string& name)
        :OpenThreader(name)
    {
    }
    virtual ~Worker() {}
    virtual void start()
    {
        mapKeyFunc_["msg_from_main"] = { (Handle)&Worker::msg_from_main };
        OpenThreader::start();
    }
    void msg_from_main(const Data& data)
    {
    }
    void onData(const Data& data)
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
    virtual void onMsg(OpenThreadMsg& msg)
    {
        const Data* data = msg.data<Data>();
        if (data) onData(*data);
    }
    // proto will be delete
    bool send(int sid, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>[%s] key:%s\n", name_.c_str(), ThreadName(sid).c_str(), key.c_str());
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
    virtual void stop()
    {
        OpenThreader::stop();
    }
    static bool Send(std::vector<std::string>& vectName, const std::string& key, ProtoBuffer* proto)
    {
        printf("Send=>size[%d] key:%s\n", (int)vectName.size(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(-1, "Global", key, proto));
        bool ret = OpenThread::Send(vectName, data);
        assert(ret);
        return ret;
    }
protected:
    void sendLoop(const std::string& key)
    {
        auto proto = new ProtoBuffer;
        send(pid(), key, proto);
    }
    bool canLoop()
    {
        OpenThread* p = thread_.get();
        return p ? (p->isRunning() && !p->hasMsg()) : false;
    }
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
        while (inspectorId_ < 0)
        {
            inspectorId_ = ThreadId("Inspector");
            if (inspectorId_ >= 0)
            {
                auto proto = new TimerInfoMsg;
                proto->workerId_ = pid();
                proto->dataTime_ = OpenThread::MilliUnixtime();
                proto->cpuCost_ = thread_->cpuCost();
                proto->leftCount_ = thread_->leftCount();
                send(inspectorId_, "return_timer_info", proto);
                break;
            }
            OpenThread::Sleep(100);
        }
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
            inspectorId_ = ThreadId("Inspector");
        }
        return inspectorId_;
    }
    virtual void onStart()
    {
        while (inspectorId_ < 0)
        {
            inspectorId_ = ThreadId("Inspector");
            OpenThread::Sleep(100);
        }
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

int main()
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
        vectName.push_back(vectWorker[i]->name());
        vectWorker[i]->start();
    }
    // all working, send "msg_from_main" msg;
    auto msg = new ProtoBuffer;
    Worker::Send(vectName, "msg_from_main", msg);

    OpenThread::ThreadJoinAll();
    for (size_t i = 0; i < vectWorker.size(); i++)
    {
        delete vectWorker[i];
    }
    vectWorker.clear();
    printf("Pause\n");
    return getchar();
}
```

## 6.Worker Model
Create a certain number of worker threads to form a factory and provide a unique interface service to the outside.
```C++
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <vector>
#include "openthread.h"
using namespace open;

struct Product
{
    int id_;
    std::string goods_;
    Product() :id_(0) {}
};
class Worker : public OpenThreader
{   
    template <class T>
    struct Task
    {
        std::shared_ptr<T> data_;
        OpenSync openSync_;
        Task() :data_(0) {}
    };
    //Factory
    class Factory
    {
        const std::vector<Worker*> vectWorker_;
    public:
        Factory()
        :vectWorker_({
            new Worker("Producer1"),
            new Worker("Producer2"),
            new Worker("Producer3"),
            new Worker("Producer4"),
            }) {}
        Worker* getWorker()
        {
            if (vectWorker_.empty()) return 0;
            return vectWorker_[std::rand() % vectWorker_.size()];
        }
    };
    static Factory Instance_;
    // Worker
    Worker(const std::string& name)
        :OpenThreader(name)
    {
        uid_ = 1;
        start();
    }
    ~Worker()
    {
        for (size_t i = 0; i < vectTask_.size(); ++i)
            vectTask_[i].openSync_.wakeup();
    }
    virtual void onMsg(OpenThreadMsg& msg)
    {
        Task<Product>* task = msg.edit<Task<Product>>();
        if (task)
        {
            vectTask_.push_back(*task);
        }
        if (rand() % 2 == 0)
        {
            OpenThread::Sleep(1000);
        }
        for (size_t i = 0; i < vectTask_.size(); ++i)
        {
            auto& task = vectTask_[i];
            if (task.data_)
            {
                task.data_->id_ = pid_ + 100 * uid_++;
                task.data_->goods_ = name_ + " Dog coin" + std::to_string(task.data_->id_);
            }
            task.openSync_.wakeup();
        }
        vectTask_.clear();
    }
    int uid_;
    std::vector<Task<Product>> vectTask_;
public:
    static bool MakeProduct(std::shared_ptr<Product>& product)
    {
        auto worker = Instance_.getWorker();
        if (!worker)  return false;
        auto proto = std::shared_ptr<Task<Product>>(new Task<Product>);
        proto->data_ = product;
        bool ret = OpenThread::Send(worker->pid(), proto);
        assert(ret);
        proto->openSync_.await();
        return ret;
    }
};
Worker::Factory Worker::Instance_;

void TestThread(OpenThreadMsg& msg)
{
    if (msg.state_ == OpenThread::START)
    {
        for (size_t i = 0; i < 100; i++)
        {
            auto product = std::shared_ptr<Product>(new Product());
            Worker::MakeProduct(product);
            printf("[%s] Recevie Product:%s\n", msg.name().c_str(), product->goods_.c_str());
        }
        msg.thread().stop();
    }
}
int main()
{
    OpenThread::Create("TestThread1", TestThread);
    OpenThread::Create("TestThread2", TestThread);
    OpenThread::Create("TestThread3", TestThread);
    OpenThread::Create("TestThread4", TestThread);
    // wait stop
    OpenThread::ThreadJoinAll();
    printf("Pause\n");
    return getchar();
}
```