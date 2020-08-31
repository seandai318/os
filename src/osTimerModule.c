//Copyright (c) 2020, 2019, Sean Dai
//
//This file is the timer module thread entry.  Other threads requiring the timer service register to
//the timer module.  Each client can have different timeout granuality.  The timer module sets up a timeout
//signal with a minimal interval.  When the signal calls, the time module will go through all clients to 
//check if that client shall be notified of the timeout, and do so when the client times out.

#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <error.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <errno.h>
#include <pthread.h>

#include "osTimerModule.h"
#include "osResourceMgmt.h"
#include "osDebug.h"
#include "osMemory.h"


typedef struct timerClientPriv {
	int pipeId;
	struct timerClientPriv* pNext;
} osTimerClientPriv_t;


typedef struct osTimerSlotPriv {
	int maxCount;
	int count;
	osTimerClientPriv_t* pClient;
} osTimerSlotPriv_t;


static void timerHandler( int sig, siginfo_t *si, void *uc );
static int osStartTicking(int expireMS, int intervalMS );
static void osTimerRegisterClient(osTimerModuleMsg_t* pTimerMsg);


static osTimerSlotPriv_t* osTimerSlot;
static int timerEpFd;

int osTimerModuleInit(int* timerWriteFd)
{
    struct epoll_event event;
    int pipefd[2];

    timerEpFd = epoll_create1(0);
    if(timerEpFd == -1)
    {
        logError("Failed to create epoll file descriptor.");
        return -1;
    }

    event.events = EPOLLIN|EPOLLET|EPOLLONESHOT;

    if(pipe2(pipefd, O_NONBLOCK) == -1)
    {
        logError("pipe2 fails");
        return -1;
    }

	*timerWriteFd = pipefd[1];
    event.data.fd = pipefd[0];

    //---printf("debug, TimerResource, epollfd=%d, readfd=%d, writefd=%d\n", timerEpFd, pipefd[0], pipefd[1]);
    if(epoll_ctl(timerEpFd, EPOLL_CTL_ADD, pipefd[0], &event))
    {
        logError("Failed to add file descriptor to epoll.");
        close(timerEpFd);
        return -1;
    }
	logInfo("pipefd(%d) is added into epoll fd(%d).", pipefd[0], timerEpFd);

    osTimerSlot = (osTimerSlotPriv_t*) osmalloc(OS_TIMER_MAX_TIMOUT_MULTIPLE * sizeof(osTimerSlotPriv_t), NULL);

    for (int i=0; i< OS_TIMER_MAX_TIMOUT_MULTIPLE; i++)
    {
        osTimerSlot[i].maxCount = i+1;
        osTimerSlot[i].count = 0;
        osTimerSlot[i].pClient = NULL;
    }

    return 0;
}


void* osStartTimerModule(void* pIPCArgInfo)
{
	int eventCount = 0;
	char buffer[OS_TIMER_MODULE_MSG_MAX_BYTES];
	struct epoll_event event, events[OS_MAX_FD_PER_PROCESS];
//	int readFd;

#if 0	
	if(pIPCArgInfo == NULL)
	{
		error(0,0,"IPC info is empty when starting timer module");
		return NULL;
	}
	
	osIPCArg_t* pIPCArg = (osIPCArg_t*)pIPCArgInfo;
//	int timerEpFd = pIPCArg->pIPCInfo->timerEpFd;
//	if(timerEpFd == -1)
//	{
//		error(0,0,"epoll FD is not defined when starting timer module");
//		return NULL;
//	}
	
	if(pIPCArg->pIPCInfo->fdNum != 1)
	{
		error(0,0,"fdNum is not 1 in IPC info when starting timer module");
		return NULL;
	}
		
	osFdInfo_t* pFdInfo = &pIPCArg->pIPCInfo->fdInfo[0];
	if(pFdInfo->fdType != OS_FD_PIPE)
	{
		error(0,0,"the IPC type is not PIPE when starting timer module");
		return NULL;
	}
	
	readFd = pFdInfo->readFd;
	if(readFd == -1)
	{
		error(0,0,"the readFd is not defined when starting timer module");
		return NULL;
	}
#endif

    logInfo("threadId = %u.", (unsigned int)pthread_self());

    osStartTicking(OS_TIMER_MIN_TIMEOUT_MS, OS_TIMER_MIN_TIMEOUT_MS);

	//printf("debug, after timerInit\n");
	int ipcMsgSize = sizeof(osIPCMsg_t);
	int timerModuleMsgSize = sizeof(osTimerModuleMsg_t);
	while (1) 
	{
		//printf("debug, timer waiting for msg\n");
        eventCount = epoll_wait(timerEpFd, events, OS_MAX_FD_PER_PROCESS, 30000);
		if(eventCount > 1)
		{
			logError("event count=%d, timer module shall only have 1 event count", eventCount);
			continue;
		}
		
		//printf("debug, timer, eventCount=%d\n", eventCount);
        for(int i = 0; i < eventCount; i++) 
		{
			event.events = EPOLLIN|EPOLLET|EPOLLONESHOT;
 			event.data.fd = events[i].data.fd;
			epoll_ctl(timerEpFd, EPOLL_CTL_MOD, events[i].data.fd, &event);
	
			//---mdebug(LM_TIMER, "message is received, timerEpFd=%d, dataFd=%d\n", timerEpFd, events[i].data.fd);	
			while (1) 
			{
				int n;
				n = read(events[i].data.fd, (char *)buffer, ipcMsgSize);

				if(n == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
				//	printf("EAGAIN received\n");
					break;
				}

				//printf("debug, timerMgr, buf0=%x, buf1=%x, buf2=%x, buf3=%x, buf4=%x, buf5=%x, buf6=%x, buf7=%x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
				buffer[n] = '\0';
				//tmdebug(LM_TIMER,_t * pDebug=(tmdebug(LM_TIMER,_t*) buffer;
				//printf("debug, timerMgr, pDebug->ptr=%p, value=%d\n", pDebug->ptr, *(int*)pDebug->ptr);
				//osPipePtr_t* pPipePtr = (osPipePtr_t*) buffer;
				osIPCMsg_t* pIPCMsg = (osIPCMsg_t*) buffer;
				//---mdebug(LM_TIMER, "pIPCMsg=%p, pIPCMsg->interface=%d\n", (void*)pIPCMsg, pIPCMsg->interface);
				if(pIPCMsg->interface == OS_TIMER_ALL)
				{
					osTimerModuleMsg_t* pTimerModuleMsg = (osTimerModuleMsg_t*) pIPCMsg->pMsg;
				
					if(pTimerModuleMsg->msgType == OS_TIMER_MODULE_REG) 
					{	
						osTimerRegisterClient(pTimerModuleMsg);
						continue;
                			}
				}	
						
				logError("epoll received message with wrong interface type, %d", pIPCMsg->interface);
			}
		}
	}
}


static void osTimerRegisterClient(osTimerModuleMsg_t* pMsg)
{
	//printf("debug, registerClient\n");	
	if (pMsg->timeoutMultiple > OS_TIMER_MAX_TIMOUT_MULTIPLE)
	{
		//not allowed, use the OS_MAX_TIMEOUT_MULTIPLE, and notify the clientPipeId
		pMsg->timeoutMultiple = OS_TIMER_MAX_TIMOUT_MULTIPLE;
	}

	osIPCMsg_t ipcMsg;
//	osTimerModuleMsg_t* pMsg1=(osTimerModuleMsg_t*) malloc(sizeof(osTimerModuleMsg_t*));
	ipcMsg.interface = OS_TIMER_ALL;
	pMsg->msgType = OS_TIMER_MODULE_REG_RESPONSE;
	ipcMsg.pMsg = (void*)pMsg;
	//---mdebug(LM_TIMER, "writeFd=%d, pMsg=%p, msgType=%d, reg_response=%d\n", pMsg->clientPipeId, ipcMsg.pMsg, ((osTimerModuleMsg_t*)ipcMsg.pMsg)->msgType, OS_TIMER_MODULE_REG_RESPONSE);
	write(pMsg->clientPipeId, (void*) &ipcMsg, sizeof(osIPCMsg_t));
	
	//to-do sem get, to protect confliction with timer interrupt
	osTimerClientPriv_t* pClient = (osTimerClientPriv_t*) osmalloc(sizeof(osTimerClientPriv_t), NULL);
	pClient->pipeId = pMsg->clientPipeId;
		
	osTimerSlotPriv_t* pSlot = &osTimerSlot[pMsg->timeoutMultiple-1];
	pClient->pNext = pSlot->pClient;
	pSlot->pClient = pClient;
	//to-do sem release
}
	
osIPCMsg_t ipcMsg;
#if 0	//debug garbage printout
osTimerModuleMsg_t timerModule[100];
static int itimer = 0;
#endif
static int itimer = 0;
static void timerHandler( int sig, siginfo_t *si, void *uc )
{
//    debug("threadId = %u.", (unsigned int)pthread_self());

	for(int i=0; i<OS_TIMER_MAX_TIMOUT_MULTIPLE; i++)
	{
		if(++osTimerSlot[i].count >= osTimerSlot[i].maxCount)
		{
			osTimerSlot[i].count = 0;
			
			//browse the link list
			osTimerClientPriv_t* pClient = osTimerSlot[i].pClient;
			while (pClient != NULL)
			{
#if 0	//debug garbage printout
				osTimerModuleMsg_t* pMsg = &timerModule[itimer++];
				if (itimer >= 99)
				{
					itimer = 0;
				}
#else
//				osTimerModuleMsg_t* pMsg = (osTimerModuleMsg_t*) malloc(sizeof(osTimerModuleMsg_t));
#endif
//				pMsg->msgType = OS_TIMER_MODULE_EXPIRE;
				//---mdebug(LM_TIMER, "timerExpire, writeFd=%d, msgType=%d\n", pClient->pipeId, OS_TIMER_MODULE_EXPIRE);
			//	osIPCMsg_t ipcMsg;
				ipcMsg.interface = OS_TIMER_TICK;
				ipcMsg.pMsg = NULL;
				write(pClient->pipeId, (void*) &ipcMsg, sizeof(osIPCMsg_t));
			
				pClient = pClient->pNext;
			}
		}
//		itimer++;
	}
	itimer++;
}


static int osStartTicking(int expireMS, int intervalMS )
{
        struct sigevent te;
        struct itimerspec its;
        struct sigaction sa;
        int sigNo = SIGRTMIN;
	timer_t timerID;

        /* Set up signal handler. */
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = timerHandler;
        sigemptyset(&sa.sa_mask);
//		pthread_sigmask(SIG_UNBLOCK, &sa.sa_mask, NULL);
        if (sigaction(sigNo, &sa, NULL) == -1) 
		{
        	logError("sigaction fails");
        }

        /* Set and enable alarm */
        te.sigev_notify = SIGEV_SIGNAL;
        te.sigev_signo = sigNo;
        te.sigev_value.sival_ptr = &timerID;

	//printf("debug, before timer_create\n");
        timer_create(CLOCK_REALTIME, &te, &timerID);
	//printf("debug, after timer_create\n");
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = intervalMS * 1000000;
        its.it_value.tv_sec = 0;
        its.it_value.tv_nsec = expireMS * 1000000;
	//printf("before timer_settime\n");
        timer_settime(timerID, 0, &its, NULL);

	//printf("debug, endof osStartTicking\n");
        return 1;
}
