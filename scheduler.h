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
 * Scheduler structures and functions
 *
 */

#ifndef _SCHEDULER_H
#define _SCHEDULER_H
#include <sys/time.h>
#include "list.h"

struct schedule_entry {
    struct timeval tv;
    void (*func)(void*);
    void *data;
    struct list_head link;
    struct event ev;
};

extern struct schedule_entry *schedule (struct timeval tv, void (*func) (void *),
                                 void *data);
extern void deschedule (struct schedule_entry *);
extern void handle_schedule_event(int fd, short event, void *arg);

typedef int (*schedule_iterator_t)(struct schedule_entry *se, void *data);
extern int iterate_schedule(schedule_iterator_t, void *data);

#endif
