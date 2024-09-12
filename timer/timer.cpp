#include "../timer/timer.h"
#include "../http/http.h"

 void time_heap::add_timer( std::shared_ptr<heap_timer> timer){
      if( !timer )
        {
            return;
        }
        array.push_back(timer);
        siftup(array.size()-1);
        
 }

 void time_heap::siftup(int i){
    while(true){
        int p=getfather(i);
        if(p<0||array[p]->expire<=array[p]->expire)
        break;
        std::swap(array[p],array[i]);
        i=p;
    }
 }
 


std::shared_ptr<heap_timer> time_heap::top(){
        if ( empty() )
        {
            return NULL;
        }
        return array[0];
    }

void time_heap::del_timer(std::shared_ptr<heap_timer> timer){
      if( !timer ){
		return ;
	  }
	std::function<void()> notask=[](){};
        timer->task1=notask;
    };
}
 

void time_heap::adjust_timer(std::shared_ptr<heap_timer> new_timer,std::shared_ptr<heap_timer> old_timer){
         del_timer(old_timer);
         add_timer(new_timer);
         LOG_INFO("adjust timer");
}

void time_heap::tick(){
      LOG_INFO("time tick");
       std::unique_lock<std::mutex> locker(m_mutex);
       if(array.size()==0) 
          return;
        std::shared_ptr<heap_timer> tmp=array[0];
        time_t cur = time( NULL );
        while( !empty() )
        {
            if( tmp->expire > cur||!tmp )
            {
                break;
            }
            tmp->task1();//std::function<void()>=std::bind();
            pop_timer();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            tmp = array[0];
        }
}


void time_heap::pop_timer(){
     if( empty() )
        {
        LOG_ERROR("heap is empty")
        throw std::out_of_range("heap is empty");
        return ;
        }
        
        std::swap(array[0],array[array.size()-1]);
        array.pop_back();
        shift_down(0);
}

 bool time_heap::empty(){
     return array.empty();
 }

  void time_heap::shift_down( int hole )
    {
       while(true){
        int l=getleft(hole),r=getright(hole),min=hole;
        if(l<array.size()&&array[l]->expire<array[min]->expire) 
           min=l;
        if(r<array.size()&&array[r]->expire<array[min]->expire)
           min=r;

           if(min==hole)
           break;
           std::swap(array[hole],array[min]);
           hole=min;
       }
    }



void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}


int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void Utils::timer_handler()
{
    heaper.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

void Utils::cb_func(client_data *user_data)
{   
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
    LOG_INFO("close sockfd is %d",user_data->sockfd);
}

int Utils::u_epollfd=0;
int* Utils::u_pipefd=0;

