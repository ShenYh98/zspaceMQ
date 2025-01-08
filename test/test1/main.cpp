#include <iostream>  

#include "MessageQueue.hpp"
#include "initThreadMessageQueue.h"

using namespace std;
using namespace CommonLib;
using namespace ThreadMessageQueue;

typedef struct
{
    /* data */
    int valueInt;
    double valueDouble;
} s_Data;

class TestSubA
{
public:
    TestSubA() {
        std::cout << "[TestSubA] init test sub" << std::endl;

        MessageQueue<s_Data>::getInstance().subscribe("TestSub", this,
            [&](const s_Data &msg){
                std::thread::id this_id = std::this_thread::get_id();
                std::cout << "[TestSubA] " << this_id << " " << msg.valueInt << " " << msg.valueDouble << std::endl;
            });

        MessageQueue<int>::getInstance().subscribe("TestSubA1", this,
            [&](const int &msg){
                std::thread::id this_id = std::this_thread::get_id();
                std::cout << "[TestSubA1] " << this_id << " " << msg << std::endl;
            });
    }

    ~TestSubA() {
        std::cout << "[TestSubA] destroy." << std::endl;
        InitMessageQueue::getInstance().destroySubscribe(this); // 如果不调用这个接口，这个对象被析构了，还是会收到消息
    }
};

class TestSubB
{
public:
    TestSubB() {
        std::cout << "[TestSubB] init test sub" << std::endl;

        MessageQueue<s_Data>::getInstance().subscribe("TestSub", this,
            [&](const s_Data &msg){
                std::thread::id this_id = std::this_thread::get_id();
                std::cout << "[TestSubB] " << this_id << " " << msg.valueInt << " " << msg.valueDouble << std::endl;
            });
    }

    ~TestSubB() {
        std::cout << "[TestSubB] destroy." << std::endl;
        InitMessageQueue::getInstance().destroySubscribe(this);
    }
};

/*
 * 1. 消息队列广播收发数据
 * 2. 不同类型消息队列收发数据
 * 3. 一段时间后销毁队列，测试订阅是否从队列移除
 */
int main() {
    TestSubA *testSubA = new TestSubA();
    TestSubB *testSubB = new TestSubB();

    int num = 1;
    while (true)
    {
        s_Data sdata;
        sdata.valueInt = 1;
        sdata.valueDouble = 1.0;
        if (num <=5) {
            MessageQueue<s_Data>::getInstance().publish("TestSub", 10, sdata); // 广播发送，TestSubA和TestSubB都会收到消息
            sleep(3);
            MessageQueue<int>::getInstance().publish("TestSubA1", 10, 1); // TestSubA的TestSubA1会收到消息
            sleep(3);

            if (num > 0) {
                num++;
            }
        } else { // 运行5次后销毁对象
            num = 0;
            delete(testSubA);
        }
    }

    return 0;
}
