/********************************************************
 * Copyright (C) 2019, 2020 Sean Dai
 *
 * @file osPL.c  Pointer-length functions
 * Adapted from pl.c @Creytiv.com
 ********************************************************
 *@file pl.c  Pointer-length functions
 *
 * Copyright (C) 2010 Creytiv.com
 ********************************************************/

#include <ctype.h>
#include <sys/types.h>
#define __EXTENSIONS__ 1
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include "osTypes.h"
#include "osMemory.h"
#include "osMBuf.h"
#include "osPL.h"
#include "osMisc.h"
#include "osDebug.h"


/** Pointer-length NULL initialiser */
const osPointerLen_t pl_null = {NULL, 0};


void osPL_init(osPointerLen_t* pl)
{
	if(!pl)
	{
		return;
	}
	pl->p= NULL;
	pl->l = 0;
}


/**
 * Initialise a pointer-length object from a NULL-terminated string
 *
 * @param pl  Pointer-length object to be initialised
 * @param str NULL-terminated string
 */
void osPL_setStr(osPointerLen_t *pl, const char *str, size_t len)
{
    if (!pl || !str)
    {
        return;
    }

    pl->p = str;
    pl->l = !len ? strlen(str) : len;
}


/**
 * Initialise a pointer-length object from current position and
 * length of a memory buffer
 *
 * @param pl  Pointer-length object to be initialised
 * @param mb  Memory buffer
 */
void osPL_setMBuf(osPointerLen_t *pl, const struct osMBuf *mb)
{
	if (!pl || !mb)
	{
		return;
	}

	pl->p = (char *)osMBuf_getCurBuf(mb);
	pl->l = osMBuf_getRemaining(mb);
}


//simply set the p and l to a osPointerLen, no memory allocation is performed.
void osPL_set(osPointerLen_t* pl, void* p, size_t l)
{
	if(!pl)
	{
		return;
	}

	pl->p = p;
	pl->l = l;
}


/**
 * Convert a pointer-length object to a numeric 32-bit value
 *
 * @param pl Pointer-length object
 *
 * @return 32-bit value
 */
uint32_t osPL_str2u32(const osPointerLen_t *pl)
{
	uint32_t value=0, multiple=1;
	const char *ptr;

	if (!pl || !pl->p)
	{
		return 0;
	}

	ptr = &pl->p[pl->l];
	while (ptr > pl->p) 
	{
		const int c = *--ptr - '0';
		if (c > 9 || c < 0)
		{
			return 0;
		}

		value += multiple * c;
		multiple *= 10;
	}

	return value;
}


osStatus_e osPL_convertStr2u32(const osPointerLen_t *pl, uint32_t* pValue)
{
	if(!pl || !pl->p)
    {
		logError("null pointer, pl=%p, pl->p=%p.", pl, pl->p);		
        return OS_ERROR_NULL_POINTER;
    }

	*pValue = 0;
    uint32_t multiple=1;
    const char *ptr;

    ptr = &pl->p[pl->l];
    while (ptr > pl->p)
    {
        const int c = *--ptr - '0';
        if (c > 9 || c < 0)
        {
            return OS_ERROR_INVALID_VALUE;
        }

        *pValue += multiple * c;
        multiple *= 10;
    }

    return OS_STATUS_OK;
}

/**
 * Convert a hex pointer-length object to a numeric 32-bit value
 *
 * @param pl Pointer-length object
 *
 * @return 32-bit value
 */
uint32_t osPL_hexstr2u32(const osPointerLen_t *pl)
{
	uint32_t value=0, multiple=1;
	const char *ptr;

	if (!pl || !pl->p)
		return 0;

	ptr = &pl->p[pl->l];
	while (ptr > pl->p)
	{
		const char ch = *--ptr;
		uint8_t c;

		if ('0' <= ch && ch <= '9')
		{
			c = ch - '0';
		}
		else if ('A' <= ch && ch <= 'F')
		{
			c = ch - 'A' + 10;
		}
		else if ('a' <= ch && ch <= 'f')
		{
			c = ch - 'a' + 10;
		}
		else
		{
			return 0;
		}

		value += multiple * c;
		multiple *= 16;
	}

	return value;
}


/**
 * Convert a pointer-length object to a numeric 64-bit value
 *
 * @param pl Pointer-length object
 *
 * @return 64-bit value
 */
uint64_t osPL_str2u64(const osPointerLen_t *pl)
{
	uint64_t value=0, multiple=1;
	const char *ptr;

	if (!pl || !pl->p)
	{
		return 0;
	}

	ptr = &pl->p[pl->l];
	while (ptr > pl->p) 
	{
		const int c = *--ptr - '0';
		if (c > 9 || c < 0)
		{
			return 0;
		}

		value += multiple * c;
		multiple *= 10;
	}

	return value;
}


osStatus_e osPL_convertStr2u64(const osPointerLen_t *pl, uint64_t* pValue)
{
    if(!pl || !pl->p)
    {
        logError("null pointer, pl=%p, pl->p=%p.", pl, pl->p);
        return OS_ERROR_NULL_POINTER;
    }

    *pValue = 0;
    uint32_t multiple=1;
    const char *ptr;

    ptr = &pl->p[pl->l];
    while (ptr > pl->p)
    {
        const int c = *--ptr - '0';
        if (c > 9 || c < 0)
        {
            return OS_ERROR_INVALID_VALUE;
        }

        *pValue += multiple * c;
        multiple *= 10;
    }

    return OS_STATUS_OK;
}


/**
 * Convert a hex pointer-length object to a numeric 64-bit value
 *
 * @param pl Pointer-length object
 *
 * @return 64-bit value
 */
uint64_t osPL_hexstr2u64(const osPointerLen_t *pl)
{
	uint64_t value=0, multiple=1;
	const char *ptr;

	if (!pl || !pl->p)
		return 0;

	ptr = &pl->p[pl->l];
	while (ptr > pl->p)
	{
		const char ch = *--ptr;
		uint8_t c;

		if ('0' <= ch && ch <= '9')
		{
			c = ch - '0';
		}
		else if ('A' <= ch && ch <= 'F')
		{
			c = ch - 'A' + 10;
		}
		else if ('a' <= ch && ch <= 'f')
		{
			c = ch - 'a' + 10;
		}
		else
		{
			return 0;
		}

		value += multiple * c;
		multiple *= 16;
	}

	return value;
}


/**
 * Convert a pointer-length object to floating point representation.
 * Both positive and negative numbers are supported, a string with a
 * minus sign ('-') is treated as a negative number.
 *
 * @param pl Pointer-length object
 *
 * @return Double value
 */
double osPL_str2float(const osPointerLen_t *pl)
{
	double value=0, multiple=1;
	const char *ptr;
	bool neg = false;

	if (!pl || !pl->p)
	{
		return 0;
	}

	ptr = &pl->p[pl->l];

	while (ptr > pl->p)
	{
		const char ch = *--ptr;

		if ('0' <= ch && ch <= '9') 
		{
			value += multiple * (ch - '0');
			multiple *= 10;
		}
		else if (ch == '.') 
		{
			value /= multiple;
			multiple = 1;
		}
		else if (ch == '-' && ptr == pl->p) 
		{
			neg = true;
		}
		else 
		{
			return 0;
		}
	}

	return neg ? -value : value;
}


//replace the number pointered by pl with n.  The value n must be smaller than the original number in pl
int osPL_modifyu32(osPointerLen_t* pl, uint32_t n)
{
	if(!pl)
	{
		return -1;
	}

	if(pl->p[0] < '0' || pl->p[0] > '9' || pl->p[pl->l-1] < '0' || pl->p[pl->l-1] > '9')
	{
		return -1;
	}

	char newStr[10]={' '};
	for (int i=0; i<pl->l; i++)
	{
		newStr[i] = n % 10 + '0';
		n = n / 10;
		if(n == 0)
		{
			break;
		}
	}

	if(n > 0)
	{
		return -1;
	}

	for(int i=pl->l-1, j=0; i>=0; i--, j++)
	{
		*((char*)pl->p + i) = newStr[j];
	}
}

/**
 * Check if pointer-length object is set
 *
 * @param pl Pointer-length object
 *
 * @return true if set, false if not set
 */
bool osPL_isset(const osPointerLen_t *pl)
{
	return pl ? pl->p && pl->l : false;
}


/**
 * Copy a pointer-length object to a NULL-terminated string
 *
 * @param pl   Pointer-length object
 * @param str  Buffer for NULL-terminated string
 * @param size Size of buffer
 *
 * @return 0 if success, otherwise errorcode
 */
int osPL_strcpy(const osPointerLen_t *pl, char *str, size_t size)
{
	size_t len;

	if (!pl || !pl->p || !str || !size)
	{
		return EINVAL;
	}

	len = min(pl->l, size-1);

	memcpy(str, pl->p, len);
	str[len] = '\0';

	return 0;
}


//copy a content of srcPL to dstPL, dstPL is expected to already have memory allocated for dstPL->p, the len of srcPL must be smaller than or equal to dstPL
int osPL_plcpy(osPointerLen_t* dstPL, osPointerLen_t* srcPL)
{
	if(!dstPL || !srcPL)
	{
		return EINVAL;
	}

	if(srcPL->l > dstPL->l)
	{
		return EINVAL;
	}

	memcpy((void*)dstPL->p, srcPL->p, srcPL->l);

	return 0;
}


/**
 * Duplicate a pointer-length object to a NULL-terminated string
 *
 * @param dst Pointer to destination string (set on return)
 * @param src Source pointer-length object
 *
 * @return 0 if success, otherwise errorcode
 */
int osPL_strdup(char **dst, const osPointerLen_t *src)
{
	char *p;

	if (!dst || !src || !src->p)
	{
		return EINVAL;
	}

	p = osmalloc_r(src->l+1, NULL);
	if (!p)
	{
		return ENOMEM;
	}

	memcpy(p, src->p, src->l);
	p[src->l] = '\0';

	*dst = p;

	return 0;
}


/**
 * Duplicate a pointer-length object to a new pointer-length object
 *
 * @param dst Destination pointer-length object (set on return)
 * @param src Source pointer-length object
 *
 * @return 0 if success, otherwise errorcode
 */
int osDPL_dup(osDPointerLen_t *dst, const osPointerLen_t *src)
{
	char *p;

	if (!dst || !src || !src->p)
	{	
		return EINVAL;
	}

	p = osmalloc_r(src->l, NULL);
	if (!p)
	{
		return ENOMEM;
	}

	memcpy(p, src->p, src->l);

	dst->p = p;
	dst->l = src->l;

	return 0;
}


/**
 * Compare a pointer-length object with a NULL-terminated string
 * (case-sensitive)
 *
 * @param pl  Pointer-length object
 * @param str NULL-terminated string
 *
 * @return 0 if match, otherwise errorcode
 */
int osPL_strcmp(const osPointerLen_t *pl, const char *str)
{
	osPointerLen_t s;

	if (!pl || !str)
	{
		return EINVAL;
	}

	osPL_setStr(&s, str, 0);

	return osPL_cmp(pl, &s);
}


/**
 * Compare a pointer-length object with a NULL-terminated string
 * (case-insensitive)
 *
 * @param pl  Pointer-length object
 * @param str NULL-terminated string
 *
 * @return 0 if match, otherwise errorcode
 */
int osPL_strcasecmp(const osPointerLen_t *pl, const char *str)
{
	osPointerLen_t s;

	if (!pl || !str)
	{
		return EINVAL;
	}

	osPL_setStr(&s, str, 0);

	return osPL_casecmp(pl, &s);
}


/**
 * Compare two pointer-length objects (case-sensitive)
 *
 * @param pl1  First pointer-length object
 * @param pl2  Second pointer-length object
 *
 * @return 0 if match, otherwise errorcode
 */
int osPL_cmp(const osPointerLen_t *pl1, const osPointerLen_t *pl2)
{
	if (!pl1 || !pl2)
	{
		return EINVAL;
	}

	/* Different length -> no match */
	if (pl1->l != pl2->l)
	{
		return EINVAL;
	}

	/* Zero-length strings are always identical */
	if (pl1->l == 0)
	{
		return 0;
	}

	/* The two pl's are the same */
	if (pl1 == pl2)
	{
		return 0;
	}

	/* Two different pl's pointing to same string */
	if (pl1->p == pl2->p)
	{
		return 0;
	}

	return 0 == memcmp(pl1->p, pl2->p, pl1->l) ? 0 : EINVAL;
}



/**
 * Compare two pointer-length objects (case-insensitive)
 *
 * @param pl1  First pointer-length object
 * @param pl2  Second pointer-length object
 *
 * @return 0 if match, otherwise errorcode
 */
int osPL_casecmp(const osPointerLen_t *pl1, const osPointerLen_t *pl2)
{
	if (!pl1 || !pl2)
	{
		return EINVAL;
	}

	/* Different length -> no match */
	if (pl1->l != pl2->l)
	{
		return EINVAL;
	}

	/* Zero-length strings are always identical */
	if (pl1->l == 0)
	{
		return 0;
	}

	/* The two pl's are the same */
	if (pl1 == pl2)
	{
		return 0;
	}

	/* Two different pl's pointing to same string */
	if (pl1->p == pl2->p)
	{
		return 0;
	}

	return 0 == strncasecmp(pl1->p, pl2->p, pl1->l) ? 0 : EINVAL;
}


osPointerLen_t osPL_clone(const osPointerLen_t* pl)
{
	osPointerLen_t clonePL;

	clonePL.p = pl->p;
	clonePL.l = pl->l;

	return clonePL;
}


/**
 * Locate character in pointer-length string
 *
 * @param pl  Pointer-length string
 * @param c   Character to locate
 *
 * @return Pointer to first char if found, otherwise NULL
 */
const char* osPL_findchar(const osPointerLen_t *pl, char c, size_t* pos)
{
	const char *p, *end;

	if (!pl)
	{
		return NULL;
	}

	*pos=0;
	end = pl->p + pl->l;
	for (p = pl->p; p < end; p++, (*pos)++) 
	{
		if (*p == c)
		{
			return p;
		}
	}

	return NULL;
}


const char* osPL_findStr(const osPointerLen_t *pl, const char* pattern, size_t patternLen)
{
    if (!pl || !pattern)
    {
        return NULL;
    }

	osPointerLen_t workPL = osPL_clone(pl);

	patternLen = !patternLen ? strlen(pattern) : patternLen;

	while (true)
	{
		size_t pos=0;
		const char* p = osPL_findchar(&workPL, pattern[0], &pos);
		if(!p)
		{
			return NULL;
		}
			
		osPL_advance(&workPL, pos);
	
		if(workPL.l < patternLen)
		{
			return NULL;
		}

		if(memcmp(p, pattern, patternLen) == 0)
		{
			return p;
		}

		if(workPL.l >1)
		{
			osPL_advance(&workPL, 1);
		}
		else
		{
			return NULL;
		}
	}

	return NULL;
}


void osPL_trimLWS(osPointerLen_t* pl, bool isTrimTop, bool isTrimBottom)
{
	if (!pl || pl->l == 0)
	{
		return;
	}

	if(isTrimTop)
	{
		for(int i=0; i<pl->l; i++)
		{
			if(!OS_IS_LWS(pl->p[i]))
			{
				pl->p = &pl->p[i];
				pl->l -=  i;
				break; 
			}
		}
	}
	
	if(isTrimBottom)
	{
		for(int i=pl->l - 1; i>=0; i--)
		{
			if(!OS_IS_LWS(pl->p[i]))	
			{
				pl->l = i + 1;
				break;
			}
		}
	}
}


void osDPL_dealloc(osDPointerLen_t* pl)
{
    if(!pl)
    {
        return;
    }

    if(pl->p && pl->l)
    {
        pl->p = osfree((void*) pl->p);
        pl->l=0;
    }
}


void osVPL_init(osVPointerLen_t* pl, bool isVPLDynamic)
{
    if(!pl)
    {
        return;
    }
    pl->pl.p= NULL;
    pl->pl.l = 0;
	pl->isVPLDynamic = isVPLDynamic;
}


void osVPL_set(osVPointerLen_t* pl, void* p, size_t l, bool isPDynamic)
{
    if(!pl)
    {
        return;
    }

    pl->pl.p = p;
    pl->pl.l = l;

	pl->isPDynamic = isPDynamic;
}


void osVPL_setStr(osVPointerLen_t *pl, const char *str, size_t len, bool isPDynamic)
{
    if (!pl || !str)
    {
        return;
    }

    pl->pl.p = str;
    pl->pl.l = !len ? strlen(str) : len;

	pl->isPDynamic = isPDynamic;
}


void osVPL_setPL(osVPointerLen_t *pl, const osPointerLen_t* origPl, bool isPDynamic)
{
	if(!pl || !origPl)
	{
		return;
	}

	pl->pl = *origPl;
	pl->isPDynamic = isPDynamic;
}


void osVPL_free(osVPointerLen_t* vpl)
{
	if(!vpl)
	{
		return;
	}

	if(vpl->isPDynamic)
	{
		osfree((void*) vpl->pl.p);
		vpl->pl.l = 0;
	}

	if(vpl->isVPLDynamic)
	{
		osfree(vpl);
	}
}	


//split a string into 2, based on the first match of the splitter
void osPL_split(osPointerLen_t* srcPL, char splitter, osPointerLen_t* sub1, osPointerLen_t* sub2)
{
	if(!srcPL || !sub1 || !sub2)
	{
		return;
	}

	sub1 = srcPL;
	sub2->l = 0;
	for(int i=0; i<srcPL->l; i++)
	{
		if(srcPL->p[i] == splitter)
		{
			sub1->l = i++;
			if(i < srcPL->l)
			{
				sub2->p = &srcPL->p[i];
				sub2->l = srcPL->l - i;
			}
			break;
		}
	}
}
