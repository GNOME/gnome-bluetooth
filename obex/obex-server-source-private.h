/* portions of this file stolen from openobex, which is... */

/*     Copyright (c) 1999, 2000 Pontus Fuchs, All Rights Reserved.
 *     Copyright (c) 1998, 1999, 2000 Dag Brattli, All Rights Reserved.
 *
 *     This library is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU Lesser General Public
 *     License as published by the Free Software Foundation; either
 *     version 2 of the License, or (at your option) any later version.
 *
 *     This library is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *     Lesser General Public License for more details.
 *
 *     You should have received a copy of the GNU Lesser General Public
 *     License along with this library; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *     MA  02111-1307  USA
 */

#define MODE_SRV    0x80
#define MODE_CLI    0x00

#include <netinet/in.h>


enum
{
    STATE_IDLE,
    STATE_START,
    STATE_SEND,
    STATE_REC,
};

/* fake typedef for convenience */
typedef char GNetBuf;

struct obex {
    uint16_t mtu_tx;            /* Maximum OBEX TX packet size */
    uint16_t mtu_rx;            /* Maximum OBEX RX packet size */
    uint16_t mtu_tx_max;        /* Maximum TX we can accept */
                                                                                
    int fd;         /* Socket descriptor */
    int serverfd;
    int writefd;        /* write descriptor - only OBEX_TRANS_FD */
    unsigned int state;
     
    int keepserver;     /* Keep server alive */
    int filterhint;     /* Filter devices based on hint bits */
    int filterias;      /* Filter devices based on IAS entry */
 
    GNetBuf *tx_msg;        /* Reusable transmit message */
    GNetBuf *rx_msg;        /* Reusable receive message */
 
    obex_object_t   *object;    /* Current object being transfered */
    obex_event_t    eventcb;    /* Event-callback */

    /* snip: more follows this point but you're not knowing about it:
     * it turns out to vary depending on how libopenobex was compiled. */
};
