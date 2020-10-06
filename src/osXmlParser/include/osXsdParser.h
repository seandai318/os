#ifndef _OS_XSD_PARSER_H
#define _OS_XSD_PARSER_H

#include "osMBuf.h"
#include "osPL.h"
#include "osXmlParserData.h"


osXsdElement_t* osXsd_parseElement(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pTagInfo);
osXsdElement_t* osXsd_parseElementAny(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pElemTagInfo);
osStatus_e osXmlElement_getAttrInfo(osList_t* pAttrList, osXsdElement_t* pElement);
osXmlDataType_e osXsd_getElemDataType(osPointerLen_t* typeValue);
osStatus_e osXsd_elemCallback(osXsdElement_t* pXsdElem, osXmlDataCallbackInfo_t* callbackInfo);
osStatus_e osXsd_browseNode(osXsdElement_t* pXsdElem, osXmlDataCallbackInfo_t* callbackInfo);
osXsdNamespace_t* osXsd_parse(osMBuf_t* pXmlBuf);
osPointerLen_t* osXsd_getXSAlias();
void osXsd_setXSAlias(osPointerLen_t* pXsAlias);
void osXsdElement_cleanup(void* data);


#endif
