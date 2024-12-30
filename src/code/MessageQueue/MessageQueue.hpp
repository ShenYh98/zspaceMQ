#pragma once

#include <iostream>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <fstream>
#include <cstring>
#include <sstream>

#include "ThreadPool.h"

/* posix通信相关 */
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>

#define  MQ_THREAD_MAX     16
#define  MQ_THREAD_MIN     8

#define  MQ2_THREAD_MAX    16
#define  MQ2_THREAD_MIN    8

#define  SQ_THREAD_MAX     16
#define  SQ_THREAD_MIN     8

#define  SQ2_THREAD_MAX    16
#define  SQ2_THREAD_MIN    8

#define  INIT_SUB_PATH     "./tmp/init_sub_fifo"

namespace CommonLib {

/* 第一版线程间通信的消息队列(单线程通信,无法进程间通信) */
template<typename Message>
struct Subscriber {
    std::string topic;
    std::function<void(const Message&)> callback;
};

template<typename Message, typename Response>
struct Responder {
    std::string topic;
    std::function<void(const Message&, std::function<void(const Response&)>)> callback;
};

template<typename Message>
class MessageQueue {
public:
    MessageQueue() : threadPool(MQ_THREAD_MIN, MQ_THREAD_MAX) {}
    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;

    static MessageQueue<Message>& getInstance() {
        static MessageQueue<Message> instance;
        return instance;
    }

    // 订阅在初始化处完成
    void subscribe(const std::string& topic, const std::function<void(const Message&)>& callback) {
        std::lock_guard<std::mutex> lock(mutex);
        subscribers.push_back({ topic, callback });
    }

    void publish(const std::string& topic, const Message& message) {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& subscriber : subscribers) {
            if (subscriber.topic == topic) {
                threadPool.Add(subscriber.callback, message);
            }
        }
    }

private:
    std::thread mq_thread;
public:
    std::vector<Subscriber<Message>> subscribers;
    ThreadPool threadPool;
    std::mutex mutex;
};

template<typename Message, typename Response>
class ServiceQueue {
public:
    ServiceQueue() : threadPool(SQ_THREAD_MIN, SQ_THREAD_MAX) {}
    ServiceQueue(const ServiceQueue&) = delete;
    ServiceQueue& operator=(const ServiceQueue&) = delete;

    static ServiceQueue<Message, Response>& getInstance() {
        static ServiceQueue<Message, Response> instance;
        return instance;
    }
    
    void subscribe(const std::string& topic, const std::function<void(const Message&, std::function<void(const Response&)>)>& callback) {
        std::lock_guard<std::mutex> lock(mutex);
        responders.push_back({ topic, callback });
    }

    std::future<Response> publish(const std::string& topic, const Message& message)  {
        auto prom = std::make_shared<std::promise<Response>>();
        auto future = prom->get_future();

        std::function<void(const Message&, std::function<void(const Response&)>)> toCall;

        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& responder : responders) {
                if (responder.topic == topic) {
                    toCall = responder.callback;
                    break;  // Assuming only one responder per topic for simplicity
                }
            }
        }

        if (toCall) {
            threadPool.Add([&, callback=toCall, prom](const Message& msg) {
                        callback(msg, [prom](const Response& resp) {
                            prom->set_value(resp);
                        });
                    }, message);
        }

        return future;
    }

private:
    std::vector<Responder<Message, Response>> responders;
    ThreadPool threadPool;
    std::mutex mutex;
};

/* 第二版线程及进程双通信的消息队列(线程进程间都可以通信) */
class MessageHandle
{
private:
    // 私有构造函数，防止外部直接创建实例
    MessageHandle() {}
    // 禁止拷贝构造函数和赋值操作符，确保单例
    MessageHandle(const MessageHandle&) = delete;
    MessageHandle& operator=(const MessageHandle&) = delete;
 
private:
    static MessageHandle instance_;
    std::vector<std::string> v_recv_topic;
 
public:
    static MessageHandle& getInstance() {
        return instance_;
    }
 
    void setVTopic(const std::vector<std::string>& v_recvTopic) {
        this->v_recv_topic = v_recvTopic;
    }

    void getVTopic(std::vector<std::string>& v_recvTopic) {
        v_recvTopic = this->v_recv_topic;
    }
};

MessageHandle MessageHandle::instance_;
MessageHandle& messageHandle = MessageHandle::getInstance();

template<typename Message>
class MessageQueue2 {
private:
    MessageQueue2() : 
        threadPool(MQ2_THREAD_MIN, MQ2_THREAD_MAX) , 
        init_sub_path(INIT_SUB_PATH)
        {
        }
    MessageQueue2(const MessageQueue2&) = delete;
    MessageQueue2& operator=(const MessageQueue2&) = delete;

public:
    static MessageQueue2<Message>& getInstance() {
        static MessageQueue2<Message> instance;
        return instance;
    }

    // 订阅在初始化处完成
    void subscribe(const std::string& topic, const std::function<void(const Message&)>& callback) {
        threadPool.Add(std::bind(&MessageQueue2::server, this, std::placeholders::_1, std::placeholders::_2), topic, callback);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 发送在初始化处完成
    void publish(const std::string& topic, Message& data) {
        messageHandle.getVTopic(this->v_recv_topic);

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
class ServiceQueue2 {
private:
    ServiceQueue2() : 
        threadPool(SQ2_THREAD_MIN, SQ2_THREAD_MAX)
    {}
    ServiceQueue2(const ServiceQueue2&) = delete;
    ServiceQueue2& operator=(const ServiceQueue2&) = delete;

public:
    static ServiceQueue2<Message, Response>& getInstance() {
        static ServiceQueue2<Message, Response> instance;
        return instance;
    }

    // 订阅在初始化处完成
    void subscribe(const std::string& topic, const std::function<void(const Message&, Response&)>& callback) {
        threadPool.Add(std::bind(&ServiceQueue2::server, this, std::placeholders::_1, std::placeholders::_2), topic, callback);
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

}
