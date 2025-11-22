/***************************************************************************
 
 
 
 ***************************************************************************/
#include "contact_common.h"

ContactDef *OVERRIDE_LATELINK_GatewayVendor_GetContactDef(const char *pchContactName)
{
	if(pchContactName && stricmp(pchContactName, "Nw_Gateway_Professions_Merchant") == 0)
	{
		return contact_DefFromName(pchContactName);
	}

	return NULL;
}

/* End of File */
