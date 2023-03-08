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
