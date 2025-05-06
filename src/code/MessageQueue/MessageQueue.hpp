#pragma once

#include <mutex>
#include <ctime>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <unordered_set>
#include <condition_variable>

#include "ThreadPool.h"
#include "DequeCache.hpp"
#include "MessageHandle.h"

/* posix通信相关 */
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>

using namespace CommonLib;

/* 第一版线程间通信的消息队列(单线程通信,无法进程间通信) */
namespace ThreadMessageQueue {

    template<typename Message>
    struct Subscriber {
        int id;                                         // 订阅id
        std::string topic;                              // 订阅话题
        std::function<void(const Message&)> callback;   // 回调函数
        void* trace;                                    // 调用对象跟踪
    };

    template<typename Message>
    struct PublishHandle {
        std::string topic;      // 话题
        Message message;        // 数据
        bool is_block;          // 是否并发执行, 默认是阻塞的true
        bool is_front;          // 是否头插数据, 默认是尾插false
        int timeout;            // 超时时间 -1为不设超时

        // 构造函数
        PublishHandle
            (
            const std::string& t, 
            const Message& m, 
            bool isblock = true,
            bool isfront = false,
            int tmo = -1
            )
            : topic(t), message(m), is_block(isblock), is_front(isfront), timeout(tmo) {}
    };

    struct DelayLock
    {
        std::mutex mtx;
        std::condition_variable cv;
        bool is_lockHeld;
    };

    template<typename Message, typename Response>
    struct Responder {
        int id; // TODO 暂时不用
        std::string topic;
        std::function<void(const Message&, Response&)> callback;
        void* trace;
    };

    template<typename Message>
    class MessageQueue {
    private:
        MessageQueue()
        {
            std::srand(static_cast<unsigned int>(std::time(nullptr)));
        }
        MessageQueue(const MessageQueue&) = delete;
        MessageQueue& operator=(const MessageQueue&) = delete;

    public:
        static MessageQueue<Message>& getInstance() {
            static MessageQueue<Message> instance;
            return instance;
        }

        // 加入追踪调用对象指针
        void subscribe(const std::string& topic, void* trace, const std::function<void(const Message&)>& callback) {
            std::lock_guard<std::mutex> lock(mutex);

            auto id = std::rand() % 10000;
            auto checkMap = MessageHandle::getInstance().getSubInfo();
            while (true)
            {
                if (checkMap.find(id) != checkMap.end()) {
                    id = std::rand() % 10000;
                } else {
                    break;
                }
            }
            
            MessageHandle::getInstance().registerObject(trace); // 登录跟踪的类的指针
            subscribers.push_back({ id, topic, callback, trace });
            // threadPoolMap[id] = std::make_unique<ThreadPool>(MQ_THREAD_MIN, MQ_THREAD_MAX);
            delayLockMap[id].is_lockHeld = false;
            MessageHandle::getInstance().setTopicNum(id, topic);
        }

        // 没有超时机制的老接口, 尽量不要用这个接口
        void publish(const std::string& topic, const Message& message) {
            std::lock_guard<std::mutex> lock(mutex);

            PublishHandle<Message> pubHandle = {topic, message, true};
            publishHandleProc(pubHandle);
        }

        // 判断是否要并发执行回调(默认是非并发,排队执行回调)
        void publish(const std::string& topic, const bool& is_block, const Message& message) {
            std::lock_guard<std::mutex> lock(mutex);

            PublishHandle<Message> pubHandle = {topic, message, is_block};
            publishHandleProc(pubHandle);
        }

        // 判断是否要并发执行回调, 判断是否要头插数据(默认是尾插)
        void publish(const std::string& topic, const bool& is_block, const bool& is_front, const Message& message) {
            std::lock_guard<std::mutex> lock(mutex);

            PublishHandle<Message> pubHandle = {topic, message, is_block, is_front};
            publishHandleProc(pubHandle);
        }

        // 加入超时判断, 如果有一个订阅回调处理超时, 就不给这个订阅发送消息 (超时判断一定是阻塞的)
        void publish(const std::string& topic, const int& timeout, const Message& message) {
            std::lock_guard<std::mutex> lock(mutex);

            if (timeout < 0) {
                throw std::invalid_argument("Timeout cannot be negative");
            } else {
                PublishHandle<Message> pubHandle = {topic, message, true, false, timeout};
                publishHandleProc(pubHandle);
            }
        }

        // 加入超时判断, 判断是否要头插数据(默认是尾插)
        void publish(const std::string& topic, const bool& is_front, const int& timeout, const Message& message) {
            std::lock_guard<std::mutex> lock(mutex);

            if (timeout < 0) {
                throw std::invalid_argument("Timeout cannot be negative");
            } else {
                PublishHandle<Message> pubHandle = {topic, message, true, is_front, timeout};
                publishHandleProc(pubHandle);
            }
        }

    private:
        int64_t getCurrentEpochSeconds() {
            // 获取世纪秒
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        
            return seconds;
        }

        void setSubInfoContainer (const int& id, const std::string& topic, const int& runState) {
            SubInfo subInfo;
            subInfo.type = "mq";
            subInfo.topic = topic;
            subInfo.subTid = gettid();
            subInfo.runState = runState;
            subInfo.startTime = getCurrentEpochSeconds();
            subInfo.cacheSize = CacheStrategy<Message>::getInstance().getSize(id);

            auto subInfoMap = MessageHandle::getInstance().getSubInfo();
            if (subInfoMap.find(id) == subInfoMap.end()) {
                auto it = subInfoMap.find(id);
                subInfo.runTime = 0; // 如果map中找不到这个订阅，执行时间给0
                subInfo.topicNum = it->second.topicNum;
            } else {
                auto it = subInfoMap.find(id);
                subInfo.runTime = it->second.runTime; // 如果map中找到这个订阅，执行时间给上一次订阅执行回调的时间
                subInfo.topicNum = it->second.topicNum;
            }
            
            MessageHandle::getInstance().setSubInfo(id, subInfo);
        }

        void setSubInfoContainer (const int& id, const std::string& topic, const int& runState, const double& runTime) {
            SubInfo subInfo;
            subInfo.type = "mq";
            subInfo.topic = topic;
            subInfo.subTid = gettid();
            subInfo.runState = runState;
            subInfo.runTime = runTime;
            subInfo.startTime = getCurrentEpochSeconds();
            subInfo.cacheSize = CacheStrategy<Message>::getInstance().getSize(id);

            auto subInfoMap = MessageHandle::getInstance().getSubInfo();
            if (subInfoMap.find(id) != subInfoMap.end()) {
                auto it = subInfoMap.find(id);
                subInfo.topicNum = it->second.topicNum;
            }

            MessageHandle::getInstance().setSubInfo(id, subInfo);
        }

        void recycleTrashs(const int& subId) {
            // 回收订阅锁
            for (auto it = subscriberLockMap.begin(); it != subscriberLockMap.end(); it++) {
                if (it->first == subId) {
                    it = subscriberLockMap.erase(it);
                    break;
                }
            }
            // 回收线程池
            /*
            for (auto it = threadPoolMap.begin(); it != threadPoolMap.end(); it++) {
                if (it->first == subId) {
                    // 判断线程是否执行完，如果有正在处理的线程不能销毁
                    if (0 == threadPoolMap[it->first].get()->Busynum()) {
                        it = threadPoolMap.erase(it);
                    }
                    break;
                }
            }
            */
            // 回收标志位
            for (auto it = delayLockMap.begin(); it != delayLockMap.end(); it++) {
                if (it->first == subId) {
                    // 判断线程是否执行完，如果有正在处理的线程不能销毁
                    delayLockMap.erase(it);
                    break;
                }
            }
            // 回收监控信息
            MessageHandle::getInstance().eraseSubInfo(subId);
        }

        void threadPoolMapHandle (const int& subid, const std::string& topic, const bool& is_block, const std::function<void(const Message&)>& toCall) {
            // threadPoolMap[subid]->Add([=](const int& tmpSubid, const std::string& tmpTopic, const bool& tmpBlock) {
            MessageHandle::getInstance().threadPool->Add([=](const int& tmpSubid, const std::string& tmpTopic, const bool& tmpBlock) {
                if (tmpBlock) {
                    std::mutex& subLock = subscriberLockMap[tmpSubid];
                    std::lock_guard<std::mutex> subTaskLock(subLock);
                    
                    setSubInfoContainer(subid, tmpTopic, 1);
                    auto start = std::chrono::high_resolution_clock::now();

                    auto value = CacheStrategy<Message>::getInstance().front(tmpSubid);
                    toCall(value);

                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> duration = end - start;
                    setSubInfoContainer(subid, tmpTopic, 0, duration.count());
                } else {
                    setSubInfoContainer(subid, tmpTopic, 1);
                    auto start = std::chrono::high_resolution_clock::now();

                    auto value = CacheStrategy<Message>::getInstance().front(tmpSubid);
                    toCall(value);

                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> duration = end - start;
                    setSubInfoContainer(subid, tmpTopic, 0, duration.count());
                }
            }, subid, topic, is_block);
        }

        void threadPoolMapHandle (const int& subid, const std::string& topic, const int& timeout, const std::function<void(const Message&)>& toCall) {
            // threadPoolMap[subid]->Add([=](const int& tmpSubid, const std::string& tmpTopic, const int& tmpTimeout) {
            MessageHandle::getInstance().threadPool->Add([=](const int& tmpSubid, const std::string& tmpTopic, const int& tmpTimeout) {
                std::chrono::_V2::steady_clock::time_point start_time;
                {
                    std::mutex& submtx = delayLockMap[tmpSubid].mtx;
                    std::unique_lock<std::mutex> lock(submtx);

                    start_time = std::chrono::steady_clock::now();

                    while (delayLockMap[tmpSubid].is_lockHeld && (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(tmpTimeout))) {
                        delayLockMap[tmpSubid].cv.wait_for(lock, std::chrono::milliseconds(100)); // 定期检查
                    }

                    if (delayLockMap[tmpSubid].is_lockHeld) {
                        std::cerr << "sub id:" << tmpSubid << " lockHeld." << std::endl;
                    }
                    if (!(std::chrono::steady_clock::now() - start_time < std::chrono::seconds(tmpTimeout))) {
                        auto duration = std::chrono::steady_clock::now() - start_time;
                        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
                        std::cerr << "sub id:" << tmpSubid << " time:" << seconds << " timed out." << std::endl;
                    }

                    if (delayLockMap[tmpSubid].is_lockHeld && !(std::chrono::steady_clock::now() - start_time < std::chrono::seconds(tmpTimeout))) {
                        std::cerr << "sub id:" << tmpSubid << " tid:" << gettid() << " timed out waiting for the lock." << std::endl;
                        if (std::find(subTrashs.begin(), subTrashs.end(), subid) == subTrashs.end()) {
                            subTrashs.push_back(tmpSubid);
                        }
                        delayLockMap[tmpSubid].cv.notify_all(); // 只有有一个排队等待的回调超时了，后面排队的都不用等了直接结束
                        return; // 超时，直接返回
                    }
            
                    delayLockMap[tmpSubid].is_lockHeld = true;
                }

                if (std::find(subTrashs.begin(), subTrashs.end(), subid) == subTrashs.end()) {
                    setSubInfoContainer(subid, tmpTopic, 1);
                    auto start = std::chrono::high_resolution_clock::now();

                    // 执行回调
                    auto value = CacheStrategy<Message>::getInstance().front(tmpSubid);
                    toCall(value);

                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> duration = end - start;
                    setSubInfoContainer(subid, tmpTopic, 0, duration.count());
                } else {
                    return; // 超时，直接返回
                }

                {
                    std::mutex& submtx = delayLockMap[tmpSubid].mtx;
                    std::unique_lock<std::mutex> lock(submtx);
                    // 释放锁
                    delayLockMap[tmpSubid].is_lockHeld = false;
                }

                delayLockMap[tmpSubid].cv.notify_one(); // 通知一个线程执行接完成
            }, subid, topic, timeout);
        }

        void publishHandleProc (const PublishHandle<Message>& publishHandle) {
            // 发布的处理程序, 做一个封装
            for (auto subscriber = subscribers.begin(); subscriber != subscribers.end(); subscriber++) {
                if (!MessageHandle::getInstance().isObjectAlive(subscriber->trace)) {
                    // 遍历订阅容器的同时, 判断跟踪的类是否存活
                    
                    /*
                    if (0 == threadPoolMap[subscriber->id].get()->Busynum()) { // 确保没有正在执行的线程了再去回收
                        std::cerr << "id:" << subscriber->id << " topic:" << subscriber->topic << " subscriber trace miss." << std::endl;
                        // 回收各类容器
                        recycleTrashs(subscriber->id);

                        // 回收订阅
                        subscriber = subscribers.erase(subscriber);
                    }
                    */
                } else if (std::find(subTrashs.begin(), subTrashs.end(), subscriber->id) != subTrashs.end()) {
                    // 判断这个订阅是否在订阅垃圾列表中
                } else {
                    if (subscriber->topic == publishHandle.topic) {
                        if (publishHandle.is_front == false) { // 数据插尾部
                            CacheStrategy<Message>::getInstance().push_back(subscriber->id, publishHandle.message);
                        } else { // 数据插头部
                            CacheStrategy<Message>::getInstance().push_front(subscriber->id, publishHandle.message);
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 加点延时保证它的先后顺序

                        if (publishHandle.timeout != -1 && publishHandle.is_block != false) {
                            threadPoolMapHandle(subscriber->id, subscriber->topic, publishHandle.timeout, subscriber->callback);
                        } else {
                            threadPoolMapHandle(subscriber->id, subscriber->topic, publishHandle.is_block, subscriber->callback);
                        }
                    }
                }                
            }
        }

    private:
        std::vector<Subscriber<Message>> subscribers;
        std::mutex mutex;
        std::vector<int> subTrashs;
        std::unordered_map<int, DelayLock> delayLockMap; // 超时发布才会用到, 用来解锁的
        std::unordered_map<int, std::mutex> subscriberLockMap; // 每条订阅的回调函数加锁, 订阅回调函数执行完才能执行下一个回调
        // std::unordered_map<int, std::unique_ptr<ThreadPool>> threadPoolMap;
    };

    template<typename Message, typename Response>
    class ServiceQueue {
    private:
        ServiceQueue()
            {
                std::srand(static_cast<unsigned int>(std::time(nullptr)));
            }
        ServiceQueue(const ServiceQueue&) = delete;
        ServiceQueue& operator=(const ServiceQueue&) = delete;

    public:
        static ServiceQueue<Message, Response>& getInstance() {
            static ServiceQueue<Message, Response> instance;
            return instance;
        }

        void subscribe(const std::string& topic, void* trace, const std::function<void(const Message&, Response&)>& callback) {
            std::lock_guard<std::mutex> lock(mutex);
            
            auto id = std::rand() % 10000;
            auto checkMap = MessageHandle::getInstance().getSubInfo();
            while (true)
            {
                if (checkMap.find(id) != checkMap.end()) {
                    id = std::rand() % 10000;
                } else {
                    break;
                }
            }

            int match = 0;
            for (auto responder : responders) {
                // 服务队列的订阅是点对点的,只能存在一个话题,在订阅阶段将重复的话题过滤掉
                if (responder.topic == topic) {
                    match = 1;
                    break;
                }
            }
            if (!match) {
                MessageHandle::getInstance().registerObject(trace); // 登录跟踪的类的指针
                responders.push_back({ id, topic, callback, trace });
            }
        }

        // 加入超时判断, 由于服务队列要等应答,如果订阅有死循环阻塞,则发布方会一直阻塞
        Response publish(const std::string& topic, const int& timeout, const Message& message) {
            Response response;
            int trashTopicMatch = 0;

            std::function<void(const Message&, Response&)> toCall;

            {
                std::lock_guard<std::mutex> lock(mutex);
                // TODO 这里遍历太多, 可能会影响性能,后期待优化
                // 遍历服务队列订阅列表, 如果跟踪对象不存在了,则删除订阅
                for (auto responder = responders.begin(); responder != responders.end(); responder++) {
                    if (!MessageHandle::getInstance().isObjectAlive(responder->trace)) {
                        responder = responders.erase(responder);
                    }
                }
                for (auto trashTopic : v_trashTopic) {
                    if (trashTopic == topic) {
                        trashTopicMatch = 1;
                        break;
                    }
                }
                if (!trashTopicMatch) {
                    for (const auto& responder : responders) {
                        // 服务队列是点对点的, 一条订阅对应一个发布
                        if (responder.topic == topic) {
                            toCall = responder.callback;
                            break;
                        }
                    }
                }
            }

            if (toCall) {
                std::mutex cv_mutex;
                std::condition_variable cv;
                bool done = false;

                MessageHandle::getInstance().threadPool->Add([&](){
                    setSubInfoContainer(topic, 1);
                    auto start = std::chrono::high_resolution_clock::now();

                    // 执行回调函数
                    toCall(message, response);
                    {
                        std::lock_guard<std::mutex> lock(cv_mutex);
                        done = true;
                    }

                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> duration = end - start;
                    setSubInfoContainer(topic, 0, duration.count());

                    cv.notify_one();
                });

                // 等待回调函数完成或超时
                {
                    std::unique_lock<std::mutex> lock(cv_mutex);
                    if (!cv.wait_for(lock, std::chrono::seconds(timeout), [&]() { return done; })) {
                        // 超时, 可以选择记录日志或处理超时情况
                        v_trashTopic.push_back(topic);
                        std::cerr << "Callback timed out!" << std::endl;
                    }
                }
            }

            return response;
        }

    private:
        int64_t getCurrentEpochSeconds() {
            // 获取世纪秒
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        
            return seconds;
        }

        void setSubInfoContainer (const std::string& topic, const int& runState) {
            SubInfo subInfo;
            subInfo.type = "sq";
            subInfo.topic = topic;
            subInfo.subTid = gettid();
            subInfo.runState = runState;
            subInfo.startTime = getCurrentEpochSeconds();

            for (auto responder : responders) {
                if (responder.topic == topic) {
                    auto subInfoMap = MessageHandle::getInstance().getSubInfo();
                    if (subInfoMap.find(responder.id) == subInfoMap.end()) {
                        subInfo.runTime = 0; // 如果map中找不到这个订阅，执行时间给0
                    } else {
                        auto it = subInfoMap.find(responder.id);
                        subInfo.runTime = it->second.runTime; // 如果map中找到这个订阅，执行时间给上一次订阅执行回调的时间
                    }

                    MessageHandle::getInstance().setSubInfo(responder.id, subInfo);
                    break;
                }
            }
        }
        
        void setSubInfoContainer (const std::string& topic, const int& runState, const double& runTime) {
            SubInfo subInfo;
            subInfo.type = "sq";
            subInfo.topic = topic;
            subInfo.subTid = gettid();
            subInfo.runState = runState;
            subInfo.runTime = runTime;
            subInfo.startTime = getCurrentEpochSeconds();

            for (auto responder : responders) {
                if (responder.topic == topic) {
                    MessageHandle::getInstance().setSubInfo(responder.id, subInfo);
                    break;
                }
            }
        }

    private:
        std::vector<Responder<Message, Response>> responders;
        std::vector<std::string> v_trashTopic; // 超时的订阅, 将话题保存到垃圾话题队列
        std::mutex mutex;
    };

};
