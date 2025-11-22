/***************************************************************************



***************************************************************************/

#include "NNOCharacterBackground.h"

#include "resourcemanager.h"

#include "StringCache.h"
#include "stdtypes.h"
#include "AutoGen/NNOCharacterBackground_h_ast.h"
#include "AutoGen/NNOCharacterBackground_h_ast.c"
#include "error.h"


AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_GameSystems););

DictionaryHandle g_hCharacterBackgroundGroupDict;
extern DictionaryHandle g_hItemDict;
extern DictionaryHandle g_hPowerTreeNodeDefDict;

typedef struct GameAccountDataExtract GameAccountDataExtract;
AUTO_RUN;
// Registers the character paths dictionary
int CharacterBackground_Startup(void)
{
	// Set up reference dictionaries
	g_hCharacterBackgroundGroupDict = RefSystem_RegisterSelfDefiningDictionary("CharacterBackgroundGroup", false, parse_CharacterBackgroundGroup, true, true, NULL);

	return 1;
}

void CharacterBackground_Load(void)
{
	// Load all character paths into the dictionary
	resLoadResourcesFromDisk(g_hCharacterBackgroundGroupDict, "defs/classes", ".background",  "CharacterBackgrounds.bin", PARSER_OPTIONALFLAG);
	//validate references
#ifndef GAMECLIENT
	{
		int i = 0;
		int j = 0;
		RefDictIterator iter;
		CharacterBackgroundGroup* pGroup = NULL;

		RefSystem_InitRefDictIterator(g_hCharacterBackgroundGroupDict, &iter);
		while (pGroup = (CharacterBackgroundGroup*)RefSystem_GetNextReferentFromIterator(&iter))
		{
			if (IS_HANDLE_ACTIVE(pGroup->pDisplayName.hMessage) && !TranslateDisplayMessage(pGroup->pDisplayName))
			{
				ErrorFilenamef(pGroup->pchFile, "Invalid DisplayName \"%s\" in character background group %s", REF_STRING_FROM_HANDLE(pGroup->pDisplayName.hMessage), pGroup->pchName);
			}
			if (IS_HANDLE_ACTIVE(pGroup->pFlavorText.hMessage) && !TranslateDisplayMessage(pGroup->pFlavorText))
			{
				ErrorFilenamef(pGroup->pchFile, "Invalid FlavorText \"%s\" in character background group %s", REF_STRING_FROM_HANDLE(pGroup->pFlavorText.hMessage), pGroup->pchName);
			}
			for (i = 0; i < eaSize(&pGroup->ppchGrantedItems); i++)
			{
				if (!RefSystem_ReferentFromString(g_hItemDict, pGroup->ppchGrantedItems[i]))
				{
					ErrorFilenamef(pGroup->pchFile, "Invalid GrantedItem %s in character background group %s", pGroup->ppchGrantedItems[i], pGroup->pchName);
				}
			}
			for (i = 0; i < eaSize(&pGroup->ppBackgrounds); i++)
			{
				CharacterBackgroundChoice* pChoice = pGroup->ppBackgrounds[i];
				if (IS_HANDLE_ACTIVE(pChoice->pDisplayName.hMessage) && !TranslateDisplayMessage(pChoice->pDisplayName))
				{
					ErrorFilenamef(pGroup->pchFile, "Invalid DisplayName \"%s\" in character background %s.%s", REF_STRING_FROM_HANDLE(pChoice->pDisplayName.hMessage), pGroup->pchName, pChoice->pchName);
				}
				if (IS_HANDLE_ACTIVE(pChoice->pFlavorText.hMessage) && !TranslateDisplayMessage(pChoice->pFlavorText))
				{
					ErrorFilenamef(pGroup->pchFile, "Invalid FlavorText \"%s\" in character background %s.%s", REF_STRING_FROM_HANDLE(pChoice->pFlavorText.hMessage), pGroup->pchName, pChoice->pchName);
				}
				if (IS_HANDLE_ACTIVE(pChoice->pBiographyText.hMessage) && !TranslateDisplayMessage(pChoice->pBiographyText))
				{
					ErrorFilenamef(pGroup->pchFile, "Invalid BiographyText \"%s\" in character background %s.%s", REF_STRING_FROM_HANDLE(pChoice->pBiographyText.hMessage), pGroup->pchName, pChoice->pchName);
				}
				for (j = 0; j < eaSize(&pChoice->ppchGrantedItems); j++)
				{
					if (!RefSystem_ReferentFromString(g_hItemDict, pChoice->ppchGrantedItems[j]))
					{
						ErrorFilenamef(pGroup->pchFile, "Invalid GrantedItem %s in character background %s.%s", pChoice->ppchGrantedItems[j], pGroup->pchName, pChoice->pchName);
					}
				}
				for (j = 0; j < eaSize(&pChoice->ppchPowerTreeNodeChoices); j++)
				{
					if (!RefSystem_ReferentFromString(g_hPowerTreeNodeDefDict, pChoice->ppchPowerTreeNodeChoices[j]))
					{
						ErrorFilenamef(pGroup->pchFile, "Invalid PowerTreeNode %s in character background %s.%s", pChoice->ppchPowerTreeNodeChoices[j], pGroup->pchName, pChoice->pchName);
					}
				}
			}
		}
	}
#endif

}

