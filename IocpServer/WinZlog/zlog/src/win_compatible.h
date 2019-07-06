//windows¼æÈÝÎÄ¼þ
#ifndef WIN_COMPATIBLE_H
#define WIN_COMPATIBLE_H

#include "event.h"
#include <time.h>
#include <stdio.h>

static long getEventTid(zlog_event_t * a_event)
{
	if(!a_event)
	{
		return 0;
	}
#ifdef _MSC_VER
	return (long)(a_event->tid.p);
#else
	return (long)(a_event->tid);
#endif
}
//
#ifdef _MSC_VER

#define STDOUT_FILENO GetStdHandle(STD_OUTPUT_HANDLE)
#define STDERR_FILENO GetStdHandle(STD_ERROR_HANDLE)
#define va_copy(a, b) a=b
#define lstat _stat
#define snprintf _snprintf_s
#define popen _popen
#define pclose _pclose
#define DEF_TIME_FMT "%Y-%m-%d %H:%M:%S"

#else

#define DEF_TIME_FMT "%F %T"

#endif

#endif
