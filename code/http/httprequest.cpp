/*
 * @Author: parluo 
 * @Date: 2021-03-18 21:12:19 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-23 15:04:06
 */


#include "httprequest.h"


const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
    "/index", "register", "/login", "/welcome", "video", "/picture",
};
const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0},
    {"/login.html", 1},
};
// 初始化里面把一些内部变量清了
void HttpRequest::Init(){
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE; // 初始化状态量
    header_.clear();
    post_.clear();
}

// 长连接和短连接的判断
// 如果1.1版本中不希望使用长连接，首部要Connection:close
// http1.0默认Connection:keep-alive,可能不认识关闭的请求
bool HttpRequest::IsKeepAlive() const{
    if(header_.count("Connection")==1){
        return header_.find("Connection")->second=="keep-alive" & version_=="1.1";
    }
    return false;
}
// parse 接受外部传入的Buffer对象，进行解析
// 无论做啥都是在解析消息
bool HttpRequest::parse(Buffer& buff){
    const char CRLF[] = "\r\n";
    if(buff.ReadableBytes()<=0){
        return false;
    }
    while(buff.ReadableBytes()!= state_!=FINISH){
        // 在一段区间里面寻找一段区间内容
        // 这里CRLF是指针啊
        const char* lineEnd = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF+2); 
        std::string line(buff.Peek(), lineEnd); // 拿到第一行
        switch(state_){
            case REQUEST_LINE:
                if(!ParseRequestLine_(line)){
                    return false;
                }
                ParsePath_();
                break;
            case HEADERS:
                ParseHeader_(line);
                if(buff.ReadableBytes()<=2){ // 如果没有BODY了就结束
                    state_ = FINISH;
                }
                break;
            case BODY:
                ParseBody_(line);
                break;
            default:
                break;
        }
        if(lineEnd == buff.BeginWrite()){break;} // 这一行有用么？ 比如只有一行且没有CRLF时，会在此处推出
        buff.RetrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}
// 请求行内容示例：  GET /index.html HTTP/1.1
bool HttpRequest::ParseRequestLine_(const std::string&line){
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;  // 就是match_results [0] 放匹配到的 [1][2]...放括号的捕捉项 
    if(regex_match(line, subMatch, patten)){
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;  //转到下一个状态
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}
// 从内部变量path_处理请求的网页
void HttpRequest::ParsePath_(){
    if(path_=="/"){ // 相当于主页
        path_ = "/index.html";
    }
    else{
        for(auto &item: DEFAULT_HTML){ //对默认的页面请求，补齐
            if(item==path_){
                path_ += ".html";
                break;
            }
        }

        /*
            if(DEAFAULT_HTML.find(path_)!=DEAFAULT_HTML.end()) // O(1)
                path_ += ".html";        
        */

    }
}
// 消息头示例：  
/* 
User-Agent : Mozilla/5.0
Accept: image/gif, image/jpeg
*/
void HttpRequest::ParseHeader_(const std::string& line){
    std::regex patten("^([^:]*):?(.*)$");
    std::smatch subMatch;
    if(std::regex_match(line, subMatch, patten)){
        header_[subMatch[1]] = subMatch[2];
    }
    else{
        state_ = BODY; // 下一个状态
    }
}
// 一般body是POST方式才带的
void HttpRequest::ParseBody_(const std::string& line){
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body: %s, len: %d", line.c_str(), line.size());
}

// POST body的形式 https://blog.csdn.net/weixin_30376453/article/details/95453727
void HttpRequest::ParsePost_(){
    // POST的body类型： application/x-www-form-urlencoded 浏览器的原生 form 表单
    if(method_ == "POST" && header_["Content-Type"]=="application/x-www-form-urlencoded"){
        ParseFromUrlencoded_();
        if(DEFAULT_HTML_TAG.count(path_)){ // 如果是注册或者登陆页面
            // 这里如果用索引[path_]，ERROR：对非静态成员引用必须与特定对象相对
            // 因为path_是类成员对象，必须先初始化对象示例才行
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag: %d", tag);
            if(tag==0 || tag == 1){
                bool isLogin = (tag==1);
                if(UserVerify(post_["username"], post_["password"], isLogin)){
                    path_ = "/welcome.html";
                }
                else{
                    path_ = "/error.html";
                }
            }  
        }
    }
}
// 单个16进制转10进制
int HttpRequest::ConverHex(char ch){
    if(ch>='A' && ch<='F') return ch-'A'+10;
    if(ch>='a' && ch <= 'f') return ch-'a'+10;
    return ch; // 自动隐式转换
}

/* application/x-www-form-urlencoded
form数据转换成一个字串（name1=value1&name2=value2…），然后把这个字串append到url后面，用?分割，加载这个新的url
例如百度图片搜索纽约
https://image.baidu.com/search/index?
tn=baiduimage&ipn=r&ct=201326592&cl=2&lm=-1
&st=-1&fm=result&fr=&sf=1&fmq=1616413818332_R
&pv=&ic=0&nc=1&z=&hd=&latest=&copyright=&se=1
&showtab=0&fb=0&width=&height=&face=0&istype=2
&ie=utf-8&sid=&word=纽约
*/
void HttpRequest::ParseFromUrlencoded_(){
    if(body_.size()==0) return;
    std::string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;
    for(; i<n; ++i){
        char ch = body_[i];
        switch(ch){
            case '=':
                key = body_.substr(j, i-j);
                j = i + 1;
                break;
            case '+':
                body_[i] = ' ';
                break;
            case '%':
                num = ConverHex(body_[i+1])*16 + ConverHex(body_[i+2]);
                body_[i+2] = num % 10 + '0';
                body_[i+1] = num / 10 + '0';
                i += 2;
                break;
            case '&':
                value = body_.substr(j, i-j);
                j = i + 1;
                post_[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default:
                break;
        }
    }
    assert(j<=i);
    if(post_.count(key)==0 && j<i){
        value = body_.substr(j, i-j);
        post_[key] = value;
    }
}


bool HttpRequest::UserVerify(const std::string &name, const std::string &pwd, bool isLogin){
    if(name=="" || pwd=="") {
        return false;
    }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql, SqlConnPool::Instance());
    assert(sql);
    bool flag = false; // 是否发生错误
    unsigned int j = 0;
    char order[256] = {0};
    MYSQL_FIELD * fields = nullptr;
    MYSQL_RES *res = nullptr;
    if(!isLogin){flag = true;};
    snprintf(order, 256, "SELECT username, passward FROM user WHERE username='%s', LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);
    if(mysql_query(sql, order)){ // 返回0：成功 1：失败
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);
    while(MYSQL_ROW row = mysql_fetch_row(res)){
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        std::string password(row[1]);
        if(isLogin){  // 是否登录
            if(pwd==password){
                flag = true;
            }
            else{
                flag = false;
                LOG_DEBUG("pwd error");
            }
        }
        else{
            flag = false;
            LOG_DEBUG("user used");
        }
    }
    mysql_free_result(res);
    if(!isLogin && flag == true){
        LOG_DEBUG("register!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')",name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if(mysql_query(sql, order)){
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG("UserVerify success!");
    return flag;
}


std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}