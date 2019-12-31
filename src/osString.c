/**
 * @file osString.c String format functions
 *
 * Copyright (C) 2019 InterLogic
 */


#define _GNU_SOURCE 1
#define __EXTENSIONS__ 1
#include <string.h>
#include <strings.h>
#include "osTypes.h"
#include "osMemory.h"
#include "osString.h"



/**
 * Convert an ASCII hex character to binary format
 *
 * @param ch ASCII hex character
 *
 * @return Binary value
 */
uint8_t osStr_chHex2Bin(char ch)
{
    if ('0' <= ch && ch <= '9')
        return ch - '0';

    else if ('A' <= ch && ch <= 'F')
        return ch - 'A' + 10;

    else if ('a' <= ch && ch <= 'f')
        return ch - 'a' + 10;

    return 0;
}


/**
 * Convert a ascii hex string to binary format
 *
 * @param hex Destinatin binary buffer
 * @param len Length of binary buffer
 * @param str Source ascii string
 *
 * @return 0 if success, otherwise errorcode
 */
int osStrHex2Bin(uint8_t *hex, size_t len, const char *str)
{
	size_t i;

	if (!hex || !str || (strlen(str) != (2 * len)))
	{
		return EINVAL;
	}

	for (i=0; i<len*2; i+=2)
	{
		hex[i/2]  = osStr_chHex2Bin(str[i]) << 4;
		hex[i/2] += osStr_chHex2Bin(str[i+1]);
	}

	return 0;
}


/**
 * Copy a 0-terminated string with maximum length
 *
 * @param dst Destinatin string
 * @param src Source string
 * @param n   Maximum size of destination, including 0-terminator
 */
void osStrCopyMaxLen(char *dst, const char *src, size_t n)
{
	if (!dst || !src || !n)
	{
		return;
	}

	(void)strncpy(dst, src, n-1);
	dst[n-1] = '\0'; /* strncpy does not null terminate if overflow */
}


/**
 * Duplicate a 0-terminated string
 *
 * @param dst Pointer to destination string (set on return)
 * @param src Source string
 *
 * @return 0 if success, otherwise errorcode
 */
int osStrDup(char **dst, const char *src)
{
	char *p;
	size_t sz;

	if (!dst || !src)
	{
		return EINVAL;
	}

	sz = strlen(src) + 1;

	p = osMem_alloc(sz, NULL);
	if (!p)
	{
		return ENOMEM;
	}

	memcpy(p, src, sz);

	*dst = p;

	return 0;
}


/**
 * Compare two 0-terminated strings
 *
 * @param s1 First string
 * @param s2 Second string
 *
 * @return an integer less than, equal to, or greater than zero if s1 is found
 *         respectively, to be less than, to match, or be greater than s2
 */
int osStrCmp(const char *s1, const char *s2)
{
	if (!s1 || !s2)
	{
		return 1;
	}

	return strcmp(s1, s2);
}


/**
 * Compare two 0-terminated strings, ignoring case
 *
 * @param s1 First string
 * @param s2 Second string
 *
 * @return an integer less than, equal to, or greater than zero if s1 is found
 *         respectively, to be less than, to match, or be greater than s2
 */
int osStrCaseCmp(const char *s1, const char *s2)
{
	/* Same strings -> equal */
	if (s1 == s2)
	{
		return 0;
	}

	if (!s1 || !s2)
	{
		return 1;
	}

	return strcasecmp(s1, s2);
}


/**
 * Calculate the length of a string, safe version.
 *
 * @param s String
 *
 * @return Length of the string
 */
size_t osStrLen(const char *s)
{
	return s ? strlen(s) : 0;
}


/**
 * Look up an error message string corresponding to an error number.
 *
 * @param errnum Error Code
 * @param buf    Buffer for storing error message
 * @param sz     Buffer size
 *
 * @return Error message string
 */
const char *osStrError(int errnum, char *buf, size_t sz)
{
    const char *s;

    if (!buf || !sz)
	{
        return NULL;
	}

    buf[0] = '\0';
#ifdef HAVE_STRERROR_R

#ifdef __GLIBC__
    s = strerror_r(errnum, buf, sz);
#else
    (void)strerror_r(errnum, buf, sz);
    s = buf;
#endif

#elif defined (WIN32) & !defined (__MINGW32__)
    (void)strerror_s(buf, sz, errnum);
    s = buf;
#else
    /* fallback */
    (void)errnum;
    s = "unknown error";
#endif

    buf[sz - 1] = '\0';

    return s;
}

