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
    auto msg = std::shared_ptr<OpenSync>(new OpenSync);
    auto data = std::shared_ptr<std::string>(new std::string);
    data->assign("Waiting for you!");
    msg->put(data);
    threadRef.send(msg);
    const Test1Data* ret = msg->awaitReturn<Test1Data>();
    if (ret)
    {
        assert(ret->data_ == "Of Course,I Still Love You!");
        printf("Test1====>>:%s\n", ret->data_.c_str());
    }
    // stop thread
    threadRef.stop();

    // wait stop
    OpenThread::ThreadJoin(threadRef);
    printf("Pause\n");
    return getchar();
}
