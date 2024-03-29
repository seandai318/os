/* Copyright <c) 2020, Sean Dai
 * xml parser data structures
 */

#ifndef _OS_XML_PARSER_DATA_H
#define _OS_XML_PARSER_DATA_H

#include "osList.h"
#include "osTypes.h"
#include "osMBuf.h"

#include "osXmlParserIntf.h"



typedef enum {
	OS_XML_TAG_XS_SCHEMA,
	OS_XML_TAG_XS_ELEMENT,
	OS_XML_TAG_XS_COMPLEX_TYPE,
	OS_XML_XS_ALL,
	OS_XML_XS_SEQUENCE,
	OS_XML_XS_CHOICE,
} osXmlTag_e;


typedef enum {
	OS_XML_RESTRICTION_FACET_UNKNOWN,
	OS_XML_RESTRICTION_FACET_LENGTH,
	OS_XML_RESTRICTION_FACET_MIN_LENGTH,
	OS_XML_RESTRICTION_FACET_MAX_LENGTH,
	OS_XML_RESTRICTION_FACET_PATTERN,		//regular expression pattern
	OS_XML_RESTRICTION_FACET_ENUM,			//can be value or string, depending on the restriction base type
	OS_XML_RESTRICTION_FACET_MIN_INCLUSIVE,
	OS_XML_RESTRICTION_FACET_MAX_INCLUSIVE,
	OS_XML_RESTRICTION_FACET_MIN_EXCLUSIVE,
	OS_XML_RESTRICTION_FACET_MAX_EXCLUSIVE,
	OS_XML_RESTRICTION_FACET_WHITE_SPACE,	//"preserve", "replace", "collapse"
	OS_XML_RESTRICTION_FACET_TOTAL_DIGITS,
	OS_XML_RESTRICTION_FACET_FRACTION_DIGITS,
} osXmlRestrictionFacet_e;


typedef enum {
	OS_XML_RES_FACET_WHITE_SPACE_PRESERVE,	//"preserve", no touch to the white space
	OS_XML_RES_FACET_WHITE_SPACE_REPLACE,	//"replace", REPLACE all white space characters (line feeds, tabs, spaces, and carriage returns) with spaces
 	OS_XML_RES_FACET_WHITE_SPACE_COLLAPSE,	//""collapse", line feeds, tabs, spaces, carriage returns are replaced with spaces, leading and trailing spaces are removed, and multiple spaces are reduced to a single space
} osXmlRestrictionFacetWSAction_e;


typedef struct {
    osXmlRestrictionFacet_e facet;
    union {
        uint64_t value;
        osPointerLen_t string;  //used when facet = OS_XML_RESTRICTION_FACET_PATTERN, or OS_XML_RESTRICTION_FACET_ENUM (when base=xs:string)
    };
} osXmlRestrictionFacet_t;


//for now, only support LAX
typedef enum {
	OS_XSD_PROCESS_CONTENTS_LAX,
	OS_XSD_PROCESS_CONTENTS_SKIP,
	OS_XSD_PROCESS_CONTENTS_STRICT,
} osXsdAnyElemPS_e;

typedef enum {
	OS_XSD_NAME_SPACE_ANY,		//##any
	OS_XSD_NAME_SPACE_OTHER,	//##other
	OS_XSD_NAME_SPACE_LOCAL,	//##local
	OS_XSD_NAME_SPACE_TARGET,	//##targetNamespace
	OS_XSD_NAME_SPACE_LIST,		//List of {URI references, ##targetNamespace, ##local}
} osXsdAnyElemNS_e;


typedef enum {
	OS_XML_ELEMENT_DISP_TYPE_ALL,
	OS_XML_ELEMENT_DISP_TYPE_SEQUENCE,
//	OS_XML_ELEMENT_DISP_TYPE_CHOICE,	//the choice is implemented as a property of a element
} osXmlElemDispType_e;


typedef struct osXml_complexTypeInfo {
    osPointerLen_t typeName;	//must be in the beginning of this data structure, for the use in osXsd_getTypeByname()
    bool isMixed;
	osXmlElemDispType_e elemDispType;
	osList_t elemList;		//each element is comprised of osXsdElement_t
} osXmlComplexType_t;


typedef struct {
    osPointerLen_t typeName;    //must be in the beginning of this data structure, for the use in osXsd_getTypeByname()
    osXmlDataType_e baseType;
    osList_t facetList;         //each element is a osXmlRestrictionFacet_t
} osXmlSimpleType_t;


typedef struct {
	osXsdAnyElemNS_e elemNamespace;	//for now, only support ##any or ##other
	osXsdAnyElemPS_e processContent;
} osXsdAnyElementTag_t;


typedef struct {
	bool isLeaf;
	bool isRootAnyElem;
} osXmlAnyElement_t;


typedef struct {
	short nsUseLabel;	//-1: used as default ns, 0: explicitly use alias only, 1: used as both default and alias ns (it is possible in a XSD, a namespace is assigned as default as well as alias)
	osPointerLen_t ns;
	osPointerLen_t nsAlias;
} osXsd_nsAliasInfo_t;


typedef struct {
	bool isXmlAnyElem;	//determine if this is for xsd <xs:any> tag or a xml element
	union {
		osXsdAnyElementTag_t elemAnyTag;	//when isXmlElemAny = false
		osXmlAnyElement_t xmlAnyElem;		//when isXmlElemAny = true
	};
} osXmlAnyElemInfo_t;


typedef struct {
    int minOccurs;          //>=0
    int maxOccurs;          // -1 means unbounded
	uint32_t tag;			//uniquely identify a choice block
} osXsd_choiceInfo_t;
 	

typedef struct {
    osDPointerLen_t targetNS;	//target ns or a xsd name (for no target ns schema), when xsd nme is used, ns.pTargetNS = NULL. useosDPointerLen_t because the pl->p is dynamically allocated to take care the case that user passed in xsdName may be a local variable.
    osList_t nsAliasList;		//each entry contains osXsd_nsAliasInfo_t
    osPointerLen_t defaultNS;
	osPointerLen_t xsAlias;		//for now xsAlias can either be empty or a spec ific alias, can not be both
} osXsd_schemaInfo_t;


typedef struct osXsdElement {
	bool isRootElement;
	osPointerLen_t elemName;
	osXsd_schemaInfo_t* pSchema;
	osPointerLen_t elemTypeName;
	osXsd_choiceInfo_t* pChoiceInfo;
	int minOccurs;			//>=0
	int maxOccurs;			// -1 means unbounded
    bool isQualified;
    osPointerLen_t elemDefault;
    osPointerLen_t fixed;
	osXmlDataType_e dataType;
	//do not free pComplex or pSimple when a osXsdElement_t is freed, otherwise if multiple xsdElement points to the same Type object, over free will happen
	union {
		osXmlComplexType_t* pComplex;
		osXmlSimpleType_t* pSimple;
		osXmlAnyElemInfo_t anyElem;		//if dataType = OS_XML_DATA_TYPE_ANY
	};
//	osList_t* pSimpleTypeList;	//Since simpleType objects shall be kept permanently, this can be removed.  Currently this is not used.
} osXsdElement_t;


typedef struct {
//	bool isTargetNSUsingXsd;	//if true, the targetNS is a XSD name (not from XSD schema, instead, assigned by user)
//	osPointerLen_t schemaName;	//xsd file name.  only relevant for a xsd that has null target namespace (user uses this to refer a xsd, otherwise, the target ns will be used to refer a namespace)
	osXsd_schemaInfo_t schemaInfo;
    osList_t gElementList;      //a list of global element for a schema of a namespace, each entry contains osXsdElement_t
    osList_t gComplexList;      //a list of global complexType for a schema of a namespace, each entry contains osXmlComplexType_t
    osList_t gSimpleList;       //a list of global simpleType for a schema of a namespace, each entry contains osXmlSimpleType_t
	osMBuf_t* pXsdBuf;			//keep a reference to xsdBuf so that it can be freed when needed
} osXsdSchema_t;


typedef struct osXsdNamespace {
	osPointerLen_t* pTargetNS;	//if pTargetNS=NULL, a empty target namespace.  In that case, The targetNS inside shcemaInfo contains xsd name 
	osList_t schemaList;		//each entry contains a osXsd_schema_t since a namespace may have multiple schema/xsd
} osXsdNamespace_t;	//based on target namespace


//help data structure for tag and namve-value pair list
typedef struct {
    osPointerLen_t tag;
    bool isTagDone;                 //the tag is wrapped in one line, i.e., <tag, tag-content />
    bool isEndTag;                  //the line is the end of tag, i.e., </tag>
    bool isPElement;                //if the union data is pElement, this value is true.  That happens in <xs:element> xxx </element> <xs:choice> xxx </choice> or <xs:any> xxx </xs:any>
    union {
        osList_t attrNVList;        //each list element contains osXmlNameValue_t, for <xs:element xxx />, <xs:any xxx /> and other xs:xxx cases
        osXsdElement_t* pElement;   //if tag is xs:element or xs:any, and contains sub tags (i.e., the element and any do not end in one line), this data structure will be used
    };
} osXmlTagInfo_t;



#endif
