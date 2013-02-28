#include <stdio.h>
#include <stdarg.h>
#include "common.h"

static inline int isLogEnabledFor(int);
static int kCurrentLogLevel = LOG_INFO;

void slog(int pLevel, char *pFormat, ...)
{
  #ifdef SHAIRPORT_LOG
  if(isLogEnabledFor(pLevel))
  {
    va_list argp;
    va_start(argp, pFormat);
    vprintf(pFormat, argp);
    va_end(argp);
  }
  #endif
}


static inline int isLogEnabledFor(int pLevel)
{
  if(pLevel <= kCurrentLogLevel)
  {
    return TRUE;
  }
  return FALSE;
}


