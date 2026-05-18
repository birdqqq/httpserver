#include "../include/handlers/LoginHandler.h"

//q 登录逻辑 当客户在浏览器中输入： 用户名：qq 密码：123 点击登录后

void LoginHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    // 处理登录逻辑
    // 验证 contentType
    auto contentType = req.getHeader("Content-Type");//q content-Type：application/json 内容格式是json contentType=application/json
    if (contentType.empty() || contentType != "application/json" || req.getBody().empty())
    {
        LOG_INFO << "content" << req.getBody();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(0);
        resp->setBody("");
        return;
    }

    // JSON 解析使用 try catch 捕获异常
    try
    {
        json parsed = json::parse(req.getBody()); //q parse()将json字符串转换为json对象 类似于std::map
        std::string username = parsed["username"];//q 作用：username="qq"
        std::string password = parsed["password"];//q  password=123
        // 验证用户是否存在
        int userId = queryUserId(username, password);//q 查询数据库 验证用户名密码正确与否 函数queryUserId()会返回匹配行的行号
        if (userId != -1)//q 找到了
        {
            // 获取会话
            auto session = server_->getSessionManager()->getSession(req, resp);
            // 会话都不是同一个会话，因为会话判断是不是同一个会话是通过请求报文中的cookie来判断的
            // 所以不同页面的访问是不可能是相同的会话的，只有该页面前面访问过服务端，才会有会话记录
            // 那么判断用户是否在其他地方登录中不能通过会话来判断
            
            //q getSession()通过req中的Cookie取出SessionId，在getSessionManager()中找session项;找到了返回;没找到创建空session
            //q 并将新SessionId写进Cookie

            // 在会话中存储用户信息
            session->setValue("userId", std::to_string(userId));
            session->setValue("username", username);
            session->setValue("isLoggedIn", "true");
            // if (server_->onlineUsers_.find(userId) == server_->onlineUsers_.end() || server_->onlineUsers_[userId] == false)
            //q 在线用户表中没有当前用户 || 当前用户已经下线false    onlineUsers_是一个map<int,bool>   表示是否首次登陆或离线后登录

            //q 读写锁 改编
            //q 读写锁 共享锁 安全的读 原代码if(...)读的时候直接读 没有锁
            bool isOnline = false;//声明一个bool表示用户是否在线
            {
                std::shared_lock<std::shared_mutex> readLock(server_->mutexForOnlineUsers_);
                //  ↑共享锁，多线程可以同时读

                auto it = server_->onlineUsers_.find(userId);
                isOnline = (it != server_->onlineUsers_.end() && it->second == true);
            }
            if(!isOnline)//q 如果当前用户不在线则可以登录
            {
                //q 原自动管理锁
                // {
                //     std::lock_guard<std::mutex> lock(server_->mutexForOnlineUsers_);//加锁防止多个用户同时修改onlineUsers_,产生竞争
                //     server_->onlineUsers_[userId] = true;
                // }
                //q 加大括号的原因 因为只有修改onlineUsers_时才需要加锁 所以将这两步用大括号括起来 修改完onlineUsers_自动解锁 后面的步骤不需要加锁！
                
                //q 读写锁
                {
                // 写操作：独占锁，只有一个线程能写
                std::unique_lock<std::shared_mutex> writeLock(server_->mutexForOnlineUsers_);
                server_->onlineUsers_[userId] = true;
                }  // 自动解锁

                // 更新历史最高在线人数
                server_->updateMaxOnline(server_->onlineUsers_.size());
                // 用户存在登录成功
                // 封装json 数据。
                json successResp;//q 创建json对象
                successResp["success"] = true;
                successResp["userId"] = userId;
                std::string successBody = successResp.dump(4); //q 首行缩进4

                resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");//q 请求行
                resp->setCloseConnection(false);//q 长连接?
                resp->setContentType("application/json");//q 报文格式 json
                resp->setContentLength(successBody.size());//q body长度
                resp->setBody(successBody);//q body内容 successBody= "{\n    \"success\": true,\n    \"userId\": 5\n}"
                return;
            }
            else//q 表示当前用户在线
            {
                // FIXME: 当前该用户正在其他地方登录中，将原有登录用户强制下线更好
                // 不允许重复登录，
                json failureResp;
                failureResp["success"] = false;
                failureResp["error"] = "账号已在其他地方登录";
                std::string failureBody = failureResp.dump(4);

                resp->setStatusLine(req.getVersion(), http::HttpResponse::k403Forbidden, "Forbidden");
                resp->setCloseConnection(true);
                resp->setContentType("application/json");
                resp->setContentLength(failureBody.size());
                resp->setBody(failureBody);
                return;
            }
        }
        else // 账号密码错误，请重新登录
        {
            // 封装json数据
            json failureResp;
            failureResp["status"] = "error";
            failureResp["message"] = "Invalid username or password";
            std::string failureBody = failureResp.dump(4);

            resp->setStatusLine(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized");
            resp->setCloseConnection(false);
            resp->setContentType("application/json");
            resp->setContentLength(failureBody.size());
            resp->setBody(failureBody);
            return;
        }
    }
    catch (const std::exception &e)
    {
        // 捕获异常，返回错误信息
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
        return;
    }
}

int LoginHandler::queryUserId(const std::string &username, const std::string &password)
{
    // 前端用户传来账号密码，查找数据库是否有该账号密码
    // 使用预处理语句, 防止sql注入
    std::string sql = "SELECT id FROM users WHERE username = ? AND password = ?";//q mySQL查询语句 表示:
    //q                查询   id 从   users表中 查找 username=?  和 password=?的行 返回这行的 id  
    // std::vector<std::string> params = {username, password};
    sql::ResultSet* res = mysqlUtil_.executeQuery(sql, username, password);//q ResultSet是数据库的封装类 将数据库返回的数据封装为c++对象
                                                                           //q 方便操作，res指向表头（第一行的前一行）
    if (res->next())//q res->next()表示表头的下一行 即 第一个匹配结果
    {
        int id = res->getInt("id");
        return id;
    }
    // 如果查询结果为空，则返回-1
    return -1;
}

