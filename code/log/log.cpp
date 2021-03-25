#include "log.h"

using namespace std;


Log::Log(){
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

Log* Log::Instance(){
    static Log inst;
    return &inst;
}

void Log::AsyncWrite_(){
    string str = "";
    while(deque_->pop(str)){
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}


// 单独创建一个写日志的线程
void Log::init(int level=1, const char* path="./log", const char* suffix=".log",int maxQueueCapability=1024){
    isOpen_ = true;
    level_ = level;
    if(maxQueueCapability > 0){
        isAsync_ = true;
        if(!deque_){
            unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            deque_ = move(newDeque);

            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
            writeThread_ = move(NewThread);
        }
    }
    else{
        isAsync_ = false;
    }

    lineCount_ = 0;

    time_t timer = time(nullptr); //time_t 底层就是long int
    struct tm *sysTime = localtime(&timer); // 从time_t的格式转换为tm结构体的时间格式
    auto t = *sysTime;
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    // 生成日志文件名称
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%-2d%s",path_, t.tm_year+1900, t.tm_mon + 1, t.tm_mday, suffix_);
    toDay_ = t.tm_mday;
    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll(); // 清零、初始化
        if(fp_){ // 如果是打开的先关了
            flush(); // 日志队列赶紧出队，确保队列空了（因为Log类额外有一个写线程用来写日志内容，能保证它写完队列里的string）
            fclose(fp_);
        }

        fp_ = fopen(fileName, "a");
        if(fp_ == nullptr){
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a");
        }
        assert(fp_!=nullptr);
    }
}