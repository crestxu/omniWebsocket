#ifndef UTILS_H
#define UTILS_H
#include<stdio.h>
#define RLOG_RAW (1<<10)
#define RLOG_DEBUG 0
#define RLOG_VERBOSE 1
#define RLOG_NOTICE 2
#define RLOG_WARNING 3
#define MAX_LOGMSG_LEN    1024 /* Default maximum length of syslog messages */
void LogRaw(int level, const char *msg);

void Log(int level, const char *fmt, ...);

void LogFromHandler(int level, const char *msg);

void oom(const char *msg);

int ll2string(char *s, size_t len, long long value) ;
long long ustime(void);

long long mstime(void);
#endif
