/*
 * Copyright (C) 1999  Internet Software Consortium.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef ISC_TYPES_H
#define ISC_TYPES_H 1

#include <isc/int.h>
#include <isc/boolean.h>
#include <isc/list.h>

/***
 *** Core Types.
 ***/

typedef unsigned int			isc_result_t;
typedef struct isc_mem			isc_mem_t;
typedef struct isc_mempool		isc_mempool_t;
typedef struct isc_msgcat		isc_msgcat_t;
typedef unsigned int			isc_eventtype_t;
typedef struct isc_event		isc_event_t;
typedef ISC_LIST(struct isc_event)	isc_eventlist_t;
typedef struct isc_task			isc_task_t;
typedef struct isc_taskmgr		isc_taskmgr_t;
typedef struct isc_rwlock		isc_rwlock_t;

typedef void (*isc_taskaction_t)(isc_task_t *, isc_event_t *);

#endif /* ISC_TYPES_H */