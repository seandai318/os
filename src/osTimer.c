//Copyright (c) 2019, InterLogic

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <err.h>
#include "osTimer.h"
#include "osTimerModule.h"
#include "osResourceMgmt.h"
#include "osDebug.h"


static int osTimerTickExpire();	//this is a internal function called when periodically tick time for the module expires
static uint64_t osChainInsertTimerEvent(osTimerChainNode_t* pChainNode, time_t diffMsec, void* pData);
static int osChainStopTimerFreeTimerEvent(osTimerChainNode_t* pCurNode, uint64_t timerId);
static osTimerChainNode_t* osChainFreeSubChainTimerEvents(osTimerChainNode_t* pCurNode, time_t diffMsec);
static osTimerSubChainNode_t* osTimerFindSubChainNode(osTimerChainNode_t* pChainNode, int isForceInsert, time_t mSec);
static osTimerChainNode_t* osTimerFindChainNode(osTimerChainNode_t* pChainNode, int isForceInsert, time_t diffSec);
static osTimertEvent_t* osTimerEventCreate(osTimerChainNode_t* pChainNode, osTimerSubChainNode_t* pSubChainNode, void* pData);
static uint64_t osChainInsertTimerEvent(osTimerChainNode_t* pChainNode, time_t diffMsec, void* pData);

static __thread osTimerChainNode_t* pTimerChain;
static __thread int writeFd;
static __thread int timerSubChainInterval;
static __thread int isTimerReady=0;
static __thread timeoutCallBackFunc_t onTimeout;
static __thread int debugCount=0; 

//debug test
//int testa;
// this function shall be called once by application in the same thread once 
int osTimerInit(int localWriteFd, int remoteWriteFd, int timeoutMultiple, timeoutCallBackFunc_t callBackFunc)
{
	writeFd = localWriteFd;
	onTimeout = callBackFunc;
	pTimerChain = (osTimerChainNode_t*) malloc(sizeof(osTimerChainNode_t));
	
	pTimerChain->nodeTimeSec = 0;
	pTimerChain->nodeId = 1;
	pTimerChain->pTimerSubChain = NULL;
	pTimerChain->pNext = NULL;

	osIPCMsg_t ipcMsg;
	ipcMsg.interface = OS_TIMER_ALL;	
	osTimerModuleMsg_t* pMsg = (osTimerModuleMsg_t*) malloc(sizeof(osTimerModuleMsg_t));
	pMsg->msgType = OS_TIMER_MODULE_REG;
	pMsg->clientPipeId = remoteWriteFd;
	pMsg->timeoutMultiple = timeoutMultiple;
	ipcMsg.pMsg = (void*) pMsg;

	debug("debug, osTimerInit, pMsg=%p\n", pMsg);
	//pipePtrMsg.ptr = pIPCMsg;
	
//	testa=15;
//	printf("debug, osTimerInit, testa=%d, testa addr=%p\n", testa, &testa);
//	void* pTesta = &testa;
//	char* tt=(char*) pIPCMsg;
//	printf("debug, osTimerInit, byte1=%x,byte2=%x, byte3=%x, byte4=%x, bute5=%x, byte6=%x, byte7=%x, byte8=%x\n", tt[0], tt[1], tt[2], tt[3], tt[4], tt[5], tt[6], tt[7]);
//	struct tdebug {
//		void* ptr;
//	};
//	struct tdebug tttt;
//	tttt.ptr = &testa;
//	write(writeFd, (void*) &tttt, sizeof(struct tdebug));
	write(writeFd, (void*) &ipcMsg, sizeof(osIPCMsg_t));
	timerSubChainInterval = timeoutMultiple * OS_TIMER_MIN_TIMEOUT_MS;
	
	return 0;
}


int osTimerGetMsg(void* pMsg)
{
	int status = 0;

	osTimerModuleMsgType_e msgType = ((osTimerModuleMsg_t*) pMsg)->msgType;
	//debug("msgReceived, msgType=%d\n", msgType);
	if (msgType == OS_TIMER_MODULE_REG_RESPONSE)
	{
		timerSubChainInterval = ((osTimerModuleMsg_t*)pMsg)->timeoutMultiple * OS_TIMER_MIN_TIMEOUT_MS;
		isTimerReady = 1;
	}
	else if (msgType = OS_TIMER_MODULE_EXPIRE)
	{
		debug("debugCount=%d", ++debugCount);
		osTimerTickExpire();
		if(debugCount==1) osStartTimer(1500, NULL, NULL);
		if(debugCount==2) osStartTimer(1000, NULL, NULL);
		if(debugCount==3) osStartTimer(100000, NULL, NULL);
	}
	else
	{
		status = -1;
	}
	
	free(pMsg);

	return status;
}


uint64_t osStartTimer(time_t msec, timeoutCallBackFunc_t callback, void* pData)
{
	debug("onStartTimer, timeout=%ld, isTimerReady=%d", msec, isTimerReady);
	if (!isTimerReady)
	{
		return 0;
	}
	
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	
	if(pTimerChain->nodeTimeSec ==0)
	{
		pTimerChain->nodeTimeSec = tp.tv_sec;
	}

	time_t diffSec = tp.tv_sec + msec/1000 - pTimerChain->nodeTimeSec;
	debug("pTimerChain=%p, pTimerChain->nodeTimeSec=%ld, curTimeSec=%ld, diffSec=%ld", pTimerChain, pTimerChain->nodeTimeSec, tp.tv_sec, diffSec);
	osTimerChainNode_t* pNewNode;
	if (diffSec >= OS_TIMEOUT_SUB_CHAIN_TIME)
	{
		pNewNode = osTimerFindChainNode(pTimerChain, 1, diffSec);
		if(pNewNode == NULL)
		{
			warn("timer could not find a a chain node, diffTime=%ld", diffSec);
			return 0;
		}
	}
	else
	{
		pNewNode = pTimerChain;
	}
		
	time_t diffMsec = (tp.tv_sec - pNewNode->nodeTimeSec)*1000 + msec + tp.tv_nsec/1000000;
	osTimerInfo_t* pTimerInfo = malloc(sizeof(osTimerInfo_t));
	pTimerInfo->pData = pData;
	pTimerInfo->callback = callback;
	debug("diffMSec=%ld", diffMsec);
	return osChainInsertTimerEvent(pNewNode, diffMsec, pTimerInfo);
}


int osStopTimer(uint64_t timerId)
{
	if(timerId == 0 || pTimerChain->nodeTimeSec ==0)
	{
		return -1;
	}
	
	return osChainStopTimerFreeTimerEvent(pTimerChain, timerId);
}


static int osTimerTickExpire()
{
	if(pTimerChain->nodeTimeSec ==0)
	{
		return 0;
	}
		
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	time_t diffSec = tp.tv_sec - pTimerChain->nodeTimeSec;
	time_t diffMsec;
	osTimerChainNode_t* pNewNode;
	
	//the timeout shall not jump ore than one timerChainNode
	if(diffSec >= 2 * OS_TIMEOUT_SUB_CHAIN_TIME)
	{
		perror("panic!, timeExpire jumped.");
		return -1;
	}
	
	//first handle the timeout in the current c:/hainNode.  There may have a small chance the timeout also happen
	//in the next timerChainNode, that will be handled after the current chainNode is processed.
	
	//the current chainNode has no timeEvent to check
	if(pTimerChain->pTimerSubChain == NULL && diffSec >= OS_TIMEOUT_SUB_CHAIN_TIME)
	{
		pNewNode = pTimerChain->pNext;
		debug("move to next pTimerChain, pNewTimerChain=%p", pNewNode);
	}
	else
	{
		//free all the timeEvents of the current subChain that has time stamp smaller than diffMsec
		diffMsec = (tp.tv_sec - pTimerChain->nodeTimeSec)*1000 + tp.tv_nsec/1000000;
		pNewNode = osChainFreeSubChainTimerEvents(pTimerChain, diffMsec);
	}
	
	debug("pNewNode=%p, pNewNode->nodeTimeSec=%ld, pTimerChain->nodeTimeSec=%ld", pNewNode, pNewNode->nodeTimeSec, pTimerChain->nodeTimeSec);	
	//the current chainNode is the only chainNode, or the next chainNode is not the neighbor node
	if(pNewNode == NULL || pNewNode->nodeTimeSec > (pTimerChain->nodeTimeSec + OS_TIMEOUT_SUB_CHAIN_TIME))
	{
		pTimerChain->nodeId += 1;
		pTimerChain->nodeTimeSec += OS_TIMEOUT_SUB_CHAIN_TIME;
		pTimerChain->pTimerSubChain = NULL;
		debug("create a new timerChain node, nodeId=%d", pTimerChain->nodeId);
	}
	//the next chainNode is not empty and is the neighbor, jump to it.  if pNewNode==pTimerChain, donothing more
	else if (pNewNode != pTimerChain)
	{
		//free the current timerChainNode
		free(pTimerChain);
		pTimerChain = pNewNode;
		debug("move to the new timerChain node, nodeId=%d", pNewNode->nodeId);
		
		//now we need to check if any timeEvent in the newNode expires
		if(diffSec >= OS_TIMEOUT_SUB_CHAIN_TIME)
		{
			diffMsec = (tp.tv_sec - pTimerChain->nodeTimeSec)*1000 + tp.tv_nsec/1000000;
			pNewNode = osChainFreeSubChainTimerEvents(pTimerChain, diffMsec);
			if(pNewNode != pTimerChain)
			{
				perror("pNewNode != pTimerChain, we do not expect it");
			}
		}
	}
		
	return 0;
}
	
	
//free a timer event in a subChain if timerId match is found, due to stopTimer() call
//return 0 if a matching event is found, otherwise, return -1
static int osChainStopTimerFreeTimerEvent(osTimerChainNode_t* pCurNode, uint64_t timerId)
{
	if(pCurNode == NULL)
	{
		perror("pCurNode is NULL");
		return -1;
	}
	
	time_t diffMsec = (timerId >> OS_TIMER_ID_EVENT_BITS) & OS_TIMER_ID_SUBCHAIN_MASK;
	
	osTimerSubChainNode_t* pNode = osTimerFindSubChainNode(pCurNode, 0, diffMsec);
	
	if (pNode == NULL)
	{
		return -1;
	}
	
	osTimertEvent_t* pTimerEvent = pNode->pTimerEvent;
	osTimertEvent_t* pTimerEventNext;
	osTimertEvent_t* pTimerEventPrev = pTimerEvent;
	while(pTimerEvent != NULL)
	{
		pTimerEventNext = pTimerEvent->pNext;
		
		//a matching timerId found
		if(timerId  == pTimerEvent->timerId)
		{
			free(pTimerEvent->pUserData);
			free(pTimerEvent);
			pTimerEventPrev->pNext = pTimerEventNext;
				
			//if this is the first timerEvent, redirect the pNode->pTimerEvent
			if(pNode->pTimerEvent == pTimerEventPrev)
			{
				pNode->pTimerEvent = pTimerEventNext;
			}
			return 0;
		}

		pTimerEventPrev = pTimerEvent;
		pTimerEvent = pTimerEventNext;
	}
	
	return -1;
}


//free the timer event in a subChain for all timeEvents with time stamp smaller than the specified mSec
//return the next chainNode if the last subNodeId is removed (the timeout is for the biggest possible time slot)
//return the current chainNode if the next timeout is expected stay in the same chainNode
static osTimerChainNode_t* osChainFreeSubChainTimerEvents(osTimerChainNode_t* pCurNode, time_t diffMsec)
{
	if(pCurNode == NULL)
	{
		perror("pCurNode is NULL");
		exit(1);
	}
	
	osTimerSubChainNode_t* pNode = pCurNode->pTimerSubChain;
	if(pNode == NULL)
	{
		debug("pTimerChain=%p, ->pTimerSubChain is NULL", pCurNode);
		return pCurNode;
	}

	time_t newNodeMsec = (diffMsec / OS_TIMEOUT_SUB_CHAIN_INTERVAL) * OS_TIMEOUT_SUB_CHAIN_INTERVAL;
	debug("newNodeMsec=%ld, Current timerChainNode=%p, pNode->nodeTimeMSec=%ld", newNodeMsec, pCurNode, pNode->nodeTimeMSec);
	int nodeId = 0;
	//remove all subChain node that has time stamp less than or equal to new one	
	while (pNode != NULL && (pNode->nodeTimeMSec - newNodeMsec <=0))
	{
		debug("on subChain node (nodeId=%d), pNode->nodeTimeMSec=%ld, newNodeMsec=%ld", pNode->nodeId, pNode->nodeTimeMSec, newNodeMsec);
		osTimertEvent_t* pTimerEvent = pNode->pTimerEvent;
		osTimertEvent_t* pTimerEventNext;
		osTimertEvent_t* pTimerEventPrev = pTimerEvent;
		while(pTimerEvent != NULL)
		{
			pTimerEventNext = pTimerEvent->pNext;

			if(((osTimerInfo_t*)pTimerEvent->pUserData)->callback)
			{
				((osTimerInfo_t*)pTimerEvent->pUserData)->callback(pTimerEvent->timerId, ((osTimerInfo_t*)pTimerEvent->pUserData)->pData);
			}
			else
			{
				onTimeout(pTimerEvent->timerId, ((osTimerInfo_t*)pTimerEvent->pUserData)->pData);
			}
			free(pTimerEvent->pUserData);
			free(pTimerEvent);
			
			pTimerEvent = pTimerEventNext;
		}
	
		pCurNode->pTimerSubChain = pNode->pNext;
		nodeId = pNode->nodeId;
		free(pNode);
		pNode = pCurNode->pTimerSubChain;
	}
		
	//this is the last possible subChianNode, the next timeout shall go to next timerChain 
	if(nodeId == MAX_SUB_CHAIN_NODE_NUM)
	{
		return pCurNode->pNext;
	}
	
	return pCurNode;
}
	
	
	
static osTimerChainNode_t* osTimerFindChainNode(osTimerChainNode_t* pCurNode, int isForceInsert, time_t diffSec)
{
	if(pCurNode == NULL)
	{
		return NULL;
	}
	
	time_t newNodeSec = pCurNode->nodeTimeSec + (diffSec / OS_TIMEOUT_SUB_CHAIN_TIME) * OS_TIMEOUT_SUB_CHAIN_TIME;
	
	osTimerChainNode_t* pNode = pCurNode;
	osTimerChainNode_t* pPrevNode = pNode;
	debug("pNode=%p, newNodeSec=%ld", pNode, newNodeSec);
	while (pNode != NULL)
	{
		int delta = pNode->nodeTimeSec - newNodeSec;
		if(delta < 0)
		{
			pPrevNode = pNode;
			pNode = pNode->pNext;
		}
		else if (delta == 0)
		{
			return pNode;
		}
		else
		{
			if (isForceInsert)
			{
				osTimerChainNode_t* pChainNode = (osTimerChainNode_t*) malloc(sizeof(osTimerChainNode_t));
				if(pChainNode == NULL)
				{
					perror("could not allocate pChainNode");
					return NULL;
				}

				pChainNode->nodeTimeSec = newNodeSec;
				pChainNode->nodeId = diffSec / OS_TIMEOUT_SUB_CHAIN_TIME + 1;
				pChainNode->pTimerSubChain = NULL;
				pChainNode->pNext = pNode;
				
				pPrevNode->pNext = pChainNode;
		
				debug("a new TimerChainNode is created, pChainNode=%p, nodeTimeSec=%ld, nodeId=%d", pChainNode, pChainNode->nodeTimeSec, pChainNode->nodeId);		
				return pChainNode;
			}

			break;
		}
	}
	
	// if we are here, we are at the end of the chain list
	if(pNode == NULL && isForceInsert)
	{
		osTimerChainNode_t* pChainNode = (osTimerChainNode_t*) malloc(sizeof(osTimerChainNode_t));
		if(pChainNode == NULL)
		{
			perror("could not allocate pChainNode");
			return NULL;
		}

		pChainNode->nodeTimeSec = newNodeSec;
		pChainNode->nodeId = diffSec / OS_TIMEOUT_SUB_CHAIN_TIME + 1;
		pChainNode->pTimerSubChain = NULL;
		pChainNode->pNext = NULL;
				
		pPrevNode->pNext = pChainNode;
				
		debug("a new TimerChainNode is created in the end, pChainNode=%p, nodeTimeSec=%ld, nodeId=%d", pChainNode, pChainNode->nodeTimeSec, pChainNode->nodeId);
		return pChainNode;	
	}
	
	return NULL;
}


static osTimerSubChainNode_t* osTimerFindSubChainNode(osTimerChainNode_t* pChainNode, int isForceInsert, time_t mSec)
{
	if(pChainNode == NULL)
	{
		return NULL;
	}
	
	if(mSec >= OS_TIMEOUT_SUB_CHAIN_TIME * 1000)
	{
		warn("the msec(%ld) > OS_TIMEOUT_SUB_CHAIN_TIME (%d)", mSec, OS_TIMEOUT_SUB_CHAIN_TIME);
		return NULL;
	}
	
	time_t newNodeMsec = (mSec / OS_TIMEOUT_SUB_CHAIN_INTERVAL) * OS_TIMEOUT_SUB_CHAIN_INTERVAL;
	
	osTimerSubChainNode_t* pNode = pChainNode->pTimerSubChain;
	osTimerSubChainNode_t* pPrevNode = pNode;
	while (pNode != NULL)
	{
		debug("pChainNode->pTimerSubChain=%p", pNode);
		time_t delta = pNode->nodeTimeMSec - newNodeMsec;
		if(delta < 0)
		{
			pPrevNode = pNode;
			pNode = pNode->pNext;
		}
		else if (delta == 0)
		{
			debug("a subchain node already exists, nodeTimeMSec=%ld", pNode->nodeTimeMSec);
			return pNode;
		}
		else
		{
			if (isForceInsert)
			{
				osTimerSubChainNode_t* pSubChainNode = (osTimerSubChainNode_t*) malloc(sizeof(osTimerSubChainNode_t));
				if(pSubChainNode == NULL)
				{
					perror("could not allocate pSubChainNode");
					return NULL;
				}

				pSubChainNode->nodeTimeMSec = newNodeMsec;
				pSubChainNode->nodeId = mSec / OS_TIMEOUT_SUB_CHAIN_INTERVAL + 1;
				pSubChainNode->eventCount = 0;
				pSubChainNode->pTimerEvent = NULL;
				pSubChainNode->pLastTimerEvent = NULL;
				pSubChainNode->pNext = pNode;
				
				pPrevNode->pNext = pSubChainNode;
				
				debug("a new subchain node is created, nodeTimeMSec=%ld, nodeId=%d", pSubChainNode->nodeTimeMSec, pSubChainNode->nodeId);
				return pSubChainNode;
			}
			
			break;
		}
	}	
		
	// if we are here, we are at the end of the chain list
	if (pNode == NULL && isForceInsert)
	{
		osTimerSubChainNode_t* pSubChainNode = (osTimerSubChainNode_t*) malloc(sizeof(osTimerSubChainNode_t));
		if(pSubChainNode == NULL)
		{
			perror("could not allocate pSubChainNode");
			return NULL;
		}

		pSubChainNode->nodeTimeMSec = newNodeMsec;
		pSubChainNode->nodeId = mSec / OS_TIMEOUT_SUB_CHAIN_INTERVAL + 1;
		pSubChainNode->eventCount = 0;
		pSubChainNode->pTimerEvent = NULL;
		pSubChainNode->pLastTimerEvent = NULL;
		pSubChainNode->pNext = NULL;
				
		// this covers the pChainNode->pTimerSubChain == NULL case
		if(pChainNode->pTimerSubChain == NULL)
		{
			pChainNode->pTimerSubChain = pSubChainNode;
		}
		else
		{
			pPrevNode->pNext = pSubChainNode;
		}

		debug("a new subchain node is created, pSubChainNode=%p, nodeTimeMSec=%ld, nodeId=%d", pSubChainNode, pSubChainNode->nodeTimeMSec, pSubChainNode->nodeId);
		return pSubChainNode;
	}
	
	return NULL;
}

static osTimertEvent_t* osTimerEventCreate(osTimerChainNode_t* pChainNode, osTimerSubChainNode_t* pSubChainNode, void* pData)
{
	if(pChainNode == NULL || pSubChainNode == NULL)
	{
		return NULL;
	}
	
	osTimertEvent_t* pTimerEvent = (osTimertEvent_t*) malloc(sizeof(osTimertEvent_t));
	if(pTimerEvent == NULL)
	{
		perror("could not allocate pTimerEvent");
		return NULL;
	}		
			
	pTimerEvent->pUserData = pData;
	pTimerEvent->nodeId = pSubChainNode->eventCount;
	pTimerEvent->timerId = (uint64_t) pChainNode->nodeId << (OS_TIMER_ID_SUBCHAIN_BITS+OS_TIMER_ID_EVENT_BITS);
	pTimerEvent->timerId |= (uint64_t) pSubChainNode->nodeId << OS_TIMER_ID_EVENT_BITS;
	pTimerEvent->timerId |= pTimerEvent->nodeId;
	pTimerEvent->pNext = NULL;	
		
	debug("a new timerEvent is created, timerId=%lx", pTimerEvent->timerId);	
	return pTimerEvent;
}
	
	
static uint64_t osChainInsertTimerEvent(osTimerChainNode_t* pChainNode, time_t diffMsec, void* pData)
{
	if(diffMsec >= OS_TIMEOUT_SUB_CHAIN_TIME*1000)
	{
		warn("diffMsec(%ld) >= OS_TIMEOUT_SUB_CHAIN_TIME(%d)", diffMsec, OS_TIMEOUT_SUB_CHAIN_TIME*1000);
		return 0;
	}
			
	osTimerSubChainNode_t* pNode = osTimerFindSubChainNode(pChainNode, 1, diffMsec);
	if(pNode == NULL)
	{
		perror("could not allocate pSubChainNode");
		return 0;
	}
		
	pNode->eventCount++;
		
	osTimertEvent_t* pTimerEvent = osTimerEventCreate(pChainNode, pNode, pData);
	if(pTimerEvent == NULL)
	{
		perror("could not allocate pTimerEvent");
		return 0;
	}		
			
	if(pNode->pTimerEvent == NULL)
	{
		pNode->pTimerEvent = pTimerEvent;
		pNode->pLastTimerEvent = pTimerEvent;
	}
	else
	{
		pNode->pLastTimerEvent->pNext = pTimerEvent;
		pNode->pLastTimerEvent = pTimerEvent;
	}	
	
	return pTimerEvent->timerId;
}
