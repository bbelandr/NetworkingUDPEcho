/************************************************************************
* File:  utils.h
*
* Purpose:
*
* Notes:
*
************************************************************************/
#ifndef	__utils_h
#define	__utils_h


//#define is_bigendian() ( (*(char*)&bsti) == 0 )
// ( (*(char*)&bsti) == 0 )
bool is_bigendian();

uint64_t htonll (uint64_t InAddr) ;
void swapbytes(void *_object, size_t size);



void die(const char *msg);
double timestamp();
extern char Version[];

double getTime(int);
double getTime1();
double getCurTimeD();
double getCurTime(struct timespec *ts);
double getTimestampD(); 

int delay(int64_t ns);
int gettimeofday_benchmark();

long getMicroseconds(struct timeval *t);
double convertTimeval(struct timeval *t);

long getTimeSpan(struct timeval *start_time, struct timeval *end_time);
void setUnblockOption(int sock, char unblock);
void sockBlockingOn(int sock);
void sockBlockingOff(int sock);

#endif


