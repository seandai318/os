/**
 * @file osTime.c  Time formatting
 *
 * Copyright (C) 2019 InterLogic
 */
#include <time.h>
#include "osTypes.h"
#include "osTime.h"


static const char *dayv[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static const char *monv[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


/**
 * Print Greenwich Mean Time
 *
 * @param pf Print function for output
 * @param ts Time in seconds since the Epoch or NULL for current time
 *
 * @return 0 if success, otherwise errorcode
 */
int osGMTtime(osPrintf_t *pf, void *ts)
{
	const struct tm *tm;
	time_t t;

	if (!ts) 
	{
		t  = time(NULL);
		ts = &t;
	}

	tm = gmtime(ts);
	if (!tm)
	{
		return EINVAL;
	}

	return osPrintf_handler(pf, "%s, %02u %s %u %02u:%02u:%02u GMT",
			  dayv[min((unsigned)tm->tm_wday, ARRAY_SIZE(dayv)-1)],
			  tm->tm_mday,
			  monv[min((unsigned)tm->tm_mon, ARRAY_SIZE(monv)-1)],
			  tm->tm_year + 1900,
			  tm->tm_hour, tm->tm_min, tm->tm_sec);
}


/**
 * Print the human readable time
 *
 * @param pf       Print function for output
 * @param seconds  Pointer to number of seconds
 *
 * @return 0 if success, otherwise errorcode
 */
int osHumanTime(osPrintf_t *pf, const uint32_t *seconds)
{
	/* max 136 years */
	const uint32_t sec  = *seconds%60;
	const uint32_t min  = *seconds/60%60;
	const uint32_t hrs  = *seconds/60/60%24;
	const uint32_t days = *seconds/60/60/24;
	int err = 0;

	if (days)
	{
		err |= osPrintf_handler(pf, "%u day%s ", days, 1==days?"":"s");
	}

	if (hrs) 
	{
		err |= osPrintf_handler(pf, "%u hour%s ", hrs, 1==hrs?"":"s");
	}

	if (min) 
	{
		err |= osPrintf_handler(pf, "%u min%s ", min, 1==min?"":"s");
	}

	if (sec) 
	{
		err |= osPrintf_handler(pf, "%u sec%s", sec, 1==sec?"":"s");
	}

	return err;
}
