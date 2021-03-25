/*
 * @Author: parluo 
 * @Date: 2021-03-17 21:28:45 
 * @Last Modified by: parluo
 * @Last Modified time: 2021-03-20 17:40:05
 */


#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse{
private:

    int code_; // 状态码
    bool isKeepAlive_;
    
    std::string path_;
    std::string srcDir_;

    char* mmFile_; // 响应文件的数据指针
    struct stat mmFileStat_;  //文件信息结构体


    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
public: 
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void UnmapFile();
    char* File();
    
    size_t FileLen() const;
    void MakeResponse(Buffer& buff);
    void ErrorHtml_();
    void AddStateLine_(Buffer& buff);
    void AddHeader_(Buffer& buff);
    void AddContent_(Buffer& buff);
    void ErrorContent(Buffer& buff, string message);
    std::string GetFileType_();
};

#endif