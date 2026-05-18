#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../GomokuServer.h"

class EntryHandler : public http::router::RouterHandler 
{
public:
    //q 构造时将GomokuServer对象指针赋值到server_，因为handle需要使用服务器资源 Session MySQL AiGame等
    explicit EntryHandler(GomokuServer* server) : server_(server) {}//q 禁止隐式转换 一种安全声明

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;//q 重写父类的handle函数

private:
    GomokuServer* server_;
};