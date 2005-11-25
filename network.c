/*
 * Layer Two Tunnelling Protocol Daemon
 * Copyright (C) 1998 Adtran, Inc.
 * Copyright (C) 2002 Jeff McAdams
 *
 * Mark Spencer
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * Network routines for UDP handling
 */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "l2tp.h"

char hostname[256];
struct sockaddr_in server, from;        /* Server and transmitter structs */
int server_socket;              /* Server socket */
#ifdef USE_KERNEL
int kernel_support;             /* Kernel Support there or not? */
#endif


int init_network (void)
{
    long arg;
    int length = sizeof (server);
    gethostname (hostname, sizeof (hostname));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = gconfig.listenaddr; 
    server.sin_port = htons (gconfig.port);
    if ((server_socket = socket (PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        l2tp_log (LOG_CRIT, "%s: Unable to allocate socket. Terminating.\n",
             __FUNCTION__);
        return -EINVAL;
    };
    /* L2TP/IPSec: Set up SA for listening port here?  NTB 20011015
     */
    if (bind (server_socket, (struct sockaddr *) &server, sizeof (server)))
    {
        close (server_socket);
        l2tp_log (LOG_CRIT, "%s: Unable to bind socket: %s. Terminating.\n",
             __FUNCTION__, strerror(errno), errno);
        return -EINVAL;
    };
    if (getsockname (server_socket, (struct sockaddr *) &server, &length))
    {
        l2tp_log (LOG_CRIT, "%s: Unable to read socket name.Terminating.\n",
             __FUNCTION__);
        return -EINVAL;
    }
#ifdef USE_KERNEL
    if (gconfig.forceuserspace)
    {
        l2tp_log (LOG_LOG, "Not looking for kernel support.\n");
        kernel_support = 0;
    }
    else
    {
        if (ioctl (server_socket, SIOCSETL2TP, NULL) < 0)
        {
            l2tp_log (LOG_LOG, "L2TP kernel support not detected.\n");
            kernel_support = 0;
        }
        else
        {
            l2tp_log (LOG_LOG, "Using l2tp kernel support.\n");
            kernel_support = -1;
        }
    }
#else
    l2tp_log (LOG_LOG, "This binary does not support kernel L2TP.\n");
#endif
    arg = fcntl (server_socket, F_GETFL);
    arg |= O_NONBLOCK;
    fcntl (server_socket, F_SETFL, arg);
    gconfig.port = ntohs (server.sin_port);
    return 0;
}

inline void extract (void *buf, int *tunnel, int *call)
{
    /*
     * Extract the tunnel and call #'s, and fix the order of the 
     * version
     */

    struct payload_hdr *p = (struct payload_hdr *) buf;
    if (PLBIT (p->ver))
    {
        *tunnel = p->tid;
        *call = p->cid;
    }
    else
    {
        *tunnel = p->length;
        *call = p->tid;
    }
}

inline void fix_hdr (void *buf)
{
    /*
     * Fix the byte order of the header
     */

    struct payload_hdr *p = (struct payload_hdr *) buf;
    _u16 ver = ntohs (p->ver);
    if (CTBIT (p->ver))
    {
        /*
         * Control headers are always
         * exactly 12 bytes big.
         */
        swaps (buf, 12);
    }
    else
    {
        int len = 6;
        if (PSBIT (ver))
            len += 4;
        if (PLBIT (ver))
            len += 2;
        if (PFBIT (ver))
            len += 4;
        swaps (buf, len);
    }
}

void dethrottle (void *call)
{
/*	struct call *c = (struct call *)call; */
/*	if (c->throttle) {
#ifdef DEBUG_FLOW
		log(LOG_DEBUG, "%s: dethrottling call %d, and setting R-bit\n",__FUNCTION__,c->ourcid); 
#endif 		c->rbit = RBIT;
		c->throttle = 0;
	} else {
		log(LOG_DEBUG, "%s:  call %d already dethrottled?\n",__FUNCTION__,c->ourcid); 	
	} */
}

void control_xmit (void *b)
{
    struct buffer *buf = (struct buffer *) b;
    struct tunnel *t;
    struct timeval tv;
    int ns;
    if (!buf)
    {
        l2tp_log (LOG_WARN, "%s: called on NULL buffer!\n", __FUNCTION__);
        return;
    }

    buf->retries++;
    t = buf->tunnel;
    ns = ntohs (((struct control_hdr *) (buf->start))->Ns);
    if (t)
    {
        if (ns < t->cLr)
        {
#ifdef DEBUG_CONTROL_XMIT
            l2tp_log (LOG_DEBUG, "%s: Tossing packet %d\n", __FUNCTION__, ns);
#endif
            /* Okay, it's been received.  Let's toss it now */
            toss (buf);
            return;
        }
    }
    if (buf->retries > DEFAULT_MAX_RETRIES)
    {
        /*
           * Too many retries.  Either kill the tunnel, or
           * if there is no tunnel, just stop retransmitting.
         */
        if (t)
        {
            if (t->self->needclose)
            {
                l2tp_log (LOG_DEBUG,
                     "Unable to deliver closing message for tunnel %d. Destroying anyway.\n",
                     t->ourtid);
                t->self->needclose = 0;
                t->self->closing = -1;
            }
            else
            {
                l2tp_log (LOG_NOTICE,
                     "Maximum retries exceeded for tunnel %d.  Closing.\n",
                     t->ourtid);
                strcpy (t->self->errormsg, "Timeout");
                t->self->needclose = -1;
            }
        }
	free(buf->rstart);
	free(buf);
    }
    else
    {
        /*
           * FIXME:  How about adaptive timeouts?
         */
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        schedule (tv, control_xmit, buf);
#ifdef DEBUG_CONTROL_XMIT
        l2tp_log (LOG_DEBUG, "%s: Scheduling and transmitting packet %d\n",
             __FUNCTION__, ns);
#endif
        udp_xmit (buf);
    }
}

void udp_xmit (struct buffer *buf)
{
    /*
     * Just send it all out.
     */
#if 0
    struct sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_port = buf->port;
    /* if (buf->retry>-1) buf->retry++; */
    bcopy (&buf->addr, &to.sin_addr, sizeof (buf->addr));
#endif
    sendto (server_socket, buf->start, buf->len, 0,
            (struct sockaddr *) &buf->peer, sizeof (buf->peer));

}

int build_fdset (fd_set *readfds)
{
	struct tunnel *tun;
	struct call *call;
	int max = 0;

	tun = tunnels.head;
	FD_ZERO (readfds);

	while (tun)
	{
		call = tun->call_head;
		while (call)
		{
			if (call->needclose ^ call->closing)
			{
				call_close (call);
				call = tun->call_head;
				if (!call)
					break;
				continue;
			}
			if (call->fd > -1)
			{
				if (!call->needclose && !call->closing)
				{
					if (call->fd > max)
						max = call->fd;
					FD_SET (call->fd, readfds);
				}
			}
			call = call->next;
		}
		/* Now that call fds have been collected, and checked for
		 * closing, check if the tunnel needs to be closed too
		 */
		if (tun->self->needclose ^ tun->self->closing)
		{
			if (gconfig.debug_tunnel)
				l2tp_log (LOG_DEBUG, "%s: closing down tunnel %d\n",
						__FUNCTION__, tun->ourtid);
			call_close (tun->self);
			/* Reset the while loop
			 * and check for NULL */
			tun = tunnels.head;
			if (!tun)
				break;
			continue;
		}
		tun = tun->next;
	}
	FD_SET (server_socket, readfds);
	if (server_socket > max)
		max = server_socket;
	FD_SET (control_fd, readfds);
	if (control_fd > max)
		max = control_fd;
	return max;
}

void network_thread ()
{
    /*
     * We loop forever waiting on either data from the ppp drivers or from
     * our network socket.  Control handling is no longer done here.
     */
    int fromlen;                /* Length of the address */
    int tunnel, call;           /* Tunnel and call */
    int recvsize;               /* Length of data received */
    struct buffer *buf;         /* Payload buffer */
    struct call *c, *sc;        /* Call to send this off to */
    struct tunnel *st;          /* Tunnel */
    fd_set readfds;             /* Descriptors to watch for reading */
    int max;                    /* Highest fd */
    struct timeval tv;          /* Timeout for select */
    /* This one buffer can be recycled for everything except control packets */
    buf = new_buf (MAX_RECV_SIZE);
    for (;;)
    {
        max = build_fdset (&readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        schedule_unlock ();
        select (max + 1, &readfds, NULL, NULL, NULL);
        schedule_lock ();
        if (FD_ISSET (control_fd, &readfds))
        {
            do_control ();
        }
        if (FD_ISSET (server_socket, &readfds))
        {
            /*
             * Okay, now we're ready for reading and processing new data.
             */
            recycle_buf (buf);
            /* Reserve space for expanding payload packet headers */
            buf->start += PAYLOAD_BUF;
            buf->len -= PAYLOAD_BUF;
            fromlen = sizeof (from);
            recvsize =
                recvfrom (server_socket, buf->start, buf->len, 0,
                          (struct sockaddr *) &from, &fromlen);
            if (recvsize < MIN_PAYLOAD_HDR_LEN)
            {
                if (recvsize < 0)
                {
                    if (errno != EAGAIN)
                        l2tp_log (LOG_WARN,
                             "%s: recvfrom returned error %d (%s)\n",
                             __FUNCTION__, errno, strerror (errno));
                }
                else
                {
                    l2tp_log (LOG_WARN, "%s: received too small a packet\n",
                         __FUNCTION__);
                }
            }
            else
            {
                buf->len = recvsize;
                if (gconfig.debug_network)
                {
                    l2tp_log (LOG_DEBUG, "%s: recv packet from %s, size = %d, "
							"tunnel = %d, call = %d\n", __FUNCTION__,
							inet_ntoa (from.sin_addr), recvsize, tunnel, call);
                }
                if (gconfig.packet_dump)
                {
                    do_packet_dump (buf);
                }
                fix_hdr (buf->start);
                extract (buf->start, &tunnel, &call);
                if (!
                    (c =
                     get_call (tunnel, call, from.sin_addr.s_addr,
                               from.sin_port)))
                {
                    if ((c =
                         get_tunnel (tunnel, from.sin_addr.s_addr,
                                     from.sin_port)))
                    {
                        /*
                         * It is theoretically possible that we could be sent
                         * a control message (say a StopCCN) on a call that we
                         * have already closed or some such nonsense.  To prevent
                         * this from closing the tunnel, if we get a call on a valid
                         * tunnel, but not with a valid CID, we'll just send a ZLB
                         * to ack receiving the packet.
                         */
                        if (gconfig.debug_tunnel)
                            l2tp_log (LOG_DEBUG,
                                 "%s: no such call %d on tunnel %d.  Sending special ZLB\n",
                                 __FUNCTION__);
                        handle_special (buf, c, call);
                    }
                    else
                        l2tp_log (LOG_DEBUG,
                             "%s: unable to find call or tunnel to handle packet.  call = %d, tunnel = %d Dumping.\n",
                             __FUNCTION__, call, tunnel);

                }
                else
                {
                    buf->peer = from;
                    /* Handle the packet */
                    c->container->chal_us.vector = NULL;
                    if (handle_packet (buf, c->container, c))
                    {
                        if (gconfig.debug_tunnel)
                            l2tp_log (LOG_DEBUG, "%s: bad packet\n", __FUNCTION__);
                    };
                    if (c->cnu)
                    {
                        /* Send Zero Byte Packet */
                        control_zlb (buf, c->container, c);
                        c->cnu = 0;
                    }
                }
            }
        };

        st = tunnels.head;
        while (st)
        {
            sc = st->call_head;
            while (sc)
            {
                if ((sc->fd >= 0) && FD_ISSET (sc->fd, &readfds))
                {
                    /* Got some payload to send */
                    int result;
                    recycle_payload (buf, sc->container->peer);
#ifdef DEBUG_FLOW_MORE
                    l2tp_log (LOG_DEBUG, "%s: rws = %d, pSs = %d, pLr = %d\n",
                         __FUNCTION__, sc->rws, sc->pSs, sc->pLr);
#endif
/*					if ((sc->rws>0) && (sc->pSs > sc->pLr + sc->rws) && !sc->rbit) {
#ifdef DEBUG_FLOW
						log(LOG_DEBUG, "%s: throttling payload (call = %d, tunnel = %d, Lr = %d, Ss = %d, rws = %d)!\n",__FUNCTION__,
								 sc->cid, sc->container->tid, sc->pLr, sc->pSs, sc->rws); 
#endif
						sc->throttle = -1;
						We unthrottle in handle_packet if we get a payload packet, 
						valid or ZLB, but we also schedule a dethrottle in which
						case the R-bit will be set
						FIXME: Rate Adaptive timeout? 						
						tv.tv_sec = 2;
						tv.tv_usec = 0;
						sc->dethrottle = schedule(tv, dethrottle, sc); 					
					} else */
/*					while ((result=read_packet(buf,sc->fd,sc->frame & SYNC_FRAMING))>0) { */
                    while ((result =
                            read_packet (buf, sc->fd, SYNC_FRAMING)) > 0)
                    {
                        add_payload_hdr (sc->container, sc, buf);
                        if (gconfig.packet_dump)
                        {
                            do_packet_dump (buf);
                        }


                        sc->prx = sc->data_rec_seq_num;
                        if (sc->zlb_xmit)
                        {
                            deschedule (sc->zlb_xmit);
                            sc->zlb_xmit = NULL;
                        }
                        sc->tx_bytes += buf->len;
                        sc->tx_pkts++;
                        udp_xmit (buf);
                        recycle_payload (buf, sc->container->peer);
                    }
                    if (result != 0)
                    {
                        l2tp_log (LOG_WARN,
                             "%s: tossing read packet, error = %s (%d).  Closing call.\n",
                             __FUNCTION__, strerror (-result), -result);
                        strcpy (sc->errormsg, strerror (-result));
                        sc->needclose = -1;
                    }
                }
                sc = sc->next;
            }
            st = st->next;
        }
    }

}
