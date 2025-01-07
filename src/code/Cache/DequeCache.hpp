#pragma once

#include <iostream>
#include <deque>
#include <unordered_map>

#define  CACHE_MAX_SIZE    16

using namespace std;

namespace CommonLib {

template <typename Data>
class DequeCache : public std::deque<Data> {
public:
    DequeCache() {
        maxSize = CACHE_MAX_SIZE;
    }
    DequeCache(const int& size) {
        maxSize = size;
    }
    ~DequeCache() {

    }

    // 重构push_back，加入队列最大限制
    void push_back(const Data& value) {
        if (this->size() >= maxSize) {
            // 如果队列已满，移除最旧的元素
            this->pop_front();
        }
        // 插入新元素
        std::deque<Data>::push_back(value);
    }

    // TODO 临时调试打印一些简单类型数据
    void printCache() const {
        for (const auto& elem : *this) {
            std::cout << elem << " ";
        }
        std::cout << std::endl;
    }

private:
    int maxSize; // 缓存的最大大小
    
};

template <typename Data>
class CacheStrategy
{
private:
    CacheStrategy() {}
    CacheStrategy(const CacheStrategy&) = delete;
    CacheStrategy& operator=(const CacheStrategy&) = delete;

public:
    static CacheStrategy<Data>& getInstance() {
        static CacheStrategy<Data> instance;
        return instance;
    }

    void push_back(const std::string& topic, const Data& value) {
        std::lock_guard<std::mutex> lock(mutex);

        // 检查 cacheMap 中是否已经存在该 topic
        if (cacheMap.find(topic) != cacheMap.end()) {
            cacheMap[topic].push_back(value);
        } else {
            cacheMap[topic] = DequeCache<Data>(CACHE_MAX_SIZE);
            cacheMap[topic].push_back(value);
        }
    }

    // 拿完数据就删除这个数据
    Data front(const std::string& topic) {
        std::lock_guard<std::mutex> lock(mutex);
        
        Data data;
        if (cacheMap.find(topic) != cacheMap.end()) {
            if (!cacheMap[topic].empty()) {
                data = cacheMap[topic].front();
                cacheMap[topic].pop_front();
            }
        }
        return data;
    }

    void printCache(const std::string& topic) {
        cacheMap[topic].printCache();
    }

private:
    std::unordered_map< std::string, DequeCache<Data> > cacheMap;
    std::mutex mutex;
};

}