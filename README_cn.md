# OpenThread
OpenThread是最舒心的跨平台多线程并发库，多线程三大设计模式: Await模式, Worker模式和Actor模式。

使用优雅的方式，创建线程、管理线程和线程间通信，从而实现多核并发。

OpenThread无任何依赖，全平台设计，只有两个源文件，让小白都可以轻松玩转C++多线程开发。

**OpenLinyou项目设计跨平台服务器框架，在VS或者XCode上写代码，无需任何改动就可以编译运行在Linux上，甚至是安卓和iOS.**
OpenLinyou：https://github.com/openlinyou
https://gitee.com/linyouhappy

## 跨平台支持
Windows、linux、Mac、iOS、Android等跨平台设计

## 编译和执行
请安装cmake工具，用cmake构建工程，可以在vs或者xcode上编译运行。
源代码：https://github.com/openlinyou/openthread
https://gitee.com/linyouhappy/openthread
```
#克隆项目
git clone https://github.com/openlinyou/openthread
cd ./openthread
#创建build工程目录
mkdir build
cd build
cmake ..
#如果是win32，在该目录出现openthread.sln，点击它就可以启动vs写代码调试
make
./helloworld
```

## 全部源文件
+ src/openthread.h
+ src/openthread.cpp

## 技术特点
OpenThread的技术特点：
1. 跨平台设计，提供Linux统一的pthread接口，支持安卓和iOS。
2. 线程池管理采用智能指针和无锁map，实现高效访问线程对象。
3. 每个线程自带消息队列，消息放入队列原子锁，而读取消息队列，无锁操作。保证线程交换信息高效。
4. 线程交互数据，采用智能指针管理，实现内存自动化管理，无需担忧内存泄漏。
5. 多线程三大设计模式: Await模式, Worker模式和Actor模式。

## 多线程开发三大设计模式
1. Await模式。两条线程，一条线程向另一条线程请求，同时阻塞等待；另一条线程接收到请求，返回数据唤醒第一条线程；第一条线程唤醒，拿到数据继续执行。
2. Worker模式。适合客户端，创建一定量的worker线程，组成factory，向外提供唯一接口服务。
3. Actor模式。适合服务端，一条线程一条Actor，不同的Actor负责不同的功能。

## 1.创建线程HelloWorld
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
    printf("Pause\n");
    return getchar();
}
```

## 2.Await模式
在主线程创建OpenSyncReturn对象，把它发给子线程，并阻塞等待子线程返回。
子线程接到该消息后，再发消息唤醒，再发OpenSync对象给主线程，等待主线程响应。
主线程线程被唤醒后，收到子线程消息携带的OpenSync对象，唤醒子线程。
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

// 子线程调用
void Test1Thread(OpenThreadMsg& msg)
{
    //线程启动的消息
    if (msg.state_ == OpenThread::START)
    {
        printf("Test1Thread[%s] START\n", msg.name().c_str());
        OpenThread::Sleep(1000);
    }
    //线程接收到的消息
    else if (msg.state_ == OpenThread::RUN)
    {
        // //接收主线程的OpenSyncReturn对象，对其唤醒并发消息。
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

            //等待主线程唤醒
            sptr->openSync_.await();
        }
        OpenThread::Sleep(1000);
    }
    //线程退出前的消息
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
    threadRef.start(Test1Thread);

    // 给子线程发送消息
    auto msg = std::shared_ptr<OpenSyncReturn<TestData, Test1Data>>(new OpenSyncReturn<TestData, Test1Data>);
    {
        auto data = std::shared_ptr<TestData>(new TestData);
        data->data_ = "Waiting for you!";
        msg->put(data);
    }
    threadRef.send(msg);
    //阻塞主线程，等待子线程唤醒
    auto ret = msg->awaitReturn();
    if (ret)
    {
        assert(ret->data_ == "Of Course,I Still Love You!");
        printf("Test1====>>:%s\n", ret->data_.c_str());

        //唤醒子线程的阻塞
        ret->openSync_.wakeup();
    }
    // 向子线程发送关闭消息
    threadRef.stop();

    // 等待全部线程退出
    OpenThread::ThreadJoin(threadRef);
    printf("Pause\n");
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
所以，在程序启动的时候，用OpenThread::Init(256)可以指定线程最大数量。线程的目标主要是发挥多核性能。
创建太多线程会带来性能损耗，最好线程数是CPU核数的2倍。尽量避免频繁创建和销毁线程。
为了防止线程之间混淆，设计了线程池OpenThreadPool。可以对不同的业务配置专门的线程池。
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
	//指定线程最大数量限制，只有程序启动的时候才可修改
	OpenThread::Init(256);
    size_t capacity = OpenThread::GetThreadCapacity();
    assert(capacity == 256)
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
void Test5()
{
    //新建线程池
    OpenThreadPool pool;
    pool.init(64);

    auto thread = pool.create("Independent");
    if (thread)
    {
        thread->start(Test5Thread2);
        thread->stop();
    }
    //停止该线程池的全部线程
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

## 5.Actor设计模式
Actor模式。适合服务端，一条线程一条Actor，不同的Actor负责不同的功能。
用Worker类封装使用OpenThread，一条线程一个Worker业务。Inspector(监控)、Timer(定时器)和Server(服务器)继承Worker。
Inspector负责监控多个Timer运行信息，做负载均衡。
Timer提供定时器服务，启动时，向Inspector注册，并提供运行信息。
Server向Inspector查询可用的Timer，然后向此Timer请求定时服务。
```C++
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <map>
#include <unordered_map>
#include "openthread.h"
using namespace open;

class ProtoBuffer : public OpenThreadProto
{
    std::shared_ptr<void> data_;
public:
    int dataType_;
    ProtoBuffer() 
        : OpenThreadProto()
        ,dataType_(0)
        ,data_(0){}
    virtual ~ProtoBuffer() {}
    template <class T>
    inline T& data() 
    { 
        T* t = 0;
        if (data_)
        {
            t = dynamic_cast<T*>((T*)data_.get());
            if (data_.get() == t) return *t;
            data_.reset();
        }
        t = new T;
        data_ = std::shared_ptr<T>(t);
        return *t;
    }
    template <class T>
    inline T& data() const
    {
        if (data_)
        {
            T* t = dynamic_cast<T*>((T*)data_.get());
            if (data_.get() == t) return *t;
        }
        assert(false);
        static T t;
        return t;
    }
    static inline int ProtoType() { return (int)(uintptr_t) & (ProtoType); }
    virtual inline int protoType() const { return ProtoBuffer::ProtoType(); }
};

struct ProtoLoop : public OpenThreadProto
{
    int type_;
    ProtoLoop() :type_(-1) {}
    static inline int ProtoType() { return (int)(uintptr_t) & (ProtoType); }
    virtual inline int protoType() const { return ProtoLoop::ProtoType(); }
};

struct TimerEventMsg
{
    int workerId_;
    int64_t deadline_;
    TimerEventMsg() : workerId_(0), deadline_(0) {}
};

struct TimerInfoMsg
{
    int workerId_;
    size_t leftCount_;
    int64_t cpuCost_;
    int64_t dataTime_;
    TimerInfoMsg() : workerId_(0), leftCount_(0), cpuCost_(0), dataTime_(0) {}
};

enum EMsgId
{
    query_timer_info,
    get_timer_info,
    request_timer,
};

class Inspector : public OpenThreadWorker
{
    std::unordered_map<std::string, TimerInfoMsg> mapTimerInfo_;
    std::vector<int> vectQueryId;
public:
    Inspector(const std::string& name):OpenThreadWorker(name)
    {
        registers(ProtoLoop::ProtoType(), (OpenThreadHandle)&Inspector::onProtoLoop);
        registers(ProtoBuffer::ProtoType(), (OpenThreadHandle)&Inspector::onProtoBuffer);
    }
    virtual void onStart() {}
private:
    void onProtoLoop(const ProtoLoop& proto)
    {
        printf("Inspector::onProtoLoop[%s]Recevie<<==[%s]\n", name_.c_str(), proto.srcName_.c_str());
        std::vector<int> vectPid;
        vectPid.reserve(mapTimerInfo_.size());
        for (auto iter = mapTimerInfo_.begin(); iter != mapTimerInfo_.end(); iter++)
        {
            if (iter->second.workerId_ >= 0)
                vectPid.push_back(iter->second.workerId_);
        }
        auto root = std::shared_ptr<ProtoBuffer>(new ProtoBuffer);
        root->dataType_ = get_timer_info;
        send(vectPid, root);
    }
    void onProtoBuffer(const ProtoBuffer& proto)
    {
        printf("Inspector::onProtoBuffer[%s]Recevie<<==[%s]\n", name_.c_str(), proto.srcName_.c_str());
        if (proto.dataType_ == get_timer_info)
        {
            auto& msg = proto.data<TimerInfoMsg>();
            auto& timerInfo = mapTimerInfo_[proto.srcName_];
            timerInfo = msg;
            if (!vectQueryId.empty())
            {
                auto root = std::shared_ptr<ProtoBuffer>(new ProtoBuffer);
                root->dataType_ = query_timer_info;
                auto& info = root->data<TimerInfoMsg>();
                info = timerInfo;
                send(vectQueryId, root);

                vectQueryId.clear();
            }
        }
        else if (proto.dataType_ == query_timer_info)
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
                vectQueryId.push_back(proto.srcPid_);
                auto root = std::shared_ptr<ProtoLoop>(new ProtoLoop);
                sendLoop(root);
            }
            else
            {
                auto root = std::shared_ptr<ProtoBuffer>(new ProtoBuffer);
                root->dataType_ = query_timer_info;
                auto& info = root->data<TimerInfoMsg>();
                info = *tmpInfo;
                send(proto.srcPid_, root);
            }
        }
    }
};


class Timer:public OpenThreadWorker
{
    int inspectorId_;
    std::multimap<int64_t, int> mapTimerEvent_;
public:
    Timer(const std::string& name):OpenThreadWorker(name)
    {
        inspectorId_ = -1;
        registers(ProtoLoop::ProtoType(), (OpenThreadHandle)&Timer::onProtoLoop);
        registers(ProtoBuffer::ProtoType(), (OpenThreadHandle)&Timer::onProtoBuffer);
    }
protected:
    virtual void onStart()
    {
        while (inspectorId_ < 0)
        {
            inspectorId_ = ThreadId("Inspector");
            if (inspectorId_ >= 0)
            {
                auto root = std::shared_ptr<ProtoBuffer>(new ProtoBuffer);
                root->dataType_ = get_timer_info;
                auto& msg = root->data<TimerInfoMsg>();
                msg.workerId_ = pid();
                msg.dataTime_ = OpenThread::MilliUnixtime();
                msg.cpuCost_ = thread_->cpuCost();
                msg.leftCount_ = thread_->leftCount();

                send(inspectorId_, root);
                break;
            }
            OpenThread::Sleep(100);
        }
        auto root = std::shared_ptr<ProtoLoop>(new ProtoLoop);
        sendLoop(root);
    }
private:
    void onProtoLoop(const ProtoLoop& proto)
    {
        printf("Timer::onProtoLoop[%s]Recevie<<==[%s]\n", name_.c_str(), proto.srcName_.c_str());
        assert(proto.srcPid_ == pid_);
        int64_t curTime = 0;
        while (canLoop())
        {
            if (!mapTimerEvent_.empty())
            {
                curTime = OpenThread::MilliUnixtime();
                while (!mapTimerEvent_.empty())
                {
                    auto iter = mapTimerEvent_.begin();
                    if (curTime > iter->first)
                    {
                        auto root = std::shared_ptr<ProtoBuffer>(new ProtoBuffer);
                        root->dataType_ = request_timer;
                        auto& msg = root->data<TimerEventMsg>();
                        msg.workerId_ = pid();
                        msg.deadline_ = curTime;

                        send(iter->second, root);

                        mapTimerEvent_.erase(iter);
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
    void onProtoBuffer(const ProtoBuffer& proto)
    {
        printf("Timer::onProtoBuffer[%s]Recevie<<==[%s]\n", name_.c_str(), proto.srcName_.c_str());
        if (proto.dataType_ == get_timer_info)
        {
            auto root = std::shared_ptr<ProtoBuffer>(new ProtoBuffer);
            root->dataType_ = get_timer_info;
            auto& msg = root->data<TimerInfoMsg>();
            msg.workerId_ = pid();
            msg.dataTime_  = OpenThread::MilliUnixtime();
            msg.cpuCost_   = thread_->cpuCost();
            msg.leftCount_ = thread_->leftCount();
            send(proto.srcPid_, root);

            auto sptr = std::shared_ptr<ProtoLoop>(new ProtoLoop);
            sendLoop(sptr);
        }
        else if (proto.dataType_ == request_timer)
        {
            auto& msg = proto.data<TimerEventMsg>();
            mapTimerEvent_.insert({ msg.deadline_, proto.srcPid_ });

            auto sptr = std::shared_ptr<ProtoLoop>(new ProtoLoop);
            sendLoop(sptr);
        }
    }
};

class Server:public OpenThreadWorker
{
    int inspectorId_;
    int collect_;
public:
    Server(const std::string& name)
        :OpenThreadWorker(name)
        ,inspectorId_(-1)
    {
        collect_ = 0;
        registers(ProtoLoop::ProtoType(), (OpenThreadHandle)&Server::onProtoLoop);
        registers(ProtoBuffer::ProtoType(), (OpenThreadHandle)&Server::onProtoBuffer);
    }
protected:
    virtual void onStart()
    {
        while (inspectorId_ < 0)
        {
            inspectorId_ = ThreadId("Inspector");
            OpenThread::Sleep(10);
        }
        auto sptr = std::shared_ptr<ProtoLoop>(new ProtoLoop);
        sendLoop(sptr);
    }
private:
    void onProtoLoop(const ProtoLoop& proto)
    {
        printf("Server::onProtoLoop[%s]Recevie<<==[%s]\n", name_.c_str(), proto.srcName_.c_str());
        auto root = std::shared_ptr<ProtoBuffer>(new ProtoBuffer);
        root->dataType_ = query_timer_info;
        send(inspectorId_, root);
    }

    void onProtoBuffer(const ProtoBuffer& proto)
    {
        printf("Server::onProtoBuffer[%s]Recevie<<==[%s]\n", name_.c_str(), proto.srcName_.c_str());
        if (proto.dataType_ == query_timer_info)
        {
            auto& msg = proto.data<TimerInfoMsg>();
            if (msg.workerId_ > 0)
            {
                auto root = std::shared_ptr<ProtoBuffer>(new ProtoBuffer);
                root->dataType_ = request_timer;
                auto& event = root->data<TimerEventMsg>();
                int64_t curTime = OpenThread::MilliUnixtime();
                event.deadline_ = curTime + curTime % 2000;
                if (event.deadline_ > curTime + 2000)
                {
                    event.deadline_ = curTime;
                }
                send(msg.workerId_, root);
            }
            else
            {
                auto sptr = std::shared_ptr<ProtoLoop>(new ProtoLoop);
                sendLoop(sptr);
            }
        }
        else if (proto.dataType_ == request_timer)
        {
            if (collect_++ > 100)
            {
                OpenThread::StopAll();
                return;
            }
            sendLoop(std::shared_ptr<ProtoLoop>(new ProtoLoop));
        }
    }
};

int main()
{
    OpenThread::StopAll();
    std::vector<OpenThreadWorker*> vectWorker =
    {
        new Inspector("Inspector"),
        new Timer("timer1"),
        new Timer("timer2"),
        new Server("server1"),
        new Server("server2"),
        new Server("server3"),
        new Server("server4")
    };
    for (size_t i = 0; i < vectWorker.size(); i++)
    {
        vectWorker[i]->start();
    }

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

## 6.Worker设计模式
适合客户端，创建一定量的worker线程，组成factory，向外提供唯一接口服务。
```C++
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <vector>
#include "openthread.h"
using namespace open;
//业务数据结构
struct Product
{
    int id_;
    std::string goods_;
    Product():id_(0) {}
};

//OpenThread交换协议
struct ProtoTask : public OpenThreadProto
{
    std::shared_ptr<Product> data_;
    OpenSync openSync_;

    static inline int ProtoType() { return 1; }
    virtual inline int protoType() const { return ProtoTask::ProtoType(); }
};

class Worker : public OpenThreadWorker
{   
    //Worker工程线程Factory，提供4个worker线程。
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
        :OpenThreadWorker(name)
    {
        mapHandle_[ProtoTask::ProtoType()] = (OpenThreadHandle)&Worker::makeProduct;
        uid_ = 1;
        start();
    }
    ~Worker()
    {
        for (size_t i = 0; i < vectTask_.size(); ++i)
        {
            vectTask_[i].openSync_.wakeup();
        }
    }
    //生产产品
    void makeProduct(const ProtoTask& proto)
    {
        vectTask_.push_back(proto);
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
    std::vector<ProtoTask> vectTask_;
public:
    //对外服务统一接口
    static bool MakeProduct(std::shared_ptr<Product>& product)
    {
        auto worker = Instance_.getWorker();
        if (!worker)  return false;
        auto proto = std::shared_ptr<ProtoTask>(new ProtoTask);
        proto->data_ = product;
        bool ret = worker->send(-1, proto);
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
    //创建4条测试线程
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