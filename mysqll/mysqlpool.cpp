#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <atomic>
#include <iostream>
#include <thread>
#include <memory>
#include "../mysqll/mysqlpool.h"
#include "../LOG/log.h"



std::shared_ptr<connection_pool> connection_pool::Get() {
	if (sling_conpoll != nullptr) {
		return sling_conpoll;
	}
	else {
		sling_conpoll = std::shared_ptr<connection_pool>(new connection_pool);
		return sling_conpoll;
	}
}

void connection_pool::initt(std::string url, std::string User,std:: string PassWord,std:: string DBName, int Port, int MaxConn, int close_log)
{
		m_url = url;
		m_Port = Port;
		m_User = User;
		m_PassWord = PassWord;
		m_DatabaseName = DBName;
		m_close_log = close_log;
	for (int i = 0; i < MaxConn; i++)//加入MYSQL
	{ 
		
         MYSQL *con = NULL;
		con = mysql_init(con);

		if (con == NULL)
		{
			std::cout << "Error: connect mysql failed: " << mysql_error(con) << std::endl;
           mysql_close(con), con = NULL;
			exit(1);
		}
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);//都初始化为当前主机状态

		if (con == NULL)
		{
			std::cout << "Error: connect mysql failed: " << mysql_error(con) << std::endl;
            mysql_close(con), con = NULL;
			exit(1);
		}
		connqueue.push(con);
		++m_FreeConn;

        }

	resource.store(m_FreeConn, std::memory_order_relaxed);

	m_MaxConn = m_FreeConn;
}


MYSQL *connection_pool::GetConnection()//取出一个进行使用
{

    bool use_expected = false;
    bool use_desired = true;
    MYSQL* con=nullptr;

while (resource.fetch_sub(1) <= 0) {
	resource.fetch_add(1);
}

do {
	use_expected = false;
	use_desired = true;
} while (!tp.compare_exchange_strong(use_expected, use_desired));

con = connqueue.front();
connqueue.pop();
m_FreeConn--;
m_CurConn++;
do {
	use_expected = true;
	use_desired = false;
} while (!tp.compare_exchange_strong(use_expected, use_desired));

return  con;
}

bool connection_pool::ReleaseConnection(MYSQL *con)//释放当前使用的mysql，重新假如
{
    bool use_expected = false;
    bool use_desired = true;
if (con==nullptr)
	return false;

do {
	use_expected = false;
	use_desired = true;
} while (!tp.compare_exchange_strong(use_expected, use_desired));

connqueue.push(con);
m_FreeConn++;
m_CurConn--;

do {
	use_expected = true;
	use_desired = false;
} while (!tp.compare_exchange_strong(use_expected, use_desired));

resource++;
return true;
}

void connection_pool::DestroyPool()
{

	bool use_expected = false;
	bool use_desired = true;
	do {
		use_expected = false;
		use_desired = true;
	} while (!tp.compare_exchange_strong(use_expected, use_desired));

	if (connqueue.size() > 0)
	{
		while (!connqueue.empty()) connqueue.pop();
		m_CurConn = 0;
		m_FreeConn = 0;
	}

	do {
		use_expected = true;
		use_desired = false;
	} while (!tp.compare_exchange_strong(use_expected, use_desired));

}


connection_pool::~connection_pool() {
	DestroyPool();
}


int connection_pool::GetFreeConn()
{
	return sling_conpoll->m_FreeConn;
}

/*每个MYSQL对应一个RAII*/
connectionRAII::connectionRAII(MYSQL **SQL,std::shared_ptr<connection_pool> connpool){
	*SQL = connpool->GetConnection();
	
	conRAII = *SQL;
	poolRAII =connpool;
}

connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}
std::shared_ptr<connection_pool> connection_pool::sling_conpoll = nullptr;

