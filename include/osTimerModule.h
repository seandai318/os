//Copyright 2019, InterLogic

#ifndef _OS_TIMER_MODULE_H
#define _OS_TIMER_MODULE_H

#define OS_TIMER_MODULE_MSG_MAX_BYTES	100
#define OS_TIMER_MAX_TIMOUT_MULTIPLE	10
#define OS_TIMER_MIN_TIMEOUT_MS			50


typedef enum {
	OS_TIMER_MODULE_REG,
	OS_TIMER_MODULE_REG_RESPONSE,
	OS_TIMER_MODULE_EXPIRE,
} osTimerModuleMsgType_e;


typedef struct osTimerModuleMsg {
	osTimerModuleMsgType_e msgType;
	int clientPipeId;
	int timeoutMultiple;
} osTimerModuleMsg_t;


void* osStartTimerModule(void* pIFCInfo);


#endif
