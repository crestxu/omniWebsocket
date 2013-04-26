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
#include"sha1.h"
#include"base64.h"
#include <sys/uio.h>
#include <errno.h>

#define WEBSOCKET_OK 0
#define WEBSOCKET_ERR 1

#define WEBSOCKET_REPLY_CHUNK_BYTES (5*1500) /* 5 TCP packets with default MTU */
#define WEBSOCKET_NOTUSED(V) ((void) V)
#define WEBSOCKET_IOBUF_LEN 1024
#define WEBSOCKET_METHOD_GET "Get"
#define WEBSOCKET_METHOD_POST "Post"
#define WEBSOCKET_UPGRADE "Upgrade:"
#define WEBSOCKET_CONNECTION "Connection:"
#define WEBSOCKET_HOST "Host:"
#define WEBSOCKET_SEC_WEBSOCKET_VERSION "Sec-WebSocket-Version:"
#define WEBSOCKET_SEC_WEBSOCKET_KEY "Sec-WebSocket-Key:"
#define WEBSOCKET_SEC_WEBSOCKET_ORIGIN "Sec-WebSocket-Origin:"




#define WEBSOCKET_TEXT_OPCODE 0x1
#define WEBSOCKET_CLOSE_OPCODE 0x8
#define WEBSOCKET_PING_OPCODE 0x9
#define WEBSOCKET_PONG_OPCODE 0xA
#define WEBSOCKET_LAST_FRAME   0x8

typedef unsigned char u_char;
typedef unsigned short uint16_t;

static const u_char WEBSOCKET_TEXT_LAST_FRAME_BYTE    =  WEBSOCKET_TEXT_OPCODE  | (WEBSOCKET_LAST_FRAME << 4);
static const u_char WEBSOCKET_CLOSE_LAST_FRAME_BYTE[] = {WEBSOCKET_CLOSE_OPCODE | (WEBSOCKET_LAST_FRAME << 4), 0x00};
static const u_char WEBSOCKET_PING_LAST_FRAME_BYTE[]  = {WEBSOCKET_PING_OPCODE  | (WEBSOCKET_LAST_FRAME << 4), 0x00};
static const u_char WEBSOCKET_PAYLOAD_LEN_16_BYTE   = 126;
static const u_char WEBSOCKET_PAYLOAD_LEN_64_BYTE   = 127;

#define WEBSOCKET_MAX_HEAD_LEVEL 10 //define the max head line of websocket

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
    sds payload;
} websocket_frame_t;

typedef struct {
    sds Method;
    sds Uri;
    sds Version;
    sds Upgrade;
    sds Connection;
    sds Sec_WebSocket_Version;
    sds Host;
    sds Sec_WebSocket_Key;
    sds Sec_WebSocket_Origin;
}handshake_frame_t;

typedef struct websocketClient {
   enum
    {
        HandshakeStage = 0,
        LoginStage,
        ConnectedStage
    }; 
    int fd;
    int flags;
    int stage; //web socket stage
    sds querybuf;
    list *reply;
    int sentlen;
    websocket_frame_t data_frame;
    handshake_frame_t handshake_frame;
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
void processInputBuffer(websocketClient *c);
int processDataFrame(websocketClient *c);
int processHandShake(websocketClient *c);
void resetClient(websocketClient *c); 
int processCommand(websocketClient *c);
int parseWebSocketHead(sds querybuf,handshake_frame_t * handshake_frame);
int parseWebSocketDataFrame(sds querybuf,websocket_frame_t * data_frame);
void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask);
#endif
