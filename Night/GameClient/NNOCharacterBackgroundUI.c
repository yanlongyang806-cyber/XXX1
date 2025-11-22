/***************************************************************************



***************************************************************************/

#include "../common/NNOCharacterBackground.h"

#include "resourcemanager.h"

#include "StringCache.h"
#include "stdtypes.h"
#include "Entity.h"
#include "LoginCommon.h"
#include "PowerTreeHelpers.h"
#include "nnocharacterbackgroundui_c_ast.h"
#include "PowerTree.h"
#include "PowerGrid.h"
#include "PowerVars.h"
#include "Login2Common.h"

#include "UIGen.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_UISystem););

extern Login2CharacterCreationData *g_CharacterCreationData;

AUTO_STRUCT;
typedef struct NNOCharacterBackgroundGroupInfo
{
	const char* pcName;			AST(POOL_STRING)
	const char* pcDisplayNameKey; AST(POOL_STRING)
	const char* pcFlavorTextKey; AST(POOL_STRING)
	const char* pcTextureName;			AST(POOL_STRING)
	F32 XPercent;
	F32 YPercent;
} NNOCharacterBackgroundGroupInfo;

AUTO_STRUCT;
typedef struct NNOCharacterBackgroundChoiceInfo
{
	const char* pcName;			AST(POOL_STRING)
	const char* pcDisplayNameKey; AST(POOL_STRING)
	const char* pcFlavorTextKey; AST(POOL_STRING)
	const char* pcBioTextKey; AST(POOL_STRING)
	char* pcPowers;	AST(ESTRING)
} NNOCharacterBackgroundChoiceInfo;

extern DictionaryHandle g_hCharacterBackgroundGroupDict;
extern DictionaryHandle g_hPowerTreeNodeDefDict;

AUTO_EXPR_FUNC(UIGen) ACMD_IFDEF(GAMECLIENT) ACMD_NAME(GenGetPossibleBackgroundGroups);
void exprGenGetPossibleBackgroundGroups(SA_PARAM_NN_VALID UIGen *pSelf)
{
	static NNOCharacterBackgroundGroupInfo **eaData = NULL;
	S32 i;

	if (!eaData)
	{
		RefDictIterator iter;
		CharacterBackgroundGroup* pGroup = NULL;

		eaSetSizeStruct(&eaData, parse_NNOCharacterBackgroundGroupInfo, RefSystem_GetDictionaryNumberOfReferents(g_hCharacterBackgroundGroupDict));

		RefSystem_InitRefDictIterator(g_hCharacterBackgroundGroupDict, &iter);
		for (i = eaSize(&eaData)-1; i >= 0; i--)
		{
			pGroup = (CharacterBackgroundGroup*)RefSystem_GetNextReferentFromIterator(&iter);
			eaData[i]->pcName = allocAddString(pGroup->pchName);
			eaData[i]->pcTextureName = allocAddString(pGroup->pchTextureName);
			eaData[i]->pcDisplayNameKey = REF_STRING_FROM_HANDLE(pGroup->pDisplayName.hMessage);
			eaData[i]->pcFlavorTextKey = REF_STRING_FROM_HANDLE(pGroup->pFlavorText.hMessage);
			eaData[i]->XPercent = pGroup->XPercent;
			eaData[i]->YPercent = pGroup->YPercent;
		}
	}
	ui_GenSetManagedListSafe(pSelf, &eaData, NNOCharacterBackgroundGroupInfo, false);

}

AUTO_EXPR_FUNC(UIGen) ACMD_IFDEF(GAMECLIENT) ACMD_NAME(GenGetPossibleBackgroundChoicesForGroup);
void exprGenGetPossibleBackgroundChoicesForGroup(SA_PARAM_NN_VALID UIGen *pSelf, const char* pcGroupName)
{
	NNOCharacterBackgroundChoiceInfo ***peaData = ui_GenGetManagedListSafe(pSelf, NNOCharacterBackgroundChoiceInfo);
	CharacterBackgroundGroup* pGroup = RefSystem_ReferentFromString(g_hCharacterBackgroundGroupDict, pcGroupName);
	S32 i, j;

	if (pGroup)
	{
		eaSetSizeStruct(peaData, parse_NNOCharacterBackgroundChoiceInfo, eaSize(&pGroup->ppBackgrounds));
		for (i = eaSize(peaData)-1; i >= 0; i--)
		{
			(*peaData)[i]->pcName = allocAddString(pGroup->ppBackgrounds[i]->pchName);
			(*peaData)[i]->pcDisplayNameKey = REF_STRING_FROM_HANDLE(pGroup->ppBackgrounds[i]->pDisplayName.hMessage);
			(*peaData)[i]->pcFlavorTextKey = REF_STRING_FROM_HANDLE(pGroup->ppBackgrounds[i]->pFlavorText.hMessage);
			(*peaData)[i]->pcBioTextKey = REF_STRING_FROM_HANDLE(pGroup->ppBackgrounds[i]->pBiographyText.hMessage);
			estrClear(&(*peaData)[i]->pcPowers);
			for (j = 0; j < eaSize(&pGroup->ppBackgrounds[i]->ppchPowerTreeNodeChoices); j++)
			{
				PTNodeDef* pDef = RefSystem_ReferentFromString(g_hPowerTreeNodeDefDict, pGroup->ppBackgrounds[i]->ppchPowerTreeNodeChoices[j]);
				const char* pcDisplayName = TranslateDisplayMessage(pDef->pDisplayMessage);
				if (j>0)
					estrConcat(&(*peaData)[i]->pcPowers, ", ", 2);
				estrConcatf(&(*peaData)[i]->pcPowers, "%s", pcDisplayName);
			}
		}
		ui_GenSetManagedListSafe(pSelf, peaData, NNOCharacterBackgroundChoiceInfo, false);
	}
	else
	{
		ui_GenSetManagedListSafe(pSelf, NULL, NNOCharacterBackgroundChoiceInfo, false);
	}
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(GetCharacterBackgroundChoiceInGroupByIndex);
SA_RET_OP_VALID CharacterBackgroundChoice* exprGetCharacterBackgroundChoiceInGroupByIndex(const char* pcGroupName, int iChoiceIndex)
{
	CharacterBackgroundGroup* pGroup = RefSystem_ReferentFromString(g_hCharacterBackgroundGroupDict, pcGroupName);
	if (!pGroup)
	{
		Errorf("Invalid Character Background Group name passed to GetCharacterBackgroundChoiceInGroupByIndex: %s", pcGroupName);
		return NULL;
	}

	if(iChoiceIndex >= eaSize(&pGroup->ppBackgrounds))
	{
		Errorf("Index %d too high, GetCharacterBackgroundChoiceInGroupByIndex: %s", iChoiceIndex, pcGroupName);
		return NULL;
	}

	return pGroup->ppBackgrounds[iChoiceIndex];
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(GetCharacterBackgroundChoiceCountInGroup);
int exprGetCharacterBackgroundChoiceCountInGroup(const char* pcGroupName)
{
	CharacterBackgroundGroup* pGroup = RefSystem_ReferentFromString(g_hCharacterBackgroundGroupDict, pcGroupName);
	if (!pGroup)
	{
		Errorf("Invalid Character Background Group name passed to GetCharacterBackgroundChoiceCountInGroup: %s", pcGroupName);
		return 0;
	}

	return eaSize(&pGroup->ppBackgrounds);
}

AUTO_EXPR_FUNC(UIGen) ACMD_IFDEF(GAMECLIENT) ACMD_NAME(SetCharacterBackgroundChoice);
void exprSetCharacterBackgroundChoice(const char* pcGroupName, const char* pcChoiceName)
{
	char* estrFormatted;
	CharacterBackgroundGroup* pGroup = RefSystem_ReferentFromString(g_hCharacterBackgroundGroupDict, pcGroupName);

	bool bFound = false;
	if (!pGroup)
	{
		Errorf("Invalid Character Background Group name passed to SetCharacterBackgroundChoice: %s", pcGroupName);
		return;
	}
	else
	{
		int i;
		for (i = 0; i < eaSize(&pGroup->ppBackgrounds); i++)
		{
			if (pGroup->ppBackgrounds[i]->pchName == allocFindString(pcChoiceName))
				bFound = true;
		}
	}
	if (!bFound)
	{
		Errorf("Invalid Character Background Choice name passed to SetCharacterBackgroundChoice: %s", pcChoiceName);
		return;
	}
	estrStackCreate(&estrFormatted);
	estrPrintf(&estrFormatted, "%s.%s", pcGroupName, pcChoiceName);
    if (g_CharacterCreationData)
	{
		if (g_CharacterCreationData->gameSpecificChoice)
        {
			StructFreeString(g_CharacterCreationData->gameSpecificChoice);
        }

		g_CharacterCreationData->gameSpecificChoice = StructAllocString(estrFormatted);
	}

	estrDestroy(&estrFormatted);
}

AUTO_EXPR_FUNC(UIGen);
void CharacterCreation_GetPowerListFromBackground(SA_PARAM_NN_VALID UIGen* pGen, const char* pchGroupName, const char* pchChoiceName)
{
	PowerListNode ***peaPowerList =  ui_GenGetManagedListSafe(pGen, PowerListNode);
	CharacterBackgroundGroup* pGroup = RefSystem_ReferentFromString(g_hCharacterBackgroundGroupDict, pchGroupName);
	CharacterBackgroundChoice* pChoice = NULL;
	int i, j, iNodes = 0;

	if (pGroup)
	{
		for (i = 0; i < eaSize(&pGroup->ppBackgrounds); i++)
		{
			if (pGroup->ppBackgrounds[i]->pchName == allocFindString(pchChoiceName))
			{
				pChoice = pGroup->ppBackgrounds[i];
				for (j = 0; j < eaSize(&pChoice->ppchPowerTreeNodeChoices); j++)
				{
					PowerListNode *pListNode = eaGetStruct(peaPowerList, parse_PowerListNode, iNodes++);
					PTNodeDef* pNodeDef = RefSystem_ReferentFromString(g_hPowerTreeNodeDefDict, pChoice->ppchPowerTreeNodeChoices[j]);

					FillPowerListNode(NULL, pListNode, NULL, NULL, NULL, NULL, NULL, pNodeDef);
				}
				break;
			}
		}
	}

	while (eaSize(peaPowerList) > iNodes)
		StructDestroy(parse_PowerListNode, eaPop(peaPowerList));

	ui_GenSetListSafe(pGen, peaPowerList, PowerListNode);
}

#include "nnocharacterbackgroundui_c_ast.c"
