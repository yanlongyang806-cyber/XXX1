/***************************************************************************
 
 
 
 *
 ***************************************************************************/

#include "NNOGatewayCraftingMapping.h"
#include "Gateway/gslGatewaySession.h"
#include "ItemAssignments.h"
#include "itemAssignments_h_ast.h"

#include "Entity.h"
#include "Player.h"
#include "gslItemAssignments.h"
#include "ItemAssignmentsUICommon.h"
#include "ItemAssignmentsUICommon_h_ast.h"

#include "itemCommon.h"
#include "itemCommon_h_ast.h"
#include "inventoryCommon.h"
#include "inventoryCommon_h_ast.h"
#include "itemEnums.h"
#include "itemEnums_h_ast.h"

#include "NNOGatewayCraftingMapping_h_ast.h"

void SubscribeCraftingList(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams)
{
	
}

bool IsModifiedCraftingList(GatewaySession *psess, ContainerTracker *ptracker)
{
	Entity *pent = session_GetLoginEntity(psess);
	MappedCraftingList *pList = (MappedCraftingList*)ptracker->pMapped;
	ItemAssignmentPersistedData *pData = SAFE_MEMBER2(pent,pPlayer,pItemAssignmentPersistedData);
	ItemAssignmentPersonalPersistedBucket *pPersistedBucket = pData ? eaGet(&pData->eaPersistedPersonalAssignmentBuckets, 0) : NULL;

	if(pList && pPersistedBucket && 
		(pPersistedBucket->uLastPersonalUpdateTime != pList->uLastPersonalUpdateTime
		|| !pent->pPlayer->pItemAssignmentData || pent->pPlayer->pItemAssignmentData->uLastUpdateTime != pList->uLastUpdateTime))
	{
		return true;
	}

	return false;
}

bool CheckModifiedCraftingList(GatewaySession *psess, ContainerTracker *ptracker)
{
	return false;
}

bool IsReadyCraftingList(GatewaySession *psess, ContainerTracker *ptracker)
{
	return session_GetLoginEntity(psess) != NULL;
}

bool CraftingMappingItemDefCheck(ItemDef *pDef)
{
	S32 s_eDoNotDisplay = StaticDefineIntGetInt(ItemCategoryEnum,"Craft_DoNotDisplay");

	if(!pDef || eaiFind(&pDef->peCategories, s_eDoNotDisplay) != -1)
		return false;

	return true;
}

void AddMappedCraftingItem(MappedCraftingItem ***pppItems, ItemDef *pDef, int iCount)
{
	MappedCraftingItem *pMappedItem;

	if(!CraftingMappingItemDefCheck(pDef))
		return;
	
	pMappedItem = StructCreate(parse_MappedCraftingItem);

	SET_HANDLE_FROM_REFERENT(g_hItemDict,pDef,pMappedItem->hDef);
	pMappedItem->iCount = iCount;

	eaPush(pppItems,pMappedItem);
}

#include "ActivityCommon.h"
#include "gslActivity.h"

void *CreateMappedCraftingList(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj)
{
	//Assume leadership
	const char *pchCraft = ptracker->estrID;
	static ItemAssignmentList pList = {0};
	MappedCraftingList *pData = StructCreate(parse_MappedCraftingList);
	Entity *pent = session_GetLoginEntity(psess);
	char *pchLastHeadder = NULL;
	int i;
	ItemCategory eCategory = StaticDefineIntGetInt(ItemCategoryEnum,pchCraft);
	const ItemAssignmentCategorySettings *pSettings;
	ItemAssignmentPersistedData *pItemAssignmentData = SAFE_MEMBER2(pent,pPlayer,pItemAssignmentPersistedData);
	ItemAssignmentPersonalPersistedBucket *pPersistedBucket = pItemAssignmentData ? eaGet(&pItemAssignmentData->eaPersistedPersonalAssignmentBuckets, 0) : NULL;
	//ItemAssignmentCategorySettings** ppCategories = ItemAssignmentCategory_GetCategoryList();

	//If no entity or item assignment data, just return an empty mapped structure
	if(!pent || !pItemAssignmentData)
		return pData;

	PERFINFO_AUTO_START_FUNC();

	pSettings = ItemAssignmentCategory_GetSettings(eCategory);

	eaClearStruct(&pList.ppAssignments,parse_ItemAssignmentUI);

	if(pPersistedBucket)
		pData->uLastPersonalUpdateTime = pPersistedBucket->uLastPersonalUpdateTime;

	pData->uLastUpdateTime = pent->pPlayer->pItemAssignmentData->uLastUpdateTime;

	if(!pent)
	{
		PERFINFO_AUTO_STOP();
		return pData;
	}

	if(eCategory && !pSettings)
	{
		const ItemAssignmentCategorySettings** ppCategories = ItemAssignmentCategory_GetCategoryList();
		for(i=0;i<eaSize(&ppCategories);i++)
		{
			if(eaiFind(&ppCategories[i]->peAssociatedItemCategories,eCategory) != -1)
			{
				pSettings = ppCategories[i];
				break;
			}
		}
	}

	if(pSettings)
	{
		if(ItemAssignments_EvaluateCategoryIsHidden(pent,pSettings))
		{
			PERFINFO_AUTO_STOP();
			return pData;
		}
	}

	if(pSettings)
	{
		COPY_HANDLE(pData->DisplayName.hMessage,pSettings->msgDisplayName.hMessage);

		if(pSettings && pSettings->pchEventName)
		{
			U32 uEventClockTime = gslActivity_GetEventClockSecondsSince2000();
			U32 uEndTime,uStartTime;
			EventDef *pEventDef = EventDef_Find(pSettings->pchEventName);

			if(pEventDef)
			{
				ShardEventTiming_GetUsefulTimes(&(pEventDef->ShardTimingDef),uEventClockTime,&uStartTime,&uEndTime,NULL);

				if(uStartTime <= uEventClockTime && uEndTime >= uEventClockTime)
					pData->uAvailabilityTime = uEndTime - (gslActivity_GetEventClockSecondsSince2000() - timeSecondsSince2000());
			}

			pData->pchAvailabilityEvent = pSettings->pchEventName;
		}
	}

	pent->iPartitionIdx_UseAccessor = 1;

	if(!psess->pItemAssignmentsCache)
	{
		psess->pItemAssignmentsCache = StructCreate(parse_ItemAssignmentCachedStruct);
	}

	gslRequestItemAssignments(pent);

	GetAvailableItemAssignmentsByCategoryInternal(psess->lang, pent, &pList.ppAssignments, "CraftingInventory", pchCraft, "Base_Common Base_Rare", 
		kGetActiveAssignmentFlags_SortByWeight | kGetActiveAssignmentFlags_AddWeightHeaders | kGetActiveAssignmentFlags_SortRequiredNumericAscending, 
		-1, -1, 0, NULL);



	for(i=0;i<eaSize(&pList.ppAssignments);i++)
	{
		MappedCraftingEntry *pEntry = StructCreate(parse_MappedCraftingEntry);
		ItemAssignmentDef *pDef = NULL;
		int n,j;
		int iCount = 0;
		ItemAssignmentSlotUI **ppSlots = NULL;
		ItemAssignmentSlotUI **ppRequiredSlots = NULL;
		ItemAssignmentOutcome *pBaseOutcome = NULL;

		pEntry->bIsHeader = pList.ppAssignments[i]->bIsHeader;
		if(pEntry->bIsHeader)
		{
			estrCopy2(&pEntry->pchHeader,pList.ppAssignments[i]->pchDisplayName);

			pchLastHeadder = pEntry->pchHeader;

			eaPush(&pData->ppEntries,pEntry);

			pEntry->uNextUpdateTime = timeSecondsSince2000() + ItemAssignmentPersonalUpdateTime(pent,0);

			continue;
		}

		pDef = GET_REF(pList.ppAssignments[i]->hDef);

		SET_HANDLE_FROM_REFERENT(g_hItemAssignmentDict,GET_REF(pList.ppAssignments[i]->hDef),pEntry->hDef);
		estrCopy2(&pEntry->pchFailedRequirementsReasons,pList.ppAssignments[i]->estrFailsRequires);
		estrCopy2(&pEntry->pchHeader,pchLastHeadder);
		pEntry->bFailsLevelRequirements = pList.ppAssignments[i]->eFailsRequiresReasons & kItemAssignmentFailsRequiresReason_RequiredNumeric ? true : false;
		pEntry->bFailsLevelRequirementsFilter = pList.ppAssignments[i]->eFailsRequiresReasons & kItemAssignmentFailsRequiresReason_RequiredNumeric && pDef->pRequirements->iRequiredNumericValue >= inv_GetNumericItemValue(pent,REF_STRING_FROM_HANDLE(pDef->pRequirements->hRequiredNumeric)) + 3;
		pEntry->bFailsResourcesRequirements = pList.ppAssignments[i]->eFailsRequiresReasons & kItemAssignmentFailsRequiresReason_CantFillSlots
			|| pList.ppAssignments[i]->eFailsRequiresReasons & kItemAssignmentFailsRequiresReason_RequiredItemCost ? true : false;

		SetGenSlotsForItemAssignment(psess->lang, pent,&ppSlots,&iCount,true,&ppRequiredSlots,pDef->pchName,ITEM_ASSIGNMENT_SLOTOPTION_EXCLUDE_OPTIONAL);

		for(n=0;n<eaSize(&ppRequiredSlots);n++)
		{
			MappedCraftingSlot *pItem;

			pItem = StructCreate(parse_MappedCraftingSlot);

			estrCopy2(&pItem->pchIcon, ppRequiredSlots[n]->pchIcon);
			pItem->bFillsRequirements = ppRequiredSlots[n]->eFailsReason == 0;
			estrCopy2(&pItem->pchCategories, ppRequiredSlots[n]->estrRequiredCategories);

			eaPush(&pEntry->ppRequired,pItem);
		}

		for(n=0;n<eaSize(&pDef->pRequirements->eaItemCosts);n++)
		{
			MappedCraftingItem *pItem = StructCreate(parse_MappedCraftingItem);
			ItemDef *pItemDef = GET_REF(pDef->pRequirements->eaItemCosts[n]->hItem);

			if(pItemDef && pItemDef->eType != kItemType_Numeric)
			{
				pItem->iRequired = pDef->pRequirements->eaItemCosts[n]->iCount;
				SET_HANDLE_FROM_REFERENT(g_hItemDict,pItemDef,pItem->hDef);

				if (pItemDef->eType == kItemType_Numeric) {
					pItem->iCount = inv_GetNumericItemValue(pent, pItemDef->pchName);
				} else {
					S32 eSearchBags = 0;
					S32 eExcludeSearchBags = 0;
					ItemAssignments_GetSearchInvBagFlags(&eSearchBags, &eExcludeSearchBags);
					
					pItem->iCount = inv_FindItemCountByDefNameEx(pent, eSearchBags, eExcludeSearchBags, pItemDef->pchName, kItemFlag_SlottedOnAssignment, pItem->iRequired, false);
				}

				pItem->bFillsRequirements = pItem->iCount >= pItem->iRequired;
			}

			eaPush(&pEntry->ppConsumables,pItem);
		}

		

		for(j=0;j<eaSize(&pDef->eaOutcomes);j++)
		{
			pBaseOutcome = eaGet(&pDef->eaOutcomes, j);

			if (SAFE_MEMBER2(pBaseOutcome, pResults, pSampleRewards))
			{
				InvRewardRequest *pRequest = pBaseOutcome->pResults->pSampleRewards;

				for(n=0;n<eaSize(&pRequest->eaItemRewards);n++)
				{
					AddMappedCraftingItem(&pEntry->ppRewards,GET_REF(pRequest->eaItemRewards[n]->hItem),pRequest->eaItemRewards[n]->count);
				}

				for (n = 0; n < eaSize(&pRequest->eaNumericRewards); n++)
				{
					AddMappedCraftingItem(&pEntry->ppRewards,GET_REF(pRequest->eaNumericRewards[n]->hDef),pRequest->eaNumericRewards[n]->iNumericValue);
				}

				break;
			}
		}
		

		eaPush(&pData->ppEntries,pEntry);

		eaDestroyStruct(&ppSlots,parse_ItemAssignmentSlotUI);
		eaDestroy(&ppRequiredSlots);
	}

	PERFINFO_AUTO_STOP();
	return pData;
}

void DestroyMappedCraftingList(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj)
{
	StructDestroy(parse_MappedCraftingList, pvObj);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// Crafting Detail

void SubscribeCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvParams)
{

}

bool IsModifiedCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker)
{
	return false;
}

bool CheckModifiedCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker)
{
	return false;
}

bool IsReadyCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker)
{
	return !!session_GetLoginEntity(psess);
}

ItemAssignmentGatewaySlotData *gslGateway_ConvertSlotUIToGatewaySlotData(Entity *pEnt, ItemAssignmentSlotUI **ppSlots, ItemAssignmentSlotUI *pData)
{
	ItemAssignmentGatewaySlotData *pReturn;
	InvBagIDs BagID = StaticDefineIntGetInt(InvBagIDsEnum, "CraftingInventory");
	Item **eaPotentialItems = NULL;

	pReturn = StructCreate(parse_ItemAssignmentGatewaySlotData);
	pReturn->pSlot = StructClone(parse_ItemAssignmentSlotUI, pData);

	ItemAssignments_GetPotentialItemsForSlot(pEnt, BagID, ppSlots, pData, &eaPotentialItems);

	FOR_EACH_IN_EARRAY_FORWARDS(eaPotentialItems, Item, pItem)
	{
		ItemDef *pItemDef = pItem ? GET_REF(pItem->hItem) : NULL;
		if (pItem && pItemDef)
		{
			MappedCraftingItem *pItemData = NULL;
			pItemData = StructCreate(parse_MappedCraftingItem);

			SET_HANDLE_FROM_REFERENT(g_hItemDict, pItemDef, pItemData->hDef);
			pItemData->uID = pItem->id;
			pItemData->iCount = pItem->count;

			eaPush(&pReturn->ppItems, pItemData);
		}
	}
	FOR_EACH_END

	eaDestroy(&eaPotentialItems);
	return pReturn;
}

void *CreateMappedCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj)
{
	const char *pchCraft = ptracker->estrID;
	ItemAssignmentDef *pDef =  ItemAssignment_DefFromName(pchCraft);
	MappedCraftingDetail *pData = NULL;
	int i,n;
	S32 iCount = 0;
	Entity *pent = NULL;
	ItemAssignmentSlotUI **ppSlots = NULL;
	Language lang = psess->lang;

	PERFINFO_AUTO_START_FUNC();

	pent = session_GetLoginEntity(psess);
	if(!pent)
	{
		PERFINFO_AUTO_STOP();
		return NULL;
	}

	pent->iPartitionIdx_UseAccessor = 1;

	if(!psess->pItemAssignmentsCache)
	{
		psess->pItemAssignmentsCache = StructCreate(parse_ItemAssignmentCachedStruct);
		psess->pItemAssignmentsCache->pRewardRequestData = StructCreate(parse_ItemAssignmentRewardRequestData);
	}

	pIACache = psess->pItemAssignmentsCache;

	if(pDef)
	{
		ItemAssignmentSlottedItem **eaItems = NULL;

		pData = StructCreate(parse_MappedCraftingDetail);

		SET_HANDLE_FROM_REFERENT(g_hItemAssignmentDict,pDef,pData->hDef);

		if(eaSize(&psess->pItemAssignmentsCache->eaSlots) == 0 || GET_REF(psess->pItemAssignmentsCache->eaSlots[0]->hDef) != pDef)
		{
			for (i = eaSize(&psess->pItemAssignmentsCache->eaSlots)-1; i >= 0; i--)
			{
				ItemAssignmentsClearSlottedItem(psess->pItemAssignmentsCache->eaSlots[i]);
			}

			eaSetSizeStruct(&psess->pItemAssignmentsCache->eaSlots, parse_ItemAssignmentSlotUI, eaSize(&pDef->eaSlots));

			buildSlotsForItemAssignment(lang, pent, pDef, true, &psess->pItemAssignmentsCache->eaSlots, &iCount);
			SET_HANDLE_FROM_REFERENT(g_hItemAssignmentDict,pDef,psess->pItemAssignmentsCache->hCurrentDef);

			ItemAssignments_AutoSlotBestItems(lang, pent);
		}

		eaItems = ItemAssignment_GetOrCreateTempItemAssignmentSlottedItemList();

		//Get required assets structures
		//SetGenSlotsForItemAssignment(pent,&psess->pItemAssignmentsCache->eaSlots,&iCount,true,&ppSlots,pDef->pchName,ITEM_ASSIGNMENT_SLOTOPTION_EXCLUDE_OPTIONAL);

		for(n=0;n<eaSize(&psess->pItemAssignmentsCache->eaSlots);n++)
		{
			ItemAssignmentGatewaySlotData *pSlotData = gslGateway_ConvertSlotUIToGatewaySlotData(pent,psess->pItemAssignmentsCache->eaSlots,psess->pItemAssignmentsCache->eaSlots[n]);

			if(pSlotData->pSlot->bOptionalSlot)
			{
				pSlotData->index = eaSize(&pData->ppOptional);
				eaPush(&pData->ppOptional,pSlotData);
			}
			else
			{
				pSlotData->index = eaSize(&pData->ppRequired);
				eaPush(&pData->ppRequired,pSlotData);
			}
		}

		// List all consumable items
		for(n=0;n<eaSize(&pDef->pRequirements->eaItemCosts);n++)
		{
			MappedCraftingItem *pItem = StructCreate(parse_MappedCraftingItem);
			ItemDef *pItemDef = GET_REF(pDef->pRequirements->eaItemCosts[n]->hItem);

			if(pItemDef && pItemDef->eType != kItemType_Numeric)
			{
				pItem->iRequired = pDef->pRequirements->eaItemCosts[n]->iCount;
				
				SET_HANDLE_FROM_REFERENT(g_hItemDict,pItemDef,pItem->hDef);
				
				if (pItemDef->eType == kItemType_Numeric) {
					pItem->iCount = inv_GetNumericItemValue(pent, pItemDef->pchName);
				} else {
					S32 eSearchBags = 0;
					S32 eExcludeSearchBags = 0;
					ItemAssignments_GetSearchInvBagFlags(&eSearchBags, &eExcludeSearchBags);

					pItem->iCount = inv_FindItemCountByDefNameEx(pent, eSearchBags, eExcludeSearchBags, pItemDef->pchName, kItemFlag_SlottedOnAssignment, pItem->iRequired, false);
				}

				pItem->bFillsRequirements = pItem->iCount >= pItem->iRequired;
			}
			else
			{
				pItem->bFillsRequirements = 0;
			}

			eaPush(&pData->ppConsumables,pItem);
		}

		{
			ItemAssignmentOutcomeUI **eaOutcomes = NULL;

			ItemAssignment_GetUIOutcomes(lang, pent,pDef,eaItems,&eaOutcomes);

			for (n = 0; n < eaSize(&pDef->eaOutcomes); n++)
			{
				int j;

				ItemAssignmentOutcomeRewardRequest* pRequest = StructCreate(parse_ItemAssignmentOutcomeRewardRequest);
				ItemAssignmentOutcome* pOutcome = pDef->eaOutcomes[n];
				MappedCraftingRewardRank* pRewardRank = StructCreate(parse_MappedCraftingRewardRank);

				gslItemAssignments_FillOutcomeRewardRequest(pent, NULL, NULL, pDef, pOutcome, pRequest);

				eaPush(&pData->ppRewardOutcomes,pRewardRank);

				pRewardRank->pData = StructClone(parse_InvRewardRequest,pRequest->pData);
				pRewardRank->pChanceData = StructClone(parse_ItemAssignmentOutcomeUI,eaOutcomes[n]);

				for(j=eaSize(&pRewardRank->pData->eaNumericRewards)-1;j>=0;j--)
				{
					ItemDef *pItemDef = GET_REF(pRewardRank->pData->eaNumericRewards[j]->hDef);

					if(!CraftingMappingItemDefCheck(pItemDef))
					{
						StructDestroy(parse_ItemNumericData,pRewardRank->pData->eaNumericRewards[j]);
						eaRemove(&pRewardRank->pData->eaNumericRewards,j);
					}
				}

				for(j=eaSize(&pRewardRank->pData->eaItemRewards)-1;j>=0;j--)
				{
					ItemDef *pItemDef = GET_REF(pRewardRank->pData->eaItemRewards[j]->hItem);

					if(!CraftingMappingItemDefCheck(pItemDef))
					{
						StructDestroy(parse_Item,pRewardRank->pData->eaItemRewards[j]);
						eaRemove(&pRewardRank->pData->eaItemRewards,j);
					}
				}

				StructDestroy(parse_ItemAssignmentOutcomeRewardRequest,pRequest);
			}
		}

		estrClear(&pData->pchFailedRequirementsReasons);

		pData->bFailsRequirements = ItemAssignment_GetFailsRequirementsReason(lang, pent, pDef,
			&pData->pchFailedRequirementsReasons, &pData->eFailsRequiresReasons,
			pData->bfInvalidSlots, (ARRAY_SIZE(pData->bfInvalidSlots) * sizeof(pData->bfInvalidSlots[0]) * 8), true);

		pData->uDuration = ItemAssignments_CalculateDuration(pent,pDef,eaItems);
	}

	PERFINFO_AUTO_STOP();
	return pData;
}

void DestroyMappedCraftingDetail(GatewaySession *psess, ContainerTracker *ptracker, void *pvObj)
{
	StructDestroy(parse_MappedCraftingDetail, pvObj);
}

#include "NNOGatewayCraftingMapping_h_ast.c"