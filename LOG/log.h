#ifndef _LOG_H_
#define _LOG_H_

#include "../queue/blockqueue.h"
#include <string>
#include <thread>
#include <mutex>
#include <stdarg.h>

class Log {
private:
    Log(const Log&) = delete;
    Log& operator=(const Log &) = delete;
    static std::shared_ptr<Log> slinglog;
    threadsafe_queue<std::string> block_queues;
    char dir_name[128];     //路径名
    char log_name[128];     //log文件名
    int m_split_lines;      //日志最大行数
    int m_log_buf_size;     //日志缓冲区大小
    long long m_count;      //日志行数记录
    int m_today;            //按天分文件,记录当前时间是那一天
    FILE *m_fp;             //打开log的文件指针
    char *m_buf;            //要输出的内容
    bool m_is_async;        //是否同步标志位           
    static std::mutex mm_mutex;
    int start;//同步字段
    bool m_stdout;//是否标准输出
    
public:
static int  m_close_log;  
Log();
~Log();
static std::shared_ptr<Log> get();
 bool init(const std:: string& filename,int close_log,int log_buf_size=8192,int split_lines=5000000,bool use_queue=false,bool stdout_=false);

    void write_log(int level,const std::string&,...);
    void flush();
private:
  void async_log_write(){
        while(!block_queues.empty()){
              std::unique_lock<std::mutex> guard(mm_mutex);
                std::shared_ptr<std::string> lg=block_queues.wait_and_pop();
                const char*p=(const char*)*(lg.get())->c_str();
              fputs(p,m_fp);
              if(m_stdout){
                fputs(p,stdout);
              }
        }
    }

    public:
    static void async_log(){
        if(slinglog!=nullptr)
        Log::slinglog->get()->async_log_write();
        else{
        std::shared_ptr<Log> mp=Log::get();
        mp->async_log_write();
        }
    }
};


#define LOG_DEBUG(format, ...) if(0 ==(Log::get()->m_close_log)) {Log::get()->write_log(0, format, ##__VA_ARGS__); Log::get()->flush();}
#define LOG_INFO(format, ...) if(0 == (Log::get()->m_close_log)) {Log::get()->write_log(1, format, ##__VA_ARGS__); Log::get()->flush();}
#define LOG_WARN(format, ...) if(0 ==(Log::get()->m_close_log)) {Log::get()->write_log(2, format, ##__VA_ARGS__); Log::get()->flush();}
#define LOG_ERROR(format, ...) if(0 == (Log::get()->m_close_log)) {Log::get()->write_log(3, format, ##__VA_ARGS__); Log::get()->flush();}

#endif