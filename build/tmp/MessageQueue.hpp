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
#include <unordered_set>
#include <condition_variable>
#include <filesystem>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <fcntl.h>

#include "DequeCache.hpp"
#include "MessageHandle.h"

/* TODO 暂时屏蔽
#include "xpack/json.h"
*/

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

    template<typename Message, typename Response>
    struct Responder {
        int id; // TODO 暂时不用
        std::string topic;
        std::function<void(const Message&, Response&)> callback;
        void* trace;
    };

    typedef enum {
        THREAD,
        FORK
    } MQTYPE;

    template<typename Message>
    class MessageQueue{
    private:
        MessageQueue()
        {
            std::lock_guard<std::mutex> lock(mutex);
            std::srand(static_cast<unsigned int>(std::time(nullptr)));
        }
        ~MessageQueue()
        {
            stop();
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

            subscribers.push_back({ id, topic, callback, trace });
            subscriberDoingMap[id] = false;
            MessageHandle::getInstance().setTopicNum(id, topic);
        }

        // 这个接口支持进程通信
        void subscribe(const std::string& topic, const MQTYPE& mqType, void* trace, const std::function<void(const Message&)>& callback) {
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

            subscribers.push_back({ id, topic, callback, trace });
            subscriberDoingMap[id] = false;
            MessageHandle::getInstance().setTopicNum(id, topic);

            if (MQTYPE::FORK == mqType) {
                // 如果启用进程通信，还要执行下面逻辑
                MessageHandle::getInstance().threadPool->Add([=](const std::string& topic, const std::function<void(const Message&)>& callback) {
                    int socketFd = -1;
                    bool connected = false;
                    std::string relTopicPath = MessageHandle::getInstance().topicPath + topic;

                    // 最外层循环确保掉线重连
                    while (true)
                    {
                        while (!connect(connected, relTopicPath, socketFd)) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // 连接失败，时延2s再尝试
                        }

                        while (connected) {
                            if (waitForMessage(connected, socketFd, 100)) {
                                receive(connected, socketFd, callback);
                            }
                        }
                    }
                }, topic, callback);
            }
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
        int publish(const std::string& topic, const int& timeout, const Message& message) {
            std::lock_guard<std::mutex> lock(mutex);

            int res = 0;

            if (timeout < 0) {
                throw std::invalid_argument("Timeout cannot be negative");
            } else {
                PublishHandle<Message> pubHandle = {topic, message, true, false, timeout};
                res = publishHandleProc(pubHandle);
            }

            return res;
        }

        // 加入超时判断, 判断是否要头插数据(默认是尾插)
        int publish(const std::string& topic, const bool& is_front, const int& timeout, const Message& message) {
            std::lock_guard<std::mutex> lock(mutex);

            int res = 0;
            
            if (timeout < 0) {
                throw std::invalid_argument("Timeout cannot be negative");
            } else {
                PublishHandle<Message> pubHandle = {topic, message, true, is_front, timeout};
                res = publishHandleProc(pubHandle);
            }

            return res;
        }

        // 这个接口支持进程通信
        int publish(const std::string& topic, const MQTYPE& mqType, const int& timeout, const Message& message) {
            std::lock_guard<std::mutex> lock(mutex);

            int res = 0;

            if (MQTYPE::THREAD == mqType) {
                // 如果是线程模式，直接采用老接口
                res = publish(topic, timeout, message);
            } else if (MQTYPE::FORK == mqType) {
                std::string relTopicPath = MessageHandle::getInstance().topicPath + topic;

                if(start(relTopicPath)) {
                    sendMsg(relTopicPath, message);
                }
            }

            return res;
        }
    private:
        void setThreadAffinity(pthread_t thread, const std::vector<int>& cpu_ids) {
            // 配置cpu亲和性
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            
            for (int cpu : cpu_ids) {
                CPU_SET(cpu, &cpuset);  // 添加多个 CPU 核心
            }
            
            if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
            } else {
            }
        }

        int64_t getCurrentEpochSeconds() {
            // 获取世纪秒
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        
            return seconds;
        }

        void threadPoolMapHandle (const int& subid, const std::string& topic, const bool& is_block, const std::function<void(const Message&)>& toCall) {
            MessageHandle::getInstance().threadPool->Add([=](const int& tmpSubid, const std::string& tmpTopic, const bool& tmpBlock) {
                if (!MessageHandle::getInstance().cpus.empty()) {
                    setThreadAffinity(pthread_self(), MessageHandle::getInstance().cpus); // 配置cpu亲和性
                }

                if (tmpBlock) {
                    std::mutex& subLock = subscriberLockMap[tmpSubid];
                    std::lock_guard<std::mutex> subTaskLock(subLock);

                    subscriberDoingMap[tmpSubid] = true;

                    auto value = CacheStrategy<Message>::getInstance().front(tmpSubid);
                    toCall(value);

                    subscriberDoingMap[tmpSubid] = false;
                } else {
                    auto value = CacheStrategy<Message>::getInstance().front(tmpSubid);
                    toCall(value);
                }
            }, subid, topic, is_block);
        }

        int publishHandleProc (const PublishHandle<Message>& publishHandle) {
            // 发布的处理程序, 做一个封装
            int res = 0;

            for (auto subscriber = subscribers.begin(); subscriber != subscribers.end(); subscriber++) {
                if (subscriber->topic == publishHandle.topic) {
                    if (subscriberDoingMap[subscriber->id] == true) { // 判断锁是否正在被使用
                        int topicSize = 0;
                        for (auto it : subscribers) {
                            if (it.topic == subscriber->topic) {
                                topicSize++;
                            }
                        }
                        res = 1;
                    } else {
                        if (publishHandle.is_front == false) { // 数据插尾部
                            CacheStrategy<Message>::getInstance().push_back(subscriber->id, publishHandle.message);
                        } else { // 数据插头部
                            CacheStrategy<Message>::getInstance().push_front(subscriber->id, publishHandle.message);
                        }

                        if (publishHandle.timeout != -1 && publishHandle.is_block != false) {
                            threadPoolMapHandle(subscriber->id, subscriber->topic, publishHandle.is_block, subscriber->callback);
                        } else {
                            threadPoolMapHandle(subscriber->id, subscriber->topic, publishHandle.is_block, subscriber->callback);
                        }
                    }
                }
            }

            return res;
        }

        /* 进程通信相关接口（订阅） */
        bool connect(bool& connected, const std::string& topic, int& socketFd) {
            socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (socketFd < 0) {
                perror("socket");
                return false;
            }
            
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, topic.c_str(), sizeof(addr.sun_path) - 1);
            
            if (::connect(socketFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                perror("connect");
                close(socketFd);
                socketFd = -1;
                return false;
            }

            connected = true;

            return true;
        }

        void disconnect(bool& connected, int& socketFd) {
            if (socketFd >= 0) {
                close(socketFd);
                socketFd = -1;
            }
            connected = false;
        }

        bool waitForMessage(const bool& connected, const int& socketFd, int timeoutMs = -1) {
            if (!connected) {
                return false;
            }
            
            struct pollfd pfd;
            pfd.fd = socketFd;
            pfd.events = POLLIN;
            
            int ret = poll(&pfd, 1, timeoutMs);

            if (ret < 0) {
                perror("poll");
                return false;
            } else if (ret == 0) {
                // 超时
                return false;
            }
            
            return (pfd.revents & POLLIN) != 0;
        }

        bool receive(bool& connected, int& socketFd, const std::function<void(const Message&)>& callback) {
            if (!connected) {
                return false;
            }
            
            char buffer[4096];
            ssize_t bytesRead = recv(socketFd, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead <= 0) {
                if (bytesRead == 0) {
                } else {
                    perror("recv");
                }

                disconnect(connected, socketFd);

                return false;
            }
            buffer[bytesRead] = '\0';

            /* TODO 暂时屏蔽
            Message data;
            xpack::json::decode(buffer, data); // JSON转结构体
            callback(data);
            */
            
            return true;
        }

        /* 进程通信相关接口（发布） */
        bool start(const std::string& topic) {
            // 创建Unix域套接字
            int serverFd = -1;
            if (serverFdMap.find(topic) == serverFdMap.end()) {
                if (createDirectoriesRecursive(topic)) { // 目录创建成功
                    serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
                }
            } else {
                return true;
            }
            
            if (serverFd < 0) {
                perror("socket");
                return false;
            }
            
            // 设置非阻塞模式
            int flags = fcntl(serverFd, F_GETFL, 0);
            fcntl(serverFd, F_SETFL, flags | O_NONBLOCK);
            
            // 绑定到地址
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, topic.c_str(), sizeof(addr.sun_path) - 1);
            
            // 移除可能存在的旧socket文件
            unlink(topic.c_str());
            
            if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                perror("bind");
                close(serverFd);
                return false;
            }
            
            // 开始监听
            if (listen(serverFd, 5) < 0) {
                perror("listen");
                close(serverFd);
                return false;
            }

            runningMap[topic] = true;
            serverFdMap[topic] = serverFd;
            topics.push_back(topic);

            // 启动接受连接的线程
            MessageHandle::getInstance().threadPool->Add([=](const std::string& topic) {
                acceptConnections(topic);
            }, topic);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // 首次创建订阅监听，需要延时2s等所有订阅连上

            return true;
        }
        
        void stop() {
            for (auto topic : topics) {
                runningMap[topic] = false;
            
                // 关闭所有客户端连接
                auto clientFds = clientFdsMap[topic];
                for (int clientFd : clientFds) {
                    close(clientFd);
                }
                clientFdsMap.erase(topic);
                
                // 关闭服务器套接字
                int serverFd = serverFdMap[topic];
                if (serverFd >= 0) {
                    close(serverFd);
                }
                serverFdMap.erase(topic);
                
                // 移除socket文件
                unlink(topic.c_str());
            }
        }

        void sendMsg(const std::string& topic, const Message& message) {
            if (clientFdsMap[topic].empty()) {
                return;
            }

            /* TODO 暂时屏蔽
            string data = xpack::json::encode(message); // 结构体转JSON
            */
            string data = ""; // TODO 临时定义

            // 广播消息给所有客户端
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto it = clientFdsMap[topic].begin(); it != clientFdsMap[topic].end();) {
                int clientFd = *it;

                // 检查客户端连接是否仍然有效
                if (!isClientConnected(clientFd)) {
                    close(clientFd);
                    it = clientFdsMap[topic].erase(it);
                    continue;
                }

                // 检查socket输出缓冲区是否已有数据待发送
                int available;
                ioctl(clientFd, TIOCOUTQ, &available); // 获取输出队列中的字节数
                if (available > 0) {
                    ++it; // 移动到下一个客户端
                    continue; // 跳过当前客户端的发送
                }

                ssize_t bytesSent = send(clientFd, data.c_str(), data.length(), 0);
                
                if (bytesSent < 0) {
                    // 发送失败，移除客户端
                    perror("send");
                    close(clientFd);
                    it = clientFdsMap[topic].erase(it);
                } else {
                    ++it;
                }
            }
        }

        void acceptConnections(const std::string& topic) {
            bool running = runningMap[topic];
            int serverFd = serverFdMap[topic];

            while (running) {
                struct pollfd pfd;
                pfd.fd = serverFd;
                pfd.events = POLLIN;
                
                int ret = poll(&pfd, 1, 100); // 100ms超时
                
                if (ret < 0) {
                    perror("poll");
                    break;
                } else if (ret == 0) {
                    // 超时，继续循环
                    continue;
                }
                
                if (pfd.revents & POLLIN) {
                    // 接受新连接
                    int clientFd = accept(serverFd, NULL, NULL);
                    if (clientFd < 0) {
                        perror("accept");
                        continue;
                    }
                    
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    clientFdsMap[topic].push_back(clientFd);
                }
            }

            runningMap.erase(topic);
        }

        bool isClientConnected(int fd) {
            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLIN | POLLERR | POLLHUP;
            pfd.revents = 0;
            
            // 使用poll检查套接字状态，超时设为0（立即返回）
            int result = poll(&pfd, 1, 0);
            
            if (result < 0) {
                // poll错误，假设连接已断开
                return false;
            }
            
            // 检查是否有错误或挂起事件
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                return false;
            }
            
            return true;
        }
    
        bool createDirectoriesRecursive(const std::string& path) {
            // 从路径中提取目录部分（去掉文件名）
            std::filesystem::path dirPath = std::filesystem::path(path).parent_path();
            
            // 如果目录已经存在，直接返回成功
            if (std::filesystem::exists(dirPath)) {
                return true;
            }
            
            // 递归创建目录
            return std::filesystem::create_directories(dirPath);
        }
    private:
        std::mutex mutex;
        std::vector<Subscriber<Message>> subscribers;
        std::unordered_map<int, std::mutex> subscriberLockMap; // 每条订阅的回调函数加锁, 订阅回调函数执行完才能执行下一个回调
        std::unordered_map<int, bool> subscriberDoingMap; // 判断订阅是否正在执行

        /* 进程通信相关参数 */
        std::mutex clientsMutex;
        std::vector<std::string> topics;
        std::unordered_map<std::string,std::vector<int>> clientFdsMap;  // 一个话题对应多个客户端
        std::unordered_map<std::string, int> serverFdMap;               // 一个话题对应一个服务端
        std::unordered_map<std::string, bool> runningMap;               // 一个话题对应一个服务状态
    };

    template<typename Message, typename Response>
    class ServiceQueue {
    private:
        ServiceQueue()
        {
            std::srand(static_cast<unsigned int>(std::time(nullptr)));
        }
        ~ServiceQueue()
        {
            stop();
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
                // MessageHandle::getInstance().registerObject(trace); // 登录跟踪的类的指针
                responders.push_back({ id, topic, callback, trace });
                MessageHandle::getInstance().setTopicNum(id, topic);
            }
        }

        // 这个接口支持进程通信
        void subscribe(const std::string& topic, const MQTYPE& mqType, void* trace, const std::function<void(const Message&, Response&)>& callback) {
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
                // MessageHandle::getInstance().registerObject(trace); // 登录跟踪的类的指针
                responders.push_back({ id, topic, callback, trace });
                MessageHandle::getInstance().setTopicNum(id, topic);
            }

            if (MQTYPE::FORK == mqType) {
                // 如果启用进程通信，还要执行下面逻辑
                MessageHandle::getInstance().threadPool->Add([=](const std::string& topic, const std::function<void(const Message&, Response&)>& callback) {
                    int socketFd = -1;
                    bool connected = false;
                    std::string relTopicPath = MessageHandle::getInstance().topicPath + topic;

                    // 最外层循环确保掉线重连
                    while (true)
                    {
                        while (!connect(connected, relTopicPath, socketFd)) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // 连接失败，时延2s再尝试
                        }

                        while (connected) {
                            if (waitForMessage(connected, socketFd, 100)) {
                                receive(connected, socketFd, callback);
                            }
                        }
                    }
                }, topic, callback);
            }
        }

        // 加入超时判断, 由于服务队列要等应答,如果订阅有死循环阻塞,则发布方会一直阻塞
        Response publish(const std::string& topic, const Message& message) {
            Response response;
            std::function<void(const Message&, Response&)> toCall;

            {
                std::lock_guard<std::mutex> lock(mutex);

                for (const auto& responder : responders) {
                    // 服务队列是点对点的, 一条订阅对应一个发布
                    if (responder.topic == topic) {
                        toCall = responder.callback;
                        break;
                    }
                }
            }

            toCall(message, response);

            return response;
        }

        // 这个接口支持进程通信
        Response publish(const std::string& topic, const MQTYPE& mqType, const int& timeout, const Message& message) {
            Response response;
            
            if (MQTYPE::THREAD == mqType) {
                response = publish(topic, message);
            } else if (MQTYPE::FORK == mqType) {
                std::string relTopicPath = MessageHandle::getInstance().topicPath + topic;

                if(start(relTopicPath)) {
                    sendMsg(relTopicPath, timeout, message, response);
                }
            }

            return response;
        }

    private:
        /* 进程通信相关接口（发布） */
        bool connect(bool& connected, const std::string& topic, int& socketFd) {
            socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (socketFd < 0) {
                perror("socket");
                return false;
            }
            
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, topic.c_str(), sizeof(addr.sun_path) - 1);
            
            if (::connect(socketFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                perror("connect");
                close(socketFd);
                socketFd = -1;
                return false;
            }

            connected = true;

            return true;
        }

        void disconnect(bool& connected, int& socketFd) {
            if (socketFd >= 0) {
                close(socketFd);
                socketFd = -1;
            }
            connected = false;
        }

        bool waitForMessage(const bool& connected, const int& socketFd, int timeoutMs = -1) {
            if (!connected) {
                return false;
            }
            
            struct pollfd pfd;
            pfd.fd = socketFd;
            pfd.events = POLLIN;
            
            int ret = poll(&pfd, 1, timeoutMs);

            if (ret < 0) {
                perror("poll");
                return false;
            } else if (ret == 0) {
                // 超时
                return false;
            }
            
            return (pfd.revents & POLLIN) != 0;
        }

        bool receive(bool& connected, int& socketFd, const std::function<void(const Message&, Response&)>& callback) {
            if (!connected) {
                return false;
            }
            
            char buffer[4096];
            ssize_t bytesRead = recv(socketFd, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead <= 0) {
                if (bytesRead == 0) {
                } else {
                    perror("recv");
                }

                disconnect(connected, socketFd);

                return false;
            } else {
                buffer[bytesRead] = '\0';

                /* TODO 暂时屏蔽
                Message data;
                Response response;
                xpack::json::decode(buffer, data); // JSON转结构体
                callback(data, response);

                // 发送ACK确认
                string ack = xpack::json::encode(response);
                ssize_t bytesSent = send(socketFd, ack.c_str(), ack.length(), MSG_NOSIGNAL);
                if (bytesSent <= 0) {
                } else {
                }
                */
            }

            return true;
        }

        /* 进程通信相关接口（订阅） */
        bool start(const std::string& topic) {
            // 创建Unix域套接字
            int serverFd = -1;
            if (serverFdMap.find(topic) == serverFdMap.end()) {
                if (createDirectoriesRecursive(topic)) { // 目录创建成功
                    serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
                }
            } else {
                return true;
            }
            
            if (serverFd < 0) {
                perror("socket");
                return false;
            }
            
            // 设置非阻塞模式
            int flags = fcntl(serverFd, F_GETFL, 0);
            fcntl(serverFd, F_SETFL, flags | O_NONBLOCK);
            
            // 绑定到地址
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, topic.c_str(), sizeof(addr.sun_path) - 1);
            
            // 移除可能存在的旧socket文件
            unlink(topic.c_str());
            
            if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                perror("bind");
                close(serverFd);
                return false;
            }
            
            // 开始监听
            if (listen(serverFd, 5) < 0) {
                perror("listen");
                close(serverFd);
                return false;
            }

            runningMap[topic] = true;
            serverFdMap[topic] = serverFd;
            topics.push_back(topic);

            // 启动接受连接的线程
            MessageHandle::getInstance().threadPool->Add([=](const std::string& topic) {
                acceptConnections(topic);
            }, topic);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // 首次创建订阅监听，需要延时2s等所有订阅连上

            return true;
        }
        
        void stop() {
            for (auto topic : topics) {
                runningMap[topic] = false;
            
                // 关闭所有客户端连接
                auto clientFd = clientFdsMap[topic];
                if (clientFd >= 0) {
                    close(clientFd);
                }
                clientFdsMap.erase(topic);
                
                // 关闭服务器套接字
                int serverFd = serverFdMap[topic];
                if (serverFd >= 0) {
                    close(serverFd);
                }
                serverFdMap.erase(topic);
                
                // 移除socket文件
                unlink(topic.c_str());
            }
        }

        bool sendMsg(const std::string& topic, const int& timeout, const Message& message, Response& response) {
            if (clientFdsMap.find(topic) == clientFdsMap.end()) {
                return false;
            }

            int clientFd = clientFdsMap[topic];
            if (clientFd < 0) {
                return false;
            }

            /* TODO 暂时屏蔽
            string data = xpack::json::encode(message); // 结构体转JSON
            */
            string data = ""; // TODO 临时定义

            std::lock_guard<std::mutex> lock(clientsMutex);
            // 检查客户端连接是否仍然有效
            if (!isClientConnected(clientFd)) {
                close(clientFd);
                clientFdsMap.erase(topic);

                return false;
            }

            // 检查socket输出缓冲区是否已有数据待发送
            int available;
            ioctl(clientFd, TIOCOUTQ, &available); // 获取输出队列中的字节数
            if (available > 0) {
                return false;
            }

            ssize_t bytesSent = send(clientFd, data.c_str(), data.length(), 0);
            if (bytesSent < 0) {
                // 发送失败，移除客户端
                perror("send");
                close(clientFd);
                clientFdsMap.erase(topic);

                return false;
            } else {
                // 设置接收超时为5秒
                struct timeval tv;
                tv.tv_sec = (long)timeout;
                tv.tv_usec = 0;
                setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                char ack[4096];
                ssize_t bytesRecv = recv(clientFd, ack, sizeof(ack) - 1, 0);
                string tmpAck = ack;
                if (bytesRecv <= 0) {
                    if (bytesRecv == 0) {
                    } else {
                        perror("recv");
                    }
                } else {
                    ack[bytesRecv] = '\0';
                    /* TODO 暂时屏蔽
                    xpack::json::decode(ack, response); // JSON转结构体
                    */
                }

                // 恢复默认超时设置（阻塞模式）
                tv.tv_sec = 0;
                tv.tv_usec = 0;
                setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

                return true;
            }
        }

        void acceptConnections(const std::string& topic) {
            bool running = runningMap[topic];
            int serverFd = serverFdMap[topic];

            while (running) {
                struct pollfd pfd;
                pfd.fd = serverFd;
                pfd.events = POLLIN;
                
                int ret = poll(&pfd, 1, 100); // 100ms超时
                
                if (ret < 0) {
                    perror("poll");
                    break;
                } else if (ret == 0) {
                    // 超时，继续循环
                    continue;
                }
                
                if (pfd.revents & POLLIN) {
                    // 接受新连接
                    int clientFd = accept(serverFd, NULL, NULL);
                    if (clientFd < 0) {
                        perror("accept");
                        continue;
                    }
                    
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    clientFdsMap[topic] = clientFd;
                }
            }

            runningMap.erase(topic);
        }

        bool isClientConnected(int fd) {
            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLIN | POLLERR | POLLHUP;
            pfd.revents = 0;
            
            // 使用poll检查套接字状态，超时设为0（立即返回）
            int result = poll(&pfd, 1, 0);
            
            if (result < 0) {
                // poll错误，假设连接已断开
                return false;
            }
            
            // 检查是否有错误或挂起事件
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                return false;
            }
            
            return true;
        }
    
        bool createDirectoriesRecursive(const std::string& path) {
            // 从路径中提取目录部分（去掉文件名）
            std::filesystem::path dirPath = std::filesystem::path(path).parent_path();
            
            // 如果目录已经存在，直接返回成功
            if (std::filesystem::exists(dirPath)) {
                return true;
            }
            
            // 递归创建目录
            return std::filesystem::create_directories(dirPath);
        }

    private:
        std::vector<Responder<Message, Response>> responders;
        std::mutex mutex;

        /* 进程通信相关参数 */
        std::mutex clientsMutex;
        std::vector<std::string> topics;
        std::unordered_map<std::string, int> clientFdsMap;  // 一个话题对应多个客户端
        std::unordered_map<std::string, int> serverFdMap;   // 一个话题对应一个服务端
        std::unordered_map<std::string, bool> runningMap;   // 一个话题对应一个服务状态
    };
};
