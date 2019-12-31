/**
 * @file osUtf8.h  Interfaces for utf8 eocoding/decoding
 *
 * Copyright (C) 2019 InterLogic
 */


#ifndef _OS_UTF8_H
#define _OS_UTF8_H

#include <stdarg.h>
#include "osPrintf.h"
#include "osPL.h"


/* unicode */
/* encode a string to a utf8 array and output via pf */
int osUtf8Encode(osPrintf_t *pf, const char *str);
/* decode a utf8 array into a string and output via pf */
int osUtf8Decode(osPrintf_t *pf, const osPointerLen_t *pl);
size_t osUtf8EncodeByteseq(char u[4], unsigned cp);


#endif
