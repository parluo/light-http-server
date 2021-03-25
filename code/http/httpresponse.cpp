/*
 * @Author: parluo 
 * @Date: 2021-03-20 14:48:52 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-23 13:51:33
 */

#include "httpresponse.h"

using namespace std;
//后缀类型
const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE{
    {".html",   "text/html"},
    {".xml",    "text/xml"},
    {".xhtml",  "application/xhtml+xml"},
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"}
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
    {400, "/400.html"},
    {403, "/403.html"},
    {404, "/404.html"},
};
HttpResponse::HttpResponse(){
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}
void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code){
    assert(srcDir != "");
    if(mmFile_){ UnmapFile();}
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmFileStat_ = {0};

}
// 把文件映射到内存
void HttpResponse::UnmapFile(){
    if(mmFile_){
        munmap(mmFile_, mmFileStat_.st_size); // 从mmFile_地址映射一定长度的空间
        mmFile_ = nullptr;
    }
}

// 如果错误，会拿到错误页面的path_
void HttpResponse::ErrorHtml_(){
    if(CODE_PATH.count(code_)==-1){ 
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

// 往写Buffer里面response 状态行
void HttpResponse::AddStateLine_(Buffer& buff){
    string status;
    if(CODE_STATUS.count(code_)==1){
        status = CODE_STATUS.find(code_)->second;
    }
    else{
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

// response的首部
void HttpResponse::AddHeader_(Buffer& buff){
    buff.Append("Connection: ");
    if(isKeepAlive_){ // 如果request中表明是长连接就在response中返回一些消息：超时、最多max次请求
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    }
    else{
        buff.Append("close\r\n");
    }
    buff.Append("Content_type: "+ GetFileType_()+"\r\n");
}
// 获取文件类型
string HttpResponse::GetFileType_(){
    string::size_type idx = path_.find_last_of('.');
    if(idx == string::npos){ // string::npos表示不存在的值 真实值一般为-1
        return "text/plain";
    }
    string suffix = path_.substr(idx); // 后缀
    if(SUFFIX_TYPE.count(suffix)==1){
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}
void HttpResponse::ErrorContent(Buffer& buff, string message){
    
}
//返回响应文件的大小byte
size_t HttpResponse::FileLen() const{
    return mmFileStat_.st_size;
}
char* HttpResponse::File(){
    return mmFile_;
}
void HttpResponse::AddContent_(Buffer& buff){
    // 打开要传的文件
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if(srcFd<0){
        ErrorContent(buff, "File NotFound!");
        return;
    }
   /* 将文件映射到内存提高文件的访问速度 
        MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0,mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet==-1){
        ErrorContent(buff, "File NotFound!");
        return;
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    buff.Append("Content-lenght: "+to_string(mmFileStat_.st_size)+"\r\n\r\n"); // 两个换行符，其实是有一行空行的
}
// 接受Buffer对象，server写东西
void HttpResponse::MakeResponse(Buffer& buff){
    // stat函数可以得到诸如修改时间，所有者，文件权限等文件属性。
    // data()返回只想内容的常指针
    // S_ISDIR 判断是否是路径
    if(stat((srcDir_ + path_).data(), &mmFileStat_)<0 ||S_ISDIR(mmFileStat_.st_mode)){
        code_ = 404; //404: 服务器无法根据客户端的请求找到资源（网页）
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)){ // 正在被别的读
        code_ = 403; // 403: 服务器理解请求客户端的请求，但是拒绝执行此请求
    }
    else if(code_==-1){
        code_ = 200; // 成功，OK
    }
    ErrorHtml_();
    AddStateLine_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}
