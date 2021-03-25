/*
 * @Author: parluo 
 * @Date: 2021-03-15 14:18:01 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-20 21:21:56
 */

#include "httpconn.h"

using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount; 
bool HttpConn::isET;

// 连接数量加1, 清空读写buffer
void HttpConn::init(int fd, const sockaddr_in & addr){
    assert(fd>0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

int HttpConn::GetFd() const{
    return fd_;
}

void HttpConn::Close(){
    response_.UnmapFile();
    if(isClose_ == false){
        isClose_ = true;
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_,GetPort(), (int)userCount);
    }
}

ssize_t HttpConn::read(int* saveErrno){
    ssize_t len = -1;
    do{
        len = readBuff_.ReadFd(fd_, saveErrno);
        if(len<=0) break;
    }while(isET);
    return len;
}

// 处理当前这个HttpConn连接对象的消息
bool HttpConn::process(){
    request_.Init();
    if(readBuff_.ReadableBytes()<=0){ // 检查buffer是否能继续读
        return false;
    }
    else if(request_.parse(readBuff_)){ // 如果成功解析
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    }
    else{
        response_.Init(srcDir, request_.path(), false, 400);
    }

    response_.MakeResponse(writeBuff_);

    // 响应的一些消息
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    // 文件内容
    if(response_.FileLen()>0 && response_.File()){
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;        
    }
    LOG_DEBUG("filesize: %d， %d to %d", response_.FileLen(), iovCnt_, ToWriteBytes());
    return true;
}