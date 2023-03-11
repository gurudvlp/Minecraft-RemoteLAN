//  This file was pretty heavily influenced by the guide at:
//
//      https://www.binarytides.com/raw-udp-sockets-c-linux/

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
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <errno.h>

#include "mc_caster.h"

unsigned short mccaster_checksum(unsigned short * ptr, int nbytes);

int mccaster_sockethnd;
char * MCCASTER_GROUP = "224.0.2.60";

//char mccaster_message[MCCASTER_MSG_BUF_SIZE];

//  Destination address
struct sockaddr_in mccaster_addr;
//unsigned int addrlen = sizeof(mccaster_addr);
//char mccaster_remoteAddr[INET_ADDRSTRLEN];

//  We need to set up a 96 bit header for checksum calculation
struct pseudo_header
{
    uint32_t source_addr;
    uint32_t destination_addr;
    uint8_t placeholder;
    uint8_t protocol;
    uint16_t udp_length;
};

void mccaster_init()
{
    mccaster_sockethnd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if(mccaster_sockethnd < 0)
    {
        printf("Failed to create raw socket.\n");
        printf("You may need to be root.\n");
        exit(1);
    }


}

//  Parse the message that has been passed to this function, and then
//  relay that message to the multicast group.
void mccaster_sendMessage(char * proxyAddr, char * message)
{
    char toBroadcast[255];

    //  We need to craft a datagram
    char datagram[4096];
    char source_ip[32];
    char * data;
    char * pseudogram;

    //  Parse the message that came in so that we can relay it
    int ec;
    for(ec = 0; ec < strlen(message); ec++)
    {
        if(message[ec] == '|')
        {
            //  Pipe is the delimiter.  No more copying IP
            source_ip[ec] = '\0';
            break;
        }
        else
        {
            source_ip[ec] = message[ec];
        }
    }

    strcpy(toBroadcast, &message[strlen(source_ip) + 1]);

    //  Zero out the packet buffer
    memset(datagram, 0, 4096);

    //  Create IP and UDP headers
    struct iphdr * ipHeader = (struct iphdr *)datagram;
    struct udphdr * udpHeader = (struct udphdr *)(datagram + sizeof(struct ip));

    struct sockaddr_in sin;
    struct pseudo_header psh;

    //  Data
    data = datagram + sizeof(struct iphdr) + sizeof(struct udphdr);
    strcpy(data, toBroadcast);

    //  Add spoofed IP address
    //strcpy(source_ip, "10.6.9.1");

    sin.sin_family = AF_INET;
    sin.sin_port = htons(4445);
    sin.sin_addr.s_addr = inet_addr(MCCASTER_GROUP);

    //  Fill in the IP Header
    ipHeader->ihl = 5;
    ipHeader->version = 4;
    ipHeader->tos = 0;
    ipHeader->tot_len = sizeof(struct iphdr) + sizeof(struct udphdr) + strlen(data);
    ipHeader->id = htonl(54321);
    ipHeader->frag_off = 0;
    ipHeader->ttl = 255;
    ipHeader->protocol = IPPROTO_UDP;
    ipHeader->check = 0;
    ipHeader->saddr = inet_addr(source_ip);
    ipHeader->daddr = sin.sin_addr.s_addr;

    //  Now create the checksum
    ipHeader->check = mccaster_checksum((unsigned short *)datagram, ipHeader->tot_len);

    //  Fill in the UDP header
    udpHeader->source = htons(42069);
    udpHeader->dest = htons(4445);
    udpHeader->len = htons(8 + strlen(data));
    udpHeader->check = 0;

    //  And create the UDP checksum using the pseudo header
    psh.source_addr = inet_addr(source_ip);
    psh.destination_addr = sin.sin_addr.s_addr;
    psh.placeholder = 0; 
    psh.protocol = IPPROTO_UDP;
    psh.udp_length = htons(sizeof(struct udphdr) + strlen(data));

    int psize = sizeof(struct pseudo_header) + sizeof(struct udphdr) + strlen(data);
    pseudogram = malloc(psize);

    memcpy(pseudogram, (char *)&psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), udpHeader, sizeof(struct udphdr) + strlen(data));

    udpHeader->check = mccaster_checksum((unsigned short *)pseudogram, psize);

    if(sendto(mccaster_sockethnd, datagram, ipHeader->tot_len, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        printf("Failed to send proxied multicast packet: %s\n", strerror(errno));
    }
}

unsigned short mccaster_checksum(unsigned short * ptr, int nbytes)
{
    register long sum;
    unsigned short oddByte;
    register short answer;

    sum = 0;

    while(nbytes > 1)
    {
        sum += *ptr++;
        nbytes -= 2;
    }

    if(nbytes == 1)
    {
        oddByte = 0;
        *((unsigned char *)&oddByte) = *(unsigned char *)ptr;
        sum += oddByte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;

    return answer;
}