#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include"ae.h"
#include<string.h>
#include"anet.h"
#include"websocket.h"
int testHandShake()
{
    sds querybuf=sdsnew("GET /demo HTTP/1.1\r\nHost: example.com\r\nConnection: Upgrade\r\nHost: 10.15.1.218:12345\r\nSec-WebSocket-Origin: null\r\nSec-WebSocket-Key: 4tAjitqO9So2Wu8lkrsq3w==\r\nSec-WebSocket-Version: 8\r\n\r\n"); 
   handshake_frame_t handshake_frame;
   parseWebSocketHead(querybuf,&handshake_frame);
   sdsfree(querybuf);
    printf("Method:%s\r\nUri:%s\r\nVersion:%s\r\nConnection:%s\r\nSec_key:%s\r\nSec_vesion:%s\r\nSec_origin:%s\r\n",
            handshake_frame.Method,handshake_frame.Uri,handshake_frame.Version,handshake_frame.Connection,
            handshake_frame.Sec_WebSocket_Key,handshake_frame.Sec_WebSocket_Version,handshake_frame.Sec_WebSocket_Origin);
}

int main(void)
{

    testHandShake();
	return 0;
}
