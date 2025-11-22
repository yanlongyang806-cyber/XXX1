/***************************************************************************



***************************************************************************/
#ifndef NNOITEMDECRIPTION_H__
#define NNOITEMDECRIPTION_H__
#pragma once

#include "AppLocale.h"  // for language

AUTO_STRUCT;
typedef struct NNOItemInfo
{
	Item *pItem;				AST(UNOWNED)
	ItemDef *pItemDef;			AST(UNOWNED NO_NETSEND)

	S32 iMinDamage;
	S32 iMaxDamage;
	F32 fPowerRechargeTime;

	int iMinLevel;
	int iMaxLevel;
	int iItemQualityIndex;
	int iItemPowerFactor;
	int iItemCount;
	S32 iChargesLeft;
	S32 iPowerMaxCharges;
	U32 uItemProgressionTier;
	U32 uItemProgressionLevel;
	U32 uItemProgressionXP;
	U32 uItemProgressionXPRequired;

	bool bEntUsableExpr;
	bool bItemIsRecommended;
	bool bEntHasClass;
	bool bEntHasPath;
	bool bIsOwnedByPlayer;
	bool bIsMount;
	bool bEntMeetsLevelRequirements;
	bool bBindsOnEquip;
	bool bBindsOnPickup;
	bool bBindsToAccount;
	bool bIsBound;
	bool bIsGem;
	bool bIsItemEnchant;
	bool bIsPetEnchant;
	bool bItemIdentified;
	bool bItemHasBeenDyed;
	bool bIsTransmutated;
	bool bCantDiscard;	//negative, but matches item flag.
	bool bItemProgressionReadyToEvo;
	bool bItemProgressionIsMaxLevel;

	const char *pchItemQuality;				AST(UNOWNED)
	const char *pchItemName;				AST(UNOWNED)
	const char *pchItemUsable;				AST(UNOWNED)
	const char *pchItemPrompt;				AST(UNOWNED)
	const char *pchItemMission; 			AST(UNOWNED)
	const char *pchItemRequiresExprPower;	AST(UNOWNED)

	char *estrItemValue;				AST(ESTRING NAME(ItemValue))
	char *estrFormattedItemValue;		AST(ESTRING NAME(FormattedItemValue))
	char *estrItemUsagePrompt;			AST(ESTRING NAME(ItemUsagePrompt))
	char *estrItemEventTooltip;			AST(ESTRING NAME(ItemEventTooltip))
	char *estrItemInnatePowerAutoDesc;	AST(ESTRING NAME(ItemInnatePowerAutoDesc))
	char *estrItemPowerAutoDesc;		AST(ESTRING NAME(ItemPowerAutoDesc))
	char *estrItemFlavorDesc;			AST(ESTRING NAME(ItemFlavorDesc))
	char *estrItemCategories;			AST(ESTRING NAME(ItemCategories))
	char *estrItemWeaponDamage;			AST(ESTRING NAME(Itemweapondamage))
	char *estrItemDifferences;			AST(ESTRING NAME(ItemDifferences))
	char *estrItemWeaponDamageDiff;		AST(ESTRING NAME(ItemWeaponDamageDiff))
	char *estrItemSetDesc;				AST(ESTRING NAME(ItemSetDesc))
	char *estrGemSlots;					AST(ESTRING NAME(GemSlots))
	char *estrCharClassRequires;		AST(ESTRING NAME(CharClassRequires))
	char *estrCharPathRequires;			AST(ESTRING NAME(CharPathRequires))
	char *estrTransmutatedTo;			AST(ESTRING NAME(rTransmutatedTo))
	char *estrGameLifetimeLeft;			AST(ESTRING NAME(GameLifetimeLeft))
	char *estrRealLifetimeLeft;			AST(ESTRING NAME(RealLifetimeLeft))

} NNOItemInfo;


bool GetNNOItemInfoComparedStruct(Language lang,
	NNOItemInfo *pinfo,
	SA_PARAM_OP_VALID Item *pItem,
	SA_PARAM_OP_VALID Item *pItemOther, 
	SA_PARAM_OP_VALID Entity *pEnt, 
	bool bGetItemAttribDiffs,
	S32 eActiveGemSlotType);

bool GetNNOItemInfoComparedStructNoStrings(Language lang,
	NNOItemInfo *pinfo,
	SA_PARAM_OP_VALID Item *pItem,
	SA_PARAM_OP_VALID Item *pItemOther, 
	SA_PARAM_OP_VALID Entity *pEnt, 
	bool bGetItemAttribDiffs,
	S32 eActiveGemSlotType);

void GetNNOItemInfoCompared(char **pestrResult1,
	char **pestrResult2,
	Language lang,
	SA_PARAM_OP_VALID Item *pItem,
	SA_PARAM_OP_VALID Item *pItemOther, 
	SA_PARAM_OP_VALID Entity *pEnt, 
	const char *pchDescriptionKey1,
	const char *pchDescriptionKey2,
	const char *pchContextKey,
	S32 eActiveGemSlotType);

void GetNNOSuperCritterPetInfo(char **ppestrResult,
	Language lang,
	Item *pItem,
	Entity *pCritterFakeEntity,
	Entity *pEnt, 
	char const * pchDescriptionKey,
	char const * pchContextKey);

bool GetNNOIsItemRecommended(SA_PARAM_OP_VALID Entity *pEnt, SA_PARAM_OP_VALID Item *pItem, SA_PARAM_OP_VALID Item *pOtherItem);
void GetNNOBuildGemSlotString(Item* pItem, ItemDef* pItemDef, char** estrGemSlots, Language lang, const char* pchSlotMessageName);

void GetNNOSuperCritterPetActivePowerString(Item *pItem, SuperCritterPetDef* pSCPDef, Language lang, char **pestrActivePowersStringOut);

#endif /* NNOITEMDECRIPTION_H__*/

// End of File
