/*
 * ShairPort Initializer - Network Initializer/Mgr for Hairtunes RAOP
 * Copyright (c) M. Andrew Webster 2011
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include "socketlib.h"
#include "shairport.h"
#include "hairtunes.h"
#include "common.h"
#include "vol.h"
#include "audio.h"

// TEMP

int bufferStartFill = -1;

#ifdef _WIN32
#define DEVNULL "nul"
#else
#define DEVNULL "/dev/null"
#endif

static void handleClient(int pSock, char *pPassword, char *pHWADDR);
static void writeDataToClient(int pSock, struct shairbuffer *pResponse);
static int readDataFromClient(int pSock, struct shairbuffer *pClientBuffer);

static int parseMessage(struct connection *pConn,unsigned char *pIpBin, unsigned int pIpBinLen, char *pHWADDR);
static void propogateCSeq(struct connection *pConn);

static void cleanupBuffers(struct connection *pConnection);
static void cleanup(struct connection *pConnection);

static int startAvahi(const char *pHwAddr, const char *pServerName, int pPort);

static int getAvailChars(struct shairbuffer *pBuf);
static void addToShairBuffer(struct shairbuffer *pBuf, char *pNewBuf);
static void addNToShairBuffer(struct shairbuffer *pBuf, char *pNewBuf, int pNofNewBuf);

static char *getTrimmedMalloc(char *pChar, int pSize, int pEndStr, int pAddNL);
static char *getTrimmed(char *pChar, int pSize, int pEndStr, int pAddNL, char *pTrimDest);

static void initConnection(struct connection *pConn, struct keyring *pKeys, struct comms *pComms, int pSocket, char *pPassword);
static void closePipe(int *pPipe);
static void initBuffer(struct shairbuffer *pBuf, int pNumChars);

static void setKeys(struct keyring *pKeys, char *pIV, char* pAESKey, char *pFmtp);
static RSA *loadKey(void);

static int g_avahi_pid=-1;

static void handle_sigchld(int signo) {
    int status;
    waitpid(-1, &status, WNOHANG);
}

char tPidFile[255] = "/var/run/shairport.pid";

static void handle_sigterm(int signo) {
    if (g_avahi_pid>0)
    {
        syslog(LOG_INFO,"SIGTERM caught, killing avahi-publish-service (%d) before exit\n", g_avahi_pid);
        kill(g_avahi_pid, SIGTERM);
    }
    int pid=0;
    FILE* fd=fopen(tPidFile,"r");
    if (fd==NULL)
    {
        syslog(LOG_ERR, "PID File: %s not found\n",tPidFile);
    } else {
        fscanf(fd,"%d",&pid);
        fclose(fd);
        if (getpid()==pid){
          syslog(LOG_INFO,"Removing PID file\n");
          if (remove(tPidFile)!=0)
          {
            syslog(LOG_ERR,"Cannot remove PID file %s\n",tPidFile);
          } else {
            syslog(LOG_INFO,"PID file: %s removed.\n",tPidFile);
          }
        } else {
          syslog(LOG_ERR,"PID file mismatch, please check\n");
        }
    }
    exit(0);
}

char tAoDriver[56] = "";
char tAoDeviceName[56] = "";
char tAoDeviceId[56] = "";
/*
char tAlsaCtl[56] = "";
char tAlsaVol[56] = "";
*/


int main(int argc, char **argv)
{
  // unfortunately we can't just IGN on non-SysV systems
  openlog("SHAIRPORT", LOG_PID | LOG_PERROR, LOG_USER);

// Register SIGCHLD to avoid zombee process
  struct sigaction sa;
  sa.sa_handler = handle_sigchld;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGCHLD, &sa, NULL) < 0) {
      perror("sigaction");
      return 1;
  }

// Register SIGTERM to kill avahi process
  struct sigaction sa_term;
  sa_term.sa_handler=handle_sigterm;
  sa.sa_flags=0;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGTERM, &sa_term, NULL) < 0)
  {
    perror("sigaction on sigterm");
    return 1;
  }

  char tHWID[HWID_SIZE] = {0,51,52,53,54,55};
  char tHWID_Hex[HWID_SIZE * 2 + 1];
  memset(tHWID_Hex, 0, sizeof(tHWID_Hex));

  char tServerName[56] = "ShairPort";
  char tPassword[56] = "";

  struct addrinfo *tAddrInfo;
  int  tSimLevel = 0;
  int  tUseKnownHWID = FALSE;
  int  tDaemonize = FALSE;
  int  tPort = PORT;
  int tLogLvl = LOG_ERR;

  char *arg;
  while ( (arg = *++argv) ) {
    parse_vol_arg(arg);
    parse_audio_arg(arg);
    if(!strcmp(arg, "-a"))
    {
       strncpy(tServerName, *++argv, 55);
       argc--;
    }
    else if(!strncmp(arg, "--apname=", 9))
    {
      strncpy(tServerName, arg+9, 55);
    }
/*
    else if(!strncmp(arg, "--ao_driver=",12 ))
    {
      strncpy(tAoDriver, arg+12, 55);
    }
*/
/*
    else if(!strncmp(arg, "--alsa_pcm=",11 ))
    {
      strncpy(tAoDriver, arg+11, 55);
    }
*/
/*
    else if(!strncmp(arg, "--ao_devicename=",16 ))
    {
      strncpy(tAoDeviceName, arg+16, 55);
    }
*/
/*
    else if(!strncmp(arg, "--alsa_volume=",14 ))
    {
      strncpy(tAlsaVol, arg+14, 55);
    }
*/
/*
    else if(!strncmp(arg, "--ao_deviceid=",14 ))
    {
      strncpy(tAoDeviceId, arg+14, 55);
    }
*/
/*
    else if(!strncmp(arg, "--alsa_ctl=",11 ))
    {
      strncpy(tAlsaCtl, arg+11, 55);
    }
*/
    else if(!strncmp(arg, "--apname=", 9))
    {
      strncpy(tServerName, arg+9, 55);
    }
    else if(!strcmp(arg, "-p"))
    {
      strncpy(tPassword, *++argv, 55);
      argc--;
    }
    else if(!strncmp(arg, "--password=",11 ))
    {
      strncpy(tPassword, arg+11, 55);
    }
    else if(!strncmp(arg, "--pid_file=",11 ))
    {
      strncpy(tPidFile, arg+11, 55);
    }
    else if(!strcmp(arg, "-o"))
    {
      tPort = atoi(*++argv);
      argc--;
    }
    else if(!strncmp(arg, "--server_port=", 14))
    {
      tPort = atoi(arg+14);
    }
    else if(!strcmp(arg, "-b")) 
    {
      bufferStartFill = atoi(*++argv);
      argc--;
    }
    else if(!strncmp(arg, "--buffer=", 9))
    {
      bufferStartFill = atoi(arg + 9);
    }
    else if(!strncmp(arg, "--buffer_size=", 14))
    {
      int fsize = atoi(arg + 14);
      set_buffer_frames(fsize);
    }
    else if(!strcmp(arg, "-k"))
    {
      tUseKnownHWID = TRUE;
    }
    else if(!strcmp(arg, "-q") || !strncmp(arg, "--quiet", 7))
    {
      tLogLvl=LOG_CRIT;
    }
    else if(!strcmp(arg, "-d"))
    {
      tDaemonize = TRUE;
    }
    else if(!strcmp(arg, "-v"))
    {
      tLogLvl=LOG_WARNING;
    }
    else if(!strcmp(arg, "-v2"))
    {
      tLogLvl=LOG_INFO;
    }
    else if(!strcmp(arg, "-vv") || !strcmp(arg, "-v3"))
    {
      tLogLvl=LOG_DEBUG;
    }    
    else if(!strcmp(arg, "-h") || !strcmp(arg, "--help"))
    {
      printf("ShairPort version 0.05 C port - Airport Express emulator\n");
      printf("Usage:\nshairport [OPTION...]\n\nOptions:\n");
      printf("  -h                                    Show this help\n");
      printf("  -a, --apname=AirPort                  Sets Airport name\n");
      printf("  -p, --password=secret                 Sets Password (not working)\n");
      printf("  -o, --server_port=5002                Sets Port for Avahi/dns-sd/howl\n");
      printf("  -b, --buffer=282                      Sets Number of frames to buffer before beginning playback\n");
      printf("  --buffer_size=1024                    Sets buffer size\n");
      printf("  -d                                    Daemonize\n");
      printf("  --pid_file=/var/run/shairport.pid     Sets PID file in -d mode\n");
      print_audio_args();
      print_vol_args();
      printf("  -d                                    Daemon mode\n");
      printf("  -q, --quiet                           Supresses all output.\n");
      printf("  -v,-v2,-v3,-vv                        Various debugging levels\n\n");
      return 0;
    }    
  }

  if ( (bufferStartFill >= 0 && bufferStartFill < 30) || bufferStartFill > get_buffer_frames() ) {
     syslog(LOG_ERR, "buffer value must be > 30 and < %d\n", get_buffer_frames());
     return(-1);
  }

  setlogmask(LOG_UPTO(tLogLvl));
  if(tDaemonize)
  {
    closelog();
    openlog("SHAIRPORT", LOG_PID, LOG_DAEMON);
    setlogmask(LOG_UPTO(tLogLvl));
    int tPid = fork();
    if(tPid < 0)
    {
      exit(1); // Error on fork
    }
    else if(tPid > 0)
    {
      syslog(LOG_INFO,"Writing PID file %s\n",tPidFile);
      FILE* fd=fopen(tPidFile,"w");
      if (fd==NULL)
      {
        syslog(LOG_WARNING,"Cannot update PID file %s\n", tPidFile);
      }
      else
      {
        fprintf(fd,"%d\n",tPid);
        fclose(fd);
      }
      exit(0);
    }
    else
    {
      setsid();
      int tIdx = 0;
      for(tIdx = getdtablesize(); tIdx >= 0; --tIdx)
      {
        close(tIdx);
      }
      tIdx = open(DEVNULL, O_RDWR);
      dup(tIdx);
      dup(tIdx);
    }
  }
  srandom ( time(NULL) );
  // Copy over empty 00's
  //tPrintHWID[tIdx] = tAddr[0];

  int tIdx = 0;
  for(tIdx=0;tIdx<HWID_SIZE;tIdx++)
  {
    if(tIdx > 0)
    {
      if(!tUseKnownHWID)
      {
        int tVal = ((random() % 80) + 33);
        tHWID[tIdx] = tVal;
      }
      //tPrintHWID[tIdx] = tAddr[tIdx];
    }
    sprintf(tHWID_Hex+(tIdx*2), "%02X",tHWID[tIdx]);
  }
  //tPrintHWID[HWID_SIZE] = '\0';

  syslog(LOG_DEBUG, "LogLevel: %d\n", tLogLvl);
  syslog(LOG_INFO, "AirName: %s\n", tServerName);
  syslog(LOG_DEBUG, "HWID: %.*s\n", HWID_SIZE, tHWID+1);
  syslog(LOG_DEBUG, "HWID_Hex(%d): %s\n", (int)strlen(tHWID_Hex), tHWID_Hex);

  if(tSimLevel >= 1)
  {
    #ifdef SIM_INCL
    sim(tSimLevel, tTestValue, tHWID);
    #endif
    return(1);
  }
  else
  {
    g_avahi_pid=startAvahi(tHWID_Hex, tServerName, tPort);
    syslog(LOG_INFO, "Starting connection server: specified server port: %d\n", tPort);
    int tServerSock = setupListenServer(&tAddrInfo, tPort);
    if(tServerSock < 0)
    {
      freeaddrinfo(tAddrInfo);
      syslog(LOG_ERR, "Error setting up server socket on port %d, try specifying a different port\n", tPort);
      exit(1);
    }

    int tClientSock = 0;
    while(1)
    {
      syslog(LOG_INFO, "Waiting for clients to connect\n");
      tClientSock = acceptClient(tServerSock, tAddrInfo);
      if(tClientSock > 0)
      {
        int tPid = fork();
        if(tPid == 0)
        {
          freeaddrinfo(tAddrInfo);
          tAddrInfo = NULL;
          syslog(LOG_DEBUG, "...Accepted Client Connection..\n");
          close(tServerSock);
          handleClient(tClientSock, tPassword, tHWID);
          //close(tClientSock);
          return 0;
        }
        else
        {
          syslog(LOG_DEBUG, "Child now busy handling new client\n");
          close(tClientSock);
        }
      }
      else
      {
        // failed to init server socket....try waiting a moment...
        sleep(2);
      }
    }
  }

  syslog(LOG_DEBUG, "Finished, and waiting to clean everything up\n");
  sleep(1);
  if(tAddrInfo != NULL)
  {
    freeaddrinfo(tAddrInfo);
  }
  return 0;
}

static int findEnd(char *tReadBuf)
{
  // find \n\n, \r\n\r\n, or \r\r is found
  int tIdx = 0;
  int tLen = strlen(tReadBuf);
  for(tIdx = 0; tIdx < tLen; tIdx++)
  {
    if(tReadBuf[tIdx] == '\r')
    {
      if(tIdx + 1 < tLen)
      {
        if(tReadBuf[tIdx+1] == '\r')
        {
          return (tIdx+1);
        }
        else if(tIdx+3 < tLen)
        {
          if(tReadBuf[tIdx+1] == '\n' &&
             tReadBuf[tIdx+2] == '\r' &&
             tReadBuf[tIdx+3] == '\n')
          {
            return (tIdx+3);
          }
        }
      }
    }
    else if(tReadBuf[tIdx] == '\n')
    {
      if(tIdx + 1 < tLen && tReadBuf[tIdx+1] == '\n')
      {
        return (tIdx + 1);
      }
    }
  }
  // Found nothing
  return -1;
}

static void handleClient(int pSock, char *pPassword, char *pHWADDR)
{
  fflush(stdout);

  socklen_t len;
  struct sockaddr_storage addr;
  #ifdef AF_INET6
  unsigned char ipbin[INET6_ADDRSTRLEN];
  #else
  unsigned char ipbin[INET_ADDRSTRLEN];
  #endif
  unsigned int ipbinlen;
  int port;
  char ipstr[64];

  len = sizeof addr;
  getsockname(pSock, (struct sockaddr*)&addr, &len);

  // deal with both IPv4 and IPv6:
  if (addr.ss_family == AF_INET) {
      syslog(LOG_DEBUG, "Constructing ipv4 address\n");
      struct sockaddr_in *s = (struct sockaddr_in *)&addr;
      port = ntohs(s->sin_port);
      inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
      memcpy(ipbin, &s->sin_addr, 4);
      ipbinlen = 4;
  } else { // AF_INET6
      syslog(LOG_DEBUG, "Constructing ipv6 address\n");
      struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
      port = ntohs(s->sin6_port);
      inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);

      union {
          struct sockaddr_in6 s;
          unsigned char bin[sizeof(struct sockaddr_in6)];
      } addr;
      memcpy(&addr.s, &s->sin6_addr, sizeof(struct sockaddr_in6));

      if(memcmp(&addr.bin[0], "\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\xff\xff", 12) == 0)
      {
        // its ipv4...
        memcpy(ipbin, &addr.bin[12], 4);
        ipbinlen = 4;
      }
      else
      {
        memcpy(ipbin, &s->sin6_addr, 16);
        ipbinlen = 16;
      }
  }

  syslog(LOG_DEBUG, "Peer IP address: %s\n", ipstr);
  syslog(LOG_DEBUG, "Peer port      : %d\n", port);

  int tMoreDataNeeded = 1;
  struct keyring     tKeys;
  struct comms       tComms;
  struct connection  tConn;
  initConnection(&tConn, &tKeys, &tComms, pSock, pPassword);

  while(1)
  {
    tMoreDataNeeded = 1;

    initBuffer(&tConn.recv, 80); // Just a random, small size to seed the buffer with.
    initBuffer(&tConn.resp, 80);
    
    int tError = FALSE;
    while(1 == tMoreDataNeeded)
    {
      tError = readDataFromClient(pSock, &(tConn.recv));
      if(!tError && strlen(tConn.recv.data) > 0)
      {
        syslog(LOG_DEBUG, "Finished Reading some data from client\n");
        // parse client request
        tMoreDataNeeded = parseMessage(&tConn, ipbin, ipbinlen, pHWADDR);
        if(1 == tMoreDataNeeded)
        {
          syslog(LOG_DEBUG, "\n\nNeed to read more data\n");
        }
        else if(-1 == tMoreDataNeeded) // Forked process down below ended.
        {
          syslog(LOG_DEBUG, "Forked Process ended...cleaning up\n");
          cleanup(&tConn);
          // pSock was already closed
          return;
        }
        // if more data needed,
      }
      else
      {
        syslog(LOG_DEBUG, "Error reading from socket, closing client\n");
        // Error reading data....quit.
        cleanup(&tConn);
        return;
      }
    }
    syslog(LOG_DEBUG, "Writing: %d chars to socket\n", tConn.resp.current);
    //tConn->resp.data[tConn->resp.current-1] = '\0';
    writeDataToClient(pSock, &(tConn.resp));
   // Finished reading one message...
    cleanupBuffers(&tConn);
  }
  cleanup(&tConn);
  fflush(stdout);
}

static void writeDataToClient(int pSock, struct shairbuffer *pResponse)
{
  syslog(LOG_DEBUG, "\n----Beg Send Response Header----\n%.*s\n", pResponse->current, pResponse->data);
  send(pSock, pResponse->data, pResponse->current,0);
  syslog(LOG_DEBUG, "----Send Response Header----\n");
}

static int readDataFromClient(int pSock, struct shairbuffer *pClientBuffer)
{
  char tReadBuf[MAX_SIZE];
  tReadBuf[0] = '\0';

  int tRetval = 1;
  int tEnd = -1;
  while(tRetval > 0 && tEnd < 0)
  {
     // Read from socket until \n\n, \r\n\r\n, or \r\r is found
      syslog(LOG_DEBUG, "Waiting To Read...\n");
      fflush(stdout);
      tRetval = read(pSock, tReadBuf, MAX_SIZE);
      // if new buffer contains the end of request string, only copy partial buffer?
      tEnd = findEnd(tReadBuf);
      if(tEnd >= 0)
      {
        if(pClientBuffer->marker == 0)
        {
          pClientBuffer->marker = tEnd+1; // Marks start of content
        }
        syslog(SOCKET_LOG_LEVEL, "Found end of http request at: %d\n", tEnd);
        fflush(stdout);        
      }
      else
      {
        tEnd = MAX_SIZE;
        syslog(SOCKET_LOG_LEVEL, "Read %d of data so far\n%s\n", tRetval, tReadBuf);
        fflush(stdout);
      }
      if(tRetval > 0)
      {
        // Copy read data into tReceive;
        syslog(SOCKET_LOG_LEVEL, "Read %d data, using %d of it\n", tRetval, tEnd);
        addNToShairBuffer(pClientBuffer, tReadBuf, tRetval);
        syslog(LOG_DEBUG, "Finished copying data\n");
      }
      else
      {
        syslog(LOG_DEBUG, "Error reading data from socket, got: %d bytes", tRetval);
        return tRetval;
      }
  }
  if(tEnd + 1 != tRetval)
  {
    syslog(SOCKET_LOG_LEVEL, "Read more data after end of http request. %d instead of %d\n", tRetval, tEnd+1);
  }
  syslog(SOCKET_LOG_LEVEL, "Finished Reading Data:\n%s\nEndOfData\n", pClientBuffer->data);
  fflush(stdout);
  return 0;
}

static char *getFromBuffer(char *pBufferPtr, const char *pField, int pLenAfterField, int *pReturnSize, char *pDelims)
{
  syslog(LOG_DEBUG, "GettingFromBuffer: %s\n", pField);
  char* tFound = strstr(pBufferPtr, pField);
  int tSize = 0;
  if(tFound != NULL)
  {
    tFound += (strlen(pField) + pLenAfterField);
    int tIdx = 0;
    char tDelim = pDelims[tIdx];
    char *tShortest = NULL;
    char *tEnd = NULL;
    while(tDelim != '\0')
    {
      tDelim = pDelims[tIdx++]; // Ensures that \0 is also searched.
      tEnd = strchr(tFound, tDelim);
      if(tEnd != NULL && (NULL == tShortest || tEnd < tShortest))
      {
        tShortest = tEnd;
      }
    }
    
    tSize = (int) (tShortest - tFound);
    syslog(LOG_DEBUG, "Found %.*s  length: %d\n", tSize, tFound, tSize);
    if(pReturnSize != NULL)
    {
      *pReturnSize = tSize;
    }
  }
  else
  {
    syslog(LOG_DEBUG, "Not Found\n");
  }
  return tFound;
}

static char *getFromHeader(char *pHeaderPtr, const char *pField, int *pReturnSize)
{
  return getFromBuffer(pHeaderPtr, pField, 2, pReturnSize, "\r\n");
}

static char *getFromContent(char *pContentPtr, const char* pField, int *pReturnSize)
{
  return getFromBuffer(pContentPtr, pField, 1, pReturnSize, "\r\n");
}

static char *getFromSetup(char *pContentPtr, const char* pField, int *pReturnSize)
{
  return getFromBuffer(pContentPtr, pField, 1, pReturnSize, ";\r\n");
}

// Handles compiling the Apple-Challenge, HWID, and Server IP Address
// Into the response the airplay client is expecting.
static int buildAppleResponse(struct connection *pConn, unsigned char *pIpBin,
                              unsigned int pIpBinLen, char *pHWID)
{
  // Find Apple-Challenge
  char *tResponse = NULL;

  int tFoundSize = 0;
  char* tFound = getFromHeader(pConn->recv.data, "Apple-Challenge", &tFoundSize);
  if(tFound != NULL)
  {
    char tTrim[tFoundSize + 2];
    getTrimmed(tFound, tFoundSize, TRUE, TRUE, tTrim);
    syslog(LOG_DEBUG, "HeaderChallenge:  [%s] len: %d  sizeFound: %d\n", tTrim, (int)strlen(tTrim), tFoundSize);
    int tChallengeDecodeSize = 16;
    char *tChallenge = decode_base64((unsigned char *)tTrim, tFoundSize, &tChallengeDecodeSize);
    syslog(LOG_DEBUG, "Challenge Decode size: %d  expected 16\n", tChallengeDecodeSize);

    int tCurSize = 0;
    unsigned char tChalResp[38];

    memcpy(tChalResp, tChallenge, tChallengeDecodeSize);
    tCurSize += tChallengeDecodeSize;
    
    memcpy(tChalResp+tCurSize, pIpBin, pIpBinLen);
    tCurSize += pIpBinLen;

    memcpy(tChalResp+tCurSize, pHWID, HWID_SIZE);
    tCurSize += HWID_SIZE;

    int tPad = 32 - tCurSize;
    if (tPad > 0)
    {
      memset(tChalResp+tCurSize, 0, tPad);
      tCurSize += tPad;
    }

    char *tTmp = encode_base64((unsigned char *)tChalResp, tCurSize);
    syslog(LOG_DEBUG, "Full sig: %s\n", tTmp);
    free(tTmp);

    // RSA Encrypt
    RSA *rsa = loadKey();  // Free RSA
    int tSize = RSA_size(rsa);
    unsigned char tTo[tSize];
    RSA_private_encrypt(tCurSize, (unsigned char *)tChalResp, tTo, rsa, RSA_PKCS1_PADDING);
    
    // Wrap RSA Encrypted binary in Base64 encoding
    tResponse = encode_base64(tTo, tSize);
    int tLen = strlen(tResponse);
    while(tLen > 1 && tResponse[tLen-1] == '=')
    {
      tResponse[tLen-1] = '\0';
    }
    free(tChallenge);
    RSA_free(rsa);
  }

  if(tResponse != NULL)
  {
    // Append to current response
    addToShairBuffer(&(pConn->resp), "Apple-Response: ");
    addToShairBuffer(&(pConn->resp), tResponse);
    addToShairBuffer(&(pConn->resp), "\r\n");
    free(tResponse);
    return TRUE;
  }
  return FALSE;
}

//parseMessage(tConn->recv.data, tConn->recv.mark, &tConn->resp, ipstr, pHWADDR, tConn->keys);
static int parseMessage(struct connection *pConn, unsigned char *pIpBin, unsigned int pIpBinLen, char *pHWID)
{
  int tReturn = 0; // 0 = good, 1 = Needs More Data, -1 = close client socket.
  if(pConn->resp.data == NULL)
  {
    initBuffer(&(pConn->resp), MAX_SIZE);
  }

  char *tContent = getFromHeader(pConn->recv.data, "Content-Length", NULL);
  if(tContent != NULL)
  {
    int tContentSize = atoi(tContent);
    if(pConn->recv.marker == 0 || strlen(pConn->recv.data+pConn->recv.marker) != tContentSize)
    {
      syslog(HEADER_LOG_LEVEL, "Content-Length: %s value -> %d\n", tContent, tContentSize);
      if(pConn->recv.marker != 0)
      {
        syslog(HEADER_LOG_LEVEL, "ContentPtr has %d, but needs %d\n", 
                (int)strlen(pConn->recv.data+pConn->recv.marker), tContentSize);
      }
      // check if value in tContent > 2nd read from client.
      return 1; // means more content-length needed
    }
  }
  else
  {
    syslog(LOG_DEBUG, "No content, header only\n");
  }

  // "Creates" a new Response Header for our response message
  addToShairBuffer(&(pConn->resp), "RTSP/1.0 200 OK\r\n");

  int tLen = strchr(pConn->recv.data, ' ') - pConn->recv.data;
  if(tLen < 0 || tLen > 20)
  {
    tLen = 20;
  }
  syslog(LOG_INFO, "********** RECV %.*s **********\n", tLen, pConn->recv.data);

  if(pConn->password != NULL)
  {
    
  }

  if(buildAppleResponse(pConn, pIpBin, pIpBinLen, pHWID)) // need to free sig
  {
    syslog(LOG_DEBUG, "Added AppleResponse to Apple-Challenge request\n");
  }

  // Find option, then based on option, do different actions.
  if(strncmp(pConn->recv.data, "OPTIONS", 7) == 0)
  {
    propogateCSeq(pConn);
    addToShairBuffer(&(pConn->resp),
      "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER\r\n");
  }
  else if(!strncmp(pConn->recv.data, "ANNOUNCE", 8))
  {
    char *tContent = pConn->recv.data + pConn->recv.marker;
    int tSize = 0;
    char *tHeaderVal = getFromContent(tContent, "a=aesiv", &tSize); // Not allocated memory, just pointing
    if(tSize > 0)
    {
      int tKeySize = 0;
      char tEncodedAesIV[tSize + 2];
      getTrimmed(tHeaderVal, tSize, TRUE, TRUE, tEncodedAesIV);
      syslog(LOG_DEBUG, "AESIV: [%.*s] Size: %d  Strlen: %d\n", tSize, tEncodedAesIV, tSize, (int)strlen(tEncodedAesIV));
      char *tDecodedIV =  decode_base64((unsigned char*) tEncodedAesIV, tSize, &tSize);

      // grab the key, copy it out of the receive buffer
      tHeaderVal = getFromContent(tContent, "a=rsaaeskey", &tKeySize);
      char tEncodedAesKey[tKeySize + 2]; // +1 for nl, +1 for \0
      getTrimmed(tHeaderVal, tKeySize, TRUE, TRUE, tEncodedAesKey);
      syslog(LOG_DEBUG, "AES KEY: [%s] Size: %d  Strlen: %d\n", tEncodedAesKey, tKeySize, (int)strlen(tEncodedAesKey));
      // remove base64 coding from key
      char *tDecodedAesKey = decode_base64((unsigned char*) tEncodedAesKey,
                              tKeySize, &tKeySize);  // Need to free DecodedAesKey

      // Grab the formats
      int tFmtpSize = 0;
      char *tFmtp = getFromContent(tContent, "a=fmtp", &tFmtpSize);  // Don't need to free
      tFmtp = getTrimmedMalloc(tFmtp, tFmtpSize, TRUE, FALSE); // will need to free
      syslog(LOG_DEBUG, "Format: %s\n", tFmtp);

      RSA *rsa = loadKey();
      // Decrypt the binary aes key
      char *tDecryptedKey = malloc(RSA_size(rsa) * sizeof(char)); // Need to Free Decrypted key
      //char tDecryptedKey[RSA_size(rsa)];
      if(RSA_private_decrypt(tKeySize, (unsigned char *)tDecodedAesKey, 
      (unsigned char*) tDecryptedKey, rsa, RSA_PKCS1_OAEP_PADDING) >= 0)
      {
        syslog(LOG_DEBUG, "Decrypted AES key from RSA Successfully\n");
      }
      else
      {
        syslog(LOG_INFO, "Error Decrypting AES key from RSA\n");
      }
      free(tDecodedAesKey);
      RSA_free(rsa);

      setKeys(pConn->keys, tDecodedIV, tDecryptedKey, tFmtp);

      propogateCSeq(pConn);
    }
  }
  else if(!strncmp(pConn->recv.data, "SETUP", 5))
  {
    // Setup pipes
    struct comms *tComms = pConn->hairtunes;
    if (! (pipe(tComms->in) == 0 && pipe(tComms->out) == 0))
    {
      syslog(LOG_INFO, "Error setting up hairtunes communications...some things probably wont work very well.\n");
    }
    
    // Setup fork
    char tPort[8] = "6000";  // get this from dup()'d stdout of child pid

    int tPid = fork();
    if(tPid == 0)
    {
      int tDataport=0;
      char tCPortStr[8] = "59010";
      char tTPortStr[8] = "59012";
      int tSize = 0;

      char *tFound  =getFromSetup(pConn->recv.data, "control_port", &tSize);
      getTrimmed(tFound, tSize, 1, 0, tCPortStr);
      tFound = getFromSetup(pConn->recv.data, "timing_port", &tSize);
      getTrimmed(tFound, tSize, 1, 0, tTPortStr);

      syslog(LOG_DEBUG, "converting %s and %s from str->int\n", tCPortStr, tTPortStr);
      int tControlport = atoi(tCPortStr);
      int tTimingport = atoi(tTPortStr);

      syslog(LOG_DEBUG, "Got %d for CPort and %d for TPort\n", tControlport, tTimingport);
      char *tRtp = NULL;
      char *tPipe = NULL;

      // *************************************************
      // ** Setting up Pipes, AKA no more debug/output  **
      // *************************************************
      dup2(tComms->in[0],0);   // Input to child
      closePipe(&(tComms->in[0]));
      closePipe(&(tComms->in[1]));

      dup2(tComms->out[1], 1); // Output from child
      closePipe(&(tComms->out[1]));
      closePipe(&(tComms->out[0]));

      struct keyring *tKeys = pConn->keys;
      pConn->keys = NULL;
      pConn->hairtunes = NULL;

      // Free up any recv buffers, etc..
      if(pConn->clientSocket != -1)
      {
        close(pConn->clientSocket);
        pConn->clientSocket = -1;
      }
      cleanupBuffers(pConn);
      hairtunes_init(tKeys->aeskey, tKeys->aesiv, tKeys->fmt, tControlport, tTimingport,
                      tDataport, tRtp, tPipe,  bufferStartFill);

      // Quit when finished.
      syslog(LOG_DEBUG, "Returned from hairtunes init....returning -1, should close out this whole side of the fork\n");
      return -1;
    }
    else if(tPid >0)
    {
      // Ensure Connection has access to the pipe.
      closePipe(&(tComms->in[0]));
      closePipe(&(tComms->out[1]));

      char tFromHairtunes[80];
      int tRead = read(tComms->out[0], tFromHairtunes, 80);
      if(tRead <= 0)
      {
        syslog(LOG_ERR, "Error reading port from hairtunes function, assuming default port: %s\n", tPort);
      }
      else
      {
        int tSize = 0;
        char *tPortStr = getFromHeader(tFromHairtunes, "port", &tSize);
        if(tPortStr != NULL)
        {
          getTrimmed(tPortStr, tSize, TRUE, FALSE, tPort);
        }
        else
        {
          syslog(LOG_ERR, "Read %d bytes, Error translating %s into a port\n", tRead, tFromHairtunes);
        }
      }
      //  READ Ports from here?close(pConn->hairtunes_pipes[0]);
      propogateCSeq(pConn);
      int tSize = 0;
      char *tTransport = getFromHeader(pConn->recv.data, "Transport", &tSize);
      addToShairBuffer(&(pConn->resp), "Transport: ");
      addNToShairBuffer(&(pConn->resp), tTransport, tSize);
      // Append server port:
      addToShairBuffer(&(pConn->resp), ";server_port=");
      addToShairBuffer(&(pConn->resp), tPort);
      addToShairBuffer(&(pConn->resp), "\r\nSession: DEADBEEF\r\n");
    }
    else
    {
      syslog(LOG_ERR, "Error forking process....dere' be errors round here.\n");
      return -1;
    }
  }
  else if(!strncmp(pConn->recv.data, "TEARDOWN", 8))
  {
    // Be smart?  Do more finish up stuff...
    addToShairBuffer(&(pConn->resp), "Connection: close\r\n");
    propogateCSeq(pConn);
    close(pConn->hairtunes->in[1]);
    syslog(LOG_DEBUG, "Tearing down connection, closing pipes\n");
    //close(pConn->hairtunes->out[0]);
    tReturn = -1;  // Close client socket, but sends an ACK/OK packet first
  }
  else if(!strncmp(pConn->recv.data, "FLUSH", 5))
  {
    write(pConn->hairtunes->in[1], "flush\n", 6);
    propogateCSeq(pConn);
  }
  else if(!strncmp(pConn->recv.data, "SET_PARAMETER", 13))
  {
    propogateCSeq(pConn);
    int tSize = 0;
    char *tVol = getFromHeader(pConn->recv.data, "volume", &tSize);
    syslog(LOG_DEBUG, "About to write [vol: %.*s] data to hairtunes\n", tSize, tVol);

    write(pConn->hairtunes->in[1], "vol: ", 5);
    write(pConn->hairtunes->in[1], tVol, tSize);
    write(pConn->hairtunes->in[1], "\n", 1);
    syslog(LOG_DEBUG, "Finished writing data write data to hairtunes\n");
  }
  else
  {
    syslog(LOG_DEBUG, "\n\nUn-Handled recv: %s\n", pConn->recv.data);
    propogateCSeq(pConn);
  }
  addToShairBuffer(&(pConn->resp), "\r\n");
  return tReturn;
}

// Copies CSeq value from request, and adds standard header values in.
static void propogateCSeq(struct connection *pConn) //char *pRecvBuffer, struct shairbuffer *pConn->recp.data)
{
  int tSize=0;
  char *tRecPtr = getFromHeader(pConn->recv.data, "CSeq", &tSize);
  addToShairBuffer(&(pConn->resp), "Audio-Jack-Status: connected; type=analog\r\n");
  addToShairBuffer(&(pConn->resp), "CSeq: ");
  addNToShairBuffer(&(pConn->resp), tRecPtr, tSize);
  addToShairBuffer(&(pConn->resp), "\r\n");
}

void cleanupBuffers(struct connection *pConn)
{
  if(pConn->recv.data != NULL)
  {
    free(pConn->recv.data);
    pConn->recv.data = NULL;
  }
  if(pConn->resp.data != NULL)
  {
    free(pConn->resp.data);
    pConn->resp.data = NULL;
  }
}

static void cleanup(struct connection *pConn)
{
  cleanupBuffers(pConn);
  if(pConn->hairtunes != NULL)
  {

    closePipe(&(pConn->hairtunes->in[0]));
    closePipe(&(pConn->hairtunes->in[1]));
    closePipe(&(pConn->hairtunes->out[0]));
    closePipe(&(pConn->hairtunes->out[1]));
  }
  if(pConn->keys != NULL)
  {
    if(pConn->keys->aesiv != NULL)
    {
      free(pConn->keys->aesiv);
    }
    if(pConn->keys->aeskey != NULL)
    {
      free(pConn->keys->aeskey);
    }
    if(pConn->keys->fmt != NULL)
    {
      free(pConn->keys->fmt);
    }
    pConn->keys = NULL;
  }
  if(pConn->clientSocket != -1)
  {
    close(pConn->clientSocket);
    pConn->clientSocket = -1;
  }
}

static int startAvahi(const char *pHWStr, const char *pServerName, int pPort)
{
  int tMaxServerName = 25; // Something reasonable?  iPad showed 21, iphone 25
  int tPid = fork();
  if(tPid == 0)
  {
    char tName[100 + HWID_SIZE + 3];
    if(strlen(pServerName) > tMaxServerName)
    {
      syslog(LOG_ERR,"Hey dog, we see you like long server names, "
              "so we put a strncat in our command so we don't buffer overflow, while you listen to your flow.\n"
              "We just used the first %d characters.  Pick something shorter if you want\n", tMaxServerName);
    }
    
    tName[0] = '\0';
    char tPort[SERVLEN];
    sprintf(tPort, "%d", pPort);
    strcat(tName, pHWStr);
    strcat(tName, "@");
    strncat(tName, pServerName, tMaxServerName);
    syslog(AVAHI_LOG_LEVEL, "Avahi/DNS-SD Name: %s\n", tName);
    
    execlp("avahi-publish-service", "avahi-publish-service", tName,
         "_raop._tcp", tPort, "tp=UDP","sm=false","sv=false","ek=1","et=0,1",
         "cn=0,1","ch=2","ss=16","sr=44100","pw=false","vn=3","txtvers=1", NULL);
    execlp("dns-sd", "dns-sd", "-R", tName,
         "_raop._tcp", ".", tPort, "tp=UDP","sm=false","sv=false","ek=1","et=0,1",
         "cn=0,1","ch=2","ss=16","sr=44100","pw=false","vn=3","txtvers=1", NULL);
    execlp("mDNSPublish", "mDNSPublish", tName,
         "_raop._tcp", tPort, "tp=UDP","sm=false","sv=false","ek=1","et=0,1",
         "cn=0,1","ch=2","ss=16","sr=44100","pw=false","vn=3","txtvers=1", NULL);
    if(errno == -1) {
            perror("error");
    }

    syslog(LOG_INFO, "Bad error... couldn't find or failed to run: avahi-publish-service OR dns-sd OR mDNSPublish\n");
    exit(1);
  }
  else
  {
    syslog(LOG_DEBUG, "Avahi/DNS-SD started on PID: %d\n", tPid);
  }
  return tPid;
}

static int getAvailChars(struct shairbuffer *pBuf)
{
  return (pBuf->maxsize / sizeof(char)) - pBuf->current;
}

static void addToShairBuffer(struct shairbuffer *pBuf, char *pNewBuf)
{
  addNToShairBuffer(pBuf, pNewBuf, strlen(pNewBuf));
}

static void addNToShairBuffer(struct shairbuffer *pBuf, char *pNewBuf, int pNofNewBuf)
{
  int tAvailChars = getAvailChars(pBuf);
  if(pNofNewBuf > tAvailChars)
  {
    int tNewSize = pBuf->maxsize * 2 + MAX_SIZE + sizeof(char);
    char *tTmpBuf = malloc(tNewSize);

    tTmpBuf[0] = '\0';
    memset(tTmpBuf, 0, tNewSize/sizeof(char));
    memcpy(tTmpBuf, pBuf->data, pBuf->current);
    free(pBuf->data);

    pBuf->maxsize = tNewSize;
    pBuf->data = tTmpBuf;
  }
  memcpy(pBuf->data + pBuf->current, pNewBuf, pNofNewBuf);
  pBuf->current += pNofNewBuf;
  if(getAvailChars(pBuf) > 1)
  {
    pBuf->data[pBuf->current] = '\0';
  }
}

static char *getTrimmedMalloc(char *pChar, int pSize, int pEndStr, int pAddNL)
{
  int tAdditionalSize = 0;
  if(pEndStr)
    tAdditionalSize++;
  if(pAddNL)
    tAdditionalSize++;
  char *tTrimDest = malloc(sizeof(char) * (pSize + tAdditionalSize));
  return getTrimmed(pChar, pSize, pEndStr, pAddNL, tTrimDest);
}

// Must free returned ptr
static char *getTrimmed(char *pChar, int pSize, int pEndStr, int pAddNL, char *pTrimDest)
{
  int tSize = pSize;
  if(pEndStr)
  {
    tSize++;
  }
  if(pAddNL)
  {
    tSize++;
  }
  
  memset(pTrimDest, 0, tSize);
  memcpy(pTrimDest, pChar, pSize);
  if(pAddNL)
  {
    pTrimDest[pSize] = '\n';
  }
  if(pEndStr)
  {
    pTrimDest[tSize-1] = '\0';
  }
  return pTrimDest;
}

static void initConnection(struct connection *pConn, struct keyring *pKeys,
                    struct comms *pComms, int pSocket, char *pPassword)
{
  pConn->hairtunes = pComms;
  if(pKeys != NULL)
  {
    pConn->keys = pKeys;
    pConn->keys->aesiv = NULL;
    pConn->keys->aeskey = NULL;
    pConn->keys->fmt = NULL;
  }
  pConn->recv.data = NULL;  // Pre-init buffer expected to be NULL
  pConn->resp.data = NULL;  // Pre-init buffer expected to be NULL
  pConn->clientSocket = pSocket;
  if(strlen(pPassword) >0)
  {
    pConn->password = pPassword;
  }
  else
  {
    pConn->password = NULL;
  }
}

static void closePipe(int *pPipe)
{
  if(*pPipe != -1)
  {
    close(*pPipe);
    *pPipe = -1;
  }
}

static void initBuffer(struct shairbuffer *pBuf, int pNumChars)
{
  if(pBuf->data != NULL)
  {
    syslog(LOG_DEBUG, "Hrm, buffer wasn't cleaned up....trying to free\n");
    free(pBuf->data);
    syslog(LOG_DEBUG, "Free didn't seem to seg fault....huzzah\n");
  }
  pBuf->current = 0;
  pBuf->marker = 0;
  pBuf->maxsize = sizeof(char) * pNumChars;
  pBuf->data = malloc(pBuf->maxsize);
  memset(pBuf->data, 0, pBuf->maxsize);
}

static void setKeys(struct keyring *pKeys, char *pIV, char* pAESKey, char *pFmtp)
{
  if(pKeys->aesiv != NULL)
  {
    free(pKeys->aesiv);
  }
  if(pKeys->aeskey != NULL)
  {
    free(pKeys->aeskey);
  }
  if(pKeys->fmt != NULL)
  {
    free(pKeys->fmt);
  }
  pKeys->aesiv = pIV;
  pKeys->aeskey = pAESKey;
  pKeys->fmt = pFmtp;
}

#define AIRPORT_PRIVATE_KEY \
"-----BEGIN RSA PRIVATE KEY-----\n" \
"MIIEpQIBAAKCAQEA59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUt\n" \
"wC5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDRKSKv6kDqnw4U\n" \
"wPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuBOitnZ/bDzPHrTOZz0Dew0uowxf\n" \
"/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJQ+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/\n" \
"UAaHqn9JdsBWLUEpVviYnhimNVvYFZeCXg/IdTQ+x4IRdiXNv5hEewIDAQABAoIBAQDl8Axy9XfW\n" \
"BLmkzkEiqoSwF0PsmVrPzH9KsnwLGH+QZlvjWd8SWYGN7u1507HvhF5N3drJoVU3O14nDY4TFQAa\n" \
"LlJ9VM35AApXaLyY1ERrN7u9ALKd2LUwYhM7Km539O4yUFYikE2nIPscEsA5ltpxOgUGCY7b7ez5\n" \
"NtD6nL1ZKauw7aNXmVAvmJTcuPxWmoktF3gDJKK2wxZuNGcJE0uFQEG4Z3BrWP7yoNuSK3dii2jm\n" \
"lpPHr0O/KnPQtzI3eguhe0TwUem/eYSdyzMyVx/YpwkzwtYL3sR5k0o9rKQLtvLzfAqdBxBurciz\n" \
"aaA/L0HIgAmOit1GJA2saMxTVPNhAoGBAPfgv1oeZxgxmotiCcMXFEQEWflzhWYTsXrhUIuz5jFu\n" \
"a39GLS99ZEErhLdrwj8rDDViRVJ5skOp9zFvlYAHs0xh92ji1E7V/ysnKBfsMrPkk5KSKPrnjndM\n" \
"oPdevWnVkgJ5jxFuNgxkOLMuG9i53B4yMvDTCRiIPMQ++N2iLDaRAoGBAO9v//mU8eVkQaoANf0Z\n" \
"oMjW8CN4xwWA2cSEIHkd9AfFkftuv8oyLDCG3ZAf0vrhrrtkrfa7ef+AUb69DNggq4mHQAYBp7L+\n" \
"k5DKzJrKuO0r+R0YbY9pZD1+/g9dVt91d6LQNepUE/yY2PP5CNoFmjedpLHMOPFdVgqDzDFxU8hL\n" \
"AoGBANDrr7xAJbqBjHVwIzQ4To9pb4BNeqDndk5Qe7fT3+/H1njGaC0/rXE0Qb7q5ySgnsCb3DvA\n" \
"cJyRM9SJ7OKlGt0FMSdJD5KG0XPIpAVNwgpXXH5MDJg09KHeh0kXo+QA6viFBi21y340NonnEfdf\n" \
"54PX4ZGS/Xac1UK+pLkBB+zRAoGAf0AY3H3qKS2lMEI4bzEFoHeK3G895pDaK3TFBVmD7fV0Zhov\n" \
"17fegFPMwOII8MisYm9ZfT2Z0s5Ro3s5rkt+nvLAdfC/PYPKzTLalpGSwomSNYJcB9HNMlmhkGzc\n" \
"1JnLYT4iyUyx6pcZBmCd8bD0iwY/FzcgNDaUmbX9+XDvRA0CgYEAkE7pIPlE71qvfJQgoA9em0gI\n" \
"LAuE4Pu13aKiJnfft7hIjbK+5kyb3TysZvoyDnb3HOKvInK7vXbKuU4ISgxB2bB3HcYzQMGsz1qJ\n" \
"2gG0N5hvJpzwwhbhXqFKA4zaaSrw622wDniAK5MlIE0tIAKKP4yxNGjoD2QYjhBGuhvkWKY=\n" \
"-----END RSA PRIVATE KEY-----"

static RSA *loadKey(void)
{
  BIO *tBio = BIO_new_mem_buf(AIRPORT_PRIVATE_KEY, -1);
  RSA *rsa = PEM_read_bio_RSAPrivateKey(tBio, NULL, NULL, NULL); //NULL, NULL, NULL);
  BIO_free(tBio);
  syslog(RSA_LOG_LEVEL, "RSA Key: %d\n", RSA_check_key(rsa));
  return rsa;
}
