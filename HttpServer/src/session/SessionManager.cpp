#include"../include/session/SessionManager.h"
#include <iomanip>
#include <iostream>
#include <sstream>

namespace http
{
namespace session
{

// 初始化会话管理器，设置会话存储对象和随机数生成器
SessionManager::SessionManager(std::unique_ptr<SessionStorage> storage)
    : storage_(std::move(storage)) 
    , rng_(std::random_device{}()) // 初始化随机数生成器，用于生成随机的会话ID  q 真随机数设备random_device，创建临时匿名对象 用完即毁
{}

// 从请求中获取或创建会话，也就是说，如果请求中包含会话ID，则从存储中加载会话，否则创建一个新的会话
std::shared_ptr<Session> SessionManager::getSession(const HttpRequest& req, HttpResponse* resp)
{   
    std::string sessionId = getSessionIdFromCookie(req);//q 从req中的cookie中获取SssionId
    
    std::shared_ptr<Session> session;//q 生成一个空的session指针

    if (!sessionId.empty())//找到sessionid了
    {
        session = storage_->load(sessionId);//q 通过sessionid在仓库中找对应的session并返回
    }

    if (!session || session->isExpired())//q session为空 或 session过期 isExpired()检查session是否过期的函数 
                                         //q 过期原因：用户注销 服务器重启 存储清理（Redis）Cookie丢失。。。 
    {
        sessionId = generateSessionId();//q 重新获取sessionid
        session = std::make_shared<Session>(sessionId, this);//q this指的是SessionManager对象 重新生成一个session 传入session和sessionmanager*
        setSessionCookie(sessionId, resp);//q 设置sessionid
    }
    else 
    {
        session->setManager(this); // 为现有会话设置管理器 q session
    }

    session->refresh();//q 刷新过期时间
    storage_->save(session);  // 这里可能有问题，需要确保正确保存会话
    return session;
}

// 生成唯一的会话标识符，确保会话的唯一性和安全性
std::string SessionManager::generateSessionId()
{
    std::stringstream ss;//q 字节串流 见文档
    std::uniform_int_distribution<> dist(0, 15);//q 设置随机数的范围 确保随机数再0-15之间均匀分布 0-15来表示十六进制的数

    // 生成32个字符的会话ID，每个字符是一个十六进制数字
    for (int i = 0; i < 32; ++i)
    {
        ss << std::hex << dist(rng_);//q std::hex表示将输入转变为16进制字符 dist(rng_)随机生成0-15的数字
    }
    return ss.str();//q 32位的十六进制字符串
}

void SessionManager::destroySession(const std::string& sessionId)//q 删除session
{
    storage_->remove(sessionId);
}

void SessionManager::cleanExpiredSessions()
{
    // 注意：这个实现依赖于具体的存储实现
    // 对于内存存储，可以在加载时检查是否过期
    // 对于其他存储的实现，可能需要定期清理过期会话
}

std::string SessionManager::getSessionIdFromCookie(const HttpRequest& req)//q 从cookie中获取sessionid
{
    std::string sessionId;
    std::string cookie = req.getHeader("Cookie"); //q cookie长这样：Cookie: sessionId=abc123; theme=dark; userId=42

    if (!cookie.empty())
    {
        size_t pos = cookie.find("sessionId=");
        if (pos != std::string::npos)//q npos表示无效位置
        {
            pos += 10; // 跳过"sessionId="
            size_t end = cookie.find(';', pos);
            if (end != std::string::npos)//q 有 ; 表示cookie中不只存储了sessionid一个值
            {
                sessionId = cookie.substr(pos, end - pos);
            }
            else//q cookie中只存储了sessionid一个值 
            {
                sessionId = cookie.substr(pos);//q 从pos位置取到最后
            }
        }
    }
    
    return sessionId;
}

void SessionManager::setSessionCookie(const std::string& sessionId, HttpResponse* resp)
{
    // 设置会话ID到响应头中，作为Cookie
    std::string cookie = "sessionId=" + sessionId + "; Path=/; HttpOnly";
    resp->addHeader("Set-Cookie", cookie);
}

} // namespace session
} // namespace http