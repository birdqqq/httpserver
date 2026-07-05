#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>

namespace http
{

namespace session
{

class SessionManager;

//q 本质是服务器中的用户档案

class Session : public std::enable_shared_from_this<Session>//q 见文档  使Session安全的shared_ptr<自己>
{
public:
    Session(const std::string& sessionId, SessionManager* sessionManager, int maxAge = 3600); // 默认1小时过期
    
    const std::string& getId() const 
    { return sessionId_; }

    bool isExpired() const;//q  判断是否过期
    void refresh(); // 刷新过期时间

    void setManager(SessionManager* sessionManager) //q 重新设置管理器 一般用于重新加载时
    { sessionManager_ = sessionManager; }

    SessionManager* getManager() const 
    { return sessionManager_; }

    // 数据存取
    void setValue(const std::string&key, const std::string&value);
    std::string getValue(const std::string&key) const;
    void remove(const std::string&key);
    void clear();
private:
    std::string                                  sessionId_;
    std::unordered_map<std::string, std::string> data_; //q 存用户数据的地方
    std::chrono::system_clock::time_point        expiryTime_;
    int                                          maxAge_; // 过期时间（秒）
    SessionManager*                              sessionManager_;
};

} // namespace session
} // namespace http