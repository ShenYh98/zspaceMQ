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

        // TODO 线程初始化消息队列暂时用不到,后期补充
        void initSubscribeInfo();

        void destroySubscribe (void* obj);

    private:
        void execForPeriodTime(const size_t& time, std::function<void(time_t*)> callback);

    private:
        ThreadPool *threadPool;
        mutable std::mutex mutex;
    };
};
