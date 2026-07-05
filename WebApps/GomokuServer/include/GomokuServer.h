#pragma once

#include <atomic>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <mutex>

#include <shared_mutex>


#include "AiGame.h"//q 对AI对战逻辑的封装
#include "../../../HttpServer/include/http/HttpServer.h"//q 说明GomokuServer是基于该类的业务系统
#include "../../../HttpServer/include/utils/MysqlUtil.h"//q 数据库
#include "../../../HttpServer/include/utils/FileUtil.h"//q 文件
#include "../../../HttpServer/include/utils/JsonUtil.h"//q Json 说明

//q 前向声明
//q Handler业务分层：每个接口都有自己的处理类
class LoginHandler;
class EntryHandler;
class RegisterHandler;
class MenuHandler;
class AiGameStartHandler;
class LogoutHandler;
class AiGameMoveHandler;
class GameBackendHandler;

//q 表示游戏状态
#define DURING_GAME 1 
#define GAME_OVER 2
//q 最多ai对局数，考虑了并发情况
#define MAX_AIBOT_NUM 4096
//q 整个服务器的总控制器  HTTP+登录+Session+AI对战
class GomokuServer
{
public:
    //q 构造函数，需要参数：端口，名字，控制服务器socket的端口复用（默认不复用）
    GomokuServer(int port,
                 const std::string& name,
                 muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort);
    void setThreadNum(int numThreads);//q 设置线程数
    void start();//q 启动服务器
private:
    void initialize();//q 总初始化（数据库，路由，session）
    void initializeSession();//q 支持用户登陆状态
    void initializeRouter();//q Http路由系统
    void initializeMiddleware();//q 支持中间件
    
    //q 框架设计开始
    void setSessionManager(std::unique_ptr<http::session::SessionManager> manager)
    //q unique_ptr c++的一种独占所有权的智能指针，独占所有权：同时只能有一个unique_ptr只想某对象
    {
        httpServer_.setSessionManager(std::move(manager));//q 所有权转移，因为GomokuServer是对httpServer的封装 直接将管理者转移给httpServer即可
                                                          //q move()函数是移动至新位置，原位置清空了
    }
    //q 对HttpServer的封装
    http::session::SessionManager*  getSessionManager() const
    {
        return httpServer_.getSessionManager();
    }
    
    void restartChessGameVsAi(const http::HttpRequest& req, http::HttpResponse* resp);
    void getBackendData(const http::HttpRequest& req, http::HttpResponse* resp);

    //q 封装http响应
    void packageResp(const std::string& version, http::HttpResponse::HttpStatusCode statusCode,
                     const std::string& statusMsg, bool close, const std::string& contentType,
                     int contentLen, const std::string& body, http::HttpResponse* resp);

    // 获取历史最高在线人数
    int getMaxOnline() const
    {
        return maxOnline_.load();
    }

    // 获取当前在线人数
    int getCurOnline() const
    {
        return onlineUsers_.size();
    }

    void updateMaxOnline(int online)
    {
        maxOnline_ = std::max(maxOnline_.load(), online);
    }

    // 获取用户总数
    //q 业务层-->MySQL
    int getUserCount()
    {
        //q 查询语句，统计users表中的总记录数
        std::string sql = "SELECT COUNT(*) as count FROM users";

        //q executrQuery是查训方法
        sql::ResultSet* res = mysqlUtil_.executeQuery(sql);
        if (res->next())
        {
            return res->getInt("count");
        }
        return 0;
    }
    
private:
    friend class EntryHandler;
    friend class LoginHandler;
    friend class RegisterHandler;
    friend class MenuHandler;
    friend class AiGameStartHandler;
    friend class LogoutHandler;
    friend class AiGameMoveHandler;
    friend class GameBackendHandler;

private:
    enum GameType
    {
        NO_GAME = 0,
        MAN_VS_AI = 1,
        MAN_VS_MAN = 2
    };
    // 实际业务制定由GomokuServer来完成
    // 需要留意httpServer_提供哪些接口供使用
    //q 网络框架的核心
    http::HttpServer                                 httpServer_;
    //q 数据库管理
    http::MysqlUtil                                  mysqlUtil_;
    // userId -> AiBot
    std::unordered_map<int, std::shared_ptr<AiGame>> aiGames_;
    //q 多线程环境必须加锁
    std::mutex                                       mutexForAiGames_;
    // userId -> 是否在游戏中
    //q 用户在线表
    std::unordered_map<int, bool>                    onlineUsers_;
    std::mutex                                       mutexForOnlineUsers_; 
    // std::shared_mutex                                mutexForOnlineUsers_;//q 将原来的自动管理锁-->读写锁 这样,读:不会互斥 写:单线程独占
    // 最高在线人数
    std::atomic<int>                                 maxOnline_;
};