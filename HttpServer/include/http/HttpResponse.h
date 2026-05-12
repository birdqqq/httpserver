#pragma once

#include <muduo/net/TcpServer.h>

namespace http
{

class HttpResponse 
{
public:
    enum HttpStatusCode // enum 枚举类型 默认从0开始  
    {
        kUnknown, //未知状态码，未设置具体状态码时使用
        k200Ok = 200,  //200 请求已经成功处理 服务器服务器返回了请求资源
        k204NoContent = 204, // 204 请求已经成功处理 但没有返回内容
        k301MovedPermanently = 301, // 301 重定向状态码 表示请求的资源已经被永久的移动到新的URL 此时客户端应使用新的URL发起请求
        k400BadRequest = 400, // 400 客户端错误状态码 请求语法有误
        k401Unauthorized = 401, // 401 客户端错误 需要客户的用户认证
        k403Forbidden = 403, // 403 服务器理解客户请求 但拒绝此请求
        k404NotFound = 404, // 404 NOT FOUND
        k409Conflict = 409, // 409 请求处理遇到冲突 通常与资源的当前状态有关
        k500InternalServerError = 500, // 500 服务器错误 服务器遇到未知情况 无法处理请求
    };

    HttpResponse(bool close = true)
        : statusCode_(kUnknown) //状态码设置为0
        , closeConnection_(close) // 默认结束后关闭连接 , close=true
    {}

    void setVersion(std::string version)//设置http版本
    { httpVersion_ = version; }
    void setStatusCode(HttpStatusCode code) //设置服务器状态码
    { statusCode_ = code; }

    HttpStatusCode getStatusCode() const  // 获取当前http响应的状态码
    { return statusCode_; }

    void setStatusMessage(const std::string message) // 设置http响应的状态消息
    { statusMessage_ = message; }

    void setCloseConnection(bool on) // 是否在发送响应后关闭连接
    { closeConnection_ = on; }

    bool closeConnection() const // 获取当前的关闭连接设置
    { return closeConnection_; }
    
    void setContentType(const std::string& contentType) // 设置相应内容的媒体类型 通过添加Content-Type头实现
    { addHeader("Content-Type", contentType); }

    void setContentLength(uint64_t length) // 设置响应体的长度 通过添加Content-Length头实现
    { addHeader("Content-Length", std::to_string(length)); }

    void addHeader(const std::string& key, const std::string& value) // 添加或修改响应头 
    { headers_[key] = value; }
    
    void setBody(const std::string& body) // 设置响应体内容
    { 
        body_ = body;
        // body_ += "\0";
    }

    void setStatusLine(const std::string& version,
                         HttpStatusCode statusCode,
                         const std::string& statusMessage); 

    void setErrorHeader(){} // 预留方法 设置错误响应头

    void appendToBuffer(muduo::net::Buffer* outputBuf) const;
private:
    std::string                        httpVersion_; 
    HttpStatusCode                     statusCode_;
    std::string                        statusMessage_;
    bool                               closeConnection_;
    std::map<std::string, std::string> headers_;
    std::string                        body_;
    bool                               isFile_;
};

} // namespace http
