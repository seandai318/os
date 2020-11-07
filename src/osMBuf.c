/********************************************************
 * Copyright (C) 2019, 2020 Sean Dai
 *
 * @file osMBuf.c  Memory buffers
 * Adapted from mbuf.c @Creytiv.com
 ******************************************************** 
 * @file mbuf.c  Memory buffers
 *
 * Copyright (C) 2010 Creytiv.com
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "osTypes.h"
#include "osPL.h"
#include "osMemory.h"
#include "osMBuf.h"
#include "osPrintf.h"
#include "osDebug.h"
#include "osMisc.h"


enum {DEFAULT_SIZE=512};

static osMBuf_t* osMBuf_allocInternal(size_t size, bool isNeedMutex);
static osMBuf_t *osMBuf_allocRefInternal(osMBuf_t *mbr, bool isNeedMutex);



static void osMBuf_destructor(void *data)
{
	osMBuf_t *mb = data;

	osfree(mb->buf);
}


/**
 * Allocate a new memory buffer
 *
 * @param size Initial buffer size
 *
 * @return New memory buffer, NULL if no memory
 */
static osMBuf_t* osMBuf_allocInternal(size_t size, bool isNeedMutex)
{
	osMBuf_t *mb;

	if(!size)
	{
		return NULL;
	}

	if(isNeedMutex)
	{
		mb = oszalloc_r(sizeof(*mb), osMBuf_destructor);
	}
	else
	{
        mb = oszalloc(sizeof(*mb), osMBuf_destructor);
	}
	if (!mb)
	{
		return NULL;
	}


    uint8_t *buf;

    if(isNeedMutex)
	{
    	mb->buf = osmalloc_r(size, NULL);
	}
	else
	{
        mb->buf = osmalloc(size, NULL);
    }
    if (!mb->buf)
    {
		osfree(mb);
        return NULL;
    }

    mb->size = size;

	return mb;
}


osMBuf_t* osMBuf_alloc(size_t size)
{
    return osMBuf_allocInternal(size, false);
}


osMBuf_t* osMBuf_alloc_r(size_t size)
{
	return osMBuf_allocInternal(size, true);
}


/**
 * Allocate a new mbuf with a reference to another mbuf, the new mbuf's buf points to the original mbuf's buf
 *
 * @param mbr Memory buffer to reference
 *
 * @return New memory buffer, NULL if no memory
 */
static osMBuf_t *osMBuf_allocRefInternal(osMBuf_t *mbr, bool isNeedMutex)
{
	osMBuf_t *mb;

	if (!mbr)
	{
		return NULL;
	}

	if(isNeedMutex)
	{
		mb = oszalloc_r(sizeof(*mb), osMBuf_destructor);
	}
	else
	{
		mb = oszalloc(sizeof(*mb), osMBuf_destructor);
    }
	if (!mb)
	{
		return NULL;
	}

	mb->buf  = osmemref(mbr->buf);
	mb->size = mbr->size;
	mb->pos  = mbr->pos;
	mb->end  = mbr->end;

	return mb;
}


osMBuf_t *osMBuf_allocRef(osMBuf_t *mbr)
{
	return osMBuf_allocRefInternal(mbr, false);
}


osMBuf_t *osMBuf_allocRef_r(osMBuf_t *mbr)
{
    return osMBuf_allocRefInternal(mbr, true);
}


//use the original buffer's pos.  Be noted osMem_ref is not called, pDupMBuf shall be used as a temporary helping variable, shall NOT be live longer than pOrigMBuf
void osMBuf_allocRef1(osMBuf_t* pDupMBuf, osMBuf_t* pOrigMBuf, size_t newPos, size_t len)
{
	if(!pDupMBuf || !pOrigMBuf)
	{
		return;
	}

	
    pDupMBuf->buf = pOrigMBuf->buf;
	pDupMBuf->size = pOrigMBuf->size;
    pDupMBuf->pos = newPos;
    pDupMBuf->end = newPos + len;
}


//reposition the pos to 0.  Be noted osMem_ref is not called, pDupMBuf shall be used as a temporary helping variable, shall NOT be live longer than pOrigMBuf
//startPos is the pOrigMBuf->pos where the pDupMBuf shall start from
void osMBuf_allocRef2(osMBuf_t* pDupMBuf, osMBuf_t* pOrigMBuf, size_t startPos, size_t len)
{
    if(!pDupMBuf || !pOrigMBuf)
    {
        return;
    }


    pDupMBuf->buf = &pOrigMBuf->buf[startPos];
	pDupMBuf->pos = 0;
	pDupMBuf->end = len;
	pDupMBuf->size = len;
}


/**
 * Initialize a memory buffer
 *
 * @param mb Memory buffer to initialize
 */
void osMBuf_init(osMBuf_t *mb)
{
	if (!mb)
		return;

	mb->buf  = NULL;
	mb->size = 0;
	mb->pos  = 0;
	mb->end  = 0;
}


/**
 * Reset a memory buffer
 *
 * @param mb Memory buffer to reset
 */
void osMBuf_reset(osMBuf_t *mb)
{
	if (!mb)
	{
		return;
	}

	mb->buf = osfree(mb->buf);
	osMBuf_init(mb);
}


void osMBuf_dealloc(osMBuf_t *mb)
{
	if(!mb)
	{
		return;
	}

//	mb->buf = osfree(mb->buf);  this function shall be called as part of osfree(mb)
	osfree(mb);
}


/**
 * Resize a memory buffer
 *
 * @param mb   Memory buffer to resize
 * @param size New buffer size, if size=0, trim the unused buf
 *
 * @return 0 if success, otherwise errorcode
 *
 * isNeedMutex is deduced from mb
 */
int osMBuf_realloc(osMBuf_t *mb, size_t size)
{
	if (!mb)
	{
		return -1;
	}

	if(!size)
	{
		size = mb->end;
		if(!mb->end || mb->end == mb->size)
		{
			return 0;
		}
	}

	if(osmem_isNeedMutex(mb))
	{
		mb->buf = mb->buf ? osrealloc(mb->buf, size) : osmalloc_r(size, NULL);
	}
	else
	{
        mb->buf = mb->buf ? osrealloc(mb->buf, size) : osmalloc(size, NULL);
    }

	if (!mb->buf)
	{
		return -1;
	}

	mb->size = size;

	return 0;
}



/**
 * Shift mbuf content position
 *
 * @param mb    Memory buffer to shift
 * @param shift Shift offset count
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_shift(osMBuf_t *mb, ssize_t shift)
{
	size_t rsize;
	uint8_t *p;

	if (!mb)
	{
		return EINVAL;
	}

	if (((ssize_t)mb->pos + shift) < 0 || ((ssize_t)mb->end + shift) < 0)
	{
		return ERANGE;
	}

	rsize = mb->end + shift;

	if (rsize > mb->size)
	{
		int err;

		err = osMBuf_realloc(mb, rsize);
		if (err)
		{
			return err;
		}
	}

	p = osMBuf_getCurBuf(mb);

	memmove(p + shift, p, osMBuf_getRemaining(mb));

	mb->pos += shift;
	mb->end += shift;

	return 0;
}


int osMBuf_modifyStr(osMBuf_t *mb, char* str, size_t strLen, size_t pos)
{
	if(!mb || !str)
	{
		return -1;
	}

logError("to-remove, mb->end=%ld, mb->pos=%ld", mb->end,  mb->pos);
	if(pos+strLen >= mb->end)
	{
		return -1;
	}

	memcpy(mb->buf + pos, str, strLen);

	return 0;
}
	

osMBuf_t* osMBuf_readFile(char* file, size_t initBufSize)
{
	osMBuf_t* pBuf = NULL;
	FILE* fp = NULL;

	if(!file)
	{
		goto EXIT;
	}

    fp = fopen(file, "rb");
	if(!fp)
	{
		goto EXIT;
	}

	pBuf = osMBuf_alloc(initBufSize);
	if(!pBuf)
	{
		goto EXIT;
	}

	char c;
    while((c = getc(fp)) != EOF)
    {
		pBuf->buf[pBuf->pos++] = c;
		if(pBuf->pos >= pBuf->size)
		{
			if(osMBuf_realloc(pBuf, 2*pBuf->size) != 0)
			{
				osMBuf_dealloc(pBuf);
				goto EXIT;
			}
		}
	}

	pBuf->buf[pBuf->pos]='\0';
	pBuf->end = pBuf->pos;
	pBuf->pos = 0;

EXIT:
	if(fp)
	{
		fclose(fp);
	}

	return pBuf;
}


/**
 * Write a block of memory to a memory buffer starting from the current mbuf position
 *
 * @param mb   Memory buffer
 * @param buf  Memory block to write
 * @param size Number of bytes to write
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_writeBuf(osMBuf_t *mb, const uint8_t *buf, size_t size, bool isAdvancePos)
{
	size_t rsize;

	if (!mb || !buf)
	{
		return EINVAL;
	}

	rsize = mb->pos + size;

	if (rsize > mb->size)
	{
		const size_t dsize = mb->size ? (mb->size * 2) : DEFAULT_SIZE;

		int err;
		err = osMBuf_realloc(mb, MAX(rsize, dsize));
		if (err)
		{
			return err;
		}
	}		

	memcpy(mb->buf + mb->pos, buf, size);

	if(isAdvancePos)
	{
		mb->pos += size;
		mb->end  = MAX(mb->end, mb->pos);
	}
	else
	{
		mb->end  = MAX(mb->end, mb->pos + size);
	}

	return 0;
}

//copy the srcmb buf between the startPos and stopPos (the characters including startPos until stopPos-1) to destmb.  
int osMBuf_writeBufRange(osMBuf_t *destmb, const osMBuf_t *srcmb, size_t startPos, size_t stopPos, bool isAdvancePos)
{
    size_t rsize;
	size_t size= stopPos-startPos;

    if (!destmb || !srcmb || stopPos < startPos)
    {
        return EINVAL;
    }

    rsize = destmb->pos + size;

    if (rsize > destmb->size)
    {
        const size_t dsize = destmb->size ? (destmb->size * 2) : DEFAULT_SIZE;

        int err;
        err = osMBuf_realloc(destmb, MAX(rsize, dsize));
        if (err)
        {
            return err;
        }
    }

    memcpy(destmb->buf + destmb->pos, &srcmb->buf[startPos], size);

    if(isAdvancePos)
    {
        destmb->pos += size;
        destmb->end  = MAX(destmb->end, destmb->pos);
    }
    else
    {
        destmb->end  = MAX(destmb->end, destmb->pos + size);
    }

    return 0;
}


int osMBuf_setZero(osMBuf_t *mb, size_t size, bool isAdvancePos)
{
	if(!mb)
	{
		return EINVAL;
	}

	if(size > (mb->size - mb->pos))
	{
		return EINVAL;
	}

	memset(&mb->buf[mb->pos], 0, size);
    if(isAdvancePos)
    {
        mb->pos += size;
        mb->end  = MAX(mb->end, mb->pos);
    }
    else
    {
        mb->end  = MAX(mb->end, mb->pos + size);
    }

	return 0;
}


/**
 * Write an 8-bit value to a memory buffer in the current mbuf position
 *
 * @param mb Memory buffer
 * @param v  8-bit value to write
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_writeU8(osMBuf_t *mb, uint8_t v, bool isAdvancePos)
{
	return osMBuf_writeBuf(mb, (uint8_t *)&v, sizeof(v), isAdvancePos);
}


int osMBuf_writeU8Str(osMBuf_t *mb, uint8_t v, bool isAdvancePos)
{
	char str[OS_MAX_UINT64_STR_LEN];
	int len = osUInt2Str8(v, str);
    return osMBuf_writeBuf(mb, (uint8_t *)str, len, isAdvancePos);
}


/**
 * Write a 16-bit value to a memory buffer in the current mbuf position
 *
 * @param mb Memory buffer
 * @param v  16-bit value to write
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_writeU16(osMBuf_t *mb, uint16_t v, bool isAdvancePos)
{
	return osMBuf_writeBuf(mb, (uint8_t *)&v, sizeof(v), isAdvancePos);
}


int osMBuf_writeU16Str(osMBuf_t *mb, uint16_t v, bool isAdvancePos)
{
    char str[OS_MAX_UINT64_STR_LEN];
    int len = osUInt2Str16(v, str);
    return osMBuf_writeBuf(mb, (uint8_t *)str, len, isAdvancePos);
}


/**
 * Write a 32-bit value to a memory buffer in the current mbuf position
 *
 * @param mb Memory buffer
 * @param v  32-bit value to write
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_writeU32(osMBuf_t *mb, uint32_t v, bool isAdvancePos)
{
	return osMBuf_writeBuf(mb, (uint8_t *)&v, sizeof(v), isAdvancePos);
}


int osMBuf_writeU32Str(osMBuf_t *mb, uint32_t v, bool isAdvancePos)
{
    char str[OS_MAX_UINT64_STR_LEN];
    int len = osUInt2Str32(v, str);
    return osMBuf_writeBuf(mb, (uint8_t *)str, len, isAdvancePos);
}


/**
 * Write a 64-bit value to a memory buffer in the current mbuf position
 *
 * @param mb Memory buffer
 * @param v  64-bit value to write
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_writeU64(osMBuf_t *mb, uint64_t v, bool isAdvancePos)
{
	return osMBuf_writeBuf(mb, (uint8_t *)&v, sizeof(v), isAdvancePos);
}


int osMBuf_writeU64Str(osMBuf_t *mb, uint64_t v, bool isAdvancePos)
{
    char str[OS_MAX_UINT64_STR_LEN];
    int len = osUInt2Str64(v, str);
    return osMBuf_writeBuf(mb, (uint8_t *)str, len, isAdvancePos);
}


/**
 * Write a null-terminated string to a memory buffer in the current mbuf position
 *
 * @param mb  Memory buffer
 * @param str Null terminated string to write
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_writeStr(osMBuf_t *mb, const char *str, bool isAdvancePos)
{
	if (!str)
	{
		return EINVAL;
	}

	return osMBuf_writeBuf(mb, (const uint8_t *)str, strlen(str), isAdvancePos);
}


/**
 * Write a pointer-length string to a memory buffer in the current mbuf position
 *
 * @param mb  Memory buffer
 * @param pl  Pointer-length string
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_writePL(osMBuf_t *mb, const osPointerLen_t *pl, bool isAdvancePos)
{
	if (!pl)
	{
		return EINVAL;
	}

	return osMBuf_writeBuf(mb, (const uint8_t *)pl->p, pl->l, isAdvancePos);
}


//write until a pattern is meet, the writing ends after the last character of the pattern
int osMBuf_writeUntil(osMBuf_t *mb, const osPointerLen_t* src, const osPointerLen_t* pattern, bool isAdvancePos)
{
	const char* p = osPL_findStr(src, pattern->p, pattern->l);
	if(!p)
	{
		return EINVAL;
	}
			
	osPointerLen_t writePL = {src->p, p - src->p + pattern->l};
	return osMBuf_writePL(mb, &writePL, true);
}
 

/**
 * Write a pointer-length string to a memory buffer, excluding a section
 *
 * @param mb   Memory buffer
 * @param pl   Pointer-length string
 * @param skip Part of pl to exclude
 *
 * @return 0 if success, otherwise errorcode
 *
 * @todo: create substf variante
 */
int osMBuf_writePLskipSection(osMBuf_t *mb, const osPointerLen_t *pl, const osPointerLen_t *skip, bool isAdvancePos)
{
    osPointerLen_t r;
    int err;

    if (!pl)
    {
        return EINVAL;
    }

    if(!skip)
    {
        return osMBuf_writePL(mb, pl, isAdvancePos);
    }

    if (pl->p > skip->p || (skip->p + skip->l) > (pl->p + pl->l))
    {
        return ERANGE;
    }

    r.p = pl->p;
    r.l = skip->p - pl->p;

    err = osMBuf_writeBuf(mb, (const uint8_t *)r.p, r.l, isAdvancePos);
    if (err)
    {
        return err;
    }

    r.p = skip->p + skip->l;
    r.l = pl->p + pl->l - r.p;

    return osMBuf_writeBuf(mb, (const uint8_t *)r.p, r.l, isAdvancePos);
}


/**
 * Read a block of memory from a memory buffer pointed by the current mbuf position
 *
 * @param mb   Memory buffer
 * @param buf  Buffer to read data to
 * @param size Size of buffer
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_readBuf(osMBuf_t *mb, uint8_t *buf, size_t size)
{
	if (!mb || !buf)
	{
		return EINVAL;
	}

	if (size > osMBuf_getRemaining(mb)) 
	{
		logWarning("tried to read beyond mbuf end (%u > %u)\n", size, osMBuf_getRemaining(mb));
		return EOVERFLOW;
	}

	memcpy(buf, mb->buf + mb->pos, size);

	mb->pos += size;

	return 0;
}


/**
 * Read an 8-bit value from a memory buffer pointed by the current mbuf position
 *
 * @param mb Memory buffer
 *
 * @return 8-bit value
 */
uint8_t osMBuf_readU8(osMBuf_t *mb)
{
	uint8_t v;

	return (0 == osMBuf_readBuf(mb, &v, sizeof(v))) ? v : 0;
}


/**
 * Read a 16-bit value from a memory buffer pointed by the current mbuf position
 *
 * @param mb Memory buffer
 *
 * @return 16-bit value
 */
uint16_t osMBuf_readU16(osMBuf_t *mb)
{
	uint16_t v;

	return (0 == osMBuf_readBuf(mb, (uint8_t *)&v, sizeof(v))) ? v : 0;
}


/**
 * Read a 32-bit value from a memory buffer pointed by the current mbuf position
 *
 * @param mb Memory buffer
 *
 * @return 32-bit value
 */
uint32_t osMBuf_readU32(osMBuf_t *mb)
{
	uint32_t v;

	return (0 == osMBuf_readBuf(mb, (uint8_t *)&v, sizeof(v))) ? v : 0;
}


/**
 * Read a 64-bit value from a memory buffer pointed by the current mbuf position
 *
 * @param mb Memory buffer
 *
 * @return 64-bit value
 */
uint64_t osMBuf_readU64(osMBuf_t *mb)
{
	uint64_t v;

	return (0 == osMBuf_readBuf(mb, (uint8_t *)&v, sizeof(v))) ? v : 0;
}


/**
 * Read a string from a memory buffer pointed by the current mbuf position, assuming the memory buffer is a NULL terminated string
 *
 * @param mb   Memory buffer
 * @param str  Buffer to read string to
 * @param size Size of buffer
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_readStr(osMBuf_t *mb, char *str, size_t size)
{
	if (!mb || !str)
	{
		return EINVAL;
	}

	while (size--)
	{
		const uint8_t c = osMBuf_readU8(mb);
		*str++ = c;
		if ('\0' == c)
		{
			break;
		}
	}

	return 0;
}



/**
 * Duplicate a null-terminated string from a memory buffer pointed by the current mbuf position
 *
 * @param mb   Memory buffer
 * @param strp Pointer to destination string; allocated and set
 * @param len  Length of string
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_strdup(osMBuf_t *mb, char **strp, size_t len)
{
	char *str;
	int err;

	if (!mb || !strp)
	{
		return EINVAL;
	}

	str = osmalloc(len + 1, NULL);
	if (!str)
	{
		return ENOMEM;
	}

	err = osMBuf_readBuf(mb, (uint8_t *)str, len);
	if (err)
	{
		goto out;
	}

	str[len] = '\0';

 out:
	if (err)
	{
		osfree(str);
	}
	else
	{
		*strp = str;
	}

	return err;
}


/**
 * Write n bytes of value 'c' to a memory buffer, the current position will also extend by length n
 *
 * @param mb   Memory buffer
 * @param c    Value to write
 * @param n    Number of bytes to write
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_fill(osMBuf_t *mb, uint8_t c, size_t n)
{
	size_t rsize;

	if (!mb || !n)
	{
		return EINVAL;
	}

	rsize = mb->pos + n;

	if (rsize > mb->size) 
	{
		const size_t dsize = mb->size ? (mb->size * 2) : DEFAULT_SIZE;

		int err;
		err = osMBuf_realloc(mb, MAX(rsize, dsize));
		if (err)
			return err;
	}

	memset(mb->buf + mb->pos, c, n);

	mb->pos += n;
	mb->end  = MAX(mb->end, mb->pos);

	return 0;
}



/**
 * Debug the memory buffer
 *
 * @param pf Print handler
 * @param mb Memory buffer
 *
 * @return 0 if success, otherwise errorcode
 */
int osMBuf_debug(FILE* pf, const osMBuf_t *mb)
{
	if (!mb || !pf)
	{
		return 0;
	}

	osPrintf_t printInfo;
	printInfo.pHandler = osPrintfHandler_debug;
	printInfo.arg = pf;

	int err = osPrintf_handler(&printInfo, "mBuf=%p, buf=%p, buf.nref=%d, pos=%zu, end=%zu, size=%zu", mb, mb->buf, osMem_getRefNum(mb->buf), mb->pos, mb->end, mb->size);
	if (!err)
	{
		err = osPrintf_handler(&printInfo, "\n");
	}

	return err;
}


/**
 * Set absolute position
 *
 * @param mb  Memory buffer
 * @param pos Position
 */
void osMBuf_setPos(osMBuf_t *mb, size_t pos)
{
    mb->pos = pos;
    MBUF_CHECK_POS(mb);
}

void osMBuf_advance(osMBuf_t *mb, ssize_t n)
{
	if(!mb)
	{
		return;
	}

	if ((mb->pos + n) < 0 || ((mb->pos + n) > mb->size))
	{
		return;
	}

	mb->pos += n;
}


//return value, -1, no match found, otherwise, the beginning of the pattern in the string
ssize_t osMbuf_findMatch(osMBuf_t* pBuf, osPointerLen_t* pattern)
{
	if(!pBuf || !pattern || pattern->l == 0 || !pattern->p )
	{
		logError("null pointer, pBuf=%p, pattern=%p, pattern(%p:%ld).", pBuf, pattern, pattern->p, pattern->l);
		return -1;
	}

	ssize_t pos = -1;
	size_t end = pBuf->end - pattern->l;
	while(pBuf->pos < end)
	{
		//check char for pos-0 and pos-1, if match, compare the whole pattern
		if(pBuf->buf[pBuf->pos] == pattern->p[0])
		{
			pos = pBuf->pos++;
			if(pattern->l == 1)
			{
				goto EXIT;
			}
			else if(pattern->l == 2)
			{
				if(pBuf->buf[pBuf->pos++] == pattern->p[1])
				{
					goto EXIT;
				}
			}
			else
			{
				if(pBuf->buf[pBuf->pos++] == pattern->p[1])
				{
					if(strncasecmp(&pBuf->buf[pBuf->pos], &pattern->p[2], pattern->l-2) == 0)
					{
						goto EXIT;
					}
				}
			}
            pBuf->pos = pos + 1;
		}
		else
		{
			pBuf->pos++;
		}
	}

	pos = -1;

EXIT:
	if(pos != -1)
	{
		pBuf->pos = pos;
	}
	return pos;
}


ssize_t osMbuf_findValue(osMBuf_t* pBuf, char tag1, char tag2, bool isExclSpace, osPointerLen_t* pValue)
{
	if(!pBuf || !pValue)
	{
		return -1;
	}

	ssize_t pos = -1;
	ssize_t len = -1;
	while(pBuf->pos < pBuf->end)
	{
		if(pBuf->buf[pBuf->pos] == tag1)
		{
			pos = ++pBuf->pos;
			while(pBuf->pos < pBuf->end)
			{
				if(pBuf->buf[pBuf->pos++] == tag2)
				{
					len = pBuf->pos - pos - 1;
					break;
				}
			}

			if(len != -1)
			{
				break;
			}
		}
		else
		{
			pBuf->pos++;
		}
	}

	if(pos == -1 || len == -1)
	{
		return -1;
	}

	if(isExclSpace)
	{
		size_t end = pos+len;
		for(; pos < end; pos++)
		{
			if(pBuf->buf[pos] != ' ' && pBuf->buf[pos] != '\t')
			{
				pValue->p = &pBuf->buf[pos];
				break;
			}
		}
		len = end - pos;

		for(int i=end-1; i>=pos; i--)
		{
			if(pBuf->buf[i] != ' ' && pBuf->buf[i] != '\t')
			{
				pValue->l = len;
				break;
			}
			else
			{
				--len;
			}
		}
	}
	else
	{
		pValue->p = &pBuf->buf[pos];
		pValue->l = len;
	}

	return pos;
}


//convert a pl to mBuf.  pl memory is seperately managed, no need to free pl memory when mBuf dealloc.
osMBuf_t* osMBuf_setPL(osPointerLen_t *pl)
{
    if(!pl)
    {
        return NULL;
    }

    //when free, no need to free the pl memory
    osMBuf_t* mb = oszalloc(sizeof(osMBuf_t), NULL);
    if (!mb)
    {
        return NULL;
    }

    mb->buf = (uint8_t*)pl->p;
    mb->size = pl->l;

    return mb;
}

