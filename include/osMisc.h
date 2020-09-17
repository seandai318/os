/* Copyright 2019, 2020, Sean Dai
 */

#ifndef _OS_MISC_H
#define _OS_MISC_H

#include "osTypes.h"


#define OS_IS_LWS(a) (!(a^0x20) || !(a^0x9) || !(a^0xa) || !(a^0xd))

#define OS_MAX_UINT64_STR_LEN	21 //the actual len is 20, plus 1 for null termination

typedef enum {
	OS_INT_TYPE_U8,
	OS_INT_TYPE_U16,
	OS_INT_TYPE_U32,
    OS_INT_TYPE_U64,
} osIntType_e;


//for the next hop distribution, when distribution module is ready, move there
typedef enum {
    OS_NODE_SELECT_MODE_ROUND_ROBIN,
    OS_NODE_SELECT_MODE_PRIORITY,
} osNodeSelMode_e;


int osMinInt(int a, int b);

osStatus_e osStr2U64(char* ch, uint32_t len, uint64_t* value);
osStatus_e osStr2Int(char* ch, uint32_t len, int* value);
/* isNullTerm the output is a null terminated string, otherwise, the string is not null terminated
 * isReverse the output is reversed, like if the integer is 1234, the string output is: 4321
 * the return value is the string length
 */
int osUInt2Str(void* pn, osIntType_e inttype, char* s, bool isNullTerm, bool isReverse);
//return null terminated string, the return value is the string len
int osUInt2Str8(uint8_t n, char* s);
//return null terminated string, the return value is the string len
int osUInt2Str16(uint16_t n, char* s);
//return null terminated string, the return value is the string len
int osUInt2Str32(uint32_t n, char* s);
//return null terminated string, the return value is the string len
int osUInt2Str64(uint64_t n, char* s);

char* osGetNodeId();

#endif

