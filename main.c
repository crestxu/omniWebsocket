#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include"ae.h"
#include<string.h>
#include"anet.h"
#include"websocket.h"

int main(void)
{

    char *ip="10.26.190.45";
    int port=8413;
    server.bindaddr=ip;
    server.port=port;
    initServer();
    aeMain(server.el);
    aeDeleteEventLoop(server.el);
	return 0;
}
