/**
 * @file osTime.h  Interfaces for time format
 *
 * Copyright (C) 2019 InterLogic
 */


#ifndef _OS_TIME_H
#define _OS_TIME_H

#include <stdarg.h>
#include "osPrintf.h"


/* time */
int  osGMTtime(osPrintf_t *pf, void *ts);
int  osHumanTime(osPrintf_t *pf, const uint32_t *seconds);


#endif
