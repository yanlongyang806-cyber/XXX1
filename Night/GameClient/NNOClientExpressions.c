/***************************************************************************



***************************************************************************/

#include "gclEntity.h"
#include "gclSuperCritterPet.h"
#include "CharacterStatus.h"
#include "UIGen.h"
#include "UITray.h"
#include "inventory_uiexpr.h"
#include "GraphicsLib.h"
#include "CharacterCreationUI.h"

#include "resourcemanager.h"
#include "CharacterClass.h"
#include "StringCache.h"
#include "stdtypes.h"
#include "FolderCache.h"
#include "Entity.h"
#include "LoginCommon.h"
#include "message.h"
#include "ItemCommon.h"
#include "ItemEnums.h"
#include "ExpressionMinimal.h"
#include "BlockEarray.h"
#include "expressionprivate.h"
#include "gamestringformat.h"
#include "combateval.h"
#include "GameAccountDataCommon.h"
#include "structDefines.h"
#include "mission_common.h"
#include "PowerTreeHelpers.h"
#include "EntityIterator.h"
#include "aiDebugShared.h"
#include "PowerList.h"
#include "SuperCritterPet.h"
#include "Character.h"
#include "entcritter.h"
#include "fcinventoryui.h"

#include "../Common/NNOItemDescription.h"

#include "AutoGen/PowersEnums_h_ast.h"
#include "AutoGen/Powers_h_ast.h"
#include "AutoGen/GameServerLib_autogen_ServerCmdWrappers.h"
#include "AutoGen/Entity_h_ast.h"
#include "AutoGen/Character_h_ast.h"
#include "AutoGen/PowerTree_h_ast.h"
#include "Autogen/itemCommon_h_ast.h"
#include "Autogen/itemEnums_h_ast.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_UISystem););

//static fake entity to calculate pet stats:
static Entity* spPetTooltipFakeEnt = NULL;

// this is sketchy, but safe.  The goal is to not make a fake entity every frame unless
// the item has changed.  Item uid is not available from the auction house.  This saves the item
// pointer and compares it against the current item.
// It's saved in a U64 just to make it more obvious that it is not safe to dereference.
static U64 su64CurrentPetToolItem_pointer_do_not_dereference = 0;

//helper function, gets an attribute of an ent.  Attrib should be the designer-facing enum value name.
static F32 getAttributeScore(Entity* pEnt, const char* attrib)
{
	return (F32)round(*F32PTR_OF_ATTRIB(pEnt->pChar->pattrBasic, StaticDefineInt_FastStringToInt(AttribTypeEnum, attrib, -1)));
}

//builds the pretty tooltip for the gem slots
AUTO_EXPR_FUNC(UIGen) ACMD_NAME(GenGetItemGemList);
void exprGenGetItemGemList(SA_PARAM_NN_VALID ExprContext * pContext, SA_PARAM_NN_VALID UIGen *pGen, SA_PARAM_OP_VALID Item *pItem)
{
	ItemGemSlotFilledInfo ***peaGemSlots = NULL;
	if( pItem )
	{
		ItemDef *pItemDef = GET_REF(pItem->hItem);
		if( pItemDef )
		{
			int i;
			peaGemSlots = ui_GenGetManagedListSafe(pGen, ItemGemSlotFilledInfo);
			eaSetSizeStruct(peaGemSlots, parse_ItemGemSlotFilledInfo, eaSize(&pItemDef->ppItemGemSlots));
			for (i = 0; i < eaSize(&pItemDef->ppItemGemSlots); i++)
			{
				ItemGemSlotDef *pGemSlotDef = pItemDef->ppItemGemSlots[i];
				ItemDef* pDef = item_GetSlottedGemItemDef(pItem, i);

				StructInit(parse_ItemGemSlotFilledInfo, (*peaGemSlots)[i]);
				(*peaGemSlots)[i]->eType = pGemSlotDef->eType;
				(*peaGemSlots)[i]->bIsFilled = !!pDef;
			}
		}
	}
	
	ui_GenSetManagedListSafe(pGen, peaGemSlots, ItemGemSlotFilledInfo, false);
}

//builds the pretty tooltip for the gem slots
AUTO_EXPR_FUNC(UIGen) ACMD_NAME(GenGetItemGemDetailedList);
void exprGenGetItemGemDetailedList(SA_PARAM_NN_VALID ExprContext * pContext, SA_PARAM_NN_VALID UIGen *pGen, SA_PARAM_OP_VALID Item *pItem)
{
	Item ***peaFakeItems = NULL;
	if( pItem )
	{
		ItemDef *pItemDef = GET_REF(pItem->hItem);
		if( pItemDef )
		{
			int i;
			peaFakeItems = ui_GenGetManagedListSafe(pGen, Item);
			eaSetSizeStruct(peaFakeItems, parse_Item, eaSize(&pItemDef->ppItemGemSlots));
			for (i = 0; i < eaSize(&pItemDef->ppItemGemSlots); i++)
			{
				ItemDef* pDef = item_GetSlottedGemItemDef(pItem, i);
				if (pDef)
				{
					NOCONST(Item)* pItemNC = inv_ItemInstanceFromDefName(pDef->pchName, 0, 0, NULL, NULL, NULL, false, NULL);
					StructDestroy(parse_Item, (*peaFakeItems)[i]);
					item_trh_GetOrCreateAlgoProperties(pItemNC);
					pItemNC->pAlgoProps->uProgressionLevel = item_GetSlottedGemProgressionLevel(pItem, i);
					pItemNC->pAlgoProps->uProgressionXP = item_GetSlottedGemProgressionXP(pItem, i);
					(*peaFakeItems)[i] = CONTAINER_RECONST(Item, pItemNC);
				}
			}
		}
	}

	ui_GenSetManagedListSafe(pGen, peaFakeItems, Item, false);
}


AUTO_EXPR_FUNC(UIGen) ACMD_NAME(ItemGetSlottedGemItemDef);
SA_RET_OP_VALID ItemDef *exprItemGetSlottedGemItemDef(SA_PARAM_NN_VALID Item *pHolderItem, S32 iSlot)
{
	if( pHolderItem )
		return item_GetSlottedGemItemDef(pHolderItem, iSlot);
	return NULL;
}


AUTO_EXPR_FUNC(UIGen) ACMD_NAME(ItemHasSlottedGem);
bool exprItemHasSlottedGem(SA_PARAM_OP_VALID Item *pHolderItem)
{
	if( pHolderItem && pHolderItem->pSpecialProps)
	{
		int i;
		for (i = 0; i < eaSize(&pHolderItem->pSpecialProps->ppItemGemSlots); i++)
		{
			if (GET_REF(pHolderItem->pSpecialProps->ppItemGemSlots[i]->hSlottedItem))
				return true;
		}
	}
	return false;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(ItemPickBestIdentifyScroll);
const char* exprItemPickBestIdentifyScroll(SA_PARAM_OP_VALID Item *pItem)
{
	BagIterator* pIter = bagiterator_Create();
	Entity* pPlayer = entActivePlayerPtr();
	NOCONST(Item)* pBestMatch = NULL;
	ItemDef* pBestDef = NULL;
	if (!pItem)
		return NULL;
	inv_bag_FindItem(pPlayer, InvBagIDs_Inventory, pIter, kFindItem_ByType, (void*)kItemType_IdentifyScroll, true, false);
	for (; !bagiterator_Stopped(pIter); inv_bag_FindItem(pPlayer, InvBagIDs_Inventory, pIter, kFindItem_ByType, (void*)kItemType_IdentifyScroll, true, false))
	{
		NOCONST(Item)* pScroll = bagiterator_GetItem(pIter);
		int iScrolllevel = item_trh_GetLevel(pScroll);
		if (iScrolllevel >= item_GetLevel(pItem))
		{
			if (!pBestMatch || (iScrolllevel < item_trh_GetLevel(pBestMatch)) || (iScrolllevel == item_trh_GetLevel(pBestMatch) && (pScroll->flags & (kItemFlag_Bound | kItemFlag_BoundToAccount))))
			{
				//new "best" scroll
				pBestMatch = pScroll;
			}
		}
	}
	bagiterator_Destroy(pIter);

	if (pBestMatch)
		pBestDef = GET_REF(pBestMatch->hItem);
	return pBestDef ? pBestDef->pchName : "";
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(InventoryKeyPickBestIdentifyScroll);
const char* exprInventoryKeyPickBestIdentifyScroll(const char* pchKey)
{
	UIInventoryKey Key = {0};
	Item* pItem = NULL;
	gclInventoryParseKey(pchKey, &Key);
	pItem = Key.pSlot ? Key.pSlot->pItem : NULL;
	if (!pItem)
		return NULL;

	return exprItemPickBestIdentifyScroll(pItem);
}

static void _getSCPItemTooltip(char ** pestrResult, 
	SA_PARAM_OP_VALID Item *pItem,
	SA_PARAM_OP_VALID Entity *pEnt, 
	ACMD_EXPR_DICT(Message) const char *pchDescriptionKey,
	ACMD_EXPR_DICT(Message) const char *pchContextKey)
{
	// To calculate stats, which depend on powers, we have to make a fake entity and combat tick it:
	if(su64CurrentPetToolItem_pointer_do_not_dereference != (U64) pItem || !spPetTooltipFakeEnt)
	{
		su64CurrentPetToolItem_pointer_do_not_dereference = (U64) pItem;
		StructDestroySafe(parse_Entity, &spPetTooltipFakeEnt);
		spPetTooltipFakeEnt = scp_CreateFakeEntity(pEnt, pItem, NULL);
	}

	GetNNOSuperCritterPetInfo(pestrResult,langGetCurrent(),pItem,spPetTooltipFakeEnt,pEnt,pchDescriptionKey,pchContextKey);
}

//builds the pretty tooltip for a super critter pet, along with the format message.
AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenGetSCPItemTooltip");
const char* GenExprGetSCPItemTooltip(	ExprContext* pContext, 
	SA_PARAM_OP_VALID Item *pItem,
	SA_PARAM_OP_VALID Entity *pEnt, 
	ACMD_EXPR_DICT(Message) const char *pchDescriptionKey,
	ACMD_EXPR_DICT(Message) const char *pchContextKey
	)
{
	char *estrResult = NULL;
	char *result = NULL;

	_getSCPItemTooltip(&estrResult,pItem,pEnt,pchDescriptionKey,pchContextKey);

	if (estrResult)
	{
		result = exprContextAllocScratchMemory(pContext, strlen(estrResult) + 1);
		memcpy(result, estrResult, strlen(estrResult) + 1);
		estrDestroy(&estrResult);
	}

	return NULL_TO_EMPTY(result);
}


// Fills in a message to show info about what is available for this pet
//	at this (or next) quality level.  
AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenGetSCPItemQualityUpgradeString");
void GenExprGetSCPItemQualityUpgradeString(	ExprContext* pContext, 
													SA_PARAM_OP_VALID UIGen * pGen,
													char* pchGenResultVar,
													int iActivePetIdx,
													bool bAfterUpgrade,
													ACMD_EXPR_DICT(Message) const char *pchMessageKey)
{
	Entity* pPlayerEnt = entActivePlayerPtr();
	Item* pPetItem = scp_GetActivePetItem(pPlayerEnt, iActivePetIdx);
	SuperCritterPet* pPet= pPetItem ? scp_GetPetFromItem(pPetItem) : NULL;
	SuperCritterPetDef* pPetDef = pPet ? GET_REF(pPet->hPetDef) : NULL;
	Entity* pFakePetEnt = scp_GetFakePetEntity(iActivePetIdx);
	int i;
	UIGenVarTypeGlob * pGenResultVar = eaIndexedGetUsingString(&pGen->eaVars, pchGenResultVar);

	//data vars:
	F32 fQualityModifier, fPreviousQualityModifier;
	S32 iCost, iQuality, iPreviousQuality;
	U32 iMaxRank, iPreviousMaxRank;
	const char* pchPetName = NULL;
	const char* pchNewPower = NULL;
	const char* pchNewEquipSlot = NULL;
	const char* pchNewCostume = NULL;
	char *estrActivePowersString = NULL;
	char pchQualityName[64];	 //just holds a message key
	char pchPreviousQualityName[64];  //just holds a message key
	U32 uNewPowerAtLevel=0, uNewEquipSlotAtLevel=0, uNewCostumeAtLevel=0;

	if(!pPetDef || !pFakePetEnt){
		return;
	}

	// basic stats:
	iPreviousQuality = iQuality = item_GetQuality(pPetItem);
	if(bAfterUpgrade && iQuality < g_SCPConfig.iMaxUpgradeQuality)
	{
		iQuality += 1;
	}
	else if (!bAfterUpgrade && iPreviousQuality > 0)
	{
		iPreviousQuality -= 1;
	}
	fQualityModifier = g_SCPConfig.eafQualityModifiers[iQuality];
	fPreviousQualityModifier = g_SCPConfig.eafQualityModifiers[iPreviousQuality];
	iMaxRank = g_SCPConfig.eaiMaxLevelsPerQuality[iQuality];
	iPreviousMaxRank = g_SCPConfig.eaiMaxLevelsPerQuality[iPreviousQuality];
	iCost = g_SCPConfig.eafCostToUpgradeQuality[iQuality - 1];

	sprintf(pchPreviousQualityName, "Item.Quality.Formatted.%d", iPreviousQuality);
	sprintf(pchQualityName, "Item.Quality.Formatted.%d", iQuality);
	pchPetName = scp_GetPetItemName(pPetItem);
	
	// costume:
	for(i = 0; i < eaiSize(&g_SCPConfig.eaiCostumeUnlockLevels); i++)
	{
		if(iPreviousMaxRank < g_SCPConfig.eaiCostumeUnlockLevels[i] && iMaxRank >= g_SCPConfig.eaiCostumeUnlockLevels[i] && i < eaSize(&pPetDef->eaCostumes))
		{
			// it is newly available in with this quality.
			pchNewCostume = TranslateDisplayMessage(pPetDef->eaCostumes[i]->displayMsg);
			uNewCostumeAtLevel = g_SCPConfig.eaiCostumeUnlockLevels[i];
		}
	}

	// item:
	for(i = 0; i < eaiSize(&g_SCPConfig.eaiEquipSlotUnlockLevels); i++)
	{
		if(iPreviousMaxRank < g_SCPConfig.eaiEquipSlotUnlockLevels[i] && iMaxRank >= g_SCPConfig.eaiEquipSlotUnlockLevels[i])
		{
			// it is newly available in with this quality.
			SCPEquipSlotDef* pSlot = eaGet(&pPetDef->eaEquipSlots, i);
			if (pSlot)
			{	
				pchNewEquipSlot = StaticDefineInt_FastIntToString(InvBagIDsEnum, pSlot->eID);
				uNewEquipSlotAtLevel = g_SCPConfig.eaiEquipSlotUnlockLevels[i];
			}
		}
	}

	//power:
	uNewPowerAtLevel = 30;	//Hard coding this for now.

	// passive power unlocked at level 30 is tagged with PetUnlock01. Bleh this is harder than it should be!
	if(iPreviousMaxRank < uNewPowerAtLevel && iMaxRank >= uNewPowerAtLevel)
	{
		Power** eaPowers = NULL;
		S32* pePurposes = NULL;
		gclGetPowerPurposesFromString(pContext, "PetUnlock01", &pePurposes);
		if( pePurposes  )
		{
			gclPowerListFilter(pFakePetEnt, pFakePetEnt->pChar->ppPowers, &eaPowers, NULL, NULL, NULL, NULL, NULL, pePurposes,	false, true, true);
		}
		if(eaPowers){
			PowerDef* pPowerDef = GET_REF(eaPowers[0]->hDef);
			if (pPowerDef)
			{
				pchNewPower = TranslateDisplayMessage(pPowerDef->msgDisplayName);
			}
		}
	}
	
	GetNNOSuperCritterPetActivePowerString(pPetItem, pPetDef, langGetCurrent(), &estrActivePowersString);

	estrClear(&pGenResultVar->pchString);

	langFormatGameMessageKey(langGetCurrent(), &pGenResultVar->pchString, pchMessageKey,
		STRFMT_INT(		"Cost",					iCost),
		STRFMT_INT(		"MaxRank",				iMaxRank),
		STRFMT_INT(		"NewQuality",			iQuality),
		STRFMT_INT(		"OldQuality",			iPreviousQuality),
		STRFMT_INT(		"RatingBonusTotal",		(int) ((fQualityModifier - 1) * 100)),
		STRFMT_INT(		"RatingBonusDelta",		(int) (((fQualityModifier - fPreviousQualityModifier) * 100))),
		STRFMT_STRING(	"PetName",				NULL_TO_EMPTY(pchPetName)),
		STRFMT_STRING(	"NewPower",				NULL_TO_EMPTY(pchNewPower)),
		STRFMT_STRING(	"NewEquipSlot",			NULL_TO_EMPTY(pchNewEquipSlot)),
		STRFMT_STRING(	"NewCostume",			NULL_TO_EMPTY(pchNewCostume)),
		STRFMT_INT(		"NewPowerAtLevel",		uNewPowerAtLevel),
		STRFMT_INT(		"NewEquipSlotAtLevel",	uNewEquipSlotAtLevel),
		STRFMT_INT(		"NewCostumeAtLevel",	uNewCostumeAtLevel),
		STRFMT_STRING(	"NewQualityName",		NULL_TO_EMPTY(TranslateMessageKey(pchQualityName))),
		STRFMT_STRING(	"OldQualityName",		NULL_TO_EMPTY(TranslateMessageKey(pchPreviousQualityName))),
		STRFMT_STRING(	"ActivePowersString",	NULL_TO_EMPTY(estrActivePowersString)),
		STRFMT_END
		);

	estrDestroy(&estrActivePowersString);
}


static void _getNNOItemInfo(char ** pestrResult,
								SA_PARAM_OP_VALID Item *pItem,
									SA_PARAM_OP_VALID Entity *pEnt,
									const char *pchMessageKey,
									const char *pchContextKey,
									S32 eActiveGemSlotType )
{
	if (pchMessageKey == NULL || pchMessageKey[0] == 0)
	{
		ItemDef *pItemDef = SAFE_GET_REF(pItem, hItem);
		devassert(pItemDef);
		if (pItemDef == NULL)
			return;

		pchMessageKey = "Inventory_ItemInfo_Auto";
		switch (pItemDef->eType)
		{
		case kItemType_Mission:
			pchMessageKey = "Inventory_QuestItemInfo_Auto";
			break;
		case kItemType_MissionGrant:
			pchMessageKey = "Inventory_QuestGrantItemInfo_Auto";
			break;
		case kItemType_SuperCritterPet:
			{
				_getSCPItemTooltip(pestrResult,pItem,pEnt,"Inventory_SCPItemInfo_Auto",pchContextKey);
				return;
			}
			break;
		default:
			break;
		}
	}

	GetNNOItemInfoCompared(pestrResult,NULL,langGetCurrent(), pItem, pItem, pEnt, pchMessageKey, NULL, pchContextKey, eActiveGemSlotType);
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenGetNNOItemInfoCompared");
void GenExprGetNNOItemInfoCompared(	ExprContext* pContext,
											SA_PARAM_OP_VALID UIGen * pGen,
											const char * pchMainResultVar,
											const char * pchEquippedResultVar,
											SA_PARAM_OP_VALID Item *pItem,
											SA_PARAM_OP_VALID Entity *pEnt,
											const char *pchContextKey,
											S32 eActiveGemSlotType)
{
	UIGenVarTypeGlob * pMainResultVar = eaIndexedGetUsingString(&pGen->eaVars, pchMainResultVar);
	UIGenVarTypeGlob * pEquippedResultVar = eaIndexedGetUsingString(&pGen->eaVars, pchEquippedResultVar);
	ItemDef *pItemDef = SAFE_GET_REF(pItem, hItem);
	Language lang = langGetCurrent();

	if (!pItem || !pItemDef)
		return;

	estrClear(&pMainResultVar->pchString);
	estrClear(&pEquippedResultVar->pchString);

	if (item_IsUnidentified(pItem) || pItemDef->eType == kItemType_Mission || pItemDef->eType == kItemType_MissionGrant || pItemDef->eType == kItemType_SuperCritterPet)
	{
		// not a compare
		_getNNOItemInfo(&pMainResultVar->pchString,pItem,pEnt,NULL,pchContextKey,eActiveGemSlotType);
	}
	else
	{
		// do a compare
		static InventorySlot **s_eaSlots=NULL; // a better idea than this is a static array 2 elements long, and a function that takes a cap of 2
		Item * pItemCompare = NULL;
		int iComparableItems;
		char const * pchFirstItemKey = "Inventory_ItemInfo_Auto";
		char const * pchFirstCompareKey = "Inventory_EquippedComparisonItemInfo_Auto";
		gclInvGetComparableSlots(&s_eaSlots, pItem, pEnt);
		iComparableItems = eaSize(&s_eaSlots);

		if (iComparableItems)
		{
			pItemCompare = s_eaSlots[0]->pItem;

			if (iComparableItems > 1)
			{
				pchFirstItemKey = "Inventory_ItemInfo_Auto_DoubleComparePrimary";
				pchFirstCompareKey = "Inventory_EquippedComparisonItemInfo_Auto_Right";
			}
		}

		GetNNOItemInfoCompared(&pMainResultVar->pchString, &pEquippedResultVar->pchString, lang,pItem,pItemCompare,pEnt,pchFirstItemKey,pchFirstCompareKey,
								pchContextKey,eActiveGemSlotType);

		if (iComparableItems > 1)
		{
			char *aestrResult[2] = {NULL,NULL};

			// Second compare key is always left
			GetNNOItemInfoCompared(&aestrResult[0],&aestrResult[1],lang,pItem,s_eaSlots[1]->pItem,pEnt,"Inventory_ItemInfo_Auto_DoubleCompareSecondary","Inventory_EquippedComparisonItemInfo_Auto_Left",
								pchContextKey,eActiveGemSlotType);

			if(aestrResult[0])
			{
				// appending just some diff text
				estrAppend(&pMainResultVar->pchString,&aestrResult[0]);
				estrDestroy(&aestrResult[0]);
			}

			if(aestrResult[1])
			{
				// appending a second equipped item
				estrAppend(&pEquippedResultVar->pchString,&aestrResult[1]);
				estrDestroy(&aestrResult[1]);
			}
		}

		eaClearFast(&s_eaSlots);
	}
}


AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenGetNNOItemInfo");
const char* GenExprGetNNOItemInfo(	ExprContext* pContext, 
									SA_PARAM_OP_VALID Item *pItem,
									SA_PARAM_OP_VALID Entity *pEnt, 
									ACMD_EXPR_DICT(Message) const char *pchDescriptionKey,
									const char *pchContextKey,
									S32 eActiveGemSlotType)
{
	char *result = NULL;
	char *estrResult = NULL;

	if (pItem)
	{
		_getNNOItemInfo(&estrResult,pItem,pEnt,pchDescriptionKey,pchContextKey,eActiveGemSlotType);

		if(estrResult)
		{
			result = exprContextAllocScratchMemory(pContext, strlen(estrResult) + 1);
			memcpy(result, estrResult, strlen(estrResult) + 1);
			estrDestroy(&estrResult);
		}
	}

	return NULL_TO_EMPTY(result);
}

//NNO item comparison function, done as a super-quick prototype. Doesn't support dual-wield yet.
AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenGetItemToBeReplaced");
SA_RET_OP_VALID Item* GenExprGetItemToBeReplaced(ExprContext* pContext, SA_PARAM_OP_VALID Item *pItem, SA_PARAM_OP_VALID Entity *pEnt, char* pchIgnoreBag)
{
	ItemDef* pDef = SAFE_GET_REF(pItem, hItem);
	InvBagIDs eIgnoreBag = StaticDefineIntGetInt(InvBagIDsEnum, pchIgnoreBag);
	InvBagIDs eEquipBag = pDef ? eaiGet(&pDef->peRestrictBagIDs, 0) : InvBagIDs_None;
	int bagIndex;
	InventorySlot* pSlot;
	Item* pEquippedItem;
	GameAccountDataExtract *pExtract;
	InventoryBag* pBag;

	if (!pItem || !pDef || eEquipBag == eIgnoreBag || !(pDef->eType == kItemType_Upgrade || pDef->eType == kItemType_Weapon)) 
	{
		return NULL;
	}

	pExtract = entity_GetCachedGameAccountDataExtract(pEnt);

	pBag = CONTAINER_RECONST(InventoryBag, inv_GetBag(CONTAINER_NOCONST(Entity, pEnt), eEquipBag, pExtract));
	if (pBag && pDef->eRestrictSlotType == kSlotType_Any && inv_bag_GetFirstEmptySlot(pEnt, pBag) != -1)
	{
		return NULL;
	}

	bagIndex = (SAFE_MEMBER(pDef, eRestrictSlotType) == kSlotType_Secondary) ? 1 : 0;
	pSlot = inv_ent_GetSlotPtr(pEnt, eEquipBag,  bagIndex, pExtract);
	pEquippedItem = SAFE_MEMBER(pSlot, pItem);
	return pEquippedItem;
}

//This function has been gutted, now just returns the item def's texture name.
//Leaving it in the game based on the high likelihood that NNO will want some sort
//of proprietary icon handling in the future. (Un-ID'd items?)
AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenGetItemIcon");
const char* GenExprGetItemIcon(ExprContext* pContext, SA_PARAM_OP_VALID Item *pItem)
{
	return item_GetIconName(pItem, NULL);
}

// Returns true if pItem is better than the pOtherItem
// Currently allows an item to be regarded as usable up to two levels higher than itself
AUTO_EXPR_FUNC(UIGen) ACMD_NAME("IsItemRecommended");
bool GenExprIsItemRecommended(SA_PARAM_OP_VALID Entity *pEnt, SA_PARAM_OP_VALID Item *pItem, SA_PARAM_OP_VALID Item *pOtherItem)
{
	return GetNNOIsItemRecommended(pEnt, pItem, pOtherItem);
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("TargetShouldShowSelectedFX");
bool GenExprTargetShouldShowSelectedFX(SA_PARAM_OP_VALID Entity *pEnt)
{
	Entity* pPlayerEnt = entActivePlayerPtr();
	
	if (pEnt && pPlayerEnt && pPlayerEnt->pChar)
	{
		CharacterClass *pClass =  GET_REF(pPlayerEnt->pChar->hClass);

		if (pClass && pClass->bUseProximityTargetingAssistEnt)
		{
			return pPlayerEnt->pChar->erProxAssistTaget == entGetRef(pEnt);
		}
		else if (pPlayerEnt->pChar->currentTargetRef == entGetRef(pEnt))
		{
			return gclEntGetIsFoe(pPlayerEnt, pEnt) && !EntWasObject(pEnt);
		}
	}

	return false;
}

AUTO_COMMAND ACMD_NAME("EvaluateLeftClick") ACMD_ACCESSLEVEL(0) ACMD_CATEGORY(Interface);
void cmdEvaluateLeftClick(int bOn)
{
	UIGen *pRadialInteract = NULL;
	
	if (bOn)
	{
		pRadialInteract = ui_GenFind("Interact_Radial_List", kUIGenTypeLayoutBox);
	}

	if (pRadialInteract && ui_GenLayoutBoxGetSelectedIndex(pRadialInteract) >= 0)
	{
		ui_GenSendMessage(pRadialInteract, "InitiateInteractUnlessReviving");
	}
	else
	{
		gclPowerSlotExec(bOn, 0);
	}
}


// A very specific metric needed for NNO feats (talent trees) to return the normalize percentage through 
// the tree's group
AUTO_EXPR_FUNC(UIGen) ACMD_NAME("NNOTalentTreeGetUnitPercentageThroughTree");
F32 GenExprNNOTalentTreeGetPercentageThroughTree(SA_PARAM_OP_VALID Entity *pEnt, const char *pchTreeDef, const char *pchPoints)
{
	if (pEnt && pEnt->pChar)
	{
		PowerTree *pTree = character_FindTreeByDefName(pEnt->pChar, pchTreeDef);
		if (pTree)
		{
			S32 iPointsSpent;
			PowerTreeDef *pTreeDef = GET_REF(pTree->hDef);
			if (!pTreeDef)
				return 0.f;

			iPointsSpent = entity_PointsSpentInTreeUnderLevel(CONTAINER_NOCONST(Entity, pEnt),CONTAINER_NOCONST(PowerTree, pTree), pchPoints, -1);
			if (!iPointsSpent)
				return 0.f;

			// calculate the number of points to have gotten all groups
			{
				S32 iHighestPoints = 0;
				PTGroupDef *pLastGroup = NULL;
				
				FOR_EACH_IN_EARRAY(pTreeDef->ppGroups, PTGroupDef, pGroup)
				{
					if (pGroup->pRequires && pGroup->pRequires->iMinPointsSpentInThisTree > iHighestPoints)
					{
						iHighestPoints = pGroup->pRequires->iMinPointsSpentInThisTree;
						pLastGroup = pGroup;
					}
				}
				FOR_EACH_END

				if (pLastGroup)
				{
					S32 iMaxRank = 0;
					// get the max rank of the nodes in this group
					FOR_EACH_IN_EARRAY(pLastGroup->ppNodes, PTNodeDef, pNode)
					{
						S32 count = eaSize(&pNode->ppRanks);
						if (count > iMaxRank)
						{
							iMaxRank = count;
						}
					}
					FOR_EACH_END

					iHighestPoints += iMaxRank;
				}

				return (iHighestPoints) ? (F32)iPointsSpent / (F32)(iHighestPoints) : 0.f;
			}
		}
	}
	
	return 0;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenGetCurrentBossEntity");
SA_RET_OP_VALID Entity* GenExprGetCurrentBossEntity()
{
	static EntityRef s_erBoss = 0;
	static U32 s_uiLastIteration = 0;
	//This is kind of expensive so we only want to do the actual iteration once per frame.
	//Otherwise, just use whatever we got in the earlier check.
	if (s_uiLastIteration == g_ui_State.uiFrameCount)
	{
		return entFromEntityRef(PARTITION_CLIENT, s_erBoss);
	}
	else
	{
		EntityIterator* pIter = NULL;
		Entity* pEnt = NULL;
		Entity* pPlayer = entActivePlayerPtr();
 		int i = 0;
		MultiVal mv = {0};
		pIter = entGetIteratorSingleType(PARTITION_CLIENT, 0,  ENTITYFLAG_PROJECTILE | ENTITYFLAG_IGNORE | ENTITYFLAG_DEAD | ENTITYFLAG_DESTROY, GLOBALTYPE_ENTITYCRITTER);
		s_uiLastIteration = g_ui_State.uiFrameCount;
		s_erBoss = 0;

		while (pEnt = EntityIteratorGetNext(pIter))
		{
			if (entGetUIVar(pEnt, "Boss", &mv) && pEnt->pChar && MultiValToBool(&mv))
			{
				if (character_HasMode(pEnt->pChar,kPowerMode_Combat))
				{
					s_erBoss = pEnt->myRef;
					EntityIteratorRelease(pIter);
					return pEnt;
				}
				//For now, don't check AITargets and just assume that if a boss is in combat we should show the bar. 
				/*
				for (i = 0; i < eaSize(&pEnt->pChar->ppAITargets); i++)
				{
					if (pEnt->pChar->ppAITargets[i]->entRef == pPlayer->myRef)
					{
						s_erBoss = pEnt->myRef;
						EntityIteratorRelease(pIter);
						return pEnt;
					}
				}
				*/
			}
		}
		EntityIteratorRelease(pIter);
	}

	return NULL;
}


///////////////////
// LevelUpChecklist helper functions
//

S32 gclGenExprEntityGetPowerTablePoints(SA_PARAM_OP_VALID Entity *pEnt, const char *pchTable);
S32 gclGenExprGetPointsEarned(SA_PARAM_OP_VALID Entity *pEnt, const char* pchPoints );
int Player_ExprGetPointsSpentUnderLevel(SA_PARAM_OP_VALID Entity *pPlayer, const char *pchPoints, int iLevel);
S32 exprEntGetLevelExp(SA_PARAM_OP_VALID Entity *pEntity);
bool exprUseNextItemOfName(SA_PARAM_OP_VALID Entity *pEntity, S32 eBagID, const char *pchName);
#include "Player.h"
#include "PowerGrid.h"
extern DictionaryHandle g_hItemDict;

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(LevelUpCheckList_GetItemDefForLevelUpBox);
SA_RET_OP_VALID ItemDef* exprLevelUpCheckList_GetItemDefForLevelUpBox()
{
	static const char* s_pchLevelUpBoxNames[] =
	{
		"Level_Up_Box_01",
		"Level_Up_Box_02",
		"Level_Up_Box_03",
		"Level_Up_Box_04",
		"Level_Up_Box_05",
		"Level_Up_Box_06",
		"Level_Up_Box_07",
		"Level_Up_Box_08",
		"Level_Up_Box_09",
		"Level_Up_Box_10"
	};
	Entity* pEntity = entActivePlayerPtr();
	if( pEntity )
	{
		S32 iPlayerLevel = exprEntGetLevelExp(pEntity);
		unsigned int i;
		const unsigned int NUM_LEVELUP_BOXES = sizeof(s_pchLevelUpBoxNames) / sizeof(s_pchLevelUpBoxNames[0]);
		for(i=0;i<NUM_LEVELUP_BOXES;++i)
		{
			ItemDef * pItemDef = RefSystem_ReferentFromString(g_hItemDict, s_pchLevelUpBoxNames[i]);
			S32 iMinLevel = pItemDef ? (pItemDef->pRestriction ? pItemDef->pRestriction->iMinLevel : 0) : 0;
			if(iMinLevel <= iPlayerLevel )
			{
				if(inv_ent_AllBagsCountItemsAtLeast(entActiveOrSelectedPlayer(), s_pchLevelUpBoxNames[i], 1))
				{
					return pItemDef;
				}
			}
		}
	}
	return NULL;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(LevelUpCheckList_OpenFirstLevelUpBox);
void exprLevelUpCheckList_OpenFirstLevelUpBox()
{
	Entity* pEntity = entActivePlayerPtr();
	if( pEntity && pEntity->pInventoryV2 )
	{
		ItemDef* pItemDef = exprLevelUpCheckList_GetItemDefForLevelUpBox();
		if( pItemDef )
		{
			if( exprUseNextItemOfName(pEntity, InvBagIDs_Inventory, pItemDef->pchName) )
			{
				return;
			}
		}
	}
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(LevelUpCheckList_ShowNewTask);
bool exprLevelUpCheckList_ShowNewTask(int iMinLevel, const char* pchMissionName)
{
	bool bResult = false;
	Entity* pEntity = entActivePlayerPtr();
	if( pEntity && pchMissionName && *pchMissionName )
	{
		int iPlayerLevel = exprEntGetLevelExp(pEntity);
		if( iPlayerLevel >= iMinLevel )
		{
			MissionInfo *pInfo = mission_GetInfoFromPlayer(pEntity);
			if (pInfo)
			{
				CompletedMission * pCompleted = eaIndexedGetUsingString(&pInfo->completedMissions, pchMissionName);
				if(pCompleted)
				{
					bResult = false;
				}
				else
				{
					bResult = true;
				}
			}
		}
	}
	return bResult;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(LevelUpCheckList_ShowNewPowerPoints);
bool exprLevelUpCheckList_ShowNewPowerPoints()
{
	Entity* pEntity = entActivePlayerPtr();
	return pEntity && (0 < gclGenExprEntityGetPowerTablePoints(pEntity, "Playerpowerpoints"));
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(LevelUpCheckList_ShowNewFeats);
bool exprLevelUpCheckList_ShowNewFeats()
{
	Entity* pEntity = entActivePlayerPtr();
	return pEntity && (0 < gclGenExprEntityGetPowerTablePoints(pEntity, "Featpoints_paragon") + gclGenExprEntityGetPowerTablePoints(pEntity, "Featpoints_heroic"));
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(LevelUpCheckList_ShowNewAbility);
bool exprLevelUpCheckList_ShowNewAbility()
{
	Entity* pEntity = entActivePlayerPtr();
	return pEntity && (0 < (gclGenExprGetPointsEarned(pEntity, "AbilityScorePoints") - Player_ExprGetPointsSpentUnderLevel(pEntity, "AbilityScorePoints", exprEntGetLevelExp(pEntity))));
}


AUTO_EXPR_FUNC(UIGen) ACMD_NAME(GenSetDataPower);
bool exprGenSetDataPower(SA_PARAM_NN_VALID UIGen *pGen, SA_PARAM_OP_VALID Power *pPower)
{
	ui_GenSetPointer(pGen, pPower, parse_Power);
	return !!pPower;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(GenSetDataPowerDef);
bool exprGenSetDataPowerDef(SA_PARAM_NN_VALID UIGen *pGen, SA_PARAM_OP_VALID PowerDef *pPowerDef)
{
	ui_GenSetPointer(pGen, pPowerDef, parse_PowerDef);
	return !!pPowerDef;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(GenSetDataItemDef);
bool gclInvExprGenSetDataItemDef(SA_PARAM_NN_VALID UIGen *pGen, SA_PARAM_OP_VALID ItemDef *pItemDef)
{
	ui_GenSetPointer(pGen, pItemDef, parse_ItemDef);
	return !!pItemDef;
}

AUTO_EXPR_FUNC(UIGen);
void PlayerUI_SetLastLevelPowerMenuOpenedAt(U32 iLevel)
{
	ServerCmd_gslPlayerUI_SetLastLevelPowersMenuOpenAt(iLevel);
}

AUTO_EXPR_FUNC(UIGen);
bool PlayerUI_GetLastLevelPowerMenuOpenedAt()
{
	Entity *pEnt = entActivePlayerPtr();
	return (U32)SAFE_MEMBER3(pEnt, pPlayer, pUI, uiLastLevelPowersMenuAt);
}


AUTO_EXPR_FUNC(UIGen);
void PlayerUI_SetLevelUpWizardDismissedAt(U32 iLevel)
{
	ServerCmd_gslPlayerUI_SetLevelUpWizardDismissedAt(iLevel);
}

AUTO_EXPR_FUNC(UIGen);
bool PlayerUI_GetLevelUpWizardDismissedAt()
{
	Entity *pEnt = entActivePlayerPtr();
	return (U32)SAFE_MEMBER3(pEnt, pPlayer, pUI, uiLevelUpWizardDismissedAt);
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(LevelUpCheckList_GetNewestPowerDefByPurpose);
SA_RET_OP_VALID PowerDef* exprLevelUpCheckList_GetNewestPowerDefByPurpose( const char* pchPurpose)
{
	PowerDef* pResultPowerDef = NULL;
	Entity* pEntity = entActivePlayerPtr();
	if( pEntity )
	{
		int iPurpose = StaticDefineIntGetInt(PowerPurposeEnum,pchPurpose);
		int t,n,r,p; // 4 nested loops isn't quite as good as 5.
		S32 iPlayerLevel = exprEntGetLevelExp(pEntity);
		Character* pChar = pEntity->pChar;
		Player* pPlayer = pEntity->pPlayer;
		S32 iLastLevelPowersMenuAt = 0;
		S32 iHighestPowerLevel = 0;

		if( pPlayer && pPlayer->pUI )
		{
			iLastLevelPowersMenuAt = (S32)(U32)pPlayer->pUI->uiLastLevelPowersMenuAt;
			if( iLastLevelPowersMenuAt >= iPlayerLevel )
				return NULL;
		}

		for(t=0;t<eaSize(&pChar->ppPowerTrees);t++)
		{
			PowerTree * pPowerTree = pChar->ppPowerTrees[t];
			PowerTreeDef * pPowerTreeDef = GET_REF(pPowerTree->hDef);
			for(n=0;n<eaSize(&pPowerTree->ppNodes);n++)
			{
				PTNode* pPTNode = pPowerTree->ppNodes[n];
				PTNodeDef* pPTNodeDef = GET_REF(pPTNode->hDef);
				for(r=0;r<eaSize(&pPTNodeDef->ppRanks);r++)
				{
					PTNodeRankDef* pPTNodeRankDef = pPTNodeDef->ppRanks[r];
					if( pPTNodeRankDef->pRequires && (pPTNodeRankDef->pRequires->iTableLevel <= iPlayerLevel) && (pPTNodeRankDef->pRequires->iTableLevel > iLastLevelPowersMenuAt) )
					{
						// Found a power at the player's current level, so run through all the powers in this PTNode to find the newest one.
						for(p=0;p<eaSize(&pPTNode->ppPowers);p++)
						{
							Power* pPower = pPTNode->ppPowers[p];
							PowerDef* pPowerDef = GET_REF(pPower->hDef);
							// Check that it's the right purpose
							if( pPowerDef->ePurpose == iPurpose && iHighestPowerLevel < pPTNodeRankDef->pRequires->iTableLevel )
							{
								pResultPowerDef = pPowerDef;
								iHighestPowerLevel = pPTNodeRankDef->pRequires->iTableLevel;
							}
						}
					}
				}
			}
		}
	}
	return pResultPowerDef;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(LevelUpCheckList_GetNewestPowerDefByCategory);
SA_RET_OP_VALID PowerDef* exprLevelUpCheckList_GetNewestPowerDefByCategory( const char* pchCategory)
{
	PowerDef* pResultPowerDef = NULL;
	Entity* pEntity = entActivePlayerPtr();
	if( pEntity )
	{
		int iCategory = StaticDefineIntGetInt(PowerCategoriesEnum,pchCategory);
		int t,n,r,p,c; // 5 nested loops means you're doing it right.
		S32 iPlayerLevel = exprEntGetLevelExp(pEntity);
		Character* pChar = pEntity->pChar;
		Player* pPlayer = pEntity->pPlayer;
		S32 iLastLevelPowersMenuAt = 0;
		S32 iHighestPowerLevel = 0;

		if( pPlayer && pPlayer->pUI )
		{
			iLastLevelPowersMenuAt = (S32)(U32)pPlayer->pUI->uiLastLevelPowersMenuAt;
			if( iLastLevelPowersMenuAt >= iPlayerLevel )
				return NULL;
		}

		for(t=0;t<eaSize(&pChar->ppPowerTrees);t++)
		{
			PowerTree * pPowerTree = pChar->ppPowerTrees[t];
			PowerTreeDef * pPowerTreeDef = GET_REF(pPowerTree->hDef);
			for(n=0;n<eaSize(&pPowerTree->ppNodes);n++)
			{
				PTNode* pPTNode = pPowerTree->ppNodes[n];
				PTNodeDef* pPTNodeDef = GET_REF(pPTNode->hDef);
				for(r=0;r<eaSize(&pPTNodeDef->ppRanks);r++)
				{
					PTNodeRankDef* pPTNodeRankDef = pPTNodeDef->ppRanks[r];
					if( pPTNodeRankDef->pRequires && (pPTNodeRankDef->pRequires->iTableLevel <= iPlayerLevel) && (pPTNodeRankDef->pRequires->iTableLevel > iLastLevelPowersMenuAt) )
					{
						// Found a power at the player's current level, so run through all the powers in this PTNode to find the newest one.
						for(p=0;p<eaSize(&pPTNode->ppPowers);p++)
						{
							Power* pPower = pPTNode->ppPowers[p];
							PowerDef* pPowerDef = GET_REF(pPower->hDef);
							// Check that it's the right category
							for(c=0;c<eaiSize(&pPowerDef->piCategories);c++)
							{
								if( pPowerDef->piCategories[c] == iCategory && iHighestPowerLevel < pPTNodeRankDef->pRequires->iTableLevel )
								{
									pResultPowerDef = pPowerDef;
									iHighestPowerLevel = pPTNodeRankDef->pRequires->iTableLevel;
								}
							}
						}
					}
				}
			}
		}
	}
	return pResultPowerDef;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(PowerListNodeIsNew);
bool exprPowerListNodeIsNew( SA_PARAM_OP_VALID PowerListNode* pPowerListNode)
{
	Entity* pEntity = entActivePlayerPtr();
	if( pEntity && pPowerListNode )
	{
		S32 iPlayerLevel = exprEntGetLevelExp(pEntity);
		Player* pPlayer = pEntity->pPlayer;
		if( pPlayer && pPlayer->pUI )
		{
			S32 iLastLevelPowersMenuAt = (S32)(U32)pPlayer->pUI->uiLastLevelPowersMenuAt;
			if( (pPowerListNode->iLevel <= iPlayerLevel) && (pPowerListNode->iLevel > iLastLevelPowersMenuAt) )
				return true;
		}
	}
	return false;
}

////////////////
// Paragon Paths
//

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenGetValidSecondaryCharacterPathPowerTreeName");
const char *exprGenGetValidSecondaryCharacterPathPowerTreeName(ExprContext *pContext, bool bObeyRequiredLevel, U32 iIndex)
{
	Entity* pEnt = entActivePlayerPtr();
	if (pEnt && pEnt->pChar)
	{
		static CharacterPath **s_eaPathList = NULL;
		CharacterPath *pPath;
		RefDictIterator iter;

		RefSystem_InitRefDictIterator(g_hCharacterPathDict, &iter);
		while(pPath = RefSystem_GetNextReferentFromIterator(&iter))
		{
			if(character_CanPickSecondaryPath(pEnt->pChar, pPath, bObeyRequiredLevel))
			{
				if( iIndex == 0 )
					return REF_HANDLE_GET_STRING(pPath->hPowerTree);

				--iIndex;
			}
			else
			{
				int i;
				for (i = 0; i < eaSize(&pEnt->pChar->ppSecondaryPaths); i++)
				{
					CharacterPath* pPathOnChar = GET_REF(pEnt->pChar->ppSecondaryPaths[i]->hPath);
					if(pPath == pPathOnChar)
					{
						if( iIndex == 0 )
							return REF_HANDLE_GET_STRING(pPath->hPowerTree);
						
						--iIndex;
						break;
					}
				}
			}
		}
	}
	return "";
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenGetValidSecondaryCharacterPathChoices");
void exprGenGetValidSecondaryCharacterPathChoices(ExprContext *pContext, SA_PARAM_NN_VALID UIGen *pGen, bool bObeyRequiredLevel)
{
	Entity* pEnt = entActivePlayerPtr();
	if (pGen && pEnt && pEnt->pChar)
	{
		static CharacterPath **s_eaPathList = NULL;
		CharacterPath *pPath;
		RefDictIterator iter;

		eaClear(&s_eaPathList);

		RefSystem_InitRefDictIterator(g_hCharacterPathDict, &iter);
		while(pPath = RefSystem_GetNextReferentFromIterator(&iter))
		{
			if(character_CanPickSecondaryPath(pEnt->pChar, pPath, bObeyRequiredLevel))
			{
				eaPush(&s_eaPathList, pPath);
			}
		}

		ui_GenSetListSafe(pGen, &s_eaPathList, CharacterPath);
	}
	else if (pGen)
	{
		ui_GenSetListSafe(pGen, NULL, CharacterPath);
	}
	
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenPlayerCountValidPathChoices");
S32 exprGenPlayerHasAnyValidPathChoices(ExprContext *pContext, bool bObeyRequiredLevel)
{
	Entity* pEnt = entActivePlayerPtr();
	int iCount = 0;
	if (pEnt && pEnt->pChar)
	{
		CharacterPath *pPath;
		RefDictIterator iter;

		RefSystem_InitRefDictIterator(g_hCharacterPathDict, &iter);
		while(pPath = RefSystem_GetNextReferentFromIterator(&iter))
		{
			if(character_CanPickSecondaryPath(pEnt->pChar, pPath, bObeyRequiredLevel))
			{
				iCount++;
			}
		}
	}
	return iCount;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenChooseSecondaryCharacterPath");
void exprGenChooseSecondaryCharacterPath(ExprContext *pContext, const char* pchPathName)
{
	Entity* pEnt = entActivePlayerPtr();
	if (pEnt && pEnt->pChar && pchPathName && pchPathName[0])
	{
		ServerCmd_Character_ChooseSecondaryCharacterPath(pchPathName);
	}
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenGetPlayerPathNamesByType");
const char* exprGenGetPlayerPathNamesByType(ExprContext *pContext, const char* pchType, const char* pchDelimiter)
{
	Entity* pEnt = entActivePlayerPtr();
	CharacterPathType eType = StaticDefineInt_FastStringToInt(CharacterPathTypeEnum, pchType, -1);
	static char* s_pRetStr = NULL;
	int i;

	if (pEnt && pEnt->pChar)
	{
		CharacterPath** eaPaths = NULL;
		estrClear(&s_pRetStr);

		eaStackCreate(&eaPaths, 4);
		entity_GetChosenCharacterPathsOfType(pEnt, &eaPaths, eType);
		for (i = 0; i < eaSize(&eaPaths); i++)
		{
			if (i > 0)
				estrAppend2(&s_pRetStr, pchDelimiter);

			estrAppend2(&s_pRetStr, TranslateDisplayMessage(eaPaths[i]->pDisplayName));
		}
	}
	return s_pRetStr ? exprContextAllocString(pContext, s_pRetStr) : NULL;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenPlayerCountOwnedPathsByType");
S32 exprGenPlayerCountOwnedPathsByType(ExprContext *pContext, const char* pchTypeName)
{
	Entity* pEnt = entActivePlayerPtr();
	CharacterPathType eType = StaticDefineInt_FastStringToInt(CharacterPathTypeEnum, pchTypeName, -1);
	CharacterPath** eaPaths = NULL;
	eaStackCreate(&eaPaths, 4);

	entity_GetChosenCharacterPathsOfType(pEnt, &eaPaths, eType);

	return eaSize(&eaPaths);
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(GenGetExamplePowerDefList);
void exprGenGetExamplePowerDefList(ExprContext *pContext, SA_PARAM_NN_VALID UIGen *pGen, SA_PARAM_OP_VALID Entity *pFakeEnt)
{
	static PowerDef **s_eaPowers = NULL;
	eaClear(&s_eaPowers);
	if( pFakeEnt )
	{
		int p;
		Character* pChar = pFakeEnt->pChar;
		if( pChar )
		{
			CharacterClass *pClass =  GET_REF(pChar->hClass);
			if( pClass )
			{
				for(p=0;p<eaSize(&pClass->ppExamplePowers);++p)
				{
					CharacterClassPower * pPower = pClass->ppExamplePowers[p];
					PowerDef* pPowerDef = GET_REF(pPower->hdef);
					eaPush(&s_eaPowers,pPowerDef);
				}
			}
		}
	}
	ui_GenSetManagedListSafe(pGen, &s_eaPowers, PowerDef, false);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Lockbox Metadata
//
//   This is extra display-only data associated with lockboxes such as a banner add, and special text or items we want
// to highlight in places such as the key-purchase UI. At least initially we can specify a title, subtitle, banner texture name
// and a list of teasers (either message keys or itemdefs) that are displayed as highlighted info or items.
//   Note that the data files and messages are in a strange place (in the ui folder) so we will load the messages. defs files
// (other than config) do not load their .ms files on the client. 

AUTO_ENUM;
typedef enum LockboxTeaserType
{
	LockboxTeaserType_ItemDef,
	LockboxTeaserType_MessageKey
} LockboxTeaserType;

extern StaticDefineInt LockboxTeaserTypeEnum[];

AUTO_STRUCT;
typedef struct LockboxTeaserEntry
{
	const LockboxTeaserType	eTeaserType;		AST(NAME(TeaserType))
	const char *pcTeaserValue;					AST(NAME(TeaserValue))
} LockboxTeaserEntry;

extern ParseTable parse_LockboxTeaserEntry[];
#define TYPE_parse_LockboxTeaserEntry LockboxTeaserEntry


AUTO_STRUCT;
typedef struct LockboxMetadataDef
{
	const char *pchLockboxMetadataName;				AST(KEY STRUCTPARAM POOL_STRING)
		// The internal name of the lockboxMetadata

	const char *pcBannerTexture;			AST(NAME(BannerTexture))

	const char *pcTitleMessageKey;			AST(NAME(TitleMessage))
	const char *pcSubtitleMessageKey;		AST(NAME(SubtitleMessage))

	LockboxTeaserEntry **ppTeaserEntries;	AST(NAME(TeaserEntry))
	
}LockboxMetadataDef;


AUTO_STRUCT;
typedef struct LockboxMetadataDefs
{
	LockboxMetadataDef **ppDefs;					AST(NAME(LockboxMetadata))
}LockboxMetadataDefs;

extern ParseTable parse_LockboxMetadataDefs[];
#define TYPE_parse_LockboxMetadataDefs LockboxMetadataDefs


//  Global storage

LockboxMetadataDefs g_LockboxMetadataDefs;

//////////////
//  Load/reload

static void gclNNOLockboxMetaData_Load()	
{
	StructReset(parse_LockboxMetadataDefs, &g_LockboxMetadataDefs);
	ParserLoadFiles("ui/metadata/LockboxMetadata/", ".lockboxmetadata", "lockboxmetadata.bin", PARSER_OPTIONALFLAG, parse_LockboxMetadataDefs, &g_LockboxMetadataDefs);
}

static void gclNNOLockboxMetaData_Reload_CB(const char *relpath, int when)
{
	loadstart_printf("Reloading Lockbox Metadata...");
	gclNNOLockboxMetaData_Load();
	loadend_printf(" done.");
}


AUTO_STARTUP(LockboxMetadata,1);
void gclLockboxMetadata_Load(void)
{
	loadstart_printf("Loading Lockbox Metadata... ");

	gclNNOLockboxMetaData_Load();
	loadend_printf("done (%d entries)", eaSize(&g_LockboxMetadataDefs.ppDefs));
	FolderCacheSetCallback(FOLDER_CACHE_CALLBACK_UPDATE, "defs/lockboxmetadata/*.lockboxmetadata", gclNNOLockboxMetaData_Reload_CB);
}

//////////////
// Find

static LockboxMetadataDef *LockboxMetadataDef_Find(const char *pchName)
{
	if(pchName && *pchName)
	{
		int iIndex = eaIndexedFindUsingString(&g_LockboxMetadataDefs.ppDefs,pchName);
		if(iIndex != -1)
			return g_LockboxMetadataDefs.ppDefs[iIndex];
	}
	return NULL;
}

//////////////
// Uigen expressions to fetch data

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenLockboxMetadata_GetBannerTexture");
const char* exprGenLockboxMetadata_GetBannerTexture(ExprContext *pContext, const char* pchMetadataDefName)
{
	LockboxMetadataDef *pMetadataDef = LockboxMetadataDef_Find(pchMetadataDefName);

	if (pMetadataDef!=NULL && pMetadataDef->pcBannerTexture!=NULL)
	{
		return(pMetadataDef->pcBannerTexture);
	}
	return("");
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenLockboxMetadata_GetTitle");
const char* exprGenLockboxMetadata_GetTitle(ExprContext *pContext, const char* pchMetadataDefName)
{
	LockboxMetadataDef *pMetadataDef = LockboxMetadataDef_Find(pchMetadataDefName);

	if (pMetadataDef!=NULL && pMetadataDef->pcTitleMessageKey!=NULL)
	{
		return(pMetadataDef->pcTitleMessageKey);
	}
	return("");
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenLockboxMetadata_GetSubtitle");
const char* exprGenLockboxMetadata_GetSubtitle(ExprContext *pContext, const char* pchMetadataDefName)
{
	LockboxMetadataDef *pMetadataDef = LockboxMetadataDef_Find(pchMetadataDefName);

	if (pMetadataDef!=NULL && pMetadataDef->pcSubtitleMessageKey!=NULL)
	{
		return(pMetadataDef->pcSubtitleMessageKey);
	}
	return("");
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME("GenLockboxMetadata_GetTeaserEntries");
void exprGenLockboxMetadata_GetTeaserEntries(SA_PARAM_NN_VALID UIGen *pGen, SA_PARAM_OP_VALID Entity *pEnt, const char *pchMetadataDefName)
{
	LockboxTeaserEntry ***peaLTEList = ui_GenGetManagedListSafe(pGen, LockboxTeaserEntry);
	LockboxMetadataDef *pMetadataDef = LockboxMetadataDef_Find(pchMetadataDefName);
	S32 i;

	eaClear(peaLTEList);

	if (pMetadataDef!=NULL)
	{
		for(i = 0; i < eaSize(&(pMetadataDef->ppTeaserEntries)); i++)
		{
			eaPush(peaLTEList, pMetadataDef->ppTeaserEntries[i]);
		}
	}
	ui_GenSetManagedListSafe(pGen, peaLTEList, LockboxTeaserEntry, false);
}

///////////////
// Stat Blocks Power helpers
// For use in the Character Creator with the AbilityScore_<Class>_<StatBlock>.powers
AUTO_EXPR_FUNC(UIGen) ACMD_NAME(CharacterCreation_GetAbilityValueFromStatBlock);
S64 exprCharacterCreation_GetAbilityValueFromStatBlock(ExprContext* pContext, const char * pcStatName, const char* pchTreeName, SA_PARAM_OP_VALID Entity * pEntity)
{
	S32 t, n, m;

	// Map pcStatName to AttribType
	AttribType attrib = (AttribType)StaticDefineIntGetInt(AttribTypeEnum,pcStatName);

	// Find the PowerDef in the tree that maps to the requested stat

	// Find the requested tree on the character
	PowerTreeDef *pTreeDef = powertreedef_Find(pchTreeName);

	if( !pEntity || !pEntity->pChar || !pTreeDef)
		return 0;

	for(t = eaSize(&pEntity->pChar->ppPowerTrees)-1; t >= 0; t--)
	{
		PowerTree *pTree = pEntity->pChar->ppPowerTrees[t];
		if(GET_REF(pTree->hDef) == pTreeDef)
		{
			// Find the node that modifies the requested Stat
			for (n = eaSize(&pTree->ppNodes)-1; n >= 0; n--)
			{
				PTNodeDef *pNode = GET_REF(pTree->ppNodes[n]->hDef);
				PowerDef *pPowerDef = GET_REF(pNode->ppRanks[0]->hPowerDef);
				for( m=0; m<eaSize(&pPowerDef->ppOrderedMods); ++m )
				{
					AttribModDef* pAttribModDef = pPowerDef->ppOrderedMods[m];
					if( attrib == pAttribModDef->offAttrib )
					{
						// Return the value of the magnitude expression for that AttribMod
						MultiVal result;
						exprEvaluate(pAttribModDef->pExprMagnitude, pContext, &result);
						return MultiValGetInt(&result,NULL);
					}
				}
			}
		}
	}

	// 
	return 0;
}

//////////////
// Init function to expose enum values to the uigens
AUTO_RUN_LATE;
void gclLockboxMetadata_Init(void)
{
	ui_GenInitStaticDefineVars(LockboxTeaserTypeEnum, "LockboxTeaserType_");
}

//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
S32 gclExprEntGetAttribModMagnitudeByTag(SA_PARAM_OP_VALID Entity *pent,
										const char* attribName,
										S32 index,
										char *pchTags);

static void GetTemporaryHP(Character *pChar, S32 *piTempHPOut ,S32 *piTempHPMaxOut)
{
	static S32 *s_piTags = NULL;
	static AttribModNet **s_ppModNets = NULL;
	
	eaClearFast(&s_ppModNets);
	
	
	if (! eaiSize(&s_piTags))
		StaticDefineIntParseStringForInts(PowerTagsEnum, "TemporaryHP", &s_piTags, NULL);

	*piTempHPOut = 0;
	*piTempHPMaxOut = 0;
		
	character_ModsNetGetTotalMagnitudeByTag(pChar, kAttribType_Shield, s_piTags, &s_ppModNets);

	FOR_EACH_IN_EARRAY_FORWARDS(s_ppModNets, AttribModNet, pNet)
	{
		*piTempHPOut += pNet->iHealth;
		*piTempHPMaxOut += pNet->iHealthMax ? pNet->iHealthMax : pNet->iHealth;
	}
	FOR_EACH_END

}


AUTO_EXPR_FUNC(entityutil) ACMD_NAME("NNOEntGetTempHealthPoints");
S32 exprNNOEntGetTempHealthPoints(SA_PARAM_OP_VALID Entity *pEntity)
{
	if (pEntity && pEntity->pChar && pEntity->pChar->pattrBasic)
	{
		S32 tempHP = 0;
		S32 maxTempHP = 0;
		
		GetTemporaryHP(pEntity->pChar, &tempHP, &maxTempHP);

		return tempHP;
	}

	return 0.f;
}


// Returns the health percent of the Entity [0..1].  Returns 1 on failure.
AUTO_EXPR_FUNC(entityutil) ACMD_NAME("NNOEntGetHealthPercent");
F32 exprNNOEntGetHealthPercent(SA_PARAM_OP_VALID Entity *pEntity)
{
	if (pEntity && pEntity->pChar && pEntity->pChar->pattrBasic)
	{
		F32 fMaxHP = pEntity->pChar->pattrBasic->fHitPointsMax;
		F32 fCurHP = pEntity->pChar->pattrBasic->fHitPoints;
		S32 tempHP = 0;
		S32 maxTempHP = 0;
		
				
		if (fMaxHP <= 0)
			return 1.f;

		GetTemporaryHP(pEntity->pChar, &tempHP, &maxTempHP);

		if (tempHP <= 0 || fCurHP + maxTempHP <= fMaxHP)
			return fCurHP / fMaxHP;
		
		// with tempHP we have more than the maxHP, so we are going to need to scale things
		return fCurHP / (fCurHP + maxTempHP);
	}

	return 1.f;
}

// Returns the health percent of the Entity [0..1].  Returns 1 on failure.
AUTO_EXPR_FUNC(entityutil) ACMD_NAME("NNOEntGetTempHealthPercent");
F32 exprNNOEntGetTempHealthPercent(SA_PARAM_OP_VALID Entity *pEntity)
{
	if (pEntity && pEntity->pChar && pEntity->pChar->pattrBasic)
	{
		F32 fMaxHP = pEntity->pChar->pattrBasic->fHitPointsMax;
		F32 fCurHP = pEntity->pChar->pattrBasic->fHitPoints;
		S32 tempHP = 0;
		S32 maxTempHP = 0;
		
		if (fMaxHP <= 0)
			return 0.f;
		
		GetTemporaryHP(pEntity->pChar, &tempHP, &maxTempHP);
		
		if (tempHP <= 0)
			return 0.f;
		
		if (fCurHP + maxTempHP <= fMaxHP)
			return (fCurHP+tempHP)/(fMaxHP);

		return (fCurHP+tempHP)/(fCurHP + maxTempHP);
	}

	return 0.f;

}

#include "autogen/NNOClientExpressions_c_ast.c"

// End of File
