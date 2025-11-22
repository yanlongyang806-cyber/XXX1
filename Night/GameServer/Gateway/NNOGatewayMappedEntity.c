/***************************************************************************
 
 
 
 *
 ***************************************************************************/
#include "stdtypes.h"
#include "MemoryPool.h"

#include "Entity.h"
#include "Player.h"
#include "EntitySavedData.h"
#include "CharacterClass.h"
#include "Character.h"
#include "Character_tick.h"
#include "OfficerCommon.h"
#include "MapDescription.h"
#include "GameStringFormat.h"
#include "../StaticWorld/ZoneMap.h"

#include "Gateway/gslGatewaySession.h"
#include "PowerTreeHelpers.h"
#include "EntityLib.h"
#include "Gateway/gslGatewayMappedEntity.h"
#include "ai/aiFCExprFunc.h"
#include "GameAccountDataCommon.h"
#include "CharacterAttribs.h"
#include "CharacterClass.h"
#include "mission_common.h"
#include "tradeCommon.h"

#include "Powers_h_ast.h"
#include "Entity_h_ast.h"
#include "EntitySavedData_h_ast.h"
#include "entCritter.h"
#include "entCritter_h_ast.h"
#include "inventoryCommon.h"
#include "InventoryCommon_h_ast.h"
#include "OfficerCommon_h_ast.h"
#include "MapDescription_h_ast.h"
#include "Powertree_h_ast.h"
#include "ItemCommon.h"
#include "itemCommon_h_ast.h"
#include "ItemEnums.h"
#include "itemEnums_h_ast.h"
#include "tradeCommon_h_ast.h"

#include "ItemAssignments.h"
#include "ItemAssignmentsUICommon.h"
#include "itemAssignments_h_ast.h"
#include "ItemAssignmentsUICommon_h_ast.h"
#include "gslItemAssignments.h"
#include "NNOItemDescription.h"
#include "NumericConversionCommon.h"


#include "textparser.h"

#include "NNOGatewayContainerMapping.h"
#include "NNOGatewayCraftingMapping.h"
#include "NNOGatewayCommonMappings.h"

#include "NNOGatewayCommonMappings_h_ast.h"
#include "NNOItemDescription_h_ast.h"
#include "NNOGatewayMappedEntity_c_ast.h"



#define DBG_PRINTF(format, ...) printf(format,##__VA_ARGS__)
//#define DBG_PRINTF(format, ...)

typedef struct Critter Critter;
typedef struct Inventory Inventory;
typedef struct ItemAssignmentUI ItemAssignmentUI;

/////////////////////////////////////////////////////////////////////////////
// Owned container needs to exist because session.jspp requires it to be there
// NW currently does not have any owned containers, so the structure is mostly blank
//

AUTO_STRUCT;
typedef struct OwnedContainer
{
	ContainerID id;   AST(NAME(id))
		// All Owned Containers have an ID
} OwnedContainer;

AUTO_STRUCT;
typedef struct MappedInventory
{
	EARRAY_OF(MappedInvBag) ppBags;					AST(NAME(Bags))
	EARRAY_OF(MappedInvBag) ppPlayerBags;			AST(NAME(PlayerBags) SELF_ONLY)

	EARRAY_OF(MappedInvSlot) ppNotAssignedSlots;	AST(NAME(NotAssignedSlots) SELF_ONLY) 
	EARRAY_OF(MappedInvSlot) ppAssignedSlots;		AST(NAME(AssignedSlots) SELF_ONLY)

	MappedInvSlot **ppTradeItems;					AST(NAME(TradeBag) SELF_ONLY)

} MappedInventory;

AUTO_STRUCT;
typedef struct MappedAttribs
{
	F32 fStr;			AST(NAME(STR) DEFAULT(-1))
	F32 fCon;			AST(NAME(CON) DEFAULT(-1))
	F32 fDex;			AST(NAME(DEX) DEFAULT(-1))
	F32 fInt;			AST(NAME("INT") DEFAULT(-1))
	F32 fWis;			AST(NAME(WIS) DEFAULT(-1))
	F32 fCha;			AST(NAME(CHA) DEFAULT(-1))

	F32 fPower;			AST(NAME(stat_power) DEFAULT(-1))
	F32 fCrit;			AST(NAME(stat_crit) DEFAULT(-1))
	F32 fArmorPen;		AST(NAME(stat_ArmorPen) DEFAULT(-1))
	F32 fRecovery;		AST(NAME(stat_Recovery) DEFAULT(-1))
	F32 fDefense;		AST(NAME(stat_Defense) DEFAULT(-1))
	F32 fRegen;			AST(NAME(stat_Regen) DEFAULT(-1))
	F32 fLifeSteal;		AST(NAME(stat_HealthSteal) DEFAULT(-1))
	F32 fMovement;		AST(NAME(stat_Movement) DEFAULT(-1))
	F32 fDeflect;		AST(NAME(stat_Deflect) DEFAULT(-1))

	F32 fHitPointsMax;		AST(NAME(HitPointsMax) DEFAULT(-1))
	F32 fArmorClass;		AST(NAME(AC) DEFAULT(-1))
	F32 fMagicArmorClass;	AST(NAME(MagicAC) DEFAULT(-1))
} MappedAttribs;

AUTO_STRUCT;
typedef struct MappedCurrencies
{
	U32 iGold;			AST(NAME(Gold) SELF_ONLY)
	U32 iSilver;		AST(NAME(Silver) SELF_ONLY)
	U32 iCopper;		AST(NAME(Copper) SELF_ONLY)
	U32 iDiamonds;		AST(NAME(Diamonds) SELF_ONLY)
	U32 iFoundryTips;	AST(NAME(FoundryTips) SELF_ONLY)
	U32 iRoughDiamonds;	AST(NAME(RoughDiamonds) SELF_ONLY)
	U32 iArdent;		AST(NAME(Ardent) SELF_ONLY)
	U32 iCelestial;		AST(NAME(Celestial) SELF_ONLY)
	U32 iGlory;			AST(NAME(Glory) SELF_ONLY)

	U32 iDiamondsConvertLeft; AST(NAME(DiamondsConvertLeft) SELF_ONLY)
	U32 iDiamondsConverted; AST(NAME(DiamondsConverted) SELF_ONLY)
}MappedCurrencies;

AUTO_STRUCT;
typedef struct MappedItemAssignment
{
	REF_TO(ItemAssignmentDef) hDef; AST(REFDICT(ItemAssignmentDef))
	// The ItemAssignmentDef

	const char* pchDisplayName; AST(UNOWNED)
	// Display name of this assignment

	const char* pchIcon; AST(POOL_STRING)
	// The icon name for this assignment

	ItemAssignmentCategory eCategory;
	// The category of this assignment

	const char* pchCategoryIcon; AST(POOL_STRING)

	U32 uAssignmentID;
	// Unique assignment ID

	U32 uTimeStarted;			AST(FORMATSTRING(JSON_SECS_TO_RFC822=1))
	// The time the assignment started

	U32 uFinishDate;			AST(FORMATSTRING(JSON_SECS_TO_RFC822=1))
	// When the assignment will finish naturally

	S32 sFinishEarlyCost;		AST(DEFAULT(-1))

	S32 iSlotIndex;				AST(DEFAULT(-1))
	// for strict assignment slots

	U32 bIsLockedSlot : 1;

	U32 bHasCompleteDetails : 1;
	// Whether or not the complete details is available for this assignment

	U32 bIsAbortable : 1;
	// Whether or not this assignment is abortable

	InvRewardRequest *pRewards;
}MappedItemAssignment;

AUTO_STRUCT;
typedef struct MappedItemAssignments
{
	MappedItemAssignment **ppAssignments;
	int iComplete;
	int iActive;
}MappedItemAssignments;

S32 *s_eaAttribMappings = NULL;

////////////////////////////////////////////////////////////////////////////////////
// Mapped entity that will be returned to the gateway server, and translated into JSON
//

AUTO_STRUCT;
typedef struct MappedEntity
{
	ContainerID id;

	bool bHidden;				AST(NAME(Hidden)) // REQUIRED
	bool bViewerIsOwner;		AST(NAME(ViewerIsOwner)) // REQUIRED
	bool bNeedsFixup;			AST(NAME(NeedsFixup))

	char *estrName;				AST(ESTRING NAME(Name))
	char *estrTitle;			AST(NAME(Title) ESTRING)
	char *estrClassName;		AST(ESTRING NAME(ClassName))
	char *estrClassIcon;		AST(ESTRING NAME(ClassIcon))
	char *estrLastMapName;		AST(ESTRING NAME(LastMapName))
	char *estrGenderName;		AST(ESTRING NAME(GenderName))
	const char *pcClassType;	AST(POOL_STRING NAME(ClassType))		// the internal class name
	char *estrPublicAccountName;  AST(ESTRING NAME(PublicAccountName))  // REQUIRED
	char *estrSpeciesName;		AST(ESTRING NAME(SpeciesName))
	char *estrHistory;			AST(ESTRING NAME(History))
	char *estrGuildName;		AST(ESTRING NAME(GuildName))


	char *estrLastPlayed;		AST(ESTRING NAME(LastPlayed))
		// Must be an RFC822 Date

	char *estrDescription;		AST(ESTRING NAME(Description))

	S32 iLevel;					AST(NAME(Level))
	S32 iLevelUi;				AST(NAME(LevelUi))
	S32 iMaxHealth;				AST(NAME(MaxHealth))

	EARRAY_OF(OwnedContainer) ppOwned;  AST(NAME(OwnedContainers)) // Needs to exist, but NW will keep it empty

	MappedInventory *pInventory;		AST(NAME(Inventory))
	MappedAttribs *pAttribs;			AST(NAME(Attribs))
	const char *pchAttribPrimary;		AST(POOL_STRING)
	const char *pchAttribSecondary;		AST(POOL_STRING)
	const char *pchAttribTertiary;		AST(POOL_STRING)
	MappedCurrencies *pCurrencies;		AST(NAME(Currencies) SELF_ONLY)

	MappedItemAssignments *pItemAssignments; AST(NAME(ItemAssignments) SELF_ONLY)
	ItemAssignmentCategoryUIList *pItemAssignmentCategories; AST(NAME(ItemAssignmentCategories) SELF_ONLY)

	const char *pchAuctionSettings;		AST(ESTRING)
} MappedEntity;


MappedEntity *CreateNNOMappedEntity(GatewaySession *psess, Entity* psrc);

/////////////////////////////////////////////////////////////////////////////

//
// GetLastMapName
//
// Does the dance necessary to get the most recently visited map's name.
//
static const char *GetLastMapName(Language lang, Entity *psrc)
{
	if(psrc->pSaved)
	{
		ZoneMapInfo *pzmi = NULL;
		Message *pmsg;

		if(psrc->pSaved->lastStaticMap)
		{
			pzmi = zmapInfoGetByPublicName(psrc->pSaved->lastStaticMap->mapDescription);
		}
		else if(psrc->pSaved->lastNonStaticMap)
		{
			pzmi = zmapInfoGetByPublicName(psrc->pSaved->lastNonStaticMap->mapDescription);
		}

		if(pzmi)
		{
			pmsg = zmapInfoGetDisplayNameMessagePtr(pzmi);
			if(pmsg)
			{
				return langTranslateMessage(lang, pmsg);
			}
		}
	}

	return NULL;
}

void NNOSetClassNameType(Entity *pEntity, const char ** pcClassTypeName)
{
	if (pEntity && pEntity->pChar)
	{
		CharacterClass *pClass = GET_REF(pEntity->pChar->hClass);
		if(pClass && pClass->pchName)
		{
			*pcClassTypeName = allocAddString(pClass->pchName);
		}
	}

	if(!(*pcClassTypeName))
	{
		*pcClassTypeName = allocAddString("None");
	}
}

void NNOSetMaxHealth(GatewaySession *psess, Entity *psrc, MappedEntity *pMappedEnt)
{
	pMappedEnt->iMaxHealth = SAFE_MEMBER3(psrc, pChar, pattrBasic, fHitPointsMax);

	if(GetCharacterClassEnum(psrc) == StaticDefineIntGetInt(CharClassTypesEnum, "Space"))
	{
		// see if class has hit points (for ship)
		if(psrc->pChar)
		{
			CharacterClass *pClass = GET_REF(psrc->pChar->hClass);
			if(pClass && eaSize(&pClass->ppAttrBasic) > 1)
			{
				// ships have same health regardless of level
				pMappedEnt->iMaxHealth = pClass->ppAttrBasic[1]->fHitPointsMax;	
			}
		}
	}
}

MappedCurrencies *createNNOMappedCurrencies(Entity *pEnt)
{
	MappedCurrencies *pCurrencies = StructAlloc(parse_MappedCurrencies);
	GameAccountData *pData = entity_GetGameAccount(pEnt);

	S32 iResources = inv_GetNumericItemValue(pEnt, "Resources");

	pCurrencies->iCopper = iResources % 100;
	pCurrencies->iSilver = (iResources / 100) % 100;
	pCurrencies->iGold = (iResources / 100) / 100;
	
	pCurrencies->iDiamonds = inv_GetNumericItemValue(pEnt, "Astral_Diamonds");
	pCurrencies->iRoughDiamonds = inv_GetNumericItemValue(pEnt, "Astral_Diamonds_Rough");
	pCurrencies->iArdent = inv_GetNumericItemValue(pEnt, "Invocation_Ardent");
	pCurrencies->iCelestial = inv_GetNumericItemValue(pEnt, "Invocation_Celestial");
	pCurrencies->iGlory = inv_GetNumericItemValue(pEnt, "Pvp_Resources");

	pCurrencies->iFoundryTips = gad_GetAccountValueInt(pData, microtrans_GetShardFoundryTipBucketKey());

	pCurrencies->iDiamondsConverted = NumericConversion_QuantityConverted(pEnt,"Astral_Diamonds");
	pCurrencies->iDiamondsConvertLeft = NumericConversion_QuantityRemainingFromString(pEnt,"Astral_Diamonds");

	return pCurrencies;
}



ItemDef *GetPlayerBagItemDefForInventoryBag(Inventory *pInventory, InvBagIDs id)
{
	if(id < InvBagIDs_PlayerBag1 || id > InvBagIDs_PlayerBag9)
		return NULL;
	if(pInventory)
	{
		InventoryBag *pbag = eaIndexedGetUsingInt(&pInventory->ppInventoryBags, InvBagIDs_PlayerBags);

		if(pbag)
		{
			InventorySlot *pslot = eaGet(&pbag->ppIndexedInventorySlots, id - InvBagIDs_PlayerBag1);
			if(pslot && pslot->pItem)
			{
				return GET_REF(pslot->pItem->hItem);
			}
		}
	}

	return NULL;
}

bool checkSelfOnlyInventoryBag(InventoryBag *pBag)
{
	bool bEquipBag = (invbag_flags(pBag) & (InvBagFlag_EquipBag|InvBagFlag_WeaponBag|InvBagFlag_ActiveWeaponBag)) != 0;

	if(bEquipBag)
		return false;

	return true;
}

MappedInventory *createNNOMappedInventory(Language lang, Entity *pEnt, bool bViewerIsOwner)
{
	InvBagIDs s_eCraftingBagID = 0; 
	InvBagIDs s_eCurrencyBagID = 0; 
	ItemAssignmentPersistedData *pItemAssignmentData = SAFE_MEMBER2(pEnt, pPlayer, pItemAssignmentPersistedData);
	MappedInventory *pInventory;
	static TradeBagLite s_TradeBag = {0};
	static S32* s_TradeBagIDs = NULL;

	PERFINFO_AUTO_START_FUNC();

	if(!s_eCurrencyBagID)
	{
		s_eCraftingBagID = StaticDefineIntGetInt(InvBagIDsEnum, "CraftingInventory"); 
	}

	if(!s_TradeBagIDs)
	{
		ea32Push(&s_TradeBagIDs,StaticDefineIntGetInt(InvBagIDsEnum, "CraftingInventory"));
		ea32Push(&s_TradeBagIDs,StaticDefineIntGetInt(InvBagIDsEnum, "CraftingResources"));
		ea32Push(&s_TradeBagIDs,InvBagIDs_Inventory);
	}

	pInventory = StructAlloc(parse_MappedInventory);

	if(pEnt && pEnt->pInventoryV2)
	{
		int i;
		int j;

		// First loop through all the regular bags and extract the interesting information.
		for(i = 0; i < eaSize(&pEnt->pInventoryV2->ppInventoryBags); i++)
		{
			ItemDef *pdef;
			InventoryBag *psrcBag = pEnt->pInventoryV2->ppInventoryBags[i];
			MappedInvBag *pbag = NULL;
			int iMaxSlots = invbag_trh_maxslots(CONTAINER_NOCONST(Entity, (pEnt)), CONTAINER_NOCONST(InventoryBag, (psrcBag)));

			//Don't add these bags if you don't own the character
			if(!bViewerIsOwner && checkSelfOnlyInventoryBag(psrcBag))
			{
				continue;
			}

			if(psrcBag->BagID == InvBagIDs_Inventory
				|| (psrcBag->BagID >= InvBagIDs_PlayerBag1 && psrcBag->BagID <= InvBagIDs_PlayerBag9))
			{
				if(eaSize(&psrcBag->ppIndexedInventorySlots) == 0)
					continue;

				pbag = StructAlloc(parse_MappedInvBag);

				eaPush(&pInventory->ppPlayerBags, pbag);

				// Get its user-visible name, if it has one.
				pdef = GetPlayerBagItemDefForInventoryBag(pEnt->pInventoryV2, psrcBag->BagID);
				if(pdef)
				{
					estrCopy2(&pbag->estrName, langTranslateDisplayMessage(lang, pdef->displayNameMsg));
					estrCopy2(&pbag->estrIcon, pdef->pchIconName);
				}
				else if(psrcBag->BagID == InvBagIDs_Inventory)
				{
					estrCopy2(&pbag->estrName, langTranslateMessageKey(lang, "Inventory_MainBag_Name"));
					estrCopy2(&pbag->estrIcon, "Inventory_Misc_Bag1_Brown");
				}
			}
			else
			{
				pbag = StructAlloc(parse_MappedInvBag);
				eaPush(&pInventory->ppBags, pbag);
			}

			pbag->BagID = psrcBag->BagID;

			for(j = 0; j < eaSize(&psrcBag->ppIndexedInventorySlots); j++)
			{
				Item *psrcItem = psrcBag->ppIndexedInventorySlots[j]->pItem;
				
				if(!psrcItem && iMaxSlots > 0)
				{
					// Fill out the user bags with empty slots.			
					MappedInvSlot *pslot = createMappedInvSlotFromItem(pEnt, lang, NULL);
					eaPush(&pbag->ppSlots, pslot);
				}
				
				if(psrcItem)
				{
					MappedInvSlot *pslot = createMappedInvSlotFromItem(pEnt,lang,psrcItem);

					eaPush(&pbag->ppSlots, pslot);

					if(pItemAssignmentData && pbag->BagID == s_eCraftingBagID)
					{
						bool bOnAssignment = false;
						int iAssign = 0;
						MappedInvSlot ***peaSlots = NULL;
						for (iAssign = eaSize(&pItemAssignmentData->eaActiveAssignments) - 1; iAssign >= 0; --iAssign)
						{
							ItemAssignment *pAssignment = pItemAssignmentData->eaActiveAssignments[iAssign];
							S32 iItem;
							for (iItem = eaSize(&pAssignment->eaSlottedItems) - 1; iItem >= 0; --iItem)
							{
								if (pAssignment->eaSlottedItems[iItem]->uItemID == psrcItem->id)
								{
									bOnAssignment = true;
									break;
								}
							}
							if (bOnAssignment)
								break;
						}

						if(bOnAssignment)
						{
							peaSlots = &pInventory->ppAssignedSlots;
						}
						else
						{
							peaSlots = &pInventory->ppNotAssignedSlots;
						}

						for(iAssign=0;iAssign<eaSize(peaSlots);iAssign++)
						{
							if(GET_REF((*peaSlots)[iAssign]->hItemDef) == GET_REF(pslot->hItemDef))
							{
								(*peaSlots)[iAssign]->count += pslot->count;
								break;
							}
						}

						if(iAssign==eaSize(peaSlots))
						{
							eaPush(peaSlots,StructClone(parse_MappedInvSlot,pslot));
						}
					}
				}
			}

			// Fill out the user bags with empty slots.	
			while(eaSize(&pbag->ppSlots) < iMaxSlots)
			{
				MappedInvSlot *pslot = createMappedInvSlotFromItem(pEnt, lang, NULL);
				eaPush(&pbag->ppSlots, pslot);
			}
		}

		Trade_GetTradeableItems(&s_TradeBag.ppTradeSlots,pEnt,&s_TradeBagIDs,false,false);


		for(i=0;i<eaSize(&s_TradeBag.ppTradeSlots);i++)
		{
			MappedInvSlot *pslot = createMappedInvSlotFromItem(pEnt,lang,s_TradeBag.ppTradeSlots[i]->pItem);
			eaPush(&pInventory->ppTradeItems,pslot);
		}

		eaClearStruct(&s_TradeBag.ppTradeSlots,parse_TradeSlotLite);
	}

	PERFINFO_AUTO_STOP();
	return pInventory;
}

F32 GetAttribValue(Entity *pEntity,S32 eAttrib)
{
	F32 r = 0;
	if (pEntity && pEntity->pChar && pEntity->pChar->pattrBasic)
	{
		if(eAttrib >= 0 && IS_NORMAL_ATTRIB(eAttrib))
		{
			r = *F32PTR_OF_ATTRIB(pEntity->pChar->pattrBasic,eAttrib);
		}
	}
	return r;
}

MappedAttribs *createNNOMappedAttribs(Entity *psrc)
{
	MappedAttribs *pAttribs;
	int i = 0;
	int iCount = 0;

	PERFINFO_AUTO_START_FUNC();

	pAttribs = StructCreate(parse_MappedAttribs);

	while(TOK_GET_TYPE(parse_MappedAttribs[i].type) != TOK_END)
	{
		if(TOK_GET_TYPE(parse_MappedAttribs[i].type) == TOK_F32_X)
		{
			//ea32Push(&s_eaAttribMappings,StaticDefineIntGetInt(AttribTypeEnum,parse_MappedAttribs[i].name));
			F32 *fData = (F32*)(((char*)pAttribs) + parse_MappedAttribs[i].storeoffset);

			*fData = round(GetAttribValue(psrc, s_eaAttribMappings[iCount]));
			iCount++;
		}
		i++;
	}

	PERFINFO_AUTO_STOP();
	return pAttribs;
}

ItemAssignmentCategoryUIList *createNNOMappedItemAssignmentCategories(Language lang, Entity *psrc)
{
	ItemAssignmentCategoryUIList *pList;
	int i;

	PERFINFO_AUTO_START_FUNC();

	pList = StructCreate(parse_ItemAssignmentCategoryUIList);
	ItemAssignment_FillAssignmentCategories(lang,psrc,&pList->ppCategories,lItemAssignmentCategoryUIFlags_RankHeaders);

	for(i=0;i<eaSize(&pList->ppCategories);i++)
	{
		pList->ppCategories[i]->fPercentageThroughCurrentLevel *= 100;
	}

	PERFINFO_AUTO_STOP();
	return pList;
}

MappedItemAssignment *createNNOMappedItemAssignment(Entity *psrc, ItemAssignmentUI *pUIAssignment)
{
	MappedItemAssignment *pEntry = StructCreate(parse_MappedItemAssignment);
	ItemAssignmentOutcome* pOutcome = NULL;
	ItemAssignmentDef *pDef = NULL;

	pDef = GET_REF(pUIAssignment->hDef);

	COPY_HANDLE(pEntry->hDef,pUIAssignment->hDef);
	pEntry->pchDisplayName = pUIAssignment->pchDisplayName;
	pEntry->pchIcon = pUIAssignment->pchIcon;
	pEntry->eCategory = pUIAssignment->eCategory;
	pEntry->pchCategoryIcon = pUIAssignment->pchCategoryIcon;
	pEntry->uAssignmentID = pUIAssignment->uAssignmentID;
	pEntry->uTimeStarted = pUIAssignment->uTimeStarted;
	pEntry->iSlotIndex = pUIAssignment->iSlotIndex;
	pEntry->bIsLockedSlot = pUIAssignment->bIsLockedSlot;
	pEntry->bHasCompleteDetails = pUIAssignment->bHasCompleteDetails;
	pEntry->bIsAbortable = pUIAssignment->bIsAbortable;
	pEntry->uFinishDate = pUIAssignment->uTimeStarted + pUIAssignment->uDuration;

	pOutcome = ItemAssignment_GetOutcomeByName(pDef, pUIAssignment->pchOutcomeName);

	if (pOutcome)
	{
		int i;

		ItemAssignmentOutcomeRewardRequest* pRequest = StructCreate(parse_ItemAssignmentOutcomeRewardRequest);
		gslItemAssignments_FillOutcomeRewardRequest(psrc, NULL, NULL, pDef, pOutcome, pRequest);

		for(i=eaSize(&pRequest->pData->eaNumericRewards)-1;i>=0;i--)
		{
			ItemDef *pItemDef = GET_REF(pRequest->pData->eaNumericRewards[i]->hDef);

			if(!CraftingMappingItemDefCheck(pItemDef))
			{
				StructDestroy(parse_ItemNumericData,pRequest->pData->eaNumericRewards[i]);
				eaRemove(&pRequest->pData->eaNumericRewards,i);
			}
		}

		for(i=eaSize(&pRequest->pData->eaItemRewards)-1;i>=0;i--)
		{
			ItemDef *pItemDef = GET_REF(pRequest->pData->eaItemRewards[i]->hItem);

			if(!CraftingMappingItemDefCheck(pItemDef))
			{
				StructDestroy(parse_Item,pRequest->pData->eaItemRewards[i]);
				eaRemove(&pRequest->pData->eaItemRewards,i);
			}
		}

		pEntry->pRewards = StructClone(parse_InvRewardRequest,pRequest->pData);
		StructDestroy(parse_ItemAssignmentOutcomeRewardRequest, pRequest);
	}

	return pEntry;
}

MappedItemAssignments *createNNOMappedItemAssignmentList(Language lang, Entity *psrc)
{
	static ItemAssignmentList *pList = NULL;
	MappedItemAssignments *pReturn;
	int i;

	PERFINFO_AUTO_START_FUNC();

	pReturn = StructCreate(parse_MappedItemAssignments);

	if(!pList)
		pList = StructCreate(parse_ItemAssignmentList);
	else
		eaClearStruct(&pList->ppAssignments,parse_ItemAssignmentUI);

	ItemAssignment_FillItemAssignmentSlots(lang, psrc,&pList->ppAssignments);

	for(i=0;i<eaSize(&pList->ppAssignments);i++)
	{
		MappedItemAssignment *pAssignment;
		ItemAssignmentUI *pUIAssignment = pList->ppAssignments[i];
		ItemAssignment *pActiveAssignment = NULL;

		pAssignment = createNNOMappedItemAssignment(psrc, pUIAssignment);

		pActiveAssignment = ItemAssignment_EntityGetActiveAssignmentByID(psrc,pUIAssignment->uAssignmentID);

		if(pActiveAssignment)
		{
			ItemAssignments_GetForceCompleteNumericCost(psrc,pActiveAssignment,&pAssignment->sFinishEarlyCost);
			pReturn->iActive++;
			if(pUIAssignment->pchOutcomeName)
				pReturn->iComplete++;
		}

		eaPush(&pReturn->ppAssignments,pAssignment);
	}
	

	PERFINFO_AUTO_STOP();
	return pReturn;
}

// Copied and re-purposed to not use context alloc from ExpressionFunc.C nextStringInList
// Return the next string in the given list with cycling, e.g.
//    NextStringInList("A", "A B C") -> "B"
//    NextStringInList("C", "A B C") -> "A"
const char *nextStringInList(const char *pchString, const char *pchList)
{
	size_t szLength = strlen(pchList) + 1;
	char *pchListCopy = alloca(szLength);
	char *pchContext;
	char *pchStart;
	bool bNext = false;
	strcpy_s(pchListCopy, szLength, pchList);
	pchStart = strtok_r(pchListCopy, " ,\t\r\n", &pchContext);
	pchListCopy = pchStart;
	do 
	{
		if (bNext)
			return allocFindString(pchStart);
		if (!stricmp(pchStart, pchString))
			bNext = true;
	} while (pchStart = strtok_r(NULL, " ,\t\r\n", &pchContext));
	return allocFindString( pchListCopy);
}

/////////////////////////////////////////////////////////////////////////////

//
// CreateNNOMappedEntity
//
// Given a source Entity, allocate and fill in a new MappedEntity that
// summarizes and simplifies the original Entity for the web client.
//
MappedEntity *CreateNNOMappedEntity(GatewaySession *psess, Entity *psrc)
{
	MappedEntity *pdest;
	Language lang = psess->lang;
	S32 *eaiPowerCats = NULL;
	S32 *eaiPowerCatsEx = NULL;
	Entity *pOfflineCopy = NULL;
	CharacterClass *pClass = NULL;
	Entity *pPlayerEnt = NULL;
	bool bViewerIsOwner = (psess->idAccount == SAFE_MEMBER2(psrc, pPlayer, accountID));
	
	if(!psrc)
		return NULL;

	PERFINFO_AUTO_START_FUNC();

	pdest = StructAlloc(parse_MappedEntity);

	pdest->id = psrc->myContainerID;
	pdest->bViewerIsOwner = bViewerIsOwner;

	langFormatGameString(lang, &pdest->estrName, "{Entity.Name}", STRFMT_ENTITY(psrc), STRFMT_END);
	langFormatGameString(lang, &pdest->estrTitle, "{Entity.Title}", STRFMT_ENTITY(psrc), STRFMT_END);

	if(psrc->myEntityType == GLOBALTYPE_ENTITYPLAYER)
	{
		pPlayerEnt = psrc;
		if(psrc->pPlayer && psrc->pPlayer->publicAccountName)
		{
			estrPrintf(&pdest->estrPublicAccountName, "%s", psrc->pPlayer->publicAccountName);
		}
	}
	else if(psrc && psrc->pSaved)
	{
		ContainerID id = psrc->pSaved->conOwner.containerID;
		ContainerTracker *ptracker = session_FindDBContainerTracker(psess, GLOBALTYPE_ENTITYPLAYER, id);
		if(ptracker)
		{
			pPlayerEnt = GET_REF(ptracker->hEntity);
		}
	}

	pdest->bHidden = !bViewerIsOwner && SAFE_MEMBER3(pPlayerEnt, pPlayer, pGatewayInfo, bHidden);
	pdest->bNeedsFixup = pPlayerEnt->pSaved->uGameSpecificFixupVersion != (U32)gameSpecificFixup_Version();

	if(pdest->bHidden || pdest->bNeedsFixup)
	{
		// If the entity is hidden, all you get is the name.
		PERFINFO_AUTO_STOP();
		return pdest;
	}

	langFormatGameString(lang, &pdest->estrClassName, "{Entity.Class}", STRFMT_ENTITY(psrc), STRFMT_END);
	langFormatGameString(lang, &pdest->estrGenderName, "{Entity.SpeciesGender}", STRFMT_ENTITY(psrc), STRFMT_END);
	langFormatGameString(lang, &pdest->estrSpeciesName, "{Entity.Species}", STRFMT_ENTITY(psrc), STRFMT_END);
	
	estrCreate(&pdest->estrClassIcon);
	pClass = GET_REF(psrc->pChar->hClass);
	estrPrintf(&pdest->estrClassIcon,"Icon_Build_%s",pClass->pchName);

	estrCreate(&pdest->estrHistory);
	estrCopy2(&pdest->estrHistory,psrc->pSaved->savedDescription);

	// If there is a guild ID this ent is in a guild. 
	// For some reason the normal guild_IsMember check doesn't work here
	if (SAFE_MEMBER2(psrc, pPlayer, iGuildID))
	{
		estrCopy2(&pdest->estrGuildName, psrc->pPlayer->pcGuildName);
	}

	estrCopy2(&pdest->estrLastMapName, GetLastMapName(lang, psrc));

	pdest->iLevelUi = entity_GetSavedExpLevel(psrc);

	NNOSetClassNameType(psrc, &pdest->pcClassType);

	NNOSetMaxHealth(psess, psrc, pdest);

	if(psrc->pPlayer)
	{
		estrCopy2(&pdest->estrLastPlayed, timeGetRFC822StringFromSecondsSince2000(psrc->pPlayer->iLastPlayedTime));
	}

	pdest->pInventory = createNNOMappedInventory(lang,psrc,bViewerIsOwner);
	
	// Primary Attrib lookup
	{
		char *estrLookup = NULL;
		Message *pMessage = RefSystem_ReferentFromString(gMessageDict,"CharacterCreation_StatPriorityPerClass");
		
		estrCreate(&estrLookup);
		estrPrintf(&estrLookup,"%s%d",pClass->pchName,1);
		pdest->pchAttribPrimary = nextStringInList(estrLookup,pMessage->pcDefaultString);

		estrPrintf(&estrLookup,"%s%d",pClass->pchName,2);
		pdest->pchAttribSecondary = nextStringInList(estrLookup,pMessage->pcDefaultString);

		estrPrintf(&estrLookup,"%s%d",pClass->pchName,3);
		pdest->pchAttribTertiary = nextStringInList(estrLookup,pMessage->pcDefaultString);

		estrDestroy(&estrLookup);
	}

	pdest->pAttribs = createNNOMappedAttribs(psrc);
	pdest->pCurrencies = createNNOMappedCurrencies(psrc);
	pdest->pItemAssignments = createNNOMappedItemAssignmentList(lang, psrc);
	pdest->pItemAssignmentCategories = createNNOMappedItemAssignmentCategories(lang, psrc);

	pdest->pchAuctionSettings = estrCreateFromStr("@AuctionSettings[Main]");

	if(bViewerIsOwner)
	{
		if(!SAFE_MEMBER2(psrc,pPlayer,pItemAssignmentPersistedData))
			gslRequestItemAssignments(psrc);
	}

	PERFINFO_AUTO_STOP();
	return pdest;
}

/////////////////////////////////////////////////////////////////////////////

void MappedEntityInit(void)
{
	ParseTable *ptable = parse_MappedAttribs;
	int i = 0;

	PERFINFO_AUTO_START_FUNC();

	while(TOK_GET_TYPE(parse_MappedAttribs[i].type) != TOK_END)
	{
		if(TOK_GET_TYPE(parse_MappedAttribs[i].type) == TOK_F32_X)
		{
			ea32Push(&s_eaAttribMappings,StaticDefineIntGetInt(AttribTypeEnum,parse_MappedAttribs[i].name));
		}
		i++;	
	}

	PERFINFO_AUTO_STOP();
}

MappedEntity *CreateMappedEntity(GatewaySession *psess, ContainerTracker *ptracker, MappedEntity *pent)
{
	PERFINFO_AUTO_START_FUNC();

	if(!pent)
	{
		Entity *pOfflineCopy = NULL;
		if(ptracker->pOfflineCopy)
		{
			cmap_DestroyOfflineEntity(ptracker->pOfflineCopy);
			ptracker->pOfflineCopy = NULL;
		}
		
		pOfflineCopy = session_GetEntityOfflineCopy(psess, ptracker);
		pent = CreateNNOMappedEntity(psess, pOfflineCopy);
	}

	PERFINFO_AUTO_STOP();
	return pent;
}

void DestroyMappedEntity(GatewaySession *psess, ContainerTracker *ptracker, MappedEntity *pent)
{
	PERFINFO_AUTO_START_FUNC();

	if(pent)
	{
		StructDestroy(parse_MappedEntity, pent);
	}

	if(ptracker->pOfflineCopy)
	{
		cmap_DestroyOfflineEntity(ptracker->pOfflineCopy);
		ptracker->pOfflineCopy = NULL;
	}

	PERFINFO_AUTO_STOP();
}

#include "NNOGatewayMappedEntity_c_ast.c"

// End of File
