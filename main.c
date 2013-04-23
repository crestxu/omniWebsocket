#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include"ae.h"
#include<string.h>
#include"anet.h"
#include"websocket.h"

int main(void)
{

    char *ip="127.0.0.1";
    int port=8413;
    server.bindaddr=ip;
    server.port=port;
    initServer();
    aeMain(server.el);
    aeDeleteEventLoop(server.el);
	return 0;
}
