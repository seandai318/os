/********************************************************
 * Copyright (C) 2020 Sean Dai
 *
 * @file osXmlParserLb.h
 * header file for osXmlParserLb.c
 ********************************************************/



#ifndef _OS_XML_PARSER_LIB_H
#define _OS_XML_PARSER_LIB_H


#include "osPL.h"

#include "osXmlParser.h"
#include "osXmlParserData.h"


//for function osXml_parseTag()
typedef enum {
    OS_XSD_TAG_INFO_START,
    OS_XSD_TAG_INFO_TAG_COMMENT,
    OS_XSD_TAG_INFO_BEFORE_TAG_INSIDE_QUOTE,
    OS_XSD_TAG_INFO_TAG_START,
    OS_XSD_TAG_INFO_TAG,
    OS_XSD_TAG_INFO_CONTENT_NAME_START,
    OS_XSD_TAG_INFO_CONTENT_NAME,
    OS_XSD_TAG_INFO_CONTENT_NAME_STOP,
    OS_XSD_TAG_INFO_CONTENT_VALUE_START,
    OS_XSD_TAG_INFO_CONTENT_VALUE,
    OS_XSD_TAG_INFO_END_TAG_SLASH,
} osXsdCheckTagInfoState_e;


//this data structure used to help xml parsing against its xsd
typedef struct osXsd_elemPointer {
    osXsdElement_t* pCurElem;       //the current working xsd element
    struct osXsd_elemPointer* pParentXsdPointer;    //the parent xsd pointer
    int curIdx;                     //which idx in assignedChildIdx[] that the xsd element is current processing, used in for the ordering presence of sequence deposition
    bool  assignedChildIdx[OS_XSD_COMPLEX_TYPE_MAX_ALLOWED_CHILD_ELEM]; //if true, the list idx corresponding child element value has been assigned
} osXsd_elemPointer_t;



osXsdElement_t* osXsd_createAnyElem(osPointerLen_t* pTag, bool isRootAnyElem);
bool isExistXsdAnyElem(osXsd_elemPointer_t* pXsdPointer);
int osXml_tagCmp(osPointerLen_t* pNsAlias, char* str, int strLen, osPointerLen_t* pTag);
int osXml_xsTagCmp(char* str, int strlen, osPointerLen_t* pTag);
bool osXml_singleDelimitMatch(const char* str, int strlen, char delimit, osPointerLen_t* tag, osPointerLen_t* pFirstSection);


#endif
