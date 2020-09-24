/* Copyright <c) 2020, Sean Dai
 * xml parser main function module
 */

#ifndef _OS_XML_PARSER_H
#define _OS_XML_PARSER_H

#include "osList.h"
#include "osTypes.h"
#include "osMBuf.h"

#include "osXmlParserData.h"


#define OS_XSD_COMPLEX_TYPE_MAX_ALLOWED_CHILD_ELEM	50
#define OSXML_IS_LWS(a) (!(a^0x20) || !(a^0x9) || !(a^0xa))
#define OSXML_IS_XS_TYPE(a) (a != NULL && a->l >=3 && !(a->p[0]^'x') && !(a->p[1]^'s') && !(a->p[2]^':'))
#define OSXML_IS_COMMENT_START(p) (*p=='<' && *(p+1)=='!' && *(p+2)=='-' && *(p+3)=='-')
#define OSXML_IS_COMMENT_STOP(p) (*p=='>' && *(p-1)=='-' && *(p-2)=='-')

#define OS_XML_MAX_FILE_NAME_SIZE	160		//the maximum xml and xsd file name length


osStatus_e osXsd_elemCallback(osXsdElement_t* pXsdElem, osXmlDataCallbackInfo_t* callbackInfo);
osStatus_e osXml_parseTag(osMBuf_t* pBuf, bool isTagNameChecked, bool isXsdFirstTag, osXmlTagInfo_t** ppTagInfo, size_t* tagStartPos);
osXsdElement_t* osXsd_parseElement(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pTagInfo);
osXsdElement_t* osXsd_parseElementAny(osMBuf_t* pXmlBuf, osXmlTagInfo_t* pElemTagInfo);
osStatus_e osXmlElement_getAttrInfo(osList_t* pAttrList, osXsdElement_t* pElement);
bool osXml_isXsdElemSimpleType(osXsdElement_t* pXsdElem);
osXmlDataType_e osXsd_getElemDataType(osPointerLen_t* typeValue);
bool osXml_isDigitType(osXmlDataType_e dataType);
bool osXml_isXSSimpleType(osXmlDataType_e dataType);
void osXsdElement_cleanup(void* data);



#endif
