#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h> 
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "osLocalDebug.h"

int osDebugGetLocalIP(char* interface, char* localIP)
{
    unsigned char ip_address[15];
    int fd;
    struct ifreq ifr;
     
	if(localIP == NULL)
	{
		return -1;
	}

	if (interface == NULL)
	{
		strcpy(localIP, "127.0.0.1");
		return 0;
	}

    /*AF_INET - to define network interface IPv4*/
    /*Creating soket for it.*/
    fd = socket(AF_INET, SOCK_DGRAM, 0);
     
    /*AF_INET - to define IPv4 Address type.*/
    ifr.ifr_addr.sa_family = AF_INET;
     
    /*eth0 - define the ifr_name - port name
    where network attached.*/
    memcpy(ifr.ifr_name, interface, IFNAMSIZ-1);
     
    /*Accessing network interface information by
    passing address using ioctl.*/
    ioctl(fd, SIOCGIFADDR, &ifr);
    /*closing fd*/
    close(fd);
     
    /*Extract IP Address*/
    strcpy(localIP,inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
     
    return 0;
}


char* osLocalDebugGetLocalIP()
{
	char* localIP = malloc(15);
	osDebugGetLocalIP(OS_LOCAL_INTERFACE, localIP);

	return localIP;
}
