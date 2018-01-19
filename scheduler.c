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

#ifdef DEBUG_SCHEDULE
#define SDBG(lvl,fmt,a...) \
    l2tp_log(lvl, "SCHEDULER: " fmt "\n", ##a)
#else
#define SDBG(lvl,fmt,a...) \
    ({})
#endif

#ifdef DEBUG_SCHEDULE
#define __schedule_list_op(se,op,fn,ln) ({ \
        (se)->list_op_name = op; \
        (se)->list_op_fn = fn; \
        (se)->list_op_ln = ln; \
    })
#else
#define __schedule_list_op(se,op,fn,ln) ({ })
#endif

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

#if defined(DEBUG_SCHEDULE)
    SDBG(LOG_DEBUG, "calling '%s' func=%p data=%p",
         se->handler_name, se->func, se->data);
#elif defined(DEBUG_EVENTS)
    l2tp_log(LOG_DEBUG, "EVENT: %s: fd=%d event=%hd arg=%p\n",
             __FUNCTION__, fd, event, arg);
#endif

    __schedule_remove(se);
    se->func(se->data);
    __schedule_list_op(se, "ran", __FUNCTION__, __LINE__);
    free_poison(se, 'S', sizeof(*se));
    free(se);
}

struct schedule_entry *__schedule (struct timeval tv, void (*func) (void *),
                                 void *data SCHEDULE_DEBUG_OPT_ARGS
                                 SCHEDULE_FUNC_OPT_ARGS)
{
    struct schedule_entry *se;

    SDBG(LOG_DEBUG, "adding '%s' func=%p data=%p in %lu.%06lu sec from %s:%u",
         handler_name, func, data, tv.tv_sec, tv.tv_usec, caller_fn, caller_ln);

    se = calloc(1, sizeof(struct schedule_entry));
    assert(se);

    se->tv = tv;
    se->func = func;
    se->data = data;
#ifdef DEBUG_SCHEDULE
    se->handler_name = handler_name;
#endif

    __schedule_append(se);
    __schedule_list_op(se, "add", caller_fn, caller_ln);

    evtimer_set(&se->ev, handle_schedule_event, se);
    evtimer_add(&se->ev, &se->tv);

    return se;
}

void __deschedule (struct schedule_entry *se SCHEDULE_DEBUG_OPT_ARGS)
{
    SDBG(LOG_DEBUG, "removing '%s' func=%p data=%p from %s:%u",
         se->handler_name, se->func, se->data, caller_fn, caller_ln);

    evtimer_del(&se->ev);
    __schedule_remove(se);
    __schedule_list_op(se, "del", caller_fn, caller_ln);
    free_poison(se, 's', sizeof(*se));
    free(se);
}

int __iterate_schedule(schedule_iterator_t iterator, void *data
                       SCHEDULE_ITER_OPT_ARGS)
{
    struct schedule_entry *se, *tmp;
    int bail = 0;

    list_for_each_entry_safe(se, tmp, &schedule_list, link) {
        SDBG(LOG_DEBUG, "%s iterating over '%s' from %s:%u",
             iterator_name, se->handler_name, caller_fn, caller_ln);
        bail = iterator(se, data);
        if (bail)
            break;
    }

    return bail;
}

/*
 * vim: :set sw=4 ts=4 et
 */
