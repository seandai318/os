/* Copyright <c) 2020, Sean Dai
 * xml parser main function module
 */

#ifndef _OS_XML_PARSER_H
#define _OS_XML_PARSER_H

#include "osList.h"
#include "osTypes.h"
#include "osMBuf.h"
#include "osPL.h"

#include "osXmlParserData.h"


#define OSXML_IS_LWS(a) (!(a^0x20) || !(a^0x9) || !(a^0xa))
#define OS_XML_MAX_FILE_NAME_SIZE	160		//the maximum xml and xsd file name length


typedef struct {
	bool isDefaultNSXsdName;	//if true, the defaultNS is assigned with xsd name
    osList_t nsAliasList;       //each entry contains osXsd_nsAliasInfo_t
    osPointerLen_t defaultNS;
} osXml_nsInfo_t;


osStatus_e osXml_xmlCallback(osXsdElement_t* pElement, osPointerLen_t* value, osXmlDataCallbackInfo_t* callbackInfo);



#endif
