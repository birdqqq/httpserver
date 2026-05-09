#include "../../include/http/HttpServer.h"

#include <any>
#include <functional>
#include <memory>

namespace http
{

// 默认http回应函数
//q 访问到未注册路径时返回 404 Not Found
void defaultHttpCallback(const HttpRequest &, HttpResponse *resp)
{
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
}

HttpServer::HttpServer(int port,
                       const std::string &name,
                       bool useSSL,
                       muduo::net::TcpServer::Option option)
    : listenAddr_(port)
    , server_(&mainLoop_, listenAddr_, name, option)
    , useSSL_(useSSL)
    //q httpCallback_ 绑定的是handleRequest函数，this指针是一位内handleRequest是类成员函数，需要this指针指明在哪个对象上使用
    , httpCallback_(std::bind(&HttpServer::handleRequest, this, std::placeholders::_1, std::placeholders::_2))
{
    initialize();
}

// 服务器运行函数
void HttpServer::start()
{
    LOG_WARN << "HttpServer[" << server_.name() << "] starts listening on" << server_.ipPort();
    server_.start();//q tcp服务器启动
    mainLoop_.loop();//q reactor核心，一直在监听有无新连接、有无数据、有无客户端断开
}

void HttpServer::initialize()
{
    // 设置回调函数
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this,
                  std::placeholders::_1,
                  std::placeholders::_2,
                  std::placeholders::_3));
}

void HttpServer::setSslConfig(const ssl::SslConfig& config)
{
    if (useSSL_)
    {
        sslCtx_ = std::make_unique<ssl::SslContext>(config);
        if (!sslCtx_->initialize())
        {
            LOG_ERROR << "Failed to initialize SSL context";
            abort();
        }
    }
}

void HttpServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
    //q 建立连接
    if (conn->connected())
    {
        //q 是否使用ssl加密
        if (useSSL_)
        {
            //q 对普通链接conn进行加密
            auto sslConn = std::make_unique<ssl::SslConnection>(conn, sslCtx_.get());
            //q 回调onMessage函数处理数据
            sslConn->setMessageCallback(
                std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            //q 将加密连接加到ssl字典中，conn是key sslConn是value,用conn查找sslConn
            sslConns_[conn] = std::move(sslConn);
            //q 开始ssl握手,客户端服务器交换加密参数
            sslConns_[conn]->startHandshake();
        }
        //q 不是加密连接直接使用HttpContext解析上下文
        conn->setContext(HttpContext());
    }
    //q 连接断开
    else 
    {
        if (useSSL_)
        {
            //q 加密通信需要在字典中擦除该链接，释放内存
            sslConns_.erase(conn);
        }
    }
}

//q conn 哪个客户端；buf 原始数据；receiveTime 到达时间
void HttpServer::onMessage(const muduo::net::TcpConnectionPtr &conn,
                           muduo::net::Buffer *buf,
                           muduo::Timestamp receiveTime)
{
    try
    {
        // 这层判断只是代表是否支持ssl
        if (useSSL_)
        {
            LOG_INFO << "onMessage useSSL_ is true";
            // 1.查找对应的SSL连接
            auto it = sslConns_.find(conn);
            if (it != sslConns_.end())
            {
                LOG_INFO << "onMessage sslConns_ is not empty";
                // 2. SSL连接处理数据
                it->second->onRead(conn, buf, receiveTime);

                // 3. 如果 SSL 握手还未完成，直接返回
                if (!it->second->isHandshakeCompleted())
                {
                    LOG_INFO << "onMessage sslConns_ is not empty";
                    return;
                }

                // 4. 从SSL连接的解密缓冲区获取数据（明文）
                muduo::net::Buffer* decryptedBuf = it->second->getDecryptedBuffer();
                if (decryptedBuf->readableBytes() == 0)
                    return; // 没有解密后的数据

                // 5. 使用解密后的数据进行HTTP 处理
                buf = decryptedBuf; // 将 buf 指向解密后的数据
                LOG_INFO << "onMessage decryptedBuf is not empty";
            }
        }
        // HttpContext对象用于解析出buf中的请求报文，并把报文的关键信息封装到HttpRequest对象中
        HttpContext *context = boost::any_cast<HttpContext>(conn->getMutableContext());
        if (!context->parseRequest(buf, receiveTime)) // 解析一个http请求
        {
            // 如果解析http报文过程中出错
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");//4**是服务器错误 400表示请求语法错误
            conn->shutdown();
        }
        // 如果buf缓冲区中解析出一个完整的数据包才封装响应报文
        if (context->gotAll())//q gotAll() 得到完整的请求才会为true 在tcp中http请求可能会拆成多个tcp包，httpcontext会攒到一个完成请求时返回true
        {
            onRequest(conn, context->request());//q 将得到的完整请求交给onRequest处理
            context->reset();//q 清空工作台，准备接收下一个请求
        }
    }
    catch (const std::exception &e)//q std::exception是异常基类，此处可以捕获：bad_alloc(内存不足) out_of_range(越界) invalid_argument(参数错误)等类型
    {
        // 捕获异常，返回错误信息
        LOG_ERROR << "Exception in onMessage: " << e.what();
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }
}

void HttpServer::onRequest(const muduo::net::TcpConnectionPtr &conn, const HttpRequest &req)
{
    const std::string &connection = req.getHeader("Connection");//q 获取请求头中的Connection，http1.0在请求头中Connection:"Keep-Alive"是长连接，
                                                                //q 1.0默认为短链接，请求完就断开;1.1默认长连接
    bool close = ((connection == "close") ||
                  (req.getVersion() == "HTTP/1.0" && connection != "Keep-Alive"));
    HttpResponse response(close);

    // 根据请求报文信息来封装响应报文对象
    httpCallback_(req, &response); // 执行onHttpCallback函数? q 执行的是handleRequest函数

    // 可以给response设置一个成员，判断是否请求的是文件，如果是文件设置为true，并且存在文件位置在这里send出去。
    muduo::net::Buffer buf;
    response.appendToBuffer(&buf);//q 将得到的数据转化为报文形式：请求头 请求体
    // 打印完整的响应内容用于调试
    LOG_INFO << "Sending response:\n" << buf.toStringPiece().as_string();

    conn->send(&buf);
    // 如果是短连接的话，返回响应报文后就断开连接
    if (response.closeConnection())
    {
        conn->shutdown();
    }
}

// 执行请求对应的路由处理函数
void HttpServer::handleRequest(const HttpRequest &req, HttpResponse *resp)
{
    try
    {
        // 处理请求前的中间件
        HttpRequest mutableReq = req;//q 创建一个req的副本 因为讲过中间件可能要添加请求头或属性
        middlewareChain_.processBefore(mutableReq);//q 按照注册顺序，顺序执行中间件的前置处理逻辑

        // 路由处理
        if (!router_.route(mutableReq, resp))//q 核心路由分发，router_中有有1张路由表 记录URL路径和处理函数handler
        //q 比如： URL: /login 找到类LoginHandler,调用函数LoginHandler::handle()处理请求
        //q 体现httpCallback_默认绑定handleRequset的原因：所有请求都要经过handleRequset参能去到正确的handle处理
        {
            //q 没找到匹配的URL处理函数
            LOG_INFO << "请求的啥,url:" << req.method() << " " << req.path();
            LOG_INFO << "未找到路由,返回404";
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
        }

        // 处理响应后的中间件
        middlewareChain_.processAfter(*resp);//q 处理后置中间件链，顺序和前置相反 栈式调用 先进后出
    }
    catch (const HttpResponse& res) 
    {
        // 处理中间件抛出的响应（如CORS预检请求）
        *resp = res;
    }
    catch (const std::exception& e) 
    {
        // 错误处理
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setBody(e.what());
    }
}

} // namespace http