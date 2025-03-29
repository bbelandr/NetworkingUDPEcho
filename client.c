/*********************************************************
* Module Name: UDPEchoV2   client  
*
* File Name:    client.c
*
* Summary:
*  This file contains the client portion of a client/server
*    UDP-based performance tool.
*  
* Params:
*  char *  server = argv[1];
*  uint32_t  serverPort = atoi(argv[2]);
*  uint32_t delay atoll(argv[3]);
*  uint32_t messageSize = atoi(argv[4]);
*  uin32_t nIterations = atoi(argv[5]);
*
*  Usage :   client
*             <Server IP>
*             <Server Port>
*             [<Iteration Delay (usecs)>]
*             [<Message Size (bytes)>]
*             [<# of iterations>]
*
* outputs:  
*    The per iteration information printed to stdout:
*      printf("%f %4.9f %4.9f %d %d\n", 
*             wallTime, RTTSample, smoothedRTT, 
*             receivedCount,  numberRTTSamples);
*
*       wallTime:  The time
*       RTTSample:  the sample based on the current message arrival
*       smoothedRTT: computes a smoothed RTT average using a weighted filter
*       numberRTTSamples: The number of samples received
*
* A1: 3/12/2025   Extend with opMode 1 -  CBR traf gen 
*
*********************************************************/
#include "UDPEcho.h"
#include "AddressUtility.h"
#include "utils.h"

void myUsage();
void clientCNTCCode();
void CatchAlarm(int ignored);

extern char Version[];

//uncomment to see debug trace
//#define TRACEME 1


//Define this globally so our async handlers can access

char *server = NULL;                   /* IP address of server */
int sock = -1;                         /* Socket descriptor */
double startTime = 0.0;
double endTime = 0.0;

char* outputFile = NULL;
uint16_t opMode = 0;

//Stats and counters - these are global as the asynchronous handlers need access
uint32_t numberOfTrials=0; /*counts number of attempts */
uint32_t numberLost=0;
uint32_t numberOutOfOrder=0;
uint32_t numberTOs=0;
//incremented by the rx seq number - nest rx seq number 
uint32_t totalGaps=0;

uint32_t TxErrorCount=0;
uint32_t RxErrorCount=0;
uint32_t largestSeqRecv = 0;
uint32_t receivedCount = 0;


double RTTSum = 0.0;
uint32_t numberRTTSamples=0;

//Maintains current wall clock time
double wallTime = 0.0;

//Prior to sendto, records the current wall clock time
double lastMsgTxWallTime = 0.0;
struct timespec msgTxTime;    



static const unsigned int TIMEOUT_SECS = 2; // Seconds between retransmits

void myUsage()
{


  printf("UDPEchoV2:client(v%s): <Server IP> <Server Port> <Iteration Delay (usecs)> <Message Size (bytes)>] <# of iterations> <opMode> 'outputFile'\n",
                Version);
}


/*************************************************************
*
* Function: Main program for  UDPEcho client
*           
* inputs: 
*  char *  server = argv[1];
*  uint32_t  serverPort = atoi(argv[2]);
*  uint32_t delay atoll(argv[3]);
*  uint32_t messageSize = atoi(argv[4]);
*  uin32_t nIterations = atoi(argv[5]);
*
*  Usage :   client
*             <Server IP>
*             <Server Port>
*             [<Iteration Delay (usecs)>]
*             [<Message Size (bytes)>]
*             [<# of iterations>]
*
*   Timeval struct defines the two components as long ints
*         The following nicely prints: 
*         printf("%ld.%06ld\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
*
****************************************************************/

int main(int argc, char *argv[]) 
{
  int32_t rc = NOERROR;
  uint32_t delay=0;
  int32_t  messageSize=0;
  int32_t nIterations=  -1;
  char *service = NULL;


  struct addrinfo addrCriteria;         // Criteria for address
  struct addrinfo *servAddr = NULL;;    // Holder for returned list of server addrs

  int rtnVal = 0;
  int sock = -1;
  struct sigaction handler; // Signal handler
  ssize_t numBytes = 0;
  struct sockaddr_storage fromAddr; // Source address of server
  socklen_t fromAddrLen = 0;
  char *TxBuffer = NULL;
  char *RxBuffer = NULL;
  //Used to help pack and unpack the network buffer
  uint32_t *TxIntPtr  = NULL;
  uint32_t *RxIntPtr  = NULL;
  bool loopForever=false;
  bool loopFlag=true;
  double iterationDelay = 0.0;
  uint32_t sequenceNumber=0;
  struct timespec reqDelay;
  struct timespec remDelay;

  //Used for the RTT sample
  double  Tstart = 0.0;
  double  Tstop = 0.0;
  //most recent RTT sample
  double RTTSample = 0.0;
  //smoothed avg
  double smoothedRTT = 0.0;
  double alpha = 0.10;
  messageHeaderDefault *TxHeaderPtr=NULL;
  messageHeaderDefault *RxHeaderPtr=NULL;
  uint32_t count = 0;



  if (argc <= 3)    /* need at least server name and port */
  {
    myUsage();
    exit(1);
  }

  wallTime = getCurTimeD();
  startTime = wallTime;



  //set defaults
  delay=1000000;
  messageSize = 56;
  nIterations=0;

  server = argv[1];     // First arg: server address/name
  service = argv[2];

//Delay in microseconds
  if (argc >3)
    delay = atoi(argv[3]);

//messageSize in bytes
  if (argc >4)
  {
    messageSize= atoi(argv[4]);
    if (messageSize > MAX_DATA_BUFFER)
      messageSize = MAX_DATA_BUFFER;
  }

  if (argc >5) {
    nIterations = atoi(argv[5]);
    if (nIterations == 0)
      loopForever=true;
  }

//OpMode
  if (argc > 6) {
    opMode = atoi(argv[6]);
  }

//outputFile
  if (argc > 7) {
    outputFile = argv[7];
    // Redirecting stdout to outputFile
    freopen(outputFile, "w", stdout);
  }

  signal (SIGINT, clientCNTCCode);

  iterationDelay = ((double)delay)/1000000;/* Iteration delay in seconds */
  reqDelay.tv_sec = (uint32_t)floor(iterationDelay);
  if (reqDelay.tv_sec >= 1)
    reqDelay.tv_nsec = (uint32_t)( 1000000000 * (iterationDelay - (double)reqDelay.tv_sec));
  else
    reqDelay.tv_nsec = (uint32_t) (1000000000 * iterationDelay);


  remDelay.tv_sec = 0;
  remDelay.tv_nsec = 0;


  sequenceNumber++;


  //Init memory for first send
  TxBuffer = malloc((size_t)messageSize);
  if (TxBuffer == NULL) {
    printf("client: HARD ERROR malloc of Tx  %d bytes failed \n", messageSize);
    exit(1);
  }
  memset(TxBuffer, 0, messageSize);
  //This pointer is used when packing the header into the network buffer
  TxIntPtr  = (uint32_t *) TxBuffer;

  messageHeaderDefault TxHeader;
  TxHeaderPtr=&TxHeader;


  //Design note:  We will use separate header data structures. 
  //And then to pack or unpack the/from the buffer we do each uint32_t at a time performing
  //  the conversion to the correct byte order.


//typedef struct {
//  uint32_t sequenceNum;
//  uint32_t timeSentSeconds;
//  uint32_t timeSentNanoSeconds;
//} messageHeaderDefault;

  //Init memory for receive 
  RxBuffer = malloc((size_t)messageSize);
  if (RxBuffer == NULL) {
    printf("client: HARD ERROR malloc of Rx %d bytes failed \n", messageSize);
    exit(1);
  }
  memset(RxBuffer, 0, messageSize);

  messageHeaderDefault RxHeader;
  RxHeaderPtr=&RxHeader;

  //This pointer is used when packing the header into the network buffer
  RxIntPtr  = (uint32_t *) RxBuffer;


#ifdef TRACEME
  printf("client: server:%s  service:%s  messageSize:%d \n", 
     server,service,messageSize);
#endif

//Code to confirm the sizes of various structs and data types
//LECTURE
  uint32_t tmpW, tmpX, tmpY, tmpZ;
  tmpW = sizeof(struct sockaddr);
  tmpX = sizeof(struct sockaddr_in);
  tmpY = sizeof(struct sockaddr_in6);
  tmpZ = sizeof(struct sockaddr_storage);
  printf("client: size sockadd:%d, sockaddr_in:%d, sockaddr_in6:%d, sockaddr_storage:%d \n",
          tmpW, tmpX, tmpY, tmpZ);


  // Tell the system what kind(s) of address info we want
  memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
  addrCriteria.ai_family = AF_UNSPEC;             // Any address family
  addrCriteria.ai_socktype = SOCK_DGRAM;          // Only datagram sockets
  addrCriteria.ai_protocol = IPPROTO_UDP;         // Only UDP protocol

  // Get address(es)
  rtnVal = getaddrinfo(server, service, &addrCriteria, &servAddr);
  if (rtnVal != 0) {
    printf("client: failed getaddrinfo, %s \n", gai_strerror(rtnVal));
  }

#if 0

       int getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints,
                       struct addrinfo **res);

 struct addrinfo {
               int              ai_flags;
               int              ai_family;
               int              ai_socktype;
               int              ai_protocol;
               socklen_t        ai_addrlen;
               struct sockaddr *ai_addr;
               char            *ai_canonname;
               struct addrinfo *ai_next;
           };

#endif

  //servAddr holds list of addrinfo's
  // Display returned addresses
  count=0;
  struct sockaddr_storage clntAddr; // Client address

  struct addrinfo *addr = NULL;
  for (addr = servAddr; addr != NULL; addr = addr->ai_next) {
    count++;
    PrintSocketAddress(addr->ai_addr, stdout);
    fputc('\n', stdout);
  }
  printf("client: found %d addresses based on name %s \n",count, server);
  struct sockaddr *myIPV6Addr = NULL;
  count = findAF_INET6SocketAddress(servAddr,&myIPV6Addr);
  if (count == 0) {
    printf("Did not find any V6 addresses \n");
  } else{
    printf("client: found %d V6 addresses based on name %s \n",count, server);
    printf("getaddrinfo:  found addresse \n");
    PrintSocketAddress(myIPV6Addr, stdout);
    fputc('\n', stdout);
  }

  struct sockaddr *myIPV4Addr = NULL;
  count = findAF_INETSocketAddress(servAddr,&myIPV4Addr);
  if (count > 0) {
    printf("client: found %d V4 addresses based on name %s \n",count, server);
    PrintSocketAddress(myIPV4Addr, stdout);
    fputc('\n', stdout);
  } else {
    printf("getaddrinfo:  Failed to find V4 addr ???  \n");
  }

  // Create a reliable, stream socket using UDP
  sock = socket(servAddr->ai_family, servAddr->ai_socktype,
      servAddr->ai_protocol); // Socket descriptor for client
  if (sock < 0)
    DieWithSystemMessage("socket() failed");

  // Set signal handler for alarm signal
  handler.sa_handler = CatchAlarm;
  if (sigfillset(&handler.sa_mask) < 0) // Block everything in handler
    DieWithSystemMessage("sigfillset() failed");
  handler.sa_flags = 0;

  if (sigaction(SIGALRM, &handler, 0) < 0)
    DieWithSystemMessage("sigaction() failed for SIGALRM");



  while (loopFlag)
  {

    lastMsgTxWallTime = getCurTime(&msgTxTime);
    wallTime = lastMsgTxWallTime;
    //Update the TxHeader
    TxHeaderPtr->sequenceNum = sequenceNumber++;
    TxHeaderPtr->timeSentSeconds = msgTxTime.tv_sec;
    TxHeaderPtr->timeSentNanoSeconds = msgTxTime.tv_nsec;
    TxHeaderPtr->opMode = opMode;       // Updated to also include the opMode

    //pack the header into the network buffer
    TxIntPtr  = (uint32_t *) TxBuffer;
    *TxIntPtr++  = htonl(TxHeaderPtr->sequenceNum);
    *TxIntPtr++  = htonl(TxHeaderPtr->timeSentSeconds);
    *TxIntPtr++  = htonl(TxHeaderPtr->timeSentNanoSeconds);
    *TxIntPtr++  = htonl(TxHeaderPtr->opMode);
    rc = NOERROR;
    numberOfTrials++;
    if ( (!loopForever) &&  (numberOfTrials > nIterations) )
    {
         loopFlag=false;
	 //A1
         clientCNTCCode();
         break;
    }
    Tstart= getTimestampD();
    // Send the string to the server
    numBytes = sendto(sock, TxBuffer, messageSize, 0,
      servAddr->ai_addr, servAddr->ai_addrlen);
    if (numBytes < 0) {
        TxErrorCount++;
//#ifdef TRACEME
        perror("client: sendto error \n");
//#endif
        continue;
    }
    else if (numBytes != messageSize){
//#ifdef TRACEME
      printf("client: sendto return %d not equal to messageSize:%d \n", (int32_t) numBytes,messageSize);
//#endif
        continue;
    }

    // Receive a response

    // Set length of from address structure (in-out parameter)
    fromAddrLen = sizeof(fromAddr);
    alarm(TIMEOUT_SECS); // Set the timeout

    //returns -1 on error else bytes received
    rc =  recvfrom(sock, RxBuffer, messageSize, 0, (struct sockaddr *) &fromAddr, &fromAddrLen);
    if (rc == ERROR) {
      if (errno == EINTR) {     // Alarm went off
//#ifdef TRACEME
          printf("client: recvfrom error EINTR, numberTOs:%d \n",numberTOs);
//#endif
         rc = NOERROR;
         continue;
      } else {
        RxErrorCount++;
//#ifdef TRACEME
          perror("client: recvfrom other error \n");
//#endif
      }
    }
    else {
      //succeeded!
      numBytes=rc;
      alarm(0);
 //Obtain RTT sample
      Tstop =  getTimestampD();
      RTTSample= Tstop - Tstart;
      RTTSum += RTTSample;
      numberRTTSamples++;
      smoothedRTT = (1-alpha)*smoothedRTT + alpha*RTTSample;
      rc = NOERROR;
      receivedCount++;
      wallTime = getCurTimeD();

      RxIntPtr  = (uint32_t *) RxBuffer;
      RxHeaderPtr->sequenceNum =  ntohl(*RxIntPtr++);
      RxHeaderPtr->timeSentSeconds =  ntohl(*RxIntPtr++);
      RxHeaderPtr->timeSentNanoSeconds  =  ntohl(*RxIntPtr++);

      printf("%f %4.9f %4.9f %d %d\n", 
             wallTime, RTTSample, smoothedRTT, 
             receivedCount,  numberRTTSamples);
#ifdef TRACEME
      printf("client: succeeded to recv %d bytes from server \n", (int) numBytes);
      printf("Rxed: %d %d %d \n", 
             RxHeaderPtr->sequenceNum, 
             RxHeaderPtr->timeSentSeconds, RxHeaderPtr->timeSentNanoSeconds);
#endif
    }
    //delay requested amount
    rc = nanosleep((const struct timespec*)&reqDelay, &remDelay);


  }
  close(sock);
  exit(0);
}

// Handler for SIGALRM
void CatchAlarm(int ignored) {
  numberTOs++;
}


void clientCNTCCode() 
{
  double avgRTT  = 0.0;
  double avgLossRate  = 0.0;
  double totalLost = 0;
  double duration = 0.0;

  wallTime = getCurTimeD();
  endTime = wallTime;
  duration = endTime - startTime;

  if (numberRTTSamples > 0) {
     avgRTT = RTTSum /  (double)numberRTTSamples;
  } 

  totalLost =  (double)  numberOfTrials - numberRTTSamples;

  if (numberOfTrials >  0) {
    avgLossRate = totalLost / (double)numberOfTrials;
  }
//uint32_t TxErrorCount=0;
//uint32_t RxErrorCount=0;
//uint32_t largestSeqRecv = 0;
//uint32_t receivedCount = 0;
  
//uint32_t numberLost=0;
//uint32_t numberOutOfOrder=0;
//uint32_t numberTOs=0;
//incremented by the rx seq number - nest rx seq number 
  if (outputFile != NULL) {
    freopen("/dev/tty", "w", stdout); // resetting stdout to print back to the terminal
  }
  double avgActualSendRate = 0.0;
  if (opMode == 0) {
    printf("UDPEchoV2:Client:Summary:  %12.6f %6.6f %4.9f %2.4f %d %d %d %d %6.0f %d %d %d \n",
          wallTime, duration, avgRTT, avgLossRate, numberOfTrials, receivedCount, numberRTTSamples,numberTOs, totalLost,
             RxErrorCount, TxErrorCount, numberOutOfOrder);
  }
  else if (opMode == 1) {
    avgRTT = 0;
    avgActualSendRate = receivedCount / duration;
    printf("UDPEchoV2:Client:Summary:  %12.6f %6.6f %4.9f %2.4f %d %d %d %d %6.0f %d %d %d \n",
      wallTime, duration, avgRTT, avgActualSendRate, avgLossRate, numberOfTrials, receivedCount, numberRTTSamples,numberTOs, totalLost,
         RxErrorCount, TxErrorCount, numberOutOfOrder);
  }
  exit(0);
}

/*
  if (opMode == 1) {
    avgRTT = 0;
    print avgRTT and then immediately avgActualSendRate;
  }
  if (opMode == 0) {
    avgActualSendRate = 0;
  }
  HINT: When computing the avg rates, make sure when you divide the total number of bytes sent / received by the total time using the most accurate time that is possible. 
*/





