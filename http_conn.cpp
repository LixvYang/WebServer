#include "http_conn.h"

int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

// 设置文件描述符非阻塞
void setnonblocking(int fd)
{
  int old_flag = fcntl(fd, F_GETFL);
  int new_flag = old_flag | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_flag);
}

// 向epoll 中监听需要添加的文件描述符
void addfd(int epollfd, int fd, bool one_shot)
{
  epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLRDHUP; 

  if (one_shot)
  {
    event.events |= EPOLLONESHOT;
  }
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  // 设置文件描述符非阻塞
  setnonblocking(fd);
}

// 移除文件描述符
void removefd(int epollfd, int fd)
{
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
  close(fd);
}

// 修改
void modfd(int epollfd, int fd, int ev)
{
  epoll_event event;
  event.data.fd = fd;
  event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 初始化链接
void http_conn::init(int sockfd, const sockaddr_in &addr)
{
  m_sockfd = sockfd;
  m_address = addr;
  // 端口复用
  int reuse = 1;
  setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  
  // 添加到epoll对象中
  addfd(m_epollfd, sockfd, true);
  m_user_count++;
}

// 关闭连接
void http_conn::close_conn()
{
  if (m_sockfd != -1)
  {
    removefd(m_epollfd, m_sockfd);
    m_sockfd = -1;
    m_user_count--;
  }
}

bool http_conn::read()
{
  printf("一次性读完所有数据");
  return true;
}

bool http_conn::write()
{
  printf("一次性写完所有数据");
  return true;
}

// 由线程池中的工作线程调用，这是处理http请求的入口函数
void http_conn::process()
{
  // 解析http请求
  printf("parse request, create response");

  // 生成相应
}