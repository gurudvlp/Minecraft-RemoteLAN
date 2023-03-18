#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <time.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "proxy.h"

#define reverse_bytes_32(num) ( ((num & 0xFF000000) >> 24) | ((num & 0x00FF0000) >> 8) | ((num & 0x0000FF00) << 8) | ((num & 0x000000FF) << 24) )

//  Socket handle
int proxy_sockethnd;

//  Destination address
struct sockaddr_in proxy_addr;
unsigned int proxy_addrlen = sizeof(proxy_addr);
char proxy_remoteAddr[INET_ADDRSTRLEN];

char proxy_message[PROXY_MSG_BUF_SIZE];

char * proxy_peers[65535];
int proxy_peerSockets[65536];
unsigned short proxy_peerCount = 0;

void proxy_init()
{
    //  Load list of peers
    proxy_loadPeers();

    //  Create a socket
    proxy_sockethnd = socket(AF_INET, SOCK_DGRAM, 0);
    if(proxy_sockethnd < 0)
    {
        printf("Failed to create proxy listener.\n");
        exit(1);
    }

    unsigned int yes = 1;
    if(setsockopt(proxy_sockethnd, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0)
    {
        printf("Reusing socket ADDR failed (proxy).\n");
        exit(1);
    }

    //  Set up destination address
    memset(&proxy_addr, 0, sizeof(proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    proxy_addr.sin_port = htons(PROXY_PORT);

    //  Bind to the receiving address
    if(bind(proxy_sockethnd, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr)) < 0)
    {
        printf("Error binding to proxy socket.\n");
        exit(1);
    }
}

//  Send the supplied message to all known peers.
//  The sent format of the message will be:
//      ORIGIN IP|MESSAGE
//
//   >  10.0.0.21|[MOTD]Brian World![/MOTD][AD]39239[/AD]
void proxy_sendToPeers(char * originAddr, char * message)
{
    //  First thing, we need to make sure that this message did not originate
    //  from a peer.  If this message did originate from a peer, then relaying
    //  it back to that peer will cause that peer to send it back..  creating
    //  an infinite loop of relays.
    //
    //  We also check if the origin of this message is from a local subnet.  If
    //  it is not, we do NOT want to forward it.  It came from somewhere else.
    if(!proxy_isLocal(originAddr)) { return; }
    if(proxy_isPeer(originAddr)) { return; }
    

    char outgoingMessage[512];
    sprintf(&outgoingMessage[0], "%s|%s", originAddr, message);

    //  Loop through all known peers and forward this message
    int ep;
    for(ep = 0; ep < proxy_peerCount; ep++)
    {
        printf("Sending: %s: %s\n", proxy_peers[ep], outgoingMessage);

        int ep_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if(ep_socket < 0)
        {
            printf("Something went wrong with creating socket to send to peer.\n");
            printf("Error: %s\n", strerror(errno));
        }
        else
        {
            struct sockaddr_in ep_addr;
            memset(&ep_addr, 0, sizeof(ep_addr));
            ep_addr.sin_family = AF_INET;
            inet_pton(AF_INET, proxy_peers[ep], &(ep_addr.sin_addr));
            ep_addr.sin_port = htons(PROXY_PORT);

            int nbytes = sendto(
                ep_socket,
                outgoingMessage,
                strlen(outgoingMessage),
                0,
                (struct sockaddr*)&ep_addr,
                sizeof(ep_addr)
            );

            if(nbytes < 0)
            {
                printf("Failure relaying message to peer: %s\n", proxy_peers[ep]);
            }

            close(ep_socket);
        }
    }
    
}

//  Attempt to pop the next available incoming message from the queue.
void proxy_pop()
{
    //  First, we clear any previous messages.
    memset(&proxy_message, '\0', PROXY_MSG_BUF_SIZE);

    //  Now, check if there is any data ready to come in.
    int count;
    ioctl(proxy_sockethnd, FIONREAD, &count);
    if(count < 22)
    {
        //  There are too few bytes waiting to be received.  No messages yet.
        return;
    }

    int nbytes = recvfrom(
        proxy_sockethnd,
        proxy_message,
        PROXY_MSG_BUF_SIZE,
        0,
        (struct sockaddr *)&proxy_addr,
        &proxy_addrlen
    );

    if(nbytes < 0)
    {
        printf("Error receiving data from proxy socket.\n");
        exit(1);
    }

    if(nbytes < PROXY_MSG_BUF_SIZE)
    {   
        proxy_message[nbytes] = '\0';
    }
    else
    {
        proxy_message[PROXY_MSG_BUF_SIZE - 1] = '\0';
    }

    //  Parse out the remote machine's IP
    if(inet_ntop(AF_INET, &proxy_addr.sin_addr.s_addr, proxy_remoteAddr, sizeof(proxy_remoteAddr)) != NULL)
    {
        //  Don't do anything.  This worked.
    }
    else
    {
        //  Finding the address failed.  We need to set a default.
        memset(&proxy_remoteAddr, 'x', sizeof(proxy_remoteAddr));
        proxy_remoteAddr[INET_ADDRSTRLEN - 1] = '\0';
    }
}

//  Attempt to load the list of peers
void proxy_loadPeers()
{
    FILE * fh;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    fh = fopen("/etc/mc-remotelan/peers.list", "r");
    if(fh == NULL)
    {
        printf("No peers file was located.");
        printf("Please create /etc/mc-remotelan/peers.list");
        printf("Place one IP address per line.");

        return;
    }

    while((read = getline(&line, &len, fh)) != -1)
    {
        proxy_peers[proxy_peerCount] = (char *)malloc(INET_ADDRSTRLEN);
        strcpy(proxy_peers[proxy_peerCount], line);

        proxy_peers[proxy_peerCount][strcspn(proxy_peers[proxy_peerCount], "\n")] = 0;

        //  Initialize a socket to send data to this peer                                                               <<< Probably a good way to go
        /*proxy_peerSockets[proxy_peerCount] = socket(AF_INET, SOCK_DGRAM, 0);
        if(proxy_peerSockets[proxy_peerCount] < 0)
        {
            printf("Peer seems malformed: %s\n", proxy_peers[proxy_peerCount]);
        }
        else
        {
            //  Setup the destination address
        }*/

        proxy_peerCount++;
    }

}

bool proxy_isPeer(char * peerAddr)
{
    unsigned short ep = 0;
    
    for(ep = 0; ep < proxy_peerCount; ep++)
    {
        if(strcmp(proxy_peers[ep], peerAddr) == 0) { return true; }
    }

    return false;
}

bool proxy_isLocal(char * originAddr)
{
    struct ifaddrs * interfaces;
    struct ifaddrs * einterface;
    getifaddrs(&interfaces);

    struct sockaddr_in * sa;
    
    int32_t originNet = 0;
    inet_pton(AF_INET, originAddr, (void *)&originNet);
    originNet = reverse_bytes_32(originNet);

    char * addr;
    char * netmask;
    int32_t addrNet = 0;
    int32_t netmaskNet = 0;

    char strAddr[16];

    for(einterface = interfaces; einterface; einterface = einterface->ifa_next)
    {
        if(einterface->ifa_addr && einterface->ifa_addr->sa_family == AF_INET)
        {
            sa = (struct sockaddr_in *)einterface->ifa_addr;
            addr = inet_ntoa(sa->sin_addr);
            strcpy(strAddr, addr);
            inet_pton(AF_INET, strAddr, (void *)&addrNet);
            addrNet = reverse_bytes_32(addrNet);
            
            netmask = inet_ntoa(((struct sockaddr_in *)einterface->ifa_netmask)->sin_addr);
            inet_pton(AF_INET, netmask, (void *)&netmaskNet);
            netmaskNet = reverse_bytes_32(netmaskNet);

            if((addrNet & netmaskNet) == (originNet & netmaskNet))
            {
                freeifaddrs(interfaces);
                return true;
            }
            /*else
            {
                printf("? %u %u %u\n", addrNet, netmaskNet, originNet);
            }*/

            //printf("Addr: %s %s/%s\n", einterface->ifa_name, strAddr, netmask);
            //printf("\t%p %p\n", addr, netmask);
        }
    }

    freeifaddrs(interfaces);

    return false;
}

//  Check and return whether there is a message available.
unsigned char proxy_isMessageAvailable()
{
    if(proxy_message[0] == '\0') { return 0; }
    return 1;
}

char * proxy_getMessage()
{
    return proxy_message;
}

char * proxy_getRemoteAddr()
{
    return proxy_remoteAddr;
}