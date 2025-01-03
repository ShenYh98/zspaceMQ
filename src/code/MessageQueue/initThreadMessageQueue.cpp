#include "initThreadMessageQueue.h"

using namespace ThreadMessageQueue;

void InitMessageQueue::initSubscribeInfo() {
    // 在一定时间内,等待接收所有消息队列话题
    /*
    execForPeriodTime(10, [&](time_t*){
    });
    */
}

void InitMessageQueue::destroySubscribe (void* obj) {
    std::lock_guard<std::mutex> lock(mutex);

    MessageHandle::getInstance().unregisterObject(obj);
}

void InitMessageQueue::execForPeriodTime(const size_t& time, std::function<void(time_t*)> callback) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        std::cerr << "Error getting clock time" << std::endl;
        return;
    }
 
    time_t startTime = ts.tv_sec;
    time_t cur_time;
 
    while (true) {
        if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
            std::cerr << "Error getting clock time" << std::endl;
            return;
        }
        cur_time = ts.tv_sec;
        size_t elapsed_seconds = cur_time - startTime;
 
        if (elapsed_seconds >= time) {
            break;
        }

        // 执行回调函数
        callback(&startTime);
    }
}
