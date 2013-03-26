#ifndef _HAIRTUNES_H_
#define _HAIRTUNES_H_
int hairtunes_init(char *pAeskey, char *pAesiv, char *fmtpstr, int pCtrlPort, int pTimingPort,
         int pDataPort, char *pRtpHost, char*pPipeName, int bufStartFill);

// default buffer size
// needs to be a power of 2 because of the way BUFIDX(seqno) works
//#define BUFFER_FRAMES 1024

int get_buffer_frames();
void set_buffer_frames(int);
#endif 
