#include "mod.h"


int main(int argc, char *argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    std::string user = "root";
    std::string passwd = "123456";
    std::string databasename = "youdb";

    //命令行解析
    MOD config;
    config.parse_arg(argc, argv);

    webserver server;

    //初始化
    server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    

    //日志
    server.log_write();

    

    //线程池
    server.threadpool_();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //数据库
    server.sql_pool();

    //运行
    server.eventLoop();

    return 0;
}