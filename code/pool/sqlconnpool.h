/*
 * @Author: parluo 
 * @Date: 2021-03-15 14:39:11 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-21 15:38:38
 */

#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H
#include <mariadb/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool{
private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_; // 池子大小
    int useCount_;
    int freeCount_;

    std::queue<MYSQL*> connQue_; // sql的连接池
    std::mutex mtx_;
    sem_t semId_;
public:
    static SqlConnPool* Instance();
    MYSQL *GetConn();
    void FreeConn(MYSQL * conn);
    int GetFreeConnCount();
    void Init(const char* host, int port, const char* user, const char* pwd, const char* dbname, int connSize);
    void ClosePool();
};


#endif