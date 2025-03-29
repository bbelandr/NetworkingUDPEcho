#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "UDPEcho.h"

#define TRACE 1

/*************************************************************
*
* Function: void PrintSocketAddress(const struct sockaddr *address, FILE *stream) {
* 
* Summary: Prints address/name associated with the address - sends to stream handle
*
* Inputs:
*   const struct sockaddr *address : reference to the address to print
*   FILE *stream
*           
*
* outputs:  
*     updates caller's object data
*
* notes: 
*    
*   Timeval struct defines the two components as long ints
*         The following nicely printers: 
*         printf("%ld.%06ld\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
*
***************************************************************/
void PrintSocketAddress(const struct sockaddr *address, FILE *stream) {
  // Test for address and stream
  if (address == NULL || stream == NULL)
    return;

  void *numericAddress; // Pointer to binary address
  // Buffer to contain result (IPv6 sufficient to hold IPv4)
  char addrBuffer[INET6_ADDRSTRLEN];
  in_port_t port; // Port to print
  // Set pointer to address based on address family
  switch (address->sa_family) {
  case AF_INET:
    numericAddress = &((struct sockaddr_in *) address)->sin_addr;
    port = ntohs(((struct sockaddr_in *) address)->sin_port);
    break;
  case AF_INET6:
    numericAddress = &((struct sockaddr_in6 *) address)->sin6_addr;
    port = ntohs(((struct sockaddr_in6 *) address)->sin6_port);
    break;
  default:
    fputs("[unknown type]", stream);    // Unhandled type
    return;
  }
  // Convert binary to printable address
  if (inet_ntop(address->sa_family, numericAddress, addrBuffer,
      sizeof(addrBuffer)) == NULL)
    fputs("[invalid address]", stream); // Unable to convert
  else {
    fprintf(stream, "%s", addrBuffer);
    if (port != 0)                // Zero not valid in any socket addr
      fprintf(stream, "-%u", port);
  }
}

/*************************************************************
*
* Function: int findAF_INET6SocketAddress(struct addrinfo *,
*                   struct sockaddr **callersaddress) 
* 
* Summary:  attempts to find an IP V6 addr in the list.  Returns ERROR if can not find one.
*            else fills in the caller's addr ptr with the address and returns NOERROR.
*
* Inputs:
*   struct addrinfo *  : reference to callers addrinfo  
*   struct sockaddr **callersaddress : reference to the callers address ptr that is to be filled in
*           
*
* outputs:  
*     updates callersaddress if an IPV6 addr is found in the list. 
*     returns the number of IPV6 address that are found.  0 means none found.
*     If > 1, the callersaddress is filled in with the first
*
* notes: 
*    
*
***************************************************************/
int findAF_INET6SocketAddress(struct addrinfo *servAddr, struct sockaddr **callersaddress) 
{
  struct addrinfo *addr = NULL;
  uint32_t count =0;


  for (addr = servAddr; addr != NULL; addr = addr->ai_next) {
    if (addr->ai_addr->sa_family == AF_INET6) {
      if (count == 0)
        *callersaddress = addr->ai_addr;
      count++;
    }
  }

#ifdef TRACE
  printf("findAF_INET6SocketAddress: Found %d V6 addresses \n", count);
#endif
  return count;
}

/*************************************************************
*
* Function: int findAF_INETSocketAddress(const struct sockaddr *address, struct sockaddr **callersaddress) 
* 
* Summary:  attempts to find an IP V4 addr in the list.  Returns ERROR if can not find one.
*            else fills in the caller's addr ptr with the first V4 addr in the list
*           and returns NOERROR.
*
* Inputs:
*   const struct sockaddr *address : reference to address of interest 
*   struct sockaddr **callersaddress : reference to the callers address ptr that is to be filled in
*           
*
* outputs:  
*     updates callersaddress if an IPV4 addr is found. Returns the number of
*        IPV4 addresses found. A 0 means none found. 
*
* notes: 
*    
*
***************************************************************/
int findAF_INETSocketAddress(struct addrinfo *servAddr, struct sockaddr **callersaddress) 
{
  struct addrinfo *addr = NULL;
  uint32_t count =0;

  for (addr = servAddr; addr != NULL; addr = addr->ai_next) {
    if (addr->ai_addr->sa_family == AF_INET) {
      if (count == 0)
        *callersaddress = addr->ai_addr;
      count++;
    }
  }

#ifdef TRACE
  printf("findAF_INETSocketAddress: Found %d V4 addresses \n", count);
#endif

  return count;
}



bool SockAddrsEqual(const struct sockaddr *addr1, const struct sockaddr *addr2) {
  if (addr1 == NULL || addr2 == NULL)
    return addr1 == addr2;
  else if (addr1->sa_family != addr2->sa_family)
    return false;
  else if (addr1->sa_family == AF_INET) {
    struct sockaddr_in *ipv4Addr1 = (struct sockaddr_in *) addr1;
    struct sockaddr_in *ipv4Addr2 = (struct sockaddr_in *) addr2;
    return ipv4Addr1->sin_addr.s_addr == ipv4Addr2->sin_addr.s_addr
        && ipv4Addr1->sin_port == ipv4Addr2->sin_port;
  } else if (addr1->sa_family == AF_INET6) {
    struct sockaddr_in6 *ipv6Addr1 = (struct sockaddr_in6 *) addr1;
    struct sockaddr_in6 *ipv6Addr2 = (struct sockaddr_in6 *) addr2;
    return memcmp(&ipv6Addr1->sin6_addr, &ipv6Addr2->sin6_addr,
        sizeof(struct in6_addr)) == 0 && ipv6Addr1->sin6_port
        == ipv6Addr2->sin6_port;
  } else
    return false;
}
