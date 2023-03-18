/*
 * main.c
 * 
 * Copyright 2023 Brian Murphy <gurudvlp@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mcast_listener.h"
#include "mcast_caster.h"
#include "proxy.h"

int main(int argc, char **argv)
{
    printf("mc-remotelan\n");
    printf("Copyright 2023 Brian Murphy\n\n");

    mclistener_init();
    mccaster_init();
    proxy_init();

    while(1)
    {
        //  Pop the next available message from the minecraft listener queue.
        mclistener_pop();

        //  Check if a message has been popped.
        if(mclistener_isMessageAvailable() == 1)
        {
            char * message = mclistener_getMessage();
            char * remoteAddr = mclistener_getRemoteAddr();

            proxy_sendToPeers(remoteAddr, message);
        }
        
        //  Pop the next available message from the proxy listener queue
        proxy_pop();

        //  Check if a message has actually been popped.
        if(proxy_isMessageAvailable() == 1)
        {
            char * message = proxy_getMessage();
            char * remoteAddr = proxy_getRemoteAddr();

            printf("Proxy In: %s %s\n", remoteAddr, message);

            mccaster_sendMessage(remoteAddr, message);
        }
        
        sleep(0);
    }

    exit(0);
}