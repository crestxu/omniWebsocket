#include"websocket.h"

struct websocketServer server; /* server global state */


static void resetDataFrame(websocket_frame_t *dataframe)
{

    if(dataframe!=NULL)
    {
        if(dataframe->payload!=NULL)
        {
            sdsfree(dataframe->payload);
            //dataframe->payload=NULL;
        }
        //memcpy(dataframe,0,sizeof(dataframe));

    }
}
static void resetHandShakeFrame(handshake_frame_t *handframe)
{
    if(handframe!=NULL)
    {
        if(handframe->Connection!=NULL)
        {
            sdsfree(handframe->Connection);
            handframe->Connection=NULL;
        }
        if(handframe->Host!=NULL)
        {
            sdsfree(handframe->Method);
            handframe->Method=NULL;
        }
        if(handframe->Sec_WebSocket_Key!=NULL)
        {
            sdsfree(handframe->Sec_WebSocket_Key);
            handframe->Sec_WebSocket_Key=NULL;
        }
        if(handframe->Sec_WebSocket_Origin!=NULL)
        {
            sdsfree(handframe->Sec_WebSocket_Origin);
            handframe->Sec_WebSocket_Origin=NULL;
        }
        if(handframe->Sec_WebSocket_Version!=NULL)
        {
            sdsfree(handframe->Sec_WebSocket_Version);
            handframe->Sec_WebSocket_Version=NULL;
        }
        if(handframe->Upgrade!=NULL)
        {
            sdsfree(handframe->Upgrade);
            handframe->Upgrade=NULL;
        }
        if(handframe->Uri!=NULL)
        {
            sdsfree(handframe->Uri);
            handframe->Uri=NULL;
        }
        if(handframe->Version!=NULL)
        {
            sdsfree(handframe->Version);
            handframe->Version=NULL;
        }

    }



}

static void acceptCommonHandler(int fd) {
    websocketClient *c;
    if ((c = createClient(fd)) == NULL) {
        Log(RLOG_WARNING,"Error allocating resoures for the client");
        close(fd); /* May be already closed, just ingore errors */
        return;
    }
    /* If maxclient directive is set and this is one client more... close the
     * connection. Note that we create the client instead to check before
     * for this condition, since now the socket is already set in nonblocking
     * mode and we can send an error for free using the Kernel I/O */
    if (server.maxclients && listLength(server.clients) > server.maxclients) {
        char *err = "-ERR max number of clients reached\r\n";

        /* That's a best effort error message, don't check write errors */
        if (write(c->fd,err,strlen(err)) == -1) {
            /* Nothing to do, Just to avoid the warning... */
        }
        freeClient(c);
        return;
    }
    server.stat_numconnections++;
}
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
    acceptCommonHandler(cfd);
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
    aeCreateTimeEvent(server.el, 1, serverCron, NULL, NULL);
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
		sendServerPingMsg();
        Log(RLOG_VERBOSE,"%d clients connected",
                listLength(server.clients));
    }

    /* Close connections of timedout clients */
    if ((server.maxidletime && !(loops % 100)))
        closeTimedoutClients();

    server.cronloops++;
    return 100;
}

void sendServerPingMsg(void){
	websocketClient *c;
    listNode *ln;
    time_t now = time(NULL);
    listIter li;

    listRewind(server.clients,&li);
    while ((ln = listNext(&li)) != NULL) {
        c = listNodeValue(ln);
		addReplySds(c,formatted_websocket_ping());
        
    }
	
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
	resetHandShakeFrame(&c->handshake_frame);

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
//    Log(RLOG_VERBOSE,"rev:%s",c->querybuf);
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
	 resetHandShakeFrame(&c->handshake_frame);
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


        zfree(argv);
    }




//    Log(RLOG_VERBOSE,"Method:%s\r\nUri:%s\r\nVersion:%s\r\nConnection:%s\r\nSec_key:%s\r\nSec_vesion:%s\r\nSec_origin:%s\r\n",
  //          handshake_frame->Method,handshake_frame->Uri,handshake_frame->Version,handshake_frame->Connection,
    //        handshake_frame->Sec_WebSocket_Key,handshake_frame->Sec_WebSocket_Version,handshake_frame->Sec_WebSocket_Origin);

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

    } else if (frame->payload_len == 127) {
        unsigned short len;
        memcpy(&len, buf+2, 8);
        offset+=8;
        frame->payload_len = websocket_ntohll(len);

    }

    if(frame->mask_key)
    {
        memcpy(frame->mask_key,buf+offset,4);
        offset+=4;
    }
    if((datalen=(sdslen(querybuf)-offset))!=frame->payload_len)
    {
        Log(RLOG_VERBOSE,"desc=recv_error packet_len=%d recv_data_len=%d",frame->payload_len,datalen);
//        return WEBSOCKET_ERR;
    }
    Log(RLOG_DEBUG,"desc=dataframe len=%d",frame->payload_len);
    if (frame->payload_len > 0) {
                //copy data to payload len

        if ((frame->opcode == WEBSOCKET_TEXT_OPCODE)) {
            frame->payload =sdsempty(); // = sdsdup(sdsrange(buf,offset,frame->payload_len));
            sdscatlen(frame->payload,buf+offset,frame->payload_len);
            if (frame->mask) {
                for (i = 0; i < frame->payload_len; i++) {
                    frame->payload[i] = frame->payload[i] ^ frame->mask_key[i % 4];
                }
            }

        }
        Log(RLOG_VERBOSE,"desc=data_frame value=%s",frame->payload);
    }

    querybuf=sdsrange(querybuf,offset+frame->payload_len,-1);

    Log(RLOG_DEBUG,"desc=after_process_dataframe querybuf_len=%d",sdslen(querybuf));
    if (frame->opcode == WEBSOCKET_CLOSE_OPCODE) {

    }
	if (frame->opcode == WEBSOCKET_PONG_OPCODE) {
		Log(RLOG_DEBUG,"desc=recv_pong");

    }

    return WEBSOCKET_OK;
}
int processDataFrame(websocketClient *c) {

//    resetDataFrame(&c->data_frame);
    if(parseWebSocketDataFrame(c->querybuf,&c->data_frame)!=WEBSOCKET_OK)
        return WEBSOCKET_ERR;

    return WEBSOCKET_OK;
}
int _installWriteEvent(websocketClient *c) {

    if (c->fd <= 0) return WEBSOCKET_ERR;
    if (c->bufpos == 0 && listLength(c->reply) == 0 &&aeCreateFileEvent(server.el, c->fd, AE_WRITABLE,
        sendReplyToClient, c) == AE_ERR) return WEBSOCKET_ERR;
    return WEBSOCKET_OK;
}
int _addReplyToBuffer(websocketClient *c, char *s, size_t len) {
    size_t available = sizeof(c->buf)-c->bufpos;

    /* If there already are entries in the reply list, we cannot
     * add anything more to the static buffer. */
    if (listLength(c->reply) > 0) return WEBSOCKET_ERR;

    /* Check that the buffer has enough space available for this string. */
    if (len > available) return WEBSOCKET_ERR;

    memcpy(c->buf+c->bufpos,s,len);
    c->bufpos+=len;
    return WEBSOCKET_OK;
}
void _addReplySdsToList(websocketClient *c, sds s) {
    sds *tail;
    if (listLength(c->reply) == 0) {
        listAddNodeTail(c->reply,s);
    } else {
        tail = listNodeValue(listLast(c->reply));

        /* Append to this object when possible. */
/*        if (tail != NULL &&
            sdslen(tail)+sdslen(s) <= WEBSOCKET_REPLY_CHUNK_BYTES)
        {
            tail = dupLastObjectIfNeeded(c->reply);
            tail = sdscatlen(tail,s,sdslen(s));
            sdsfree(s);
        } else {*/
            listAddNodeTail(c->reply,s);
       // }
    }
}
void addReplySds(websocketClient *c, sds s) {
    if (_installWriteEvent(c) != WEBSOCKET_OK) {
        /* The caller expects the sds to be free'd. */
        sdsfree(s);
        return;
    }
    if (_addReplyToBuffer(c,s,sdslen(s)) == WEBSOCKET_OK) {
        sdsfree(s);
    } else {
        /* This method free's the sds when it is no longer needed. */
        _addReplySdsToList(c,s);
    }
}
void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask) {
    websocketClient *c = privdata;
    int nwritten = 0, totwritten = 0, objlen;
    sds o;
    WEBSOCKET_NOTUSED(el);
    WEBSOCKET_NOTUSED(mask);

    while(c->bufpos > 0 || listLength(c->reply)) {
        if (c->bufpos > 0) {
     
            nwritten = write(fd,c->buf+c->sentlen,c->bufpos-c->sentlen);
            if (nwritten <= 0) break;
            c->sentlen += nwritten;
            totwritten += nwritten;

            /* If the buffer was sent, set bufpos to zero to continue with
             * the remainder of the reply. */
            if (c->sentlen == c->bufpos) {
                c->bufpos = 0;
                c->sentlen = 0;
            }
        } else {
            o = listNodeValue(listFirst(c->reply));
            objlen = sdslen(o);

            if (objlen == 0) {
                listDelNode(c->reply,listFirst(c->reply));
                continue;
            }

            nwritten = write(fd, ((char*)o)+c->sentlen,objlen-c->sentlen);
            if (nwritten <= 0) break;
            c->sentlen += nwritten;
            totwritten += nwritten;

            /* If we fully sent the object on head go to the next one */
            if (c->sentlen == objlen) {
                listDelNode(c->reply,listFirst(c->reply));
                c->sentlen = 0;
            }
        }
        /* Note that we avoid to send more thank REDIS_MAX_WRITE_PER_EVENT
         * bytes, in a single threaded server it's a good idea to serve
         * other clients as well, even if a very large request comes from
         * super fast link that is always able to accept data (in real world
         * scenario think about 'KEYS *' against the loopback interfae) */
        //if (totwritten > REDIS_MAX_WRITE_PER_EVENT) break;
    }
    if (nwritten == -1) {
        if (errno == EAGAIN) {
            nwritten = 0;
        } else {
                Log(RLOG_VERBOSE,
                "Error writing to client: %s", strerror(errno));
            freeClient(c);
            return;
        }
    }
    if (totwritten > 0) c->lastinteraction = time(NULL);
    if (listLength(c->reply) == 0) {
        c->sentlen = 0;
        aeDeleteFileEvent(server.el,c->fd,AE_WRITABLE);
    }
}




static sds formatted_websocket_ping()
{
    
    sds frame=sdsempty();

    long len=sdslen(str);
    if (frame != NULL) {
        frame = sdscatlen(frame, (void*)&WEBSOCKET_PING_LAST_FRAME_BYTE, sizeof(WEBSOCKET_PING_LAST_FRAME_BYTE));
    }
    return frame;
}



static sds formatted_websocket_frame(sds str)
{
    
    sds frame=sdsempty();

    long len=sdslen(str);
    if (frame != NULL) {
        frame = sdscatlen(frame, (void*)&WEBSOCKET_TEXT_LAST_FRAME_BYTE, sizeof(WEBSOCKET_TEXT_LAST_FRAME_BYTE));

        if (len <= 125) {
            frame = sdscatlen(frame, &len, 1);
        } else if (len < (1 << 16)) {
            frame = sdscatlen(frame, (void*)&WEBSOCKET_PAYLOAD_LEN_16_BYTE, sizeof(WEBSOCKET_PAYLOAD_LEN_16_BYTE));
            uint16_t len_net = htons(len);
            frame = sdscatlen(frame, &len_net, 2);
        } else {
            frame = sdscatlen(frame, (void*)&WEBSOCKET_PAYLOAD_LEN_64_BYTE, sizeof(WEBSOCKET_PAYLOAD_LEN_64_BYTE));
            uint64_t len_net = websocket_ntohll(len);
            frame = sdscatlen(frame, &len_net, 8);
        }
        frame = sdscatlen(frame, str, sdslen(str));
    }
    return frame;
}

int generateAcceptKey(sds webKey,char *key,int len)
{
    //char buff[1024]={0};
    unsigned char shastr[20]={0};

    SHA1_CTX context;
    char *magicstr="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    SHA1Init(&context);
    SHA1Update(&context,webKey,sdslen(webKey));
    SHA1Update(&context,magicstr,strlen(magicstr));
    SHA1Final(shastr,&context);
    base64_encode(shastr,20,key,len);
    Log(RLOG_VERBOSE,"dec=generate_accept_key value=%s",key);


    return WEBSOCKET_OK;
}
void sendMsg(list *monitors, sds msg) {
    listNode *ln;
    listIter li;
    listRewind(monitors,&li);
    while((ln = listNext(&li))) {
        websocketClient *monitor = ln->value;
        sds tmpmsg=sdsdup(msg);
        addReplySds(monitor,tmpmsg);
    }
}

int processCommand(websocketClient *c) {
    if(c->stage==HandshakeStage){ //do hand shake

        char buff[1024]={0};
        char *res="HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-WebSocket-Origin: null\r\nSec-WebSocket-Location: ws://example.com\r\n\r\n";
        
        char key[128]={0};
        generateAcceptKey(c->handshake_frame.Sec_WebSocket_Key,key,128);
        snprintf(buff,1024,res,key);
        sds m=sdsnew(buff);
        addReplySds(c,m);
        c->stage=ConnectedStage;
    }
    else{  //process data
        if(c->data_frame.payload!=NULL)
        {
            sds mm2=sdsdup(c->data_frame.payload);
            sds mm=formatted_websocket_frame(mm2);
            //addReplySds(c,mm);
            sendMsg(server.clients,mm);
            sdsfree(mm);
            sdsfree(mm2);
            resetDataFrame(&c->data_frame);
        }
    }

    Log(RLOG_DEBUG,"desc=querylen len=%d",sdslen(c->querybuf));
    return WEBSOCKET_OK;
}
