/*
 * @Author: parluo 
 * @Date: 2021-03-15 13:53:48 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-20 16:04:24
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include <vector>
#include <atomic>
#include <assert.h>


// 一个围绕着vector<char> buffer构建的类
// 用来装描述符传过来的消息
class Buffer{
private:
    char* BeginPtr_();
    const char* BeginPtr_() const;
    void MakeSpace_(size_t len);
    std::vector<char> buffer_;
    std::atomic<std::size_t> readPos_; // 当前读取的指针位置 上限是writePos_
    std::atomic<std::size_t> writePos_; // 当前写如的指针位置 上限buffer_.size()
public:
    // 清零
    void RetrieveAll(){
        bzero(&buffer_[0], buffer_.size());
        readPos_ = 0;
        writePos_ = 0;
    }
    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);
    const char* Peek() const;
    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);
    void Append(const Buffer&buff);
    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void EnsureWriteable(size_t len);
    size_t WriteableBytes() const;
    size_t ReadableBytes() const;
    size_t PrependableBytes() const{ //已经读的长度
        return readPos_;
    }
    char* BeginWrite(){
        return BeginPtr_() + writePos_;
    }
    // 返回写指针位置
    const char* BeginWriteConst() const{
        return BeginPtr_() + writePos_;
    }
    void HasWritten(size_t len){
        writePos_ += len;
    }

    

};
#endif