/*
 * @Author: parluo 
 * @Date: 2021-03-14 20:31:30 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-14 21:09:50
 */

#ifndef EPOLLER_H
#define EPOLLER_H
#include <sys/epoll.h>
#include <fcntl.h> // fcntl() 修改描述符 半关闭
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <errno.h> // eanger 没有读取的 错误

class Epoller{
private:
    int epollFd_;
    std::vector<struct epoll_event> events_;
public:
    explicit Epoller(int maxEvent=1024);
    ~Epoller();
    bool AddFd(int fd, uint32_t events);
    bool ModFd(int fd, uint32_t events);
    bool DelFd(int fd);
    int Wait(int timeoutMs=-1);
    int GetEventFd(size_t i) const;
    uint32_t GetEvents(size_t i) const;
};

#endif