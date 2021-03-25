/*
 * @Author: parluo 
 * @Date: 2021-03-18 14:11:14 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-20 16:46:04
 */


#include "buffer.h"

char* Buffer::BeginPtr_(){
    return &*buffer_.begin();
}
const char* Buffer::BeginPtr_() const{
    return &*buffer_.begin();
}
// 返回读指针位置
const char* Buffer::Peek() const{
    return readPos_ + BeginPtr_();
}
// 返回可以继续写的字节长度
size_t Buffer::WriteableBytes() const{
    return buffer_.size()-writePos_;
}
// 准备读取的长度
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}

// 在这里移动readPos_指针
void Buffer::Retrieve(size_t len){
    assert(len <= ReadableBytes());
    readPos_ += len;
}
// 把readPos_指针移动到end
void Buffer::RetrieveUntil(const char* end){
    assert(Peek()<=end);
    Retrieve(end-Peek());
}
// 确保空间够写，不管是用扩容还是挪移
void Buffer::EnsureWriteable(size_t len){
    if(WriteableBytes()<len){
        MakeSpace_(len);
    }
    assert(WriteableBytes()>=len);
}

// 腾挪/扩容空间
// 以读+剩余的<总的需求时， 要resize vector
void Buffer::MakeSpace_(size_t len){
    if(WriteableBytes() + PrependableBytes() < len){
        buffer_.resize(writePos_ + len + 1);
    }
    else{
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_()+readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}
void Buffer::Append(const std::string& str){
    Append(str.data(), str.length()); // string data()返回数据的指针
}
void Buffer::Append(const Buffer& buff){
    Append(buff.Peek(), buff.ReadableBytes());
}
// len: 超过当前可写多出的那一部分
void Buffer::Append(const char* str, size_t len){
    assert(str);
    EnsureWriteable(len);  // 要写这么长的数据，先确保空间够
    std::copy(str, str+len, BeginWrite());
    HasWritten(len);
}
// 从一个连接上的文件描述符中读取东西
ssize_t Buffer::ReadFd(int fd, int* saveErrno){
    char buff[65535];
    struct iovec iov[2];
    const size_t writable = WriteableBytes();
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

// 在一次函数调用中：
// ① writev以顺序iov[0]、iov[1]至iov[iovcnt-1]从各缓冲区中聚集输出数据到fd
// ② readv则将从fd读入的数据按同样的顺序散布到各缓冲区中，readv总是先填满一个缓冲区，然后再填下一个
    const ssize_t len = readv(fd, iov, 2);
    if(len<0){
        *saveErrno = errno; // 系统中的错误都存储在errno中 https://blog.csdn.net/return9/article/details/89384313
    }
    else if(static_cast<size_t>(len)<= writable){ // iov[0]不够写，占用了buff一部分
        writePos_ += len;
    }
    else{
        writePos_ = buffer_.size(); // 这里提前吧writePos_挪到末尾， 与后面要开辟空间有关
        Append(buff, len-writable); // 要补充的 buf len
    }
    return len;    
}