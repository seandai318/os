/**
 * @file osMBuf.h  Interface to memory buffers
 *
 * Copyright (C) 2019, InterLogic
 */


#ifndef _OS_MBUF_H
#define _OS_MBUF_H


#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include "osTypes.h"
#include "osPL.h"


#ifndef RELEASE
#define MBUF_DEBUG 0  /**< Mbuf debugging (0 or 1) */
#endif

#if MBUF_DEBUG
/** Check that mbuf position does not exceed end */
#define MBUF_CHECK_POS(mb)						\
	if ((mb) && (mb)->pos > (mb)->end) {				\
		BREAKPOINT;						\
	}
/** Check that mbuf end does not exceed size */
#define MBUF_CHECK_END(mb)						\
	if ((mb) && (mb)->end > (mb)->size) {				\
		BREAKPOINT;						\
	}
#else
#define MBUF_CHECK_POS(mb)
#define MBUF_CHECK_END(mb)
#endif

/** Defines a memory buffer, the buf has to be allocated via one of osMemory methods */
typedef struct osMBuf {
	uint8_t *buf;   /**< Buffer memory      */
	size_t size;    /**< Size of buffer     */
	size_t pos;     /**< current position in buffer */
	size_t end;     /**< End of buffer      */
} osMBuf_t;


//struct osPointerLen;
//struct osPrintf;

//function with _r are meant for simultaneous access by multiple threads

osMBuf_t *osMBuf_alloc(size_t size);
osMBuf_t *osMBuf_alloc_r(size_t size);
osMBuf_t *osMBuf_allocRef(osMBuf_t *mbr);
//pDupMBuf's initial pos =newPos
void osMBuf_allocRef1(osMBuf_t* pDupMBuf, osMBuf_t* pOrigMBuf, size_t newPos, size_t len);
//pDupMBuf's initial pos = 0
void osMBuf_allocRef2(osMBuf_t* pDupMBuf, osMBuf_t* pOrigMBuf, size_t newPos, size_t len);
void     osMBuf_init(osMBuf_t *mb);
void     osMBuf_reset(osMBuf_t *mb);
void 	 osMBuf_dealloc(osMBuf_t *mb);
int      osMBuf_realloc(osMBuf_t *mb, size_t size);
int      osMBuf_shift(osMBuf_t *mb, ssize_t shift);
int 	osMBuf_modifyStr(osMBuf_t *mb, char* str, size_t strLen, size_t pos);
int      osMBuf_writeBuf(osMBuf_t *mb, const uint8_t *buf, size_t size, bool isAdvancePos);
//copy the srcmb buf between the startPos and stopPos (the characters including startPos until stopPos-1) to destmb.
int 	osMBuf_writeBufRange(osMBuf_t *destmb, const osMBuf_t *srcmb, size_t startPos, size_t stopPos, bool isAdvancePos);
int 	osMBuf_setZero(osMBuf_t *mb, size_t size, bool isAdvancePos);
osMBuf_t* osMBuf_readFile(char* file, size_t initBufSize);
int      osMBuf_writeU8(osMBuf_t *mb, uint8_t v, bool isAdvancePos);
int      osMBuf_writeU16(osMBuf_t *mb, uint16_t v, bool isAdvancePos);
int      osMBuf_writeU32(osMBuf_t *mb, uint32_t v, bool isAdvancePos);
int      osMBuf_writeU64(osMBuf_t *mb, uint64_t v, bool isAdvancePos);
int 	osMBuf_writeU8Str(osMBuf_t *mb, uint8_t v, bool isAdvancePos);
int 	osMBuf_writeU16Str(osMBuf_t *mb, uint16_t v, bool isAdvancePos);
int 	osMBuf_writeU32Str(osMBuf_t *mb, uint32_t v, bool isAdvancePos);
int 	osMBuf_writeU64Str(osMBuf_t *mb, uint64_t v, bool isAdvancePos);
int      osMBuf_writeStr(osMBuf_t *mb, const char *str, bool isAdvancePos);
int      osMBuf_writePL(osMBuf_t *mb, const struct osPointerLen *pl, bool isAdvancePos);
int      osMBuf_writePLskipSection(osMBuf_t *mb, const struct osPointerLen *pl, const struct osPointerLen *skip, bool isAdvancePos);
//write until a pattern is meet, the writing ends after the last character of the pattern
int 	osMBuf_writeUntil(osMBuf_t *mb, const osPointerLen_t* src, const osPointerLen_t* pattern, bool isAdvancePos);
int      osMBuf_readBuf(osMBuf_t *mb, uint8_t *buf, size_t size);
uint8_t  osMBuf_readU8(osMBuf_t *mb);
uint16_t osMBuf_readU16(osMBuf_t *mb);
uint32_t osMBuf_readU32(osMBuf_t *mb);
uint64_t osMBuf_readU64(osMBuf_t *mb);
int      osMBuf_readStr(osMBuf_t *mb, char *str, size_t size);
int      osMBuf_strdup(osMBuf_t *mb, char **strp, size_t len);
int      osMBuf_fill(osMBuf_t *mb, uint8_t c, size_t n);
int      osMBuf_debug(FILE* pf, const osMBuf_t *mb);
void osMBuf_advance(osMBuf_t *mb, ssize_t n);
void osMBuf_setPos(osMBuf_t *mb, size_t pos);
ssize_t osMbuf_findMatch(osMBuf_t* pBuf, osPointerLen_t* pattern);
ssize_t osMbuf_findValue(osMBuf_t* pBuf, char tag1, char tag2, bool isExclSpace, osPointerLen_t* pValue);

/**
 * Get the buffer from the current position
 *
 * @param mb Memory buffer
 *
 * @return Current buffer
 */
static inline uint8_t *osMBuf_getCurBuf(const osMBuf_t *mb)
{
	return mb ? mb->buf + mb->pos : (uint8_t *)NULL;
}


/**
 * Get number of bytes left in a memory buffer, from current position to end
 *
 * @param mb Memory buffer
 *
 * @return Number of bytes left
 */
static inline size_t osMBuf_getRemaining(const osMBuf_t *mb)
{
	return (mb && (mb->end > mb->pos)) ? (mb->end - mb->pos) : 0;
}


/**
 * Get available space in buffer (size - pos)
 *
 * @param mb Memory buffer
 *
 * @return Number of bytes available in buffer
 */
static inline size_t osMBuf_get_space(const osMBuf_t *mb)
{
	return (mb && (mb->size > mb->pos)) ? (mb->size - mb->pos) : 0;
}


/**
 * Set absolute position
 *
 * @param mb  Memory buffer
 * @param pos Position
 */
/*
void osMBuf_set_pos(osMBuf_t *mb, size_t pos)
{
	mb->pos = pos;
	MBUF_CHECK_POS(mb);
}
*/

/**
 * Set absolute end
 *
 * @param mb  Memory buffer
 * @param end End position
 */
static inline void osMBuf_set_end(osMBuf_t *mb, size_t end)
{
	mb->end = end;
	MBUF_CHECK_END(mb);
}


/**
 * Advance position +/- N bytes
 *
 * @param mb  Memory buffer
 * @param n   Number of bytes to advance
 */
#if 0
static inline void osMBuf_advance(osMBuf_t *mb, ssize_t n)
{
	mb->pos += n;
	MBUF_CHECK_POS(mb);
}
#endif

/**
 * Rewind position and end to the beginning of buffer
 *
 * @param mb  Memory buffer
 */
static inline void osMBuf_rewind(osMBuf_t *mb)
{
	mb->pos = mb->end = 0;
}


/**
 * Set position to the end of the buffer
 *
 * @param mb  Memory buffer
 */
static inline void osMBuf_skip_to_end(osMBuf_t *mb)
{
	mb->pos = mb->end;
}



#endif
