#ifndef HTTP_CONN_H_
#define HTTP_CONN_H_

#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
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
  static int m_epollfd;                      // 所有socket上的事件都被注册到一个epoll上
  static int m_user_count;                   // 统计用户数量
  static const int READ_BUFFER_SIZE = 2048;  // 读缓冲区列表大小
  static const int WRITE_BUFFER_SIZE = 1024; //写缓冲区大小

  // http 请求的方法
  enum METHOD
  {
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELTET,
    TRACE,
    OPTIONS,
    CONNECT
  };

  // 解析客户请求时，主机的状态
  // CHECK_STATE_REQUESTLINE 正在分析当前星球行
  // CHECK_STATE_HEADER
  // CHECK_STATE_CHONNECT 正在解析请求体
  enum CHECK_STATE
  {
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_CHONNECT
  };

  /*
   * 从状态机的三种可能状态，即行的可读状态，分别表示
   * 1.读到完整的行 2.行出错 3. 行数据尚不完整
   */
  enum LINE_STATUS
  {
    LINE_OK = 0,
    LINE_BAD,
    LINE_OPEN
  };

  /*
   * NO_REQUEST
   * GET_REQUEST
   * BAD_REQUEST
   * NO_RESOURCE
   * FORBIDDEN_REQUEST
   * FILE_REQUEST
   * INTERNAL_ERROR
   * CLOSED_CONNECTION
   */
  enum HTTP_CODE
  {
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
  };

  http_conn() {}
  ~http_conn() {}

  void process(); // 处理客户请求
  void init(int sockfd, const sockaddr_in &addr);
  void close_conn();
  bool read();  // 非阻塞读
  bool write(); // 非阻塞写

  LINE_STATUS parse_line();

private:
  int m_sockfd;                      // 该http连接的socket
  sockaddr_in m_address;             // 通信socket地质
  char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
  int m_read_index;                  // 读缓冲区中已经读入的客户\端数据的最后一个字节的下一个位置

  int m_checked_index; // 当前正在分析的字符在缓冲区的位置
  int m_start_line;    // 当前正在解析的行的起始位置
  char *m_url;         // 请求目标的文件名
  char *m_version;     // 协议版本 http1.1
  METHOD m_method;     // 请求方法
  char *m_host;        // 主机名
  bool m_linger;       // 判断http请求是否要保持连接

  CHECK_STATE m_check_state; // 主状态机目前所处的位置

  void init(); // 初始化连接其他的信息；

  HTTP_CODE process_read();                 // 解析http请求
  HTTP_CODE parse_request_line(char *text); // 解析请求首行
  HTTP_CODE parse_headers(char *text);      //解析请求头
  HTTP_CODE parse_content(char *text);      // 解析请求体

  bool process_write(HTTP_CODE ret);

  char *get_line()
  {
    return m_read_buf + m_start_line;
  }
  HTTP_CODE do_request();
};

#endif // !HTTP_CONN_H_