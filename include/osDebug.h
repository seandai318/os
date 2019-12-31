/**
 * @file osDebug.h  Interface to debugging module
 *
 * Copyright (C) 2019, InterLogic
 */


#ifndef _OS_DEBUG_H
#define _OS_DEBUG_H

#include <time.h>
#include <stdio.h>
#include "osTypes.h"


/** Debug levels */
enum {
	DBG_EMERG       = 0,       /**< System is unusable               */
	DBG_ALERT       = 1,       /**< Action must be taken immediately */
	DBG_CRIT        = 2,       /**< Critical conditions              */
	DBG_ERROR       = 3,       /**< Error conditions                 */
	DBG_WARNING     = 4,       /**< Warning conditions               */
	DBG_NOTICE      = 5,       /**< Normal but significant condition */
	DBG_INFO        = 6,       /**< Informational                    */
	DBG_DEBUG       = 7        /**< Debug-level messages             */
};



#define logEmerg(...) \
do {\
	struct timespec tp; \
	struct tm d; \
	clock_gettime(CLOCK_REALTIME, &tp); \
	gmtime_r(&tp.tv_sec, &d); \
	char dstr[200];  \
	sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%6ld[%s:%s:%d, Emergency] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, __FILE__, __func__, __LINE__);  \
	osDbg_printf(DBG_EMERG, dstr); \
	osDbg_printf(DBG_EMERG, __VA_ARGS__);\
	osDbg_printf(DBG_EMERG, "\n");\
} while(0);\

#define logAlert(...) \
do {\
	if(osDbg_isBypass(DBG_ALERT)) \
		continue;	\
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%6ld[%s:%s:%d, Alert] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, __FILE__, __func__, __LINE__);  \
    osDbg_printf(DBG_ALERT, dstr); \
    osDbg_printf(DBG_ALERT, __VA_ARGS__);\
	osDbg_printf(DBG_ALERT, "\n");\
} while(0);\


#define logCrit(...) \
do {\
	if(osDbg_isBypass(DBG_CRIT)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%6ld[%s:%s:%d, Critical] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, __FILE__, __func__, __LINE__);  \
    osDbg_printf(DBG_CRIT, dstr); \
    osDbg_printf(DBG_CRIT, __VA_ARGS__);\
    osDbg_printf(DBG_CRIT, "\n");\
} while(0);\


#define logError(...) \
do {\
    if(osDbg_isBypass(DBG_ERROR)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%6ld[%s:%s:%d, Error] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, __FILE__, __func__, __LINE__);  \
    osDbg_printf(DBG_ERROR, dstr); \
    osDbg_printf(DBG_ERROR, __VA_ARGS__);\
    osDbg_printf(DBG_ERROR, "\n");\
} while(0);\


#define logWarning(...) \
do {\
    if(osDbg_isBypass(DBG_WARNING)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%6ld[%s:%s:%d, Warning] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, __FILE__, __func__, __LINE__);  \
    osDbg_printf(DBG_WARNING, dstr); \
    osDbg_printf(DBG_WARNING, __VA_ARGS__);\
    osDbg_printf(DBG_WARNING, "\n");\
} while(0);\


#define logNotice(...) \
do {\
    if(osDbg_isBypass(DBG_NOTICE)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%6ld[%s:%s:%d, Notice] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, __FILE__, __func__, __LINE__);  \
    osDbg_printf(DBG_NOTICE, dstr); \
    osDbg_printf(DBG_NOTICE, __VA_ARGS__);\
    osDbg_printf(DBG_NOTICE, "\n");\
} while(0);\


#define logInfo(...) \
do {\
    if(osDbg_isBypass(DBG_INFO)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%6ld[%s:%s:%d, Info] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, __FILE__, __func__, __LINE__);  \
    osDbg_printf(DBG_INFO, dstr); \
    osDbg_printf(DBG_INFO, __VA_ARGS__);\
    osDbg_printf(DBG_INFO, "\n");\
} while(0);\


#define debug(...) \
do {\
    if(osDbg_isBypass(DBG_DEBUG)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%6ld[%s:%s:%d] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, __FILE__, __func__, __LINE__);  \
    osDbg_printf(DBG_DEBUG, dstr); \
    osDbg_printf(DBG_DEBUG, __VA_ARGS__);\
    osDbg_printf(DBG_DEBUG, "\n");\
} while(0);\


#define DEBUG_BEGIN	debug("entering...")
#define DEBUG_END	debug("exit.")


/** Debug flags */
typedef enum osDbg_flags {
	DBG_NONE = 0,                 /**< No debug flags         */
	DBG_TIME = 1<<0,              /**< Print timestamp flag   */
	DBG_ANSI = 1<<1,              /**< Print ANSI color codes */
	DBG_ALL  = DBG_TIME|DBG_ANSI  /**< All flags enabled      */
} osDbg_flags_e;


/**
 * Defines the debug print handler
 *
 * @param level Debug level
 * @param p     Debug string
 * @param len   String length
 * @param arg   Handler argument
 */
typedef void (osDbgPrint_h)(int level, const char *p, size_t len, void *arg);

void osDbg_init(int level, enum osDbg_flags flags);
void osDbg_close(void);
int  osDbg_setLogfile(const char *name);
bool osDbg_isBypass(int level);
void osDbg_setHandler(osDbgPrint_h *ph, void *arg);
void osDbg_printf(int level, const char *fmt, ...);
const char *osDbg_getLevelStr(int level);
void osDbg_printStr(const char* str, size_t strlen);

#endif
