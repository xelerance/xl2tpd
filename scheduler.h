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
#ifdef DEBUG_SCHEDULE
    const char *handler_name;
    const char *list_op_name;
    const char *list_op_fn;
    int list_op_ln;
#endif
};

#define __stringify_1(x...) #x
#define __stringify(x...) __stringify_1(x)

#ifdef DEBUG_SCHEDULE
#define SCHEDULE_DEBUG_OPT_ARGS       , const char *caller_fn, int caller_ln
#define SCHEDULE_DEBUG_OPT_VALS       , __FUNCTION__, __LINE__
#define SCHEDULE_FUNC_OPT_ARGS        , const char *handler_name
#define SCHEDULE_FUNC_OPT_VALS(func)  , __stringify(func)
#define SCHEDULE_ITER_OPT_ARGS        , const char *caller_fn, int caller_ln, \
					const char *iterator_name
#define SCHEDULE_ITER_OPT_OPTS(iter)  , __FUNCTION__, __LINE__, __stringify(iter)
#else
#define SCHEDULE_DEBUG_OPT_ARGS
#define SCHEDULE_DEBUG_OPT_VALS
#define SCHEDULE_FUNC_OPT_ARGS
#define SCHEDULE_FUNC_OPT_VALS(func)
#define SCHEDULE_ITER_OPT_ARGS
#define SCHEDULE_ITER_OPT_OPTS(iter)
#endif

extern struct schedule_entry *__schedule (struct timeval tv,
					  void (*func) (void *), void *data
					  SCHEDULE_DEBUG_OPT_ARGS
					  SCHEDULE_FUNC_OPT_ARGS);
#define schedule(tv,func,data) __schedule(tv,func,data \
					  SCHEDULE_DEBUG_OPT_VALS \
					  SCHEDULE_FUNC_OPT_VALS(func))

extern void __deschedule (struct schedule_entry * SCHEDULE_DEBUG_OPT_ARGS);
#define deschedule(se) __deschedule(se \
				    SCHEDULE_DEBUG_OPT_VALS)

extern void handle_schedule_event(int fd, short event, void *arg);

typedef int (*schedule_iterator_t)(struct schedule_entry *se, void *data);
extern int __iterate_schedule(schedule_iterator_t, void *data
			      SCHEDULE_ITER_OPT_ARGS);
#define iterate_schedule(iter,data) __iterate_schedule(iter,data \
						SCHEDULE_ITER_OPT_OPTS(iter))

#endif
