/**
 * @file osPL.h  Interface to formatted text functions
 *
 * Copyright (C) 2019 InterLogic
 */


#ifndef _OS_PL_H
#define _OS_PL_H

#include <stdarg.h>
#include <stdio.h>

#include "osTypes.h"
#include "osDebug.h"



struct osMBuf;


/* Note:
 * there are three similar pl defined: osPointerLen_t, osDPointerLen_t, and osVPointerLen_t.  The following is the suggested usage.
 * osPointerLen_t:  purely referencing a memory, when a file/module is done using this data structure, no need to free it
 * osDPointerLen_t: This can be used when a file/module receives a osPointerLen_t, but does not know how the receiving osPointerLen_t is
 *                  created by the sender (local variable? referring to other bulk memory that may be freed any time? etc.).  In this case,
 *                  if the receiving file/module wants to keep the data for a while, it can use this data structure.  it shall dynamically 
 *                  allocate pl->p, and free it when the data structure is done.
 * osVPointerLen_t: this data structure includes information like what parts are dynamically allocated.  This data structure can be used to pass a 
 *                  osPointerLen_t type mata from one module to another.  The receiving file/module knows if it needs to free the data structure 
 *                  when it is done.  The sending part would not free any memory.
 */

/** Defines a pointer-length string type */
typedef struct osPointerLen {
	const char *p;  	// pointer to string 
	size_t l;       	// length of string
} osPointerLen_t;


typedef struct osDPointerLen {
    char *p;      		// pointer to string, the string shall be deallocated after using up this data structure
    size_t l;           // length of string
} osDPointerLen_t;


//if passing this variable to another function, expect the receiving function to free the memmory.  If there is a interruption between creating of this data structure to the freeing of this data structure (for example, pass this data structure to a function, the receiving function stores it, then after receiving something from network, free this data structure), this data structure has to be either created dymanically or is a global variable
typedef struct osVPointerLen {
	osPointerLen_t pl;
	bool isPDynamic;	//if isPDynamic = true, pl.p is dynamic allocated, after using the data structure, pl.p shall be freed
	bool isVPLDynamic;	//if isVPLDynamic = true, osVPointerLen_t data structure is dynamic allocated, otherwise, static allocated
} osVPointerLen_t;


/** Initialise a pointer-length object from a constant string literal, like "this is test" or a NULL terminated string. */
#define osPL(s) {(s), strlen((s))-1}

/** Pointer-length Initializer */
#define osPL_INIT {NULL, 0}

extern const osPointerLen_t osPLnull;


void osPL_init(osPointerLen_t* pl);
void osPL_setStr(osPointerLen_t* pl, const char* str, size_t len);
void osPL_setMBuf(osPointerLen_t *pl, const struct osMBuf *mb);
//simply set the p and l to a osPointerLen, no memory allocation is performed.
void osPL_set(osPointerLen_t* pl, void* p, size_t l);
uint32_t osPL_str2u32(const osPointerLen_t *pl);
osStatus_e osPL_convertStr2u32(const osPointerLen_t *pl, uint32_t* pValue);
uint32_t osPL_hexstr2u32(const osPointerLen_t *pl);
uint64_t osPL_str2u64(const osPointerLen_t *pl);
osStatus_e osPL_convertStr2u64(const osPointerLen_t *pl, uint64_t* pValue, int* pDigits);
uint64_t osPL_hexstr2u64(const osPointerLen_t *pl);
double osPL_str2float(const osPointerLen_t *pl);
//modify the digits in pl, the pl->l is not changed
int osPL_modifyu32(osPointerLen_t* pl, uint32_t n);
//bool osPL_isset(const osPointerLen_t *pl);
int osPL_strcpy(const osPointerLen_t *pl, char *str, size_t size);
//copy a content of srcPL to dstPL, the len of srcPL must be smaller than or equal to dstPL
int osPL_plcpy(osPointerLen_t* dstPL, osPointerLen_t* srcPL);
int osPL_strdup(char **dst, const osPointerLen_t *src);
int osPL_str2PLdup(osPointerLen_t* pDestPL, char* srcStr, int strlen);
int osDPL_dup(osDPointerLen_t *dst, const osPointerLen_t *src);
osPointerLen_t osPL_clone(const osPointerLen_t* pl);
//osPointerLen_t osPL_cloneRef(const osPointerLen_t* pl);
int osPL_strplcmp(const char *str, int len, const osPointerLen_t *pl, bool isMatchLen);
int osPL_strcmp(const osPointerLen_t *pl, const char *str);
int osPL_strcasecmp(const osPointerLen_t *pl, const char *str);
int osPL_cmp(const osPointerLen_t *pl1, const osPointerLen_t *pl2);
int osPL_casecmp(const osPointerLen_t *pl1, const osPointerLen_t *pl2);
const char *osPL_findchar(const osPointerLen_t *pl, char c, size_t* pos);
const char* osPL_findStr(const osPointerLen_t *pl, const char* pattern, size_t patternLen);
void osPL_trimLWS(osPointerLen_t* pl, bool isTrimTop, bool isTrimBottom);
//split a string into 2, based on the first match of the splitter
void osPL_split(osPointerLen_t* srcPL, char splitter, osPointerLen_t* sub1, osPointerLen_t* sub2);
void osPL_compact(osPointerLen_t* pl);
void osDPL_dealloc(osDPointerLen_t *pl);

void osVPL_init(osVPointerLen_t* pl, bool isVPLDynamic);
void osVPL_set(osVPointerLen_t* pl, void* p, size_t l, bool isPDynamic);
void osVPL_setStr(osVPointerLen_t *pl, const char *str, size_t len, bool isPDynamic);
void osVPL_setPL(osVPointerLen_t *pl, const osPointerLen_t* origPl, bool isPDynamic);
void osVPL_copyPL(osVPointerLen_t *dest, osPointerLen_t *src);
void osVPL_copyVPL(osVPointerLen_t *dest, osVPointerLen_t *src);
//if isFreeAll == false, pl.p is not freed
void osVPL_free(osVPointerLen_t* pl, bool isFreeAll);



static inline bool osPL_isset(const osPointerLen_t *pl)
{
    return pl ? pl->p && pl->l : false;
}


static inline void osPL_setStr1(osPointerLen_t *pl, const char* str, size_t len)
{
	pl->p = str;
	pl->l = len;
}
	

/** Advance pl position/length by +/- N bytes */
static inline void osPL_advance(osPointerLen_t *pl, ssize_t n)
{
	pl->p += n;
	pl->l -= n;
}


static inline int osPL_shiftcpy(osPointerLen_t *dstPL, osPointerLen_t *srcPL, size_t n)
{
	if(!dstPL || !srcPL || n > srcPL->l)
	{
		return -1;
	}
	dstPL->p = &srcPL->p[n];
	dstPL->l = srcPL->l - n;

	return 0;
}


//dstPL will use the srcPL's p, but with starting and ending specified quote removed
static inline void osPL_removeQuote(osPointerLen_t* dstPL, const osPointerLen_t* srcPL, char quote)
{
	if(!dstPL || !srcPL)
	{
		return;
	}

	if(srcPL->p[0] == quote && srcPL->p[srcPL->l-1] == quote)
	{
		dstPL->p = &srcPL->p[1];
		dstPL->l = srcPL->l -2;
	}
	else
	{
		*dstPL = *srcPL;
	}
}


#endif
