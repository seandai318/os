/* Copyright <c) 2020, Sean Dai
 * xml parser interface
 */

#ifndef _OS_XML_PARSER_INTF_H
#define _OS_XML_PARSER_INTF_H

#include "osTypes.h"
#include "osPL.h"
#include "osList.h"
#include "osMBuf.h"


#define OS_XML_INVALID_EDATA_NAME	0x7FFFFFFF

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
    int eDataName;          //an entry from dataName enum, different app use different enum, like sipConfig_xmlDataName_e, diaConfig_xmlDataName_e, etc. app shall not use OS_XML_INVALID_EDATA_NAME as a valid enum value
    osPointerLen_t dataName;
    osXmlDataType_e dataType;	 //for simpleType, this is set to OS_XML_DATA_TYPE_SIMPLE by user, and when callback, set to the XS type (like int, etc.)by the xmlparser
	osPointerLen_t nsAlias;
	const osList_t* pNoXmlnsAttrList;	//no xmlns attributes
    union {	//controlled by dataType
        bool xmlIsTrue;
        uint64_t xmlInt;
        osPointerLen_t xmlStr;
    };
} osXmlData_t;


//user can use pnsAliasInfo, together with osXmlData_t.nsAlias, to get a data's ns info
//be noted if pXmlData is gotten from xsd due to osXmlDataCallbackInfo_t.isUseDefault = true, and xml instance
//does not contain the element, the nsAlias is from xsd, may not match with xml's, in other word, the nsAlias
//from xsd shall not be interpretted by pnsAliasInfo (which is gotten from xml instance).
typedef void (*osXmlDataCallback_h)(osXmlData_t* pXmlData, void* pnsAliasInfo);


//app provided info for osXmlDataCallback_h callback
typedef struct {
	bool isAllowNoLeaf;		//whether app only wants leaf node value or also wants xml module to notify the prsence of a no leaf node
	bool isUseDefault;		//whether app wants to use the default value for elements that have xsd minOccurs=0 and not appear in xml file
	bool isAllElement;		//shall xml report all elements.  if true, *xmlData is irrelevent
	osXmlDataCallback_h xmlCallback;	//if !NULL, the xml module will call this function each time a matching xml element is found, otherwise, the values will be stored in xmlData
	osXmlData_t* xmlData;
	int maxXmlDataSize;
} osXmlDataCallbackInfo_t;


typedef struct osXmlNameValue {
    osPointerLen_t name;
    osPointerLen_t value;
} osXmlNameValue_t;


//get xml instance element value based on xsd and xml mBuf
osStatus_e osXml_getElemValue(osPointerLen_t* xsdName, osMBuf_t* xsdMBuf, osMBuf_t* xmlBuf, bool isKeepXsdNsList, osXmlDataCallbackInfo_t* callbackInfo);
//get xml instance element value based on the xsd and xml files.  this function will free the xsd NS list
osStatus_e osXml_getLeafValue(char* fileFolder, char* xsdFileName, char* xmlFileName, osXmlDataCallbackInfo_t* callbackInfo);
osMBuf_t* osXsd_initNS(char* fileFolder, char* xsdFileName);
bool osXsd_isExistSchema(osPointerLen_t* pTargetNS);
osPointerLen_t* osXml_getnsInfo(void* pnsAliasInfo);
bool osXsd_isValid(osMBuf_t* pXsdBuf, osPointerLen_t* xsdName, bool isKeepNsList);
bool osXml_isValid(osMBuf_t* pXmlBuf, osMBuf_t* pXsdBuf, osPointerLen_t* xsdName, osXmlDataCallbackInfo_t* callbackInfo);


#endif
