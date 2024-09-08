#include "mod.h"

MOD::MOD(){
    PORT = 9210;

    //日志写入方式，默认同步
    LOGWrite = 0;

    //触发组合模式,默认listenfd LT + connfd LT
    TRIGMode = 0;

    //优雅关闭链接，
    OPT_LINGER = 0;

    //数据库连接池数量,默认8
    sql_num =std::thread::hardware_concurrency();

  //线程池内的线程数量,默认8
    thread_num = 8;

    //关闭日志,默认不关闭
    close_log = 0;

    //并发模型,默认是proactor
    actor_model = 0;

}

void MOD::parse_arg(int argc,char* argv[]){
          if(argc!=1){
            PORT = atoi(argv[1]);
           
    
            LOGWrite = atoi(argv[2]);
    
        
            TRIGMode = atoi(argv[3]);
        
            OPT_LINGER = atoi(argv[4]);
        
            sql_num = atoi(argv[5]);
           
            thread_num =std::thread::hardware_concurrency();
            
            close_log = atoi(argv[6]);// 0
            
            actor_model = atoi(argv[7]);
          }
          else if(argc==1){
            
          }

}



