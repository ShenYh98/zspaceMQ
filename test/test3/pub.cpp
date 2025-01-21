#include <iostream>  

#include "protoMsg/message.pb.h"

#include "MessageQueue.hpp"
#include "InitMessageQueueP.h"

using namespace std;
using namespace CommonLib;
using namespace ProcessMessageQueue;

class TestSubB
{
public:
    TestSubB() {
        std::cout << "[TestSubB] init test sub" << std::endl;

        // MessageQueue<Data>::getInstance().subscribe("TestSub", 
        //     [&](const Data &msg){
        //         std::cout << "[TestSubB] 1" << std::endl;
        //     });

        // MessageQueue<Data>::getInstance().subscribe("TestSub", 
        //     [&](const Data &msg){
        //         std::cout << "[TestSubB] 2" << std::endl;
        //     });
    }

    ~TestSubB() {
        std::cout << "[TestSubB] destroy." << std::endl;
    }
};

/*
 * 1. pub进程通过消息队列广播发送数据, sub进程是否收到数据 ✔
 * 2. pub进程通过消息队列广播发送数据, pub进程是否收到数据 ✔
 */
int main() {
    ProcessMessageQueue::InitMessageQueue::getInstance().initSubscribeInfo();
    TestSubB *testSubB = new TestSubB();
    ProcessMessageQueue::InitMessageQueue::getInstance().waitInit("node1");

    while (true)
    {
        // Data data;
        // data.set_id(1);
        // data.set_flag(2);
        // data.set_num(3);

        MessageQueue<bool>::getInstance().publish("TestSub", true); // TestSubA会收到消息
        sleep(3);
        MessageQueue<bool>::getInstance().publish("TestSub", false); // TestSubA会收到消息
        sleep(3);
    }

    return 0;
}
