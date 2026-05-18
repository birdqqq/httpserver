#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../GomokuServer.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"

#include <shared_mutex>

class LoginHandler : public http::router::RouterHandler 
{
public:
    explicit LoginHandler(GomokuServer* server) : server_(server) {}
    
    //q 将queryUserId()拆分出来 使handle负责HTTP流程（解析请求 响应 session处理） queryUserId负责数据库验证-->后端分层思想

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;//q 处理login请求

private:
    int queryUserId(const std::string& username, const std::string& password);//q 根据用户名密码查询数据库

private:
    GomokuServer*       server_;
    http::MysqlUtil     mysqlUtil_;
};