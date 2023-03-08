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
        {
            vectTask_[i].openSync_.wakeup();
        }
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
