/**
 * @file osRegex.h  Interfaces for regex
 *
 * Copyright (C) 2019 InterLogic
 */


#ifndef _OS_REGEX_H
#define _OS_REGEX_H

#include <stdarg.h>


/* Regular expressions 
 * Parse a string using basic regular expressions. Any number of matching
 * expressions can be given, and each match will be stored in a pointerLen_t
 * pointer-length type.  The pointerLen_t variables passed into the function
 * as a varList, like re_regex(buf, strlen(buf), "[0-9]+", pl1, pl2), where
 * pl1 and pl2 are the pointer variables of pointerLen_t type;
*/
int osRegex(const char *ptr, size_t len, const char *expr, ...);




#endif
