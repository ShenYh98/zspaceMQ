#include <iostream>  

#include "protoMsg/message.pb.h"

#include "MessageQueue.hpp"
#include "InitMessageQueueP.h"

using namespace std;
using namespace CommonLib;
using namespace ProcessMessageQueue;

class TestSubA
{
public:
    TestSubA() {
        std::cout << "[TestSubA] init test sub" << std::endl;

        MessageQueue<Data>::getInstance().subscribe("TestSub", 
            [&](const Data &msg){
                std::cout << "[TestSubA] 1" << msg.id() << "/" << msg.flag() << "/" << msg.num() << std::endl;
            });

        MessageQueue<Data>::getInstance().subscribe("TestSub", 
            [&](const Data &msg){
                std::cout << "[TestSubA] 2" << msg.id() << "/" << msg.flag() << "/" << msg.num() << std::endl;
            });
    }

    ~TestSubA() {
        std::cout << "[TestSubA] destroy." << std::endl;
    }
};

/*
 * 1. pub进程通过消息队列广播发送数据, sub进程是否收到数据 ✔
 * 2. pub进程通过消息队列广播发送数据, pub进程是否收到数据 ✔
 */
int main() {
    ProcessMessageQueue::InitMessageQueue::getInstance().initSubscribeInfo();
    TestSubA *testSubA = new TestSubA();
    ProcessMessageQueue::InitMessageQueue::getInstance().waitInit("node2");

    while (true)
    {
    }

    return 0;
}
