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
    while(sdslen(c->querybuf)) {
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
    char *newline = strstr(c->querybuf,"\r\n");
    int argc, j;
    sds *argv;
    size_t querylen;

    /* Nothing to do without a \r\n */
    if (newline == NULL)
        return WEBSOCKET_ERR;

    /* Split the input buffer up to the \r\n */
    querylen = newline-(c->querybuf);
    argv = sdssplitlen(c->querybuf,querylen," ",1,&argc);

    /* Leave data after the first line of the query in the buffer */
    c->querybuf = sdsrange(c->querybuf,querylen+2,-1);

    /* Setup argv array on client structure */
  //  if (c->argv) zfree(c->argv);
   // c->argv = zmalloc(sizeof(robj*)*argc);

    /* Create redis objects for all arguments. */
   /* for (c->argc = 0, j = 0; j < argc; j++) {
        if (sdslen(argv[j])) {
            c->argv[c->argc] = createObject(REDIS_STRING,argv[j]);
            c->argc++;
        } else {
            sdsfree(argv[j]);
        }
    }*/
    zfree(argv);
    return WEBSOCKET_OK;
}

int processDataFrame(websocketClient *c) {

    return WEBSOCKET_OK;
}
int processCommand(websocketClient *c) {
    return WEBSOCKET_OK;
}
