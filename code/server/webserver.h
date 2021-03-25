/*
提供一个WebServer的类，接收参数，开启start运行，
实现主函数中提出的所有要求
*/

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h> // strncat

#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/threadpool.h"
#include "./epoller.h"
#include "../http/httpconn.h"
#include "../pool/sqlconnRAII.h"

class WebServer{
public:
    WebServer(int port, int trigMode, int timeoutMS, bool OptLinger, int sqlPort, const char* sqlUser, const char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum, bool openLog, int logLevel, int logQueSize);
    ~WebServer();
    void Start();
private:
    
    int port_;
    bool openLinger_;
    int timeoutMS_; // 初始化时就设置的一个超时时间
    bool isClose_; // 在start里面判断标志符，表明初始化时出问题，start里面将不进入循环
    int listenFd_; // 监听端口文件描述符
    char* srcDir_; // 图片、视频资源路径

    uint32_t listenEvent_; // 服务器连接监听端口触发事件类型
    uint32_t connEvent_; // server和每一个client的连接的fd的事件类型

    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;

    void InitEventMode_(int trigMode);
    bool InitSocket_();
    void AddClient_(int fd, sockaddr_in addr);
    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);
    void SendError_(int fd, const char* info);
    void OnWrite_(HttpConn* client); // 写callback函数
    void OnRead_(HttpConn* client); // 读callback函数
    void CloseConn_(HttpConn* client);
    void ExtentTime_(HttpConn* client);
    void OnProcess(HttpConn* client);

    static int SetFdNonblock(int fd);
    static const int MAX_FD = 65536; // 最大连接数
};