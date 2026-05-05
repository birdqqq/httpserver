#include "../../include/http/HttpContext.h"

using namespace muduo;
using namespace muduo::net;

//state_ :是一个状态机 用于管理当前解析的的状态：解析请求行 kExpectRequestLine 
//                                               解析请求头 kExpectHeaders
//                                               解析请求体 kExpectBody
//                                               解析完成   kGotall

namespace http
{

// 将报文解析出来将关键信息封装到HttpRequest对象里面去
bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime) //返回值为一个bool值表示操作成功或者失败
{
    bool ok = true; // 解析每行请求格式是否正确？
    bool hasMore = true;  //是否还有解析的内容？
    while (hasMore)
    {
        //解析请求行
        if (state_ == kExpectRequestLine) //当前状态为 解析请求行
        {
            const char *crlf = buf->findCRLF(); // 注意这个返回值边界可能有错
            //findCRLF表示寻找结束标志：“/r/n”回车符和换行符 返回的地址是“/r”的地址
            if (crlf)//地址不为空 则找到了请求行结束的位置
            {
                ok = processRequestLine(buf->peek(), crlf);
                //peek()表示回到可读的地址（回到开始的地方） 
                // processRequestLine（begin,end）解析该范围内的数据（请求行） 返回bool值解析成功true 解析失败false
                if (ok)//如果解析成功
                {
                    request_.setReceiveTime(receiveTime);//将请求时间设置到HttpRequest类中，便于日志记录、性能分析等
                    buf->retrieveUntil(crlf + 2);//retrieveUntil()将读指针移动到指定位置，crlf+2 crlf存储的是“/r”地址
                                                 //crlf+2表示将读指针移动到“/n”的下一位，即请求头的第一位，后续继续解析请求头
                    state_ = kExpectHeaders;//请求行解析完毕 将状态设置为“解析请求头”进行下一步解析
                }
                else//解析失败 
                {
                    hasMore = false; //没有可解析的内容 终止循环
                }
            }
            else // crlf地址为空 
            {
                hasMore = false; //证明请求数据错误 停止循环
            }
        }
        //解析请求头
        else if (state_ == kExpectHeaders) 
        {
            const char *crlf = buf->findCRLF(); //使用clrf接收“/r/n”地址中的“/r”的地址
            if (crlf) 
            {
                const char *colon = std::find(buf->peek(), crlf, ':');//使用find函数在（可读的起始位置，结束位置）范围内寻找“:”
                                                                      //“:”：请求头的格式：字段名：字段值，eg: host:example.com
                if (colon < crlf)//colon在可读位置与结尾位置之间，说明找到了请求头
                {
                    request_.addHeader(buf->peek(), colon, crlf);// addHeader(a,b,c)a和b之间存进Key中 b和c之间存进Value中
                                                                 // 上述例子：将 host 存进key 将 example.com 存进value中 
                }
                else if (buf->peek() == crlf)
                { 
                    // 空行，结束Header
                    // 根据请求方法和Content-Length判断是否需要继续读取body
                    if (request_.method() == HttpRequest::kPost || 
                        request_.method() == HttpRequest::kPut) // request.method()获取当前请求使用的方法，
                                                                //判断是否为POST方法或者PUT方法
                    {
                        std::string contentLength = request_.getHeader("Content-Length");//获取请求头中的“Content-Length”字段的值
                                                                                         //放进contentLength中，这个字段表示请求体的字节长度
                        if (!contentLength.empty())//请求体不为空
                        {
                            request_.setContentLength(std::stoi(contentLength));//stoi将string转化为整数int或者size_t（一种数据类型，无符号的大小计数器）
                                                                                //setContentLength将转化的整数存进请求对象中（contentLength中）
                            if (request_.contentLength() > 0)//如果请求体的大小不为零，开始解析请求体的内容
                            {
                                state_ = kExpectBody;//状态机更新为解析请求体
                            }
                            else// 请求体为空
                            {
                                state_ = kGotAll;//将状态机设置为 解析完成 
                                hasMore = false;//已经没有解析内容了 终止循环
                            }
                        }
                        else//如果请求头中Content-Length字段为空，表示没有请求体，语法错误
                        {
                            // POST/PUT 请求没有 Content-Length，是HTTP语法错误
                            ok = false; //请求的格式错误
                            hasMore = false; //没有需要解析的内容了
                        }
                    }
                    else //如果请求的方法不是POST 或者 PUT方法 ，即请求的方法是GET HEAD DELETE等方法，这些方法没有请求体！
                    {
                        // GET/HEAD/DELETE 等方法直接完成（没有请求体）
                        state_ = kGotAll; //状态为解析完成
                        hasMore = false;  //没有需要解析的内容了
                    }
                }
                else //当buf->peek()!=crlf，此时请求头已经解析完了，证明请求格式错误
                {
                    ok = false; // Header行格式错误
                    hasMore = false;
                }
                buf->retrieveUntil(crlf + 2); // 开始读指针指向下一行数据
            }
            else //crlf==null 说明解析已经完成了
            {
                hasMore = false;//不需要在进行解析
            }
        }
        else if (state_ == kExpectBody)//解析请求体
        {
            // 检查缓冲区中是否有足够的数据
            if (buf->readableBytes() < request_.contentLength())//readableBytes()检查缓冲区中的可读数据的长度（已经接收但没有处理）
                                                                //contentLength()请求中的请求体的字节长度
                                                                //如果已经接受的数据长度小于请求体中的数据长度，证明数据还没有接收完！
            {
                hasMore = false; // 数据不完整，等待更多数据 
                return true;//当前操作成功 但是数据不完整
            }
            //经过上面的if 表示缓冲区中的数据包含了请求体的全部内容！
            // 只读取 Content-Length 指定的长度
            std::string body(buf->peek(), buf->peek() + request_.contentLength());//使用指针范围构造字符串，
                                                                                  //将请求体中的数据打包放进字符串body中
            request_.setBody(body);//setBody()将body的内容放进请求体的正文中。

            // 准确移动读指针
            buf->retrieve(request_.contentLength());//使用retrieve()移除长度为request_.contentLength()的数据，清理了缓存区，避免对已处理数据的重复读取
                                                    //相当于将读指针向前移动request_.contentLength()位，避免解析后续请求时出现错误

            state_ = kGotAll;//将请求中的请求体body放进了解析后的请求体的正文中，并将这些数据从buf的是缓存区中删除，此时当前请求已全部解析完成
                             //更新状态
            hasMore = false; //不需要在进行解析，终止循环
        }
    }
    return ok; // ok为false代表报文语法解析错误 true表示解析正常，此操作成功
}

// 解析请求行  格式必须是严格按照：METHOD URL VERSION 例子：<方法> <路径>?<参数1>=<值1>&<参数2>=<值2> <HTTP版本>
                                                        //    GET /search?q=apple&limit=10 HTTP/1.1
bool HttpContext::processRequestLine(const char *begin, const char *end)
{
    bool succeed = false; //初始化解析状态
    const char *start = begin;
    const char *space = std::find(start, end, ' ');//使用space接受“空格”的地址 
    if (space != end && request_.setMethod(start, space))
    {
        start = space + 1; //更新起始位置
        space = std::find(start, end, ' '); //寻找第二个空格
        if (space != end)
        {
            const char *argumentStart = std::find(start, space, '?');
            if (argumentStart != space) // 请求带参数
            {
                request_.setPath(start, argumentStart); // 注意这些返回值边界 提取路径信息
                request_.setQueryParameters(argumentStart + 1, space); //提取查询参数
            }
            else // 请求不带参数
            {
                request_.setPath(start, space); //请求行中没有参数 证明两空格中是路径 直接提取路径信息
            }

            start = space + 1;
            succeed = ((end - start == 8) && std::equal(start, end - 1, "HTTP/1."));//请求行的版本号必须是8为！
                                                                                    //并且前七位必须是：HTTP/1.，表示该服务器兼容http1.0和1.1两版本 
            if (succeed)
            {
                if (*(end - 1) == '1')//*(end-1)对指针end-1解引用 取地址指向的值
                {
                    request_.setVersion("HTTP/1.1");
                }
                else if (*(end - 1) == '0')
                {
                    request_.setVersion("HTTP/1.0");
                }
                else
                {
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}

} // namespace http