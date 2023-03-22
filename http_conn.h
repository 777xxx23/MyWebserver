#ifndef HTTP_CONN_h
#define HTTP_CONN_h

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include "locker.h"

class http_conn
{
public:

    static const int FILENAME_LEN = 200;//文件名的最大长度

    static const int READ_BUFFER_SIZE = 2048;//缓冲区的大小

    static const int WRITE_BUFFER_SIZE = 1024;//写缓冲区的大小


    //HTTP请求方法
    //HTTP请求方法,但本代码中仅仅支持GET
    enum METHOD{
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };


    //主状态机可能的状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTION = 0,//正在分析当前请求行
        CHECK_STATE_HEADER,//正在分析头部字段
        CHECK_STATE_CONTENT
    };

    //从状态机可能的状态
    enum LINE_STATUS
    {
        LINE_OK = 0,//读取到一个完整的行
        LINE_BAD,//行出错
        LINE_OPEN//行数据尚且不完整
    };

    //服务器处理http请求的结果
    enum HTTP_CODE
    {
        NO_REQUEST,//请求不完整需要继续读取
        GET_REQUEST,//得到了一个完整的请求
        BAD_REQUEST,//请求有语法错误
        NO_RESOURCE,//没有资源
        FORBIDDEN_REQUEST,//没有足够的权限
        FILE_REQUEST,//文件已被请求
        INTERNAL_ERROR,//服务器内部错误
        CLOSED_CONNECTION//客户端连接已关闭
    };

public:
    http_conn(){}
    ~http_conn(){}
public:
    void init(int sockfd,const sockaddr_in& addr);
    void close_conn(bool real_close = true);
    bool read();
    bool write();
    void process();

private:
    void init();
    HTTP_CODE process_read();//解析http请求
    bool process_write(HTTP_CODE ret);//填充http应答

    //下面一组函数被process_read调用以分析HTTP请求
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_header(char* text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    char* get_line(){ return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();

    //下面一组函数被process_write调用以填充HTTP应答
    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;

private:
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx;
    int m_check_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;

    CHECK_STATE m_check_state;
    METHOD m_method;

    char m_real_file[FILENAME_LEN];
    char* m_url;
    char* m_version;
    char* m_host;
    int m_content_length;
    bool m_linger;

    char* m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
};

#endif