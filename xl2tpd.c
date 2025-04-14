/*
 * Layer Two Tunnelling Protocol Daemon
 * Copyright (C) 1998 Adtran, Inc.
 * Copyright (C) 2002 Jeff McAdams
 *
 *
 * Mark Spencer
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * Main Daemon source.
 *
 */

#define _ISOC99_SOURCE
#define _XOPEN_SOURCE
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE_EXTENDED	1
#define _GNU_SOURCE

#include <stdlib.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#ifndef LINUX
# include <sys/socket.h>
#endif
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "l2tp.h"

struct tunnel_list tunnels;
int rand_source;
int ppd = 1;                    /* Packet processing delay */
int control_fd;                 /* descriptor of control area */

static char *dial_no_tmp;              /* jz: Dialnumber for Outgoing Call */
int switch_io = 0;              /* jz: Switch for Incoming or Outgoing Call */

static void open_controlfd(void);

static volatile sig_atomic_t sigterm_received;
static volatile sig_atomic_t sigint_received;
static volatile sig_atomic_t sigchld_received;
static volatile sig_atomic_t sigusr1_received;;
static volatile sig_atomic_t sighup_received;

struct control_requests_handler {
    char type;
    int (*handler) (FILE* resf, char* bufp);
};

static int control_handle_available(FILE* resf, char* bufp);
static int control_handle_lns_add_modify(FILE* resf, char* bufp);
static int control_handle_lns_status(FILE* resf, char* bufp);
static int control_handle_tunnel(FILE* respf, char* bufp);
static int control_handle_lac_connect(FILE* resf, char* bufp);
static int control_handle_lac_outgoing_call(FILE* resf, char* bufp);
static int control_handle_lac_hangup(FILE* resf, char* bufp);
static int control_handle_lac_disconnect(FILE* resf, char* bufp);
static int control_handle_lac_add_modify(FILE* resf, char* bufp);
static int control_handle_lac_remove(FILE* resf, char* bufp);
static int control_handle_lac_status();
static int control_handle_lns_remove(FILE* resf, char* bufp);

static struct control_requests_handler control_handlers[] = {
    {CONTROL_PIPE_REQ_AVAILABLE, &control_handle_available},
    {CONTROL_PIPE_REQ_LNS_ADD_MODIFY, &control_handle_lns_add_modify},
    {CONTROL_PIPE_REQ_LNS_STATUS, &control_handle_lns_status},
    {CONTROL_PIPE_REQ_TUNNEL, &control_handle_tunnel},
    {CONTROL_PIPE_REQ_LAC_CONNECT, &control_handle_lac_connect},
    {CONTROL_PIPE_REQ_LAC_OUTGOING_CALL, &control_handle_lac_outgoing_call},
    {CONTROL_PIPE_REQ_LAC_HANGUP, &control_handle_lac_hangup},
    {CONTROL_PIPE_REQ_LAC_DISCONNECT, &control_handle_lac_disconnect},
    {CONTROL_PIPE_REQ_LAC_ADD_MODIFY, &control_handle_lac_add_modify},
    {CONTROL_PIPE_REQ_LAC_REMOVE, &control_handle_lac_remove},
    {CONTROL_PIPE_REQ_LAC_STATUS, &control_handle_lac_status},
    {CONTROL_PIPE_REQ_LNS_REMOVE, &control_handle_lns_remove},

    {0, NULL}
};

static void init_tunnel_list (struct tunnel_list *t)
{
    t->head = NULL;
    t->count = 0;
    t->calls = 0;
}

/* Now sends to syslog instead - MvO */
static void show_status (void)
{
    struct schedule_entry *se;
    struct tunnel *t;
    struct call *c;
    struct lns *tlns;
    struct lac *tlac;
    struct host *h;
    unsigned long cnt = 0;

    int s = 0;
    l2tp_log (LOG_WARNING, "====== xl2tpd statistics ========\n");
    l2tp_log (LOG_WARNING, " Scheduler entries:\n");
    se = events;
    while (se)
    {
        s++;
        t = (struct tunnel *) se->data;
        tlac = (struct lac *) se->data;
        c = (struct call *) se->data;
        if (se->func == &hello)
        {
            l2tp_log (LOG_WARNING, "%d: HELLO to %d\n", s, t->tid);
        }
        else if (se->func == &magic_lac_dial)
        {
            l2tp_log (LOG_WARNING, "%d: Magic dial on %s\n", s, tlac->entname);
        }
        else if (se->func == &send_zlb)
        {
            l2tp_log (LOG_WARNING, "%d: Send payload ZLB on call %d:%d\n", s,
                     c->container->tid, c->cid);
        }
        else if (se->func == &dethrottle)
        {
            l2tp_log (LOG_WARNING, "%d: Dethrottle call %d:%d\n", s, c->container->tid,
                     c->cid);
        }
        else if (se->func == &control_xmit)
        {
            l2tp_log (LOG_WARNING, "%d: Control xmit on %d\n", s,((struct buffer *)se->data)->tunnel->tid);
        }
        else
            l2tp_log (LOG_WARNING, "%d: Unknown event\n", s);
        se = se->next;
    };
    l2tp_log (LOG_WARNING, "Total Events scheduled: %d\n", s);
    l2tp_log (LOG_WARNING, "Number of tunnels open: %d\n", tunnels.count);
    t = tunnels.head;
    while (t)
    {
        l2tp_log (LOG_WARNING, "Tunnel %s, ID = %d (local), %d (remote) to %s:%d,"
                 " control_seq_num = %d, control_rec_seq_num = %d,"
                 " cLr = %d, call count = %d ref=%u/refhim=%u",
                 (t->lac ? t->lac->entname : (t->lns ? t->lns->entname : "")),
                 t->ourtid, t->tid, IPADDY (t->peer.sin_addr),
                 ntohs (t->peer.sin_port), t->control_seq_num,
                  t->control_rec_seq_num, t->cLr, t->count,
                  t->refme, t->refhim);
        c = t->call_head;
        while (c)
        {
            cnt++;
            l2tp_log (LOG_WARNING,
                     "Call %s # %lu, ID = %d (local), %d (remote), serno = %u,"
                     " data_seq_num = %d, data_rec_seq_num = %d,"
                     " pLr = %d, tx = %u bytes (%u), rx= %u bytes (%u)",
                     (c->lac ? c->lac->
                      entname : (c->lns ? c->lns->entname : "")),
                     cnt, c->ourcid,
                     c->cid, c->serno, c->data_seq_num, c->data_rec_seq_num,
                     c->pLr, c->tx_bytes, c->tx_pkts, c->rx_bytes, c->rx_pkts);
            c = c->next;
        }
        t = t->next;
    }
    l2tp_log (LOG_WARNING, "==========Config File===========\n");
    tlns = lnslist;
    while (tlns)
    {
        l2tp_log (LOG_WARNING, "LNS entry %s\n",
                 tlns->entname[0] ? tlns->entname : "(unnamed)");
        tlns = tlns->next;
    };
    tlac = laclist;
    while (tlac)
    {
        l2tp_log (LOG_WARNING, "LAC entry %s, LNS is/are:",
                 tlac->entname[0] ? tlac->entname : "(unnamed)");
        h = tlac->lns;
        if (h)
        {
            while (h)
            {
                l2tp_log (LOG_WARNING, " %s", h->hostname);
                h = h->next;
            }
        }
        else
            l2tp_log (LOG_WARNING, " [none]");
        tlac = tlac->next;
    };
    l2tp_log (LOG_WARNING, "================================\n");
}

static void null_handler(int sig)
{
	UNUSED(sig);
       /* FIXME
        * A sighup is received when a call is terminated, unknown origine ..
        * I catch it and ll looks good, but ..
        */
}

static void status_handler (int sig)
{
    UNUSED(sig);
    show_status ();
}

static void child_handler (int sig)
{
    UNUSED(sig);
    /*
     * Oops, somebody we launched was killed.
     * It's time to reap them and close that call.
     * But first, we have to find out what PID died.
     * unfortunately, pppd will
     */
    struct tunnel *t;
    struct call *c;
    pid_t pid;
    int status;
    /* Keep looping until all are cleared */
    for(;;)
    {
        pid = waitpid (-1, &status, WNOHANG);
        if (pid < 1)
        {
            /*
             * Oh well, nobody there.  Maybe we reaped it
             * somewhere else already
             */
            return;
        }
        /* find the call that "owned" the pppd which just died */
        t = tunnels.head;
        while (t)
        {
            c = t->call_head;
            t = t->next;
            while (c)
            {
                if (c->pppd == pid)
                {
                    /* pid is no longer valid, avoid killing it later by accident in destroy_call() */
                    c->pppd = 0;

                    if ( WIFEXITED( status ) )
                    {
                        l2tp_log (LOG_DEBUG, "%s : pppd exited for call %d with code %d\n", __FUNCTION__,
                         c->cid, WEXITSTATUS( status ) );
                    }
                    else if( WIFSIGNALED( status ) )
                    {
                        l2tp_log (LOG_DEBUG, "%s : pppd terminated for call %d by signal %d\n", __FUNCTION__,
                         c->cid, WTERMSIG( status ) );
                    }
                    else
                    {
                        l2tp_log (LOG_DEBUG, "%s : pppd exited for call %d for unknown reason\n", __FUNCTION__,
                         c->cid );
                    }
                    c->needclose = -1;
                    /*
                     * OK...pppd died, we can go ahead and close the pty for
                     * it
                     */
#ifdef USE_KERNEL
                 if (!kernel_support) {

#endif
                    close (c->fd);
#ifdef USE_KERNEL
                 } else {
                     call_close (c);
                 }
#endif
                    c->fd = -1;
                    /*
                     * terminate tunnel and call loops, returning to the
                     * for(;;) loop (and possibly get the next pid)
                     */
                    t = NULL;
                    break;
                }
                c = c->next;
            }
        }
    }
}

static void death_handler (int signal)
{
    /*
       * If we get here, somebody terminated us with a kill or a control-c.
       * we call call_close on each tunnel twice to get a StopCCN out
       * for each one (we can't pause to make sure it's received.
       * Then we close the connections
     */
    struct tunnel *st, *st2;
    int sec;
    l2tp_log (LOG_CRIT, "%s: Fatal signal %d received\n", __FUNCTION__, signal);
#ifdef USE_KERNEL
        if (kernel_support || signal != SIGTERM) {
#else
        if (signal != SIGTERM) {
#endif
                st = tunnels.head;
                while (st)
                {
                        st2 = st->next;
                        strcpy (st->self->errormsg, "Server closing");
                        sec = st->self->closing;
                        if (st->lac)
                                st->lac->redial = 0;
                        call_close (st->self);
                        if (!sec)
                        {
                                st->self->closing = -1;
                                call_close (st->self);
                        }
                        st = st2;
                }
        }

    /* erase pid and control files */
    unlink (gconfig.pidfile);
    unlink (gconfig.controlfile);
    free(dial_no_tmp);
    close(server_socket);
    close(control_fd);
    closelog();

    exit (1);
}

static void sigterm_handler(int sig)
{
    UNUSED(sig);
    sigterm_received = 1;
}

static void sigint_handler(int sig)
{
    UNUSED(sig);
    sigint_received = 1;
}

static void sigchld_handler(int sig)
{
    UNUSED(sig);
    sigchld_received = 1;
}

static void sigusr1_handler(int sig)
{
    UNUSED(sig);
    sigusr1_received = 1;
}

static void sighup_handler(int sig)
{
    UNUSED(sig);
    sighup_received = 1;
}

void process_signal(void)
{
    if (sigterm_received) { sigterm_received = 0; death_handler(SIGTERM); }
    if (sigint_received) { sigint_received = 0; death_handler(SIGINT); }
    if (sigchld_received) { sigchld_received = 0; child_handler(SIGCHLD); }
    if (sigusr1_received) { sigusr1_received = 0; status_handler(SIGUSR1); }
    if (sighup_received) { sighup_received = 0; null_handler(SIGHUP); }
}

int start_pppd (struct call *c, struct ppp_opts *opts)
{
    /* char a, b; */
    char tty[512];
    char *stropt[80];
#ifdef USE_KERNEL
    struct sockaddr_pppol2tp sax;
    int flags;
#endif
    int pos = 1;
    int fd2 = -1;
#ifdef DEBUG_PPPD
    int x;
#endif
    struct termios ptyconf;
    struct call *sc;
    struct tunnel *st;

    stropt[0] = strdup (PPPD);
    if (c->pppd > 0)
    {
        l2tp_log(LOG_WARNING, "%s: PPP already started on call!\n", __FUNCTION__);
        return -EINVAL;
    }
    if (c->fd > -1)
    {
        l2tp_log (LOG_WARNING, "%s: file descriptor already assigned!\n",
             __FUNCTION__);
        return -EINVAL;
    }

#ifdef USE_KERNEL
    if (kernel_support)
    {
       fd2 = socket(AF_PPPOX, SOCK_DGRAM, PX_PROTO_OL2TP);
       if (fd2 < 0) {
           l2tp_log (LOG_WARNING, "%s: Unable to allocate PPPoL2TP socket.\n",
                __FUNCTION__);
           return -EINVAL;
       }
       flags = fcntl(fd2, F_GETFL);
       if (flags == -1 || fcntl(fd2, F_SETFL, flags | O_NONBLOCK) == -1) {
           l2tp_log (LOG_WARNING, "%s: Unable to set PPPoL2TP socket nonblock.\n",
                __FUNCTION__);
           return -EINVAL;
       }
       memset(&sax, 0, sizeof(sax));
       sax.sa_family = AF_PPPOX;
       sax.sa_protocol = PX_PROTO_OL2TP;
       sax.pppol2tp.fd = c->container->udp_fd;
       sax.pppol2tp.addr.sin_addr.s_addr = c->container->peer.sin_addr.s_addr;
       sax.pppol2tp.addr.sin_port = c->container->peer.sin_port;
       sax.pppol2tp.addr.sin_family = AF_INET;
       sax.pppol2tp.s_tunnel  = c->container->ourtid;
       sax.pppol2tp.s_session = c->ourcid;
       sax.pppol2tp.d_tunnel  = c->container->tid;
       sax.pppol2tp.d_session = c->cid;
       if (connect(fd2, (struct sockaddr *)&sax, sizeof(sax)) < 0) {
           l2tp_log (LOG_WARNING, "%s: Unable to connect PPPoL2TP socket.\n",
                __FUNCTION__);
           close(fd2);
           return -EINVAL;
       }
       stropt[pos++] = strdup ("plugin");
       stropt[pos++] = strdup ("pppol2tp.so");
       stropt[pos++] = strdup ("pppol2tp");
       stropt[pos] = malloc (11);
       snprintf (stropt[pos], 11, "%d", fd2);
        pos++;
       if (c->container->lns) {
        stropt[pos++] = strdup ("pppol2tp_lns_mode");
        stropt[pos++] = strdup ("pppol2tp_tunnel_id");
        stropt[pos] = malloc (11);
        snprintf (stropt[pos], 11, "%d", c->container->ourtid);
            pos++;
        stropt[pos++] = strdup ("pppol2tp_session_id");
        stropt[pos] = malloc (11);
        snprintf (stropt[pos], 11, "%d", c->ourcid);
            pos++;
       }
    }
    else
#endif
    {
        if ((c->fd = getPtyMaster (tty, sizeof(tty))) < 0)
        {
            l2tp_log (LOG_WARNING, "%s: unable to allocate pty, abandoning!\n",
                      __FUNCTION__);
            return -EINVAL;
        }

        /* set fd opened above to not echo so we don't see read our own packets
           back of the file descriptor that we just wrote them to */
        tcgetattr (c->fd, &ptyconf);
        *(c->oldptyconf) = ptyconf;
        ptyconf.c_cflag &= ~(ICANON | ECHO);
        ptyconf.c_lflag &= ~ECHO;
        tcsetattr (c->fd, TCSANOW, &ptyconf);
        if(fcntl(c->fd, F_SETFL, O_NONBLOCK)!=0) {
           l2tp_log(LOG_WARNING, "failed to set nonblock: %s\n", strerror(errno));
            return -EINVAL;
        }

        fd2 = open (tty, O_RDWR);
        if (fd2 < 0) {
            l2tp_log (LOG_WARNING, "unable to open tty %s, cannot start pppd", tty);
            return -EINVAL;
        }
        stropt[pos++] = strdup(tty);
    }

    {
        struct ppp_opts *p = opts;
        int maxn_opts = sizeof(stropt) / sizeof(stropt[0]) - 1;
        while (p && pos < maxn_opts)
        {
            stropt[pos] = strdup (p->option);
            pos++;
            p = p->next;
        }
        stropt[pos] = NULL;
    }

#ifdef DEBUG_PPPD
    l2tp_log (LOG_DEBUG, "%s: I'm running: \n", __FUNCTION__);
    for (x = 0; stropt[x]; x++)
    {
        l2tp_log (LOG_DEBUG, "\"%s\" \n", stropt[x]);
    };
#endif
#ifdef __uClinux__
    c->pppd = vfork ();
#else
    c->pppd = fork ();
#endif

    if (c->pppd < 0)
    {
        /* parent */
        l2tp_log(LOG_WARNING,"%s: unable to fork(), abandoning!\n", __FUNCTION__);
        close(fd2);
        return -EINVAL;
    }
    else if (!c->pppd)
    {
        /* child */

        close (0); /* redundant; the dup2() below would do that, too */
        close (1); /* ditto */
        /* close (2); No, we want to keep the connection to /dev/null. */
#ifdef USE_KERNEL
       if (!kernel_support)
#endif
       {

        /* connect the pty to stdin and stdout */
        dup2 (fd2, 0);
        dup2 (fd2, 1);
        close(fd2);
       }
        /* close all the calls pty fds */
        st = tunnels.head;
        while (st)
        {
#ifdef USE_KERNEL
             if (kernel_support) {
                if(st->udp_fd!=-1)
                    close(st->udp_fd); /* tunnel UDP fd */
                if(st->pppox_fd!=-1)
                    close(st->pppox_fd); /* tunnel PPPoX fd */
             } else
#endif
			 {
                 sc = st->call_head;
                 while (sc)
                 {
                     if(sc->fd!=-1)
                        close (sc->fd); /* call pty fd */
                     sc = sc->next;
                 }
			 }
             st = st->next;
        }

        /* close the UDP socket fd */
        if(server_socket!=-1)
            close (server_socket);

        /* close the control pipe fd */
        if(control_fd!=-1)
            close (control_fd);

        if( c->dialing[0] )
        {
            setenv( "CALLER_ID", c->dialing, 1 );
        }
        execv (PPPD, stropt);
        l2tp_log (LOG_WARNING, "%s: Exec of %s failed!\n", __FUNCTION__, PPPD);
        _exit (1);
    }
    close (fd2);
    pos = 0;
    while (stropt[pos])
    {
        free (stropt[pos]);
        pos++;
    };
    return 0;
}

void destroy_tunnel (struct tunnel *t)
{
    /*
     * Immediately destroy a tunnel (and all its calls)
     * and free its resources.  This may be called
     * by the tunnel itself,so it needs to be
     * "suicide safe"
     */

    struct call *c, *me, *next;
    struct tunnel *p;
    struct timeval tv;
    if (!t)
        return;

    /*
     * Save ourselves until the very
     * end, since we might be calling this ourselves.
     * We must divorce ourself from the tunnel
     * structure, however, to avoid recursion
     * because of the logic of the destroy_call
     */
    me = t->self;

    /*
     * Destroy all the member calls
     */
    c = t->call_head;
    while (c)
    {
		next = c->next;
        destroy_call (c);
        c = next;
    };
    /*
     * Remove ourselves from the list of tunnels
     */

    if (tunnels.head == t)
    {
        tunnels.head = t->next;
        tunnels.count--;
    }
    else
    {
        p = tunnels.head;
        if (p)
        {
            while (p->next && (p->next != t))
                p = p->next;
            if (p->next)
            {
                p->next = t->next;
                tunnels.count--;
            }
            else
            {
                l2tp_log (LOG_WARNING,
                     "%s: unable to locate tunnel in tunnel list\n",
                     __FUNCTION__);
            }
        }
        else
        {
            l2tp_log (LOG_WARNING, "%s: tunnel list is empty!\n", __FUNCTION__);
        }
    }
    if (t->lac)
    {
        t->lac->t = NULL;
        if (t->lac->redial && (t->lac->rtimeout > 0) && !t->lac->rsched &&
            t->lac->active)
        {
            l2tp_log (LOG_INFO, "Will redial in %d seconds\n",
                 t->lac->rtimeout);
            tv.tv_sec = t->lac->rtimeout;
            tv.tv_usec = 0;
            t->lac->rsched = schedule (tv, magic_lac_dial, t->lac);
        }
    }
    /* XXX L2TP/IPSec: remove relevant SAs here?  NTB 20011010
     * XXX But what if another tunnel is using same SA?
     */
    if (t->lns)
        t->lns->t = NULL;
    if (t->chal_us.challenge)
        free (t->chal_us.challenge);
    if (t->chal_them.challenge)
        free (t->chal_them.challenge);
    /* we need no free(t->chal_us.vector) here because we malloc() and free()
       the memory pointed to by t->chal_us.vector at some other place */
    if (t->chal_them.vector)
        free (t->chal_them.vector);
    if (t->pppox_fd > -1 )
        close (t->pppox_fd);
    if (t->udp_fd > -1 )
        close (t->udp_fd);
    destroy_call (me);
    free (t);
}

static struct tunnel *l2tp_call (char *host, int port, struct lac *lac,
                          struct lns *lns)
{
    /*
     * Establish a tunnel from us to host
     * on port port
     */
    struct call *tmp = NULL;
    struct hostent *hp;
    struct in_addr addr;
    port = htons (port);
    hp = gethostbyname (host);
    if (!hp)
    {
        l2tp_log (LOG_WARNING, "Host name lookup failed for %s.\n",
             host);
        if (lac->redial && (lac->rtimeout > 0) && !lac->rsched && lac->active)
        {
            struct timeval tv;
            l2tp_log (LOG_INFO, "Will redial in %d seconds\n", lac->rtimeout);
            tv.tv_sec = lac->rtimeout;
            tv.tv_usec = 0;
            lac->rsched = schedule (tv, magic_lac_dial, lac);
        }
        return NULL;
    }
    bcopy (hp->h_addr, &addr.s_addr, hp->h_length);
    /* Force creation of a new tunnel
       and set it's tid to 0 to cause
       negotiation to occur */
    /*
     * to do IPsec properly here, we need to set a socket policy,
     * and/or communicate with pluto.
     */
    tmp = get_call (0, 0, addr, port, IPSEC_SAREF_NULL, IPSEC_SAREF_NULL);
    if (!tmp)
    {
        l2tp_log (LOG_WARNING, "%s: Unable to create tunnel to %s.\n", __FUNCTION__,
             host);
        return NULL;
    }
    tmp->container->tid = 0;
    tmp->container->lac = lac;
    tmp->container->lns = lns;
    tmp->lac = lac;
    tmp->lns = lns;
    if (lac)
        lac->t = tmp->container;
    if (lns)
        lns->t = tmp->container;
    /*
     * Since our state is 0, we will establish a tunnel now
     */
    l2tp_log (LOG_NOTICE, "Connecting to host %s, port %d\n", host,
         ntohs (port));
    control_finish (tmp->container, tmp);
    return tmp->container;
}

static void magic_lac_tunnel (void *data)
{
    struct lac *lac;
    lac = (struct lac *) data;
    if (!lac)
    {
        l2tp_log (LOG_WARNING, "%s: magic_lac_tunnel: called on NULL lac!\n",
             __FUNCTION__);
        return;
    }
    if (lac->lns)
    {
        /* FIXME: I should try different LNS's if I get failures */
        l2tp_call (lac->lns->hostname, lac->lns->port, lac, NULL);
    }
    else if (deflac && deflac->lns)
    {
        l2tp_call (deflac->lns->hostname, deflac->lns->port, lac, NULL);
    }
    else
    {
        l2tp_log (LOG_WARNING, "%s: Unable to find hostname to dial for '%s'\n",
             __FUNCTION__, lac->entname);
    }
}

static struct call *lac_call (int tid, struct lac *lac, struct lns *lns)
{
    struct tunnel *t = tunnels.head;
    struct call *tmp;
    while (t)
    {
        if (t->ourtid == tid)
        {
            tmp = new_call (t);
            if (!tmp)
            {
                l2tp_log (LOG_WARNING, "%s: unable to create new call\n",
                     __FUNCTION__);
                return NULL;
            }
            tmp->next = t->call_head;
            t->call_head = tmp;
            t->count++;
            tmp->cid = 0;
            tmp->lac = lac;
            tmp->lns = lns;
            if (lac)
                lac->c = tmp;
            l2tp_log (LOG_NOTICE, "Calling on tunnel %d\n", tid);
            strcpy (tmp->dial_no, dial_no_tmp); /*  jz: copy dialnumber to tmp->dial_no  */
            control_finish (t, tmp);
            return tmp;
        }
        t = t->next;
    };
    l2tp_log (LOG_DEBUG, "%s: No such tunnel %d to generate call.\n", __FUNCTION__,
         tid);
    return NULL;
}

void magic_lac_dial (void *data)
{
    struct lac *lac;
    lac = (struct lac *) data;
    if (!lac)
    {
        l2tp_log (LOG_WARNING, "%s : called on NULL lac!\n", __FUNCTION__);
        return;
    }
    if (!lac->active)
    {
        l2tp_log (LOG_DEBUG, "%s: LAC %s not active", __FUNCTION__, lac->entname);
        return;
    }
    lac->rsched = NULL;
    lac->rtries++;
    if (lac->rmax && (lac->rtries > lac->rmax))
    {
        l2tp_log (LOG_INFO, "%s: maximum retries exceeded.\n", __FUNCTION__);
        return;
    }
    if (!lac->t)
    {
#ifdef DEGUG_MAGIC
        l2tp_log (LOG_DEBUG, "%s : tunnel not up!  Connecting!\n", __FUNCTION__);
#endif
        magic_lac_tunnel (lac);
        return;
    }
    lac_call (lac->t->ourtid, lac, NULL);
}

static void lac_hangup (int cid)
{
    struct tunnel *t = tunnels.head;
    struct call *tmp;
    while (t)
    {
        tmp = t->call_head;
        while (tmp)
        {
            if (tmp->ourcid == cid)
            {
                l2tp_log (LOG_INFO,
                     "%s :Hanging up call %d, Local: %d, Remote: %d\n",
                     __FUNCTION__, tmp->serno, tmp->ourcid, tmp->cid);
                strcpy (tmp->errormsg, "Goodbye!");
/*                                    tmp->needclose = -1; */
                kill (tmp->pppd, SIGTERM);
                return;
            }
            tmp = tmp->next;
        }
        t = t->next;
    };
    l2tp_log (LOG_DEBUG, "%s : No such call %d to hang up.\n", __FUNCTION__, cid);
    return;
}

static void lac_disconnect (int tid)
{
    struct tunnel *t = tunnels.head;
    while (t)
    {
        if (t->ourtid == tid)
        {
            l2tp_log (LOG_INFO,
                 "Disconnecting from %s, Local: %d, Remote: %d\n",
                 IPADDY (t->peer.sin_addr), t->ourtid, t->tid);
            t->self->needclose = -1;
            strcpy (t->self->errormsg, "Goodbye!");
            call_close (t->self);
            return;
        }
        t = t->next;
    };
    l2tp_log (LOG_DEBUG, "No such tunnel %d to hang up.\n", tid);
    return;
}

struct tunnel *new_tunnel ()
{
    struct tunnel *tmp = calloc (1, sizeof (struct tunnel));
    unsigned char entropy_buf[2] = "\0";
    if (!tmp)
        return NULL;
    tmp->debug = -1;
    tmp->tid = -1;
#ifndef TESTING
/*      while(get_call((tmp->ourtid = rand() & 0xFFFF),0,0,0)); */
/*        tmp->ourtid = rand () & 0xFFFF; */
        /* get_entropy((char *)&tmp->ourtid, 2); */
        get_entropy(entropy_buf, 2);
        {
            unsigned short *temp;
            temp = (unsigned short *)entropy_buf;
            tmp->ourtid = *temp & 0xFFFF;
#ifdef DEBUG_ENTROPY
            l2tp_log(LOG_DEBUG, "ourtid = %u, entropy_buf = %hx\n", tmp->ourtid, *temp);
#endif
        }

#else
    tmp->ourtid = 0x6227;
#endif
    tmp->peer.sin_family = AF_INET;
    bzero (&(tmp->peer.sin_addr), sizeof (tmp->peer.sin_addr));
#ifdef SANITY
    tmp->sanity = -1;
#endif
    tmp->qtid = -1;
    tmp->ourfc = ASYNC_FRAMING | SYNC_FRAMING;
    tmp->ourtb = (((_u64) rand ()) << 32) | ((_u64) rand ());
    tmp->fc = -1;               /* These really need to be specified by the peer */
    tmp->bc = -1;               /* And we want to know if they forgot */
    if (!(tmp->self = new_call (tmp)))
    {
        free (tmp);
        return NULL;
    };
    tmp->ourrws = DEFAULT_RWS_SIZE;
    tmp->self->ourfbit = FBIT;
    tmp->rxspeed = DEFAULT_RX_BPS;
    tmp->txspeed = DEFAULT_TX_BPS;
    memset (tmp->chal_us.reply, 0, MD_SIG_SIZE);
    memset (tmp->chal_them.reply, 0, MD_SIG_SIZE);
    tmp->chal_them.vector = malloc (VECTOR_SIZE);
    return tmp;
}

static void write_res (FILE* res_file, const char *fmt, ...)
{
    if (!res_file || ferror (res_file) || feof (res_file))
        return;
    va_list args;
    va_start (args, fmt);
    vfprintf (res_file, fmt, args);
    va_end (args);
}

static int parse_one_line (char* bufp, int context, void* tc)
{
    /* FIXME: I should check for incompatible options */
    char *s, *d, *t;
    int linenum = 0;

    s = strtok (bufp, ";");
    // parse options token by token
    while (s != NULL)
    {
        linenum++;

        while ((*s < 33) && *s)
            s++;                /* Skip over beginning white space */
        t = s + strlen (s);
        while ((t >= s) && (*t < 33))
            *(t--) = 0;         /* Ditch trailing white space */
        if (!strlen (s))
            continue;
        if (!(t = strchr (s, '=')))
        {
            l2tp_log (LOG_WARNING, "%s: token %d: no '=' in data\n",
                 __FUNCTION__, linenum);
            return -1;
        }
        d = t;
        d--;
        t++;
        while ((d >= s) && (*d < 33))
            d--;
        d++;
        *d = 0;
        while (*t && (*t < 33))
            t++;
#ifdef DEBUG_CONTROL
        l2tp_log (LOG_DEBUG, "%s: field is %s, value is %s\n",
            __FUNCTION__, s, t);
#endif
        /* Okay, bit twidling is done.  Let's handle this */

        switch (parse_one_option (s, t, context, tc))
        {
        case -1:
            l2tp_log (LOG_WARNING, "%s: error token %d\n",
                __FUNCTION__, linenum);
            return -1;
        case -2:
            l2tp_log (LOG_CRIT, "%s: token %d: Unknown field '%s'\n",
                __FUNCTION__, linenum, s);
            return -1;
        }

        s = strtok (NULL, ";");
    }
    return 0;
}

static int parse_one_line_lac (char* bufp, struct lac *tc){
    return parse_one_line(bufp, CONTEXT_LAC, tc);
}

static int parse_one_line_lns (char* bufp, struct lns *tc){
    return parse_one_line(bufp, CONTEXT_LNS, tc);
}

static struct lns* find_lns_by_name(char* name){
    struct lns *cursor;

    /* ml: First check to see if we are searching for default */
    if(strcmp(name, "default") == 0){
        return deflns;
    }

    cursor  = lnslist;
    while (cursor)
    {
        if(strcasecmp (cursor->entname, name) ==0){
            return cursor;
        }
        cursor = cursor->next;
    };

    return NULL; /* ml: Ok we could not find anything*/
}

static int control_handle_available(FILE* resf, char* bufp) {
    UNUSED(bufp);
    struct lac *lac;
    struct lns *lns;

    write_res (resf, "%02i OK\n", 0);
    lns = lnslist;
    int lns_count = 0;
    while (lns)
    {
        write_res (resf, "%02i AVAILABLE lns.%d.name=%s\n", 0, lns_count, lns->entname);
        lns_count++;
        lns= lns->next;
    };

    /* Can the default really be NULL?*/
    if(deflns){
        write_res (resf, "%02i AVAILABLE lns.%d.name=%s\n", 0, lns_count, deflns->entname);
        lns_count++;
    }

    write_res (resf, "%02i AVAILABLE lns.count=%d\n", 0, lns_count);

    lac  = laclist;
    int lac_count = 0;
    while (lac)
    {
        write_res (resf, "%02i AVAILABLE lac.%d.name=%s\n", 0, lac_count, lac->entname);
        lac_count++;
        lac= lac->next;
    };

    if(deflac){
        write_res (resf, "%02i AVAILABLE lac.%d.name=%s\n", 0, lac_count, deflac->entname);
        lac_count++;
    }

    write_res (resf, "%02i AVAILABLE lac.count=%d\n", 0, lac_count);

	struct tunnel *st;
	st = tunnels.head;
	while (st)
	{
        write_res (resf, "%02i AVAILABLE tunnel %p, id %d, ourtid %d has %d calls and self %p\n", 0, st, st->tid, st->ourtid, st->count, st->self);
		st = st->next;
	}

	write_res (resf, "%02i AVAILABLE tunnels count=%d\n", 0, tunnels.count);
	write_res (resf, "%02i AVAILABLE calls count=%d\n", 0, tunnels.calls);
	return 1;
}

static int control_handle_lns_add_modify(FILE* resf, char* bufp){
    struct lns *lns;
    char* tunstr;
    char delims[] = " ";
    tunstr = strtok (&bufp[1], delims);
    lns = find_lns_by_name(tunstr);
    if(!lns){
        lns = new_lns();
        if(lns){ /* This seems a bit stupid, but new_lns() can return NULL */
            /* ml: Give me a name please :) */
            strncpy (lns->entname, tunstr, sizeof (lns->entname));

            /* ml: Is there any good reason why I cant add it now? */
            lns->next = lnslist;
            lnslist = lns;
        }
    }

    if(lns){
        bufp = tunstr + strlen (tunstr) + 1;
        if (parse_one_line_lns (bufp, lns))
        {
            write_res (resf, "%02i Configuration parse error\n", 3);
        }else{
            write_res (resf, "%02i OK: Saved value\n", 0);
        }
    }else{
        write_res (resf, "%02i Error: Could not find lns and could not create it\n", 1);
    }

    return 1;
}

static int control_handle_lns_remove(FILE* resf, char* bufp){
    char *tunstr;
    struct lns* lns;
    struct lns* prev_lns;
    struct tunnel* t;
    struct call* c;

    tunstr = strchr (bufp, ' ') + 1;
    lns = lnslist;
    prev_lns = NULL;
    while (lns && strcasecmp (lns->entname, tunstr) != 0)
    {
        prev_lns = lns;
        lns= lns->next;
    }
    if (!lns)
    {
        l2tp_log (LOG_DEBUG, "No such tunnel '%s'\n", tunstr);
        write_res (resf, "%02i No such tunnel '%s'\n", 1, tunstr);
        return 0;
    }

    /* We need to destroy the tunnels associated with this guy */
    t = tunnels.head;
    while(t){
        if(t->lns == lns){
            c = t->call_head;

            while (c)
            {
                call_close (c);
                c = c->next;
            };
        }
        t = t->next;
    }

    if (prev_lns == NULL){
        lnslist = lns->next;
    }else{
        prev_lns->next = lns->next;
    }

    free(lns);
    write_res (resf, "%02i OK\n", 0);
    return 1;
}

static int control_handle_lns_status(FILE* resf, char* bufp){
    struct lns *lns;
    char* tunstr;
    char delims[] = " ";

    tunstr = strtok (&bufp[1], delims);
    lns = find_lns_by_name(tunstr);
    if(lns){
        /* Lets keep it simple, what is useful first */
        write_res (resf, "%02i OK\n", 0);

        int active_tunnel_count = 0;
        struct tunnel* t = tunnels.head;
        while(t){
            if(t->lns == lns){
                /* Lets provide some information on each tunnel */
                write_res (resf, "%02i STATUS tunnels.%d.id=%d\n", 0, active_tunnel_count, t->tid);
                write_res (resf, "%02i STATUS tunnels.%d.peer=%s:%d\n", 0, active_tunnel_count,
                        IPADDY (t->peer.sin_addr), ntohs (t->peer.sin_port));

                /* And some call stats */
                struct call *c = t->call_head;
                int active_call_count = 0;
                while(c){
                    write_res (resf, "%02i STATUS tunnels.%d.calls.%d.id=%d\n", 0,
                            active_tunnel_count, active_call_count, c->ourcid);

                    write_res (resf, "%02i STATUS tunnels.%d.calls.%d.tx_bytes=%d\n", 0,
                            active_tunnel_count, active_call_count, c->tx_bytes);
                    write_res (resf, "%02i STATUS tunnels.%d.calls.%d.rx_bytes=%d\n", 0,
                            active_tunnel_count, active_call_count, c->rx_bytes);

                    write_res (resf, "%02i STATUS tunnels.%d.calls.%d.tx_pkts=%d\n", 0,
                            active_tunnel_count, active_call_count, c->tx_pkts);

                    write_res (resf, "%02i STATUS tunnels.%d.calls.%d.rx_pkts=%d\n", 0,
                            active_tunnel_count, active_call_count, c->rx_pkts);

                    c = c->next;
                    active_call_count++;
                }

                write_res (resf, "%02i STATUS tunnels.%d.calls.count=%d\n", 0,
                        active_tunnel_count, active_call_count);

                active_tunnel_count++;
            }
            t = t->next;
        }

        write_res (resf, "%02i STATUS tunnels.count=%d\n", 0, active_tunnel_count);
    }else{
        write_res (resf, "%02i Error: Could not find lns\n", 1);
    }

    return 1;
}

static int control_handle_tunnel(FILE* resf, char* bufp){
    char* host;
    host = strchr (bufp, ' ') + 1;
#ifdef DEBUG_CONTROL
    l2tp_log (LOG_DEBUG, "%s: Attempting to tunnel to %s\n",
            __FUNCTION__, host);
#endif
    if (l2tp_call (host, UDP_LISTEN_PORT, NULL, NULL))
        write_res (resf, "%02i OK\n", 0);
    else
        write_res (resf, "%02i Error\n", 1);
    return 1;
}

static int control_handle_lac_connect(FILE* resf, char* bufp){
    char* tunstr = NULL;
    char* authname= NULL;
    char* password = NULL;
    int tunl = 0;
    char delims[] = " ";
    struct lac* lac;

    switch_io = 1;  /* jz: Switch for Incoming - Outgoing Calls */
    tunstr = strtok (&bufp[1], delims);

    /* Are these passed on the command line? */
    authname = strtok (NULL, delims);
    password = strtok (NULL, delims);

    lac = laclist;
    while (lac && strcasecmp (lac->entname, tunstr)!=0)
    {
        lac = lac->next;
    }

    if(lac) {
        lac->active = -1;
        lac->rtries = 0;
        if (authname != NULL)
            strncpy (lac->authname, authname, STRLEN);
        if (password != NULL)
            strncpy (lac->password, password, STRLEN);
        if (!lac->c)
        {
            magic_lac_dial (lac);
            write_res (resf, "%02i OK\n", 0);
        } else {
            l2tp_log (LOG_DEBUG,
                    "Session '%s' already active!\n", lac->entname);
            write_res (resf, "%02i Session '%s' already active!\n", 1,
                    lac->entname);
        }
        return 0;
    }

    /* did not find a tunnel by name, look by number */
    tunl = atoi (tunstr);
    if (!tunl)
    {
        l2tp_log (LOG_DEBUG, "No such tunnel '%s'\n", tunstr);
        write_res (resf, "%02i No such tunnel '%s'\n", 1, tunstr);
        return 0;
    }
#ifdef DEBUG_CONTROL
    l2tp_log (LOG_DEBUG, "%s: Attempting to call on tunnel %d\n",
            __FUNCTION__, tunl);
#endif
    if (lac_call (tunl, NULL, NULL))
        write_res (resf, "%02i OK\n", 0);
    else
        write_res (resf, "%02i Error\n", 1);

    return 1;
}

static int control_handle_lac_outgoing_call(FILE* resf, char* bufp){
    char* sub_str;
    char* tunstr;
    char* tmp_ptr;
    struct lac* lac;
    int tunl;

    switch_io = 0;  /* jz: Switch for incoming - outgoing Calls */

    sub_str = strchr (bufp, ' ') + 1;
    tunstr = strtok (sub_str, " "); /* jz: using strtok function to get */
    tmp_ptr = strtok (NULL, " ");   /*     params out of the pipe       */
    strcpy (dial_no_tmp, tmp_ptr);

    lac = laclist;
    while (lac && strcasecmp (lac->entname, tunstr)!=0)
    {
        lac = lac->next;
    }

    if(lac) {
        lac->active = -1;
        lac->rtries = 0;
        if (!lac->c)
        {
            magic_lac_dial (lac);
            write_res (resf, "%02i OK\n", 0);
        } else {
            l2tp_log (LOG_DEBUG, "Session '%s' already active!\n",
                    lac->entname);
            write_res (resf, "%02i Session '%s' already active!\n", 1,
                    lac->entname);
        }
        return 0;
    }

    /* did not find a tunnel by name, look by number */
    tunl = atoi (tunstr);
    if (!tunl)
    {
        l2tp_log (LOG_DEBUG, "No such tunnel '%s'\n", tunstr);
        write_res (resf, "%02i No such tunnel '%s'\n", 1, tunstr);
        return 0;
    }
#ifdef DEBUG_CONTROL
    l2tp_log (LOG_DEBUG, "%s: Attempting to call on tunnel %d\n",
            __FUNCTION__, tunl);
#endif
    if (lac_call (tunl, NULL, NULL))
        write_res (resf, "%02i OK\n", 0);
    else
        write_res (resf, "%02i Error\n", 1);
    return 1;
}

static int control_handle_lac_hangup(FILE* resf, char* bufp){
    char* callstr;
    int call;

    callstr = strchr (bufp, ' ') + 1;
    call = atoi (callstr);
#ifdef DEBUG_CONTROL
    l2tp_log (LOG_DEBUG, "%s: Attempting to hangup call %d\n", __FUNCTION__,
            call);
#endif
    lac_hangup (call);
    write_res (resf, "%02i OK\n", 0);
    return 1;
}

static int control_handle_lac_disconnect(FILE* resf, char* bufp){
    char* tunstr;
    struct lac* lac;
    int tunl = 0;

    tunstr = strchr (bufp, ' ') + 1;
    lac = laclist;
    while (lac)
    {
        if (!strcasecmp (lac->entname, tunstr))
        {
            lac->active = 0;
            lac->rtries = 0;
            if (lac->t)
            {
                lac_disconnect (lac->t->ourtid);
                write_res (resf, "%02i OK\n", 0);
            } else {
                l2tp_log (LOG_DEBUG, "Session '%s' not up\n",
                        lac->entname);
                write_res (resf, "%02i Session '%s' not up\n", 1,
                        lac->entname);
            }
            return 0;
        }
        lac = lac->next;
    }
    if (lac)
        return 0;
    tunl = atoi (tunstr);
    if (!tunl)
    {
        l2tp_log (LOG_DEBUG, "No such tunnel '%s'\n", tunstr);
        write_res (resf, "%02i No such tunnel '%s'\n", 1, tunstr);
        return 0;
    }

#ifdef DEBUG_CONTROL
    l2tp_log (LOG_DEBUG, "%s: Attempting to disconnect tunnel %d\n",
            __FUNCTION__, tunl);
#endif
    lac_disconnect (tunl);
    write_res (resf, "%02i OK\n", 0);
    return 1;
}

static int control_handle_lac_add_modify(FILE* resf, char* bufp){
    char* tunstr;
    struct lac* lac;
    char delims[] = " ";

    int create_new_lac = 0;
    tunstr = strtok (&bufp[1], delims);
    if ((!tunstr) || (!strlen (tunstr)))
    {
        write_res (resf,
                "%02i Configuration parse error: lac-name expected\n", 1);
        l2tp_log (LOG_CRIT, "%s: lac-name expected\n", __FUNCTION__);
        return 0;
    }
    /* go to the end  of tunnel name*/
    bufp = tunstr + strlen (tunstr) + 1;
    /* try to find lac with _tunstr_ name in laclist */
    lac = laclist;
    while (lac)
    {
        if (!strcasecmp (tunstr, lac->entname))
            return 0;
        lac = lac->next;
    }

    /* nothing found, create new lac */
    lac = new_lac ();
    if (!lac)
    {
        write_res (resf,
                "%02i Could't create new lac: no memory\n", 2);
        l2tp_log (LOG_CRIT,
                "%s: Couldn't create new lac\n", __FUNCTION__);
        return 0;
    }
    create_new_lac = 1;
    strncpy (lac->entname, tunstr, sizeof (lac->entname));

    if (parse_one_line_lac (bufp, lac))
    {
        write_res (resf, "%02i Configuration parse error\n", 3);
        return 0;
    }
    if (create_new_lac)
    {
        lac->next = laclist;
        laclist = lac;
    }
    if (lac->autodial)
    {
#ifdef DEBUG_MAGIC
        l2tp_log (LOG_DEBUG, "%s: Autodialing '%s'\n", __FUNCTION__,
                lac->entname[0] ? lac->entname : "(unnamed)");
#endif
        lac->active = -1;
        switch_io = 1;  /* If we're a LAC, autodials will be ICRQ's */
        magic_lac_dial (lac);
        /* FIXME: Should I check magic_lac_dial result somehow? */
    }
    write_res (resf, "%02i OK\n", 0);
    return 1;
}

static int control_handle_lac_remove(FILE* resf, char* bufp){
    char *tunstr;
    struct lac* lac;
    struct lac* prev_lac;

    // find lac in laclist
    tunstr = strchr (bufp, ' ') + 1;
    lac = laclist;
    prev_lac = NULL;
    while (lac && strcasecmp (lac->entname, tunstr) != 0)
    {
        prev_lac = lac;
        lac = lac->next;
    }
    if (!lac)
    {
        l2tp_log (LOG_DEBUG, "No such tunnel '%s'\n", tunstr);
        write_res (resf, "%02i No such tunnel '%s'\n", 1, tunstr);
        return 0;
    }
    // disconnect lac
    lac->active = 0;
    lac->rtries = 0;
    if (lac->t)
    {
        lac_disconnect (lac->t->ourtid);
        /* destroy_tunnel may clear lac->t */
        if (lac->t)
        {
            lac->t->lac = NULL;
            if(lac->t->self)
                lac->t->self->lac = NULL;
        }
    }
    if (lac->c)
    {
        struct call *c = lac->c;
        while (c)
        {
            c->lac = NULL;
            c = c->next;
        }
    }
    if (lac->lns)
    {
        struct host *t, *h = lac->lns;
        while (h)
        {
            t = h->next;
            free(h);
            h = t;
        }
    }

    // removes lac from laclist
    if (prev_lac == NULL)
        laclist = lac->next;
    else
        prev_lac->next = lac->next;

    free(lac);
    write_res (resf, "%02i OK\n", 0);
    return 1;
}

static int control_handle_lac_status(){
    show_status ();
    return 1;
}

void do_control ()
{
    char buf[CONTROL_PIPE_MESSAGE_SIZE];
    char *bufp; /* current buffer pointer */

    int cnt = -1;
    int done = 0;
    int handler_found = 0;
    struct control_requests_handler* handler = NULL;

    bzero(buf, sizeof(buf));
    buf[0]='\0';

    char* res_filename; /* name of file to write result of command */
    FILE* resf; /* stream for write result of command */

    while (!done)
    {
        cnt = read (control_fd, buf, sizeof (buf));
        if (cnt <= 0)
        {
            if(cnt < 0 && errno != EINTR) {
                perror("controlfd");
            }
            done = 1;
            break;
        }

        if (buf[cnt - 1] == '\n')
            buf[--cnt] = 0;
#ifdef DEBUG_CONTROL
        l2tp_log (LOG_DEBUG, "%s: Got message %s (%d bytes long)\n",
                __FUNCTION__, buf, cnt);
#endif
        bufp = buf;
        /* check if caller want to get result */
        if (bufp[0] == '@')
        {
            /* parse filename (@/path/to/file *...), where * is command */
            res_filename = &bufp[1];
            int fnlength = strcspn(res_filename, " ");
            if ((fnlength == 0) || (res_filename[fnlength] == '\0')){
                l2tp_log (LOG_DEBUG,
                        "%s: Can't parse result filename or command\n",
                        __FUNCTION__
                        );
                continue;
            }
            res_filename[fnlength] = '\0';
            bufp = &res_filename[fnlength + 1]; /* skip filename in bufp */

            /*FIXME: check quotes to allow filenames with spaces?
              (do not forget quotes escaping to allow filenames with quotes)*/

            resf = fopen (res_filename, "w");
            if (!resf) {
                l2tp_log (LOG_DEBUG, "%s: Can't open result file %s\n",
                        __FUNCTION__, res_filename);
                continue;
            }
        }else{
            resf = NULL;
            res_filename = NULL; /* to avoid 'may be used unitialized' warning */
        }

        /* Search for a handler based on request type */
        for(handler = control_handlers; handler->handler; handler++){

            /* If handler is found, then handle the request and set handler_found = 1 */
            if(handler->type == bufp[0]){
                handler->handler(resf, bufp);
                handler_found = 1;
                break;
            }
        }

        /* Does nto appear as though we found a handler, so respond with an error*/
        if(!handler_found){
            l2tp_log (LOG_DEBUG, "Unknown command %c\n", bufp[0]);
            write_res (resf, "%02i Unknown command %c\n", 1, bufp[0]);
        }

        if (resf)
        {
            fclose (resf);
            /* unlink it anyway to prevent leftover a regular file. */
            unlink(res_filename);
        }
    }

    /* Otherwise select goes nuts. Yeah, this just seems wrong */
    close (control_fd);
    open_controlfd();
}


static void usage(void) {
    printf("\nxl2tpd version:  %s\n", SERVER_VERSION);
    printf("Usage: xl2tpd [-c <config file>] [-s <secret file>] [-p <pid file>]\n"
            "              [-C <control file>] [-D] [-l] [-q <tos decimal value for control>]\n"
            "              [-v, --version]\n");
    printf("\n");
    exit(1);
}

static void init_args(int argc, char *argv[])
{
    int i=0;

    gconfig.daemon=1;
    gconfig.syslog=-1;
    memset(gconfig.altauthfile,0,STRLEN);
    memset(gconfig.altconfigfile,0,STRLEN);
    memset(gconfig.authfile,0,STRLEN);
    memset(gconfig.configfile,0,STRLEN);
    memset(gconfig.pidfile,0,STRLEN);
    memset(gconfig.controlfile,0,STRLEN);
    memset(gconfig.controltos,0,STRLEN);
    strncpy(gconfig.altauthfile,ALT_DEFAULT_AUTH_FILE,
            sizeof(gconfig.altauthfile) - 1);
    strncpy(gconfig.altconfigfile,ALT_DEFAULT_CONFIG_FILE,
            sizeof(gconfig.altconfigfile) - 1);
    strncpy(gconfig.authfile,DEFAULT_AUTH_FILE,
            sizeof(gconfig.authfile) - 1);
    strncpy(gconfig.configfile,DEFAULT_CONFIG_FILE,
            sizeof(gconfig.configfile) - 1);
    strncpy(gconfig.pidfile,DEFAULT_PID_FILE,
            sizeof(gconfig.pidfile) - 1);
    strncpy(gconfig.controlfile,CONTROL_PIPE,
            sizeof(gconfig.controlfile) - 1);
    gconfig.ipsecsaref = 0;

    for (i = 1; i < argc; i++) {
        if ((! strncmp(argv[i],"--version",9))
                || (! strncmp(argv[i],"-v",2))) {
            printf("\nxl2tpd version:  %s\n",SERVER_VERSION);
            exit(1);
        }

        if(! strncmp(argv[i],"-c",2)) {
            if(++i == argc)
                usage();
            else
                strncpy(gconfig.configfile,argv[i],
                        sizeof(gconfig.configfile) - 1);
        }
        else if (! strncmp(argv[i],"-D",2)) {
            gconfig.daemon=0;
        }
        else if (! strncmp(argv[i],"-l",2)) {
            gconfig.syslog=1;
        }
        else if (! strncmp(argv[i],"-s",2)) {
            if(++i == argc)
                usage();
            else
                strncpy(gconfig.authfile,argv[i],
                        sizeof(gconfig.authfile) - 1);
        }
        else if (! strncmp(argv[i],"-p",2)) {
            if(++i == argc)
                usage();
            else
                strncpy(gconfig.pidfile,argv[i],
                        sizeof(gconfig.pidfile) - 1);
        }
        else if (! strncmp(argv[i],"-C",2)) {
            if(++i == argc)
                usage();
            else
                strncpy(gconfig.controlfile,argv[i],
                        sizeof(gconfig.controlfile) - 1);
        }
        else if (! strncmp(argv[i],"-q",2)) {
            if(++i == argc)
                usage();
            else {
            	strncpy(gconfig.controltos,argv[i],
            	                        sizeof(gconfig.controltos) - 1);
            	if (atoi(gconfig.controltos)<0 || atoi(gconfig.controltos)>255)
            	{
	            	printf ("TOS value %s out of range(0-255)!\n", gconfig.controltos);
	            	usage();
                }
            }
        }
        else {
            usage();
        }
    }

    /*
     * defaults to syslog if no log facility was explicitly
     * specified and we are about to daemonize
     */
    if (gconfig.syslog < 0)
        gconfig.syslog = gconfig.daemon;
}


static void daemonize() {
    int pid=0;
    int i;

#ifndef CONFIG_SNAPGEAR
    if((pid = fork()) < 0) {
        l2tp_log(LOG_INFO, "%s: Unable to fork ()\n",__FUNCTION__);
        close(server_socket);
        exit(1);
    }
    else if (pid)
    {
        close(server_socket);
        closelog();
        exit(0);
    }

    close(0);
    i = open("/dev/null", O_RDWR);
    if (i == -1) {
        l2tp_log(LOG_INFO, "Redirect of stdin to /dev/null failed\n");
    } else {
        if (dup2(0, 1) == -1)
            l2tp_log(LOG_INFO, "Redirect of stdout to /dev/null failed\n");
        if (dup2(0, 2) == -1)
            l2tp_log(LOG_INFO, "Redirect of stderr to /dev/null failed\n");
        close(i);
    }
#endif
}

static void consider_pidfile() {
    int pid=0;
    int i,l;
    char buf[STRLEN];

    /* Read previous pid file. */
    i = open(gconfig.pidfile,O_RDONLY);
    if (i < 0) {
        /* l2tp_log(LOG_DEBUG, "%s: Unable to read pid file [%s]\n",
           __FUNCTION__, gconfig.pidfile);
         */
    } else
    {
        l=read(i,buf,sizeof(buf)-1);
        close (i);
        if (l >= 0)
        {
            buf[l] = '\0';
            pid = atoi(buf);
        }

        /* If the previous server process is still running,
           complain and exit immediately. */
        if (pid && pid != getpid () && kill (pid, 0) == 0)
        {
            l2tp_log(LOG_INFO,
                    "%s: There's already a xl2tpd server running.\n",
                    __FUNCTION__);
            close(server_socket);
            exit(1);
        }
    }

    pid = setsid();

    unlink(gconfig.pidfile);
    if ((i = open (gconfig.pidfile, O_WRONLY | O_CREAT, 0640)) >= 0) {
        snprintf (buf, sizeof(buf), "%d\n", (int)getpid());
        if (-1 == write (i, buf, strlen(buf)))
        {
            l2tp_log (LOG_CRIT, "%s: Unable to write to %s.\n",
                    __FUNCTION__, gconfig.pidfile);
            close (i);
            exit(1);
        }
        close (i);
    }
}

static void open_controlfd()
{
    control_fd = open (gconfig.controlfile, O_RDONLY | O_NONBLOCK, 0600);
    if (control_fd < 0)
    {
        l2tp_log (LOG_CRIT, "%s: Unable to open %s for reading.\n",
                __FUNCTION__, gconfig.controlfile);
        exit (1);
    }

    /* turn off O_NONBLOCK */
    if(fcntl(control_fd, F_SETFL, O_RDONLY)==-1) {
        l2tp_log(LOG_CRIT, "Can not turn off nonblocking mode for controlfd: %s\n",
                strerror(errno));
        exit(1);
    }
}

static void init (int argc,char *argv[])
{
    struct lac *lac;
    struct in_addr listenaddr;
    struct utsname uts;

    init_args (argc,argv);
    srand( time(NULL) );
    rand_source = 0;
    init_addr ();
    if (init_config ())
    {
        l2tp_log (LOG_CRIT, "%s: Unable to load config file\n", __FUNCTION__);
        exit (1);
    }
    if (uname (&uts)<0)
    {
        l2tp_log (LOG_CRIT, "%s : Unable to determine host system\n",
                __FUNCTION__);
        exit (1);
    }
    init_tunnel_list (&tunnels);
    if (init_network ())
        exit (1);

    if (gconfig.daemon)
        daemonize ();

    consider_pidfile();

    signal (SIGTERM, &sigterm_handler);
    signal (SIGINT, &sigint_handler);
    signal (SIGCHLD, &sigchld_handler);
    signal (SIGUSR1, &sigusr1_handler);
    signal (SIGHUP, &sighup_handler);
    signal (SIGPIPE, SIG_IGN);
    init_scheduler ();

    unlink(gconfig.controlfile);
    mkfifo (gconfig.controlfile, 0600);

    open_controlfd();

    l2tp_log (LOG_INFO, "xl2tpd version " SERVER_VERSION " started on %s PID:%d\n",
            hostname, getpid ());
    l2tp_log (LOG_INFO,
            "Written by Mark Spencer, Copyright (C) 1998, Adtran, Inc.\n");
    l2tp_log (LOG_INFO, "Forked by Scott Balmos and David Stipp, (C) 2001\n");
    l2tp_log (LOG_INFO, "Inherited by Jeff McAdams, (C) 2002\n");
    l2tp_log (LOG_INFO, "Forked again by Xelerance (www.xelerance.com) (C) 2006-2016\n");
    listenaddr.s_addr = gconfig.listenaddr;
    l2tp_log (LOG_INFO, "Listening on IP address %s, port %d\n",
            inet_ntoa(listenaddr), gconfig.port);
    lac = laclist;
    while (lac)
    {
        if (lac->autodial)
        {
#ifdef DEBUG_MAGIC
            l2tp_log (LOG_DEBUG, "%s: Autodialing '%s'\n", __FUNCTION__,
                    lac->entname[0] ? lac->entname : "(unnamed)");
#endif
            lac->active = -1;
            switch_io = 1;      /* If we're a LAC, autodials will be ICRQ's */
            magic_lac_dial (lac);
        }
        lac = lac->next;
    }
}

int main (int argc, char *argv[])
{
    init(argc,argv);
    dial_no_tmp = calloc (128, sizeof (char));
    network_thread ();
    return 0;
}
