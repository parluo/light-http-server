/*
 * @Author: parluo 
 * @Date: 2021-03-14 19:57:42 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-21 14:55:32
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>

class ThreadPool{
private:
    struct Pool{
        std::mutex mtx;
        std::condition_variable cond;
        bool isClosed; // 线程是否关闭， true将不会进入wait状态
        std::queue<std::function<void()>> tasks; // 任务队列
    };
    std::shared_ptr<Pool> pool_;
public:

// 一初始化，就创建指定个线程，进入就绪状态，并从主线程分离开来。
// 各个线程轮流从任务池取任务，执行完就等待唤醒
    explicit ThreadPool(size_t threadCount = 8):pool_(std::make_shared<Pool>()){
        assert(threadCount>0);
        for(size_t i=0; i<threadCount; ++i){
            std::thread([pool=pool_]{ // 构造一个匿名函数， 赋值捕捉pool_
                std::unique_lock<std::mutex> locker(pool->mtx);
                while(true){ 
                    if(!pool->tasks.empty()){
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                    }
                    else if(pool->isClosed) break; // struct的成员，内置类型bool默认初始化false?
                    else pool->cond.wait(locker);
                }
            }).detach();
        }
    }
    ThreadPool()=default;
    ThreadPool(ThreadPool&&) = default;
    ~ThreadPool(){
        if(static_cast<bool>(pool_)){
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true; // 手动关闭标志符
            }
            pool_->cond.notify_all(); // 通知他们全部执行手上任务
        }
    }

    // 线程池队列加入一个任务并且通知一个线程
    // 没任务就自己通知自己执行
    template<class F>
    void AddTask(F && task){
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool->tasks.emplace(std::forward<F>(task)); // forward移动时保留左值右值的特性
        }
        pool_->cond.notify_one();
    }
};

#endif