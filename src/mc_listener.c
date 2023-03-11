#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/ioctl.h>

#include "mc_listener.h"

int sockethnd;
char * MCLISTENER_GROUP = "224.0.2.60";

char mclistener_message[MCLISTENER_MSG_BUF_SIZE];

//  Destination address
struct sockaddr_in addr;
unsigned int addrlen = sizeof(addr);
char remoteAddr[INET_ADDRSTRLEN];

void mclistener_init()
{
    //  Create a socket
    sockethnd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockethnd < 0)
    {
        printf("Failed to create multicast listener.\n");
        exit(1);
    }

    unsigned int yes = 1;
    if(setsockopt(sockethnd, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0)
    {
        printf("Reusing socket ADDR failed.\n");
        exit(1);
    }

    //  Set up destination address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(MCLISTENER_PORT);

    //  Bind to the receiving address
    if(bind(sockethnd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        printf("Error binding to socket.\n");
        exit(1);
    }

    //  Request that the kernel join a multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MCLISTENER_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if(setsockopt(sockethnd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
    {
        printf("Error joiningt multicast group.\n");
        exit(1);
    }


}

//  Attempt to pop the most recently received LAN advertisement.
void mclistener_pop()
{
    //  First, we clear any previous messages.
    memset(&mclistener_message, '\0', MCLISTENER_MSG_BUF_SIZE);

    //  Now, check if there is any data ready to come in.
    int count;
    ioctl(sockethnd, FIONREAD, &count);
    if(count < 22)
    {
        //  There are too few bytes waiting to be received.  No messages yet.
        return;
    }

    int nbytes = recvfrom(
        sockethnd,
        mclistener_message,
        MCLISTENER_MSG_BUF_SIZE,
        0,
        (struct sockaddr *)&addr,
        &addrlen
    );

    if(nbytes < 0)
    {
        printf("Error receiving data from MCLISTENER socket.\n");
        exit(1);
    }

    if(nbytes < MCLISTENER_MSG_BUF_SIZE)
    {   
        mclistener_message[nbytes] = '\0';
    }
    else
    {
        mclistener_message[MCLISTENER_MSG_BUF_SIZE - 1] = '\0';
    }

    //  Parse out the remote machine's IP
    if(inet_ntop(AF_INET, &addr.sin_addr.s_addr, remoteAddr, sizeof(remoteAddr)) != NULL)
    {
        //  Don't do anything.  This worked.
    }
    else
    {
        //  Finding the address failed.  We need to set a default.
        memset(&remoteAddr, 'x', sizeof(remoteAddr));
        remoteAddr[INET_ADDRSTRLEN - 1] = '\0';
    }

    
}

//  Check and return whether there is a message available.
unsigned char mclistener_isMessageAvailable()
{
    if(mclistener_message[0] == '\0') { return 0; }
    return 1;
}

char * mclistener_getMessage()
{
    return mclistener_message;
}

char * mclistener_getRemoteAddr()
{
    return remoteAddr;
}