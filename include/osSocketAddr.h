/**
 * @file osSocketAddr.h  Interface to Socket Address
 *
 * Copyright (C) 2019 InterLogic
 */


#ifndef OS_SOCKET_ADDR_H
#define OS_SOCKET_ADDR_H


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "osPL.h"



/** Socket Address flags */
typedef enum osSA_flag {
	SA_ADDR      = 1<<0,
	SA_PORT      = 1<<1,
	SA_ALL       = SA_ADDR | SA_PORT
} osSA_flag_e;


/** Defines a Socket Address */
typedef struct osSocketAddr {
	union {
		struct sockaddr sa;
		struct sockaddr_in in;
#ifdef HAVE_INET6
		struct sockaddr_in6 in6;
#endif
		uint8_t padding[28];
	} u;
	socklen_t len;
} osSocketAddr_t;


void     osSA_init(osSocketAddr_t *sa, int af);
int      osSA_set(osSocketAddr_t *sa, const osPointerLen_t *addr, uint16_t port);
int      osSA_setStr(osSocketAddr_t *sa, const char *addr, uint16_t port);
void     osSA_setin(osSocketAddr_t *sa, uint32_t addr, uint16_t port);
void     osSA_setin6(osSocketAddr_t *sa, const uint8_t *addr, uint16_t port);
int      osSA_setSocketAddr(osSocketAddr_t *sa, const struct sockaddr *s);
void     osSA_setPort(osSocketAddr_t *sa, uint16_t port);
int      osSA_decode(osSocketAddr_t *sa, const char *str, size_t len);

int      osSA_af(const osSocketAddr_t *sa);
uint32_t osSA_in(const osSocketAddr_t *sa);
void     osSA_in6(const osSocketAddr_t *sa, uint8_t *addr);
int      osSA_ntop(const osSocketAddr_t *sa, char *buf, int size);
uint16_t osSA_port(const osSocketAddr_t *sa);
bool     osSA_isset(const osSocketAddr_t *sa, int flag);
uint32_t osSA_hash(const osSocketAddr_t *sa, int flag);

void     osSA_cpy(osSocketAddr_t *dst, const osSocketAddr_t *src);
bool     osSA_cmp(const osSocketAddr_t *l, const osSocketAddr_t *r, int flag);

bool     osSA_isLinkLocal(const osSocketAddr_t *sa);
bool     osSA_isLoopback(const osSocketAddr_t *sa);
bool     osSA_isAny(const osSocketAddr_t *sa);

struct osPrintf;
int      osSA_print_addr(struct osPrintf *pf, const osSocketAddr_t *sa);


#endif
