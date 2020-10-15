#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "osMemory.h"
#include "osPreMemory.h"
#include "osDebug.h"
#include "osMBuf.h"
#include "osList.h"
#include "osPL.h"
#include "osXmlParserIntf.h"



int main(int argc, char* argv[])
{
    if(argc !=3)
    {
        printf("need to input the file name and isKeepNsList\nusage: ./xsd xsd_file_name isKeepNsList[0|1].\n");
        return 1;
    }

    osPreMem_init();
    osDbg_init(DBG_DEBUG, DBG_ALL);
    osDbg_mInit(LM_XMLP, DBG_DEBUG);

    osDbg_mInit(LM_MEM, DBG_DEBUG);

	osmem_stat();
	osmem_allusedinfo();

	debug("i am ok here.");
	osMBuf_t* xsdMBuf = osMBuf_readFile(argv[1], 16000);
	debug("xsdMsg=\n%M", xsdMBuf);

	osPointerLen_t xsdName = {argv[1], strlen(argv[1])};
	bool isKeepNsList = argv[2][0] == '0' ? false : true;
	bool isValid = osXsd_isValid(xsdMBuf, &xsdName, isKeepNsList);	
	debug("xsd is parsed %s.", isValid ? "OK" : "NOT OK");
    
	osmem_stat();
    osmem_allusedinfo();

	osMBuf_dealloc(xsdMBuf);

   	osmem_stat();
    osmem_allusedinfo();
	return 0;
}
