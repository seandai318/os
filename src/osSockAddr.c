/********************************************************
 * Copyright (C) 2019, 2020 Sean Dai
 *
 * @file osSockAddr.c  Socket address format translation
 ********************************************************/

#include <arpa/inet.h>
#include <string.h>

#include "osDebug.h"
#include "osSockAddr.h"


osStatus_e osConvertPLton(const osIpPort_t* pIpPort, bool isIncludePort, struct sockaddr_in* pSockAddr)
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

    if(osPL_strcpy(&pIpPort->ip.pl, ip, INET_ADDRSTRLEN) != 0)
    {
        logError("fails to perform osPL_strcpy for IP(%r).", &pIpPort->ip.pl);
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

	pIpPort->ip.pl.p = pIpPort->ipMem;
	pIpPort->ip.pl.l = strlen(pIpPort->ipMem);
	pIpPort->ip.isPDynamic = false;

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


bool osIsIpv4(osPointerLen_t* pAddr)
{
	if(!pAddr)
	{
		return false;
	}

	if(pAddr->l > 15 || pAddr->l < 7)
	{
		return false;
	}

	int pos = 0;
	bool isFirstDigit = true;
	bool isDotFound = false;
	//totally 4 segement for ipv4
	for(int i=0; i<4; i++)
	{
		isFirstDigit = true;
		//check insdie each segement
		for(int j=0; j<3; j++)
		{
			//first char inside a segement must be between 1-9
			if(isFirstDigit)
			{
				if(pAddr->p[pos] < '1' || pAddr->p[pos] > '9')
				{
					return false;
				}
				else
				{
					isFirstDigit = false;
				}
			}
			else
			{
				if(pAddr->p[pos] == '.')
				{
					isDotFound = true;
					break;
				}
				else if(pAddr->p[pos] < '0' || pAddr->p[pos] > '9')
				{
					return false;
				}
			}

			++pos;
		}

		//except for last segement, after checking xxx, check if there is '.'
        if(!isDotFound && i < 3)
        {
            if(pAddr->p[pos++] != '.')
            {
                return false;
            }
        }
	}
			
	return true;
}
