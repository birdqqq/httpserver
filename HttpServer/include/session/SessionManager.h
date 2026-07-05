#pragma once

#include "SessionStorage.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"
#include <memory>
#include <random>

namespace http
{
namespace session
{

class SessionManager
{
public:
    explicit SessionManager(std::unique_ptr<SessionStorage> storage);

    // 从请求中获取或创建会话
    std::shared_ptr<Session> getSession(const HttpRequest& req, HttpResponse* resp);
    
     // 销毁会话
    void destroySession(const std::string& sessionId);

    // 清理过期会话
    void cleanExpiredSessions();

    // 更新会话
    void updateSession(std::shared_ptr<Session> session)
    {
        storage_->save(session);
    }
private:
    std::string generateSessionId();
    std::string getSessionIdFromCookie(const HttpRequest& req);
    void setSessionCookie(const std::string& sessionId, HttpResponse* resp);

private:
    std::unique_ptr<SessionStorage> storage_;//q session仓库 存的是sessionID-->Session对象  类似于 abc123-->用户qhn
    std::mt19937 rng_; // 用于生成随机会话id q mt19937类型（梅森旋转算法） 伪随机数生成器 
    //                                      q 在构造中使用random_device生成真随机数作为种子使用rng_shengcheng SessionID 见文档
};

} // namespace session
} // namespace http