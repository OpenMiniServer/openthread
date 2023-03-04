# OpenThread
Cross-platform multi-threaded design!
OpenThread is the most comfortable cross-platform multi-threaded concurrent library. 

Using elegant methods to create threads, manage threads and communicate between threads to achieve multi-core concurrency. 

OpenThread has no dependencies and is designed for all platforms with only two source files, making it easy for beginners to play with C++ multi-threading development. 

OpenLinyou series project: https://github.com/openlinyou

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
./test
```

## Technical Features
The technical features of OpenThread: 

1. Cross-platform design that provides a unified pthread interface for Linux. 

2. Thread pool management uses smart pointers and lock-free maps to achieve efficient access to thread objects.

3. Each thread has its own message queue. Messages are atomically locked when placed in the queue, while reading from the message queue is a lock-free operation. This ensures efficient exchange of information between threads. 

4. Thread interaction data is managed using smart pointers to achieve automated memory management without worrying about memory leaks.


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

## 2.Await operation
Create an OpenSync object in the current thread, send the OpenSync object to the child thread like a message and block and wait. 

After receiving the message, the child thread sends a message to wake up the blocked thread. Use the OpenSync object to block the current thread and have the child thread return data through OpenSync.
```C++
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include "openthread.h"

using namespace open;

// Test1
struct Test1Data
{
    std::string data_;
    ~Test1Data()
    {
        printf("Test1:~Test1Data\n");
    }
};

// Test1
void Test1Thread(OpenThreadMsg& msg)
{
    //Thread start message
    if (msg.state_ == OpenThread::START)
    {
        printf("Test1Thread[%s] START\n", msg.name().c_str());
        OpenThread::Sleep(1000);
    }
    //Message received by the thread
    else if (msg.state_ == OpenThread::RUN)
    {
        //Received an OpenSync object, wake it up and send a message.
        OpenSync* data = (OpenSync*)msg.data<OpenSync>();
        if (data)
        {
            const std::string* str = data->get<std::string>();
            if (str)
            {
                assert(*str == "Waiting for you!");
            }
            auto sptr = std::shared_ptr<Test1Data>(new Test1Data);
            sptr->data_.assign("Of Course,I Still Love You!");
            data->wakeup(sptr);
        }
        OpenThread::Sleep(1000);
    }
    //Message before thread exit
    else if (msg.state_ == OpenThread::STOP)
    {
        printf("Test1Thread[%s] STOP\n", msg.name().c_str());
        OpenThread::Sleep(1000);
    }
}

int main()
{
    // Create a new OpenThread object with name "Test1Thread"
    auto threadRef = OpenThread::Create("Test1Thread");
    // Start the created OpenThread object and specify its execution function
    threadRef.start(Test1Thread);

    // Create an OpenSync object
    auto msg = std::shared_ptr<OpenSync>(new OpenSync);
    // Create a string object to hold data
    auto data = std::shared_ptr<std::string>(new std::string);
    // Assign data to string object
    data->assign("Waiting for you!");
    // Put string object into OpenSync object
    msg->put(data);
    // Send OpenSync object to child thread
    threadRef.send(msg);
    // Wait for response from child thread
    const Test1Data* ret = msg->awaitReturn<Test1Data>();
    if (ret)
    {
        assert(ret->data_ == "Of Course,I Still Love You!");
        printf("Test1====>>:%s\n", ret->data_.c_str());
    }
    // Stop child thread
    threadRef.stop();
    // Wait for child thread to exit
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

## 5.Design a multi-threaded concurrent framework.
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
	//Virtual function in base class
    virtual ~ProtoBuffer() {}
};
//Intermediate data structure for communication between Worker classes
class Data
{
    ProtoBuffer* proto_; //Data carried
public:
    Data() :proto_(0), srcPid_(-1) {}
    Data(int pid, const std::string& name, const std::string& key, ProtoBuffer* proto)
        :srcPid_(pid), srcName_(name), rpc_(key), proto_(proto) {}
    ~Data()
    {
        if (proto_) delete proto_;
        proto_ = 0;
    }
    int srcPid_; // ID of the sending worker
    std::string rpc_; // Name of the routing function
    std::string srcName_; // Name of the sending worker
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
    	// Query the thread corresponding to name, not created, so it is null
        auto threadRef = OpenThread::Thread(name_);
        assert(!threadRef);
        // Create a thread named name
        threadRef = OpenThread::Create(name_);
        assert(threadRef);
        if (threadRef)
        {
            // Get the real object of the thread for using its advanced features.
            // Note that only advanced features can be used in this thread, otherwise it will cause data conflicts between threads
            thread_ = OpenThread::GetThread(threadRef);
            assert(!thread_->isRunning());
            // Can only be specified once and can only be accessed in child thread.
            thread_->setCustom(this);
            // Specify the function and start the thread,
            thread_->start(Worker::Thread);
        }
    }
    // After all threads are started, the main thread sends a message to call do_start
    void do_start(const Data& data)
    {
        onStart();
    }
    // Called when the thread starts
    virtual void onInit()
    {
    }
    // Called after all threads have started,
    virtual void onStart()
    {
    }
    //This method is called after this child thread receives a message.
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
    // Send a message to the thread with ID sid. This method will call delete to release proto and can only be passed once. Managed by std::shared_ptr
    bool send(int sid, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>[%s] key:%s\n", name_.c_str(), WorkerName(sid).c_str(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(sid, data);
        //assert(ret);
        return ret;
    }
    // Send a message to the thread named name. This method will call delete to release proto and can only be passed once. Managed by std::shared_ptr
    bool send(const std::string& name, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>[%s] key:%s\n", name_.c_str(), name.c_str(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(name, data);
        //assert(ret);
        return ret;
    }
    // Send a message to multiple threads with ID sid. This method will call delete to release proto and can only be passed once. Managed by std::shared_ptr
    bool send(std::vector<int>& vectSid, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>size[%d] key:%s\n", name_.c_str(), (int)vectSid.size(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(vectSid, data);
        //assert(ret);
        return ret;
    }
    // Send a message to multiple threads with Name. This method will call delete to release proto and can only be passed once. Managed by std::shared_ptr
    bool send(std::vector<std::string>& vectName, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>size[%d] key:%s\n", name_.c_str(), (int)vectName.size(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(vectName, data);
        //assert(ret);
        return ret;
    }
    //Stop thread
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
    //Thread exit.
    virtual void onStop()
    {
    }
    //Thread Funciton
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
        //assert(ret);
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
// Imitate protobuff data, request timer data structure
struct TimerEventMsg :public ProtoBuffer
{
    int workerId_;
    int64_t deadline_;
};
// Imitate protobuff data, timer information data structure
struct TimerInfoMsg :public ProtoBuffer
{
    TimerInfoMsg() 
        :workerId_(0), leftCount_(0), cpuCost_(0), dataTime_(0) {}
    int workerId_;
    size_t leftCount_;
    int64_t cpuCost_;
    int64_t dataTime_;
};
//Monitor class, get timer information for load balancing of Timer
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
    //Get information from the timer
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
	// Receive information from the timer
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
    //Provide the most idle timer ID to the Server.
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
//Timer object class
class Timer:public Worker
{
    int inspectorId_;
    //Timed events, from small to large, can have multiple keys. 
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
    //Timing logic, if the timer is triggered, send a timed event 
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
                    	//Trigger the timer and send a timed event
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
            	//Register with the monitor and send your own running information
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
    // Provide timer operation information.
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
    // Receive the timer subscription event from the Server. 
    // After the timer is triggered, send the event back.
    void request_timer(const Data& data)
    {
        auto& proto = data.proto<TimerEventMsg>();
        mapTimerEvent.insert({ proto.deadline_, data.srcPid_ });
        sendLoop("start_timer");
    }
};

// Business service queries the monitor for available timers 
// and then requests a timed event from this timer. 
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
    // Query the monitor for available timers.
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
    // The timer information returned by the monitor is then used to request a timed event from that timer. 
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
    // After the timer is triggered, the timed event is sent back. 
    void return_timer(const Data& data)
    {
    	// After the test times are reached, stop testing and close all threads.
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
	// Create an Inspector, 2 Timers and 2 Servers
    std::vector<Worker*> vectWorker =
    {
        new Inspector("Inspector"),
        new Timer("timer1"),
        new Timer("timer2"),
        new Server("server1"),
        new Server("server2")
    };
    // Start Workers
    std::vector<std::string> vectName;
    for (size_t i = 0; i < vectWorker.size(); i++)
    {
        vectName.push_back(vectWorker[i]->name_);
        vectWorker[i]->start();
    }
    // After all have started, send start message.
    Worker::Send(vectName, "do_start", NULL);
    // Wait for all threads to exit.
    OpenThread::ThreadJoinAll();
    // Release memory
    for (size_t i = 0; i < vectWorker.size(); i++)
    {
        delete vectWorker[i];
    }
    vectWorker.clear();
    return getchar();
}
```
