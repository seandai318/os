#include <arpa/inet.h>
#include <string.h>

#include "osDebug.h"
#include "osSockAddr.h"


osStatus_e osConvertPLton(osIpPort_t* pIpPort, bool isIncludePort, struct sockaddr_in* pSockAddr)
{
    osStatus_e status = OS_STATUS_OK;
    char ip[INET_ADDRSTRLEN]={};

    pSockAddr->sin_family = AF_INET;
    if(isIncludePort)
    {
        pSockAddr->sin_port = htons(pIpPort->port);
    }
    else
    {
        pSockAddr->sin_port = 0;
    }

    if(osPL_strcpy(&pIpPort->ip, ip, INET_ADDRSTRLEN) != 0)
    {
        logError("fails to perform osPL_strcpy for IP(%r).", pIpPort->ip);
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

    if(inet_pton(AF_INET, ip, &pSockAddr->sin_addr.s_addr) != 1)
    {
        logError("fails to perform inet_pton for IP(%s), errno=%d.", ip, errno);
        status = OS_ERROR_INVALID_VALUE;
        goto EXIT;
    }

EXIT:
    return status;
}


//if ip is null, the dotted ip is stored in a static allocated memory, which will be overwritten in next call
osStatus_e osConvertntoPL(struct sockaddr_in* pSockAddr, osIpPort_t* pIpPort)
{
	if(!pSockAddr || !pIpPort)
	{
		logError("null pointer, pSockAddr=%p, pIpPort=%p.", pSockAddr, pIpPort);
		return OS_ERROR_NULL_POINTER;
	}

	if(!inet_ntop(AF_INET, &pSockAddr->sin_addr, pIpPort->ipMem, INET_ADDRSTRLEN))
	{
		return OS_ERROR_INVALID_VALUE;
	}

	pIpPort->ip.p = pIpPort->ipMem;
	pIpPort->ip.l = strlen(pIpPort->ipMem);

	pIpPort->port = ntohs(pSockAddr->sin_port);
	
	return OS_STATUS_OK;
}


bool osIsSameSA(struct sockaddr_in* pSockAddr, struct sockaddr_in* pSockAddr1)
{
	if(!pSockAddr || !pSockAddr1)
	{
		return false;
	}

    //do not directly compare peer to avoid inconsistent in sin_zero[8]
	if(pSockAddr->sin_port == pSockAddr1->sin_port && pSockAddr->sin_addr.s_addr == pSockAddr1->sin_addr.s_addr)
	{
		return true;
	}

	return false;
}
