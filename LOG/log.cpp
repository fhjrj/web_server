#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <thread>
#include <mutex>
#include <string>
#include <stdarg.h>
#include "log.h"

Log::Log(){
      m_count=0;
        start=0;
        m_is_async=false;
}

Log::~Log(){
     if(m_fp!=nullptr){
            fclose(m_fp);
        }
}
  
   std::shared_ptr<Log> Log::get() {
        if (slinglog != nullptr) {
            return slinglog;
        }
        std::unique_lock<std::mutex> lock(mm_mutex);
        if (slinglog != nullptr) {
            lock.unlock();
            return slinglog;
        }
        slinglog = std::shared_ptr<Log>(new Log);
        lock.unlock();
        return slinglog;
    }

bool Log::init(const std::string& file_name,int close_log,int log_buf_size,int split_lines,bool use_queue,bool stdout_){
    if(use_queue==true){
        m_is_async=true;
        std::thread t2([this](){
            this->slinglog->async_log();
        });
         std::thread t3([this](){
            this->slinglog->async_log();
        });
        t2.detach();
        t3.detach();

    }
  m_stdout=stdout_;
  m_close_log=close_log;
  m_log_buf_size=log_buf_size;
  m_buf=new char [m_log_buf_size];
  memset(m_buf,'\0',m_log_buf_size);
  m_split_lines=split_lines;

  time_t t = time(NULL);
  struct tm *sys_tm = localtime(&t);
  struct tm my_tm = *sys_tm;

  const char* m=file_name.c_str();
  const char *p = strrchr(m, '/');
  char log_full_name[256] = {0};

    if (p == NULL)
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, m);//没有文件名 以时间+路径为文件名
    }
    else
    {
        strcpy(log_name, p + 1);
        strncpy(dir_name,m, p-m+1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);//有文件 就以路径+时间+文件
    }

    m_today=my_tm.tm_mday;
    m_fp=fopen(log_full_name,"a");

    if(m_fp==nullptr) return false;
    
    return true;

}


void Log::write_log(int level,const std::string& m,...){
    int n=0,m1=0;
    const char* op=m.c_str();
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16]={0};

     switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[Log in]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[Log in]:");
        break;
    }
    std::unique_lock<std::mutex> lockerp(mm_mutex);
    m_count++;
    //日志不是今天或写入的日志行数是最大行的倍数
    if(m_today!=my_tm.tm_mday||m_count%m_split_lines==0){
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};
       
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        //如果是时间不是今天,则创建今天的日志，更新m_today和m_count
        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else//如果是的话 最后名字加入超过行
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
        
    va_list valst;
    va_start(valst, op);
    std::string log_str;
    
    std::string lem="";
    lem=std::to_string( my_tm.tm_year + 1900)+std::to_string(my_tm.tm_mon + 1)+std::to_string( my_tm.tm_mday)+
        std::to_string( my_tm.tm_hour)+std::to_string(my_tm.tm_min)+std::to_string(my_tm.tm_sec)+std::to_string( now.tv_usec);
    
    int len=lem.size();
    if(len+start+strlen(s)>=m_log_buf_size-1){
        memset(m_buf,'\0',sizeof(m_buf));
        start=0;//超过buf长度 重置
    }
    
        n = snprintf(m_buf+start, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    
    /*snprintf的'\0'被后后面的vsnprintf函数覆盖*/
    if(m.size()+start+n>=m_log_buf_size-1){
            memset(m_buf,'\0',sizeof(m_buf));
            start=0;
            n = snprintf(m_buf+start, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
            m1=vsnprintf(m_buf+start+n,m_log_buf_size-n-1-start,op,valst);
  //超过buf长度 重置
    }else{
    
            m1=vsnprintf(m_buf+start+n,m_log_buf_size-n-1-start,op,valst);
    }
    m_buf[n+m1+start]='\n';
    m_buf[n+m1+start+1]='\0';
    log_str=m_buf+start;
    start=n+m1+start+1;//下一次调用时 \0被覆盖
    lockerp.unlock();

    if(m_is_async&&!block_queues.empty()){
         slinglog->block_queues.push(log_str);
    }else{
        std::unique_lock<std::mutex> lock3(mm_mutex);
        fputs(log_str.c_str(),m_fp);
        if(m_stdout){
            fputs(log_str.c_str(),stdout);
        }
    }
    va_end(valst);
}

void Log::flush(void)
{
    std::unique_lock<std::mutex> locker(mm_mutex);
    fflush(m_fp);
}

std::mutex Log::mm_mutex;
std::shared_ptr<Log> Log::slinglog=nullptr;
int  Log::m_close_log=0;  


 
