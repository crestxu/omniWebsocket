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
    

    int i=0;

   
    handshake_frame_t handshake_frame;
    while(sdslen(querybuf)>0)
    {


        char *newline = strstr(querybuf,"\r\n");
        int argc, j;
        sds *argv;
        size_t querylen;

        if (newline == NULL)
            return WEBSOCKET_ERR;

        querylen =newline - querybuf;
        argv = sdssplitlen(querybuf,querylen," ",1,&argc);

        if(argc==0) break;

        querybuf = sdsrange(querybuf,querylen+2,-1);

        if(i==0) //first line 
        {
            if(argc==3)
            {
                handshake_frame.Method=argv[0];
                handshake_frame.Uri=argv[1];
                handshake_frame.Version=argv[2];
            }
            else
            {
                 Log(RLOG_WARNING,"bad params in first head of websocket");
                 return  WEBSOCKET_ERR;
            }

        }
        else //other line
        {
            if(argc==2)
            {
                   switch(argv[0][0])
                   {
                       case 'u':
                       case 'U':
                           handshake_frame.Upgrade=argv[1];
                           break;
                       case 'c':
                       case 'C':
                           handshake_frame.Connection=argv[1];
                           break;
                       case 'h':
                       case 'H':
                           handshake_frame.Host=argv[1];
                           break;
                       case 's':
                       case 'S':
                           if(!strcasecmp(argv[1],WEBSOCKET_SEC_WEBSOCKET_VERSION)){
                               handshake_frame.Sec_WebSocket_Version=argv[1];
                           }else if(!strcasecmp(argv[1],WEBSOCKET_SEC_WEBSOCKET_KEY)){
                               handshake_frame.Sec_WebSocket_Key=argv[1];
                           }else if(!strcasecmp(argv[1],WEBSOCKET_SEC_WEBSOCKET_ORIGIN)){
                               handshake_frame.Sec_WebSocket_Origin=argv[1];
                           }
                           break;
                       default:
                           break;

                   }


            }
            else
            {
                 Log(RLOG_WARNING,"bad params in  head of websocket: %d",argc);
                 return  WEBSOCKET_ERR;
            }

        }
        if(i++>WEBSOCKET_MAX_HEAD_LEVEL)
        {
            Log(RLOG_WARNING,"error head in websocket");
            return  WEBSOCKET_ERR;
        }


/*       for (j = 0; j < argc; j++) {
            if (sdslen(argv[j])) {
                printf("sds: %s\r\n",argv[j]);
            } else {
    
                printf("sds: space%s\r\n",argv[j]);
                sdsfree(argv[j]);
            }
        }*/
        zfree(argv);
    }


    printf("Method:%s\r\nUri:%s\r\nVersion:%s\r\nConnection:%s\r\nSec_key:%s\r\nSec_vesion:%s\r\nSec_origin:%s\r\n",
            handshake_frame.Method,handshake_frame.Uri,handshake_frame.Version,handshake_frame.Connection,
            handshake_frame.Sec_WebSocket_Key,handshake_frame.Sec_WebSocket_Version,handshake_frame.Sec_WebSocket_Origin);
    sdsfree(querybuf);
 
}

int main(void)
{

    testHandShake();
	return 0;
}
