#ifndef _COMMON_H
#define _COMMON_H

#include <syslog.h>
#define die(x) { \
    fprintf(stderr, "FATAL: %s\n", x); \
    exit(1); \
}

void set_log_level(int level);
void slog(int pLevel, char *pFormat, ...);
int get_log_level();


#define SHAIRPORT_LOG 1
#define SLOG_INFO     1
#define SLOG_DEBUG    5
#define SLOG_NORMAL   1
#define SLOG_DEBUG_V  6
#define SLOG_DEBUG_VV 7


#define RSA_LOG_LEVEL SLOG_DEBUG_VV
#define SOCKET_LOG_LEVEL SLOG_DEBUG_VV
#define HEADER_LOG_LEVEL SLOG_DEBUG
#define AVAHI_LOG_LEVEL SLOG_DEBUG
#define TRUE (-1)
#define FALSE (0)

#endif
