#ifndef HTTP_CONN_H_
#define HTTP_CONN_H_

#include <sys/epoll.h>
#include <unistd.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>

class http_conn
{
public:
  static int m_epollfd;    // 所有socket上的事件都被注册到一个epoll上
  static int m_user_count; // 统计用户数量
  http_conn() {}
  ~http_conn() {}

  void process(); // 处理客户请求
  void init(int sockfd, const sockaddr_in &addr);
  void close_conn();
  bool read();  // 非阻塞读
  bool write(); // 非阻塞写

private:
  int m_sockfd;          // 该http连接的socket
  sockaddr_in m_address; // 通信socket地质
};

#endif // !HTTP_CONN_H_