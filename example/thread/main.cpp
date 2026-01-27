#include <iostream>
#include <thread>
#include <chrono>
#include <string>

#include "code/MessageQueue/MessageQueue.hpp"

using namespace std;
using namespace ThreadMessageQueue;

class SubTestA
{
public:
    SubTestA() {
        MessageQueue<std::string>::getInstance().subscribe("demo", this, 
        [&](const std::string &msg) {
            std::cout << "[Subscriber] A thread " << std::this_thread::get_id() << " get: " << msg << std::endl;
        });
    }
    ~SubTestA() {}
};

class SubTestB
{
public:
    SubTestB() {
        MessageQueue<std::string>::getInstance().subscribe("demo", this, 
        [&](const std::string &msg) {
            std::cout << "[Subscriber] B thread " << std::this_thread::get_id() << " get: " << msg << std::endl;
        });
    }
    ~SubTestB() {}
};

class SubTestC
{
public:
    SubTestC() {
        MessageQueue<std::string>::getInstance().subscribe("demo", this, 
        [&](const std::string &msg) {
            std::cout << "[Subscriber] C thread " << std::this_thread::get_id() << " get: " << msg << std::endl;
        });
    }
    ~SubTestC() {}
};

int main() {
    // 订阅者：打印接收到的消息，并显示处理线程 id
    SubTestA *subTestA = new SubTestA();
    SubTestB *subTestB = new SubTestB();
    SubTestC *subTestC = new SubTestC();

    // 发布者：每 300ms 发布一条消息，共 30 条
    std::thread publisher([&]() {
        for (int i = 1; i <= 30; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));

            std::string m = "message-" + std::to_string(i);
            MessageQueue<std::string>::getInstance().publish("demo", 10, m);
        }
    });

    publisher.join();
    delete subTestA;
    delete subTestB;
    delete subTestC;

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    return 0;
}