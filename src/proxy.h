#include <stdbool.h>

#ifndef PROXY
    #define PROXY 1
    #define PROXY_MSG_BUF_SIZE 512
    #define PROXY_PORT 11212
#endif

void proxy_init();
void proxy_pop();
void proxy_sendToPeers(char * originAddr, char * message);
unsigned char proxy_isMessageAvailable();
char * proxy_getMessage();
char * proxy_getRemoteAddr();
void proxy_loadPeers();
bool proxy_isPeer( char * peerAddr);