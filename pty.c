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
 * Pseudo-pty allocation routines...  Concepts and code borrowed
 * from pty-redir by Magosanyi Arpad.
 *
 */

#include "l2tp.h"
#include <fcntl.h>

#ifdef SOLARIS
#define PTY00 "/dev/ptyXX"
#define PTY10 "pqrstuvwxyz"
#define PTY01 "0123456789abcdef"
#endif

#ifdef LINUX
#define PTY00 "/dev/ptyXX"
#define PTY10 "pqrstuvwxyzabcde"
#define PTY01 "0123456789abcdef"
#endif

#ifdef FREEBSD
#define PTY00 "/dev/ptyXX"
#define PTY10 "p"
#define PTY01 "0123456789abcdefghijklmnopqrstuv"
#endif

int getPtyMaster (char *tty10, char *tty01)
{
    char *p10;
    char *p01;
    static char dev[] = PTY00;
    int fd;

    for (p10 = PTY10; *p10; p10++)
    {
        dev[8] = *p10;
        for (p01 = PTY01; *p01; p01++)
        {
            dev[9] = *p01;
            fd = open (dev, O_RDWR | O_NONBLOCK);
            if (fd >= 0)
            {
                *tty10 = *p10;
                *tty01 = *p01;
                return fd;
            }
        }
    }
    log (LOG_CRIT, "%s: No more free pseudo-tty's\n", __FUNCTION__);
    return -1;
}
