/*
 * @Author: parluo 
 * @Date: 2021-03-15 14:29:04 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-15 20:41:15
 */

#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"


class SqlConnRAII{
private:
    MYSQL * sql_;
    SqlConnPool* connpool_;

public:
    SqlConnRAII(MYSQL** sql, SqlConnPool* connpool){
        assert(connpool);
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    ~SqlConnRAII(){
        if(sql_){connpool_->FreeConn(sql_);}
    }
};

#endif