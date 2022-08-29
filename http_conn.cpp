#include "http_conn.h"
#include <strings.h>

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
  event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
  // event.events = EPOLLIN | EPOLLRDHUP;

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
  init();
}

void http_conn::init()
{
  m_check_state = CHECK_STATE_REQUESTLINE;
  m_checked_index = 0;
  m_start_line = 0;
  m_read_index = 0;
  m_method = GET;
  m_url = 0;
  m_version = 0;
  bzero(m_read_buf, READ_BUFFER_SIZE);
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
  if (m_read_index >= READ_BUFFER_SIZE)
  {
    return false;
  }
  // 读取到的字节
  int bytes_read = 0;
  while (true)
  {
    bytes_read = recv(m_sockfd, m_read_buf + m_read_index, READ_BUFFER_SIZE - m_read_index, 0);
    if (bytes_read == -1)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        // 没有数据
        break;
      }
      return false;
    }
    else if (bytes_read == 0)
    {
      return false;
    }
    m_read_index += bytes_read;
  }
  printf("读取到了数据,%s\n", m_read_buf);
  return true;
}

http_conn::HTTP_CODE http_conn::process_read()
{
  LINE_STATUS line_status = LINE_OK;
  HTTP_CODE ret = NO_REQUEST;
  char *text = 0;
  while (((m_check_state == CHECK_STATE_CHONNECT) && (line_status == LINE_OK)) || (line_status = parse_line()) == LINE_OK)
  {
    // 解析到了一行完整的数据 或者解析到了请求体
    text = get_line();
    m_start_line = m_checked_index;
    printf("got 1 http line :%s\n", text);
    switch (m_check_state)
    {
    case CHECK_STATE_REQUESTLINE:
    {
      ret = parse_request_line(text);
      if (ret == BAD_REQUEST)
      {
        return BAD_REQUEST;
      }
      break;
    }
    case CHECK_STATE_HEADER:
    {
      ret = parse_headers(text);
      if (ret == BAD_REQUEST)
      {
        return BAD_REQUEST;
      }
      else if (ret == GET_REQUEST)
      {
        return do_request();
      }
    }
    case CHECK_STATE_CHONNECT:
    {
      ret = parse_content(text);
      if (ret == GET_REQUEST)
      {
        return do_request();
      }
      line_status = LINE_OPEN;
      break;
    }
    default:
    {
      return INTERNAL_ERROR;
    }
    }
    return NO_REQUEST;
  }

  return NO_REQUEST;
}

// 解析http请求 请求方法 ，目标url HTTP版本
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
  // GET /index.html HTTP1.1
  m_url = strpbrk(text, " \t");
  *m_url++ = '\0';
  char *method = text;
  if (strcasecmp(method, "GET") == 0)
  {
    m_method = GET;
  }
  else
  {
    return BAD_REQUEST;
  }

  m_version = strpbrk(m_url, " \t");
  if (!m_version)
  {
    return BAD_REQUEST;
  }
  *m_version++ = '\0';
  if (strcasecmp(m_version, "HTTP/1.1") != 0)
  {
    return BAD_REQUEST;
  }

  if (strncasecmp(m_url, "http://", 7) == 0)
  {
    m_url += 7;
    m_url = strchr(m_url, '/');
  }
  if (!m_url || m_url[0] != '/')
  {
    return BAD_REQUEST;
  }

  m_check_state = CHECK_STATE_HEADER;
  return NO_REQUEST;
}

// 解析请求首行
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
  return NO_REQUEST;
}

//解析请求头
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
  return NO_REQUEST;

} // 解析请求体

// 解析一行的数据 判断依据\r\n
http_conn::LINE_STATUS http_conn::parse_line()
{
  char temp;
  for (; m_checked_index < m_read_index; ++m_checked_index)
  {
    temp = m_read_buf[m_checked_index];
    if (temp == '\r')
    {
      if ((m_checked_index + 1) == m_read_index)
      {
        return LINE_OPEN;
      }
      else if (m_read_buf[m_checked_index + 1] == '\n')
      {
        m_read_buf[m_checked_index++] = '\0';
        m_read_buf[m_checked_index++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
    else if (temp == '\n')
    {
      if ((m_checked_index > 1) && (m_read_buf[m_checked_index - 1] == '\r'))
      {
        m_read_buf[m_checked_index - 1] = '\0';
        m_read_buf[m_checked_index++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
    return LINE_OPEN;
  }
  return LINE_OK;
}

http_conn::HTTP_CODE http_conn::do_request()
{
}

bool http_conn::write()
{
  printf("一次性写完所有数据");
  return true;
}

bool http_conn::process_write(http_conn::HTTP_CODE ret)
{
}

// 由线程池中的工作线程调用，这是处理http请求的入口函数
void http_conn::process()
{
  // 解析http请求
  HTTP_CODE read_ret = process_read();
  if (read_ret == NO_REQUEST)
  {
    modfd(m_epollfd, m_sockfd, EPOLLIN);
    return;
  }
  // 生成响应
  bool write_ret = process_write(read_ret);
  if (!write_ret)
  {
    close_conn();
  }
  modfd(m_epollfd, m_sockfd, EPOLLOUT);
}