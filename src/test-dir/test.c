#include <stdio.h>
#include "osPrintf.h"
#include "osSockAddr.h"
#include "osDebug.h"

void main()
{
    osDbg_init(DBG_DEBUG, DBG_ALL);

	struct sockaddr_in sockaddr;
	osIpPort_t ipPort;
	ipPort.ip.p = "10.12.13.14";
	ipPort.ip.l=11;
	ipPort.port = 5060;
	osStatus_e status = osConvertPLton(&ipPort, true, &sockaddr);
	sockaddr.sin_port=0;
	logError("sockaddr=%S", &sockaddr);
}

