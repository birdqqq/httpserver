#pragma once

#include <iostream>

#include <muduo/net/TcpServer.h>

#include "HttpRequest.h"

namespace http
{

class HttpContext 
{
public:
    enum HttpRequestParseState
    {
        kExpectRequestLine, // 解析请求行
        kExpectHeaders, // 解析请求头
        kExpectBody, // 解析请求体
        kGotAll, // 解析完成
    };
    
    HttpContext()
    : state_(kExpectRequestLine)
    {}

    bool parseRequest(muduo::net::Buffer* buf, muduo::Timestamp receiveTime);//q 核心 将接收到的字符串解析放进request_中
    bool gotAll() const 
    { return state_ == kGotAll;  }

    //q 清空旧请求 http1.1默认长连接 keep-alive 一个tcp连接会发送多个请求 所以处理完一个请求后需要清空以便于接收下一个请求
    void reset()
    {
        state_ = kExpectRequestLine;
        HttpRequest dummyData;
        request_.swap(dummyData);
    }

    const HttpRequest& request() const
    { return request_;}

    HttpRequest& request()
    { return request_;}

private:
    bool processRequestLine(const char* begin, const char* end);
private:
    HttpRequestParseState state_;
    HttpRequest           request_;
};

} // namespace http