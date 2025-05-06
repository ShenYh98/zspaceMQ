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

        ServiceQueue<Data, Data>::getInstance().subscribe("TestSub", 
            [&](const Data &msg, Data &response){
                std::cout << "[TestSubA] 1" << msg.id() << "/" << msg.flag() << "/" << msg.num() << std::endl;
                response.set_message("ok");
                sleep(3);
            });
    }

    ~TestSubA() {
        std::cout << "[TestSubA] destroy." << std::endl;
    }
};

/*
 * 1. pub进程通过服务队列点对点发送数据, sub进程是否收到数据, 并回复
 */
int main() {
    // ProcessMessageQueue::InitMessageQueue::getInstance().initSubscribeInfo();
    TestSubA *testSubA = new TestSubA();
    // ProcessMessageQueue::InitMessageQueue::getInstance().waitInit("node2");

    while (true)
    {
    }

    return 0;
}
