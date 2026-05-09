#include <string>
#include <iostream>
#include <muduo/net/TcpServer.h>
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include "GomokuServer.h"

int main(int argc, char* argv[])//q argc参数个数，argv参数数组
{
  LOG_INFO << "pid = " << getpid();//q 获取当前进程PID
  //q 在Linux中：杀进程，查日志，调试 均需要PID
  
  std::string serverName = "HttpServer";//q 定义服务器的名字
  int port = 80;//q 设置端口
  
  // 参数解析
  int opt;
  const char* str = "p:";
  while ((opt = getopt(argc, argv, str)) != -1)//q getopt,Linux参数解析函数，挨个读取用户输入的参数
  {
    switch (opt)
    {
      case 'p':
      {
        port = atoi(optarg);//q atoi：将字符串转化为int 
        //q port是用户自定义端口
        break;
      }
      default:
        break;
    }
  }
  
  muduo::Logger::setLogLevel(muduo::Logger::WARN);//q moduo的日志打印系统，
  //q 设置为只打印WARN及以上等级的日志: WARN ERROR FATAL
  GomokuServer server(port, serverName);//q 创建服务器对象，参数：端口号 服务器名字
  server.setThreadNum(4);//q* 设置线程数位4，moduo的经典结构one loop per thread，一个线程是一个eventloop
  server.start();//启动服务器
}
