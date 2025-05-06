#include "MessageHandle.h"

using namespace ThreadMessageQueue;

void MessageHandle::registerObject(void* obj) {
    std::lock_guard<std::mutex> lock(mutex);
    if (aliveObjects.find(obj) == aliveObjects.end()) {
        aliveObjects.insert(obj);
    }
}

void MessageHandle::unregisterObject(void* obj) {
    std::lock_guard<std::mutex> lock(mutex);
    aliveObjects.erase(obj);
}

bool MessageHandle::isObjectAlive(void* obj) {
    std::lock_guard<std::mutex> lock(mutex);
    return aliveObjects.find(obj) != aliveObjects.end();
}

void MessageHandle::setSubInfo(const int& subId, const SubInfo& subInfo) {
    std::lock_guard<std::mutex> lock(subInfoMtx);
    subInfoMap[subId] = subInfo;
}

void MessageHandle::setTopicNum(const int& subId, const std::string& topic) {
    std::lock_guard<std::mutex> lock(subInfoMtx);

    SubInfo subInfo;
    subInfo.topic = topic;
    subInfo.topicNum = 0;
    subInfoMap[subId] = subInfo;
    int num = 0;
    // TODO 下面两个循环太影响性能了,待优化
    for (auto it : subInfoMap) {
        if (it.second.topic == topic) {
            num++;
        }
    }
    for (auto& it : subInfoMap) {
        if (it.second.topic == topic) {
            it.second.topicNum = num;
        }
    }
}

std::unordered_map<int, SubInfo> MessageHandle::getSubInfo() {
    return subInfoMap;
}

void MessageHandle::eraseSubInfo(const int& subId) {
    if (subInfoMap.find(subId) != subInfoMap.end()) {
        subInfoMap.erase(subId);
    }
}
