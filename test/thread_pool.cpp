#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <map>
#include <unordered_map>
#include "openthread.h"

using namespace open;

// Test3
void Test3Thread1(OpenThreadMsg& msg)
{
}
void Test3Thread2(OpenThreadMsg& msg)
{
}
void Test3()
{
    OpenThread::Init(256);
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
    OpenThreadPool pool;
    pool.init(64);

    auto thread = pool.create("Independent");
    if (thread)
    {
        thread->start(Test5Thread2);
        thread->stop();
    }
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
