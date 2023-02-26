
#pragma once

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <string>
#include <vector>

namespace tb
{

struct Worker
{
    pthread_mutex_t mutex_;
    pthread_cond_t cond_;
    pthread_t threadId_;
    bool isStarted_;
    bool isIdle_;
    bool isLoop_;
    void* arg_;
    void* (*cb_) (void*);
    Worker()
    {
        cb_  = 0;
        arg_ = 0;
        isLoop_ = false;
        isIdle_ = false;
        isStarted_ = false;
        memset(&threadId_, 0, sizeof(threadId_));

        pthread_mutex_init(&mutex_, NULL);
        pthread_cond_init(&cond_, NULL);
    }
    virtual ~Worker()
    {
        pthread_mutex_destroy(&mutex_);
        pthread_cond_destroy(&cond_);
    }
    void start()
    {
        assert(!isStarted_);
        if(isStarted_) return;
        isStarted_ = true;
        memset(&threadId_, 0, sizeof(threadId_));
        pthread_create(&threadId_, NULL, (void *(*)(void*))Worker::Run, this);
    }
    void stop()
    {
        if (!isStarted_) return;
        isStarted_ = false;
    }
    static void Run(void* arg)
    {
        assert(arg);
        if(!arg) return;
        Worker* p = (Worker*)arg;
        p->run();
    }
    void run()
    {
        //cb_ = 0;
        //arg_ = 0;
        isIdle_ = false;
        isStarted_ = true;
        while(isStarted_)
        {
            if (cb_)
            {
                cb_(arg_);
                if (!isLoop_)
                {
                    cb_ = 0;
                    arg_ = 0;
                }
            }            
            pthread_mutex_lock(&mutex_);
            isIdle_ = true;
            pthread_cond_wait(&cond_, &mutex_);
            isIdle_ = false;
            pthread_mutex_unlock(&mutex_);
        }
    }
    void signal(void* (*cb) (void*), void* arg)
    {
        while (!isIdle_)
        {
            assert(false);
        }
        cb_  = cb;
        arg_ = arg;
        pthread_cond_signal(&cond_);
    }
    bool isIdle()
    {
        //if (isIdle_)
        //{
        //    assert(!cb_);
        //    assert(!arg_);
        //}
        return isIdle_;
    }
};

};
