/***************************************************************************



***************************************************************************/
#include "Entity.h"
#include "message.h"
#include "ItemCommon.h"
#include "ItemEnums.h"
#include "ExpressionMinimal.h"
#include "Character.h"
#include "BlockEarray.h"
#include "expressionprivate.h"
#include "gamestringformat.h"
#include "combateval.h"
#include "GameAccountDataCommon.h"
#include "mission_common.h"
#include "PowersAutoDesc.h"
#include "ActivityCommon.h"
#include "entCritter.h"
#include "SuperCritterPet.h"
#include "item/itemDescCommon.h"
#include "item/itemProgressionCommon.h"

#include "NNOItemDescription.h"

#include "Autogen/itemCommon_h_ast.h"
#include "Autogen/itemEnums_h_ast.h"
#include "Autogen/PowersAutoDesc_h_ast.h"

#include "NNOItemDescription_h_ast.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_GameSystems););


extern bool g_bNNOOverrideUseOldAutodec;

static void _formatSecondsAsHMS(Language lang, char** pestrResult,
						   S32 iSeconds, const char* pchFormatMessageKey)
{
	S32 pHMS[3] = {0, 0, 0};
	// This function was lifted from gclUIGen.c (gclFormatSecondsAsHMS).
	// It used to have functionality for iShortenOutputOverSeconds. We don't need that here, quite.
	// 
	timeSecondsGetHoursMinutesSeconds(iSeconds, pHMS, false);
	langFormatGameMessageKey(lang, pestrResult, pchFormatMessageKey,
		STRFMT_INT("TimeHours", pHMS[0]),
		STRFMT_INT("TimeMinutes", pHMS[1]),
		STRFMT_INT("TimeSeconds", pHMS[2]),
		STRFMT_END);
}

static void _getItemGameLifetimeLeft(Language lang, char **estrVal,SA_PARAM_OP_VALID Item *pItem)
{
	if (pItem)
	{
		FOR_EACH_IN_EARRAY_FORWARDS(pItem->ppPowers, Power, pPower)
		{
			PowerDef *pdef = NULL;
			if(pPower->pParentPower) pPower = pPower->pParentPower;
			pdef = GET_REF(pPower->hDef);
			if (pdef)
			{
				if (pdef->fLifetimeGame)
				{
					S32 iTimeLeft = (S32)(power_GetLifetimeGameLeft(pPower));
					if (iTimeLeft > 120)
					{
						_formatSecondsAsHMS(lang, estrVal, iTimeLeft, "AutoDesc.PowerDef.FormatMinutes");
					}
					else
					{
						_formatSecondsAsHMS(lang, estrVal, iTimeLeft, "AutoDesc.PowerDef.FormatSeconds");
					}
					return;
				}
			}
		}
		FOR_EACH_END
	}
}

static void _getItemRealLifetimeLeft(Language lang, char **estrVal,SA_PARAM_OP_VALID Item *pItem)
{
	if (pItem)
	{
		FOR_EACH_IN_EARRAY_FORWARDS(pItem->ppPowers, Power, pPower)
		{
			PowerDef *pdef = NULL;
			if(pPower->pParentPower) pPower = pPower->pParentPower;
			pdef = GET_REF(pPower->hDef);
			if (pdef)
			{
				if (pdef->fLifetimeReal)
				{
					S32 iTimeLeft = (S32)(power_GetLifetimeRealLeft(pPower));
					if (iTimeLeft > 120)
					{
						_formatSecondsAsHMS(lang, estrVal, iTimeLeft, "AutoDesc.PowerDef.FormatMinutes");
					}
					else
					{
						_formatSecondsAsHMS(lang, estrVal, iTimeLeft, "AutoDesc.PowerDef.FormatSeconds");
					}
					return;
				}
			}
		}
		FOR_EACH_END
	}
}




bool GetNNOItemInfoComparedStructNoStrings(	Language lang,
	NNOItemInfo *pinfo,
	SA_PARAM_OP_VALID Item *pItem,
	SA_PARAM_OP_VALID Item *pItemOther, 
	SA_PARAM_OP_VALID Entity *pEnt, 
	bool bGetItemAttribDiffs,
	S32 eActiveGemSlotType)
{
	ItemDef *pItemDef;
	UsageRestriction *pRestrict;
	int iEntLevel;

	pinfo->pItem = pItem;
	pinfo->pItemDef = pItemDef = pItem ? GET_REF(pItem->hItem) : NULL;
	if(!pinfo || !pEnt || !pItem || !pItemDef)
	{
		devassertmsg(pEnt,"GetNNOItemInfoComparedStructNoStrings called with NULL entity.  Stop doing this.");

		return false;
	}

	// this is to make our hack in PowersAutoDesc.c more reliable (for item set powers)
	g_bNNOOverrideUseOldAutodec = true;

	pinfo->bEntUsableExpr = true;
	pinfo->bEntHasClass = true;
	pinfo->bEntHasPath = true;

	combateval_ContextSetupSimple(pEnt->pChar,item_GetLevel(pItem) ? item_GetLevel(pItem) : pEnt->pChar ? pEnt->pChar->iLevelCombat : 1,pItem);

	pinfo->bBindsOnPickup = !!(pItemDef->flags & (kItemDefFlag_BindOnPickup | kItemDefFlag_BindToAccountOnPickup) || pItem->bForceBind);
	pinfo->bBindsOnEquip = !pinfo->bBindsOnPickup && !!(pItemDef->flags & (kItemDefFlag_BindOnEquip | kItemDefFlag_BindToAccountOnEquip));
	pinfo->bBindsToAccount = !!(pItemDef->flags & (kItemDefFlag_BindToAccountOnEquip | kItemDefFlag_BindToAccountOnPickup));
	pinfo->bCantDiscard = !item_CanDiscard(pEnt,pItemDef,false);
	pinfo->bIsBound = !!(pItem->flags & (kItemFlag_Bound | kItemFlag_BoundToAccount));
	pinfo->iItemPowerFactor = item_GetPowerFactor(pItem);

	pinfo->iMinLevel = item_GetMinLevel(pItem);
	pinfo->iMaxLevel = 0;
	if (pItemDef->pRestriction && pItemDef->pRestriction->iMaxLevel)
		pinfo->iMaxLevel = pItemDef->pRestriction->iMaxLevel;

	pinfo->fPowerRechargeTime = itemeval_GetItemMaxOfRechargeAndCooldown(pItem);
	pinfo->iItemCount = pItem->count;
	pinfo->iChargesLeft = itemeval_GetItemChargesAndCount(pItem);
	pinfo->iPowerMaxCharges = itemeval_GetItemChargesMax(pItem);

	pinfo->bItemIsRecommended = GetNNOIsItemRecommended(pEnt, pItem, pItemOther);


	if(pEnt && pEnt->myContainerID)
	{
		U32 contID = pItem->id >> 32;

		if(pEnt->myContainerID == contID)
		{
			pinfo->bIsOwnedByPlayer = true;
		}
	}

	iEntLevel = entity_GetSavedExpLevel(pEnt);
	pinfo->bEntMeetsLevelRequirements = (iEntLevel >= pinfo->iMinLevel) && (pinfo->iMaxLevel == 0 || iEntLevel <= pinfo->iMaxLevel);

	if (pItemDef->pItemDamageDef && !item_IsUnidentified(pItem))
	{
		pinfo->iMinDamage = ItemWeaponDamageFromItemDef(PARTITION_CLIENT, NULL, pItemDef, kCombatEvalMagnitudeCalculationMethod_Min);
		pinfo->iMaxDamage = ItemWeaponDamageFromItemDef(PARTITION_CLIENT, NULL, pItemDef, kCombatEvalMagnitudeCalculationMethod_Max);
	}

	pRestrict = pItemDef->pRestriction;
	if (pRestrict)
	{	
		MultiVal mvResult = {0};
		pinfo->bEntUsableExpr = (pRestrict->pRequires == NULL);
		if (!pinfo->bEntUsableExpr)
		{
			itemeval_Eval(entGetPartitionIdx(pEnt), pItemDef->pRestriction->pRequires, pItemDef, NULL, pItem, pEnt, item_GetLevel(pItem), item_GetQuality(pItem), 0, pItemDef->pchFileName, -1, &mvResult);
			pinfo->bEntUsableExpr = itemeval_GetIntResult(&mvResult,pItemDef->pchFileName,pItemDef->pRestriction->pRequires);
		}

		if (pRestrict->ppCharacterClassesAllowed && pEnt->pChar)
		{
			CharacterClass *pCharClass = GET_REF(pEnt->pChar->hClass);

			pinfo->bEntHasClass = false;
			FOR_EACH_IN_EARRAY(pRestrict->ppCharacterClassesAllowed, CharacterClassRef, pClassRef)
			{
				CharacterClass *pClass = GET_REF(pClassRef->hClass);
				if (pClass)
				{
					if (pCharClass == pClass)
					{
						pinfo->bEntHasClass = true;
						break;
					}
				}
			}
			FOR_EACH_END
		}

		if (pRestrict->ppCharacterPathsAllowed && pEnt->pChar)
		{
			pinfo->bEntHasPath = false;

			FOR_EACH_IN_EARRAY(pRestrict->ppCharacterPathsAllowed, CharacterPathRef, pPathRef)
			{
				CharacterPath *pPath = GET_REF(pPathRef->hPath);
				if (pPath)
				{
					if (entity_HasCharacterPath(pEnt, pPath->pchName))
					{
						pinfo->bEntHasPath = true;
						break;
					}
				}
			}
			FOR_EACH_END
		}
	}

	if (item_GetTransmutation(pItem))
	{
		pinfo->bIsTransmutated = true;
	}

	// Get the item categories text
	if (eaiSize(&pItemDef->peCategories) > 0)
	{
		if (pItemDef->eType == kItemType_Device)
		{
			int i;
			const char *pchCategoryName;

			for (i = 0; i < eaiSize(&pItemDef->peCategories); i++)
			{
				pchCategoryName = StaticDefineIntRevLookup(ItemCategoryEnum, pItemDef->peCategories[i]);
				if (stricmp(pchCategoryName,"mount")==0)
				{
					pinfo->bIsMount=true;
				}
			}
		}
	}

	pinfo->bIsGem = (pItemDef->eType == kItemType_Gem && pItemDef->eGemType != kItemGemType_None);
	pinfo->bIsItemEnchant = 
		(pItemDef->eGemType & StaticDefineInt_FastStringToInt(ItemGemTypeEnum, "ItemOffense", 0)) ||
		(pItemDef->eGemType & StaticDefineInt_FastStringToInt(ItemGemTypeEnum, "ItemDefense", 0)) ||
		(pItemDef->eGemType & StaticDefineInt_FastStringToInt(ItemGemTypeEnum, "ItemUtility", 0)) ||
		(pItemDef->eGemType & StaticDefineInt_FastStringToInt(ItemGemTypeEnum, "WeaponEnhancement", 0)) ||
		(pItemDef->eGemType & StaticDefineInt_FastStringToInt(ItemGemTypeEnum, "ArmorEnhancement", 0));
	pinfo->bIsPetEnchant = 
		(pItemDef->eGemType & StaticDefineInt_FastStringToInt(ItemGemTypeEnum, "Pet", 0)) ||
		(pItemDef->eGemType & StaticDefineInt_FastStringToInt(ItemGemTypeEnum, "PetOffense", 0)) ||
		(pItemDef->eGemType & StaticDefineInt_FastStringToInt(ItemGemTypeEnum, "PetDefense", 0));

	pinfo->iItemQualityIndex = pItem ? item_GetQuality(pItem) : pItemDef->Quality;
	pinfo->bItemIdentified = !item_IsUnidentified(pItem);
	pinfo->bItemHasBeenDyed = (pItem && pItem->pAlgoProps && pItem->pAlgoProps->pDyeData);

	//Item Progression info
	pinfo->uItemProgressionLevel = itemProgression_GetLevel(pItem);
	if (pinfo->uItemProgressionLevel > 0)
	{
		ItemProgressionTierDef* pTier = itemProgression_GetCurrentTier(pItem);
		pinfo->bItemProgressionReadyToEvo = itemProgression_ReadyToEvo(pItem);
		pinfo->bItemProgressionIsMaxLevel = itemProgression_IsMaxLevel(pItem);
		pinfo->uItemProgressionXP = ItemProgression_GetAdjustedItemXP(pItem);
		pinfo->uItemProgressionXPRequired = ItemProgression_GetAdjustedItemXPRequiredForNextLevel(pItem);
		pinfo->uItemProgressionTier = pTier->iIndex;
	}
	g_bNNOOverrideUseOldAutodec = false;
	return true;
}

bool GetNNOItemInfoComparedStructStrings(	Language lang,
	NNOItemInfo *pinfo,
	SA_PARAM_OP_VALID Item *pItem,
	SA_PARAM_OP_VALID Item *pItemOther, 
	SA_PARAM_OP_VALID Entity *pEnt, 
	bool bGetItemAttribDiffs,
	S32 eActiveGemSlotType)
{
	static REF_TO(ItemDef) s_hResources;
	ItemDef *pResourceDef;
	const char *pchItemDesc;
	ItemDef *pItemDef;
	MissionDef *pMission;
	GameAccountDataExtract *pExtract;
	UsageRestriction *pRestrict;

	
	//Setup a static handle to the resources item
	if(!IS_HANDLE_ACTIVE(s_hResources))
	{
		SET_HANDLE_FROM_STRING(g_hItemDict, "resources", s_hResources);
	}

	pItemDef = pinfo->pItemDef;

	if(!pinfo || !pEnt || !pItem || !pItemDef)
	{
		if(!pItemDef)
			Errorf("Make sure you call GetNNOItemInfoComparedSetup first.");

		return false;
	}

	g_bNNOOverrideUseOldAutodec = true;

	// Extract variables
	pinfo->pchItemQuality = StaticDefineIntRevLookup(ItemQualityEnum, pItem ? item_GetQuality(pItem) : pItemDef->Quality);
	if (!item_IsUnidentified(pItem))
	{
		const char *pchItemPrompt = item_GetItemPowerUsagePrompt(lang, pItem);
		if (pchItemPrompt)
		{
			// TODO: This looks like this is untranslated ("Use:")
			estrAppend2(&pinfo->estrItemUsagePrompt, "Use: ");
			estrAppend2(&pinfo->estrItemUsagePrompt, pchItemPrompt);
		}
	}

	if (pinfo->bIsTransmutated)
	{
		ItemDef *pTransmutatedToItemDef = item_GetTransmutation(pItem);
		const char *pchTransmutatedItemDefName = langTranslateDisplayMessage(lang, pTransmutatedToItemDef->displayNameMsg);

		if (pchTransmutatedItemDefName == NULL)
		{
			pchTransmutatedItemDefName = pTransmutatedToItemDef->pchName;
		}			

		estrCopy2(&pinfo->estrTransmutatedTo, pchTransmutatedItemDefName);			
	}

	// Get the item categories text
	if (eaiSize(&pItemDef->peCategories) > 0)
	{
		Item_GetItemCategoriesString(lang, pItemDef, NULL, &pinfo->estrItemCategories);
	}

	pRestrict = pItemDef->pRestriction;
	if (pRestrict)
	{	
		const char* pchSeparator = TranslateMessageKey("Item.UI.Separator");
		U32 uiLenSep = pchSeparator ? (U32)strlen(pchSeparator) : 0;

		if (pRestrict->ppCharacterClassesAllowed && pEnt->pChar)
		{
			CharacterClass *pCharClass = GET_REF(pEnt->pChar->hClass);
			
			FOR_EACH_IN_EARRAY(pRestrict->ppCharacterClassesAllowed, CharacterClassRef, pClassRef)
			{
				CharacterClass *pClass = GET_REF(pClassRef->hClass);
				if (pClass)
				{
					langFormatMessageKey(lang, &pinfo->estrCharClassRequires, "Item.UI.ReqClass", 
						STRFMT_DISPLAYMESSAGE("ClassName", pClass->msgDisplayName), 
						STRFMT_END);

					if(uiLenSep && FOR_EACH_IDX(-, pClassRef) != 0)
					{
						estrConcatString(&pinfo->estrCharClassRequires, pchSeparator, uiLenSep);
					}
				}
			}
			FOR_EACH_END
		}

		if (pRestrict->ppCharacterPathsAllowed && pEnt->pChar)
		{
			FOR_EACH_IN_EARRAY(pRestrict->ppCharacterPathsAllowed, CharacterPathRef, pPathRef)
			{
				CharacterPath *pPath = GET_REF(pPathRef->hPath);
				if (pPath)
				{
					langFormatMessageKey(lang, &pinfo->estrCharPathRequires, "Item.UI.ReqPath", 
						STRFMT_DISPLAYMESSAGE("PathName",pPath->pDisplayName), 
						STRFMT_END);

					if(uiLenSep && FOR_EACH_IDX(-, pPathRef) != 0)
					{
						estrConcatString(&pinfo->estrCharPathRequires, pchSeparator, uiLenSep);
					}
				}
			}
			FOR_EACH_END
		}

		//parse required power name from requires expression
		if (pItemDef->pRestriction->pRequires)
		{
			int ii,s=beaSize(&pItemDef->pRestriction->pRequires->postfixEArray);
			static MultiVal **s_ppStack = NULL;
			for(ii=0; ii<s; ii++)
			{
				MultiVal *pVal = pItemDef->pRestriction->pRequires->postfixEArray + ii;
				if(pVal->type==MULTIOP_PAREN_OPEN || pVal->type==MULTIOP_PAREN_CLOSE)
					continue;
				if(pVal->type==MULTIOP_FUNCTIONCALL)
				{
					const char *pchFunction = pVal->str;
					if(!strncmp(pchFunction,"Entownspower", 12))
					{
						MultiVal* pPowNameVal = eaPop(&s_ppStack);
						PowerDef* pDef = RefSystem_ReferentFromString(g_hPowerDefDict, MultiValGetString(pPowNameVal, NULL));
						pinfo->pchItemRequiresExprPower = pDef ? langTranslateDisplayMessage(lang, pDef->msgDisplayName) : "INVALID_POWER";
					}
				}
				eaPush(&s_ppStack,pVal);
			}
		}
	}

	// Event-based tooltip msg.
	if (pItemDef->pchEventTooltipEvent!=NULL && pItemDef->pchEventTooltipEvent[0]!=0 && Activity_EventIsActive(pItemDef->pchEventTooltipEvent))
	{
		const char* pchEventTooltip = langTranslateDisplayMessage(lang, pItemDef->eventTooltipMsg);
		if (pchEventTooltip)
		{
			estrAppend2(&pinfo->estrItemEventTooltip, pchEventTooltip);
		}
	}

	pinfo->pchItemName = item_GetNameLang(pItem, lang, pEnt);

	pchItemDesc = item_GetTranslatedDescription(pItem, lang);
	if (pchItemDesc)
		estrPrintf(&pinfo->estrItemFlavorDesc, "%s", pchItemDesc);

	if (!item_IsUnidentified(pItem))
	{
		estrStackCreate(&pinfo->estrItemInnatePowerAutoDesc);
		estrStackCreate(&pinfo->estrItemPowerAutoDesc);
		Item_InnatePowerAutoDesc(pEnt, lang, pItem, &pinfo->estrItemInnatePowerAutoDesc, eActiveGemSlotType);
		Item_PowerAutoDescCustom(pEnt, lang, pItem, &pinfo->estrItemPowerAutoDesc, "ItemPowerMessageKey", "ItemAttribModMessageKey", eActiveGemSlotType);
	}

	pMission = GET_REF(pItemDef->hMission);
	if (pMission)
	{
		pinfo->pchItemMission = langTranslateDisplayMessage(lang, pMission->displayNameMsg);
	}

	if (pItemDef->pItemDamageDef && !item_IsUnidentified(pItem))
	{
		// pinfo->iMinDamage is filled in GetNNOItemInfoComparedStructSetup()
		// pinfo->iMaxDamage is filled in GetNNOItemInfoComparedStructSetup()

		estrStackCreate(&pinfo->estrItemWeaponDamage);
		langFormatGameMessageKey(lang, &pinfo->estrItemWeaponDamage, "Item.UI.ItemDamage",
			STRFMT_INT("ItemMinDamage", pinfo->iMinDamage),
			STRFMT_INT("ItemMaxDamage", pinfo->iMaxDamage),
			STRFMT_END);

		// TODO: I don't think SMF should be embedded in here.
		estrConcatf(&pinfo->estrItemWeaponDamage,"%s","<br>");
	}
	else if (pItemDef->pItemWeaponDef && !item_IsUnidentified(pItem))
	{
		estrStackCreate(&pinfo->estrItemWeaponDamage);
		langFormatGameMessageKey(lang, &pinfo->estrItemWeaponDamage, "Item.UI.WeaponDamage",
			STRFMT_INT("ItemWeaponDamageDieSize", pItemDef->pItemWeaponDef->iDieSize),
			STRFMT_INT("ItemWeaponDamageNumDice", pItemDef->pItemWeaponDef->iNumDice),
			STRFMT_END);

		if (pItemDef->pItemWeaponDef->pAdditionalDamageExpr)
		{
			char * estrAdditionalDamage=NULL;
			int iAdditionalDamage = combateval_EvalNew(PARTITION_CLIENT,pItemDef->pItemWeaponDef->pAdditionalDamageExpr,kCombatEvalContext_Simple,NULL);
			estrStackCreate(&estrAdditionalDamage);
			langFormatGameMessageKey(lang, &estrAdditionalDamage, "Item.UI.WeaponDamageAdditional",
				STRFMT_INT("ItemWeaponDamageAdditional", iAdditionalDamage),
				STRFMT_END);

			if (estrAdditionalDamage)
			{
				// TODO: This should probably be exposed as a separate value
				//   and glued together in the FormatGameString call instead.
				estrAppend(&pinfo->estrItemWeaponDamage, &estrAdditionalDamage);
			}
			estrDestroy(&estrAdditionalDamage);
		}
		// TODO: I don't think SMF should be embedded in here.
		estrConcatf(&pinfo->estrItemWeaponDamage,"%s","<br>");
	}

	pResourceDef = GET_REF(s_hResources);

	if (pItemDef->flags & kItemDefFlag_CantSell)
	{
		langFormatGameMessageKey(lang, &pinfo->estrFormattedItemValue, "Item.UI.CantSell", STRFMT_END);
	}
	else if (pResourceDef) 
	{
		S32 iResourceValue = item_GetDefEPValue(entGetPartitionIdx(pEnt), pEnt, pResourceDef, pResourceDef->iLevel, pResourceDef->Quality);
		S32 iValue = item_GetStoreEPValue(entGetPartitionIdx(pEnt), pEnt, pItem, NULL);
		if (iResourceValue) {
			iValue /= iResourceValue;
		}
		estrStackCreate(&pinfo->estrItemValue);
		estrPrintf(&pinfo->estrItemValue, "%d %s", iValue, item_GetDefLocalName(pResourceDef, lang));
		item_GetFormattedResourceString(lang, &pinfo->estrFormattedItemValue, iValue, "Item.FormattedResources", 100, 0);
	}


	if (pItemOther && bGetItemAttribDiffs)
	{
		ItemDef *pItemOtherDef = GET_REF(pItemOther->hItem);
		if (pItemOtherDef)
		{
			if (pItemDef->pItemDamageDef && pItemOtherDef->pItemDamageDef)
			{
				S32 iItemAverageDamage = (S32)floorf(0.5f * (pinfo->iMinDamage + pinfo->iMaxDamage) + 0.5f);
				S32 iOtherItemAverageDamage;
				S32 iItemAverageDiff; 
				S32 iOtherItemLevel = item_GetLevel(pItemOther) ? item_GetLevel(pItemOther) : pEnt->pChar ? pEnt->pChar->iLevelCombat : 1;

				combateval_ContextSetupSimple(pEnt->pChar, iOtherItemLevel, pItemOther);

				iOtherItemAverageDamage = (S32)floorf(ItemWeaponDamageFromItemDef(PARTITION_CLIENT, NULL, pItemOtherDef, kCombatEvalMagnitudeCalculationMethod_Average) + 0.5f);
				iItemAverageDiff = iItemAverageDamage - iOtherItemAverageDamage;

				estrStackCreate(&pinfo->estrItemWeaponDamageDiff);
				langFormatGameMessageKey(lang, &pinfo->estrItemWeaponDamageDiff, "Item.UI.ItemDamageDiff",
					STRFMT_INT("ItemWeaponDamageDiff", iItemAverageDiff),
					STRFMT_END);

				// TODO: I don't think SMF should be embedded in here.
				estrConcatf(&pinfo->estrItemWeaponDamageDiff,"%s","<br>");
			}

			Item_GetInnatePowerDifferencesAutoDesc(pEnt, lang, pItemOther, pItem, &pinfo->estrItemDifferences);
		}
	}

	_getItemGameLifetimeLeft(lang, &pinfo->estrGameLifetimeLeft,pItem);
	_getItemRealLifetimeLeft(lang, &pinfo->estrRealLifetimeLeft,pItem);

	// Formatted gem slot portion
	GetNNOBuildGemSlotString(pItem, pinfo->pItemDef, &pinfo->estrGemSlots, lang, "Inventory_ItemInfo_GemSlot");

	// Formatted item sets
	pExtract = entity_GetCachedGameAccountDataExtract(pEnt);
	GetItemDescriptionItemSet(&pinfo->estrItemSetDesc, lang, pinfo->pItemDef, pItem, pEnt, "Inventory_ItemInfo_ItemSet_Power", "ItemPowerMessageKey", "Inventory_ItemInfo_ItemSetFormatShort", pExtract);

	pinfo->pchItemUsable = langTranslateMessageKey(lang, "Item.UI.Usable");
	if(!pinfo->pchItemUsable)
	{
		pinfo->pchItemUsable = "[UNTRANSLATED: Item.UI.Usable]";
	}
	pinfo->pchItemUsable = (!pinfo->bEntUsableExpr || !pinfo->bEntHasClass || !pinfo->bEntMeetsLevelRequirements) ? NULL_TO_EMPTY(pinfo->pchItemUsable) : "";

	// Undo the power set hack
	g_bNNOOverrideUseOldAutodec = false;
	return true;
}

bool GetNNOItemInfoComparedStruct(	Language lang,
									NNOItemInfo *pinfo,
									SA_PARAM_OP_VALID Item *pItem,
									SA_PARAM_OP_VALID Item *pItemOther, 
									SA_PARAM_OP_VALID Entity *pEnt, 
									bool bGetItemAttribDiffs,
									S32 eActiveGemSlotType)
{
	return GetNNOItemInfoComparedStructNoStrings(lang, pinfo, pItem, pItemOther, pEnt, bGetItemAttribDiffs, eActiveGemSlotType)
		&& GetNNOItemInfoComparedStructStrings(lang, pinfo, pItem, pItemOther, pEnt, bGetItemAttribDiffs, eActiveGemSlotType);
}

//helper function, gets an attribute of an ent.  Attrib should be the designer-facing enum value name.
static F32 getAttributeScore(Entity* pEnt, const char* attrib)
{
	return (F32)round(*F32PTR_OF_ATTRIB(pEnt->pChar->pattrBasic, StaticDefineInt_FastStringToInt(AttribTypeEnum, attrib, -1)));
}

// The context message is the part that says "Double click to <whatever>".  It probably doesn't need the strings at all.
// Mostly, it is just looking at the bools.  Unfortunately, since the results depend on CONTEXT, like "Am I in the store?",
// the bools we currently provide are often wholely inadequate.  
static void _formatContextMessage(Language lang, char ** pestrFormattedContextString, char const * pchContextKey,Item const * pItem,NNOItemInfo const * pinfo)
{
	// Possibly, this whole block should be removed, and context should be handled in C, not the gens or the message key
	langFormatGameMessageKey(lang, pestrFormattedContextString, pchContextKey,
		STRFMT_ITEM(pItem),
		STRFMT_ITEMDEF(pinfo->pItemDef),

		// pinfo->iMinDamage unused
		// pinfo->iMaxDamage unused
		STRFMT_INT("ItemMinLevel",			pinfo->iMinLevel),
		STRFMT_INT("ItemMaxLevel",			pinfo->iMaxLevel),
		STRFMT_INT("ItemQualityIndex",		pinfo->iItemQualityIndex),
		STRFMT_INT("ItemPowerFactor",		pinfo->iItemPowerFactor),
		STRFMT_INT("IsUsableExpr",			pinfo->bEntUsableExpr),
		STRFMT_INT("IsRecommended",			pinfo->bItemIsRecommended),
		STRFMT_INT("HasClass",				pinfo->bEntHasClass),
		STRFMT_INT("HasPath",				pinfo->bEntHasPath),
		STRFMT_INT("IsOwnedByPlayer",		pinfo->bIsOwnedByPlayer),
		STRFMT_INT("IsMount",				pinfo->bIsMount),
		STRFMT_INT("MeetsLevel",			pinfo->bEntMeetsLevelRequirements),
		STRFMT_INT("IsBindOnEquip",			pinfo->bBindsOnEquip),
		STRFMT_INT("IsBindOnPickup",		pinfo->bBindsOnPickup),
		STRFMT_INT("IsBindToAccount",		pinfo->bBindsToAccount),
		STRFMT_INT("IsAlreadyBound",		pinfo->bIsBound),
		STRFMT_INT("CannotDiscard",			pinfo->bCantDiscard),
		STRFMT_INT("IsGem",					pinfo->bIsGem),
		STRFMT_INT("IsItemEnchant",			pinfo->bIsItemEnchant),
		STRFMT_INT("IsPetEnchant",			pinfo->bIsPetEnchant),
		STRFMT_INT("ItemIdentified",		pinfo->bItemIdentified),
		STRFMT_INT("ItemHasBeenDyed",		pinfo->bItemHasBeenDyed),
		STRFMT_INT("ItemIsTransmutated",	pinfo->bIsTransmutated),
		STRFMT_INT("ItemCount",				pinfo->iItemCount),
		STRFMT_INT("ChargesLeft",			pinfo->iChargesLeft),
		STRFMT_INT("PowerChargesMax",		pinfo->iPowerMaxCharges),
		STRFMT_FLOAT("PowerRechargeTime",	pinfo->fPowerRechargeTime),

		STRFMT_STRING("ItemQuality",			NULL_TO_EMPTY(pinfo->pchItemQuality)),
		STRFMT_STRING("ItemName",				NULL_TO_EMPTY(pinfo->pchItemName)),
		STRFMT_STRING("ItemUsable",				NULL_TO_EMPTY(pinfo->pchItemUsable)),
		STRFMT_STRING("ItemUsagePrompt",		NULL_TO_EMPTY(pinfo->estrItemUsagePrompt)),
		STRFMT_STRING("ItemEventTooltip",		NULL_TO_EMPTY(pinfo->estrItemEventTooltip)),
		STRFMT_STRING("ItemMission",			NULL_TO_EMPTY(pinfo->pchItemMission)),
		STRFMT_STRING("ItemRequiresPowerName",	NULL_TO_EMPTY(pinfo->pchItemRequiresExprPower)),

		STRFMT_STRING("GameLifetimeLeft",		NULL_TO_EMPTY(pinfo->estrGameLifetimeLeft)),
		STRFMT_STRING("RealLifetimeLeft",		NULL_TO_EMPTY(pinfo->estrRealLifetimeLeft)),
		STRFMT_STRING("ItemValue",				NULL_TO_EMPTY(pinfo->estrItemValue)),
		STRFMT_STRING("ItemInnatePowerAutoDesc",NULL_TO_EMPTY(pinfo->estrItemInnatePowerAutoDesc)),
		STRFMT_STRING("ItemPowerAutoDesc",		NULL_TO_EMPTY(pinfo->estrItemPowerAutoDesc)),
		STRFMT_STRING("ItemFlavorDesc",			NULL_TO_EMPTY(pinfo->estrItemFlavorDesc)),
		STRFMT_STRING("ItemCategories",			NULL_TO_EMPTY(pinfo->estrItemCategories)),
		STRFMT_STRING("W",						NULL_TO_EMPTY(pinfo->estrItemWeaponDamage)),
		STRFMT_STRING("ItemWeaponDamage",		NULL_TO_EMPTY(pinfo->estrItemWeaponDamage)),
		STRFMT_STRING("ItemAttribDiff",			NULL_TO_EMPTY(pinfo->estrItemDifferences)),
		STRFMT_STRING("ItemWeaponDamageDiff",	NULL_TO_EMPTY(pinfo->estrItemWeaponDamageDiff)),
		STRFMT_STRING("ItemSetInfo",			NULL_TO_EMPTY(pinfo->estrItemSetDesc)),
		STRFMT_STRING("GemSlots",				NULL_TO_EMPTY(pinfo->estrGemSlots)),
		STRFMT_STRING("ItemRequiresClasses",	NULL_TO_EMPTY(pinfo->estrCharClassRequires)),
		STRFMT_STRING("ItemRequiresPaths",		NULL_TO_EMPTY(pinfo->estrCharPathRequires)),
		STRFMT_STRING("FormattedItemValue", 	NULL_TO_EMPTY(pinfo->estrFormattedItemValue)),
		STRFMT_STRING("ItemTransmutatedTo", 	NULL_TO_EMPTY(pinfo->estrTransmutatedTo)),
		STRFMT_END
	);
}

// Gets the formatted AutoDesc message for the PowerDef
AUTO_EXPR_FUNC(UIGen) ACMD_NAME(scp_NNOTranslatePowerAutoDesc);
const char *scp_ExprNNOTranslatePowerAutoDesc(ExprContext *pContext, const char *pchPowerDefName)
{
	static char *s_pch = NULL;
	PowerDef *pPowerDef = powerdef_Find(pchPowerDefName);

	if (s_pch)
		estrDestroy(&s_pch);
	
	if (pPowerDef)
	{
		AutoDescPower *pAutoDescPower = NULL;
		
		pAutoDescPower = StructCreate(parse_AutoDescPower);
		
		if (pAutoDescPower)
		{
			g_bNNOOverrideUseOldAutodec = true;
			powerdef_AutoDesc(PARTITION_CLIENT, langGetCurrent(), pPowerDef, &s_pch,
								pAutoDescPower, "<br>", "<bsp><bsp>", "- ", NULL, NULL, NULL, 
								0, 0, entGetPowerAutoDescDetail(NULL, true), NULL, NULL);
			g_bNNOOverrideUseOldAutodec = false;

			s_pch = pAutoDescPower->pchCustom;
			pAutoDescPower->pchCustom = NULL;
			StructDestroy(parse_AutoDescPower, pAutoDescPower);
		}
	}
	
	if (!s_pch)
	{
		estrCreate(&s_pch);
	}

	return s_pch;
}

void GetNNOSuperCritterPetActivePowerString(Item *pItem, SuperCritterPetDef* pSCPDef, Language lang, char **pestrActivePowersStringOut )
{
	SuperCritterPetActivePowerDef **eaSCPActivePowers = NULL;

	scp_GetActivePowerDefs(pSCPDef, pItem, &eaSCPActivePowers);
	if (eaSize(&eaSCPActivePowers) > 0)
	{
		char *estrAutoDesc = NULL;

		const char* pchSeparator = TranslateMessageKey("Inventory_ItemInfo_Pet_ActivePower_Separator");
		U32 uiLenSep = pchSeparator ? (U32)strlen(pchSeparator) : 0;

		FOR_EACH_IN_EARRAY_FORWARDS(eaSCPActivePowers, SuperCritterPetActivePowerDef, pSCPPowerDef)
		{
			PowerDef *pPowerDef = GET_REF(pSCPPowerDef->hPowerDef);

			if (pPowerDef)
			{
				AutoDescPower *pAutoDescPower = NULL;
				estrClear(&estrAutoDesc);

				pAutoDescPower = StructCreate(parse_AutoDescPower);
				
				if (pAutoDescPower)
				{
					g_bNNOOverrideUseOldAutodec = true;
					powerdef_AutoDesc(PARTITION_CLIENT, langGetCurrent(), pPowerDef, &estrAutoDesc,
										pAutoDescPower, "<br>", "<bsp><bsp>", "- ", NULL, NULL, NULL, 
										0, 0, entGetPowerAutoDescDetail(NULL, true), NULL, NULL);
					g_bNNOOverrideUseOldAutodec = false;

					estrAutoDesc = pAutoDescPower->pchCustom;
					pAutoDescPower->pchCustom = NULL;
					StructDestroy(parse_AutoDescPower, pAutoDescPower);
				}
				

				langFormatGameMessageKey(lang, pestrActivePowersStringOut, "Inventory_ItemInfo_Pet_ActivePower", 
					STRFMT_STRING("PowerName", NULL_TO_EMPTY(langTranslateDisplayMessage(lang, pPowerDef->msgDisplayName))),
					STRFMT_STRING("PowerDesc", NULL_TO_EMPTY(langTranslateDisplayMessage(lang, pPowerDef->msgDescription))),
					STRFMT_STRING("PowerAutoDesc", NULL_TO_EMPTY(estrAutoDesc)),
					STRFMT_INT("PowerAppliesToPlayer", pSCPPowerDef->bAppliesToPlayer),
					STRFMT_INT("PowerAppliesToSCP", pSCPPowerDef->bAppliesToSummonedPet),
					STRFMT_END);
			}

			if (uiLenSep && FOR_EACH_IDX(-, pSCPPowerDef) + 1 < FOR_EACH_COUNT(pSCPPowerDef))
			{
				estrConcatString(pestrActivePowersStringOut, pchSeparator, uiLenSep);
			}
		}
		FOR_EACH_END

		estrDestroy(&estrAutoDesc);
	}
	
	eaDestroy(&eaSCPActivePowers);

}


void GetNNOSuperCritterPetInfo(char **pestrResult,
								Language lang,
								Item *pItem,
								Entity *pCritterFakeEntity,
								Entity *pEnt, 
								char const * pchDescriptionKey,
								char const * pchContextKey)
{
	ItemDef *pItemDef = SAFE_GET_REF(pItem, hItem);
	SuperCritterPet* pSCP = pItem && pItem->pSpecialProps ? pItem->pSpecialProps->pSuperCritterPet : NULL;
	SuperCritterPetDef* pSCPDef = SAFE_GET_REF(pSCP, hPetDef);
	CritterDef* pCritterDef = SAFE_GET_REF(pSCPDef, hCritterDef);
	PowerDef** eaPetPowers = NULL;
	CharacterClass* pClass = SAFE_GET_REF(pSCP, hClassDef);
	char *estrEquipSlots = NULL;
	char *estrTemp = NULL;
	char *estrEnhancementList = NULL;
	char *estrItemFlavorDesc = NULL;
	char *estrFormattedContextString = NULL;
	char *estrActivePowersString = NULL;
	int i;
	int numPowers = 0;
	bool bScalesOnBind;
	NNOItemInfo *pinfo = StructCreate(parse_NNOItemInfo);

	if( !pItem || !pItemDef || !pSCPDef)
	{
		return;
	}

	// Basic raw data
	// Right now this is just to get the "Bound" field.  Kind of overkill, but I'd rather not dupe more code.  We should take care of
	// this when we take care of the "context" issue.
	if(!GetNNOItemInfoComparedStruct(lang, pinfo, pItem, NULL, pEnt, false, 0))
	{
		estrCopy2(pestrResult, "Error getting item info");
		return;
	}

	bScalesOnBind = pSCPDef->bLevelToPlayer && pSCP->uXP == 0;

	if (pCritterDef && pClass && pCritterFakeEntity)
	{
		const char *pchItemDesc = NULL;
		bool bOwnedByPlayer = false;

		//powers:
		for (i = 0; i < eaSize(&pCritterDef->ppPowerConfigs); i++)
		{
			PowerDef* pDef = GET_REF(pCritterDef->ppPowerConfigs[i]->hPower);
			if (pDef && pDef->ePurpose == StaticDefineInt_FastStringToInt(PowerPurposeEnum, "Pet", -1))
			{
				eaPush(&eaPetPowers, pDef);
			}
		}
		for (i = 0; i < eaSize(&pCritterDef->ppPowerConfigs); i++)
		{
			PowerDef* pDef = GET_REF(pCritterDef->ppPowerConfigs[i]->hPower);
			if (pDef && pDef->ePurpose == StaticDefineInt_FastStringToInt(PowerPurposeEnum, "PetUnlock01", -1))
			{
				eaPush(&eaPetPowers, pDef);
			}
		}

		numPowers = eaSize(&eaPetPowers);

		//runestone slots:
		GetNNOBuildGemSlotString(pItem, pItemDef, &estrEnhancementList, lang, "Inventory_ItemInfo_Pet_Runstone");

		//equip slots:
		for(i=0; i< eaSize(&pSCPDef->eaEquipSlots); i++)
		{
			SCPEquipSlotDef* pSlot = eaGet(&pSCPDef->eaEquipSlots, i);
			if (!pSlot)
			{
				continue;
			}
			if (i > 0)
			{
				estrAppend2(&estrEquipSlots, ", ");
			}
			estrAppend2(&estrEquipSlots, GetPetEquipmentSlotTypeTranslate(lang, pItem, i));
		}

		pchItemDesc = item_GetTranslatedDescription(pItem, lang);
		if (pchItemDesc)
			estrPrintf(&estrItemFlavorDesc, "%s", pchItemDesc);

		if(pEnt && pEnt->myContainerID)
		{
			U32 contID = pItem->id >> 32;

			if(pEnt->myContainerID == contID)
			{
				bOwnedByPlayer = true;
			}
		}

		if (pchContextKey && pchContextKey[0])
		{
			_formatContextMessage(lang,&estrFormattedContextString,pchContextKey,pItem,pinfo);
		}

		GetNNOSuperCritterPetActivePowerString(pItem, pSCPDef, lang, &estrActivePowersString);

		langFormatGameMessageKey(lang, pestrResult, pchDescriptionKey,
			STRFMT_ITEM(pItem),
			STRFMT_ITEMDEF(pItemDef),
			STRFMT_STRING("Name",			NULL_TO_EMPTY(pSCP->pchName)),
			STRFMT_STRING("ClassName",		NULL_TO_EMPTY(langTranslateDisplayMessage(lang, pClass->msgDisplayName))),
			STRFMT_STRING("BaseName",		NULL_TO_EMPTY(langTranslateDisplayMessage(lang, pCritterDef->displayNameMsg))),
			STRFMT_STRING("EquipSlots",		NULL_TO_EMPTY(estrEquipSlots)),
			STRFMT_INT("Level",				bScalesOnBind ? scp_GetPetStartLevelForPlayerLevel(entity_GetSavedExpLevelLimited(pEnt), pItem) : pSCP->uLevel),
			STRFMT_INT("IsBindOnEquip",		!pinfo->bBindsOnPickup && !!(pItemDef->flags & (kItemDefFlag_BindOnEquip | kItemDefFlag_BindToAccountOnEquip))),
			STRFMT_INT("IsBindOnPickup",	!!(pItemDef->flags & (kItemDefFlag_BindOnPickup | kItemDefFlag_BindToAccountOnPickup) || pItem->bForceBind)),
			STRFMT_INT("IsBindToAccount",	!!(pItemDef->flags & (kItemDefFlag_BindToAccountOnEquip | kItemDefFlag_BindToAccountOnPickup))),
			STRFMT_INT("IsAlreadyBound",	(pItem->flags & kItemFlag_Bound)? 1 : 0 ),
			STRFMT_INT("IsOwnedByPlayer",	bOwnedByPlayer), 
			STRFMT_STRING("ScaleOnBind",	bScalesOnBind ? "true" : ""),
			STRFMT_INT("CannotDiscard",		!item_CanDiscard(pEnt,pItemDef,false)),
			STRFMT_STRING("ItemQuality",	NULL_TO_EMPTY(StaticDefineInt_FastIntToString(ItemQualityEnum, pItemDef->Quality))),
			STRFMT_INT("ItemQualityIndex",	pItem ? item_GetQuality(pItem) : pItemDef->Quality),
			STRFMT_INT("NumPowers",			numPowers),
			STRFMT_STRING("Pow1Name",		numPowers > 0 ? NULL_TO_EMPTY(langTranslateDisplayMessage(lang, eaPetPowers[0]->msgDisplayName)) : NULL),
			STRFMT_STRING("Pow2Name",		numPowers > 1 ? NULL_TO_EMPTY(langTranslateDisplayMessage(lang, eaPetPowers[1]->msgDisplayName)) : NULL),
			STRFMT_STRING("Pow3Name",		numPowers > 2 ? NULL_TO_EMPTY(langTranslateDisplayMessage(lang, eaPetPowers[2]->msgDisplayName)) : NULL),
			STRFMT_STRING("Pow1Desc",		numPowers > 0 ? NULL_TO_EMPTY(langTranslateDisplayMessage(lang, eaPetPowers[0]->msgDescription)) : NULL),
			STRFMT_STRING("Pow2Desc",		numPowers > 1 ? NULL_TO_EMPTY(langTranslateDisplayMessage(lang, eaPetPowers[1]->msgDescription)) : NULL),
			STRFMT_STRING("Pow3Desc",		numPowers > 2 ? NULL_TO_EMPTY(langTranslateDisplayMessage(lang, eaPetPowers[2]->msgDescription)) : NULL),
			STRFMT_STRING("EnhancementList",NULL_TO_EMPTY(estrEnhancementList)),
			STRFMT_INT("Power",				getAttributeScore(pCritterFakeEntity, "Stat_Power")),
			STRFMT_INT("Crit",				getAttributeScore(pCritterFakeEntity, "Stat_Crit")),
			STRFMT_INT("Recovery",			getAttributeScore(pCritterFakeEntity, "Stat_Recovery")),
			STRFMT_INT("ArmorPen",			getAttributeScore(pCritterFakeEntity, "Stat_ArmorPen")),
			STRFMT_INT("Defense",			getAttributeScore(pCritterFakeEntity, "Stat_Defense")),
			STRFMT_INT("Deflect",			getAttributeScore(pCritterFakeEntity, "Stat_Deflect")),
			STRFMT_INT("Regen",				getAttributeScore(pCritterFakeEntity, "Stat_Regen")),
			STRFMT_INT("HealthSteal",		getAttributeScore(pCritterFakeEntity, "Stat_HealthSteal")),
			STRFMT_INT("HitPointsMax",		getAttributeScore(pCritterFakeEntity, "HitPointsMax")),
			STRFMT_STRING("ItemFlavorDesc", NULL_TO_EMPTY(estrItemFlavorDesc)),

			STRFMT_STRING("ContextMessage", NULL_TO_EMPTY(estrFormattedContextString)),
			STRFMT_STRING("ActivePowersString", NULL_TO_EMPTY(estrActivePowersString)),
			STRFMT_END
			);
	}

	estrDestroy(&estrFormattedContextString);

	eaDestroy(&eaPetPowers);
	estrDestroy(&estrEnhancementList);
	estrDestroy(&estrEquipSlots);
	estrDestroy(&estrItemFlavorDesc);
	estrDestroy(&estrActivePowersString);

	StructDestroy(parse_NNOItemInfo, pinfo);
}

static void _getNNOItemInfoCompared(char **pestrResult,
							Language lang,
							SA_PARAM_OP_VALID Item *pItem,
							SA_PARAM_OP_VALID Item *pItemOther, 
							SA_PARAM_OP_VALID Entity *pEnt, 
							ACMD_EXPR_DICT(Message) const char *pchDescriptionKey,
							const char *pchContextKey,
							bool bGetItemAttribDiffs,
							S32 eActiveGemSlotType)
{
	char *estrFormattedContextString = NULL;
	NNOItemInfo *pinfo = StructCreate(parse_NNOItemInfo);

	// Basic raw data
	if(!GetNNOItemInfoComparedStruct(lang, pinfo, pItem, pItemOther, pEnt, bGetItemAttribDiffs, eActiveGemSlotType))
	{
		estrCopy2(pestrResult, "Error getting item info");
		return;
	}

	if (pchContextKey && pchContextKey[0])
	{
		_formatContextMessage(lang,&estrFormattedContextString,pchContextKey,pItem,pinfo);
	}

	langFormatGameMessageKey(lang, pestrResult, pchDescriptionKey,
		STRFMT_ITEM(pItem),
		STRFMT_ITEMDEF(pinfo->pItemDef),

		// pinfo->iMinDamage unused
		// pinfo->iMaxDamage unused
		STRFMT_INT("ItemMinLevel",			pinfo->iMinLevel),
		STRFMT_INT("ItemMaxLevel",			pinfo->iMaxLevel),
		STRFMT_INT("ItemQualityIndex",		pinfo->iItemQualityIndex),
		STRFMT_INT("ItemPowerFactor",		pinfo->iItemPowerFactor),
		STRFMT_INT("IsUsableExpr",			pinfo->bEntUsableExpr),
		STRFMT_INT("IsRecommended",			pinfo->bItemIsRecommended),
		STRFMT_INT("HasClass",				pinfo->bEntHasClass),
		STRFMT_INT("HasPath",				pinfo->bEntHasPath),
		STRFMT_INT("IsOwnedByPlayer",		pinfo->bIsOwnedByPlayer),
		STRFMT_INT("IsMount",				pinfo->bIsMount),
		STRFMT_INT("MeetsLevel",			pinfo->bEntMeetsLevelRequirements),
		STRFMT_INT("IsBindOnEquip",			pinfo->bBindsOnEquip),
		STRFMT_INT("IsBindOnPickup",		pinfo->bBindsOnPickup),
		STRFMT_INT("IsBindToAccount",		pinfo->bBindsToAccount),
		STRFMT_INT("IsAlreadyBound",		pinfo->bIsBound),
		STRFMT_INT("IsGem",					pinfo->bIsGem),
		STRFMT_INT("CannotDiscard",			pinfo->bCantDiscard),
		STRFMT_INT("IsItemEnchant",			pinfo->bIsItemEnchant),
		STRFMT_INT("IsPetEnchant",			pinfo->bIsPetEnchant),
		STRFMT_INT("ItemIdentified",		pinfo->bItemIdentified),
		STRFMT_INT("ItemHasBeenDyed",		pinfo->bItemHasBeenDyed),
		STRFMT_INT("ItemIsTransmutated",	pinfo->bIsTransmutated),
		STRFMT_INT("ItemCount",				pinfo->iItemCount),
		STRFMT_INT("ChargesLeft",			pinfo->iChargesLeft),
		STRFMT_INT("PowerChargesMax",		pinfo->iPowerMaxCharges),
		STRFMT_INT("ItemProgressionTier",		pinfo->uItemProgressionTier),
		STRFMT_INT("ItemProgressionLevel",		pinfo->uItemProgressionLevel),
		STRFMT_INT("ItemProgressionXP", 		pinfo->uItemProgressionXP),
		STRFMT_INT("ItemProgressionXPRequired", pinfo->uItemProgressionXPRequired),
		STRFMT_INT("ItemProgressionReadyToEvo", pinfo->bItemProgressionReadyToEvo),
		STRFMT_INT("ItemProgressionIsMaxLevel", pinfo->bItemProgressionIsMaxLevel),
		STRFMT_FLOAT("PowerRechargeTime",	pinfo->fPowerRechargeTime),

		STRFMT_STRING("ItemQuality",			NULL_TO_EMPTY(pinfo->pchItemQuality)),
		STRFMT_STRING("ItemName",				NULL_TO_EMPTY(pinfo->pchItemName)),
		STRFMT_STRING("ItemUsable",				NULL_TO_EMPTY(pinfo->pchItemUsable)),
		STRFMT_STRING("ItemUsagePrompt",		NULL_TO_EMPTY(pinfo->estrItemUsagePrompt)),
		STRFMT_STRING("ItemEventTooltip",		NULL_TO_EMPTY(pinfo->estrItemEventTooltip)),
		STRFMT_STRING("ItemMission",			NULL_TO_EMPTY(pinfo->pchItemMission)),
		STRFMT_STRING("ItemRequiresPowerName",	NULL_TO_EMPTY(pinfo->pchItemRequiresExprPower)),

		STRFMT_STRING("GameLifetimeLeft",		NULL_TO_EMPTY(pinfo->estrGameLifetimeLeft)),
		STRFMT_STRING("RealLifetimeLeft",		NULL_TO_EMPTY(pinfo->estrRealLifetimeLeft)),
		STRFMT_STRING("ItemValue",				NULL_TO_EMPTY(pinfo->estrItemValue)),
		STRFMT_STRING("ItemInnatePowerAutoDesc",NULL_TO_EMPTY(pinfo->estrItemInnatePowerAutoDesc)),
		STRFMT_STRING("ItemPowerAutoDesc",		NULL_TO_EMPTY(pinfo->estrItemPowerAutoDesc)),
		STRFMT_STRING("ItemFlavorDesc",			NULL_TO_EMPTY(pinfo->estrItemFlavorDesc)),
		STRFMT_STRING("ItemCategories",			NULL_TO_EMPTY(pinfo->estrItemCategories)),
		STRFMT_STRING("W",						NULL_TO_EMPTY(pinfo->estrItemWeaponDamage)),
		STRFMT_STRING("ItemWeaponDamage",		NULL_TO_EMPTY(pinfo->estrItemWeaponDamage)),
		STRFMT_STRING("ItemAttribDiff",			NULL_TO_EMPTY(pinfo->estrItemDifferences)),
		STRFMT_STRING("ItemWeaponDamageDiff",	NULL_TO_EMPTY(pinfo->estrItemWeaponDamageDiff)),
		STRFMT_STRING("ItemSetInfo",			NULL_TO_EMPTY(pinfo->estrItemSetDesc)),
		STRFMT_STRING("GemSlots",				NULL_TO_EMPTY(pinfo->estrGemSlots)),
		STRFMT_STRING("ItemRequiresClasses",	NULL_TO_EMPTY(pinfo->estrCharClassRequires)),
		STRFMT_STRING("ItemRequiresPaths",		NULL_TO_EMPTY(pinfo->estrCharPathRequires)),
		STRFMT_STRING("FormattedItemValue", 	NULL_TO_EMPTY(pinfo->estrFormattedItemValue)),
		STRFMT_STRING("ItemTransmutatedTo", 	NULL_TO_EMPTY(pinfo->estrTransmutatedTo)),

		STRFMT_STRING("ContextMessage", NULL_TO_EMPTY(estrFormattedContextString)),
		STRFMT_END
	);

	estrDestroy(&estrFormattedContextString);

#if GAMECLIENT
	if(g_bDisplayItemDebugInfo && pItem && pinfo->pItemDef)
	{
		ItemPowerDef *item_power;

		item_PrintDebugText(pestrResult,pEnt,pItem,pinfo->pItemDef);

		if (pItem->flags & kItemFlag_Algo)
		{
			int i;
			if (pItem->pAlgoProps)
			{
				for (i = 0; i < eaSize(&pItem->pAlgoProps->ppItemPowerDefRefs); i++)
				{
					item_power = GET_REF(pItem->pAlgoProps->ppItemPowerDefRefs[i]->hItemPowerDef);
					if (item_power)
					{
						estrConcatf(pestrResult,"\npower: %s", item_power->pchName);
					}
				}    
			}
		}
	}
#endif

	StructDestroy(parse_NNOItemInfo, pinfo);
}

void GetNNOItemInfoCompared(char **pestrResult1,
							char **pestrResult2,
							Language lang,
							SA_PARAM_OP_VALID Item *pItem,
							SA_PARAM_OP_VALID Item *pItemOther, 
							SA_PARAM_OP_VALID Entity *pEnt, 
							ACMD_EXPR_DICT(Message) const char *pchDescriptionKey1,
							ACMD_EXPR_DICT(Message) const char *pchDescriptionKey2,
							const char *pchContextKey,
							S32 eActiveGemSlotType)
{
	_getNNOItemInfoCompared(pestrResult1, lang, pItem, pItemOther, pEnt, pchDescriptionKey1, pchContextKey, (pItemOther != NULL), eActiveGemSlotType);

	if (pItemOther)
	{
		_getNNOItemInfoCompared(pestrResult2, lang, pItemOther, pItem, pEnt, pchDescriptionKey2, pchContextKey, false, eActiveGemSlotType);
	}
}

// 
static bool itemRecommended_verifyUsageRestrictions(int iPartitionIdx, Entity *pEnt, ItemDef *pItemDef)
{
	// we're allowing the level check to act as if the player was two levels higher. I don't want this
	// to be passed per function call to GenExprIsItemRecommended and GenExprGetNNOItemInfoCompared.
	// as it's more of a project-wide config, but there's no good config place to put this at the moment
	static S32 s_iOverrideLevelThreshold = 2;
	S32 iOverrideItemLevel = 0;

	if (pItemDef->pRestriction)
	{
		iOverrideItemLevel = pItemDef->pRestriction->iMinLevel - s_iOverrideLevelThreshold;
		if (iOverrideItemLevel < 1)
			iOverrideItemLevel = 1;
	}

	return itemdef_VerifyUsageRestrictions(iPartitionIdx, pEnt, pItemDef, iOverrideItemLevel, NULL, -1);
}

// Returns true if pItem is better than the pOtherItem
// Currently allows an item to be regarded as usable up to two levels higher than itself
bool GetNNOIsItemRecommended(SA_PARAM_OP_VALID Entity *pEnt, SA_PARAM_OP_VALID Item *pItem, SA_PARAM_OP_VALID Item *pOtherItem)
{
	if (pEnt && pItem)	
	{
		S32 iValue, iOtherValue;
		ItemDef *pDef = GET_REF(pItem->hItem);
		ItemDef *pOtherDef = SAFE_GET_REF(pOtherItem, hItem);
		int iPartitionIdx = entGetPartitionIdx(pEnt);

		if (pDef && (pDef->eType == kItemType_Upgrade || pDef->eType == kItemType_Weapon))//only upgrades and weapons can be "reccommended"
		{
			if (pOtherDef)
			{
				if (!itemRecommended_verifyUsageRestrictions(iPartitionIdx, pEnt, pDef))
				{
					return false;
				}
				if (!itemRecommended_verifyUsageRestrictions(iPartitionIdx, pEnt, pOtherDef))
				{
					return true;
				}

				iValue = item_GetDefEPValue(PARTITION_CLIENT, pEnt, pDef, item_GetMinLevel(pItem), item_GetQuality(pItem));
				iOtherValue = item_GetDefEPValue(PARTITION_CLIENT, pEnt, pOtherDef, item_GetMinLevel(pOtherItem), item_GetQuality(pOtherItem));

				return iValue > iOtherValue;
			}
			else if (itemdef_VerifyUsageRestrictions(iPartitionIdx, pEnt, pDef, 0, NULL, -1))
				return true;	// the item must be identified- something is better than nothing
		}
	}

	return false;
}

//used for both gems in items and runestones in pets.  Pretty formats the gemslot part of the tooltip.
void GetNNOBuildGemSlotString(Item* pItem, ItemDef* pItemDef, char** estrGemSlots, Language lang, const char* pchSlotMessageName)
{
	if (eaSize(&pItemDef->ppItemGemSlots) > 0)
	{
		char* estrGemRow = NULL;
		const char* pchTranslatedDisplayName = NULL;
		const char* pchPowerIconName = NULL;
		int i;
		ItemGemSlot *const *const ppItemGemSlots = SAFE_MEMBER2(pItem,pSpecialProps,ppItemGemSlots);
		estrStackCreate(&estrGemRow);
		for (i = 0; i < eaSize(&pItemDef->ppItemGemSlots); i++)
		{
			ItemDef* pGemDef = item_GetSlottedGemItemDef(pItem, i);
			pchTranslatedDisplayName = pGemDef ? langTranslateDisplayMessage(lang, pGemDef->displayNameMsg) : NULL;
			pchPowerIconName = NULL;
			// First check on the gem's powers for the non-Innate powers
			if( eaSize(&ppItemGemSlots) > i && eaSize(&ppItemGemSlots[i]->ppPowers) > 0 )
			{
				Power * pPower = ppItemGemSlots[i]->ppPowers[0];
				PowerDef * pPowerDef = pPower ? GET_REF(pPower->hDef) : NULL;
				pchPowerIconName = pPowerDef ? pPowerDef->pchIconName : NULL;
			}

			// Next check on the gem's ItemPowers for the Innate powers
			if( pchPowerIconName == NULL && pGemDef != NULL )
			{
				int j;
				for(j=0; j<eaSize(&pGemDef->ppItemPowerDefRefs); ++j)
				{
					// Find the ItemPowerDef that matches this slot's restriction. This is the active gemslot power.
					ItemPowerDef *pGemPowerDef = GET_REF(pGemDef->ppItemPowerDefRefs[j]->hItemPowerDef);
					if(pGemPowerDef && pGemPowerDef->pRestriction && (pGemPowerDef->pRestriction->eRequiredGemSlotType & pItemDef->ppItemGemSlots[i]->eType) )
					{
						pchPowerIconName = pGemPowerDef->pchIconName;
						if( pchPowerIconName == NULL )
						{
							PowerDef *pGemPower = GET_REF(pGemPowerDef->hPower);
							pchPowerIconName = pGemPower ? pGemPower->pchIconName : NULL;
						}
						break;
					}
				}
			}

			estrClear(&estrGemRow);
			langFormatGameMessageKey(lang, &estrGemRow, pchSlotMessageName,
				STRFMT_STRING("Color", StaticDefineInt_FastIntToString(ItemGemTypeEnum, pItemDef->ppItemGemSlots[i]->eType)),
				STRFMT_STRING("SlotTypeName", StaticDefineLangGetTranslatedMessage(lang, ItemGemTypeEnum, pItemDef->ppItemGemSlots[i]->eType)),
				STRFMT_INT("IsFilled", !!pGemDef),
				STRFMT_STRING("GemName", NULL_TO_EMPTY(pchTranslatedDisplayName)),
				STRFMT_STRING("PowerIconName", NULL_TO_EMPTY(pchPowerIconName)),
				STRFMT_END);

			estrAppend2(estrGemSlots, estrGemRow);
		}
		estrDestroy(&estrGemRow);
	}
}


#include "NNOItemDescription_h_ast.c"


// End of File
