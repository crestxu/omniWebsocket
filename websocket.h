/**
 *       @file  websocket.h
 *      @brief  <CURSOR>
 *
 * Detailed description starts here.
 *
 *     @author  xujiajie (), xujiajie@baidu.com
 *
 *   @internal
 *     Created  04/15/2013
 *    Compiler  gcc/g++
 *     Company  baidu.com inc
 *   Copyright  Copyright (c) 2013, xujiajie
 * =====================================================================================
 */

#ifndef WEBSOCKET_H
#define WEBSOCKET_H
#include<stdio.h>
#include <signal.h>
#include<unistd.h>
#include<stdlib.h>
#include"ae.h"
#include<string.h>
#include"anet.h"
#include"sds.h"
#include"adlist.h"
#include"logs.h"
#include <sys/uio.h>
#include <errno.h>
#define WEBSOCKET_REPLY_CHUNK_BYTES (5*1500) /* 5 TCP packets with default MTU */
#define WEBSOCKET_NOTUSED(V) ((void) V)
#define WEBSOCKET_IOBUF_LEN 1024
#define WEBSOCKET_METHOD_GET "Get"
#define WEBSOCKET_METHOD_POST "Post"
#define WEBSOCKET_UPGRADE "Upgrade:"
#define WEBSOCKET_CONNECTION "Connection:"
#define WEBSOCKET_HOST "Host:"
#define WEBSOCKET_SEC_WEBSOCKET_VERSION "Sec_WebSocket_Version:"
#define WEBSOCKET_SEC_WEBSOCKET_KEY "Sec_WebSocket_Key:"
#define WEBSOCKET_SEC_WEBSOCKET_ORIGIN "Sec_WebSocket_Origin:"

extern struct websocketServer server; /* server global state */

typedef struct {
    unsigned char fin:1;
    unsigned char rsv1:1;
    unsigned char rsv2:1;
    unsigned char rsv3:1;
    unsigned char opcode:4;
    unsigned char mask:1;
    unsigned char mask_key[4];
    unsigned long payload_len;
    unsigned char *payload;
} websocket_frame_t;

typedef struct {
    unsigned char * Uri;
    unsigned char * Upgrade;
    unsigned char * Connection;
    unsigned char * Sec_WebSocket_Version;
    unsigned char * Host;
    unsigned char * Sec_WebSocket_Key;
    unsigned char * Sec_WebSocket_Origin;
}handshake_frame_t;

typedef struct websocketClient {
   enum
    {
        HandshakeStage = 0,
        LoginStage,
        ConnectedStage
    }; 
    int fd;
    int stage; //web socket stage
    sds querybuf;
    list *reply;
    int sentlen;
    websocket_frame_t frame;
    time_t lastinteraction; /* time of the last interaction, used for timeout */
    /* Response buffer */
    int bufpos;
    char buf[WEBSOCKET_REPLY_CHUNK_BYTES];
} websocketClient;


/* Global server state structure */
struct websocketServer {
    pthread_t mainthread;
    int port;
    char *bindaddr;
    int ipfd;
    list *clients;
    time_t loading_start_time;
    char neterr[ANET_ERR_LEN];
    aeEventLoop *el;
    int cronloops;              /* number of times the cron function run */
    time_t stat_starttime;          /* server start time */
    long long stat_numconnections;  /* number of connections received */
    /* Configuration */
    int verbosity;
    int maxidletime;
    int daemonize;
    char *logfile;
    int syslog_enabled;
    char *syslog_ident;
    int syslog_facility;
    /* Limits */
    unsigned int maxclients;
    time_t unixtime;    /* Unix time sampled every second. */
};
int initServer();
int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData);
void closeTimedoutClients(void);
void freeClient(websocketClient *c);
websocketClient *createClient(int fd);
void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask);

#endif
