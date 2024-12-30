#pragma once

#include <iostream>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <fstream>
#include <cstring>
#include <sstream>

#include "common/ThreadPool.h"

/* posix通信相关 */
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>

#define  MQ_THREAD_MAX     5
#define  MQ_THREAD_MIN     1

#define  MQ2_THREAD_MAX    128
#define  MQ2_THREAD_MIN    64

#define  SQ_THREAD_MAX     64
#define  SQ_THREAD_MIN     60

#define  MAX_TIME          5

#define  INIT_SUB_PATH     "./tmp/init_sub_fifo"
#define  RECV_WAIT_PATH    "./tmp/recv_wait_fifo"
#define  FORK_LOCK_FILE    "example.lock"

#define  WAIT_SUB_BUF_SIZE           2048
#define  CHECK_SUB_BUF_SIZE          2048
#define  RECV_SEND_TOPIC_BUF_SIZE    2048

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
typedef enum {
    INIT,
    WAIT,
    RECV
} SubInfo;

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
public:
    MessageQueue2() : 
        threadPool(MQ2_THREAD_MIN, MQ2_THREAD_MAX) , 
        init_sub_path(INIT_SUB_PATH) ,
        recv_wait_path(RECV_WAIT_PATH)
        {
        }
    MessageQueue2(const MessageQueue2&) = delete;
    MessageQueue2& operator=(const MessageQueue2&) = delete;

    static MessageQueue2<Message>& getInstance() {
        static MessageQueue2<Message> instance;
        return instance;
    }

    void initSubscribeInfo() {
        fork_lk = open(FORK_LOCK_FILE, O_CREAT | O_RDWR, 0666);
        if (fork_lk == -1) {
            perror("open");
            return ;
        }
    
        if (lock_file(fork_lk) == -1) {
            close(fork_lk);
            return ;
        }

        int result = system("rm -rf ./tmp/*");
        result = system("rm -rf /dev/mqueue/*");

        threadPool.Add(std::bind(&MessageQueue2::pipeRead, this, std::placeholders::_1, std::placeholders::_2), init_sub_path, SubInfo::INIT);
        threadPool.Add(std::bind(&MessageQueue2::pipeRead, this, std::placeholders::_1, std::placeholders::_2), recv_wait_path, SubInfo::RECV);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void waitInit(const std::string node) {
        std::string path = "./tmp/" + node;
        pipeWrite(recv_wait_path, path);
        pipeRead(path, SubInfo::WAIT);
        messageHandle.setVTopic(this->v_recv_topic);
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
    // 创建进程的文件锁
    int lock_file(int fd) {
        struct flock fl;
        fl.l_type = F_WRLCK;  // 排他锁
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0; // 0表示锁住整个文件
    
        if (fcntl(fd, F_SETLK, &fl) == -1) {
            perror("fcntl");
            return -1;
        }
        return 0;
    }
    
    // 创建进程的文件解锁
    int unlock_file(int fd) {
        struct flock fl;
        fl.l_type = F_UNLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0;
    
        if (fcntl(fd, F_SETLK, &fl) == -1) {
            perror("fcntl");
            return -1;
        }
        return 0;
    }

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

    void pipeRead(const std::string& fifo_path, const SubInfo& e_SubInfo) {
        int fifo_fd = mkfifo(fifo_path.c_str(), 0666);
        if (fifo_fd == -1) {
            perror("mkfifo");
            // return ;
        }

        fifo_fd = open(fifo_path.c_str(), O_RDONLY);
        if (fifo_fd == -1) {
            perror("open");
            return ;
        }

        if (e_SubInfo == SubInfo::INIT) {
            checkSubscribeInfo(fifo_fd);

            // 初始化结束释放进程锁
            if (unlock_file(fork_lk) == -1) {
                close(fork_lk);
                // exit(EXIT_FAILURE);
            }
            close(fork_lk);
        } else if (e_SubInfo == SubInfo::WAIT) {
            waitSubscribeInfo(fifo_fd);
        } else if (e_SubInfo == SubInfo::RECV) {
            recvSendTopicPath(fifo_fd);
        }

        close(fifo_fd);
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

    void checkSubscribeInfo(int const& fifo_fd) {
        // 在一定时间内,等待接收所有消息队列话题
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
    
            if (elapsed_seconds >= 10) {
                break;
            }

            char buf[CHECK_SUB_BUF_SIZE];
            ssize_t num_bytes = read(fifo_fd, buf, sizeof(buf) - 1);
            if (num_bytes > 0) {
                buf[num_bytes] = '\0';
                std::string str(buf, num_bytes);
                v_topic.push_back(str);

                startTime = ts.tv_sec;
            }
        }

        // 将收到的所有话题全部发出
        Message vtopic;
        for (auto it : v_topic) {
            vtopic.add_v_topic(it);
        }
        std::string serialized_data;
        vtopic.SerializeToString(&serialized_data);

        for (auto it : v_wait_fifo_path) {
            pipeWrite(it, serialized_data);
        }
    }

    void waitSubscribeInfo(int const& fifo_fd) {
        while (true) {
            char buf[WAIT_SUB_BUF_SIZE];
            ssize_t num_bytes = read(fifo_fd, buf, sizeof(buf));
            if (num_bytes > 0) {
                buf[num_bytes] = '\0';
                std::string str(buf, num_bytes);

                Message vtopic;
                if (!vtopic.ParseFromString(str)) {
                    std::cerr << "Failed to parse received message" << std::endl;
                    close(fifo_fd);
                    return;
                }

                for (int i = 0; i < vtopic.v_topic_size(); ++i) {
                    v_recv_topic.push_back(vtopic.v_topic(i));
                }

                break;
            }
        }
    }

    void recvSendTopicPath(int const& fifo_fd) {
        // 在一定时间内,等待接收所有wait管道的地址
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
    
            if (elapsed_seconds >= 10) {
                break;
            }

            char buf[RECV_SEND_TOPIC_BUF_SIZE];
            ssize_t num_bytes = read(fifo_fd, buf, sizeof(buf) - 1);
            if (num_bytes > 0) {
                buf[num_bytes] = '\0';
                std::string str(buf, num_bytes);
                v_wait_fifo_path.push_back(str);

                startTime = ts.tv_sec;
            }
        }
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
    std::vector<std::string> v_topic;           // 消息队列从订阅那儿接收的话题组
    std::vector<std::string> v_recv_topic;      // 消息队列接收的话题组给发布使用
    std::vector<std::string> v_wait_fifo_path;  // 进程等待管道组
    std::mutex mutex;
    std::string init_sub_path;   // 订阅初始化的管道路径
    std::string recv_wait_path;  // 接收各个进程等待函数的管道路径

    int fork_lk;
};

template<typename Message, typename Response>
class ServiceQueue2 {
public:
    ServiceQueue2() : 
        threadPool(MQ2_THREAD_MIN, MQ2_THREAD_MAX)
    {}
    ServiceQueue2(const ServiceQueue2&) = delete;
    ServiceQueue2& operator=(const ServiceQueue2&) = delete;

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
            mqd_t request_mq = mq_open(request_queue_name.c_str(), O_CREAT | O_RDONLY, 0666, &attr);
            if (request_mq == -1) {
                std::cerr << "3 Failed to open/create request message queue" << std::endl;
                return;
            }
    
            // 打开/创建响应队列
            mqd_t response_mq = mq_open(response_queue_name.c_str(), O_CREAT | O_WRONLY, 0666, &attr);
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
