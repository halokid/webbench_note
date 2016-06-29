/*
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   webbench --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, fork failed
 * 
 */ 

// 非系统内置
#include "socket.c"

// 系统内置头文件
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>

/* values */
volatile int timerexpired=0;      // 声明编译器每次都要去重新读取 timerexpired， 不为此变量添加任何优化代码
int speed=0;
int failed=0;
int bytes=0;
/* globals */
int http10=1; /* 0 - http/0.9, 1 - http/1.0, 2 - http/1.1 */
/* Allow: GET, HEAD, OPTIONS, TRACE */
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "1.5"
int method=METHOD_GET;
int clients=1;
int force=0;
int force_reload=0;
int proxyport=80;
char *proxyhost=NULL;
int benchtime=30;
/* internal */
int mypipe[2];
char host[MAXHOSTNAMELEN];      //host 数组

#define REQUEST_SIZE 2048
char request[REQUEST_SIZE];     // request 数组

static const struct option long_options[]=      //静态结构体, 设定 option 结构体里面的  long_options 数组, 设定参数的属性
{
 {"force",no_argument,&force,1},
 {"reload",no_argument,&force_reload,1},
 {"time",required_argument,NULL,'t'},
 {"help",no_argument,NULL,'?'},
 {"http09",no_argument,NULL,'9'},
 {"http10",no_argument,NULL,'1'},
 {"http11",no_argument,NULL,'2'},
 {"get",no_argument,&method,METHOD_GET},
 {"head",no_argument,&method,METHOD_HEAD},
 {"options",no_argument,&method,METHOD_OPTIONS},
 {"trace",no_argument,&method,METHOD_TRACE},
 {"version",no_argument,NULL,'V'},
 {"proxy",required_argument,NULL,'p'},
 {"clients",required_argument,NULL,'c'},
 {NULL,0,NULL,0}
};

/* prototypes */
static void benchcore(const char* host,const int port, const char *request);
static int bench(void);
static void build_request(const char *url);

static void alarm_handler(int signal)
{
   timerexpired=1;
}	

//输出命令行的 help 封装成函数
static void usage(void)
{
   fprintf(stderr,
	"webbench [option]... URL\n"
	"  -f|--force               Don't wait for reply from server.\n"
	"  -r|--reload              Send reload request - Pragma: no-cache.\n"
	"  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
	"  -p|--proxy <server:port> Use proxy server for request.\n"
	"  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
	"  -9|--http09              Use HTTP/0.9 style requests.\n"
	"  -1|--http10              Use HTTP/1.0 protocol.\n"
	"  -2|--http11              Use HTTP/1.1 protocol.\n"
	"  --get                    Use GET request method.\n"
	"  --head                   Use HEAD request method.\n"
	"  --options                Use OPTIONS request method.\n"
	"  --trace                  Use TRACE request method.\n"
	"  -?|-h|--help             This information.\n"
	"  -V|--version             Display program version.\n"
	);
};

//主函数
int main(int argc, char *argv[])
{
 int opt=0;
 int options_index=0;
 char *tmp=NULL;

 if(argc==1)
 {
	  usage();
    return 2;
 } 

 // 这个 while 的逻辑是为 获取参数
 while((opt=getopt_long(argc,argv,"912Vfrt:p:c:?h",long_options,&options_index))!=EOF )
 {  
  switch(opt)
  {
   case  0 : break;
   case 'f': force=1;break;
   case 'r': force_reload=1;break; 
   case '9': http10=0;break;
   case '1': http10=1;break;
   case '2': http10=2;break;
   case 'V': printf(PROGRAM_VERSION"\n");exit(0);
   case 't': benchtime=atoi(optarg);break;	   // optarg 为 getopt.h 组件内置    
   case 'p':    //代理设置
	     /* proxy server parsing server:port */
	     tmp=strrchr(optarg,':');
	     proxyhost=optarg;     //取得代理服务器
	     if(tmp==NULL)         // 如果不符合代理的设置格式，退出程序
	     {
		     break;
	     }
	     if(tmp==optarg)
	     {
		     fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
		     return 2;
	     }
	     if(tmp==optarg+strlen(optarg)-1)
	     {
		     fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
		     return 2;
	     }
	     *tmp='\0';
	     proxyport=atoi(tmp+1);break;
   case ':':
   case 'h':
   case '?': usage();return 2;break;
   case 'c': clients=atoi(optarg);break;
  }
}    // END WHILE
 
 if(optind==argc) {
   fprintf(stderr,"webbench: Missing URL!\n");
	 usage();
	 return 2;
 }

 if(clients==0) clients=1;
 if(benchtime==0) benchtime=60;
 /* Copyright */
 fprintf(stderr,"Webbench - Simple Web Benchmark "PROGRAM_VERSION"\n"
	 "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
	 );

/**
上面一大段逻辑都是为了 获取参数 和 输出信息的， 下面才开始真正的主功能逻辑
**/   
   
// 执行完  build_request 之后， request就组成好了, request 就是HTTP头信息
 build_request(argv[optind]);
 
 
 /* print bench info */
 printf("\nBenchmarking: ");
 switch(method)
 {
	 case METHOD_GET:
	 default:
		 printf("GET");break;
	 case METHOD_OPTIONS:
		 printf("OPTIONS");break;
	 case METHOD_HEAD:
		 printf("HEAD");break;
	 case METHOD_TRACE:
		 printf("TRACE");break;
 }
 printf(" %s",argv[optind]);
 
 switch(http10)
 {
	 case 0: printf(" (using HTTP/0.9)");break;
	 case 2: printf(" (using HTTP/1.1)");break;
 }
 printf("\n");
 
 
 if(clients==1) printf("1 client");
 else
   printf("%d clients",clients);

 printf(", running %d sec", benchtime);
 if(force) printf(", early socket close");
 if(proxyhost!=NULL) printf(", via proxy server %s:%d",proxyhost,proxyport);
 if(force_reload) printf(", forcing reload");
 printf(".\n");
 /**
  上面的逻辑都还是在根据参数， 输出到命令行的内容而已， 而还没有真正执行了 访问HTTP 的逻辑
 **/
 return bench();          // 真正进行HTTP 的逻辑的函数, 直接在 main 函数里面 返回回来
}   // END MAIN FUNC



void build_request(const char *url)
{
  char tmp[10];     //定义临时变量， 函数内使用
  int i;

  // host, request 都是在 main 函数外已经定义好的变量
  bzero(host,MAXHOSTNAMELEN);
  bzero(request,REQUEST_SIZE);

  if(force_reload && proxyhost!=NULL && http10<1) http10=1;
  if(method==METHOD_HEAD && http10<1) http10=1;
  if(method==METHOD_OPTIONS && http10<2) http10=2;
  if(method==METHOD_TRACE && http10<2) http10=2;

  // 根据不同的 method 定义 request 的值
  switch(method)
  {
	  default:
	  case METHOD_GET: strcpy(request,"GET");break;
	  case METHOD_HEAD: strcpy(request,"HEAD");break;
	  case METHOD_OPTIONS: strcpy(request,"OPTIONS");break;
	  case METHOD_TRACE: strcpy(request,"TRACE");break;
  }
		  
  strcat(request," ");      //拼接字符串

  if(NULL==strstr(url,"://"))
  {
	  fprintf(stderr, "\n%s: is not a valid URL.\n",url);
	  exit(2);
  }
  if(strlen(url)>1500)
  {
         fprintf(stderr,"URL is too long.\n");
	 exit(2);
  }
  if(proxyhost==NULL)
	   if (0!=strncasecmp("http://",url,7)) 
	   { fprintf(stderr,"\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
             exit(2);
           }
  /* protocol/host delimiter */
  i=strstr(url,"://")-url+3;
  /* printf("%d\n",i); */

  if(strchr(url+i,'/')==NULL) {
                                fprintf(stderr,"\nInvalid URL syntax - hostname don't ends with '/'.\n");
                                exit(2);
                              }
  if(proxyhost==NULL)
  {
   /* get port from hostname */
   if(index(url+i,':')!=NULL &&
      index(url+i,':')<index(url+i,'/'))
   {
	   strncpy(host,url+i,strchr(url+i,':')-url-i);
	   bzero(tmp,10);
	   strncpy(tmp,index(url+i,':')+1,strchr(url+i,'/')-index(url+i,':')-1);
	   /* printf("tmp=%s\n",tmp); */
	   proxyport=atoi(tmp);
	   if(proxyport==0) proxyport=80;
   } else
   {
     strncpy(host,url+i,strcspn(url+i,"/"));
   }
   // printf("Host=%s\n",host);
   strcat(request+strlen(request),url+i+strcspn(url+i,"/"));
  } else
  {
   // printf("ProxyHost=%s\nProxyPort=%d\n",proxyhost,proxyport);
   strcat(request,url);
  }
  
/**
上次的逻辑都是一些字符串的操作， 用于获取各种参数， 判断各种参数的情况的, 获取参数赋值之后，  执行下面的逻辑
总的来说上面的处理参数的逻辑没太大的亮点， 都是一般的逻辑处理， 可参考一下 处理字符串的方法
**/  
  
/**
 组合 HTTP 协议内容， reques就是最后组合出来的内容
**/  
  if(http10==1)
	  strcat(request," HTTP/1.0");
  else if (http10==2)
	  strcat(request," HTTP/1.1");
  strcat(request,"\r\n");         //组合 HTTP 方法和 版本
  
  if(http10>0)
	  strcat(request,"User-Agent: WebBench "PROGRAM_VERSION"\r\n"); // 定义 agent
    
  if(proxyhost==NULL && http10>0)
  {
	  strcat(request,"Host: ");
	  strcat(request,host);
	  strcat(request,"\r\n");
  }
  
  if(force_reload && proxyhost!=NULL)
  {
	  strcat(request,"Pragma: no-cache\r\n");
  }
  
  if(http10>1)
	  strcat(request,"Connection: close\r\n");
  /* add empty line at end */
  if(http10>0) strcat(request,"\r\n"); 
  // printf("Req=%s\n",request);
}

/**
 上面的逻辑是 根据获取的参数 组合 HTTP 请求的头, 类似  
 ---------------------------------------------------------
 
GET /goods/list/4404 HTTP/1.1
Host: www.xxx.com
Connection: keep-alive
Cache-Control: max-age=0
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp

 --------------------------------------------------------
**/


/* vraci system rc error kod */
/**
 真正的执行 HTTP 的逻辑, 主功能的逻辑
**/
static int bench(void)
{
  int i,j,k;	
  pid_t pid=0;
  FILE *f;

  /* check avaibility of target server */
  //检查主机的可用性
  // Socket 在 socket.c 声明
  i=Socket( (proxyhost==NULL) ? host : proxyhost,proxyport);
  if(i<0) { 
	  fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
    return 1;
  }
  close(i);     //检查 主机不用可，关闭socket
  
  
  /* create pipe */
  // 创建管道
  // mypipe 为长度2 的数组， 创建两个管道？？
  if(pipe(mypipe))
  {
	  perror("pipe failed.");
	  return 3;
  }

  /* not needed, since we have alarm() in childrens */
  /* wait 4 next system clock tick */
  /*
  cas=time(NULL);
  while(time(NULL)==cas)
        sched_yield();
  */

  /* fork childs */
  //NOTE 这里开始循环生成子进程
  // 每一个 请求 HTTP 的client 就创建一个子进程
  // 循环有多少个 clients ，就 fork 多少个
  for(i=0;i<clients;i++)
  {
	   pid=fork();
    
     // 无论子进程创建成功 还是 失败， 都 sleep 一秒 
	   if(pid <= (pid_t) 0)
	   {
		   /* child process or error*/
       /**
        经典的 sleep(1) 的例子， 当执行 sleep(1) 的时候， 程序就告诉CPU 先不用进行运算，而去计算其他的，不至于因为这个 fork 逻辑而导致CPU占用太高， 也利于CPU去回收之前fork了的一些子进程（已经无用的子进程），利于系统GC， 假如不加这个，当fork逻辑多的时候，可能会造成CPU占用太高，阻塞等
       **/
	     sleep(1); /* make childs faster */
		   break;
	   }
  }     //END FOR  
  /**
    上面的 for 循环不断生成子进程， 然后 pid 获取的子进程的进程号，每一次fork成功之后，都会走下面的逻辑，匹配到  pid < (pid_t) 0
    每一个 fork 的子进程都会走这个 if 的逻辑
    
  **/

  if( pid< (pid_t) 0)     // 如果子进程创建失败
  {
    fprintf(stderr,"problems forking worker no. %d\n",i);
	  perror("fork failed.");
	  return 3;
  }

  // NOTE 上面 for循环 fork 的子进程都会匹配到这个逻辑，执行下面代码
  if(pid== (pid_t) 0)       // 如果子进程创建成功
  {
    /* I am a child */
    // 子进程开始执行， 这里是真正进行 socket 通信的逻辑， 真正进行 HTTP 通信的逻辑
    if(proxyhost==NULL)
      benchcore(host, proxyport, request);
    else
      benchcore(proxyhost, proxyport, request);
      
   /**  
   //NOTE
   执行完 benchcore 之后， 改变了  bytes, speed 的值， 这两个值都是全局的
   **/

   /* write results to pipe */
   /**
   假如
   **/
	 f=fdopen(mypipe[1],"w");
	 if(f==NULL)
	 {
		 perror("open pipe for writing failed.");
		 return 3;
	 }
	 /* fprintf(stderr,"Child - %d %d\n",speed,failed); */
	 fprintf(f,"%d %d %d\n",speed,failed,bytes);     //NOTE 有多少个 子进程， 就在写入管道多少次, 信息在管理储存是队列的方式
	 fclose(f);
	 return 0;
  } 
  else        // NOTE 上面的逻辑会一直执行 FOR 循环生成的 子进程， 跑完上面的子进程之后，开始进入到父进程
  {
	  f=fdopen(mypipe[0],"r");
	  if(f==NULL) 
	  {
		  perror("open pipe for reading failed.");
		  return 3;
	  }
	  setvbuf(f, NULL, _IONBF, 0);    // 把缓冲区与流相关好， _IONBF 是表示直接从流中读入数据或直接向流中写入数据，而没有缓冲区
	  speed=0;
    failed=0;
    bytes=0;

	  while(1)
	  {
		  pid=fscanf(f,"%d %d %d",&i,&j,&k);    // NOTE  循环读取管道队列的内容
		  if(pid<2)
      {
        fprintf(stderr,"Some of our childrens died.\n");
        break;
      }
      
      // 整合内容数据
		  speed+=i;
		  failed+=j;
		  bytes+=k;
		  /* fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid); */
		  if(--clients==0) break;
	  }
	  fclose(f);

  //输出内容数据
  printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
		  (int)((speed+failed)/(benchtime/60.0f)),
		  (int)(bytes/(float)benchtime),
		  speed,
		  failed);
  }
  return i;
}


/**
每个成功创建的子进程都会 单独执行一次这个函数
**/
void benchcore(const char *host, const int port, const char *req)
{
 int rlen;
 char buf[1500];
 int s,i;
 struct sigaction sa;     //sigaction 结构体

 /* setup alarm signal handler */
 sa.sa_handler=alarm_handler;       // 当获取 SIGALRM 信号的时候的处理函数
 sa.sa_flags=0;
 if(sigaction(SIGALRM,&sa,NULL))      // sigaction 函数 配合 sigaction 结构体用法
    exit(3);
 alarm(benchtime);          // 延迟 benchtime  给系统发送 SIGALRM 信号

 rlen=strlen(req);        //获取总的请求的长度
 
 nexttry:while(1)
 {
    if(timerexpired)        // 假如执行了 alarm_handler 函数之后,  timerexpired数值会为 1
    {
       if(failed>0)
       {
          /* fprintf(stderr,"Correcting failed by signal\n"); */
          failed--;
       }
       return;
    }
  
    // fail 是记录 webbench 请求socket的过程当中的错误次数的
    
    s=Socket(host,port);                    // 生成 socket 句柄
    if(s<0) { failed++;continue;} 
    
    if(rlen!=write(s,req,rlen)) {failed++;close(s);continue;}   // 把请求写入socket句柄
    
    if(http10==0) 
	    if(shutdown(s,1)) { failed++;close(s);continue;}
      
    if(force==0) 
    {
            /* read all available data from socket */
	    while(1)
	    {
        if(timerexpired) break;       //这个判断是 假如子进程已经执行了 alarm_handler 函数之后, timerexpired 为 1,那么就不要再重复执行下面的逻辑了
        
	      i=read(s,buf,1500);
        /* fprintf(stderr,"%d\n",i); */
	      if(i<0)        // 如果读取 socket 失败
        { 
          failed++;
          close(s);
          /**
            这里假如读取不了 HTTP 请求返回的数据，就关闭 socket连接， 然后再继续生成socket，重新请求 HTTP 服务器, 直到读取到数据
          **/
          goto nexttry;       // 读取失败的话, 重复执行上面的逻辑
        }
	      else           // 如果 HTTP 不到任何数据
		      if(i==0) break;
		    else          //  如果 HTTP 正常请求到数据
			    bytes+=i;
	    }      // END WHILE
    }
    if(close(s)) {failed++;continue;}
    speed++;
 }
}
