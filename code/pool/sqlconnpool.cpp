/*
 * @Author: parluo 
 * @Date: 2021-03-15 16:52:40 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-21 15:41:54
 */

#include "sqlconnpool.h"

using namespace std;


SqlConnPool::SqlConnPool(){
    useCount_ = 0;
    freeCount_ = 0;
}

SqlConnPool* SqlConnPool::Instance(){
    static SqlConnPool connPool;
    return &connPool;
}
/*
从SqlConnPool内部任务池子取出连接任务
*/
MYSQL* SqlConnPool::GetConn(){
    MYSQL *sql = nullptr;
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_); // 在此等待信号的到来
    {
        lock_guard<mutex> locker(mtx_); // 从队列取sql任务要加锁
        sql = connQue_.front();
        connQue_.pop();
    }
    return sql;
}
void SqlConnPool::Init(const char* host, int port, const char* user, const char* pwd, const char* dbname, int connSize=10){
    assert(connSize>0);
    for (int i = 0; i < connSize; i++)
    {   
        MYSQL *sql = nullptr;
        sql = mysql_init(sql); // 创建一个可以用于mysql_real_connect接口的MYSQL对象。由此创建的对象，可以用mysql_close()释放。
        if(!sql){
            LOG_ERROR("Mysql init error");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host, user, pwd, dbname, port, nullptr, 0);
        if(!sql){
            LOG_ERROR("Mysql Connect error!");
        }
        connQue_.push(sql);
    }
    MAX_CONN_ = connSize;
    sem_init(&semId_, 0, MAX_CONN_);
}

// 往连接池里放对象
void SqlConnPool::FreeConn(MYSQL*sql){
    assert(sql);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql);
    sem_post(&semId_); // 每放一个MYSQL*，信号量加1
}

// 
void SqlConnPool::ClosePool(){
    lock_guard<mutex> locker(mtx_);
    while (!connQue_.empty())
    {
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
    mysql_library_end();
}

SqlConnPool::~SqlConnPool(){
    ClosePool();
}