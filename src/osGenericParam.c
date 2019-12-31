/**
 * @file osGenericParm.c Generic parameter decoding
 *
 * Copyright (C) 2019 InterLogic
 */

#include "osTypes.h"
#include "osPrintf.h"
#include "osRegex.h"
#include "osPL.h"
#include "osGenericParam.h"


/**
 * Check if a semicolon separated parameter is present
 *
 * @param pl    PL string to search
 * @param pname Parameter name
 *
 * @return true if found, false if not found
 */
bool osGenericParam_isExist(const osPointerLen_t *pl, const char *pname)
{
	osPointerLen_t semi, eop;
	char expr[128];

	if (!pl || !pname)
		return false;

	//create a reg expression
	(void)osPrintf_buffer(expr, sizeof(expr), "[;]*[ \t\r\n]*%s[ \t\r\n;=]*", pname);

	if (osRegex(pl->p, pl->l, expr, &semi, NULL, &eop))
	{
		return false;
	}

	if (!eop.l && eop.p < pl->p + pl->l)
	{
		return false;
	}

	return semi.l > 0 || pl->p == semi.p;
}


/**
 * Fetch a semicolon separated parameter from a PL string
 *
 * @param pl    PL string to search
 * @param pname Parameter name
 * @param val   Parameter value, set on return
 *
 * @return true if found, false if not found
 */
bool osGenericParam_get(const osPointerLen_t *pl, const char *pname, osPointerLen_t *val)
{
	osPointerLen_t semi;
	char expr[128];

	if (!pl || !pname)
		return false;

	(void)osPrintf_buffer(expr, sizeof(expr), "[;]*[ \t\r\n]*%s[ \t\r\n]*=[ \t\r\n]*[~ \t\r\n;]+", pname);

	if (osRegex(pl->p, pl->l, expr, &semi, NULL, NULL, NULL, val))
	{
		return false;
	}

	return semi.l > 0 || pl->p == semi.p;
}


/**
 * Apply a function handler for each semicolon separated parameter
 *
 * @param pl  PL string to search
 * @param ph  Parameter handler
 * @param arg Handler argument
 */
void osGenericParam_apply(const osPointerLen_t *pl, osGParam_fmt_h *ph, void *arg)
{
	osPointerLen_t prmv, prm, semi, name, val;

	if (!pl || !ph)
	{
		return;
	}

	prmv = *pl;

	while (!osRegex(prmv.p, prmv.l, "[ \t\r\n]*[~;]+[;]*", NULL, &prm, &semi)) 
	{

		osPL_advance(&prmv, semi.p + semi.l - prmv.p);

		if (osRegex(prm.p, prm.l, "[^ \t\r\n=]+[ \t\r\n]*[=]*[ \t\r\n]*[~ \t\r\n]*", &name, NULL, NULL, NULL, &val))
		{
			break;
		}

		ph(&name, &val, arg);
	}
}
