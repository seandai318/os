#include <stdio.h>
#include <stdlib.h>

#include "osMemory.h"
#include "osPreMemory.h"
#include "osDebug.h"
#include "osMBuf.h"
#include "osList.h"
#include "osPL.h"
#include "osXmlParserIntf.h"



typedef enum {
	TEST_XML_IDENTITYTYPE,
	TEST_XML_PUBLIC_IDS,
	TEST_XML_IMS_PUBLIC_ID,
	TEST_XML_MAX_DATA_NAME_NUM,
} testConfig_xmlDataName_e;


osXmlData_t testConfig_xmlData[TEST_XML_MAX_DATA_NAME_NUM] = {
	{TEST_XML_IDENTITYTYPE,		{"IdentityType", sizeof("IdentityType")-1}, OS_XML_DATA_TYPE_SIMPLE, 0},
    {TEST_XML_PUBLIC_IDS,        {"PublicIdentifiers", sizeof("PublicIdentifiers")-1}, OS_XML_DATA_TYPE_COMPLEX, 0},
    {TEST_XML_IMS_PUBLIC_ID,	{"IMSPublicIdentity", sizeof("IMSPublicIdentity")-1}, OS_XML_DATA_TYPE_SIMPLE, 0},
};


void callback(osXmlData_t* pXmlValue, void* nsInfo)
{
	if(!pXmlValue)
	{
		logError("null pointer, pXmlValue.");
		return;
	}

	switch(pXmlValue->eDataName)
	{
		case TEST_XML_IDENTITYTYPE:
			debug("TEST_XML_IDENTITYTYPE, nsalias=%r, dataType=%d, value=%d", &pXmlValue->nsAlias, pXmlValue->dataType, pXmlValue->xmlInt);
			break;
		case TEST_XML_PUBLIC_IDS:
			debug("TEST_XML_PUBLIC_IDS, nsalias=%r, dataType=%d, value=%r", &pXmlValue->nsAlias, pXmlValue->dataType, &pXmlValue->xmlStr);
			break;
		case TEST_XML_IMS_PUBLIC_ID:
            debug("TEST_XML_IMS_PUBLIC_ID, nsalias=%r, dataType=%d, value=%r", &pXmlValue->nsAlias, pXmlValue->dataType, &pXmlValue->xmlStr);
			break;
		default:
            debug("dataName=%r is not handled, dataType=%d, nsalias=%r.", &pXmlValue->dataName, pXmlValue->dataType, &pXmlValue->nsAlias);
            switch(pXmlValue->dataType)
            {
                case OS_XML_DATA_TYPE_XS_BOOLEAN:
                    debug(" value = %s", pXmlValue->xmlIsTrue ? "true" :"false");
                    break;
                case OS_XML_DATA_TYPE_XS_UNSIGNED_BYTE:
                case OS_XML_DATA_TYPE_XS_SHORT:
                case OS_XML_DATA_TYPE_XS_INTEGER:
                case OS_XML_DATA_TYPE_XS_LONG:
                    debug("	value = %d", pXmlValue->xmlInt);
                    break;
                case OS_XML_DATA_TYPE_XS_STRING:
                    debug("	value = %r", &pXmlValue->xmlStr);
                    break;
                defult:
                    break;
            }
            break;
	}
}

int main(int argc, char* argv[])
{
    if(argc !=3)
    {
        printf("need to input the xsd and xml file names.");
        return 1;
    }

    osPreMem_init();
    osDbg_init(DBG_DEBUG, DBG_ALL);
    osDbg_mInit(LM_XMLP, DBG_DEBUG);

    //osDbg_mInit(LM_MEM, DBG_DEBUG);

	osmem_stat();
	osmem_allusedinfo();

#if 0
	debug("i am ok here.");
	osMBuf_t* xsdMBuf = osMBuf_readFile(argv[1], 16000);
	debug("xsdMsg=\n%M", xsdMBuf);
	osMBuf_t* xmlMBuf = osMBuf_readFile(argv[2], 6000);
#endif

	osXmlDataCallbackInfo_t cbInfo={true, false, true, callback, testConfig_xmlData, TEST_XML_MAX_DATA_NAME_NUM};
	osXml_getLeafValue(".", argv[1], argv[2], &cbInfo);
#if 0
	bool isValid = osXml_isXmlValid(xmlMBuf, xsdMBuf, false, &cbInfo);	
	debug("xml is parsed %s.", isValid ? "OK" : "NOT OK");
#endif

#if 0    
	osmem_stat();
    osmem_allusedinfo();

	osMBuf_dealloc(xsdMBuf);
	osMBuf_dealloc(xmlMBuf);

   	osmem_stat();
    osmem_allusedinfo();
#endif
	return 0;
}
