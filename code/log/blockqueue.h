/*
 * @Author: parluo 
 * @Date: 2021-03-16 21:33:54 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-21 16:50:26
 */

#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H
#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

/*
再写一个双端队列的类，主要用在多线程下的同步
实现线程安全的类队列
*/
template <typename T>
class BlockDeque{
private:
    std::deque<T> deq_;
    size_t capacity_; // 日志最大写入量，超过后此值的入队日志都要等待
    std:: mutex mtx_;
    bool isClose_;
    std::condition_variable condConsumer_;
    std:: condition_variable condProducer_;
public:
    explicit BlockDeque(size_t MaxCapacity=1000);
    ~BlockDeque();
    void clear();
    bool empty();
    bool full();
    void Close();
    size_t size();
    size_t capacity();
    T front();
    T back();
    void push_back(const T &item);
    void push_front(const T &item);
    bool pop(T &item);
    bool pop(T &item, int timeout);
    void flush();
};

template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity): capacity_(MaxCapcity){
    assert(MaxCapacity>0);
    isClose_ = false;
}

template<class T>
BlockDeque<T>::~BlockDeque(){
    Close();
}

template<class T>
void BlockDeque<T>::Close(){
    {
        std::lock_guard<std::mutex> locker(mtx_);
        deq_.clear();
        isClose_ = true;
    }
    condProducer_.notify_all();
    condConsumer_.notify_all();
}
template<class T>
void BlockDeque<T>::clear(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.clear();
}

// 日志刷新，更新提交
template<class T>
void BlockDeque<T>::flush(){
    condConsumer_.notify_one();
}

template<class T>
T BlockDeque<T>::front(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front(); // 但是这里若空队列，将报错
}

template<class T>
T BlockDeque<T>::back(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

template<class T>
size_t BlockDeque<T>::size(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

template<class T>
size_t BlockDeque<T>::capacity(){
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

template<class T>
void BlockDeque<T>::push_back(const T& item){
    std::lock_guard<std::mutex> locker(mtx_);
    while(deq_.size()>= capacity_){// 日志量多了生产者将在此等待阻塞
        condProducer_.wait(locker); 
    }
    deq_.push_back(item);
    condConsumer_.notify_one(); // 添加后，队列有东西，通知消费者
}

template<class T>
void BlockDeque<T>::push_front(const T&item){
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_size() >= capacity_){
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}
template<class T>
bool BlockDeque<T>::empty(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}


// 判断日志队列是否满了
template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size()>=capacity_;
}

// pop 引用传出，返回bool标记成功与否
template<class T>
bool BlockDeque<T>::pop(T &item){
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        condConsumer_.wait(locker);// 空了，消费者等待
        if(isClose_){
            return false;
        }
        item = deq_.front();
        deq_.pop_front();
        condProducer_.notify_one(); // 通知生产者可以继续加入队列
        return true;
    }
}
// 带超时的pop wait_for
template<class T>
bool BlockDeque<T>::pop(T &item, int timeout){
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout))
                == std::cv_status::timeout){
            return false;
        }
        if(isClose_){
            return false;
        }
    }
    item = deq_front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}
#endif