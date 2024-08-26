#include "web.h"

webserver::~webserver(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    user_time.clear();
    users.clear();


}


void webserver::init(int port, std::string user, std::string passWord, std::string databaseName, int log_write, 
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actormodel = actor_model;
}

void webserver::trig_mode()
{
    //LT + LT
    if (0 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;//对应的是监听的ET还是LT
        m_CONNTrigmode = 0;//对应的是http的ET还是LT
    }
    //LT + ET
    else if (1 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    //ET + LT
    else if (2 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    //ET + ET
    else if (3 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}



void webserver::log_write(){
    if(m_close_log==0){
        if(m_log_write==1){
            Log::get()->init("./serverlog",m_close_log,8192,5000000,true,true);
        }else{
            Log::get()->init("./serverlog",m_close_log,8192,5000000,false,true);
        }
    }
}

void webserver::threadpool_(){
    Threadpool::instance();
}

void webserver::sql_pool(){
  webconnpool=connection_pool::Get();
  webconnpool->initt("127.0.0.1",m_user,m_passWord,m_databaseName,3306,m_sql_num,m_close_log);
  users.front().initmysql_result(webconnpool);
}


void webserver::eventListen(){
    m_listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(m_listenfd>=0);

    if(m_OPT_LINGER==0){
         struct linger tmp={0,1};
         setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }else if(m_OPT_LINGER==1){
        struct linger tmp={1,1};
         setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }

    int ret=0;
    struct sockaddr_in address;
    memset(&address,0,sizeof(address));
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(m_port);
    
    int one=1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));/*准许端口被释放后重新利用*/
    ret=bind(m_listenfd,(struct sockaddr*)&address,sizeof(address));
    assert(ret>=0);
    ret=listen(m_listenfd,5);
    assert(ret>=0);

    epoll_event events[MAX_EVENT];
    m_epollfd=epoll_create(5);
    assert(m_epollfd!=-1);

    utils.init(timeout);
    utils.addfd(m_epollfd,m_listenfd,false,m_LISTENTrigmode);//listenfd一般设置LT  

    http_conn::m_epollfd=m_epollfd;

    ret=socketpair(PF_UNIX,SOCK_STREAM,0,m_pipefd);
    utils.addfd(m_epollfd,m_pipefd[0],false,0);//主进程监听信号
    utils.setnonblocking(m_pipefd[1]);//发送用到 信号函数作用
    
    utils.addsig(SIGPIPE,SIG_IGN);
    utils.addsig(SIGALRM,utils.sig_handler,false);
    utils.addsig(SIGTERM,utils.sig_handler,false);

    alarm(timeout);

    utils.u_epollfd=m_epollfd;
    utils.u_pipefd=m_pipefd;
}
/*当连接中断时，需要延迟关闭(linger)以保证所有数据都被传输，*/
/*linger 第一位0或非零 表示是否延迟关闭 第二位表示延时关闭时间 1秒*/
/*主进程的监听通道和listenfd都设置为EPOLLIN，且不设置EPOLLET*/
/*负责恢复客户端的套接字 专门调用http::conn的add函数 根据模式设置ET/LT*/


void webserver::timer(int connfd,struct sockaddr_in client_data){            //ET/LT
       users[connfd].init(connfd,client_data,const_cast<char*>(m_root.c_str()),m_CONNTrigmode,m_close_log,m_user,m_passWord,m_databaseName);//设置非阻塞加入监听

         user_time[connfd].sockfd=connfd;
         user_time[connfd].address=client_data;
          std::shared_ptr<heap_timer> timer=std::make_shared<heap_timer>(timeout);
                timer->user_data = &user_time[connfd];
                 std::function<void()> taskt=std::bind(&Utils::cb_func,&(this->utils),&(this->user_time[connfd]));
                timer->task1=std::move(taskt);
                user_time[connfd].timer= timer;
                utils.heaper.add_timer(timer);
                
}

 void webserver::adjust_timer( std::shared_ptr<heap_timer> oldtimer,int connfd){
                       std::shared_ptr<heap_timer> new_timer =std::make_shared<heap_timer>(timeout);
                       new_timer->user_data= &user_time[connfd];
                      std::function<void()> taskt=std::bind(&Utils::cb_func,&(this->utils),&(this->user_time[connfd]));
                      new_timer->task1=std::move(taskt);
                       user_time[connfd].timer= new_timer;
					   utils.heaper.adjust_timer(new_timer,oldtimer);
 }
 
 void webserver::deal_timer(std::shared_ptr<heap_timer> timer,int sockfd){
    timer->task1();
    if(timer){
        utils.heaper.del_timer(timer);
    };
    LOG_INFO("close fd is %d",user_time[sockfd].sockfd);
 }


 bool webserver::dealclientdata(){
    struct sockaddr_in clinet_address;
    socklen_t size=sizeof(clinet_address);
    if(0==m_LISTENTrigmode){
        int coonfd=accept(m_listenfd,(struct sockaddr*)&clinet_address,&size);
        if(coonfd<0){
            LOG_ERROR("%s:errno is %d","acccept error",errno);
            return false;
        }
        if(http_conn::m_user_count>=MAX_FD){
            utils.show_error(coonfd,"server busy");
            LOG_ERROR("%s","server busy");
            return false;
        }
        timer(coonfd,clinet_address);
    }else{
        while(1){//ET listen全部接受完在在进行操作
             int coonfd=accept(m_listenfd,(struct sockaddr*)&clinet_address,&size);
        if(coonfd<0){
            LOG_ERROR("%s:errno is %d","acccept error",errno);
            return false;
        }
        if(http_conn::m_user_count>=MAX_FD){
            utils.show_error(coonfd,"server busy");
            LOG_ERROR("%s","server busy");
            return false;
        }
        timer(coonfd,clinet_address);
        }
        return false;
    }
    return true;
 }
 



 bool webserver::dealwithsignal(bool& timeout,bool& stop_server){
    int ret=0;
    int sig;
    char signals[1024];
    ret=recv(m_pipefd[0],signals,sizeof(signals),0);

    if(ret==-1){
        return false;
    }
    else{
        for(int i=0;i<ret;i++){
            switch (signals[i])
            {
                case SIGALRM:
                {
                    timeout =true;
                    break;
                }
                case SIGTERM:{
                    stop_server=true;
                    break;
                }
            }
        }
    }
    return true;
 }

 
void webserver::eventLoop()
{   
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            //处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = dealclientdata();
                if (false == flag)
                    continue;
            }//不论是false/true都把请求做完
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                std::shared_ptr<heap_timer> timer = user_time[sockfd].timer;
                deal_timer(timer, sockfd);
            }
           
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = dealwithsignal(timeout, stop_server);//通知主进程有信号 要执行信号函数
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
           
            else if (events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if (timeout)
        {
            utils.timer_handler();
            timeout = false;
        }
    }
}

void webserver::httpread(int sockfd){
    bool result=users[sockfd].read_once();
    if(result){
        users[sockfd].improv=1;
        connectionRAII mysqlcon(&users[sockfd].mysql, webconnpool);//进行数据库的读取
        users[sockfd].process();
    }
    else{
     users[sockfd].improv=1;
     users[sockfd].timer_flag=1;   
    }
}

void  webserver::httpwrite(int sockfd){
    bool result=users[sockfd].write();
    if(result){
      users[sockfd].improv=1;
    }else{
         users[sockfd].improv=1;
          users[sockfd].timer_flag=1;  
    }
}


void webserver::http_read_and_write_task(int sockfd,int mod){
    users[sockfd].m_state=mod;
           if(mod==0){
              httpread(sockfd);
           }else{
              httpwrite(sockfd);
           }
}


void webserver::dealwithread(int sockfd){
    std::shared_ptr<heap_timer> timer=user_time[sockfd].timer;
    if(timer.get()==nullptr)
     return;
//reactor
    if(m_actormodel==1){ 
        if(timer.get()!=nullptr){
            adjust_timer(timer,sockfd);
        }
        std::future<void> ans=Threadpool::instance().submit(std::bind(&webserver::http_read_and_write_task,this,sockfd,0));
        ans.get();
         while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;/*读失败，关闭，更新状态*/
                }
                users[sockfd].improv = 0;
                break;
            }
        }
         
    }else{//proactor 无对应更新状态和事件划分
    if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

          std::future<void> ans=Threadpool::instance().submit([&](){
                connectionRAII mysqlcon(&(this->users[sockfd].mysql),this->webconnpool);
                this->users[sockfd].process();
          });
          
          ans.get();
            if (timer.get()!=nullptr)
            {  
                adjust_timer(timer,sockfd);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
    }


    void webserver::dealwithwrite(int sockfd){
        std::shared_ptr<heap_timer> timer=user_time[sockfd].timer;
        if(timer.get()==nullptr)
         return ;

         if(m_actormodel==1){
            if(timer.get()){
                adjust_timer(timer,sockfd);
            }

            std::future<void> ans=Threadpool::instance().submit(std::bind(&webserver::http_read_and_write_task,this,sockfd,1));
               ans.get();
            while(true){
                if(users[sockfd].improv==1){
                    if(users[sockfd].timer_flag==1){
                        deal_timer(timer,sockfd);
                        users[sockfd].timer_flag=0;
                    }
                    users[sockfd].improv=0;
                    break;
                }
            }
         }else{
            if(users[sockfd].write()){
             if(timer!=nullptr){
                LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                adjust_timer(timer,sockfd);
             }
            }else{
                 /*非持续连接，不用延长执行时间和writev()函数调用失败，都进行关闭*/
                if(!users[sockfd].error)//writev() error
                {
                  LOG_INFO("writev() error");
                }
                else{//not linger alive
                    LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                }

                  deal_timer(timer, sockfd);
            }
            
         }
    }




/*reactor：确保每一次操作都进行完毕后且知晓对应结果状态再进入，读写事件严格分开*/
/*proactor:限制条件没有reactor多，直接执行即可，没有专门的事件分类*/


