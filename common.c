#include <stdio.h>
#include <stdarg.h>
#include "common.h"

static inline int isLogEnabledFor(int);
static int g_current_log_level = LOG_INFO;

void slog(int pLevel, char *pFormat, ...)
{
  #ifdef SHAIRPORT_LOG
  char logMsg[132];
  if(isLogEnabledFor(pLevel))
  {
    va_list argp;
    va_start(argp, pFormat);
    vsnprintf(logMsg, 132, pFormat,argp);
    fprintf(stderr,logMsg);
//    vprintf(pFormat, argp);
    va_end(argp);
  }
  #endif
}


static inline int isLogEnabledFor(int pLevel)
{
  if(pLevel <= g_current_log_level)
  {
    return TRUE;
  }
  return FALSE;
}

void set_log_level(int lvl)
{
  g_current_log_level=lvl;
}

int get_log_level()
{
  return g_current_log_level;
}
