/*
 * @Author: parluo 
 * @Date: 2021-03-15 14:58:12 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-23 15:39:35
 */
#include "webserver.h"
using namespace std;

WebServer::WebServer(int port, int trigMode, int timeoutMS, bool OptLinger, int sqlPort, const char* sqlUser, const char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum, bool openLog, int logLevel, int logQueSize):
        port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
        timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    {
        srcDir_ = getcwd(nullptr,256); // getcwd返回工作路径，nullptr则使用malloc生成，256个长度
        assert(srcDir_);
        strncat(srcDir_, "/resources/", 16); // 从后一个中添加不超过16个的字符到第一个中
        HttpConn::userCount = 0;
        HttpConn::srcDir = srcDir_;
        SqlConnPool::Instance()->Init("localhost",sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

        InitEventMode_(trigMode);
        if(!InitSocket_()) {
            isClose_ = true;
        }
        if(openLog){
            Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
            if(isClose_){
                LOG_ERROR("============= Server init error! =============");
            }
            else{
                LOG_INFO("============ Server init =================");
                LOG_INFO("Port: %d, OpenLinger: %s", port_, OptLinger?"true":"false");
                LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                                        (listenEvent_ & EPOLLET ? "ET":"LT"),
                                        (connEvent_ & EPOLLET ? "ET":"LT"));
                LOG_INFO("LogSys level: %d", logLevel);
                LOG_INFO("srcDir: %s", HttpConn::srcDir);
                LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
            }
        }
        printf("successfully log in!\n");
    }

void WebServer::Start(){
    int timeMS = -1;
    if(!isClose_){ LOG_INFO("=========== Server start ===========");}
    while (!isClose_)
    {
        if(timeoutMS_ > 0){ // 初始化时传入的
            timeMS = timer_->GetNextTick();
        }
        int eventCnt = epoller_->Wait(timeMS); // epoll在此等待一定实际， 非阻塞， 结果在事件数组里
        for(int i = 0; i<eventCnt; ++i){
            int fd = epoller_->GetEvents(i);
            uint32_t events = epoller_->GetEvents(i);
            if(fd==listenFd_){
                DealListen_();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){ 
                /*
                EPOLLRDHUP: 断开连接或者read关闭的半关闭的情况
                EPOLLERR： 发生错误
                EPOLLHUP: 读写都关闭的情况
                */
               assert(users_.count(fd)>0);
               CloseConn_(&users_[fd]);
            }
            else if(events & EPOLLIN){
                assert(users_.count(fd)>0);
                DealRead_(&users_[fd]);
            }
            else if(events & EPOLLOUT){
                assert(users_.count(fd)>0);
                DealWrite_(&users_[fd]);
            }
            else{
                LOG_ERROR("Unexpected event");
            }
            
        }
    }
    


}
WebServer::~WebServer(){
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

/*
初始化事件标志符
*/
void WebServer::InitEventMode_(int trigMode){
    listenEvent_ = EPOLLRDHUP; // 判断client是否关闭 关闭会触发epoll_wait
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP; // 一fd触发一次+传输fd是否关闭
    switch(trigMode){
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

bool WebServer::InitSocket_(){
    int ret;
    struct sockaddr_in addr;
    if(port_>65535 || port_<1024){
        LOG_ERROR("Port: %d, error!", port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    struct linger optLinger = {0};
    // ref: https://blog.csdn.net/z735640642/article/details/84306589
    if(openLinger_){
        optLinger.l_onoff = 1; // 开关
        optLinger.l_linger = 1; // 逗留时长 ms
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_<0){
        LOG_ERROR("Create socket error", port_);
        return false;
    }
    /* 设置是否优雅关闭：
        在close(fd)时
        如关闭时有未发送数据，则逗留。
    */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret<0){
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }
    int optval = 1;
    /*
    ref: https://blog.csdn.net/sty124578/article/details/79403155?utm_medium=distribute.pc_relevant.none-task-blog-baidujs_baidulandingword-1&spm=1001.2101.3001.4242
    为了通知套接口实现不要因为一个地址已被一个套接口使用就不让它与另一个套接口捆绑，应用程序可在bind()调用前先设置SO_REUSEADDR选项。
    */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1){
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    ret = bind(ret, (struct sockaddr*)&addr, sizeof(addr));
    if(ret<0){
        LOG_ERROR("Bind Port : %d error", port_);
        close(listenFd_);
        return false;
    }
    ret = listen(listenFd_, 6);
    if(ret<0) {
        LOG_ERROR("Listen port: %d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if(ret==0){
        LOG_ERROR("Add listen error!");
        close(listenFd_))
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port: %d", port_);
    return false;
}


int WebServer::SetFdNonblock(int fd){
    assert(fd>0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK); // 主要是让read/write不阻塞
}

// 向连接的客户端发送报错误的消息，并关闭连接描述符
void WebServer::SendError_(int fd, const char*info){
    assert(fd>0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0){
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

//deal with
// 处理监听端口的事件， 新的连接过来，先接受accept,再注册连接描述符
void WebServer::DealListen_(){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do{
        int fd = accept(listenFd_, (struct sockaddr*)&addr, &len);
        if(fd<=0) {return;}
        else if(HttpConn::userCount >= MAX_FD){
            SendError_(fd, "Server busy");
            LOG_WARN("Client is full!");
            return;
        }
        AddClient_(fd, addr);
    }while(listenEvent_ & EPOLLET); // 这里是&，然而监听端口还额外注册了EPOLLRDHUP
}


// 对新的连接接入， 先在http连接类里面加入新的关闭定时任务
// 然后才注册epoll的客户端连接描述符及事件
// 
void WebServer::AddClient_(int fd, sockaddr_in addr){
    assert(fd>0);
    users_[fd].init(fd,addr);
    if(timeoutMS_ > 0){
        // 这里std::bind用来绑定一个函数的参数， 可以延迟调用
        // 由于CloseConn_() 是成员函数，有隐式的this指针要传
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::CloseConn_(HttpConn* client){
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}
// 相对server来说的读事件
// 调用read从fd读取
void WebServer::DealRead_(HttpConn* client){
    assert(client);
    ExtentTime_(client); // 又重新激活延时
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}
void WebServer::DealWrite_(HttpConn* client){
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

// 把http连接的描述符延长超时时间
void WebServer::ExtentTime_(HttpConn* client){
    assert(client);
    if(timeoutMS_>0) {
        timer_->adjust(client->GetFd(), timeoutMS_);
    }
}

void WebServer::OnWrite_(HttpConn* client){
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes()==0){
        if(client->IsKeepAlive()){ // 如果接受到的消息是保持连接，
            OnProcess(client);
            return;
        }
    }
    else if(ret<0){
        if(writeErrno == EAGAIN){
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT); 
            /* 
            EPOLLOUT fd描述符可写时将触发epoll_wait, 以前回声服务器做法是注册EPOLLIN,
             连接、输入都触发wait,输入后立即就输出,不管缓冲是否可写
             而现在是用的任务池来执行任务OnWrite_， 必须得一定时间跳出WebServer::start里面的epoll_wait
            */
            return;
        }
    }
}

void WebServer::OnRead_(HttpConn* client){
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno); // readErrno传过去的是指针，且在内部被改动了，接收了错误信息
    if(ret<=0 && readErrno != EAGAIN){ //EAGAIN： 再试一次， 没有可读的
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}
// 接受一个HttpConn对象，代表一个连接，处理来自client的消息
void WebServer::OnProcess(HttpConn* client){
    if(client->process()){ // 原本注册的是EPOLLIN 如果可读，将处理消息做出响应, 并切换为EPOLLOUT，表示要发数据
        epoller_->ModFd(client->GetFd(), connEvent_|EPOLLOUT);
    }
    else{ //不可读时说明是epoll_wait到了延时自动边缘触发，因此要继续保持EPOLLIN
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}