#pragma once

#include <iostream>
#include <deque>
#include <unordered_map>

#define  CACHE_MAX_SIZE    1024

using namespace std;

namespace CommonLib {

template <typename Data>
class DequeCache : public std::deque<Data> {
public:
    DequeCache() : 
        maxSize(CACHE_MAX_SIZE) 
    {
    }
    DequeCache(const int& size) :
        maxSize(size)
    {
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

    // 每条订阅都有缓存
    void push_back(const int& subId, Data data) {
        std::lock_guard<std::mutex> lock(mutex);

        // 检查 cacheMap 中是否已经存在该 topic
        if (cacheMap.find(subId) != cacheMap.end()) {
            cacheMap[subId].push_back(data);
        } else {
            cacheMap[subId] = DequeCache<Data>(CACHE_MAX_SIZE);
            cacheMap[subId].push_back(data);
        }
    }

    // 拿完数据就删除这个数据
    Data front(const int& subId) {
        std::lock_guard<std::mutex> lock(mutex);
        
        Data data;
        if (cacheMap.find(subId) != cacheMap.end()) {
            if (!cacheMap[subId].empty()) {
                data = cacheMap[subId].front();
                cacheMap[subId].pop_front();
            }
        }
        return data;
    }

private:
    std::unordered_map< int, DequeCache<Data> > cacheMap;
    std::mutex mutex;
};

}