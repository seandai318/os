#ifndef _OS_RESOURCE_MGMT_H
#define _OS_RESOURCE_MGMT_H


#define	OS_MAX_SUB_THREAD_NUM	10
#define OS_MAX_COMM_THREAD_NUM	5
#define OS_MAX_COMM_SOCKET_NUM	100


//typedef struct osPipePtr {
//	void* ptr;
//} osPipePtr_t;


typedef enum osInterface {
	OS_SIP = 1,
	OS_Iu,
	OS_GTPn,
	OS_GTPu,
	OS_TIMER_ALL = 100,
} osInterface_e;


typedef enum osInterModuleIntf {
	OS_TIMER_SUB,
	OS_TIMER_SIP,
	OS_TIMER_COMM,
	OS_COMM_SUB,
	OS_COMM_SIP,
	OS_ALL,
} osInterModuleIntf_e;


typedef enum osFdType {
	OS_FD_UDP=1,
	OS_FD_TCP,
	OS_FD_PIPE,
	OS_MAX_FD_TYPE,
} osFdType_e;


typedef enum osModuleName {
	OS_MODULE_TIMER,
	OS_MODULE_COMM,
	OS_MODULE_SUB,
	OS_MODULE_SIP,
} osModuleName_e;


//used as i in  osFdInfo_t fdInfo[i], the index value larger than 2 is for each worker thread, e.g., in comm thread, idx=3, 4 are for the writeFd of other worker thread 0 and worker thread 1
typedef enum osFdIndex {
	OS_FD_TIMER_LOCAL_IDX = 0,
	OS_FD_TIMER_PEER_IDX = 1,
	OS_FD_COMM_IDX = 2,
	OS_MAX_FD_PER_PROCESS = 100,
} osFsIndex_e;


//if a Fd=-1, means this Fd either is not used by the process/thread, or it will be received in other channel
typedef struct osFdInfo {
	int readFd;
	int writeFd;
	osInterface_e interface;
	osFdType_e fdType;
	void* pData;
} osFdInfo_t;


//if epollFd = -1, the process/thread needs to create a epollFd on its own
//fdInfo[0] is for the local timer read/write, fdInfo[1] is for the timer thread write, fdInfo[2] is for the worker<->comm read/write	
typedef struct osIPC {
	int epollFd;
	int fdNum;
	osFdInfo_t fdInfo[OS_MAX_FD_PER_PROCESS]; 
} osIPC_t;


typedef struct osIPCArg {
	osIPC_t* pIPCInfo;
	void* pData;
} osIPCArg_t;


typedef struct osIPCMsg {
	osInterface_e interface;
	void* pMsg;
} osIPCMsg_t;


typedef struct osResourceCommWorker {
	int workerThreadNum;
//	int commOwnFdNum;
	osIPC_t* pWorkerIPC;
	osIPC_t* pCommIPC;
} osResourceCommWorker_t;


typedef struct osResourceTimerWorker {
	osModuleName_e workerType;
	int workerThreadNum;
	osIPC_t* pWorkerIPC;
	osIPC_t* pTimerIPC;
} osResourceTimerWorker_t;
//for pipe, the pipe message structure will be destroyed by the pipe receiver, but the pUserData part cleanup will be the responsibility of the piper sender


static int osFDArray[OS_MAX_FD_PER_PROCESS];
static int osFDNum=0;

int osSetFDType(int fd, osFdType_e fdType)
{
    //TODO, use hash
}

osFdType_e osGetFDType(int fd)
{
    //TODO, use hash
}

#endif
