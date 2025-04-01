/*********************************************************
* Module Name:  UDPEchoV2 server
*
* File Name:    server.c	
*
* Summary:
*  This file contains the client portion of a client/server
*    UDP-based performance tool.
*  
* Usage:
*     server <service> 
*
* Output:
*  Per iteration output: 
*       printf("%f %d %d %d %d.%d %3.9f %3.9f\n", wallTime, (int32_t) numBytesRcvd,
*             largestSeqRecv,
*             msgHeaderPtr->sequenceNum,
*             msgHeaderPtr->timeSentSeconds,
*             msgHeaderPtr->timeSentNanoSeconds, OWDSample, smoothedOWD);
*
*  End of program summary info: 
*
*  printf("UDPEchoV2:Server:Summary:  %12.6f %6.6f %4.9f %2.4f %d %d %d %6.0f %d %d %d\n",
*        wallTime, duration, avgOWD, avgLossRate, numberOfTrials, receivedCount, largestSeqRecv, totalLost,
*        RxErrorCount, TxErrorCount, numberOutOfOrder);
*
*  
* A1: 3/12/2025:  Prepping to add support for opMode 1    CBR behavior....NO ECHO!
*                 Fixed iteration count off by 1,  cleaned up output a bit
*                 
*
* Last updated: 3/12/2025
*
*********************************************************/
#include "UDPEcho.h"
#include "AddressUtility.h"
#include "utils.h"

void CatchAlarm(int ignored);
void CNTCCode();

int sock = -1;                         /* Socket descriptor */
int bStop = 1;;
FILE *newFile = NULL;
double startTime = 0.0;
double endTime = 0.0;
double  wallTime = 0.0;
uint32_t largestSeqRecv = 0;
uint32_t receivedCount = 0;
uint32_t RxErrorCount = 0;
uint32_t TxErrorCount = 0;
uint32_t  numberOutOfOrder=0;
uint16_t opMode = 0;


double OWDSum = 0.0;
uint32_t numberOWDSamples=0;

//uncomment to see debug output
//#define TRACE 1

int main(int argc, char *argv[]) 
{

  char *buffer  = NULL;
  messageHeaderDefault msgHeader;
  messageHeaderDefault *msgHeaderPtr=&msgHeader;
  uint32_t *myBufferIntPtr  = NULL;
  uint32_t msgMinSize = (uint32_t) MESSAGEMIN;



//double OWDSum = 0.0;
//uint32_t numberOWDSamples=0;
  //most recent OWD sample
  double OWDSample = 0.0;
  //smoothed avg
  double smoothedOWD = 0.0;
  double alpha = 0.10;
  double sendTime = 0.0;

  //

  if (argc != 2) // Test for correct number of arguments
    DieWithUserMessage("Parameter(s)", "<Server Port/Service>");

  char *service = argv[1]; // First arg:  local port/service

  // Construct the server address structure
  struct addrinfo addrCriteria;                   // Criteria for address
  memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
  addrCriteria.ai_family = AF_UNSPEC;             // Any address family
  addrCriteria.ai_flags = AI_PASSIVE;             // Accept on any address/port
  addrCriteria.ai_socktype = SOCK_DGRAM;          // Only datagram socket
  addrCriteria.ai_protocol = IPPROTO_UDP;         // Only UDP socket

  struct addrinfo *servAddr; // List of server addresses
  int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
  if (rtnVal != 0)
    DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));

  // Display returned addresses
  struct addrinfo *addr = NULL;
  for (addr = servAddr; addr != NULL; addr = addr->ai_next) {
    PrintSocketAddress(addr->ai_addr, stdout);
    fputc('\n', stdout);
  }

  //Init memory for first send
  buffer = malloc((size_t)MAX_DATA_BUFFER);
  if (buffer == NULL) {
    printf("server: HARD ERROR malloc of  %d bytes failed \n", MAX_DATA_BUFFER);
    exit(1);
  }
  memset(buffer, 0, MAX_DATA_BUFFER);

  signal (SIGINT, CNTCCode);

  // Create socket for incoming connections
  int sock = socket(servAddr->ai_family, servAddr->ai_socktype,
      servAddr->ai_protocol);
  if (sock < 0)
    DieWithSystemMessage("socket() failed");

  // Bind to the local address
  if (bind(sock, servAddr->ai_addr, servAddr->ai_addrlen) < 0)
    DieWithSystemMessage("bind() failed");

  // Free address list allocated by getaddrinfo()
  freeaddrinfo(servAddr);

  wallTime = getCurTimeD();
  startTime = wallTime;
  for (;;) 
  { // Run forever
    struct sockaddr_storage clntAddr; // Client address
    // Set Length of client address structure (in-out parameter)
    socklen_t clntAddrLen = sizeof(clntAddr);

    // Block until receive message from a client
    // Size of received message
    ssize_t numBytesRcvd = recvfrom(sock, buffer, MAX_DATA_BUFFER, 0,
        (struct sockaddr *) &clntAddr, &clntAddrLen);
    if (numBytesRcvd < 0){
      RxErrorCount++;
      perror("server: Error on recvfrom ");
      continue;
    } else if (numBytesRcvd < msgMinSize) {
      RxErrorCount++;
      printf("server: Error on recvfrom, received (%d) less than MIN (%d) \n ", (int32_t)numBytesRcvd,msgMinSize);
      continue;
    } else 
    { 

      receivedCount++;
      wallTime = getCurTimeD();
      myBufferIntPtr  = (uint32_t *)buffer;
      //unpack to fill in the rx header info
      msgHeaderPtr->sequenceNum = ntohl(*myBufferIntPtr++);
      msgHeaderPtr->timeSentSeconds = ntohl(*myBufferIntPtr++);
      msgHeaderPtr->timeSentNanoSeconds = ntohl(*myBufferIntPtr++);
      msgHeaderPtr->opMode = ntohl(*myBufferIntPtr++);


      //Current wallclock time - packet send time
      sendTime =  ( (double)msgHeaderPtr->timeSentSeconds +  (((double)msgHeaderPtr->timeSentNanoSeconds)/1000000000.0) );
      OWDSample = wallTime - sendTime;
      OWDSum += OWDSample;
      numberOWDSamples++;
      smoothedOWD = (1-alpha)*smoothedOWD + alpha*OWDSample;
      opMode = msgHeaderPtr->opMode;

      if (msgHeaderPtr->sequenceNum > largestSeqRecv)
          largestSeqRecv = msgHeaderPtr->sequenceNum;

      printf("%f %d %d %d %d.%d %3.9f %3.9f\n", wallTime, (int32_t) numBytesRcvd,
             largestSeqRecv,
             msgHeaderPtr->sequenceNum,
             msgHeaderPtr->timeSentSeconds,
             msgHeaderPtr->timeSentNanoSeconds, OWDSample, smoothedOWD);
  
#ifdef TRACE 
      printf("server: Rx %d bytes from ", (int32_t) numBytesRcvd);
      fputs(" client ", stdout);
      PrintSocketAddress((struct sockaddr *) &clntAddr, stdout);
      fputc('\n', stdout);
#endif

      if (opMode == 0) {
        // Send received datagram back to the client
        ssize_t numBytesSent = sendto(sock, buffer, numBytesRcvd, 0,
          (struct sockaddr *) &clntAddr, sizeof(clntAddr));
        if (numBytesSent < 0) {
          TxErrorCount++;
          perror("server: Error on sendto ");
          continue;
        }
        else if (numBytesSent != numBytesRcvd) {
          TxErrorCount++;
          printf("server: Error on sendto, only sent %d rather than %d ",(int32_t)numBytesSent,(int32_t)numBytesRcvd);
          continue;
        }
      }
    }
  }
}

void CNTCCode() 
{
  double  duration = 0.0;
  double avgLossRate  = 0.0;
  double totalLost = 0;
  uint32_t numberOfTrials;
  double avgOWD = 0.0; 

  //estimate number of trials (only sender knows this for sure)
 //based on largest seq number seen
  numberOfTrials = largestSeqRecv;

  wallTime = getCurTimeD();
  endTime = wallTime;
  duration = endTime - startTime;

  if (numberOWDSamples > 0)
  {
    avgOWD = OWDSum / numberOWDSamples;
  } else {
  }

  if (numberOfTrials >= receivedCount)
    totalLost = numberOfTrials - receivedCount;
  else 
    totalLost = 0;

  if (numberOfTrials >  0) {
    avgLossRate = totalLost / (double)numberOfTrials;
  }

  //A1
  double avgObservedThroughput = 0.0;
  if (opMode == 0) {
    printf("UDPEchoV2:Server:Summary:  %12.6f %6.6f %4.9f %2.4f %d %d %d %6.0f %d %d %d\n",
        wallTime, duration, avgOWD, avgLossRate, numberOfTrials, receivedCount, largestSeqRecv, totalLost,
        RxErrorCount, TxErrorCount, numberOutOfOrder);
  }
  else if (opMode == 1) {
    avgObservedThroughput = receivedCount / duration;
    printf("UDPEchoV2:Server:Summary:  %12.6f %6.6f %4.9f %4.9f %2.4f %d %d %d %6.0f %d %d %d\n",
      wallTime, duration, avgOWD, avgObservedThroughput, avgLossRate, numberOfTrials, receivedCount, largestSeqRecv, totalLost,
      RxErrorCount, TxErrorCount, numberOutOfOrder);
    }
  /*
  if (opMode == 1) {
    print avgOWD and then immediately avgObservedThroughput;
  }
  if (opMode == 0) {
    avgObservedThroughput = 0;
  }
  */
        exit(0);
}



