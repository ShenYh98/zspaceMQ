#pragma once

#include <mutex>
#include <vector>
#include <iostream>
#include <unordered_set>
#include <unordered_map>

using namespace std;

namespace ThreadMessageQueue {
    struct SubInfo
    {
        pid_t subTid;                               // 订阅的id
        std::string topic;                          // 订阅的话题
        int runState;                               // 回调执行状态 1开始 0结束
        double runTime;      // 回调执行时长
    };

    // 生命周期管理类
    class MessageHandle {
        template<typename Message>
        friend class MessageQueue;
        template<typename Message, typename Response>
        friend class ServiceQueue;
        friend class InitMessageQueue;
        friend class SubInfoMonitor;

    private:
        // 私有构造函数，防止外部直接创建实例
        MessageHandle()
        {}
        // 禁止拷贝构造函数和赋值操作符，确保单例
        MessageHandle(const MessageHandle&) = delete;
        MessageHandle& operator=(const MessageHandle&) = delete;
    
    protected:
        static MessageHandle& getInstance() {
            static MessageHandle instance;
            return instance;
        }

        void registerObject(void* obj);
    
        void unregisterObject(void* obj);
    
        bool isObjectAlive(void* obj);

        void setSubInfo(const int& subId, const SubInfo& subInfo);

        std::unordered_map<int, SubInfo> getSubInfo();

        void eraseSubInfo(const int& subId);    

    private:
        mutable std::mutex mutex;
        mutable std::mutex subInfoMtx;
        std::unordered_set<void*> aliveObjects;
        std::unordered_map<int, SubInfo> subInfoMap; 
    };
};

namespace ProcessMessageQueue {
    class MessageHandle
    {
        template<typename Message>
        friend class MessageQueue;
        template<typename Message, typename Response>
        friend class ServiceQueue;
        friend class InitMessageQueue;

    private:
        // 私有构造函数，防止外部直接创建实例
        MessageHandle() {}
        // 禁止拷贝构造函数和赋值操作符，确保单例
        MessageHandle(const MessageHandle&) = delete;
        MessageHandle& operator=(const MessageHandle&) = delete;

    protected:
        static MessageHandle& getInstance() {
            static MessageHandle instance;
            return instance;
        }

        void setVTopic(const std::vector<std::string>& v_recvTopic);

        void getVTopic(std::vector<std::string>& v_recvTopic);

    private:
        std::vector<std::string> v_recv_topic;
    };
};
