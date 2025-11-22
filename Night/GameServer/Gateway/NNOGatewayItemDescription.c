/***************************************************************************
 
 
 
 ***************************************************************************/
#include "stdtypes.h"
#include "timing.h"

#include "itemCommon.h"

typedef struct Entity Entity;
typedef struct Item Item;
typedef struct ItemDef ItemDef;

#include "NNOItemDescription.h"
#include "NNOItemDescription_h_ast.h"
#include "SuperCritterPet.h"
#include "Entity.h"
#include "Entity_h_ast.h"

void OVERRIDE_LATELINK_GetItemInfoComparedSMF(char **pestrResult,
	Language lang,
	SA_PARAM_OP_VALID Item *pItem,
	SA_PARAM_OP_VALID Entity *pEnt,
	S32 eActiveGemSlotType)
{
	ItemDef *pDef = GET_REF(pItem->hItem);

	if(!pDef)
		return;

	PERFINFO_AUTO_START_FUNC();

	if(pDef->eType == kItemType_SuperCritterPet)
	{
		Entity *pFakeEnt = NULL;
		pFakeEnt = scp_CreateFakeEntity(pEnt, pItem, NULL);

		GetNNOSuperCritterPetInfo(pestrResult, lang, pItem,pFakeEnt,pEnt,"Inventory_Scpiteminfo_Auto", "Item_No_Hint");

		StructDestroySafe(parse_Entity, &pFakeEnt);
	}
	else
	{
		GetNNOItemInfoCompared(pestrResult, NULL, lang, pItem, NULL, pEnt,
			"Inventory_OtherPlayerEquippedItemInfo_Auto", NULL, "Item_Normal_Context",
			eActiveGemSlotType);
	}
	

	PERFINFO_AUTO_STOP();
}

// End of File
