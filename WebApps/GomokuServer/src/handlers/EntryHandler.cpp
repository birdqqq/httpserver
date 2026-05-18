#include "../include/handlers/EntryHandler.h"

void EntryHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    // 因为是get请求，请求的url也拿到了，我们就可以直接返回响应了
    std::string reqFile;
    reqFile.append("../WebApps/GomokuServer/resource/entry.html");
    FileUtil fileOperater(reqFile);//q fileOperation将reqFile路径中的文件基本信息记录下来 包括 路径 文件大小 文件是否存在(bool) 文件指针
    if (!fileOperater.isValid())//q 文件不存在就返回 404
    {
        LOG_WARN << reqFile << " not exist";
        fileOperater.resetDefaultFile(); // 404 NOT FOUND
    }

    std::vector<char> buffer(fileOperater.size());//q 创建文件大小的内存
    fileOperater.readFile(buffer); // 读出文件数据 q 从磁盘中读取文件内容到buffer中
    std::string bufStr = std::string(buffer.data(), buffer.size());//q buffer转string 因为http响应体body是string 
    
    resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");//q 设置状态行 http版本 状态码 OK
    resp->setCloseConnection(false);//q 是否关闭连接
    resp->setContentType("text/html");//q 告诉浏览器是html文件 浏览器就会渲染网页 而不是下载文件
    resp->setContentLength(bufStr.size());//q 设置内容长度浏览器明白body有多长
    resp->setBody(bufStr);//q 填入响应体
}
