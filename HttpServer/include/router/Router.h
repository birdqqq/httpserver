#pragma once
#include <iostream>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <regex>
#include <vector>

#include "RouterHandler.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
namespace router
{

// 选择注册对象式的路由处理器还是注册回调函数式的处理器取决于处理器执行的复杂程度
// 如果是简单的处理可以注册回调函数，否则注册对象式路由处理器(对象中可封装多个相关函数)
// 二者注册其一即可
class Router
{
public:
    using HandlerPtr = std::shared_ptr<RouterHandler>;//q 保存handler对象的指针，用于保存：loginhandler menuhandler registerhandler等类对象处理器（复杂业务）
    using HandlerCallback = std::function<void(const HttpRequest &, HttpResponse *)>;//q 函数处理器 支持lambda表达式（简单业务）

    // 路由键（请求方法 + URI）
    //q 就是路由表的key,分为两部分：method + path eg:GET /Login 
    struct RouteKey
    {
        HttpRequest::Method method;
        std::string path;

        //q 运算符重载，比较两请求的key是否相同：要求方法和路径同时相等
        bool operator==(const RouteKey &other) const
        {
            return method == other.method && path == other.path;
        }
    };

    // 为RouteKey 定义哈希函数
    struct RouteKeyHash
    {
        // size_t operator()(const RouteKey& key) const
        // {
        //     return std::hash<int>{}(static_cast<int>(key.method)) ^
        //            std::hash<std::string>{}(key.path);
        // }
        //q 重载 () 运算符，使得RouteKeyHash可以被调用
        size_t operator()(const RouteKey &key) const
        {
            //q 将key中的method（方法）（枚举类型）转换为显式类型转换为int
            size_t methodHash = std::hash<int>{}(static_cast<int>(key.method));
            //q 将key中的path（路径）用标准库中的字符串哈希函数计算哈希值
            size_t pathHash = std::hash<std::string>{}(key.path);
            //q 计算key对应的哈希值
            return methodHash * 31 + pathHash;
        }
    };

    // 注册路由处理器
    void registerHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler);

    // 注册回调函数形式的处理器
    void registerCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback);

    // 注册动态路由处理器
    void addRegexHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
    {
        std::regex pathRegex = convertToRegex(path);
        regexHandlers_.emplace_back(method, pathRegex, handler);
    }

    // 注册动态路由处理函数
    void addRegexCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback)
    {
        std::regex pathRegex = convertToRegex(path);
        regexCallbacks_.emplace_back(method, pathRegex, callback);//q emplace_back直接在vector中构造添加对象比push_back高效
    }

    // 处理请求
    bool route(const HttpRequest &req, HttpResponse *resp);

private:
//q 动态路由匹配核心：
    //q 将动态url转换为正则规则
    std::regex convertToRegex(const std::string &pathPattern)
    { // 将路径模式转换为正则表达式，支持匹配任意路径参数
        //q 将添加的路由转换为正则表达式，即替换占位符/:... eg:/user/:name --> /user/([^/]+)  R""表示c++原始字符串，拒绝转义！
        std::string regexPattern = "^" + std::regex_replace(pathPattern, std::regex(R"(/:([^/]+))"), R"(/([^/]+))") + "$";
        return std::regex(regexPattern);
    }

    // 提取路径参数
    void extractPathParameters(const std::smatch &match, HttpRequest &request)
    //q match放的就是 :id :name eg: /user/123中的123 std::smatch是正则匹配结果 [0]是完整匹配，即user/123,从[1]开始是每个匹配的结果,即match[1]=123
    {
        // Assuming the first match is the full path, parameters start from index 1
        //q 遍历找出每一个参数
        for (size_t i = 1; i < match.size(); ++i)
        {
            //q 把匹配的参数放进requset中
            request.setPathParameters("param" + std::to_string(i), match[i].str());
        }
    }

private:
    struct RouteCallbackObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerCallback callback_;
        RouteCallbackObj(HttpRequest::Method method, std::regex pathRegex, const HandlerCallback &callback)
            : method_(method), pathRegex_(pathRegex), callback_(callback) {}
    };

    struct RouteHandlerObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerPtr handler_;
        RouteHandlerObj(HttpRequest::Method method, std::regex pathRegex, HandlerPtr handler)
            : method_(method), pathRegex_(pathRegex), handler_(handler) {}
    };

    //q handler_为针对复杂业务的类处理器匹配 callback_为针对简单业务的函数处理器匹配
    //q unordered_map<key类型，value类型，哈希函数>！ 
    std::unordered_map<RouteKey, HandlerPtr, RouteKeyHash>      handlers_;       // 精准匹配
    std::unordered_map<RouteKey, HandlerCallback, RouteKeyHash> callbacks_; // 精准匹配
    std::vector<RouteHandlerObj>                                regexHandlers_;     // 正则匹配
    std::vector<RouteCallbackObj>                               regexCallbacks_;   // 正则匹配
};

//q router 是一个字典 + 路由规则匹配器

} // namespace router
} // namespace http