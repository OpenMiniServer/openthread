# OpenThread
OpenThread是世界上最舒心的跨平台多线程并发库。
使用优雅的方式，创建线程、管理线程和线程间通信，从而实现多核并发。
OpenThread无任何依赖，全平台设计，只有两个源文件，让小白都可以轻松玩转C++多线程开发。

## 跨平台支持
Windows、linux等跨平台设计

## 编译和执行
请安装cmake工具，用cmake构建工程，可以在vs或者xcode上编译运行。
源代码：https://github.com/openlinyou/openthread
```
git clone https://github.com/openlinyou/openthread
cd ./openthread
mkdir build
cd build
cmake ..
make
./test
```

## 1.创建线程
```C++
#include <assert.h>
#include <stdio.h>
#include "openthread.h"
using namespace open;

//子线程接收到三种消息就会调用此函数，三种消息为线程启动、退出和接收消息，
void TestThread(OpenThreadMsg& msg)
{
    if (msg.state_ == OpenThread::START)
    {
        printf("Hello OpenThread\n");
        //睡眠1秒钟
        OpenThread::Sleep(1000);
        //退出线程
        msg.thread().stop();
    }
}
int main()
{
    // 创建线程，并对线程取名，并设置子线程运行函数TestThread
    auto thread = OpenThread::Create("Thread", TestThread);
    // 等待子线程退出
    OpenThread::ThreadJoin(thread);
    return getchar();
}
```

## 2.阻塞等待子线程返回数据
在当前线程创建OpenSync对象，把OpenSync对象像消息一样发给子线程并阻塞等待，子线程接到该消息后，再发消息唤醒阻塞。
使用OpenSync对象阻塞当前线程，子线程通过OpenSync返回数据。
```C++
#include <assert.h>
#include <stdio.h>
#include "openthread.h"
using namespace open;
struct Test1Data
{
    std::string data_;
    ~Test1Data()
    {
        printf("Test1:~Test1Data\n");
    }
};
void Test1Thread(OpenThreadMsg& msg)
{
	//线程第一次启动消息
    if (msg.state_ == OpenThread::START)
    {
        printf("Test1Thread[%s] START\n", msg.name().c_str());
        OpenThread::Sleep(1000);
    }
    //本子线程接收到消息
    else if (msg.state_ == OpenThread::RUN)
    {
        // 接收到OpenSync对象，对其唤醒并发消息。
        OpenSync* data = (OpenSync*)msg.data<OpenSync>();
        if (data)
        {
            auto sptr = std::shared_ptr<Test1Data>(new Test1Data);
            sptr->data_.assign("I come back!");
            data->wakeup(sptr);
        }
        OpenThread::Sleep(1000);
    }
    //子线程退出前的消息
    else if (msg.state_ == OpenThread::STOP)
    {
        printf("Test1Thread[%s] STOP\n", msg.name().c_str());
        OpenThread::Sleep(1000);
    }
}
int main()
{
    // 指定线程名，并创建。未填函数，线程未启动状态，需要执行start启动
    auto threadRef = OpenThread::Create("Test1Thread");
    //启动线程，并指定线程的执行函数
    threadRef.start(Test1Thread);
    // 创建OpenSync指针对象，C++11不支持std::make_shared，故设计MakeShared
    auto msg = OpenThread::MakeShared<OpenSync>();
    // auto msg = std::make_shared<OpenSyncRef>();
    //向子线程发消息
    threadRef.send(msg);
    //阻塞，等待被唤醒
    const Test1Data* data = msg->sleepBack<Test1Data>();
    if (data)
    {
        printf("Test1====>>:%s\n", data->data_.c_str());
    }
    //停止子线程
    threadRef.stop();
    //join等待子线程退出
    OpenThread::ThreadJoin(threadRef);
    return getchar();
}
```

## 3.线程之间进行通信
分别创建子线程dog和子线程cat，子线程dog和子线程cat之间互相通信。
这是一个dog溜cat的小故事。
```C++
#include <assert.h>
#include <stdio.h>
#include "openthread.h"
using namespace open;
//dog子线程
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
        //来自主线程的消息
        if (*data == "Hello dog! Catch cat!")
        {
        	//向cat子线程发消息
            auto data = OpenThread::MakeShared<std::string>();
            data->assign("Hello cat! Catch you!");
            auto cat = OpenThread::Thread("cat");
            if (cat && !cat.send(data))
            {
                printf("Test2ThreadDog[%s] send failed\n", msg.name().c_str());
            }
        }
        //来自子线程cat的消息
        else if (*data == "Bang dog!")
        {
        	//关闭子线程cat
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
//cat子线程
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
        //来自子线程dog的消息
        if (*data == "Hello cat! Catch you!")
        {
            auto data = OpenThread::MakeShared<std::string>();
            data->assign("Bang dog!");
            //向子线程dog发消息
            if (!OpenThread::Send("dog", data))
            {
                printf("Test2ThreadCat[%s] send failed\n", msg.name().c_str());
            }
        }
        break;
    }
    case OpenThread::STOP:
        printf("Test2ThreadCat[%s] STOP\n", msg.name().c_str());
        // dog线程关闭了cat，cat线程在关闭前，也关闭dog线程，进行回击。
        OpenThread::Stop("dog");
        break;
    default:
        break;
    }
}
int main()
{
    // 创建子线程dog和cat
    auto dog = OpenThread::Create("dog", Test2ThreadDog);
    auto cat = OpenThread::Create("cat", Test2ThreadCat);
    // 向子线程dog发消息
    auto data = OpenThread::MakeShared<std::string>();
    data->assign("Hello dog! Catch cat!");
    if (!dog.send(data))
    {
        printf("Test2Thread send failed\n");
    }
    // 等待子线程退出
    OpenThread::ThreadJoin({ "dog", "cat" });
    return getchar();
}
```

## 4.批量创建和管理线程
OpenThread启动的时候，会默认设定创建线程的最大数量。超过以后，就不能修改。
所以，在程序启动的时候，用OpenThread::Init(1024)可以指定线程最大数量。线程的目标主要是发挥多核性能。
创建太多线程会带来性能损耗，最好线程数是CPU核数的2倍。尽量避免频繁创建和销毁线程。

```C++
void Test3Thread1(OpenThreadMsg& msg)
{
}
void Test3Thread2(OpenThreadMsg& msg)
{
}
int main()
{
	//指定线程最大数量限制，只有程序启动的时候才可修改
	OpenThread::Init(1024);
    size_t capacity = OpenThread::GetThreadCapacity();
    assert(capacity == 1024)
    for (size_t pid = 0; pid < capacity; pid++)
    {
    	//OpenThread::Thread查询线程对象OpenThread
        auto threadRef = OpenThread::Thread("Thread_"+std::to_string(pid));
        //由于没有创建任何线程，故是null
        assert(!threadRef);
    }
    //全部线程名称数量，线程名称指定后就一直存在。
    assert(OpenThread::GetThreadSize() == 0);
    //创建智能指针对象，发给子线程。字符串"sendMsg"
    auto data = OpenThread::MakeShared<std::string>();
    data->assign("sendMsg");
    std::string name;
    //创建1024条线程
    for (int pid = 0; pid < capacity; pid++)
    {
        name = "Thread_" + std::to_string(pid);
        //OpenThread::Create创建指定名称的线程，如果名称绑定的线程存在，就返回该线程。
        //成功以后便有线程名。 top -Hp可以查看。window系统没有线程名
        auto threadRef = OpenThread::Create(name, Test3Thread1);
        assert(threadRef && threadRef.pid() == pid && threadRef.name() == name);
        //三种方式向子线程发消息，线程对象、线程id（不是系统线程id，是数组索引id）、线程名称
        threadRef.send(data);
        OpenThread::Send(pid, data);
        OpenThread::Send(name, data);
        printf("Test3 create %s\n", name.c_str());
    }
    assert(OpenThread::GetThreadSize() == capacity);
    for (size_t pid = 0; pid < capacity; pid++)
    {
        name = "Thread_" + std::to_string(pid);
        //通过线程名查询线程，通过线程名查询线程效率比较差，推荐使用线程id查询。
        auto threadRef = OpenThread::Thread(name);
        assert(threadRef && threadRef.name() == name);
        //关闭子线程
        threadRef.stop();
    }
    printf("Test3 do stop\n");
    //等待全部子线程关闭退出
    OpenThread::ThreadJoinAll();
    printf("Test3 finish waitStop\n");
    // 再次创建子线程，子线程名称会一直存在，占用容量。
    //除非调用OpenThread::StopAll()，关闭清理全部子线程，推倒重来。
    for (size_t pid = 0; pid < capacity; pid++)
    {
        name = "Thread_" + std::to_string(pid);
        auto threadRef = OpenThread::Create(name, Test3Thread2);
        assert(threadRef && threadRef.pid() == pid && threadRef.name() == name);
    }
    printf("Test3 finish create again\n");
    //子线程名字数量超过最大容量，故用"over_boundary"创建失败
    auto threadRef = OpenThread::Create("over_boundary");
    assert(!threadRef);
    //关闭退出全部线程，并进行清理
    OpenThread::StopAll();
    return getchar();
}
```

## 5.设计一个多线程并发框架
用Worker类封装使用OpenThread，一条线程一个Worker业务。Inspector(监控)、Timer(定时器)和Server(服务器)继承Worker。
Inspector负责监控多个Timer运行信息，做负载均衡。
Timer提供定时器服务，启动时，向Inspector注册，并提供运行信息。
Server向Inspector查询可用的Timer，然后向此Timer请求定时服务。
```C++
#include <assert.h>
#include <stdio.h>
#include "openthread.h"
using namespace open;
//假设是谷歌protobuff对象
struct ProtoBuffer
{
	//父类虚函数
    virtual ~ProtoBuffer() {}
};
//Worker类通信的中间数据结构
class Data
{
    ProtoBuffer* proto_; //携带的数据
public:
    Data() :proto_(0), srcPid_(-1) {}
    Data(int pid, const std::string& name, const std::string& key, ProtoBuffer* proto)
        :srcPid_(pid), srcName_(name), rpc_(key), proto_(proto) {}
    ~Data()
    {
        if (proto_) delete proto_;
        proto_ = 0;
    }
    int srcPid_; //发送方的worker的ID
    std::string rpc_; //路由函数名称
    std::string srcName_; //发送方的worker名称
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
    	//查询name对应的线程，没有创建，故是null
        auto threadRef = OpenThread::Thread(name_);
        assert(!threadRef);
        //创建名字为name的线程
        threadRef = OpenThread::Create(name_);
        assert(threadRef);
        if (threadRef)
        {
        	//获取线程真实对象，以便使用其高级功能。
        	//注意只能在该线程使用高级功能，否则会导致线程读取数据冲突
            thread_ = OpenThread::GetThread(threadRef);
            assert(!thread_->isRunning());
            //只能指定一次，只能在子线程访问
            thread_->setCustom(this);
            //指定函数，并启动线程，
            thread_->start(Worker::Thread);
        }
    }
    //线程全部启动后，主线程发消息do_start调用
    void do_start(const Data& data)
    {
        onStart();
    }
    //线程启动时，调用
    virtual void onInit()
    {
    }
    //线程全部启动后调用，
    virtual void onStart()
    {
    }
    //本子线程收到消息后，调用此方法。
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
    // 向线程sid发送消息，该方法会调用delete释放proto,只能传一次，由std::shared_ptr管理
    bool send(int sid, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>[%s] key:%s\n", name_.c_str(), WorkerName(sid).c_str(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(sid, data);
        //assert(ret);
        return ret;
    }
    // 向线程名为name发送消息，该方法会调用delete释放proto,只能传一次，由std::shared_ptr管理
    bool send(const std::string& name, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>[%s] key:%s\n", name_.c_str(), name.c_str(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(name, data);
        //assert(ret);
        return ret;
    }
    // 向多个线程sid发送消息，该方法会调用delete释放proto,只能传一次，由std::shared_ptr管理
    bool send(std::vector<int>& vectSid, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>size[%d] key:%s\n", name_.c_str(), (int)vectSid.size(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(vectSid, data);
        //assert(ret);
        return ret;
    }
    // 向多个线程名为name发送消息，该方法会调用delete释放proto,只能传一次，由std::shared_ptr管理
    bool send(std::vector<std::string>& vectName, const std::string& key, ProtoBuffer* proto)
    {
        printf("[%s]send=>size[%d] key:%s\n", name_.c_str(), (int)vectName.size(), key.c_str());
        auto data = std::shared_ptr<Data>(new Data(pid(), name_, key, proto));
        bool ret = OpenThread::Send(vectName, data);
        //assert(ret);
        return ret;
    }
    //停止本线程
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
    //线程调出时，调用
    virtual void onStop()
    {
    }
    //本线程绑定的函数
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
    //提供给主线程发消息
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
	//给当前线程发消息
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
// 模仿protobuff数据，请求定时器数据结构
struct TimerEventMsg :public ProtoBuffer
{
    int workerId_;
    int64_t deadline_;
};
// 模仿protobuff数据，定时器信息数据结构
struct TimerInfoMsg :public ProtoBuffer
{
    TimerInfoMsg() 
        :workerId_(0), leftCount_(0), cpuCost_(0), dataTime_(0) {}
    int workerId_;
    size_t leftCount_;
    int64_t cpuCost_;
    int64_t dataTime_;
};
//监控类，获取定时器信息，为Server做负载均衡
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
    //向定时器获取信息
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
	//接收定时器的信息
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
    //向Server提供最空闲的定时器ID
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
//定时器对象
class Timer:public Worker
{
    int inspectorId_;
    //定时事件，从小到大，可以多个key。
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
    //定时逻辑，如果触发定时就发送定时事件
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
                    	//触发定时，发送定时事件
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
            	//向监控注册，并发送自身运行信息
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
    // 提供定时器运行信息
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
    //接收Server的定时器订阅事件，定时器触发后，就把事件发送回去。
    void request_timer(const Data& data)
    {
        auto& proto = data.proto<TimerEventMsg>();
        mapTimerEvent.insert({ proto.deadline_, data.srcPid_ });
        sendLoop("start_timer");
    }
};
//业务，向监控查询可用的定时器，然后向此定时器请求定时事件
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
    //向监控查询可用定时器
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
    //监控返回的定时器信息，然后向该定时器请求定时事件
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
    //定时器触发以后，发送回来的定时事件
    void return_timer(const Data& data)
    {
    	//测试次数到达后，停止测试，关闭全部线程
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
	//创建一个Inspector，2个Timer和2个Server
    std::vector<Worker*> vectWorker =
    {
        new Inspector("Inspector"),
        new Timer("timer1"),
        new Timer("timer2"),
        new Server("server1"),
        new Server("server2")
    };
    //启动Worker
    std::vector<std::string> vectName;
    for (size_t i = 0; i < vectWorker.size(); i++)
    {
        vectName.push_back(vectWorker[i]->name_);
        vectWorker[i]->start();
    }
    //全部启动以后，发送start消息
    Worker::Send(vectName, "do_start", NULL);
    //等待全部线程退出
    OpenThread::ThreadJoinAll();
    //释放内存
    for (size_t i = 0; i < vectWorker.size(); i++)
    {
        delete vectWorker[i];
    }
    vectWorker.clear();
    return getchar();
}
```
