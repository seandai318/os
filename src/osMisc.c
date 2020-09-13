/********************************************************
 * Copyright (C) 2019, Sean Dai
 *
 * @file osMisc.c  Miscellaneous middleware functions
 ********************************************************/

#include <stdio.h>

#include "osMisc.h"
#include "osTypes.h"


int osMinInt(int a, int b)
{
	return a > b ? b : a;
}


osStatus_e osStr2Int(char* ch, uint32_t len, int* value)
{
	*value = 0;
	int sign = 1;

	if(*ch == '-' || *ch =='+')
	{
		if(*ch == '-')
		{
			sign = -1;
		}
		ch++;
	}
	
	for(int i=0; i<len; i++)
	{
		if(*ch >= '0' && *ch <= '9')
		{
			*value = *value * 10 + (*ch - '0');
			ch++;
		}
		else
		{
			return OS_ERROR_INVALID_VALUE;
		}
	}

	*value *= sign;

	return OS_STATUS_OK;
}
	

/* isNullTerm the output is a null terminated string, otherwise, the string is not null terminated
 * isReverse the output is reversed, like if the integer is 1234, the string output is: 4321
 * the return value is the string length
 */
int osUInt2Str(void* pn, osIntType_e inttype, char* s, bool isNullTerm, bool isReverse)
{
	if(!pn || !s)
	{
		return 0;
	}

	uint64_t n=0;
	switch (inttype)
	{
		case OS_INT_TYPE_U8:
			n = *(uint8_t*)pn;
			break;
        case OS_INT_TYPE_U16:
            n = *(uint16_t*)pn;
            break;
        case OS_INT_TYPE_U32:
            n = *(uint32_t*)pn;
            break;
        case OS_INT_TYPE_U64:
            n = *(uint64_t*)pn;
            break;
		default:
			return 0;
	}

	int strlen = 0;
	do 
	{       
		s[strlen++] = n % 10 + '0';   /* get next digit */
	} while ((n /= 10) > 0);     /* delete it */
    
	if(isNullTerm)
	{ 
		s[strlen] = '\0';
	}

	if(!isReverse)
	{
		int i, j;
		char c;

		for(i=0, j=strlen-1; i<j; i++, j--)
		{
			c=s[i];
			s[i]=s[j];
			s[j]=c;
		}
 	}

	return strlen;
}


int osUInt2Str8(uint8_t n, char* s)
{
	return osUInt2Str(&n, OS_INT_TYPE_U8, s, true, false);
}

int osUInt2Str16(uint16_t n, char* s)
{
    return osUInt2Str(&n, OS_INT_TYPE_U16, s, true, false);
}


int osUInt2Str32(uint32_t n, char* s)
{
    return osUInt2Str(&n, OS_INT_TYPE_U32, s, true, false);
}

int osUInt2Str64(uint64_t n, char* s)
{
    return osUInt2Str(&n, OS_INT_TYPE_U64, s, true, false);
}


//TODO to get the real nodeId
char* osGetNodeId()
{
	return "sean";
}
