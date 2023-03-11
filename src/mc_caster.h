#ifndef MCCASTER
    #define MCCASTER 1
    #define MCCASTER_MSG_BUF_SIZE 256
    #define MCCASTER_PORT 4445
#endif

void mccaster_init();
void mccaster_sendMessage(char * proxyAddr, char * message);