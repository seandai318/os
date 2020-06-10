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


struct osMBuf;


/** Defines a pointer-length string type */
typedef struct osPointerLen {
	const char *p;  	// pointer to string, const shall be dropped
	size_t l;       	// length of string
} osPointerLen_t;


typedef struct osPLinfo {
	osPointerLen_t* pPL;
	bool isCase;
} osPLinfo_t;


typedef struct osDPointerLen {
    char *p;      		// pointer to string, the string shall be deallocated after using up this data structure
    size_t l;           // length of string
} osDPointerLen_t;



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
uint32_t osPL_hexstr2u32(const osPointerLen_t *pl);
uint64_t osPL_str2u64(const osPointerLen_t *pl);
uint64_t osPL_hexstr2u64(const osPointerLen_t *pl);
double osPL_str2float(const osPointerLen_t *pl);
//modify the digits in pl, the pl->l is not changed
int osPL_modifyu32(osPointerLen_t* pl, uint32_t n);
bool osPL_isset(const osPointerLen_t *pl);
int osPL_strcpy(const osPointerLen_t *pl, char *str, size_t size);
//copy a content of srcPL to dstPL, the len of srcPL must be smaller than or equal to dstPL
int osPL_plcpy(osPointerLen_t* dstPL, osPointerLen_t* srcPL);
int osPL_strdup(char **dst, const osPointerLen_t *src);
int osDPL_dup(osDPointerLen_t *dst, const osPointerLen_t *src);
osPointerLen_t osPL_clone(const osPointerLen_t* pl);
//osPointerLen_t osPL_cloneRef(const osPointerLen_t* pl);
int osPL_strcmp(const osPointerLen_t *pl, const char *str);
int osPL_strcasecmp(const osPointerLen_t *pl, const char *str);
int osPL_cmp(const osPointerLen_t *pl1, const osPointerLen_t *pl2);
int osPL_casecmp(const osPointerLen_t *pl1, const osPointerLen_t *pl2);
const char *osPL_findchar(const osPointerLen_t *pl, char c, size_t* pos);
const char* osPL_findStr(const osPointerLen_t *pl, const char* pattern, size_t patternLen);
void osPL_trimLWS(osPointerLen_t* pl, bool isTrimTop, bool isTrimBottom);
//split a string into 2, based on the first match of the splitter
void osPL_split(osPointerLen_t* srcPL, char splitter, osPointerLen_t* sub1, osPointerLen_t* sub2);

void osDPL_dealloc(osDPointerLen_t *pl);

void osVPL_init(osVPointerLen_t* pl, bool isVPLDynamic);
void osVPL_set(osVPointerLen_t* pl, void* p, size_t l, bool isPDynamic);
void osVPL_setStr(osVPointerLen_t *pl, const char *str, size_t len, bool isPDynamic);
void osVPL_free(osVPointerLen_t* pl);

/** Advance pl position/length by +/- N bytes */
static inline void osPL_advance(osPointerLen_t *pl, ssize_t n)
{
	pl->p += n;
	pl->l -= n;
}



#endif
