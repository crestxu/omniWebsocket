#include"websocket.h"

struct websocketServer server; /* server global state */
void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cport, cfd;
    char cip[128];
    WEBSOCKET_NOTUSED(el);
    WEBSOCKET_NOTUSED(mask);
    WEBSOCKET_NOTUSED(privdata);

    cfd = anetTcpAccept(server.neterr, fd, cip, &cport);
    if (cfd == AE_ERR) {
        Log(RLOG_VERBOSE,"Accepting client connection: %s", server.neterr);
        return;
    }
    Log(RLOG_VERBOSE,"Accepted %s:%d", cip, cport);
    //acceptCommonHandler(cfd);
}
int initServer()
{
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);


    server.mainthread = pthread_self();
    server.clients = listCreate();

    server.el = aeCreateEventLoop();

    if (server.port != 0) {
        server.ipfd = anetTcpServer(server.neterr,server.port,server.bindaddr);
        if (server.ipfd == ANET_ERR) {
            Log(RLOG_WARNING, "Opening port: %s", server.neterr);
            exit(1);
        }
    }
    if (server.ipfd < 0) {
        Log(RLOG_WARNING, "Configured to not listen anywhere, exiting.");
        exit(1);
    }
    //aeCreateTimeEvent(server.el, 1, serverCron, NULL, NULL);
    if (server.ipfd > 0 && aeCreateFileEvent(server.el,server.ipfd,AE_READABLE,
                acceptTcpHandler,NULL) == AE_ERR) oom("creating file event");

    return 0;
}
int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    int j, loops = server.cronloops;
    WEBSOCKET_NOTUSED(eventLoop);
    WEBSOCKET_NOTUSED(id);
    WEBSOCKET_NOTUSED(clientData);

    server.unixtime = time(NULL);


    /* Show information about connected clients */
    if (!(loops % 50)) {
        Log(RLOG_VERBOSE,"%d clients connected",
                listLength(server.clients));
    }

    /* Close connections of timedout clients */
    if ((server.maxidletime && !(loops % 100)))
        closeTimedoutClients();

    server.cronloops++;
    return 100;
}



void closeTimedoutClients(void) {
    websocketClient *c;
    listNode *ln;
    time_t now = time(NULL);
    listIter li;

    listRewind(server.clients,&li);
    while ((ln = listNext(&li)) != NULL) {
        c = listNodeValue(ln);
        if (server.maxidletime &&
                (now - c->lastinteraction > server.maxidletime))
        {
            Log(RLOG_VERBOSE,"Closing idle client");
            freeClient(c);
        }
    }
}
void freeClient(websocketClient *c) {
    listNode *ln;

    /* Note that if the client we are freeing is blocked into a blocking
     * call, we have to set querybuf to NULL *before* to call
     * unblockClientWaitingData() to avoid processInputBuffer() will get
     * called. Also it is important to remove the file events after
     * this, because this call adds the READABLE event. */
    sdsfree(c->querybuf);
    c->querybuf = NULL;

    aeDeleteFileEvent(server.el,c->fd,AE_READABLE);
    aeDeleteFileEvent(server.el,c->fd,AE_WRITABLE);
    listRelease(c->reply);
    close(c->fd);
    /* Remove from the list of clients */
    ln = listSearchKey(server.clients,c);
    //assert(ln != NULL);
    listDelNode(server.clients,ln); 
    zfree(c);
}
websocketClient *createClient(int fd) {
    websocketClient *c = (websocketClient*)zmalloc(sizeof(websocketClient));
    c->bufpos = 0;

    anetNonBlock(NULL,fd);
    anetTcpNoDelay(NULL,fd);
    if (!c) return NULL;
    if (aeCreateFileEvent(server.el,fd,AE_READABLE,
                readQueryFromClient, c) == AE_ERR)
    {
        close(fd);
        zfree(c);
        return NULL;
    }

    c->fd = fd;
    c->querybuf = sdsempty();
    c->sentlen = 0;
    c->lastinteraction = time(NULL);
    c->reply = listCreate();
    //listSetFreeMethod(c->reply,decrRefCount);
    //listSetDupMethod(c->reply,dupClientReplyValue);
    c->stage = HandshakeStage;

    listAddNodeTail(server.clients,c);

    //initClientMultiState(c);
    return c;
}
void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask) {
    websocketClient *c = (websocketClient*) privdata;
    char buf[WEBSOCKET_IOBUF_LEN];
    int nread;
    WEBSOCKET_NOTUSED(el);
    WEBSOCKET_NOTUSED(mask);

    nread = read(fd, buf, WEBSOCKET_IOBUF_LEN);
    if (nread == -1) {
        if (errno == EAGAIN) {
            nread = 0;
        } else {
            Log(RLOG_VERBOSE, "Reading from client: %s",strerror(errno));
            freeClient(c);
            return;
        }
    } else if (nread == 0) {
        Log(RLOG_VERBOSE, "Client closed connection");
        freeClient(c);
        return;
    }
    if (nread) {
        c->querybuf = sdscatlen(c->querybuf,buf,nread);
        c->lastinteraction = time(NULL);
    } else {
        return;
    }
    processInputBuffer(c);
}
void processInputBuffer(websocketClient *c) {
    /* Keep processing while there is something in the input buffer */
    while(sdslen(c->querybuf)) { //LT mode Keep process buffer data until sucess
        if (c->stage == HandshakeStage)  { //handshake
            if (processHandShake(c) != WEBSOCKET_OK) break;
        } else  { //data frame
            if (processDataFrame(c) != WEBSOCKET_OK) break;
        }

        if (processCommand(c) == WEBSOCKET_OK)
            resetClient(c);
    }
}
void resetClient(websocketClient *c) {
}
int processHandShake(websocketClient *c) {

    if(parseWebSocketHead(c->querybuf,&c->handshake_frame)!=WEBSOCKET_OK)
        return WEBSOCKET_ERR;
    return WEBSOCKET_OK;
}

int parseWebSocketHead(sds querybuf,handshake_frame_t * handshake_frame)
{
    int i=0;
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

        querybuf = sdsrange(querybuf,querylen+2,-1);
        if(argc==0) break;


        if(i==0) //first line 
        {
            if(argc==3)
            {
                handshake_frame->Method=argv[0];
                handshake_frame->Uri=argv[1];
                handshake_frame->Version=argv[2];

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
                        handshake_frame->Upgrade=argv[1];
                        break;
                    case 'c':
                    case 'C':
                        handshake_frame->Connection=argv[1];
                        break;
                    case 'h':
                    case 'H':
                        handshake_frame->Host=argv[1];
                        break;
                    case 's':
                    case 'S':
                        if(!strcasecmp(argv[0],WEBSOCKET_SEC_WEBSOCKET_VERSION)){
                            handshake_frame->Sec_WebSocket_Version=argv[1];
                        }
                        else 
                            if(!strcasecmp(argv[0],WEBSOCKET_SEC_WEBSOCKET_KEY)){
                                handshake_frame->Sec_WebSocket_Key=argv[1];
                            }else 
                                if(!strcasecmp(argv[0],WEBSOCKET_SEC_WEBSOCKET_ORIGIN)){
                                    handshake_frame->Sec_WebSocket_Origin=argv[1];
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




    Log(RLOG_VERBOSE,"Method:%s\r\nUri:%s\r\nVersion:%s\r\nConnection:%s\r\nSec_key:%s\r\nSec_vesion:%s\r\nSec_origin:%s\r\n",
            handshake_frame->Method,handshake_frame->Uri,handshake_frame->Version,handshake_frame->Connection,
            handshake_frame->Sec_WebSocket_Key,handshake_frame->Sec_WebSocket_Version,handshake_frame->Sec_WebSocket_Origin);

    return WEBSOCKET_OK;
}
int parseWebSocketDataFrame(sds querybuf,websocket_frame_t * frame)
{

    int datalen,i;
    unsigned char *buf=querybuf;
    frame->fin  = (buf[0] >> 7) & 1;
    frame->rsv1 = (buf[0] >> 6) & 1;
    frame->rsv2 = (buf[0] >> 5) & 1;
    frame->rsv3 = (buf[0] >> 4) & 1;
    frame->opcode = buf[0] & 0xf;

    frame->mask = (buf[1] >> 7) & 1;
    frame->payload_len = buf[1] & 0x7f;

    int offset=2;
    if (frame->payload_len == 126) {
        unsigned short len;
        memcpy(&len, buf+2, 2);
        offset+=2;
        frame->payload_len = ntohs(len);
        if (frame->mask) {
            offset+=4;
            memcpy(&frame->mask_key,buf+4,4);  
        }


    } else if (frame->payload_len == 127) {
        unsigned short len;
        memcpy(&len, buf+2, 8);
        offset+=8;
        frame->payload_len = websocket_ntohll(len);

        if (frame->mask) {
            offset+=4;
            memcpy(&frame->mask_key,buf+10,4);  
        }


    }

    if((datalen=(sdslen(querybuf)-offset))!=frame->payload_len)
    {
        Log(RLOG_VERBOSE,"desc=recv_error packet_len=%d recv_data_len=%d",frame->payload_len,datalen);
        return WEBSOCKET_ERR;
    }
    if (frame->payload_len > 0) {
        /*if (ngx_http_push_stream_recv(c, rev, &err, aux->data, (ssize_t) frame.payload_len) == NGX_ERROR) {
          goto closed;
          }*/
        //copy data to payload len

        if ((frame->opcode == WEBSOCKET_TEXT_OPCODE)) {
            frame->payload = sdsdup(buf+offset);
            if (frame->mask) {
                for (i = 0; i < frame->payload_len; i++) {
                    frame->payload[i] = frame->payload[i] ^ frame->mask_key[i % 4];
                }
            }

        }
    }

    if (frame->opcode == WEBSOCKET_CLOSE_OPCODE) {

    }

    return WEBSOCKET_OK;
}
int processDataFrame(websocketClient *c) {

    return WEBSOCKET_OK;
}
int processCommand(websocketClient *c) {
    return WEBSOCKET_OK;
}
