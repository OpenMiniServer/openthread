#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <map>
#include <unordered_map>
#include "openthread.h"

using namespace open;

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

int main()
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
    OpenThread::ThreadJoin({ "dog", "cat" });
    printf("Pause\n");
    return getchar();
}



