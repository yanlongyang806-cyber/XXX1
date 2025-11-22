#include "NNOGatewayCommonMappings.h"

#include "Entity.h"
#include "AppLocale.h"
#include "textparser.h"
#include "inventoryCommon.h"
#include "itemCommon.h"
#include "NNOItemDescription.h"
#include "EString.h"
#include "Powers.h"

#include "NNOItemDescription_h_ast.h"
#include "itemEnums_h_ast.h"
#include "inventoryCommon_h_ast.h"
#include "itemCommon_h_ast.h"
#include "AutoGen/NNOGatewayCommonMappings_h_ast.h"

MappedInvSlot *createMappedInvSlotFromItem(Entity *pEnt, Language lang, Item *psrcItem)
{
	MappedInvSlot *pslot;

	PERFINFO_AUTO_START_FUNC();

	pslot = StructAlloc(parse_MappedInvSlot);

	if(psrcItem)
	{
		int k;

		ItemDef *pdef = GET_REF(psrcItem->hItem);

		pslot->count = psrcItem->count;
		COPY_HANDLE(pslot->hItemDef, psrcItem->hItem);
		pslot->uID = psrcItem->id;

		if(psrcItem->pchDisplayName)
		{
			estrCopy2(&pslot->estrName, psrcItem->pchDisplayName);
		}
		else if(pdef)
		{
			// Many (most? all?) items don't have an name, grab it from the
			// from the item def it's available.
			estrCopy2(&pslot->estrName, pdef->pchName);
		}

		// other item information
		if(pdef)
		{
			static NNOItemInfo *s_pinfo = NULL;
			ItemSortType* pSortType;

			if(!s_pinfo)
				s_pinfo = StructCreate(parse_NNOItemInfo);
			else
				StructReset(parse_NNOItemInfo, s_pinfo);

			GetNNOItemInfoComparedStructNoStrings(lang, s_pinfo, psrcItem, NULL, pEnt, false, 0);
			pslot->bMeetsExpressionRequirements = s_pinfo->bEntUsableExpr;
			pslot->bHasClass = s_pinfo->bEntHasClass;
			pslot->bMeetsLevelRequirements = s_pinfo->bEntMeetsLevelRequirements;
			pslot->bIsUsable = s_pinfo->bEntUsableExpr && s_pinfo->bEntHasClass && s_pinfo->bEntMeetsLevelRequirements;

			// set the quality (rarity)
			pslot->Quality = pdef->Quality;
			if(psrcItem->pAlgoProps)
			{
				pslot->Quality = psrcItem->pAlgoProps->Quality;
			}

			pslot->bDiscardable = !(pdef->flags & kItemDefFlag_CantDiscard);
			pslot->bSellable = !(pdef->flags & kItemDefFlag_CantSell);
			pslot->bBound = false;
			pslot->bBoundToAccount = false;
			if((psrcItem->flags & kItemFlag_Bound) != 0)
			{
				pslot->bBound = true;
				estrPrintf(&pslot->esBoundText, "%s", langTranslateMessageKey(lang, "Item.UI.Bound"));
			}
			else if((psrcItem->flags & kItemFlag_BoundToAccount) != 0)
			{
				pslot->bBoundToAccount = true;
				estrPrintf(&pslot->esBoundText, "%s", langTranslateMessageKey(lang, "Item.UI.BoundToAccount"));
			}

			pslot->iValue = item_GetResourceValue(PARTITION_STATIC_CHECK, pEnt, psrcItem, "Resources");

			pSortType = item_GetSortTypeForID(pdef->iSortID);
			if(pSortType)
			{
				estrPrintf(&pslot->esSortText, "%s", langTranslateMessageRef(lang, pSortType->hNameMsg));
			}
			else
			{
				estrPrintf(&pslot->esSortText, "");
			}

			estrPrintf(&pslot->esRareText, "%s", StaticDefineLangGetTranslatedMessage(lang, ItemQualityEnum, item_GetQuality(psrcItem)));

			for(k=0;k<eaSize(&pdef->ppItemGemSlots);k++)
			{
				MappedGemSlot *pGem = StructCreate(parse_MappedGemSlot);
				CONST_EARRAY_OF(ItemGemSlot) ppSlots = SAFE_MEMBER2(psrcItem,pSpecialProps,ppItemGemSlots);

				pGem->eType = pdef->ppItemGemSlots[k]->eType;
				pGem->bFilled = eaSize(&ppSlots) > k && GET_REF(ppSlots[k]->hSlottedItem); 

				eaPush(&pslot->ppGemSlots,pGem);
			}

			pslot->bIsNew = eaIndexedFindUsingInt(&pEnt->pInventoryV2->eaiNewItemIDs,psrcItem->id) != -1;
		}

		// Grab the powers for this item.
		// (Currently not sure if these are needed, but I suspect they will be.)
		for(k = 0; k < eaSize(&psrcItem->ppPowers); k++)
		{
			MappedPower *ppow = StructAlloc(parse_MappedPower);

			eaPush(&pslot->ppPowers, ppow);
			COPY_HANDLE(ppow->hPowerDef, psrcItem->ppPowers[k]->hDef);
		}
	}

	PERFINFO_AUTO_STOP();
	return pslot;
}

#include "AutoGen/NNOGatewayCommonMappings_h_ast.c"