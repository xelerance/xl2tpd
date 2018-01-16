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
 * Scheduler code for time based functionality
 *
 */

#include <stdlib.h>
#include <string.h>
#include <event.h>
#include <assert.h>
#include "l2tp.h"
#include "scheduler.h"

static LIST_HEAD(schedule_list);

static void __schedule_append(struct schedule_entry *se)
{
    list_add_tail(&se->link, &schedule_list);
}
static void __schedule_remove(struct schedule_entry *se)
{
    list_del(&se->link);
}

void handle_schedule_event(int fd, short event, void *arg)
{
    struct schedule_entry *se = arg;

    __schedule_remove(se);
    se->func(se->data);
    free(se);
}

struct schedule_entry *schedule (struct timeval tv, void (*func) (void *),
                                 void *data)
{
    struct schedule_entry *se;

    se = calloc(1, sizeof(struct schedule_entry));
    assert(se);

    se->tv = tv;
    se->func = func;
    se->data = data;

    __schedule_append(se);

    evtimer_set(&se->ev, handle_schedule_event, se);
    evtimer_add(&se->ev, &se->tv);

    return se;
}

void deschedule (struct schedule_entry *se)
{
    evtimer_del(&se->ev);
    __schedule_remove(se);
    free(se);
}

int iterate_schedule(schedule_iterator_t iterator, void *data)
{
    struct schedule_entry *se, *tmp;
    int bail = 0;

    list_for_each_entry_safe(se, tmp, &schedule_list, link) {
        bail = iterator(se, data);
        if (bail)
            break;
    }

    return bail;
}

/*
 * vim: :set sw=4 ts=4 et
 */
