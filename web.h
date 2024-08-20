#ifndef _WEB_H
#define _WEB_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <assert.h>
#include <vector>
#include <string>
#include "http/http.h"
#include <iostream>
#include "threadpool/threadpool.h"

const int MAX_FD=65536;
const int MAX_EVENT=9999;
const int timeout=5;

class webserver{
   public:
   webserver():users(MAX_FD),user_time(MAX_FD),utils(){
     char server_path[200];
     getcwd(server_path,200);
     std::string m(server_path);
    
     m+="/resoue";
     m_root=m;
     std::cout<<"this resoures is :"<<m_root.c_str()<<std::endl;
   }
   ~webserver();

    void init(int port , std::string user, std::string passWord, std::string databaseName,
              int log_write , int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);
    void threadpool_();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(std::shared_ptr<heap_timer> timer,int connfd);//
    void deal_timer(std::shared_ptr<heap_timer> timer, int sockfd);//
    bool dealclientdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);
    void httpread(int);
    void httpwrite(int);
    void http_read_and_write_task(int,int);
    public:
    int m_port;
    std::string m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;

    int m_pipefd[2];
    int m_epollfd;
    std::vector<http_conn> users;

     //数据库相关
    std::shared_ptr<connection_pool>  webconnpool;
    std::string m_user;         //登陆数据库用户名
    std::string m_passWord;     //登陆数据库密码
    std::string m_databaseName; //使用数据库名
    int m_sql_num;

    epoll_event events[MAX_EVENT];
    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

     //定时器相关
    std::vector<client_data> user_time;
    Utils utils;

    int  m_thread_num ;

};

#endif
