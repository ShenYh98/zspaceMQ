#pragma once

#include "ThreadPool.h"
#include "MessageQueue.hpp"

#define  MAX_TIME                   5

#define  INIT_SUB_PATH              "./tmp/init_sub_fifo"
#define  RECV_WAIT_PATH             "./tmp/recv_wait_fifo"
#define  FORK_LOCK_FILE             "example.lock"

#define  WAIT_SUB_BUF_SIZE           2048
#define  CHECK_SUB_BUF_SIZE          2048
#define  RECV_SEND_TOPIC_BUF_SIZE    2048

#define  WAIT_TOPIC_TIME             10
#define  WAIT_PIPE_TIME              10

using namespace ProcessMessageQueue;

namespace ProcessMessageQueue {
    typedef enum {
        INIT,
        WAIT,
        RECV
    } SubInfo;

    class InitMessageQueue {
    private:
        InitMessageQueue() : 
            init_sub_path(INIT_SUB_PATH) ,
            recv_wait_path(RECV_WAIT_PATH)
            {
                threadPool = new ThreadPool(2, 4); // 只有两个线程在跑,跑完释放掉
            }
        InitMessageQueue(const InitMessageQueue&) = delete;
        InitMessageQueue& operator=(const InitMessageQueue&) = delete;

    public:
        static InitMessageQueue& getInstance() {
            static InitMessageQueue instance;
            return instance;
        }

        void initSubscribeInfo();

        void waitInit(const std::string node);

    private:
        // 创建进程的文件锁
        int lock_file(int fd);
        
        // 创建进程的文件解锁
        int unlock_file(int fd);

        void pipeRead(const std::string& fifo_path, const SubInfo& e_SubInfo);

        int pipeWrite(const std::string& fifo_path, const std::string& message);

        void checkSubscribeInfo(int const& fifo_fd);

        void waitSubscribeInfo(int const& fifo_fd);

        void recvSendTopicPath(int const& fifo_fd);

        std::vector<std::string> parseString(const char* str);

        void execForPeriodTime(const size_t& time, std::function<void(time_t*)> callback);

    private:
        ThreadPool *threadPool;
        std::vector<std::string> v_topic;           // 消息队列从订阅那儿接收的话题组
        std::vector<std::string> v_recv_topic;      // 消息队列接收的话题组给发布使用
        std::vector<std::string> v_wait_fifo_path;  // 进程等待管道组
        std::mutex mutex;
        std::string init_sub_path;   // 通过这个管道路径,在一定时间内接收创建订阅的话题
        std::string recv_wait_path;  // 接收各个进程等待函数的管道路径

        int fork_lk;
    };
};