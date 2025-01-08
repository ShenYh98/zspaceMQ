#pragma once

#include <mutex>
#include <ctime>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <functional>
#include <unordered_set>
#include <condition_variable>

#include "ThreadPool.h"
#include "DequeCache.hpp"

/* posix通信相关 */
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>

#define  MQ_THREAD_MAX     4
#define  MQ_THREAD_MIN     1

#define  SQ_THREAD_MAX     16
#define  SQ_THREAD_MIN     1

#define  MQ2_THREAD_MAX    16
#define  MQ2_THREAD_MIN    1

#define  SQ2_THREAD_MAX    16
#define  SQ2_THREAD_MIN    1

#define  INIT_SUB_PATH     "./tmp/init_sub_fifo"

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

    template<typename Message, typename Response>
    struct Responder {
        int id; // TODO 暂时不用
        std::string topic;
        std::function<void(const Message&, Response&)> callback;
        void* trace;
    };

    struct DelayLock
    {
        std::mutex mtx;
        std::condition_variable cv;
        bool is_lockHeld;
    };

    // 生命周期管理类
    class MessageHandle {
    private:
        // 私有构造函数，防止外部直接创建实例
        MessageHandle()
        {}
        // 禁止拷贝构造函数和赋值操作符，确保单例
        MessageHandle(const MessageHandle&) = delete;
        MessageHandle& operator=(const MessageHandle&) = delete;
    
    public:
        static MessageHandle& getInstance() {
            static MessageHandle instance;
            return instance;
        }

        void registerObject(void* obj) {
            std::lock_guard<std::mutex> lock(mutex);
            if (aliveObjects.find(obj) == aliveObjects.end()) {
                aliveObjects.insert(obj);
            }
        }
    
        void unregisterObject(void* obj) {
            std::lock_guard<std::mutex> lock(mutex);
            aliveObjects.erase(obj);
        }
    
        bool isObjectAlive(void* obj) const {
            std::lock_guard<std::mutex> lock(mutex);
            return aliveObjects.find(obj) != aliveObjects.end();
        }

    private:
        mutable std::mutex mutex;
        mutable std::mutex set_topic_mutex;
        std::unordered_set<void*> aliveObjects;
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

        // 这个订阅里没有追踪调用对象的指针,弃用
        void subscribe(const std::string& topic, const std::function<void(const Message&)>& callback) {
            std::lock_guard<std::mutex> lock(mutex);

            auto id = std::rand() % 10000;
            subscribers.push_back({ id, topic, callback });
            threadPoolMap[id] = std::make_unique<ThreadPool>(MQ_THREAD_MIN, MQ_THREAD_MAX);
            delayLockMap[id].is_lockHeld = false;
        }

        // 加入追踪调用对象指针
        void subscribe(const std::string& topic, void* trace, const std::function<void(const Message&)>& callback) {
            std::lock_guard<std::mutex> lock(mutex);

            auto id = std::rand() % 10000;
            MessageHandle::getInstance().registerObject(trace); // 登录跟踪的类的指针
            subscribers.push_back({ id, topic, callback, trace });
            threadPoolMap[id] = std::make_unique<ThreadPool>(MQ_THREAD_MIN, MQ_THREAD_MAX);
            delayLockMap[id].is_lockHeld = false;
        }

        void publish(const std::string& topic, const Message& message) {
            std::lock_guard<std::mutex> lock(mutex);

            for (auto subscriber = subscribers.begin(); subscriber != subscribers.end(); subscriber++) {
                if (!MessageHandle::getInstance().isObjectAlive(subscriber->trace)) {
                    // 遍历订阅容器的同时,判断跟踪的类是否存活
                    // 回收各类容器
                    recycleTrashs(subscriber->id);
                    // 回收订阅
                    subscriber = subscribers.erase(subscriber);
                } else if (std::find(subTrashs.begin(), subTrashs.end(), subscriber->id) != subTrashs.end()) {
                    // 判断这个订阅是否在订阅垃圾列表中
                } else {
                    if (subscriber->topic == topic) {
                        CacheStrategy<Message>::getInstance().push_back(subscriber->id, message);
                        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 加点延时保证它的先后顺序
                        
                        threadPoolMap[subscriber->id]->Add([=](const int& tmpSubid) {
                            std::mutex& subLock = subscriberLockMap[tmpSubid];
                            std::lock_guard<std::mutex> subTaskLock(subLock);

                            auto value = CacheStrategy<Message>::getInstance().front(tmpSubid);
                            subscriber->callback(value);
                        }, subscriber->id);
                    }
                }                
            }
        }

        // 加入超时判断，如果有一个订阅回调处理超时，就不给这个订阅发送消息
        void publish(const std::string& topic, const int& timeout, const Message& message) {
            std::lock_guard<std::mutex> lock(mutex);

            for (auto subscriber = subscribers.begin(); subscriber != subscribers.end(); subscriber++) {
                if (!MessageHandle::getInstance().isObjectAlive(subscriber->trace)) {
                    // 遍历订阅容器的同时,判断跟踪的类是否存活
                    // 回收各类容器
                    recycleTrashs(subscriber->id);

                    // 回收订阅
                    subscriber = subscribers.erase(subscriber);
                } else if (std::find(subTrashs.begin(), subTrashs.end(), subscriber->id) != subTrashs.end()) {
                    // 判断这个订阅是否在订阅垃圾列表中
                } else {
                    if (subscriber->topic == topic) {
                        CacheStrategy<Message>::getInstance().push_back(subscriber->id, message);
                        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 加点延时保证它的先后顺序
                        
                        threadPoolMap[subscriber->id]->Add([=](const int& tmpSubid, const int& tmpTimeout) {
                            auto start_time = std::chrono::steady_clock::now();
                            {
                                std::mutex& submtx = delayLockMap[tmpSubid].mtx;
                                std::unique_lock<std::mutex> lock(submtx);

                                while (delayLockMap[tmpSubid].is_lockHeld && (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(tmpTimeout))) {
                                    delayLockMap[tmpSubid].cv.wait_for(lock, std::chrono::milliseconds(100)); // 定期检查
                                }

                                if (delayLockMap[tmpSubid].is_lockHeld) {
                                    std::cerr << "sub id:" << tmpSubid << " timed out waiting for the lock." << std::endl;
                                    if (std::find(subTrashs.begin(), subTrashs.end(), subscriber->id) == subTrashs.end()) {
                                        subTrashs.push_back(tmpSubid);
                                    }
                                    delayLockMap[tmpSubid].cv.notify_all(); // 只有有一个排队等待的回调超时了，后面排队的都不用等了直接结束
                                    return; // 超时，直接返回
                                }
                        
                                delayLockMap[tmpSubid].is_lockHeld = true;
                            }

                            if (std::find(subTrashs.begin(), subTrashs.end(), subscriber->id) == subTrashs.end()) {
                                // 执行回调
                                auto value = CacheStrategy<Message>::getInstance().front(tmpSubid);
                                subscriber->callback(value);
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
                        }, subscriber->id, timeout);
                    }
                }                
            }
        }

    private:
        void recycleTrashs(const int& subId) {
            // 回收订阅锁
            for (auto it = subscriberLockMap.begin(); it != subscriberLockMap.end(); it++) {
                if (it->first == subId) {
                    it = subscriberLockMap.erase(it);
                    break;
                }
            }
            // 回收线程池
            for (auto it = threadPoolMap.begin(); it != threadPoolMap.end(); it++) {
                if (it->first == subId) {
                    // 判断线程是否执行完，如果有正在处理的线程不能销毁
                    if (0 == threadPoolMap[it->first].get()->Busynum()) {
                        it = threadPoolMap.erase(it);
                    }
                    break;
                }
            }
            // 回收标志位
            for (auto it = delayLockMap.begin(); it != delayLockMap.end(); it++) {
                if (it->first == subId) {
                    // 判断线程是否执行完，如果有正在处理的线程不能销毁
                    delayLockMap.erase(it);
                    break;
                }
            }
        }

    private:
        std::vector<Subscriber<Message>> subscribers;
        std::mutex mutex;
        std::vector<int> subTrashs;
        std::unordered_map<int, DelayLock> delayLockMap; // 超时发布才会用到,用来解锁的
        std::unordered_map<int, std::mutex> subscriberLockMap; // 每条订阅的回调函数加锁，订阅回调函数执行完才能执行下一个回调
        std::unordered_map<int, std::unique_ptr<ThreadPool>> threadPoolMap;
    };

    template<typename Message, typename Response>
    class ServiceQueue {
    private:
        ServiceQueue() : 
            threadPool(SQ_THREAD_MIN, SQ_THREAD_MAX) 
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

        Response publish(const std::string& topic, const Message& message) {
            Response response;

            std::function<void(const Message&, Response&)> toCall;

            {
                std::lock_guard<std::mutex> lock(mutex);
                // TODO 这里遍历太多,可能会影响性能,后期待优化
                // 遍历服务队列订阅列表,如果跟踪对象不存在了,则删除订阅
                for (auto responder = responders.begin(); responder != responders.end(); responder++) {
                    if (!MessageHandle::getInstance().isObjectAlive(responder->trace)) {
                        responder = responders.erase(responder);
                    }
                }
                for (const auto& responder : responders) {
                    // 服务队列是点对点的,一条订阅对应一个发布
                    if (responder.topic == topic) {
                        toCall = responder.callback;
                        break;
                    }
                }
            }

            if (toCall) {
                toCall(message, response);
            }

            return response;
        }

        // 加入超时判断,由于服务队列要等应答,如果订阅有死循环阻塞,则发布方会一直阻塞
        Response publish(const std::string& topic, const int& timeout, const Message& message) {
            Response response;
            int trashTopicMatch = 0;

            std::function<void(const Message&, Response&)> toCall;

            {
                std::lock_guard<std::mutex> lock(mutex);
                // TODO 这里遍历太多,可能会影响性能,后期待优化
                // 遍历服务队列订阅列表,如果跟踪对象不存在了,则删除订阅
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
                        // 服务队列是点对点的,一条订阅对应一个发布
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

                threadPool.Add([&](){
                    // 执行回调函数
                    toCall(message, response);
                    {
                        std::lock_guard<std::mutex> lock(cv_mutex);
                        done = true;
                    }
                    cv.notify_one();
                });

                // 等待回调函数完成或超时
                {
                    std::unique_lock<std::mutex> lock(cv_mutex);
                    if (!cv.wait_for(lock, std::chrono::seconds(timeout), [&]() { return done; })) {
                        // 超时，可以选择记录日志或处理超时情况
                        v_trashTopic.push_back(topic);
                        std::cerr << "Callback timed out!" << std::endl;
                    }
                }
            }

            return response;
        }

    private:
        std::vector<Responder<Message, Response>> responders;
        std::vector<std::string> v_trashTopic; // 超时的订阅,将话题保存到垃圾话题队列
        ThreadPool threadPool;
        std::mutex mutex;
    };

};

/* 第二版线程及进程双通信的消息队列(线程进程间都可以通信) */
namespace ProcessMessageQueue {

    class MessageHandle
    {
    private:
        // 私有构造函数，防止外部直接创建实例
        MessageHandle() {}
        // 禁止拷贝构造函数和赋值操作符，确保单例
        MessageHandle(const MessageHandle&) = delete;
        MessageHandle& operator=(const MessageHandle&) = delete;
    
    public:
        static MessageHandle& getInstance() {
            static MessageHandle instance;
            return instance;
        }

        void setVTopic(const std::vector<std::string>& v_recvTopic) {
            this->v_recv_topic = v_recvTopic;
        }

        void getVTopic(std::vector<std::string>& v_recvTopic) {
            v_recvTopic = this->v_recv_topic;
        }

    private:
        std::vector<std::string> v_recv_topic;
    };

    template<typename Message>
    class MessageQueue {
    private:
        MessageQueue() : 
            threadPool(MQ2_THREAD_MIN, MQ2_THREAD_MAX) , 
            init_sub_path(INIT_SUB_PATH)
            {
            }
        MessageQueue(const MessageQueue&) = delete;
        MessageQueue& operator=(const MessageQueue&) = delete;

    public:
        static MessageQueue<Message>& getInstance() {
            static MessageQueue<Message> instance;
            return instance;
        }

        // 订阅在初始化处完成
        void subscribe(const std::string& topic, const std::function<void(const Message&)>& callback) {
            threadPool.Add(std::bind(&MessageQueue::server, this, std::placeholders::_1, std::placeholders::_2), topic, callback);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 发送在初始化处完成
        void publish(const std::string& topic, Message& data) {
            MessageHandle::getInstance().getVTopic(this->v_recv_topic);

            for (auto it : v_recv_topic) {
                if (matchTopic(it, topic)) {
                    client(it, data);
                }
            }
        } 

    private:
        void client(const std::string& topic, Message& data) {
            // client
            try 
            {
                std::string queue_name = topic;
            
                mqd_t mq;
                mq = mq_open(queue_name.c_str(), O_WRONLY | O_NONBLOCK);
                if (mq == -1) {
                    std::cerr << "2 Failed to open/create message queue" << std::endl;
                    return ;
                }

                // 将发送的数据序列化
                std::string serialized_data;
                data.SerializeToString(&serialized_data);
                // 发送消息
                const char *message = serialized_data.c_str();
                if (mq_send(mq, message, strlen(message) + 1, 0) == -1) {
                    std::cerr << "Failed to send message" << std::endl;
                    mq_close(mq);
                    return ;
                }
            
                // 关闭消息队列
                mq_close(mq);
            } 
            catch (const std::exception& exc) 
            {
                std::cerr << "Client failed: " << exc.what() << std::endl;
            }
        }

        void server(const std::string& topic, const std::function<void(const Message&)>& callback) {
            try 
            {
                // 获取当前线程的ID
                auto this_id = std::this_thread::get_id();
                
                // 使用 std::stringstream 将线程ID转换为字符串
                std::stringstream ss;
                ss << this_id;
                std::string thread_id_str = ss.str();

                std::string tmp_queue_name = "/t" + thread_id_str + "_" + topic;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    pipeWrite(init_sub_path, tmp_queue_name); 
                }

                while (true) {
                    std::string queue_name = tmp_queue_name;

                    mqd_t mq;
                    struct mq_attr attr;
                    attr.mq_flags = 0;
                    attr.mq_maxmsg = 10;
                    attr.mq_msgsize = 256;
                    attr.mq_curmsgs = 0;
                
                    // 打开/创建消息队列
                    mq = mq_open(queue_name.c_str(), O_CREAT | O_RDWR, 0666, &attr);
                    if (mq == -1) {
                        std::cerr << "1 Failed to open/create message queue, topic:" << queue_name
                                << ", error: " << strerror(errno) << std::endl;
                        return ;
                    }

                    // 接收消息
                    char buffer[256];
                    ssize_t bytes_read = mq_receive(mq, buffer, 256, NULL);
                    if (bytes_read >= 0) {
                        buffer[bytes_read] = '\0';

                        Message data;
                        if (!data.ParseFromString(buffer)) {
                            std::cerr << "Failed to parse received message" << std::endl;
                            mq_close(mq);
                            return;
                        }

                        callback(data);
                        
                    } else {
                        std::cerr << "Failed to receive message or queue is empty" << std::endl;
                    }
                
                    // 关闭消息队列
                    mq_close(mq);
                }
            } 
            catch (const std::exception& exc) 
            {
                std::cerr << "Server failed: " << exc.what() << std::endl;
            }
        }

        int pipeWrite(const std::string& fifo_path, const std::string& message) { 
            int fifo_fd = open(fifo_path.c_str(), O_WRONLY);
            if (fifo_fd == -1) {
                perror("open");
                return 1;
            }
        
            write(fifo_fd, message.c_str(), message.size());
        
            close(fifo_fd);

            return 0;
        }

        bool matchTopic(const std::string& input, const std::string& pattern) {
            std::size_t slash_pos = input.find('/');
            if (slash_pos == std::string::npos) {
                // 如果没有找到'/'，返回原字符串
                return false;
            }
        
            // 从'/'之后开始查找第一个'_'的位置
            std::size_t underscore_pos = input.find('_', slash_pos);
            if (underscore_pos == std::string::npos) {
                // 如果没有找到'_'，返回原字符串（或者你可以选择抛出异常或进行其他处理）
                return false;
            }

            std::string tmpInput = input.substr(underscore_pos + 1);
            if (tmpInput == pattern) {
                return true;
            } else {
                return false;
            }
        }

    private:
        ThreadPool threadPool;
        std::vector<std::string> v_recv_topic;      // 用于接收所有订阅的话题
        std::mutex mutex;
        std::string init_sub_path;   // 通过这个管道路径,每当创建订阅,将其话题发给初始化函数
    };

    template<typename Message, typename Response>
    class ServiceQueue {
    private:
        ServiceQueue() : 
            threadPool(SQ2_THREAD_MIN, SQ2_THREAD_MAX)
        {}
        ServiceQueue(const ServiceQueue&) = delete;
        ServiceQueue& operator=(const ServiceQueue&) = delete;

    public:
        static ServiceQueue<Message, Response>& getInstance() {
            static ServiceQueue<Message, Response> instance;
            return instance;
        }

        // 订阅在初始化处完成
        void subscribe(const std::string& topic, const std::function<void(const Message&, Response&)>& callback) {
            threadPool.Add(std::bind(&ServiceQueue::server, this, std::placeholders::_1, std::placeholders::_2), topic, callback);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 发送在初始化处完成
        Response publish(const std::string& topic, Message& data) {
            auto res = client(topic, data);
            return res;
        } 

    private:
        Response client(const std::string& topic, Message& request_data) {
            try 
            {
                std::string request_queue_name = "/request_" + topic;
                std::string response_queue_name = "/response_" + topic;
                Response response_data;
        
                // 打开请求队列
                mqd_t request_mq = mq_open(request_queue_name.c_str(), O_WRONLY | O_NONBLOCK);
                if (request_mq == -1) {
                    std::cerr << "1 Failed to open/create request message queue" << std::endl;
                    return response_data;;
                }
        
                // 序列化请求数据并发送
                std::string serialized_request;
                request_data.SerializeToString(&serialized_request);
                if (mq_send(request_mq, serialized_request.c_str(), serialized_request.size() + 1, 0) == -1) {
                    std::cerr << "Failed to send request message" << std::endl;
                    mq_close(request_mq);
                    return response_data;;
                }
                mq_close(request_mq);
        
                // 打开响应队列
                mqd_t response_mq = mq_open(response_queue_name.c_str(), O_RDONLY | O_NONBLOCK);
                if (response_mq == -1) {
                    std::cerr << "2 Failed to open/create response message queue" << std::endl;
                    return response_data;
                }
        
                // 等待并接收响应
                char buffer[256];
                ssize_t bytes_read;
                while ((bytes_read = mq_receive(response_mq, buffer, 256, NULL)) == -1 && errno == EAGAIN) {
                    // 非阻塞模式下，如果队列为空，会立即返回 EAGAIN
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
        
                if (bytes_read >= 0) {
                    buffer[bytes_read] = '\0';

                    if (!response_data.ParseFromString(buffer)) {
                        std::cerr << "Failed to parse received response" << std::endl;
                    } else {
                        // std::cout << "Received response successfully" << std::endl;
                    }
                } else {
                    std::cerr << "Failed to receive response" << std::endl;
                }
        
                mq_close(response_mq);
                return response_data;
            } 
            catch (const std::exception& exc) 
            {
                Response response_data;
                std::cerr << "Client failed: " << exc.what() << std::endl;
                return response_data;
            }
        }
    
        void server(const std::string& topic, const std::function<void(const Message&, Response&)>& callback) {
            try 
            {
                std::string request_queue_name = "/request_" + topic;
                std::string response_queue_name = "/response_" + topic;
        
                struct mq_attr attr;
                attr.mq_flags = 0;
                attr.mq_maxmsg = 10;
                attr.mq_msgsize = 256;
                attr.mq_curmsgs = 0;
        
                // 打开/创建请求队列
                mqd_t request_mq = mq_open(request_queue_name.c_str(), O_CREAT | O_RDONLY | O_EXCL , 0666, &attr);
                if (request_mq == -1) {
                    std::cerr << "3 Failed to open/create request message queue" << std::endl;
                    return;
                }
        
                // 打开/创建响应队列
                mqd_t response_mq = mq_open(response_queue_name.c_str(), O_CREAT | O_WRONLY | O_EXCL , 0666, &attr);
                if (response_mq == -1) {
                    std::cerr << "4 Failed to open/create response message queue" << std::endl;
                    mq_close(request_mq);
                    return;
                }
        
                while (true) {
                    char request_buffer[256];
                    ssize_t bytes_read = mq_receive(request_mq, request_buffer, 256, NULL);
                    if (bytes_read >= 0) {
                        request_buffer[bytes_read] = '\0';
        
                        Message request_data;
                        Response response_data;
                        if (request_data.ParseFromString(request_buffer)) {
                            callback(request_data, response_data);
        
                            // 序列化响应数据并发送
                            std::string serialized_response;
                            response_data.SerializeToString(&serialized_response);
                            if (mq_send(response_mq, serialized_response.c_str(), serialized_response.size() + 1, 0) == -1) {
                                std::cerr << "Failed to send response message" << std::endl;
                            }
                        } else {
                            std::cerr << "Failed to parse received request" << std::endl;
                        }
                    } else {
                        std::cerr << "Failed to receive request or queue is empty" << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
        
                mq_close(request_mq);
                mq_close(response_mq);
            } 
            catch (const std::exception& exc) 
            {
                std::cerr << "Server failed: " << exc.what() << std::endl;
            }
        }
    private:
        ThreadPool threadPool;
    };

};
