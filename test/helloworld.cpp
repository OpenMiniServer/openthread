#include <assert.h>
#include <iostream>
#include <stdio.h>
#include "openthread.h"

using namespace open;

// Test0
void Test0Thread(OpenThreadMsg& msg)
{
    if (msg.state_ == OpenThread::START)
    {
        printf("Hello OpenThread\n");
        OpenThread::Sleep(1000);
        msg.thread().stop();
    }
}
int main()
{
    // create and start thread
    auto thread = OpenThread::Create("Test0Thread", Test0Thread);
    // wait stop
    OpenThread::ThreadJoin(thread);
    printf("Pause\n");
    return getchar();
}
