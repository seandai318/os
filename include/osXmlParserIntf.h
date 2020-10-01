/* Copyright <c) 2020, Sean Dai
 * xml parser interface
 */

#ifndef _OS_XML_PARSER_INTF_H
#define _OS_XML_PARSER_INTF_H

#include "osTypes.h"
#include "osPL.h"
#include "osMBuf.h"


typedef enum {
	OS_XML_DATA_TYPE_INVALID,
	OS_XML_DATA_TYPE_XS_BOOLEAN,
	OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE,
	OS_XML_DATA_TYPE_XS_SHORT,
	OS_XML_DATA_TYPE_XS_INTEGER,
	OS_XML_DATA_TYPE_XS_LONG,
	OS_XML_DATA_TYPE_XS_STRING,	//xs:anyURI falls in this enum
	OS_XML_DATA_TYPE_NO_XS,		//a collective type that represents  all no XS defined type, like simple and complex
	OS_XML_DATA_TYPE_SIMPLE,
	OS_XML_DATA_TYPE_COMPLEX,
	OS_XML_DATA_TYPE_ANY,		//for elements treated as <xs:any>
} osXmlDataType_e;



typedef struct {
    int eDataName;          //different app use different enum, like sipConfig_xmlDataName_e, diaConfig_xmlDataName_e, etc.
    osPointerLen_t dataName;
    osXmlDataType_e dataType;	 //for simpleType, this is set to OS_XML_DATA_TYPE_SIMPLE by user, and when callback, set to the XS type (like int, etc.)by the xmlparser
    union {
        bool xmlIsTrue;
        uint64_t xmlInt;
        osPointerLen_t xmlStr;
    };
} osXmlData_t;


typedef void (*osXmlDataCallback_h)(osXmlData_t* pXmlData);


//app provided info for osXmlDataCallback_h callback
typedef struct {
	bool isLeafOnly;		//whether app only wants leaf node value
	bool isUseDefault;		//whether app wants to use the default value for elements that have xsd minOccurs=0 and not appear in xml file
	osXmlDataCallback_h xmlCallback;	//if !NULL, the xml module will call this function each time a matching xml element is found, otherwise, the values will be stored in xmlData
	osXmlData_t* xmlData;
	int maxXmlDataSize;
} osXmlDataCallbackInfo_t;



//get xml leaf node value based on the xsd and xml files
osStatus_e osXml_getLeafValue(char* fileFolder, char* xsdFileName, char* xmlFileName, osXmlDataCallbackInfo_t* callbackInfo);
bool osXml_isXsdValid(osMBuf_t* pXsdBuf);
bool osXml_isXmlValid(osMBuf_t* pXmlBuf, osMBuf_t* pXsdBuf, osXmlDataCallbackInfo_t* callbackInfo);


#endif
