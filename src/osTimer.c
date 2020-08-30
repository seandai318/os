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
#include "osMemory.h"
#include "osList.h"



typedef struct tickInfo {
	time_t msec;
	timeoutCallBackFunc_t callback;
	void* pData;
} tickInfo_t;


static int osTimerTickExpire();	//this is a internal function called when periodically tick time for the module expires
static uint64_t osChainInsertTimerEvent(osTimerChainNode_t* pChainNode, time_t diffMsec, void* pData);
static int osChainStopTimerFreeTimerEvent(osTimerChainNode_t* pCurNode, uint64_t timerId, osTimerInfo_t** ppTimerInfo);
static osTimerChainNode_t* osChainFreeSubChainTimerEvents(osTimerChainNode_t* pCurNode, time_t diffMsec);
static osTimerSubChainNode_t* osTimerFindSubChainNode(osTimerChainNode_t* pChainNode, int isForceInsert, time_t mSec);
static osTimerSubChainNode_t* osTimerFindSubChainNodeById(osTimerChainNode_t* pChainNode, uint32_t subNodeId, osTimerSubChainNode_t** ppPrevSubNode);
static osTimerChainNode_t* osTimerFindChainNode(osTimerChainNode_t* pChainNode, int isForceInsert, time_t sec, bool isDiffSec);
static osTimerChainNode_t* osTimerFindChainNodeById(osTimerChainNode_t* pCurNode, uint32_t nodeId);
static osTimertEvent_t* osTimerEventCreate(osTimerChainNode_t* pChainNode, osTimerSubChainNode_t* pSubChainNode, void* pData);
//static uint64_t osChainInsertTimerEvent(osTimerChainNode_t* pChainNode, time_t diffMsec, void* pData);
static uint64_t osStartTimerInternal(time_t msec, timeoutCallBackFunc_t callback, void* pData, osTimerInfo_t* pTimerInfo, bool isTick);
static void osTimerExpireInternal(timeoutCallBackFunc_t callback, uint64_t timerId, osTimerInfo_t* pTimerInfo);

static __thread osTimerChainNode_t* pTimerChain;
static __thread int writeFd;
static __thread int timerSubChainInterval;
static __thread int isTimerReady=0;
static __thread timeoutCallBackFunc_t onTimeout;
static __thread osList_t tickList = {};
static __thread int debugCount=0; 
//static __thread int itick = 0;

// this function shall be called once by application in the same thread once 
int osTimerInit(int localWriteFd, int remoteWriteFd, int timeoutMultiple, timeoutCallBackFunc_t callBackFunc)
{
	//to-remove
	osmem_stat();

	writeFd = localWriteFd;
	onTimeout = callBackFunc;
	pTimerChain = (osTimerChainNode_t*) osmalloc1(sizeof(osTimerChainNode_t), NULL);
	
	pTimerChain->nodeTimeSec = 0;
	pTimerChain->nodeId = 1;
	pTimerChain->pTimerSubChain = NULL;
	pTimerChain->pNext = NULL;

	osIPCMsg_t ipcMsg;
	ipcMsg.interface = OS_TIMER_ALL;	
	osTimerModuleMsg_t* pMsg = (osTimerModuleMsg_t*) osmalloc1(sizeof(osTimerModuleMsg_t), NULL);
	pMsg->msgType = OS_TIMER_MODULE_REG;
	pMsg->clientPipeId = remoteWriteFd;
	pMsg->timeoutMultiple = timeoutMultiple;
	ipcMsg.pMsg = (void*) pMsg;

	mdebug(LM_TIMER, "received pMsg=%p\n", pMsg);

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


int osTimerGetMsg(osInterface_e intf, void* pMsg, timerReadyFunc_h timerReadyFunc)
{
	int status = 0;

	switch(intf)
	{
		case OS_TIMER_ALL:
		{
			osTimerModuleMsgType_e msgType = ((osTimerModuleMsg_t*) pMsg)->msgType;
			//mdebug(LM_TIMER, "msgReceived, msgType=%d\n", msgType);
			if (msgType == OS_TIMER_MODULE_REG_RESPONSE)
			{
				timerSubChainInterval = ((osTimerModuleMsg_t*)pMsg)->timeoutMultiple * OS_TIMER_MIN_TIMEOUT_MS;
				isTimerReady = 1;

				logError("to-remove, isTimerReady=%d, tickList=%p, tickList.head=%p", isTimerReady, &tickList, tickList.head);
				osListElement_t* le = tickList.head;
				while(le)
				{
					logError("to-remove, in le");
					tickInfo_t* pTickInfo = le->data;
					osStartTimerInternal(pTickInfo->msec, pTickInfo->callback, pTickInfo->pData, NULL, true);
					le = le->next;
				}
				osList_delete(&tickList);
				if(timerReadyFunc)
				{
					timerReadyFunc();
				}
			}
			else
			{
				status = -1;
			}

		    osfree1(pMsg);
			break;
		}
		case OS_TIMER_TICK:
//		mdebug(LM_TIMER, "debugCount=%d", ++debugCount);
//			printf("osTimer, itick=%d\n", itick++);
			osTimerTickExpire();
			break;
		default:
			status = -1;
			break;
	}

	return status;
}


uint64_t osStartTick(time_t msec, timeoutCallBackFunc_t callback, void* pData)
{
	return osStartTimerInternal(msec, callback, pData, NULL, true);
}


uint64_t osStartTimer(time_t msec, timeoutCallBackFunc_t callback, void* pData)
{
	return osStartTimerInternal(msec, callback, pData, NULL, false);
}


uint64_t osvStartTimer(time_t msec, timeoutCallBackFunc_t callback, void* pData, char* info)
{
    uint64_t timerId = osStartTimerInternal(msec, callback, pData, NULL, false);
	mlogInfo(LM_TIMER, "start a timer, timeout=%ld, timerId=0x%lx, info: %s", msec, timerId, info);
	return timerId;
}


uint64_t osRestartTimer(uint64_t timerId)
{
	if(timerId == 0)
	{
		return 0;
	}

	osTimerInfo_t* pTimerInfo = NULL;
	int ret = osChainStopTimerFreeTimerEvent(pTimerChain, timerId, &pTimerInfo);
	if(ret == -1 || pTimerInfo ==NULL)
	{
		return 0;
	}

	return osStartTimerInternal(pTimerInfo->restartTimeout, NULL, NULL, pTimerInfo, false);
}	
	

//always return 0	
int osStopTimer(uint64_t timerId)
{
	if(timerId == 0 || pTimerChain->nodeTimeSec ==0)
	{
		return -1;
	}
	
	int ret = osChainStopTimerFreeTimerEvent(pTimerChain, timerId, NULL);
	mlogInfo(LM_TIMER, "stop timerId=0x%lx, %s", timerId, ret == 0 ? "successful" : "failed");
    osTimerListSubChainNodes(NULL);

	return 0;
}


int osvStopTimer(uint64_t timerId, char* info)
{
    if(timerId == 0 || pTimerChain->nodeTimeSec ==0)
    {
        return -1;
    }

    int ret = osChainStopTimerFreeTimerEvent(pTimerChain, timerId, NULL);
    mlogInfo(LM_TIMER, "stop timerId=0x%lx, %s, info: %s", timerId, ret == 0 ? "successful" : "failed", info);
    osTimerListSubChainNodes(NULL);

    return ret;
}


static uint64_t osStartTimerInternal(time_t msec, timeoutCallBackFunc_t callback, void* pData, osTimerInfo_t* pTimerInfo, bool isTick)
{
    if (!isTimerReady)
    {
		logError("to-remove, isTimerReady=%d", isTimerReady);
		if(isTick)
		{
			tickInfo_t* pTickInfo = osmalloc1(sizeof(tickInfo_t), NULL);
			if(!pTickInfo)
			{
				logError("fails to allocate memory for pTickInfo, size(%ld).", sizeof(tickInfo_t));
				return 0;
			}

			pTickInfo->msec = msec;
			pTickInfo->callback = callback;
			pTickInfo->pData = pData;
			osList_append(&tickList, pTickInfo);
			logError("to-remove, isTimerReady=%d, tickList=%p, head=%p", isTimerReady, &tickList, tickList.head);
		}
        else
		{	
			mdebug(LM_TIMER, "start a timer, timeout=%ld, the timer module is not ready!", msec);
        }
		return 0;
    }

    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);

    if(pTimerChain->nodeTimeSec ==0)
    {
        pTimerChain->nodeTimeSec = tp.tv_sec;
    }

    time_t diffSec = tp.tv_sec + msec/1000 - pTimerChain->nodeTimeSec;
    mdebug(LM_TIMER, "pTimerChain=%p, nodeTimeSec=%ld, curTimeSec=%ld, diffSec=%ld", pTimerChain, pTimerChain->nodeTimeSec, tp.tv_sec, diffSec);
    osTimerChainNode_t* pNewNode;
    if (diffSec >= OS_TIMEOUT_SUB_CHAIN_TIME)
    {
        pNewNode = osTimerFindChainNode(pTimerChain, true, diffSec, true);
        if(pNewNode == NULL)
        {
            mlogError(LM_TIMER, "timer could not find a a chain node, diffTime=%ld", diffSec);
            return 0;
        }
        mlogInfo(LM_TIMER, "new node=%p, nodeTimeSec=%ld.", pNewNode, pNewNode->nodeTimeSec);
    }
    else
    {
        pNewNode = pTimerChain;
    }

    time_t diffMsec = (tp.tv_sec - pNewNode->nodeTimeSec)*1000 + msec + tp.tv_nsec/1000000;
	if(pTimerInfo == NULL)
	{
    	pTimerInfo = osmalloc1(sizeof(osTimerInfo_t), NULL);
    
		pTimerInfo->pData = pData;
    	pTimerInfo->callback = callback;
		if(isTick)
		{
    		pTimerInfo->nextTimeout = msec;
		}
		else
		{
			pTimerInfo->nextTimeout = 0;
		}

		pTimerInfo->restartTimeout = msec;
	}

    uint64_t timerId = osChainInsertTimerEvent(pNewNode, diffMsec, pTimerInfo);
    mdebug(LM_TIMER, "diffMSec=%ld, pTimerChain=%p, new timerNode=%p", diffMsec, pTimerChain, pNewNode);
	if(!isTick)
	{
    	mlogInfo(LM_TIMER, "start a timer, timeout=%ld, timerId=0x%lx, pTimerInfo=%p", msec, timerId, pTimerInfo);
	}
    osTimerListSubChainNodes(NULL);

    return timerId;
}



static void osTimerExpireInternal(timeoutCallBackFunc_t callback, uint64_t timerId, osTimerInfo_t* pTimerInfo)
{
	if(!callback || !pTimerInfo)
	{
		logError("null pointer, callback=%p, pTimerInfo=%p", callback, pTimerInfo);
		return;
	}

	if(pTimerInfo->nextTimeout != 0)
	{
		if(osStartTimerInternal(pTimerInfo->nextTimeout, pTimerInfo->callback, pTimerInfo->pData, pTimerInfo, true) <=0)
		{
			logError("fails to osStartTimerInternal.");
			return;
		}
	}

	callback(timerId, pTimerInfo->pData);

	if(pTimerInfo->nextTimeout == 0)
	{
		osfree1(pTimerInfo);
	}
}
	
	


//to-remove
static __thread int aaa = 0;
static int osTimerTickExpire()
{
	if(pTimerChain->nodeTimeSec ==0)
	{
		return 0;
	}

	//to-remove	
#if 1
	if(++aaa >= 600)
	{
		osmem_stat();
		osmem_allusedinfo();
		aaa = 0;
	}
#endif

	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	time_t diffSec = tp.tv_sec - pTimerChain->nodeTimeSec;
	time_t diffMsec;
	osTimerChainNode_t* pNewNode;
	
	//the timeout shall not jump more than one timerChainNode
	if(diffSec >= 2 * OS_TIMEOUT_SUB_CHAIN_TIME)
	{
		logError("panic!, timeExpire jumped, pTimerChain=%p, diffSec=%d, pTimerChain->nodeTimeSec=%d.", pTimerChain, diffSec, pTimerChain->nodeTimeSec);
		return -1;
	}
	
//	mlogInfo(LM_TIMER, "pTimerChain=%p, pTimerChain->nodeTimeSec=%d, diffSec=%d.", pTimerChain, pTimerChain->nodeTimeSec, diffSec);
	//first handle the timeout in the current c:/hainNode.  There may have a small chance the timeout also happen
	//in the next timerChainNode, that will be handled after the current chainNode is processed.
	
	//the current chainNode has no timeEvent to check
	if(pTimerChain->pTimerSubChain == NULL && diffSec >= OS_TIMEOUT_SUB_CHAIN_TIME)
	{
		pNewNode = pTimerChain->pNext;
		mdebug(LM_TIMER, "move to next pTimerChain, pNewTimerChain=%p", pNewNode);
	}
	else
	{
		//free all the timeEvents of the current subChain that has time stamp smaller than diffMsec
		diffMsec = (tp.tv_sec - pTimerChain->nodeTimeSec)*1000 + tp.tv_nsec/1000000;
		pNewNode = osChainFreeSubChainTimerEvents(pTimerChain, diffMsec);
	}
	
	//the current chainNode is the only chainNode, or the next chainNode is not the neighbor node
	if(pNewNode == NULL || pNewNode->nodeTimeSec > (pTimerChain->nodeTimeSec + OS_TIMEOUT_SUB_CHAIN_TIME))
	{
		pTimerChain->nodeId += 1;
		pTimerChain->nodeTimeSec += OS_TIMEOUT_SUB_CHAIN_TIME;
		pTimerChain->pTimerSubChain = NULL;
		mdebug(LM_TIMER, "update pTimerChain(%p, nodeTimeSec=%ld, nodeId=%d)", pTimerChain, pTimerChain->nodeTimeSec, pTimerChain->nodeId);
	}
	//the next chainNode is not empty and is the neighbor, jump to it.  if pNewNode==pTimerChain, donothing more
	else if (pNewNode != pTimerChain)
	{
		//free the current timerChainNode
		osfree1(pTimerChain);
		pTimerChain = pNewNode;
		mdebug(LM_TIMER, "move to the new timerNode(%p, nodeTimeSec=%ld, nodeId=%d)", pNewNode, pNewNode->nodeTimeSec, pNewNode->nodeId);
		
		//now we need to check if any timeEvent in the newNode expires
		if(diffSec >= OS_TIMEOUT_SUB_CHAIN_TIME)
		{
			diffMsec = (tp.tv_sec - pTimerChain->nodeTimeSec)*1000 + tp.tv_nsec/1000000;
			pNewNode = osChainFreeSubChainTimerEvents(pTimerChain, diffMsec);
			if(pNewNode != pTimerChain)
			{
				logError("pNewNode != pTimerChain, we do not expect it");
			}
		}
	}
		
	return 0;
}
	
	
//free a timer event in a subChain if timerId match is found, due to stopTimer() call
//return 0 if a matching event is found, otherwise, return -1
static int osChainStopTimerFreeTimerEvent(osTimerChainNode_t* pCurNode, uint64_t timerId, osTimerInfo_t** ppTimerInfo)
{
	if(pCurNode == NULL)
	{
		perror("pCurNode is NULL");
		return -1;
	}
	
	uint32_t nodeId = (timerId >> (OS_TIMER_ID_EVENT_BITS + OS_TIMER_ID_SUBCHAIN_BITS)) & OS_TIMER_ID_CHAIN_MASK;

    osTimerChainNode_t* pTargetNode = osTimerFindChainNodeById(pCurNode, nodeId);
    if(pTargetNode == NULL)
    {
        mlogError(LM_TIMER, "timer could not find a a chain node, nodeId=%d", nodeId);
        return 0;
    }

    uint32_t subNodeId = (timerId >> OS_TIMER_ID_EVENT_BITS) & OS_TIMER_ID_SUBCHAIN_MASK;

	osTimerSubChainNode_t* pPrevNode;
	osTimerSubChainNode_t* pNode = osTimerFindSubChainNodeById(pTargetNode, subNodeId, &pPrevNode);
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
//			osfree1(pTimerEvent->pUserData);
//			osfree1(pTimerEvent);
			pTimerEventPrev->pNext = pTimerEventNext;
			
			//if the removed event is the only event, free the subNode
			if(--pNode->eventCount == 0)
			{
				if(pPrevNode == NULL)
				{
        			pTargetNode->pTimerSubChain = pNode->pNext;
				}
				else
				{
					pPrevNode->pNext = pNode->pNext;
				}
				osfree1(pNode);	
			}
			else
			{
				//if this is the first timerEvent, redirect the pNode->pTimerEvent
				if(pNode->pTimerEvent == pTimerEvent)
				{
					pNode->pTimerEvent = pTimerEventNext;
				}
			}

			if(ppTimerInfo)
			{
				*ppTimerInfo = pTimerEvent->pUserData;
			}
			else
			{
            	osfree1(pTimerEvent->pUserData);
			}
            osfree1(pTimerEvent);
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
		return pCurNode;
	}

	time_t newNodeMsec = (diffMsec / OS_TIMEOUT_SUB_CHAIN_INTERVAL) * OS_TIMEOUT_SUB_CHAIN_INTERVAL;
	int nodeId = 0;
	//remove all subChain node that has time stamp less than or equal to new one	
	while (pNode != NULL && (pNode->nodeTimeMSec - newNodeMsec <=0))
	{
		mdebug(LM_TIMER, "timerNode(%p), timerSubNode(%p, nodeId=%d, nodeTimeMSec=%ld, newNodeMsec=%ld) times out", pCurNode, pNode, pNode->nodeId, pNode->nodeTimeMSec, newNodeMsec);
		osTimertEvent_t* pTimerEvent = pNode->pTimerEvent;
		osTimertEvent_t* pTimerEventNext;
		osTimertEvent_t* pTimerEventPrev = pTimerEvent;
		while(pTimerEvent != NULL)
		{
			pTimerEventNext = pTimerEvent->pNext;

			if(!((osTimerInfo_t*)pTimerEvent->pUserData)->nextTimeout)
			{
            	mlogInfo(LM_TIMER, "timerId=0x%lx expired.", pTimerEvent->timerId);
			}
			if(((osTimerInfo_t*)pTimerEvent->pUserData)->callback)
			{
				osTimerExpireInternal(((osTimerInfo_t*)pTimerEvent->pUserData)->callback, pTimerEvent->timerId, ((osTimerInfo_t*)pTimerEvent->pUserData));
				//((osTimerInfo_t*)pTimerEvent->pUserData)->callback(pTimerEvent->timerId, ((osTimerInfo_t*)pTimerEvent->pUserData)->pData);
			}
			else
			{
                osTimerExpireInternal(onTimeout, pTimerEvent->timerId, ((osTimerInfo_t*)pTimerEvent->pUserData));
				//onTimeout(pTimerEvent->timerId, ((osTimerInfo_t*)pTimerEvent->pUserData)->pData);
			}

			//use osfree1 to hide the memory dealloc info
			osfree1(pTimerEvent);
			
			pTimerEvent = pTimerEventNext;
		}
	
		pCurNode->pTimerSubChain = pNode->pNext;
		nodeId = pNode->nodeId;
		//use osfree1 to hide the memory dealloc info
		osfree1(pNode);
		pNode = pCurNode->pTimerSubChain;

    	osTimerListSubChainNodes(NULL);
	}
	
	//this is the last possible subChianNode, the next timeout shall go to next timerChain 
	if(nodeId == MAX_SUB_CHAIN_NODE_NUM)
	{
		return pCurNode->pNext;
	}
	
	return pCurNode;
}
	

static osTimerChainNode_t* osTimerFindChainNodeById(osTimerChainNode_t* pCurNode, uint32_t nodeId)
{
    if(pCurNode == NULL)
    {
        return NULL;
    }

    osTimerChainNode_t* pNode = pCurNode;
    osTimerChainNode_t* pPrevNode = pNode;
    int delta;
    mdebug(LM_TIMER, "pNode=%p, target NodeId=%ld", pNode, nodeId);
    while (pNode != NULL)
    {
        delta = pNode->nodeId - nodeId;
        if(delta < 0)
        {
            pPrevNode = pNode;
            pNode = pNode->pNext;
        }
        else if (delta == 0)
        {
            return pNode;
        }
	}

	return NULL;
}
	
	
static osTimerChainNode_t* osTimerFindChainNode(osTimerChainNode_t* pCurNode, int isForceInsert, time_t sec, bool isDiffSec)
{
	if(pCurNode == NULL)
	{
		return NULL;
	}
	
	time_t newNodeSec;
	if(isDiffSec)
	{
		newNodeSec = pCurNode->nodeTimeSec + (sec / OS_TIMEOUT_SUB_CHAIN_TIME) * OS_TIMEOUT_SUB_CHAIN_TIME;
	}
	else
	{
		newNodeSec = sec;
	}

	osTimerChainNode_t* pNode = pCurNode;
	osTimerChainNode_t* pPrevNode = pNode;
	int delta;
	mdebug(LM_TIMER, "pNode=%p, newNodeSec=%ld", pNode, newNodeSec);
	while (pNode != NULL)
	{
		delta = pNode->nodeTimeSec - newNodeSec;
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
				osTimerChainNode_t* pChainNode = (osTimerChainNode_t*) osmalloc1(sizeof(osTimerChainNode_t), NULL);
				if(pChainNode == NULL)
				{
					perror("could not allocate pChainNode");
					return NULL;
				}

				pChainNode->nodeTimeSec = newNodeSec;
				pChainNode->nodeId = newNodeSec / OS_TIMEOUT_SUB_CHAIN_TIME + 1;
				pChainNode->pTimerSubChain = NULL;
				pChainNode->pNext = pNode;
				
				pPrevNode->pNext = pChainNode;
		
				mdebug(LM_TIMER, "a new TimerChainNode is created, pChainNode=%p, nodeTimeSec=%ld, nodeId=%d", pChainNode, pChainNode->nodeTimeSec, pChainNode->nodeId);		
				return pChainNode;
			}

			break;
		}
	}
	
	// if we are here, we are at the end of the chain list
	if(pNode == NULL && isForceInsert)
	{
		osTimerChainNode_t* pChainNode = (osTimerChainNode_t*) osmalloc1(sizeof(osTimerChainNode_t), NULL);
		if(pChainNode == NULL)
		{
			perror("could not allocate pChainNode");
			return NULL;
		}

		pChainNode->nodeTimeSec = newNodeSec;
		pChainNode->nodeId = newNodeSec / OS_TIMEOUT_SUB_CHAIN_TIME + 1;
		pChainNode->pTimerSubChain = NULL;
		pChainNode->pNext = NULL;
				
		pPrevNode->pNext = pChainNode;
				
		mdebug(LM_TIMER, "a new TimerChainNode is created in the end, pChainNode=%p, nodeTimeSec=%ld, nodeId=%d", pChainNode, pChainNode->nodeTimeSec, pChainNode->nodeId);
		return pChainNode;	
	}
	
	return NULL;
}


static osTimerSubChainNode_t* osTimerFindSubChainNodeById(osTimerChainNode_t* pChainNode, uint32_t subNodeId, osTimerSubChainNode_t** ppPrevSubNode)
{
    if(pChainNode == NULL)
    {
        return NULL;
    }

    osTimerSubChainNode_t* pNode = pChainNode->pTimerSubChain;
	*ppPrevSubNode = NULL;
    while (pNode != NULL)
    {
        mdebug(LM_TIMER, "pChainNode->pTimerSubChain=%p", pNode);
		if(pNode->nodeId == subNodeId)
		{
			return pNode;
		}
           
		*ppPrevSubNode = pNode; 
        pNode = pNode->pNext;
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
		logError("the msec(%ld) > OS_TIMEOUT_SUB_CHAIN_TIME (%d)", mSec, OS_TIMEOUT_SUB_CHAIN_TIME);
		return NULL;
	}
	
	time_t newNodeMsec = (mSec / OS_TIMEOUT_SUB_CHAIN_INTERVAL) * OS_TIMEOUT_SUB_CHAIN_INTERVAL;
	
	osTimerSubChainNode_t* pNode = pChainNode->pTimerSubChain;
	osTimerSubChainNode_t* pPrevNode = NULL;
	while (pNode != NULL)
	{
		mdebug(LM_TIMER, "pChainNode->pTimerSubChain=%p", pNode);
		time_t delta = pNode->nodeTimeMSec - newNodeMsec;
		if(delta < 0)
		{
			pPrevNode = pNode;
			pNode = pNode->pNext;
		}
		else if (delta == 0)
		{
			mdebug(LM_TIMER, "a subchain node already exists, nodeTimeMSec=%ld", pNode->nodeTimeMSec);
			return pNode;
		}
		else
		{
			if (isForceInsert)
			{
				osTimerSubChainNode_t* pSubChainNode = (osTimerSubChainNode_t*) osmalloc1(sizeof(osTimerSubChainNode_t), NULL);
				if(pSubChainNode == NULL)
				{
					logError("could not allocate pSubChainNode");
					return NULL;
				}

				pSubChainNode->nodeTimeMSec = newNodeMsec;
				pSubChainNode->nodeId = mSec / OS_TIMEOUT_SUB_CHAIN_INTERVAL + 1;
				pSubChainNode->eventCount = 0;
				pSubChainNode->pTimerEvent = NULL;
				pSubChainNode->pLastTimerEvent = NULL;
				pSubChainNode->pNext = pNode;
			
				if(pPrevNode == NULL)
				{
					pChainNode->pTimerSubChain = pSubChainNode;
				}
				else
				{	
					pPrevNode->pNext = pSubChainNode;
				}
			
				mdebug(LM_TIMER, "a new subchain node is created, nodeTimeMSec=%ld, nodeId=%d", pSubChainNode->nodeTimeMSec, pSubChainNode->nodeId);
				return pSubChainNode;
			}
			
			break;
		}
	}	
		
	// if we are here, we are at the end of the chain list
	if (pNode == NULL && isForceInsert)
	{
		//use osmalloc1 to not print out the memory alloc info
		osTimerSubChainNode_t* pSubChainNode = (osTimerSubChainNode_t*) osmalloc1(sizeof(osTimerSubChainNode_t), NULL);
		if(pSubChainNode == NULL)
		{
			logError("could not allocate pSubChainNode");
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

		mdebug(LM_TIMER, "a new subchain node is created, pSubChainNode=%p, nodeTimeMSec=%ld, nodeId=%d", pSubChainNode, pSubChainNode->nodeTimeMSec, pSubChainNode->nodeId);
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

	//use osmalloc1 to hide the memory alloc info	
	osTimertEvent_t* pTimerEvent = (osTimertEvent_t*) osmalloc1(sizeof(osTimertEvent_t), NULL);
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
		
	mdebug(LM_TIMER, "a new timerEvent is created, timerId=%lx", pTimerEvent->timerId);	
	return pTimerEvent;
}
	
	
static uint64_t osChainInsertTimerEvent(osTimerChainNode_t* pChainNode, time_t diffMsec, void* pData)
{
	if(diffMsec >= OS_TIMEOUT_SUB_CHAIN_TIME*1000)
	{
		warn("diffMsec(%ld) >= OS_TIMEOUT_SUB_CHAIN_TIME(%d)", diffMsec, OS_TIMEOUT_SUB_CHAIN_TIME*1000);
		return 0;
	}
		
	osTimerSubChainNode_t* pNode = osTimerFindSubChainNode(pChainNode, true, diffMsec);
	if(pNode == NULL)
	{
		perror("could not allocate pSubChainNode");
		return 0;
	}

	mdebug(LM_TIMER, "subChainNode=%p, nodeTimeMSec=%d", pNode, pNode->nodeTimeMSec);		
	pNode->eventCount++;
		
	osTimertEvent_t* pTimerEvent = osTimerEventCreate(pChainNode, pNode, pData);
	if(pTimerEvent == NULL)
	{
		logError("could not allocate pTimerEvent");
		return 0;
	}		
			
	if(pNode->pTimerEvent == NULL)
	{
		pNode->pTimerEvent = pTimerEvent;
		pNode->pLastTimerEvent = pTimerEvent;
		pTimerEvent->pNext = NULL;
	}
	else
	{
		pNode->pLastTimerEvent->pNext = pTimerEvent;
		pNode->pLastTimerEvent = pTimerEvent;
		pTimerEvent->pNext = NULL;
	}	

	return pTimerEvent->timerId;
}


//only print out if the LM_TIMER module is configured on DEBUG level
void osTimerListSubChainNodes(osTimerChainNode_t* pNode)
{
	//only print out if the LM_TIMER module is configured on DEBUG level
	if(osDbg_isBypass(DBG_DEBUG, LM_TIMER))
	{
		return;
	}
		
	if(!pNode)
	{
		pNode = pTimerChain;
		if(!pNode)
		{
			mdebug(LM_TIMER, "timerSubNode list:\npTimeNode=NULL.");
			return;
		}
	}

	//size of 1080 is based on the 10 * each_subNode_print_size (which is current 107).  If each_subNode_print_size changes, the allocated buffer size shall also change.
	char prtBuffer[1080];
	int count=0, n=0;
	osTimerSubChainNode_t* pSubNode = pNode->pTimerSubChain;
	while(pSubNode)
	{
		n += sprintf(&prtBuffer[n], "pSubChainNode=%p, pNext=%p, nodeTimeMSec=%ld, eventCount=%d\n", pSubNode, pSubNode->pNext, pSubNode->nodeTimeMSec, pSubNode->eventCount);
		if(++count == 10)
		{
			prtBuffer[n] = 0;
			mdebug(LM_TIMER, "timerSubNode list:\npTimeNode=%p, nodeTimeSec=%ld, nodeId=%d\n%s", pNode, pNode->nodeTimeSec, pNode->nodeId, prtBuffer);
			n = 0;
			count =0;
		}

		pSubNode = pSubNode->pNext;
	}

	if(count >0)
	{
		prtBuffer[n] = 0;
        mdebug(LM_TIMER, "timerSubNode list:\npTimeNode=%p, nodeTimeSec=%ld, nodeId=%d\n%s", pNode, pNode->nodeTimeSec, pNode->nodeId, prtBuffer);
	}
	else if(pNode->pTimerSubChain == NULL)
	{
		mdebug(LM_TIMER, "timerSubNode list:\npTimeNode=%p, nodeTimeSec=%ld, nodeId=%d\nNo timerSubNode.");
	}
}	

		
