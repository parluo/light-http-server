/*
 * @Author: parluo 
 * @Date: 2021-03-14 21:10:37 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-23 13:54:46
 */

#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>
#include "../log/log.h"
#include "httprequest.h"
#include "httpresponse.h"

// http总的连接对象类
// 内涵数据读取类Buffer, 读写分离
// 请求类HttpRequest, HttpResponse
// 对应客户端的sockaddr_in
class HttpConn{
private:
    int fd_;
    struct sockaddr_in addr_;
    bool isClose_;
    int iovCnt_; // 统计iovec数组的长度
    struct iovec iov_[2];
    Buffer readBuff_;
    Buffer writeBuff_;
    HttpRequest request_; // 每个HttpConn对象保留的来自client的消息,作为server要对消息解析，得到client的需求信息
    HttpResponse response_; // 每个HttpConn对象里面的Server对client的响应消息对象

public:
    HttpConn();
    ~HttpConn();
    void init(int sockFd, const sockaddr_in& addr);
    ssize_t read(int* saveErrno);
    ssize_t write(int* saveErrno);
    void Close();
    int GetFd() const;
    int GetPort() const;
    const char* GetIP() const;
    sockaddr_in GetAddr() const;
    bool process();
    // iovec结构体里面一共要写的长度
    int ToWriteBytes(){
        return iov_[0].iov_len + iov_[1].iov_len;
    }
    bool IsKeepAlive() const{
        return request_.IsKeepAlive();
    }
    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount; // http连接的用户统计
};


#endif