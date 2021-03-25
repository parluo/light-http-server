/*
提供宏定义给来记录不同级别日志
*/

#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/stat.h>
#include <mutex>
#include "blockqueue.h"
#include "../buffer/buffer.h"


class Log{
public:
    void init(int level, const char* path="./log", const char* suffix=".log",
            int maxQueueCapability=1024);
    static Log* Instance();

    // 由写线程刷新日志
    static void* FlushLogThread(){ 
        Log::Instance()->AsyncWrite_(); 
    }

    void write(int level, const char* format, ...);
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen(){ return isOpen_; }

private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log(); // 析构函数得虚，重写各自的释放方法，构造不能虚，会不知道类的类型，没法构造
    void AsyncWrite_();



    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256; // LOG一次写入的最大字符长度，（\n算一个）
    static const int MAX_LINES = 50000;

    const char* path_;
    const char* suffix_;
    
    int MAX_LINES_;

    int lineCount_;
    int toDay_;

    bool isOpen_;

    Buffer buff_;
    int level_;
    bool isAsync_; // 是否异步


    FILE* fp_; // 日志文件描述符
    std::unique_ptr<BlockDeque<std::string>> deque_;
    std::unique_ptr<std::thread> writeThread_;
    std::mutex mtx_;

};

#define LOG_BASE(level,format, ...) \
    do {\
        Log* log = Log::Instance();\
        if(log->IsOpen() && (log->GetLevel() <= level)){\
            log->write(level, format, ##__VA_ARGS__);\
            log->flush();\
        }\
    }while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);