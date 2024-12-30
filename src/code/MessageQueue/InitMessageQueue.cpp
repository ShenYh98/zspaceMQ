#include "InitMessageQueue.h"

using namespace CommonLib;

void InitMessageQueue::initSubscribeInfo() {
    fork_lk = open(FORK_LOCK_FILE, O_CREAT | O_RDWR, 0666);
    if (fork_lk == -1) {
        perror("open");
        return ;
    }

    if (lock_file(fork_lk) == -1) {
        close(fork_lk);
        return ;
    }

    int result = system("rm -rf ./tmp/*");   // 删除管道的描述文件
    result = system("rm -rf /dev/mqueue/*"); // 删除消息队列的描述文件

    threadPool->Add(std::bind(&InitMessageQueue::pipeRead, this, std::placeholders::_1, std::placeholders::_2), init_sub_path, SubInfo::INIT);
    threadPool->Add(std::bind(&InitMessageQueue::pipeRead, this, std::placeholders::_1, std::placeholders::_2), recv_wait_path, SubInfo::RECV);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void InitMessageQueue::waitInit(const std::string node) {
    std::string path = "./tmp/" + node;
    pipeWrite(recv_wait_path, path);
    pipeRead(path, SubInfo::WAIT);
    
    messageHandle.setVTopic(this->v_recv_topic);

    delete(threadPool);
}

// 创建进程的文件锁
int InitMessageQueue::lock_file(int fd) {
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
int InitMessageQueue::unlock_file(int fd) {
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

void InitMessageQueue::pipeRead(const std::string& fifo_path, const SubInfo& e_SubInfo) {
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

int InitMessageQueue::pipeWrite(const std::string& fifo_path, const std::string& message) { 
    int fifo_fd = open(fifo_path.c_str(), O_WRONLY);
    if (fifo_fd == -1) {
        perror("open");
        return 1;
    }

    write(fifo_fd, message.c_str(), message.size());

    close(fifo_fd);

    return 0;
}

void InitMessageQueue::checkSubscribeInfo(int const& fifo_fd) {
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
    std::string combinationTopic;

    for (auto it = v_topic.begin(); it != v_topic.end(); it++) {
        if ( it != (v_topic.end()-1) ) {
            combinationTopic += *it + " ";
        } else {
            combinationTopic += *it + "\0";
        }
    }

    for (auto it : v_wait_fifo_path) {
        pipeWrite(it, combinationTopic);
    }
}

void InitMessageQueue::waitSubscribeInfo(int const& fifo_fd) {
    while (true) {
        char buf[WAIT_SUB_BUF_SIZE];
        ssize_t num_bytes = read(fifo_fd, buf, sizeof(buf));
        if (num_bytes > 0) {
            buf[num_bytes] = '\0';
            std::string str(buf, num_bytes);

            v_recv_topic = parseString(str.c_str());

            break;
        }
    }
}

void InitMessageQueue::recvSendTopicPath(int const& fifo_fd) {
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

std::vector<std::string> InitMessageQueue::parseString(const char* str) {
    std::vector<std::string> result;
    const char* start = str;
    const char* current = str;

    while (*current != '\0') {
        if (*current == ' ') {
            // 遇到空格，分割出一个子字符串
            if (start != current) { // 确保有内容才添加
                result.push_back(std::string(start, current));
            }
            start = current + 1; // 更新下一个子字符串的起始位置
        }
        current++;
    }

    // 处理最后一个子字符串（如果有的话）
    if (start != current) { // current 现在指向 '\0'，所以这里检查 start 是否与 current-1 不同
        result.push_back(std::string(start, current)); // 添加最后一个子字符串
    }

    return result;
}
