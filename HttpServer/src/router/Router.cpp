#include "../../include/router/Router.h"
#include <muduo/base/Logging.h>

namespace http
{
namespace router
{
//q 使用类对象处理器 对于复杂业务 HnadlerPtr智能指针 指向不同处理类
void Router::registerHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
{
    RouteKey key{method, path};
    handlers_[key] = std::move(handler);
}
//q 使用哈桑农户处理器 对于简单业务 HandlerCallback函数容器 存储处理函数支持lambda表达式
void Router::registerCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback)
{
    RouteKey key{method, path};
    callbacks_[key] = std::move(callback);
}

bool Router::route(const HttpRequest &req, HttpResponse *resp)
{
    RouteKey key{req.method(), req.path()};

    // 查找处理器 q 复杂业务对应
    auto handlerIt = handlers_.find(key);
    if (handlerIt != handlers_.end())
    {
        handlerIt->second->handle(req, resp);
        return true;
    }

    // 查找回调函数 q 简单业务对应
    auto callbackIt = callbacks_.find(key);
    if (callbackIt != callbacks_.end())
    {
        callbackIt->second(req, resp);
        return true;
    }

    // 查找动态路由处理器 q 复杂业务的类处理器
    for (const auto &[method, pathRegex, handler] : regexHandlers_)//q 结构化绑定 详见文档
    {
        std::smatch match;//q 用于存放匹配的结果，路径参数 eg:/user/123中的123
        std::string pathStr(req.path());//q 路径 转换为 string类型
        // 如果方法匹配并且动态路由匹配，则执行处理器
        if (method == req.method() && std::regex_match(pathStr, match, pathRegex))//q regex_match()详见文档
        {
            // Extract path parameters and add them to the request
            HttpRequest newReq(req); // 因为这里需要用这一次所以是可以改的 q 需要将正则表达式捕获的路径参数加到req中所以需要new一个新的req，原来是const
            extractPathParameters(match, newReq);
            
            handler->handle(newReq, resp);//使用对应类处理器中的handle函数处理请求，handler是指向类处理器的指针
            return true;
        }
    }

    // 查找动态路由回调函数 q 简单业务的函数处理器，与上面的类处理器流程一致
    for (const auto &[method, pathRegex, callback] : regexCallbacks_)
    {
        std::smatch match;
        std::string pathStr(req.path());
        // 如果方法匹配并且动态路由匹配，则执行回调函数
        if (method == req.method() && std::regex_match(pathStr, match, pathRegex))
        {
             // Extract path parameters and add them to the request
            HttpRequest newReq(req); // 因为这里需要用这一次所以是可以改的
            extractPathParameters(match, newReq);

            // callback(req, resp);
            callback(newReq,resp);//q 路径参数是存在newReq中的
            return true;
        }
    }

    return false;
}

} // namespace router
} // namespace http