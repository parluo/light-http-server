/*
 * @Author: parluo 
 * @Date: 2021-03-13 11:12:40 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-23 17:38:55
 * 
 * 定时器容器设计： 
 */

#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map> 
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack; // C++中的函数对象类，包装函数
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp; // 结构体，带了一个2参类型的私有变量_d

struct TimerNode{
    int id;
    TimeStamp expires; // 超时时间
    TimeoutCallBack cb;
    bool operator<(const TimerNode& t){
        return expires < t .expires;
    }
    bool operator<=(const TimerNode& t){
        return expires <= t.expires;
    }
};

// 一个定时器类 堆实现， 节点上存的是 连接的描述符、超时时间、以及回调函数
class HeapTimer{
private:
    void del_(size_t i);
    void siftup_(size_t i);
    bool siftdown_(size_t index, size_t n);
    void swapnode_(size_t i, size_t j);
    std::vector<TimerNode> heap_; // 堆容器 存放id，事件，回调函数
    std::unordered_map<int, size_t> ref_; // 索引id描述符在heap中的index

public:
    HeapTimer(){heap_.reserve(64);} // 64的空间
    ~HeapTimer(){clear();}

    void adjust(int id, int newExpires);
    void add(int id, int timeOut, const TimeoutCallBack& cb);
    void doWork(int id);
    void clear();
    void tick();
    void pop();
    int GetNextTick();
};
#endif // HEAP_TIMER_H
