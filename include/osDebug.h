/**
 * @file osDebug.h  Interface to debugging module
 *
 * Copyright (C) 2020, 2019, Sean Dai
 */


#ifndef _OS_DEBUG_H
#define _OS_DEBUG_H

#include <libgen.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>

#include "osTypes.h"


/** Debug levels */
typedef enum {
	DBG_EMERG       = 0,       /**< System is unusable               */
	DBG_ALERT       = 1,       /**< Action must be taken immediately */
	DBG_CRIT        = 2,       /**< Critical conditions              */
	DBG_ERROR       = 3,       /**< Error conditions                 */
	DBG_WARNING     = 4,       /**< Warning conditions               */
	DBG_NOTICE      = 5,       /**< Normal but significant condition */
	DBG_INFO        = 6,       /**< Informational                    */
	DBG_DEBUG       = 7        /**< Debug-level messages             */
} osDbgLevel_e;


typedef enum {
	LM_TIMER,		//OS timer
	LM_MEM,			//OS memory
	LM_SIPP,		//SIP Parser
	LM_SIPTR,		//SIP Transaction
	LM_TRANSPORT,	//SIP Transportation
	LM_SIPAPP,		//SIP Application
	LM_XMLP,		//XML parser
	LM_ALL,			//All modules
} osLogModule_e;



//note: filename is used here.  According to basename() man page, it may not be safe to pass in __FILE__directly here.  
//People suggest to take a copy of __FILE__ and do basename() on it, like char* filename = strdup(__FILE__); ... free(filename), 
//or even a simpler way: char filename[]=__FILE__;  I also see there is a propsal to provide a compile time macro for the basename, 
//like __FILE_BASENAME__, but it is not in the current gcc now.  when it is available, we will remove basename() to use the new one. 
//
//POSIX basename() man:
//The basename() function may modify the string pointed to by path, and may return a pointer to internal storage. The returned pointer 
//might be invalidated or the storage might be overwritten by a subsequent call to basename().
#define logEmerg(...) \
do {\
	struct timespec tp; \
	struct tm d; \
	clock_gettime(CLOCK_REALTIME, &tp); \
	gmtime_r(&tp.tv_sec, &d); \
	pthread_t tid = pthread_self();	\
	char dstr[200];  \
	char filename[]=__FILE__;	\
	sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Emergency] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
	osDbg_printf(DBG_EMERG, dstr); \
	osDbg_printf(DBG_EMERG, __VA_ARGS__);\
	osDbg_printf(DBG_EMERG, "\n");\
} while(0);\

#define logAlert(...) \
do {\
	if(osDbg_isBypass(DBG_ALERT, LM_ALL)) \
		continue;	\
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self();	\
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Alert] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_ALERT, dstr); \
    osDbg_printf(DBG_ALERT, __VA_ARGS__);\
	osDbg_printf(DBG_ALERT, "\n");\
} while(0);\


#define logCrit(...) \
do {\
	if(osDbg_isBypass(DBG_CRIT, LM_ALL)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Critical] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_CRIT, dstr); \
    osDbg_printf(DBG_CRIT, __VA_ARGS__);\
    osDbg_printf(DBG_CRIT, "\n");\
} while(0);\


#define logError(...) \
do {\
    if(osDbg_isBypass(DBG_ERROR, LM_ALL)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Error] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_ERROR, dstr); \
    osDbg_printf(DBG_ERROR, __VA_ARGS__);\
    osDbg_printf(DBG_ERROR, "\n");\
} while(0);\


#define logWarning(...) \
do {\
    if(osDbg_isBypass(DBG_WARNING, LM_ALL)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Warning] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_WARNING, dstr); \
    osDbg_printf(DBG_WARNING, __VA_ARGS__);\
    osDbg_printf(DBG_WARNING, "\n");\
} while(0);\


#define logNotice(...) \
do {\
    if(osDbg_isBypass(DBG_NOTICE, LM_ALL)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Notice] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_NOTICE, dstr); \
    osDbg_printf(DBG_NOTICE, __VA_ARGS__);\
    osDbg_printf(DBG_NOTICE, "\n");\
} while(0);\


#define logInfo(...) \
do {\
    if(osDbg_isBypass(DBG_INFO, LM_ALL)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Info] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_INFO, dstr); \
    osDbg_printf(DBG_INFO, __VA_ARGS__);\
    osDbg_printf(DBG_INFO, "\n");\
} while(0);\


#define debug(...) \
do {\
    if(osDbg_isBypass(DBG_DEBUG, LM_ALL)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_DEBUG, dstr); \
    osDbg_printf(DBG_DEBUG, __VA_ARGS__);\
    osDbg_printf(DBG_DEBUG, "\n");\
} while(0);\


#define mlogEmerg(module, ...) \
do {\
	logEmerg(...);	\
} while(0);\


#define mlogAlert(module, ...) \
do {\
    if(osDbg_isBypass(DBG_ALERT, module)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Alert] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_ALERT, dstr); \
    osDbg_printf(DBG_ALERT, __VA_ARGS__);\
    osDbg_printf(DBG_ALERT, "\n");\
} while(0);\


#define mlogCrit(module, ...) \
do {\
    if(osDbg_isBypass(DBG_CRIT, module)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Critical] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_CRIT, dstr); \
    osDbg_printf(DBG_CRIT, __VA_ARGS__);\
    osDbg_printf(DBG_CRIT, "\n");\
} while(0);\


#define mlogError(module, ...) \
do {\
    if(osDbg_isBypass(DBG_ERROR, module)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Error] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_ERROR, dstr); \
    osDbg_printf(DBG_ERROR, __VA_ARGS__);\
    osDbg_printf(DBG_ERROR, "\n");\
} while(0);\


#define mlogWarning(module, ...) \
do {\
    if(osDbg_isBypass(DBG_WARNING, module)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Warning] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_WARNING, dstr); \
    osDbg_printf(DBG_WARNING, __VA_ARGS__);\
    osDbg_printf(DBG_WARNING, "\n");\
} while(0);\


#define mlogNotice(module, ...) \
do {\
    if(osDbg_isBypass(DBG_NOTICE, module)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Notice] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_NOTICE, dstr); \
    osDbg_printf(DBG_NOTICE, __VA_ARGS__);\
    osDbg_printf(DBG_NOTICE, "\n");\
} while(0);\


#define mlogInfo(module, ...) \
do {\
    if(osDbg_isBypass(DBG_INFO, module)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d, Info] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_INFO, dstr); \
    osDbg_printf(DBG_INFO, __VA_ARGS__);\
    osDbg_printf(DBG_INFO, "\n");\
} while(0);\


#define mdebug(module, ...) \
do {\
    if(osDbg_isBypass(DBG_DEBUG, module)) \
        continue;   \
    struct timespec tp; \
    struct tm d; \
    clock_gettime(CLOCK_REALTIME, &tp); \
    pthread_t tid = pthread_self(); \
    gmtime_r(&tp.tv_sec, &d); \
    char dstr[200];  \
    char filename[]=__FILE__;   \
    sprintf(dstr, "%d/%02d/%02d %02d:%02d:%02d.%-6ld:0x%lx[%s:%s:%d] ", d.tm_year+1900, d.tm_mon+1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec, tp.tv_nsec/1000, tid, filename, __func__, __LINE__);  \
    osDbg_printf(DBG_DEBUG, dstr); \
    osDbg_printf(DBG_DEBUG, __VA_ARGS__);\
    osDbg_printf(DBG_DEBUG, "\n");\
} while(0);\


#define DEBUG_BEGIN	debug("entering...")
#define DEBUG_END	debug("exit.")

#define mDEBUG_BEGIN(module)	mdebug(module, "entering...")
#define mDEBUG_END(module) 		mdebug(module, "exit.");


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

void osDbg_init(osDbgLevel_e level, enum osDbg_flags flags);
void osDbg_mInit(osLogModule_e module, osDbgLevel_e level);
void osDbg_close(void);
int  osDbg_setLogfile(const char *name);
bool osDbg_isBypass(osDbgLevel_e level, osLogModule_e module);
void osDbg_setHandler(osDbgPrint_h *ph, void *arg);
void osDbg_printf(int level, const char *fmt, ...);
const char *osDbg_getLevelStr(int level);
void osDbg_printStr(const char* str, size_t strlen);

#endif
