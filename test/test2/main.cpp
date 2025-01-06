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

        ServiceQueue<s_Data, std::string>::getInstance().subscribe("TestSub", this,
            [&](const s_Data &msg, std::string &responder){
                std::thread::id this_id = std::this_thread::get_id();
                std::cout << "[TestSubA] " << this_id << " " << msg.valueInt << " " << msg.valueDouble << std::endl;
                sleep(2);
                responder = "ok";
            });

        // 相同话题，不同数据类型
        ServiceQueue<int, std::string>::getInstance().subscribe("TestSub", this,
            [&](const int &msg, std::string &responder){
                std::thread::id this_id = std::this_thread::get_id();
                std::cout << "[TestSubA1] " << this_id << " " << msg << std::endl;
                sleep(2);
                responder = "ok";
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

        // 和TestSubA相同话题
        ServiceQueue<s_Data, std::string>::getInstance().subscribe("TestSub", this,
            [&](const s_Data &msg, std::string &responder){
                std::thread::id this_id = std::this_thread::get_id();
                std::cout << "[TestSubB] " << this_id << " " << msg.valueInt << " " << msg.valueDouble << std::endl;
                sleep(2);
                responder = "ok";
            });

        // 和TestSubA相同话题
        ServiceQueue<int, std::string>::getInstance().subscribe("TestSub", this,
            [&](const int &msg, std::string &responder){
                std::thread::id this_id = std::this_thread::get_id();
                std::cout << "[TestSubB1] " << this_id << " " << msg << std::endl;
                sleep(2);
                responder = "ok";
            });

        // 和TestSub相同数据类型，不同话题
        ServiceQueue<s_Data, std::string>::getInstance().subscribe("TestSubB1", this,
            [&](const s_Data &msg, std::string &responder){
                std::thread::id this_id = std::this_thread::get_id();
                std::cout << "[TestSubB1] " << this_id << " " << msg.valueInt << " " << msg.valueDouble << std::endl;
                sleep(2);
                responder = "ok";
            });

        // 进入死循环，publish超时会过滤这条订阅
        ServiceQueue<s_Data, std::string>::getInstance().subscribe("TestSubB2", this,
            [&](const s_Data &msg, std::string &responder){
                while(true) {
                    sleep(10);
                }
            });
    }

    ~TestSubB() {
        std::cout << "[TestSubB] destroy." << std::endl;
        InitMessageQueue::getInstance().destroySubscribe(this);
    }
};

/*
 * 1. 服务队列点对点收发数据                                        ✔
 * 2. 同话题不同类型的服务队列，不会过滤                             ✔
 * 3. 同话题同类型的服务队列，自动过滤多余的                         ✔
 * 4. 不同话题同类型数据收发                                        ✔
 * 5. 超时未回复，一定时间自动解除阻塞，并且不再给这个话题发数据       ✔
 * 6. 一段时间后销毁队列，测试订阅是否从队列移除                      ✔
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
        if (num <=5) { // 执行5次后销毁testSubA对象
            auto res1 = ServiceQueue<s_Data, std::string>::getInstance().publish("TestSub", sdata); // TestSubA会收到消息
            if (res1 != "") {
                std::cout << "recv1 res:" << res1 << std::endl;
            }

            auto res2 = ServiceQueue<s_Data, std::string>::getInstance().publish("TestSubB1", sdata); // TestSubB会收到消息
            if (res2 != "") {
                std::cout << "recv2 res:" << res2 << std::endl;
            }

            auto res3 = ServiceQueue<int, std::string>::getInstance().publish("TestSub", 1); // TestSubA会收到消息
            if (res3 != "") {
                std::cout << "recv3 res:" << res3 << std::endl;
            }

            auto res4 = ServiceQueue<s_Data, std::string>::getInstance().publish("TestSubB2", 10, sdata); // TestSubB会收到消息，超时未回跳过订阅
            if (res4 != "") {
                std::cout << "recv4 res:" << res4 << std::endl;
            } else {
                std::cout << "recv4 timeout" << std::endl;
            }

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
