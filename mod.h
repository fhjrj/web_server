#ifndef MOD_H
#define MOD_H

#include "web.h"

class MOD{
    public:
    MOD();
    ~MOD(){};
    
    void parse_arg(int argc,char* argv[]);

    //端口号
    int PORT;

    //日志写入方式
    int LOGWrite;

    //触发组合模式
    int TRIGMode;

    //优雅关闭链接
    int OPT_LINGER;

    //数据库连接池数量
    int sql_num;

    //线程池内的线程数量
    int thread_num;

    //是否关闭日志
    int close_log;

    //并发模型选择
    int actor_model;


};

#endif
