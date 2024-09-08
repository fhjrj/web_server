#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <algorithm>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <vector>
#include <functional>
#include <time.h>
#include "../LOG/log.h"


 using TimeoutTask=std::function<void()>;
class heap_timer;

struct client_data
{
    sockaddr_in address;
    int sockfd;
    std::shared_ptr<heap_timer> timer;
};


class heap_timer
{
public:
    heap_timer( int delay )
    {
        expire = time( NULL ) + 3*delay;
    }
  ~heap_timer(){}

public:
   time_t expire;
   TimeoutTask task1;
   client_data* user_data;
};

class time_heap
{
    private:
    std::vector<std::shared_ptr<heap_timer>>  array;
public:
 ~time_heap(){}

  time_heap( ){
    array.reserve(64);
  }


  int getleft(int i){
    return 2*i+1;
  }

  int getright(int i){
    return 2*i+2;
  }

  int getfather(int i){
    return (i-1)/2;
  }
  
    public:
     std::mutex m_mutex;
     void tick();
     void del_timer(std::shared_ptr<heap_timer>);
     void pop_timer();
     void siftup(int i);
     void add_timer(std::shared_ptr<heap_timer>);
   std::shared_ptr<heap_timer> top(); 
     void shift_down( int hole );
     bool empty();
     void adjust_timer(std::shared_ptr<heap_timer>,std::shared_ptr<heap_timer>);
};


class Utils
{
public:
    Utils():heaper(){
      u_pipefd=nullptr;
      u_epollfd=0;
    }
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

    void cb_func(client_data *user_data);

public:
    static int *u_pipefd;
    time_heap heaper;
     static int u_epollfd;
     int m_TIMESLOT;
};

#endif
