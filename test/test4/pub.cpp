#include <iostream>  

#include "protoMsg/message.pb.h"

#include "MessageQueue.hpp"
#include "InitMessageQueueP.h"

using namespace std;
using namespace CommonLib;
using namespace ProcessMessageQueue;

/*
 * 1. pub进程通过服务队列点对点发送数据, sub进程是否收到数据, 并回复
 */
int main() {
    // ProcessMessageQueue::InitMessageQueue::getInstance().initSubscribeInfo();
    // ProcessMessageQueue::InitMessageQueue::getInstance().waitInit("node1");

    while (true)
    {
        Data data;
        data.set_id(1);
        data.set_flag(2);
        data.set_num(3);
        auto res = ServiceQueue<Data, Data>::getInstance().publish("TestSub", data); // TestSubA会收到消息
        std::cout << "response value:" << res.message() << std::endl;
    }

    return 0;
}
