#ifndef _COMMON_H
#define _COMMON_H

#define die(x) { \
    fprintf(stderr, "FATAL: %s\n", x); \
    exit(1); \
}

void slog(int pLevel, char *pFormat, ...);


#define SHAIRPORT_LOG 1
#define LOG_INFO     1
#define LOG_DEBUG    5
#define LOG_DEBUG_V  6
#define LOG_DEBUG_VV 7

#define RSA_LOG_LEVEL LOG_DEBUG_VV
#define SOCKET_LOG_LEVEL LOG_DEBUG_VV
#define HEADER_LOG_LEVEL LOG_DEBUG
#define AVAHI_LOG_LEVEL LOG_DEBUG
#define TRUE (-1)
#define FALSE (0)

#endif
