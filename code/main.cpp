/*
要实现一个可以运行的网络服务端程序，有以下功能：
1. socket访问；
2. http访问；
3. 数据库连接；
4. 日志
*/
// 连编多个CPP得用make
#include <unistd.h>
#include "./server/webserver.h"

int main(){
    WebServer server(1316, 3, 60000, false, // 端口 ET模式 连接超时
    3306, "root", "123456", "webserver",   //sql端口 用户 密码 数据库名称
    12, 6, true, 1, 1024);  // sql连接池 任务线程池 日志开启 日志等级 
    server.Start();
}