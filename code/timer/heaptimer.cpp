/*
 * @Author: parluo 
 * @Date: 2021-03-14 15:14:48 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-21 14:37:05
 */

#include "./heaptimer.h"

void HeapTimer::swapnode_(size_t i, size_t j){
    assert(i>=0 && i<heap_.size());
    assert(j>=0 && j<heap_.size());
    std::swap(heap_[i], heap_[j]);  // 交换堆节点数据
    ref_[heap_[i].id] = i;          // 调整索引index
    ref_[heap_[j].id] = j;
}
void HeapTimer::siftup_(size_t i){
    assert(i>=0 && i<heap_.size());
    size_t j = (i-1)/2; // 堆父节点(i-1)/2
    while(j>=0){
        if(heap_[j]<=heap_[i]){break;} // 直到比父节点大 !要是<=才行，如果只是<,在j=i=0时，将死循环
        swapnode_(i,j);
        i = j;
        j = (i-1)/2; // i = 0时！
    }
}
bool HeapTimer::siftdown_(size_t index, size_t n){
    assert(index >= 0 && index < heap_.size());
    assert(n>=0 && n<=heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j<n){
        if(j+1<n && heap_[j+1]<heap_[j]) j++; // 两个子节点，往大/右边走
        if(heap_[i]<heap_[j]) break;
        swapnode_(i,j);
        i = j; 
        j = i * 2 + 1;
    }
    return i > index; // 是否调整了
}
void HeapTimer::del_(size_t index){
    assert(!heap_.empty() && index>=0 && index<heap_.size());
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i<=n);
    if(i<n){
        swapnode_(i,n);
        if(!siftdown_(i,n)){
            siftup_(i);
        }
    }
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}


// 调整制定文件描述符的超时时间
void HeapTimer::adjust(int id, int timeout){
    assert(!heap_.empty() && ref_.count(id)>0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);
    siftdown_(ref_[id], heap_.size());
}

/*
序号、超时、回调
*/
void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb){
    assert(id>=0);
    size_t i;
    if(ref_.count(id)==0){
        i=heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(i);
    }
    else{
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout); 
        heap_[i].cb = cb;
        if(!siftdown_(i, heap_.size())){ // 不下移就上调
            siftup_(i);
        }

    }

}
void doWork(int id);
void HeapTimer::clear(){
    ref_.clear();
    heap_.clear();
}
// 走时， 检测堆里面的时间是否到了, 到了就执行堆头节点里面的callback函数
void HeapTimer::tick(){
    if(heap_.empty()){
        return;
    }
    while(!heap_.empty()){
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count()>0){
            break;
        }
        node.cb(); // 回调函数
        pop();
    }
}
void HeapTimer::pop(){
    assert(!heap_.empty());
    del_(0);
}
// 获取距离下一次事件的时间
int HeapTimer::GetNextTick(){
    tick(); // 走时， 检测堆里面的时间是否到了
    size_t res = -1;
    if(!heap_.empty()){
        res = std::chrono::duration_cast<MS>(heap_.front().expires-Clock::now()).count();
        if(res<0){res = 0;}
    }
    return res;
}