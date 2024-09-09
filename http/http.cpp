#include "http.h"
#include <fstream>
#include <mysql/mysql.h>
#include <iostream>


const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

std::mutex m_tex;
std::map<std::string,std::string> users;


void http_conn::initmysql_result(std::shared_ptr<connection_pool> pool){
    MYSQL* mysql=nullptr;
   connectionRAII mysqlcon(&mysql,pool);
    if(mysql_query(mysql,"SELECT username,passwd FROM user")){
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    MYSQL_RES* result=mysql_store_result(mysql);
    int num_fields=mysql_num_fields(result);
    MYSQL_FIELD* fields=mysql_fetch_fields(result);

    while(MYSQL_ROW row=mysql_fetch_row(result)){
        std::string m1(row[0]);
        std::string m2(row[1]);
        用户[m1]=m2;
    }
} 



int setnoblocking(int fd){
    int oldfd=fcntl(fd,F_GETFL);
    int newfd=oldfd|O_NONBLOCK;
    fcntl(fd,F_SETFL,newfd);
    return oldfd;
}

void addfd(int epollfd,int fd,bool one_shot,int MODE){
    epoll_event event;
    event.data.fd=fd;
    if(MODE==1){
        event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;
    }else{
        event.events=EPOLLIN|EPOLLRDHUP;
    }
/*是否一次监听后失效*/
    if(one_shot){
        event.events|=EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnoblocking(fd);
}
/*区分ET模式还是LT*/
void remofd(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,nullptr);
    close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd,int fd1,int ev,int MODE){
    epoll_event event;
    event.data.fd=fd1;
    if(MODE==1){
        event.events=ev|EPOLLET|EPOLLONESHOT|EPOLLRDHUP;
    }else{
        event.events=ev|EPOLLONESHOT|EPOLLRDHUP;
    }
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd1,&event);
}

 int http_conn::m_user_count=0;
 int http_conn::m_epollfd=-1;
 

 void http_conn::close_conn(bool close){
    if(close&&(m_sockfd!=-1)){
        std::cout<<"close sockfd:  "<<m_sockfd<<std::endl;
        remofd(m_epollfd,m_sockfd);
        m_sockfd=-1;
        m_user_count--;
    }
 }


 void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, std::string user, std::string passwd, std::string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_Mode);
    m_user_count++;

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_Mode = TRIGMode;
    m_close_log = close_log;
    
    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}


 void http_conn::init(){
    mysql=nullptr;
    bytes_have_send=0;
    bytes_to_send=0;
    m_check_state=CHECK_STATE_REQUESTLINE;
    m_linger=false;
    m_method=GET;
    m_url=0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    error=true;

    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;
    
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
 }


 http_conn::LINE_STATUS http_conn::parse_line(){
    char tmp;
    for(;m_checked_idx<m_read_idx;m_checked_idx++){
        tmp=m_read_buf[m_checked_idx];
        if(tmp=='\r'){
            if(m_checked_idx+1==m_read_idx){
                return LINE_OPEN;
            }else if(m_read_buf[m_checked_idx+1]=='\n'){
                m_read_buf[m_checked_idx++]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }else if(tmp=='\n'){
            if(m_read_buf[m_checked_idx-1]=='\r'&&m_checked_idx>1){
                m_read_buf[m_checked_idx-1]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
 }

 /*LT情况下 只要start<=0,返回false*/
 bool http_conn::read_once(){
    if(m_read_idx>=READ_BUFFER_SIZE){
        return false;
    }
    int start=0;
    if(m_Mode==0){
        start=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        m_read_idx+=start;

        if(start<=0){
            return false;
        }
        return true;
    }else{
        while(1){//一次性全部读取完毕
              start=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        m_read_idx+=start;
        if(start==-1){
           if(errno==EAGAIN||errno==EWOULDBLOCK)
             break;//非阻塞返回true,即一次性读取完毕
             return false;
        }else if(start==0){
            return false;
        }
         m_read_idx+=start;
        }
        return true;
    }
    std::cout<<"read over"<<std::endl;
 }
 /*EINTR:被中断*/


 /*分析HTTP请求行*/
/*strcasecmp(char* a,char* b);逐个字符进行比较字符，直到遇到空格停止 相等则返回零*/
 http_conn::HTTP_CODE http_conn::parse_headers(char* text){
   //指向第一个空格
    m_url=strpbrk(text," \t");
    if(!m_url){
        return BAD_REQUEST;
    }
    *m_url++='\0'; //截断后移 m_url前面是method,后面是url
    
        char* method=text;
    if(strcasecmp(method,"GET")==0){
        m_method=GET;
    }else if(strcasecmp(method,"POST")==0){
        m_method=POST;
        cgi=1;
    }else 
       return BAD_REQUEST;//其他请求 BAD_REQUEST
       
     //  m_url+=strspn(m_url,"\t");
       m_version=strpbrk(m_url," \t");  
       if(!m_version){
        return BAD_REQUEST;
       }
        *m_version++='\0';//指向第二个空格并隔断url,m_version开始的内容是HTTP/版本号
        
    m_version += strspn(m_version, " \t");
       if(strcasecmp(m_version,"HTTP/1.1")!=0)
       return BAD_REQUEST;
                                                                       
       // 此时请求头被分为了三个部分（\0分割），以GET为例        GET\0http://....../judge.html\0HTTP/1.1\0\0
       if(strncasecmp(m_url,"http://",7)==0){
        m_url+=7;
        m_url=strchr(m_url,'/');//m_url指向最后一个\，其后面是请求的资源
       }

        if (strncasecmp(m_url, "https://", 8) == 0)
       {
        m_url += 8;
        m_url = strchr(m_url, '/');
       }
       /*网站请求页面不同 上述两种情况*/

        if (!m_url || m_url[0] != '/')//  /不存在或者不是斜杠 文本错误
        return BAD_REQUEST;

    //当url为/时，显示判断界面，m_url现在指向/
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");//如果/后面啥也没有了 就说明没资源 就追加资源，其他情况m_url已经保存/xxx.xxx资源 包含了/
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
 }

/*请求行后面请求头的每一行进行分析*/
http_conn::HTTP_CODE http_conn::parse_request_line(char* text){
    if(text[0]=='\0'){
        if(m_content_length!=0){//有消息体，状态机转换
            m_check_state=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
        //没有消息体 返回GET 直接进行请求回应
    }
    else if(strncasecmp(text,"Connection:",11)==0){
        text+=11;
        text+=strspn(text," \t");//跳过空格
        if(strcasecmp(text,"keep-alive")==0){
            m_linger=true;
        }
    }else if(strncasecmp(text,"Content-length:",15)==0){
        text+=15;//指针后移
        text+=strspn(text," \t");
        m_content_length=atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0){
    
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }  else{
      LOG_INFO("oop!unknow header: %s", text);//其他的就是未知信息的请求头
    }

    return NO_REQUEST;
}

/*每分析一个消息头的一行 成功分析完毕，就返回NO_REQUEST，下一次分析其他信息*/


http_conn::HTTP_CODE http_conn::parse_content(char* text){
    if(m_read_idx>=(m_content_length+m_checked_idx)){
        text[m_content_length]='\0';
        m_string=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
/*读到最后一行 分析最后一行 m_start_lie跟新，下一次循环。\r\n变为\0\0 此时继续 */
http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_stats=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char* text=0;
    while((m_check_state==CHECK_STATE_CONTENT&&line_stats==LINE_OK)||((line_stats=parse_line())==LINE_OK)){
        text=get_line();/*未更新前的m_start_line为上一次读取结束的角标，也是这一次读取完毕开始的角标*/
        m_start_line=m_checked_idx;
        LOG_INFO(text);
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
          {
            ret=parse_headers(text);
            if(ret==BAD_REQUEST){
                return BAD_REQUEST;
            }
            break;
          }
          case CHECK_STATE_HEADER:
          {
            ret= parse_request_line(text);
            if(ret==BAD_REQUEST){
                return BAD_REQUEST;
            }else if(ret==GET_REQUEST){
                return do_request();
            }
            break;
          }
          case CHECK_STATE_CONTENT:
          
          {
            ret=parse_content(text);
            if(ret==GET_REQUEST){
                return do_request();
            }
            line_stats = LINE_OPEN;
            break;
          }
        
        default:
           return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;//没读到行 继续读
}

http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root);//doc_root：(提前已经把资源放在一起了) 资源所在的正真目录
    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);
    const char *p = strrchr(m_url, '/');//指向m_url的最后一个\ ,其本来就只有一个

    //处理cgi,POST
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);// 因为开头是/2   或者/3 所以跳过，后面的是资源名 现在m_url_real是  /资源名
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);//在资源所在的正真目录上添加m_url上的资源 以此完成资源的完整路今
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())
            {
                std::unique_lock<std::mutex> locker(m_tex);
                int res = mysql_query(mysql, sql_insert);
                users.insert(std::pair<std::string, std::string>(name, password));
                locker.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
}    
      if (*(p + 1) == '0')
     {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

/*上面都是确定资源种类*/

/*先取得目标资源的文件信息*/
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);//打开目标资源
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);//进行映射目标资源（全部）
    close(fd);
    return FILE_REQUEST;
}
/*根据不同文件请款做不同准备*/

void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
bool http_conn::write()
{
    int temp = 0;
    if (bytes_to_send == 0)//没有发送的 继续监听
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_Mode);
        init();
        return true;
    }
   
    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_Mode);//非阻塞，继续触发写事件
                return true;
            }
            unmap();//调用失败
            error=false;
            LOG_ERROR("writev() errno");
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;//总共需要发送的字节数，进行更新，不是数组里的
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;//数组1的内容发送完毕 长度设为零
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);//理想情况就是m_write_idx=byte_have_send
            m_iv[1].iov_len = bytes_to_send;//更新数组为第二部分需要发送的字节数，即资源文本内容字节数
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;//未发送完毕，不断更新发送指针和需要发送的字节数（数组里的字节数）
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)//发送完毕
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_Mode);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                LOG_ERROR("no m_linger");
                return false;
            }
        }
    }
}

bool http_conn::add_response(const char *format, ...)/*组成请求报文写在m_write_buf中*/
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;//写入，不断更新w_write_idx 
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);

    return true;
}
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);//先写入内容 再添加\r\n
}
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()/*类比请求报文 组装完毕回应报文后还要添加\r\n阻隔内容*/
{
    return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    /*错误报文格式：错误回应头部\r\n+错误文本长度\r\n+是否保持长时间连接\r\n+\r\n+错误文本内容   都组装在m_write_buf中,其长度为m_write_idx*/
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;//回应报文除了文本的资源内容的部分，即回应报文字段，不算资源字段，存放在iovce中
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
            /*正确回应报文格式和错误报文格式相同，但是组装的回应报文的报文体是请求资源的资源内容，没有储存在write_buf中，而是在iovce结构体中，组成完整的报文发过去*/
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    /*不是资源的回应报文，进行组装在iovce结构体中*/
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;//更新
    return true;
}
/*不要忘了更新bytes_to_send*/
void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {    /*NOREQUEST：没有需要回复 继续监听*/
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_Mode);
        return;
    }
    bool write_ret = process_write(read_ret);//有回复 根据不同的情况进行组装回复报文
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_Mode);//改为写事件
}

