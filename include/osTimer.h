//Copyright (c) 2019, InterLogic

#ifndef _OS_TIMER_H
#define _OS_TIMER_H

#include "osTypes.h"


//the subchain is 10 min long
#define OS_TIMEOUT_SUB_CHAIN_TIME	100
//the timeout granuity is 50 ms
#define OS_TIMEOUT_SUB_CHAIN_INTERVAL	50 
//the max timeout duration is 10 days
#define OS_MAX_TIMEOUT_DURATION 10*24*3600

#define MAX_SUB_CHAIN_NODE_NUM	(OS_TIMEOUT_SUB_CHAIN_TIME*1000)/OS_TIMEOUT_SUB_CHAIN_INTERVAL

#define OS_TIMER_ID_CHAIN_BITS	26
#define OS_TIMER_ID_SUBCHAIN_BITS	20
#define OS_TIMER_ID_EVENT_BITS	18
#define OS_TIMER_ID_CHAIN_MASK	0x3FFFFFF
#define OS_TIMER_ID_SUBCHAIN_MASK 0xFFFFF
#define OS_TIMER_ID_EVENT_MAK	0x3FFFF


typedef void (*timeoutCallBackFunc_t)(uint64_t timerId, void* ptr);

typedef struct osTimerInfo {
	timeoutCallBackFunc_t callback;
	void* pData;
} osTimerInfo_t;


typedef struct osTimerEvent {
	uint32_t nodeId;
	uint64_t timerId;
	void* pUserData;
	struct osTimerEvent* pNext;
} osTimertEvent_t;


typedef struct osTimerSubChainNode {
	time_t nodeTimeMSec;
	uint32_t nodeId;
	uint32_t eventCount;
	osTimertEvent_t* pTimerEvent;
	osTimertEvent_t* pLastTimerEvent;
	struct osTimerSubChainNode* pNext;
} osTimerSubChainNode_t;


typedef struct osTimerChainNode {
	time_t nodeTimeSec;
	uint32_t nodeId;
	osTimerSubChainNode_t* pTimerSubChain;
	struct osTimerChainNode* pNext;
} osTimerChainNode_t;



int osTimerInit(int localWriteFd, int remoteWriteFd, int timeoutMultiple, timeoutCallBackFunc_t callBackFunc);
int osTimerGetMsg(void* pMsg);
uint64_t osStartTimer(time_t msec, timeoutCallBackFunc_t callback, void* pData);
int osStopTimer(uint64_t timerId);

#endif
