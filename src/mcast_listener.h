#ifndef MCLISTENER
    #define MCLISTENER 1
    #define MCLISTENER_MSG_BUF_SIZE 256
    #define MCLISTENER_PORT 4445
#endif

void mclistener_init();
void mclistener_pop();
unsigned char mclistener_isMessageAvailable();
char * mclistener_getMessage();
char * mclistener_getRemoteAddr();