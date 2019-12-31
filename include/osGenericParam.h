/**
 * @file osGenericParam.h  Interfaces for generic parameter decoding
 *
 * Copyright (C) 2019 InterLogic
 */


#ifndef _OS_GENERIC_PARAM_H
#define _OS_GENERIC_PARAM_H

#include <stdarg.h>



/* param */
typedef void (osGParam_fmt_h)(const osPointerLen_t *name, const osPointerLen_t *val,
			   void *arg);

bool osGenericParam_isExist(const osPointerLen_t *pl, const char *pname);
bool osGenericParam_get(const osPointerLen_t *pl, const char *pname, osPointerLen_t *val);
void osGenericParam_apply(const osPointerLen_t *pl, osGParam_fmt_h *ph, void *arg);


#endif
