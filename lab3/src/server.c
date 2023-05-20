// server.c
#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>



#define BIND_IP_ADDR "127.0.0.1"
#define BIND_PORT 8000
#define MAX_RECV_LEN 1048576
#define MAX_SEND_LEN 1048576
#define MAX_PATH_LEN 1024
#define MAX_HOST_LEN 1024
#define MAX_CONN 20
#define MAX_WORD 50
int flag;
const char* GET="GET";
const char* http_head="HTTP/1.0";
const char* host_addr="127.0.0.1:8000";
const char* two_LF="\r\n\r\n";
#define HTTP_STATUS_200 "200 OK"
#define HTTP_STATUS_404 "404 Not Found"
#define HTTP_STATUS_500 "500 Internal Server Error"
#define TASK_QUEUE_SIZE 100
#define THREAD_POOL_SIZE 10
typedef struct {
    void (*fun)(void *);
    void *args;
} task_t;

typedef struct {
    task_t *tasks;
    int front;  //head pointer
    int rear; // tail pointer
    int count_task;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    pthread_t *threads;
    int work_threads;
    pthread_mutex_t work_threads_lock;
} thread_pool;

//get task from thread_pool
void* get(void *args)
{ 
    thread_pool* pool = (thread_pool*)args;
     while (1) {
        pthread_mutex_lock(&pool->mutex);
        while (pool->count_task==0) {//no task
              pthread_cond_wait(&pool->not_empty, &pool->mutex);
            }
        task_t* task;
        task= (task_t*)malloc(sizeof(task_t));
        task->fun = pool->tasks[pool->front].fun;
        task->args= pool->tasks[pool->front].args;
        pool->front = (pool->front + 1) % TASK_QUEUE_SIZE;
        pool->count_task--;
        // deliver signal
        pthread_cond_signal(&pool->not_full);
      //unlock mutex
        pthread_mutex_unlock(&pool->mutex);
        task->fun(task->args);
        free(task);
    }
}
//add task to thread_pool
void *add(thread_pool* pool, void(*fun)(void*), void* args)
{  //lock mutex
    pthread_mutex_lock(&pool->mutex);
    while (pool->count_task == TASK_QUEUE_SIZE ) {
        pthread_cond_wait(&pool->not_full, &pool->mutex);
    }
    pool->tasks[pool->rear].fun = fun;
    pool->tasks[pool->rear].args= args;
    pool->rear = (pool->rear+ 1) % TASK_QUEUE_SIZE;
    pool->count_task++;
    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->mutex);
}
//init thread_pool
void thread_pool_init(thread_pool* init_pool)
{   init_pool->tasks=  (task_t *)malloc(TASK_QUEUE_SIZE*sizeof(task_t));
    init_pool->front=init_pool->rear=init_pool->work_threads=0;
    pthread_mutex_init(&init_pool->mutex, NULL);
    pthread_cond_init(&init_pool->not_empty, NULL);
    pthread_cond_init(&init_pool->not_full, NULL);
    init_pool->threads =  (pthread_t*)malloc(THREAD_POOL_SIZE*sizeof(pthread_t));
    pthread_mutex_init(&init_pool->work_threads_lock, NULL);
    for (int i = 0; i < THREAD_POOL_SIZE; i++) 
        pthread_create(&init_pool->threads[i], NULL, get, init_pool);//init thread
}

void parse_request(char* request, ssize_t req_len, char **path, ssize_t *path_len)
{
    char* req = request;
    ssize_t s1,s2;
    int sign=0;
    s1=s2=0;
    int i=0;
    for(i;;i++)
    {   sign=0;
        s2=s1;
        while(req[s1]!=' '&&s1<req_len&&req[s1]!='\r') 
        s1++;
        memcpy(path[i],&req[s2],(s1-s2)*sizeof(char));
        path_len[i]=s1-s2;
        if(req[s1]=='\r'&&req[s1+2]=='\r')
        {i+=1;
        memcpy(path[i],&req[s1],4*sizeof(char));
        path_len[i]=4;
        break;
        }
        else
        {  if(req[s1]=='\r')
            {i+=1;
            memcpy(path[i],&req[s1],2*sizeof(char));
             path_len[i]=2;
             sign=1;}
        }
        if(sign)
        s1+=2;
        else
        s1+=1;
        
    }
    for(int i=0;i<MAX_PATH_LEN;i++)
    {if(i<=2)
    path[0][i]=req[i];
    else
    path[0][i]='\0';}
    
}

void handle_clnt(void* args)
{   int fd;
    int clnt_sock = *(int*)args;
    flag=200;
    // 一个粗糙的读取方法，可能有 BUG！
    // 读取客户端发送来的数据，并解析
    char* req_buf = (char*) malloc(MAX_RECV_LEN * sizeof(char));
    // 将 clnt_sock 作为一个文件描述符，读取最多 MAX_RECV_LEN 个字符
    // 但一次读取并不保证已经将整个请求读取完整
    ssize_t req_len = read(clnt_sock, req_buf, MAX_RECV_LEN);
    // 根据 HTTP 请求的内容，解析资源路径和 Host 头
char** path = (char**)malloc(MAX_WORD * sizeof(char*));
// 分别为每行分配内存空间
for (int i = 0; i < MAX_WORD; i++) {
    path[i] = (char*)malloc(MAX_PATH_LEN * sizeof(char));
}
    ssize_t *path_len=(ssize_t *)malloc(MAX_WORD*sizeof(ssize_t));
    parse_request(req_buf, req_len, path, path_len);
if(strcmp(path[0],GET)!=0) {flag=500;printf("xayah\n");}
if(strcmp(path[5],host_addr)!=0) {flag=500;printf("kaisa\n");}
char *file=(char *)malloc(MAX_PATH_LEN*sizeof(char));
for (int j=0;j<strlen(path[1]);j++) file[j]=path[1][j+1]; //去掉最前面的/
fd=open(file,O_RDONLY);
if(fd==-1) flag=404;
else{
   struct stat fileStat;
   stat(file, &fileStat);
   if ((fileStat.st_mode & S_IFMT) == S_IFDIR)
    {flag=500;}
   else{
    char *file_dir;
    file_dir = dirname(file);
    if(strcmp(file_dir,".")!=0)
    {
    flag=500;}
    }
}
close(fd);
///
    // 构造要返回的数据
    // 这里没有去读取文件内容，而是以返回请求资源路径作为示例，并且永远返回 200
    // 注意，响应头部后需要有一个多余换行（\r\n\r\n），然后才是响应内容
   if(flag==200)
   {
    for (int j=0;j<strlen(path[1]);j++) file[j]=path[1][j+1]; //去掉最前面的/(dirname会改变path)
    FILE* fp = fopen(&file[0], "r");// open file
     fseek(fp, 0, SEEK_END);
    off_t file_length = ftell(fp);//get the length of the file
    fseek(fp, 0, SEEK_SET);
    char* file_in = (char*)malloc(file_length + 1);
    size_t len = fread(file_in, 1, file_length, fp);//get the content of the fie
    char* response = (char*)malloc((MAX_SEND_LEN + file_length) * sizeof(char)) ;
   sprintf(response,
        "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n%s",
        HTTP_STATUS_200, len, file_in);
        free(file_in);
        size_t response_len = strlen(response);
        fclose(fp);
        write(clnt_sock, response, response_len);
        close(clnt_sock);
        free(response);
        }
    if(flag==500)
    {  char* response = (char*)malloc((MAX_SEND_LEN ) * sizeof(char)) ;
        sprintf(response,
        "HTTP/1.0 %s\r\n\r\n",
        HTTP_STATUS_500);
        size_t response_len = strlen(response);
        free(response);
        write(clnt_sock, response, response_len);
        close(clnt_sock);
        }
    if(flag==404)
    {char* response = (char*)malloc((MAX_SEND_LEN ) * sizeof(char)) ;
        sprintf(response,
        "HTTP/1.0 %s\r\n\r\n",
        HTTP_STATUS_404);
        size_t response_len = strlen(response);
        free(response);
        write(clnt_sock, response, response_len);
        close(clnt_sock);
        }
    free(file);
    free(path_len);
    free(req_buf);
     for (int i = 0; i < MAX_WORD; i++) {
    free(path[i]);
    
}
    
}

int main(){
    // 创建套接字，参数说明：
    //   AF_INET: 使用 IPv4
    //   SOCK_STREAM: 面向连接的数据传输方式
    //   IPPROTO_TCP: 使用 TCP 协议
    int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // 将套接字和指定的 IP、端口绑定
    //   用 0 填充 serv_addr （它是一个 sockaddr_in 结构体）
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    //   设置 IPv4
    //   设置 IP 地址
    //   设置端口
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(BIND_IP_ADDR);
    serv_addr.sin_port = htons(BIND_PORT);
    //   绑定
    bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    // 使得 serv_sock 套接字进入监听状态，开始等待客户端发起请求
    listen(serv_sock, MAX_CONN);

    // 接收客户端请求，获得一个可以与客户端通信的新的生成的套接字 clnt_sock
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    thread_pool* pool;
    pool = (thread_pool*)malloc(sizeof(thread_pool));
    thread_pool_init(pool);
    while (1) // 一直循环
    {
        // 当没有客户端连接时， accept() 会阻塞程序执行，直到有客户端连接进来
        int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        int* p_clnt_sock = (int*)malloc(sizeof(int));
        *p_clnt_sock = clnt_sock;
        add(pool, handle_clnt, (void*)p_clnt_sock);
        // 处理客户端的请求
        
    }

    // 实际上这里的代码不可到达，可以在 while 循环中收到 SIGINT 信号时主动 break
    // 关闭套接字
    close(serv_sock);
    return 0;
}
