#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>
#include <mariadb/mysql.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"


// Http消息的处理类
// 有限状态机处理 接收到的字符串消息
class HttpRequest{

public:
    enum PARSE_STATE{
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };
    enum HTTP_CODE{
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    HttpRequest(){Init();}
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer &buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;
private: 
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseBody_(const std::string& line);

    void ParsePath_();
    void ParsePost_();
    void ParseFromUrlencoded_();

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);


    PARSE_STATE state_; // 解析的状态量
    std::string method_, path_, version_, body_; // 一些请求行的信息
    std::unordered_map<std::string, std::string> header_;// 消息头的一些key-value项
    std::unordered_map<std::string, std::string> post_; // post body里面的key-value
    static const std::unordered_set<std::string>  DEFAULT_HTML; 
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;

    static int ConverHex(char ch);    
};


#endif