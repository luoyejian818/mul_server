#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h> 
#include <unistd.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/wait.h>
#include "cJSON.h"

#define UPDATE_TIME 3
#define INTERVAL_TIME 24

#define MY_PORT   6103
#define SER_IP    "192.168.0.104"
#define BACKLOG  10  //允许最大连接数
#define MAX_ALIVE_TIME  10
#define MAXLINE   1024   //数组最大缓存
#define send_file  "/mnt/hgfs/share/net/save.txt"   //预发送的文件

#define DOWNLODE_FILE  1    //客户端下载文件请求
#define UPLODE_FILE    2    //客户端上传文件请求

FILE   *fp;  //定义文件打开描述符
char recbuf[1024];   //接收数据存放数组
int sockfd = -1;     //服务器socket描述符
int ret = -1;
socklen_t len = 0;
int clifd = -1;     //客户端描述符
char ipstr[128];    //存放客户端信息
struct sockaddr_in seraddr={0};
struct sockaddr_in cliaddr={0};
pid_t pid;
pid_t pid_two;
int stat_val;
int CMD_FLAG=0;  //客户端请求命令
char result[1024];

int Jsont_to_string(char *json,const char *Item,char *data_save)
{   
    int i=0;
	cJSON * root = cJSON_Parse(json);//将接收到的数据转为json结构体
	if(root == NULL)
	  {
		printf("parse error\n");
		exit(-1);
	  }
			 
	cJSON *value = cJSON_GetObjectItem(root, Item);//根据键"date"获取其对应的值
	if(value == NULL)
	 {
		printf("getvalue error\n");
		exit(-1);
	 }
 
	char *data = cJSON_Print(value);//将获取值转化为字符串格式
	if(data == NULL)
	{
	  printf("printf error\n");
	  exit(-1);
	}
	
	memcpy(data_save,data,strlen(data));
	for(i=0;i<(strlen(data_save)-2);i++) //删除数据开头的"字符 和结尾的"字符
			{
				data_save[i]=data_save[i+1];
			}
	data_save[strlen(data_save)-2]=0x00;	
	return strlen(data_save);
}   

int CMD_handle(void)  //处理来自客户端的数据
{	
	bzero(&recbuf,sizeof(recbuf));
	ret = 0;
	while(ret == 0)   //等待接收数据
	{
		ret = recv(clifd,recbuf,sizeof(recbuf),0);
	}
    if(ret<0)
	  {					
		perror("recv CMD");
		close(clifd);		//断开客户端		 
        exit(-1);	
	  }
	else  //收到并处理数据   //定义数据格式为json格式     {"CMD":"1","FILE_NAME":"/home/tony/Desktop/111.c"}
	{
		if(ret>0)
		{   
	        int cmd_num=0;	           //客户端请求命令
            char file_name[512]={0};   //客户端请求的相关文件名
            unsigned char file_data[1024]={0};   //客户端请求的相关文件内容
            //char file_data[10]={0};   //客户端请求的相关文件内容			
	        bzero(&result,sizeof(result));
			
	        Jsont_to_string(recbuf,"CMD",result);           //获取请求命令值
			Jsont_to_string(recbuf,"FILE_NAME",file_name);  //获取文件名

			cmd_num = atoi(result);
			printf("cmd_num   is %d\n",cmd_num);
			printf("file_name is %s\n",file_name);

			if(cmd_num == DOWNLODE_FILE)  //客户端请求下载文件
			{
			   ret = send(clifd, "OK", 2, 0);    //接收到命令后 给客户端反馈一个OK 之后  等待客户端返回OK  进入发送文件模式
               if ( ret < 0 )  //发送OK错误
                {  
                 perror("Send OK");
                 close(clifd);		//断开客户端		 
                 exit(-1);				 
                } 
				
			   else
			   {
				   ret = 0;
				   bzero(&recbuf,sizeof(recbuf));
				   while(ret == 0)   //等待接收数据
	               {
		             ret = recv(clifd,recbuf,sizeof(recbuf),0);
	               }
                   if(ret<0)
	               {					
		              perror("recv \"OK\"");
					  close(clifd);		//断开客户端
					  exit(-1);
	               }				   
				   else    //接收到客户端反馈的消息
				   {
					   if(strstr(recbuf,"OK"))   //客户端反馈的消息是OK   表示客户端已经进入文件接收模式   则服务器进入文件发送模式
					   {
						   ret = access(file_name,R_OK);
						   if(ret == 0)   //文件存在可读
						   {
							 if ((fp = fopen(file_name,"r")) == NULL) 
                                {  
                                   perror("Open file\n");
                                   close(clifd);		//断开客户端								   
								   exit(-1);
                                }  			
			                 bzero(file_data, sizeof(file_data)); 						 
							 while ((ret = fread(file_data, sizeof(char), (sizeof(file_data)), fp)) >0 )	 
                              {  
                                 ret = send(clifd, file_data, ret, 0);								 
                                 if ( ret < 0 ) 
                                 {  
                                    perror("Send file");
                                    close(clifd);		//断开客户端									
                                    exit(-1); 
                                 }  
                                 bzero(file_data, sizeof(file_data));							 
                              }
			                 printf("send file over\r\n");			
			                 fclose(fp);   
						   }
						   else
						   {
							   perror("file access");
							   close(clifd);		//断开客户端
							   exit(-1);
						   }
					   }
				   }
			   }
			}
			
			else
			{
				printf("CMD worng.\r\n");
				close(clifd);		//断开客户端
			}
		}
	}
	  
}

int main(void)
{
	//第一步  打开socket文件描述符
	 int optval = 1;
    sockfd = socket(AF_INET,SOCK_STREAM,0);  //AF_INET 指用IPV4   SOCK_STREAM 指用TCP协议
	if(sockfd == -1)
	{
		perror("socket");
		exit(-1);
	}
	
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)   //屏蔽程序退出时  bind: Address already in use  的问题
    {
      perror("setsockopt1 error");
      exit(-1);
    }
	printf("socket is %d\r\n",sockfd);
	
	//第二步 bind绑定sockefd和当前电脑的IP地址和断开号
	bzero(&seraddr,sizeof(seraddr));
	seraddr.sin_family = AF_INET; //设置地址族为IPV4;
	seraddr.sin_port = htons(MY_PORT);              //定义端口
	seraddr.sin_addr.s_addr = inet_addr(SER_IP);    //设置本地IP
	ret=bind(sockfd,(const struct sockaddr *)&seraddr,sizeof(seraddr));
	if(ret < 0)
	{
		perror("bind");
		exit(-1);
	}
	
	printf("bind success.\r\n");
	//第三步  listen监听端口
       ret = listen(sockfd,BACKLOG); 
       if(ret < 0)
	   {
		perror("lisen");
		exit(-1);
	   }
	

    memset(recbuf,0,sizeof(recbuf));	
	while(1)
	{			
	  ////第四步 accept阻塞等待
	   len=sizeof(cliaddr);
	   clifd = accept(sockfd,(struct sockaddr *)&cliaddr,&len); //阻塞等待客户端来连接服务器
	   if(clifd<0)
	   {
		  perror("accept");
		  exit(-1);
	   }
	   
	  printf("client ip %s , port %d is online.\r\n",
	        inet_ntop(AF_INET,(struct sockaddr *)&cliaddr.sin_addr.s_addr,ipstr,sizeof(ipstr)),
		    ntohs(cliaddr.sin_port));   //提示上线的客户端

	  pid = fork();  //创建子进程
      if(pid == 0)//子进程
		{	
          int pid_two = fork();
           if (pid_two < 0)
            {
              perror("fork");
              return 0;
            }
		    if (pid_two == 0)
            {
            // 用孙子进程处理业务，为了保证后面的其它进程继续能执行
			   close(sockfd);
		       while(1)  //循环读取客户端发来的数据，将收到的数据转发
			   {
				  CMD_handle();
		       }
			}
		  else  //父进程关闭文件描述符，释放资源
		    {
			  close(clifd);
              waitpid(-1, &stat_val, 0);			  
            }	  
	    }	  
	   
    }
	 return 0;
}


//{"CMD":"1","FILE_NAME":"/home/tony/Desktop/111.c"}
//gcc mul_server.c  -o mul_server   -I /home/tony/cJSON/include/cjson/ -L /home/tony/cJSON/lib -lcjson
//export LD_LIBRARY_PATH=/home/tony/cJSON/lib 