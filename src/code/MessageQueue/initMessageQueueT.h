#pragma once

#include "ThreadPool.h"
#include "MessageQueue.hpp"

using namespace ThreadMessageQueue;

namespace ThreadMessageQueue {
    class InitMessageQueue {
    private:
        InitMessageQueue() {}
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

        void initThreadPool(const int& min, const int& max);

    private:
        void execForPeriodTime(const size_t& time, std::function<void(time_t*)> callback);

    private:
        mutable std::mutex mutex;
    };
};
