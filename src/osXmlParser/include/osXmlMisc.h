/********************************************************
 * Copyright (C) 2020 Sean Dai
 *
 * @file osXmlMisc.h
 * header file for osXmlMisc.c
 ********************************************************/


#ifndef _OS_XML_MISC_H
#define _OS_XML_MISC_H


#include "osTypes.h"
#include "osPL.h"
#include "osMBuf.h"


bool osXml_findPattern(osMBuf_t* pXmlBuf, osPointerLen_t* pPattern, bool isAdvancePos);
bool osXml_findWhiteSpace(osMBuf_t* pBuf, bool isAdvancePos);
int osXml_tagCmp(osPointerLen_t* pNsAlias, char* str, int strLen, osPointerLen_t* pTag);
int osXml_xsTagCmp(char* str, int strlen, osPointerLen_t* pTag);
bool osXml_singleDelimitMatch(const char* str, int strlen, char delimit, osPointerLen_t* tag, osPointerLen_t* pFirstSection);


#endif
