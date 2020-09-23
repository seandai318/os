/* Copyright <c) 2020, Sean Dai
 * xml parser for ComplexType
 */

#ifndef _OS_XML_PARSER_CTYPE_H
#define _OS_XML_PARSER_CTYPE_H

#include "osTypes.h"
#include "osMBuf.h"
#include "osList.h"

#include "osXmlParser.h"


typedef struct ctPointer {
    osXmlComplexType_t* pCT;
    int doneListIdx;        //listIdx that has already transversed
} osXsd_ctPointer_t;


osXmlComplexType_t* osXsdComplexType_parse(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pCtTagInfo, osXsdElement_t* pParentElem);
//osStatus_e osXsdComplexType_getAttrInfo(osList_t* pAttrList, osXmlComplexType_t* pCtInfo);
osStatus_e osXsdComplexType_getSubTagInfo(osXmlComplexType_t* pCtInfo, osXmlTagInfo_t* pTagInfo);
osStatus_e osXsd_transverseCT(osXsd_ctPointer_t* pCTPointer, osXmlDataCallbackInfo_t* callbackInfo);
void osXmlComplexType_cleanup(void* data);



#endif
