/*
 * @Author: parluo 
 * @Date: 2021-03-14 20:39:21 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-18 11:15:35
 */

#include "epoller.h"

Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent){
    assert(epollFd_>=0 && events_.size()>0);
}

Epoller::~Epoller(){
    close(epollFd_);
}

//events : EPOLLIN... 事件类型
bool Epoller::AddFd(int fd, uint32_t events){
    if(fd<0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0==epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::ModFd(int fd, uint32_t events){
    if(fd<0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0==epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

// 从epoll池中删除一个文件描述符
bool Epoller::DelFd(int fd){
    if(fd<0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

int Epoller::Wait(int timeoutMs){
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

int Epoller::GetEventFd(size_t i) const{ // 不会修改成员变量
    assert(i<events_.size() && i>=0);
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const{
    assert(i<events_.size() && i>=0);
    return events_[i].events;
}