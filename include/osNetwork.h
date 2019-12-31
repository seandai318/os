#ifndef OS_NETWORK_H
#define OS_NETWORK_H

typedef enum osFdType {
    OS_FD_UDP=1,
    OS_FD_TCP,
    OS_FD_PIPE,
    OS_MAX_FD_TYPE,
} osFdType_e;


static int osFDArray[OS_MAX_FD_PER_PROCESS];
static int osFDNum=0;

int osSetFDType(int fd, osFdType_e fdType)
{
	//TODO, use hash
}

int osGetFDType(int fd)
{
	//TODO, use hash
}
	


#endif
