#ifndef _COMMON_H
#define _COMMON_H

#include <syslog.h>
#define die(x) { \
    syslog(LOG_ERR, "FATAL: %s\n", x); \
    exit(1); \
}

/*
void set_log_level(int level);
void slog(int pLevel, char *pFormat, ...);
int get_log_level();
*/


#define SHAIRPORT_LOG 1

#define RSA_LOG_LEVEL LOG_DEBUG
#define SOCKET_LOG_LEVEL LOG_DEBUG
#define HEADER_LOG_LEVEL LOG_DEBUG
#define AVAHI_LOG_LEVEL LOG_DEBUG
#define TRUE (-1)
#define FALSE (0)

#endif
