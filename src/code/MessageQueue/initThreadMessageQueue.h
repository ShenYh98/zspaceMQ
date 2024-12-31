#pragma once

#include "ThreadPool.h"
#include "MessageQueue.hpp"

using namespace ThreadMessageQueue;

namespace ThreadMessageQueue {
    class InitMessageQueue {
    private:
        InitMessageQueue()
            {
                threadPool = new ThreadPool(1, 2); // 会有一个监控线程,会一直挂在后端监控订阅信息
            }
        InitMessageQueue(const InitMessageQueue&) = delete;
        InitMessageQueue& operator=(const InitMessageQueue&) = delete;

    public:
        static InitMessageQueue& getInstance() {
            static InitMessageQueue instance;
            return instance;
        }

        void initSubscribeInfo();

        void destroySubscribe (void* obj);

    private:
        

    private:
        ThreadPool *threadPool;
        mutable std::mutex mutex;
    };
};
