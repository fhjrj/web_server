#ifndef _MYSQLPOOL_H_
#define _MYSQLPOOL_H_

#include <queue>//STL not diaryqueue
#include <mysql/mysql.h>
#include <string>
#include <atomic>
#include <iostream>
#include <memory>
#include "../LOG/log.h"


class connection_pool
{
private:
	connection_pool(const  connection_pool&) = delete;
	connection_pool& opertaor(const  connection_pool&) = delete;
public:
  	std::string m_url;			 //主机地址
	int m_Port;		 //数据库端口号
	std::string m_User;		 //登陆数据库用户名
	std::string m_PassWord;	 //登陆数据库密码
	std::string m_DatabaseName; //使用数据库名
	int m_close_log;	//日志开关
private:
	int m_MaxConn;  //最大连接数
	int m_CurConn; //当前已使用的连接数
	int m_FreeConn;//剩下的数量
	std::queue<MYSQL*> connqueue;
	std::atomic<int> resource;//资源
	static std::shared_ptr<connection_pool> sling_conpoll;
	std::atomic<bool> tp;//用于锁

public:
	MYSQL* GetConnection();				 //获取数据库连接
	bool ReleaseConnection(MYSQL*); //释放连接
	int GetFreeConn();					 //获取连接
	void initt(std::string,std::string,std::string,std::string,int,int,int);
	void DestroyPool();
	static std::shared_ptr<connection_pool> Get();

	connection_pool():m_MaxConn(0),m_CurConn(0),m_FreeConn(0),tp{false} {}
	
	~connection_pool();
};

class connectionRAII{

public:
	connectionRAII(MYSQL **con,  std::shared_ptr<connection_pool> poolRAII_);
	~connectionRAII();
	
private:
	MYSQL *conRAII;
	std::shared_ptr<connection_pool> poolRAII;
};



#endif