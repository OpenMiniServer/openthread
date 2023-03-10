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
