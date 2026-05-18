#include "../../include/http/HttpRequest.h"

namespace http
{

//q httprequest主要工作就是将接收到的请求 字节流拆成不同的部分 method path ... 总结就是拆字符串 

void HttpRequest::setReceiveTime(muduo::Timestamp t)//设置请求到达时间
{
    receiveTime_ = t;
}

bool HttpRequest::setMethod(const char *start, const char *end)
{
    assert(method_ == kInvalid);//q 断言 规定method必须时kinvalid ，只有成立才会进行后续程序 为了确保setMethod只被调用一次！-->一个请求只能解析一次method
                                //q  若不是kinvalid说明setMethod已经被调用过了
    std::string m(start, end);  // [start, end) q 左闭右开逻辑
    if (m == "GET")
    {
        method_ = kGet;
    }
    else if (m == "POST")
    {
        method_ = kPost;
    }
    else if (m == "PUT")
    {
        method_ = kPut;
    }
    else if (m == "DELETE")
    {
        method_ = kDelete;
    }
    else if (m == "OPTIONS")
    {
        method_ = kOptions;
    }
    else
    {
        method_ = kInvalid;
    }

    return method_ != kInvalid;//q 更新方法 返回true 否则返回false
}

void HttpRequest::setPath(const char *start, const char *end)
{
    path_.assign(start, end);//q 传入的参数是两个指针 所以表示截取从start开始 到end结束的字符串赋值给path_，这次是在assign内部自己算字符串长度 end-start
}

void HttpRequest::setPathParameters(const std::string &key, const std::string &value)
{
    pathParameters_[key] = value;
}

std::string HttpRequest::getPathParameters(const std::string &key) const
{
    auto it = pathParameters_.find(key);
    if (it != pathParameters_.end())
    {
        return it->second;
    }
    return "";
}

std::string HttpRequest::getQueryParameters(const std::string &key) const
{
    auto it = queryParameters_.find(key);
    if (it != queryParameters_.end())
    {
        return it->second;
    }
    return "";
}

//q 举例：GET /search?name=小明&age=18&city=北京 HTTP/1.1

// 这是从问号后面分割参数 q 解析的是"name=小明&age=18&city=北京"这部分
void HttpRequest::setQueryParameters(const char *start, const char *end)
{
    std::string argumentStr(start, end);//q 截取这部分字符串用于解析 将字节流char*转成string 方便操作
    //q 双指针 
    std::string::size_type pos = 0;//q 当前&位置
    std::string::size_type prev = 0;//q 上一个&位置

    // 按 & 分割多个参数
    while ((pos = argumentStr.find('&', prev)) != std::string::npos) // npos：表示string中的极大值，类似于int 中的INT_MAX
    //q find('&',prev) 在argumentStr中寻找'&',从prev位置开始找 找到就返回对应位置 找不到返回npos
    //q 以第一次循环举例：
    //q 第一次找到name=小明
    {
        std::string pair = argumentStr.substr(prev, pos - prev);//q pair="name=小明"
        std::string::size_type equalPos = pair.find('=');//q 寻找"="的位置

        if (equalPos != std::string::npos)//q 找到了
        {
            std::string key = pair.substr(0, equalPos);//q key=name
            std::string value = pair.substr(equalPos + 1);//q value=小明 substr()一个参数表示从该位置开始到字符串末尾结束
            queryParameters_[key] = value;//q 存到map中
        }

        prev = pos + 1;//q 更新prev
    }

    // 处理最后一个参数
    //q 因为最后一个参数没有&分割 需要单独处理（执行到这一步说明前n-1个参数已经解析完毕了，该最后一个了）
    std::string lastPair = argumentStr.substr(prev);//q 最后一次循环时更新prev只想最后一个参数的第一位-->'c'的位置
    std::string::size_type equalPos = lastPair.find('=');
    if (equalPos != std::string::npos)
    {
        std::string key = lastPair.substr(0, equalPos);
        std::string value = lastPair.substr(equalPos + 1);
        queryParameters_[key] = value;
    }
}

//q 解析请求头 三个指针： 开始位置 冒号位置 结束位置    colon表示冒号    
//q eg：请求头： Content-Type: application/json\r\n 需要解析为： headers_["Content-Type"] = "application/json"
void HttpRequest::addHeader(const char *start, const char *colon, const char *end)
{
    std::string key(start, colon);//q 注意左闭右开  截取key --> Content-Type
    ++colon;                      //q 跳过冒号
    while (colon < end && isspace(*colon))//q 注意 跳过毛冒号之后时空格 所以要跳过全部空格 使colon指向value的起始位置！
    {
        ++colon;
    }
    std::string value(colon, end);//q 截取value
    while (!value.empty() && isspace(value[value.size() - 1])) // 消除尾部空格 q 因为json中加空格合法 所以value可能是"application/json     "
                                                               //q 此时end指向的时最后一个空格，所以需要删除尾部的所有空格，使用while一个一个删除
    {
        value.resize(value.size() - 1);
    }
    headers_[key] = value;//q 解析完毕：headers_["Content-Type"] = "application/json"
}

std::string HttpRequest::getHeader(const std::string &field) const//q 对外接口 取请求头
{
    std::string result;
    auto it = headers_.find(field);
    if (it != headers_.end())
    {
        result = it->second;
    }
    return result;
}

void HttpRequest::swap(HttpRequest &that)
{
    std::swap(method_, that.method_);
    std::swap(path_, that.path_);
    std::swap(pathParameters_, that.pathParameters_);
    std::swap(queryParameters_, that.queryParameters_);
    std::swap(version_, that.version_);
    std::swap(headers_, that.headers_);
    std::swap(receiveTime_, that.receiveTime_);
}

} // namespace http