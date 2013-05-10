#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include"ae.h"
#include<string.h>
#include"anet.h"
#include"websocket.h"



void myData(websocketClient *c)
{
            if(strlen(c->data_frame.payload)>0)
            {
                sds mm2=sdsdup(c->data_frame.payload);

                sds mm=formatted_websocket_frame(mm2);
                sendMsg(server.clients,mm);
                sdsfree(mm);
                sdsfree(mm2);
 
                Log(RLOG_DEBUG,"desc=querylen len=%d",sdslen(c->querybuf));

            }

}
int main(void)
{

    char *ip="10.26.190.45";
    int port=8413;
    server.bindaddr=ip;
    server.port=port;

    server.ping_interval=5;
    server.maxidletime=15;
    server.onData=myData;
    initServer();

    aeMain(server.el);
    aeDeleteEventLoop(server.el);
	return 0;
}
