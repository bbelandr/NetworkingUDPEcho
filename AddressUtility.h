/************************************************************************
* File:  AddressUtils.h
*
* Purpose:
*
* Notes:
*
************************************************************************/
#ifndef	__AddressUtils_h
#define	__AddressUtils_h

// Print socket address
void PrintSocketAddress(const struct sockaddr *address, FILE *stream);
int findAF_INET6SocketAddress(struct addrinfo *servAddr, struct sockaddr **callersaddresss);
int findAF_INETSocketAddress(struct addrinfo *servAddr, struct sockaddr **callersaddresss); 

// Test socket address equality
bool SockAddrsEqual(const struct sockaddr *addr1, const struct sockaddr *addr2);


#endif


