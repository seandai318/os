/**
 * @file osLB.h  load balance header file
 *
 * Copyright (C) 2019-2021, SeanDai
 */


#ifndef __OS_LB_H
#define __OS_LB_H

#include "osPL.h"


typedef struct {
	int	to_do;
} lbAnchorInfo_t;


lbAnchorInfo_t* lb_getAnchorInfo(osPointerLen_t* pUser);

#endif
