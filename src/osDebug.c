/**
 * @file osDebug.c  Debug printing
 *
 * Copyright (C) 2019 InterLogic
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include "osTypes.h"
#include "osPrintf.h"
#include "osDebug.h"


/** Debug configuration */
static struct osDbg {
	int level;             /**< Current debug level    */
	osDbg_flags_e flags;  /**< Debug flags            */
	osDbgPrint_h *ph;       /**< Optional print handler */
	void *arg;             /**< Handler argument       */
	FILE *f;               /**< Logfile                */
	pthread_mutex_t mutex; /**< Thread locking         */
} osDbgInfo = {
	DBG_DEBUG,
	DBG_ANSI,
	NULL,
	NULL,
	NULL,
	PTHREAD_MUTEX_INITIALIZER,
};


static osDbgLevel_e osDbgMLevel[LM_ALL];



static inline void osDbg_lock(void)
{
	pthread_mutex_lock(&osDbgInfo.mutex);
}


static inline void osDbg_unlock(void)
{
	pthread_mutex_unlock(&osDbgInfo.mutex);
}


/**
 * Initialise debug printing
 *
 * @param level Debug level
 * @param flags Debug flags
 */
void osDbg_init(osDbgLevel_e level, enum osDbg_flags flags)
{
	osDbg_lock();
	for(int i=0; i<LM_ALL; i++)
	{
		osDbgMLevel[i] = DBG_EMERG;
	}
	osDbgMLevel[LM_ALL] = level;
	osDbgInfo.level = level;
	osDbgInfo.flags = flags;
	osDbg_unlock();
}


void osDbg_mInit(osLogModule_e module, osDbgLevel_e level)
{
	if(module > LM_ALL || level > DBG_DEBUG)
	{
		return;
	}

    osDbg_lock();
	osDbgMLevel[module] = level;
    osDbg_unlock();
}


/**
 * Close debugging
 */
void osDbg_close(void)
{
	osDbg_lock();
	if (osDbgInfo.f) {
		(void)fclose(osDbgInfo.f);
		osDbgInfo.f = NULL;
	}
	osDbg_unlock();
}


/**
 * Set debug logfile
 *
 * @param name Name of the logfile, NULL to close
 *
 * @return 0 if success, otherwise errorcode
 */
int osDbg_setLogfile(const char *name)
{
	time_t t;

	osDbg_close();

	if (!name)
		return 0;

	osDbg_lock();
	osDbgInfo.f = fopen(name, "a+");
	osDbg_unlock();
	if (!osDbgInfo.f)
	{
		return errno;
	}

	(void)time(&t);
	(void)osPrintf(osDbgInfo.f, "\n===== Log Started: %s", asctime(gmtime(&t)));
	(void)fflush(osDbgInfo.f);

	return 0;
}


bool osDbg_isBypass(osDbgLevel_e level, osLogModule_e module)
{
	if(level > DBG_DEBUG || module > LM_ALL)
	{
		return true;
	}

	if(level > osDbgMLevel[module]) 
	{
        return true;
	}

	return false;
}


/**
 * Set optional debug print handler
 *
 * @param ph  Print handler
 * @param arg Handler argument
 */
void osDbg_setHandler(osDbgPrint_h *ph, void *arg)
{
	osDbgInfo.ph  = ph;
	osDbgInfo.arg = arg;
}


/* NOTE: This function should not allocate memory */
static void osDbg_onStdout(int level, const char *fmt, va_list ap)
{
	if (level > osDbgInfo.level)
		return;

	/* Print handler? */
	if (osDbgInfo.ph)
		return;

	osDbg_lock();

	if (osDbgInfo.flags & DBG_ANSI)
	{
		switch (level)
		{
			case DBG_EMERG:
			case DBG_ALERT:
			case DBG_CRIT:
			case DBG_ERROR:
			case DBG_WARNING:
				(void)osPrintf(stdout, "\x1b[31m"); /* Red */
				break;
			case DBG_NOTICE:
				(void)osPrintf(stdout, "\x1b[33m"); /* Yellow */
				break;
			case DBG_INFO:
				(void)osPrintf(stdout, "\x1b[32m"); /* Green */
				break;
			default:
				break;
		}
	}

	(void)osPrintf_onStdout(fmt, ap);

	if (osDbgInfo.flags & DBG_ANSI && level < DBG_DEBUG)
	{
		(void)osPrintf(stdout, "\x1b[;m");
	}

	osDbg_unlock();
}


/* Formatted output to print handler and/or logfile */
static void osDbg_onFile(int level, const char *fmt, va_list ap)
{
	char buf[256];
	int len;

	if (level > osDbgInfo.level)
		return;

	if (!osDbgInfo.ph && !osDbgInfo.f)
		return;

	osDbg_lock();

	len = osPrintf_onBuffer(buf, sizeof(buf), fmt, ap);
	if (len <= 0)
	{
		goto out;
	}

	/* Print handler? */
	if (osDbgInfo.ph)
	{
		osDbgInfo.ph(level, buf, len, osDbgInfo.arg);
	}

	/* Output to file */
	if (osDbgInfo.f) 
	{
		if (fwrite(buf, 1, len, osDbgInfo.f) > 0)
			(void)fflush(osDbgInfo.f);
	}

 out:
	osDbg_unlock();
}


/**
 * Print a formatted debug message
 *
 * @param level Debug level
 * @param fmt   Formatted string
 */
void osDbg_printf(int level, const char *fmt, ...)
{
	//only print the logs that has more severe level than been set
	if(level > osDbgInfo.level)
	{
		return;
	}

	va_list ap;

	va_start(ap, fmt);
	osDbg_onStdout(level, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	osDbg_onFile(level, fmt, ap);
	va_end(ap);
}



/**
 * Get the name of the debug level
 *
 * @param level Debug level
 *
 * @return String with debug level name
 */
const char *osDbg_getLevelStr(int level)
{
	switch (level) {

	case DBG_EMERG:   return "EMERGENCY";
	case DBG_ALERT:   return "ALERT";
	case DBG_CRIT:    return "CRITICAL";
	case DBG_ERROR:   return "ERROR";
	case DBG_WARNING: return "WARNING";
	case DBG_NOTICE:  return "NOTICE";
	case DBG_INFO:    return "INFO";
	case DBG_DEBUG:   return "DEBUG";
	default:          return "???";
	}
}


void osDbg_printStr(const char* str, size_t strlen)
{
	if(!str)
	{
		return;
	}

	if(strlen == 0)
	{
		printf("%s\n", str);
	}
	else
	{
		char* pStr = malloc(strlen+1);
		pStr[strlen] = '\0';

		printf("%s\n", pStr);
		
		free(pStr);
	}
}
