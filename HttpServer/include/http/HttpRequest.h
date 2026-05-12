#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include <muduo/base/Timestamp.h>

namespace http
{

class HttpRequest
{
public:
    enum Method //q enum 关键字 表示一个枚举类型 Method枚举类型名称 {}中是枚举值 下列方法分别对应 0 1 2 3 4 5 6
    {
        kInvalid, kGet, kPost, kHead, kPut, kDelete, kOptions//具体含义 详见文档
    };
    
    HttpRequest()//构造函数
        : method_(kInvalid)
        , version_("Unknown")
    {
    }
    
    void setReceiveTime(muduo::Timestamp t);//q 设置获取请求的时间 依赖moduo::timestamp库
    muduo::Timestamp receiveTime() const { return receiveTime_; }//q 记录是假用于统计请求耗时 记录日志 超时处理
    
    bool setMethod(const char* start, const char* end);//q 从http请求中获取方法 网络层收到的请求时字节流所以使用char* !
    Method method() const { return method_; }//获取请求方法的接口

    void setPath(const char* start, const char* end);//q 从请求中解析路径
    std::string path() const { return path_; }

    void setPathParameters(const std::string &key, const std::string &value);//q 解析路径参数 即 /user/123中的123
    std::string getPathParameters(const std::string &key) const;

    void setQueryParameters(const char* start, const char* end);//q 解析查询参数 即 /user/menu?id=1&name=qq 中id : 1 ; mane : qq (k-v)
    std::string getQueryParameters(const std::string &key) const;
    
    void setVersion(std::string v)//q http版本1.0/1.1
    {
        version_ = v;
    }

    std::string getVersion() const
    {
        return version_;
    }
    
    void addHeader(const char* start, const char* colon, const char* end);//q 添加请求头 请求头k-v结构
    std::string getHeader(const std::string& field) const;

    const std::map<std::string, std::string>& headers() const
    { return headers_; }

    void setBody(const std::string& body) { content_ = body; }//q 解析请求体 一般用于业务层调用 构建响应json 直接赋值即可
    void setBody(const char* start, const char* end) //q 从HTTP请求中解析请求体
    { 
        if (end >= start) 
        {
            content_.assign(start, end - start); //q string的赋值函数，这里表示 从start开始截取长度为end-start的字符串
        }
    }
    
    std::string getBody() const
    { return content_; }

    void setContentLength(uint64_t length)//q 获取请求体长度
    { contentLength_ = length; }
    
    uint64_t contentLength() const
    { return contentLength_; }

    void swap(HttpRequest& that);

private:
    Method                                       method_; // 请求方法
    std::string                                  version_; // http版本
    std::string                                  path_; // 请求路径
    std::unordered_map<std::string, std::string> pathParameters_; // 路径参数
    std::unordered_map<std::string, std::string> queryParameters_; // 查询参数
    muduo::Timestamp                             receiveTime_; // 接收时间
    std::map<std::string, std::string>           headers_; // 请求头 
    std::string                                  content_; // 请求体
    uint64_t                                     contentLength_ { 0 }; // 请求体长度
};  

} // namespace http