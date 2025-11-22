/***************************************************************************



***************************************************************************/

//#include "NNOLandingPageZenPanel.h"

#include "resourcemanager.h"

#include "StringCache.h"
#include "stdtypes.h"
#include "FolderCache.h"
#include "error.h"

#include "ActivityCommon.h"
#include "UIGen.h"

#include "NNOLandingPageZenPanel_c_ast.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_GameSystems););


AUTO_STRUCT;
typedef struct NNOLandingPageZenPanelEntry
{
	REF_TO(EventDef) hEvent;			AST(NAME(EventName))
	const char* pchImageName;			AST(POOL_STRING NAME(ImageName))
	DisplayMessage msgFlavorTextKey;	AST(NAME(FlavorTextKey) STRUCT(parse_DisplayMessage))
	
} NNOLandingPageZenPanelEntry;

AUTO_STRUCT;
typedef struct NNOLandingPageZenPanelInfo
{
	// The list of Entries
	NNOLandingPageZenPanelEntry **ppEntries;	AST(NAME("EventData"))

} NNOLandingPageZenPanelInfo;

static NNOLandingPageZenPanelInfo g_NNOLandingPageZenPanelInfo;
static int g_iSelectedEntry = -1;


AUTO_EXPR_FUNC(UIGen) ACMD_NAME(NNOLandingPageZenPanel_RequestInfo);
void NNOLandingPageZenPanel_RequestInfo()
{
	int iEntry;

	g_iSelectedEntry = -1;
	for (iEntry=0;iEntry<eaSize(&g_NNOLandingPageZenPanelInfo.ppEntries);iEntry++)
	{
		EventDef *pEventDef = GET_REF(g_NNOLandingPageZenPanelInfo.ppEntries[iEntry]->hEvent);
		if (pEventDef!= NULL && Activity_EventIsActive(pEventDef->pchEventName))
		{
			g_iSelectedEntry = iEntry;
			return;
		}
	}
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(NNOLandingPageZenPanel_HasEventOverride);
bool NNOLandingPageZenPanel_HasEventOverride()
{
	if (g_iSelectedEntry>=0)
	{
		return(true);
	}
	return(false);
}


AUTO_EXPR_FUNC(UIGen) ACMD_NAME(NNOLandingPageZenPanel_GetImage);
const char* NNOLandingPageZenPanel_GetImage()
{
	if (g_iSelectedEntry>=0 && g_iSelectedEntry<eaSize(&g_NNOLandingPageZenPanelInfo.ppEntries))
	{
		return(g_NNOLandingPageZenPanelInfo.ppEntries[g_iSelectedEntry]->pchImageName);

	}
	return("");
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(NNOLandingPageZenPanel_GetFlavorTextKey);
const char* NNOLandingPageZenPanel_GetFlavorTextKey()
{
	if (g_iSelectedEntry>=0 && g_iSelectedEntry<eaSize(&g_NNOLandingPageZenPanelInfo.ppEntries))
	{
		return(TranslateDisplayMessage(g_NNOLandingPageZenPanelInfo.ppEntries[g_iSelectedEntry]->msgFlavorTextKey));

	}
	return("");
}


// EventDef based stuff.

EventDef* NNOLandingPageZenPanel_GetSelectedEventDef()
{
	if (g_iSelectedEntry>=0 && g_iSelectedEntry<eaSize(&g_NNOLandingPageZenPanelInfo.ppEntries))
	{
		EventDef *pEventDef = GET_REF(g_NNOLandingPageZenPanelInfo.ppEntries[g_iSelectedEntry]->hEvent);
		return(pEventDef);
	}
	return(NULL);
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(NNOLandingPageZenPanel_GetEventName);
const char* NNOLandingPageZenPanel_GetEventName()
{
	EventDef *pEventDef = NNOLandingPageZenPanel_GetSelectedEventDef();
	if (pEventDef!=NULL)
	{
		return(pEventDef->pchEventName);
	}
	return("");
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(NNOLandingPageZenPanel_GetEventDisplayName);
const char* NNOLandingPageZenPanel_GetEventDisplayName()
{
	EventDef *pEventDef = NNOLandingPageZenPanel_GetSelectedEventDef();
	if (pEventDef!=NULL)
	{
		return(TranslateDisplayMessage(pEventDef->msgDisplayName));
	}
	return("");
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(NNOLandingPageZenPanel_GetEventDisplayShortDesc);
const char* NNOLandingPageZenPanel_GetEventDisplayShortDesc()
{
	EventDef *pEventDef = NNOLandingPageZenPanel_GetSelectedEventDef();
	if (pEventDef!=NULL)
	{
		return(TranslateDisplayMessage(pEventDef->msgDisplayShortDesc));
	}
	return("");
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(NNOLandingPageZenPanel_GetEventDisplayLongDesc);
const char* NNOLandingPageZenPanel_GetEventDisplayLongDesc()
{
	EventDef *pEventDef = NNOLandingPageZenPanel_GetSelectedEventDef();
	if (pEventDef!=NULL)
	{
		return(TranslateDisplayMessage(pEventDef->msgDisplayLongDesc));
	}
	return("");
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(NNOLandingPageZenPanel_GetEventBackground);
const char* NNOLandingPageZenPanel_GetEventBackground()
{
	EventDef *pEventDef = NNOLandingPageZenPanel_GetSelectedEventDef();
	if (pEventDef!=NULL)
	{
		return(pEventDef->pchBackground);
	}
	return("");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Loading

static void NNOLandingPageZenPanel_LoadInternal(const char* pchPath, S32 iWhen)
{
	StructReset(parse_NNOLandingPageZenPanelInfo, &g_NNOLandingPageZenPanelInfo);

	loadstart_printf("Loading NNOLandingPageZenPanelInfo... ");

	ParserLoadFiles(NULL, 
		"defs/config/NNOLandingPageZenPanel.def", 
		"NNOLandingPageZenPanel.bin", 
		PARSER_OPTIONALFLAG, 
		parse_NNOLandingPageZenPanelInfo,
		&g_NNOLandingPageZenPanelInfo);

	g_iSelectedEntry = -1;
	loadend_printf(" done.");
}

// Load game-specific configuration settings
void NNOLandingPageZenPanel_Load(void)
{
	NNOLandingPageZenPanel_LoadInternal(NULL, 0);
	FolderCacheSetCallback(FOLDER_CACHE_CALLBACK_UPDATE, "defs/config/NNOLandingPageZenPanel.def", NNOLandingPageZenPanel_LoadInternal);
}

#include "NNOLandingPageZenPanel_c_ast.c"

