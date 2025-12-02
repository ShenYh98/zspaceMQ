#pragma once

#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <condition_variable>

#include <future>
#include <functional>

using namespace std;
using namespace std::literals::chrono_literals;
using callback = void(*)(void*);

namespace CommonLib {

// 线程池类
class ThreadPool
{
public:
    ThreadPool(int min, int max);
    // 添加任务
    template<typename Func, typename... Args>
    void Add(Func&& func, Args&&... args) {
        std::unique_lock<std::mutex> lk(mutexPool);
        if (shutdown)
        {
            return;
        }

        // 将任务包装成 lambda 函数，并添加到任务队列
        taskQ.push(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        
        cond.notify_all();
    }

    // 忙线程个数
    int Busynum();
    // 存活线程个数
    int Alivenum();
    ~ThreadPool();

private:
    // 任务队列
    queue<std::function<void()>> taskQ;
    thread managerID;   //管理者线程ID
    vector<thread> threadIDs;   //
    int minNum;   //最小线程数
    int maxNum;   //最大线程数
    int busyNum;   //忙的线程数
    int liveNum;    //存活的线程数
    int exitNum;    //要销毁的线程数

    mutex mutexPool;    //整个线程池的锁
    condition_variable cond;     //任务队列是否为空,阻塞工作者线程
    bool shutdown;    //是否销毁线程池，销毁为1，不销毁为0

private:
    static void manager(void* arg);   //管理者线程
    static void worker(void* arg);   //工作线程
};

}

