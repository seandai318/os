/**
 * @file osString.h  Interfaces for string operations
 *
 * Copyright (C) 2019 InterLogic
 */


#ifndef _OS_STRING_H
#define _OS_STRING_H

#include <stdarg.h>



/* Character functions */
uint8_t osStr_chHex2Bin(char ch);


/* String functions */
int  osStrHex2Bin(uint8_t *hex, size_t len, const char *str);
void osStrCopyMaxLen(char *dst, const char *src, size_t n);
int  osStrDup(char **dst, const char *src);
int  osStrCmp(const char *s1, const char *s2);
int osStrCaseCmp(const char *s1, const char *s2);
size_t osStrLen(const char *s);
const char* osStrError(int errnum, char *buf, size_t sz);


/**
 * Check if string is set
 *
 * @param s Zero-terminated string
 *
 * @return true if set, false if not set
 */
static inline bool osStrIsSet(const char *s)
{
	return s && s[0] != '\0';
}




#endif
