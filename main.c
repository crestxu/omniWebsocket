#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include"ae.h"
#include<string.h>
#include"anet.h"

char errMsg[1024];
void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask);
void readClient(aeEventLoop *el, int fd, void *privdata, int mask){
	 char buf[1024]={0};    
	 int nread;    
	 nread = read(fd, buf, 1024);
//	 aeDeleteFileEvent(el,fd,AE_READABLE);
     if(nread<=0){
  //      printf("close fd %d\n",fd);
		 aeDeleteFileEvent(el,fd,AE_READABLE);
		 close(fd);
	 }
	 else{
		// printf("%d\n",nread);
//		printf("read from client %s",buf);
		aeCreateFileEvent(el,fd, AE_WRITABLE,sendReplyToClient, buf);
    }
}
void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask){
//	char *ss=(char *)privdata;
  char buffer[]="HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nhello world\r\n";

	write(fd,buffer,strlen(buffer));
  //write(fd,ss,strlen(ss));
	aeDeleteFileEvent(el,fd,AE_WRITABLE);

	aeDeleteFileEvent(el,fd,AE_READABLE);
	close(fd);
}
void tcphandler(aeEventLoop *el, int fd, void *privdata, int mask) {
	int cport,cfd;
	char cip[128];
	cfd=anetTcpAccept(errMsg, fd, cip, &cport);
	anetNonBlock(NULL,cfd); 
	anetTcpNoDelay(NULL,cfd);
	if (aeCreateFileEvent(el,cfd,AE_READABLE,readClient,NULL) == AE_ERR){
		  close(cfd);
		  printf("error accept a connection!\n");
	}


}


int print5(struct aeEventLoop *loop, long long id, void *clientData)
{
	    printf("Hello, World\n");
		    return 1000;                 /* 返回 -1 很重要，否则会出错 */
}

int main(void)
{

	aeEventLoop *loop = aeCreateEventLoop();  /* 创建5秒超时事件，处理函数是print5 */
	aeCreateTimeEvent(loop, 1000, print5, NULL, NULL);
	int fd;
	fd=anetTcpServer(errMsg,8876,"0.0.0.0");
	if(fd<0){
		printf("create socket error!\n");
		exit(-1);
	}
	aeCreateFileEvent(loop,fd,AE_READABLE,tcphandler,NULL);
	aeMain(loop);              /* 启动主循环 */
	aeDeleteEventLoop(loop);
	return 0;
}
