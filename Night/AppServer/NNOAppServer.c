
#include "../common/NNOCharacterBackground.h"
#include "inventoryCommon.h"
#include "PowerTree.h"
#include "PowerTreeHelpers.h"
#include "../../Crossroads/common/LoginCommon.h"
#include "partition_enums.h"
#include "error.h"
#include "LoginServer/aslLoginCharacterSelect.h"
#include "Entity.h"
#include "Player.h"
#include "LoginServer/aslLoginServer.h"
#include "LoginCommon.h"
#include "objTransactions.h"
#include "AutoTransDefs.h"
#include "Login2Common.h"

#include "AutoGen/Entity_h_ast.h"
#include "AutoGen/Player_h_ast.h"
#include "AutoGen/NNOAppServer_autotransactions_autogen_wrappers.h"

typedef struct PossibleCharacterChoice PossibleCharacterChoice;
typedef struct GameAccountDataExtract GameAccountDataExtract;
extern DictionaryHandle g_hCharacterBackgroundGroupDict;
extern DictionaryHandle g_hItemDict;
extern bool g_isContinuousBuilder;

//Runs on appserver, applies the background choices to the entity at character creation time.
bool CharacterBackground_DoNewCharacterInitialization(NOCONST(Entity) *ent, Login2CharacterCreationData *characterCreationData, GameAccountDataExtract *pExtract)
{
	char *pchParsed = NULL;
	char *pchGroupName = NULL;
	char *pchChoiceName = NULL;
	CharacterBackgroundGroup* pGroup = NULL;
	CharacterBackgroundChoice* pBackground = NULL;
	int i;


	estrStackCreate(&pchParsed);
    estrCopy2(&pchParsed, characterCreationData->gameSpecificChoice);
	pchGroupName = pchParsed;
	pchChoiceName = strchr(pchParsed, '.');
	if (pchChoiceName)
	{
		*pchChoiceName = '\0';
		pchChoiceName++;
	}

	pGroup = RefSystem_ReferentFromString(g_hCharacterBackgroundGroupDict, pchGroupName);
	if (pGroup)
	{
		for (i = 0; i < eaSize(&pGroup->ppBackgrounds); i++)
		{
			if (strcmp(pGroup->ppBackgrounds[i]->pchName, pchChoiceName)==0)
			{
				pBackground = pGroup->ppBackgrounds[i];
				break;
			}
		}
	}
	if (!pBackground || !pGroup)
	{
		if (!g_isContinuousBuilder)
		{
			if (characterCreationData->gameSpecificChoice && characterCreationData->gameSpecificChoice[0])
				Errorf("Loginserver couldn't find character background %s.", characterCreationData->gameSpecificChoice);
			else
				Errorf("No character background specified for new character.");
		}
		return true;
		//this will return false once we are ready to make character creation require a background.
	}
	for (i = 0;i < eaSize(&pGroup->ppchGrantedItems);i++)
	{
		ItemDef* pDef = RefSystem_ReferentFromString(g_hItemDict, pGroup->ppchGrantedItems[i]);
		inv_ent_trh_AddItemFromDef(ATR_EMPTY_ARGS,ent,NULL,inv_trh_GetBestBagForItemDef(ent, pDef, NULL, 1, true, pExtract),-1,pGroup->ppchGrantedItems[i],1, 0, 0, ItemAdd_Silent, NULL, pExtract);
	}
	for (i = 0;i < eaSize(&pBackground->ppchGrantedItems);i++)
	{
		ItemDef* pDef = RefSystem_ReferentFromString(g_hItemDict, pBackground->ppchGrantedItems[i]);
		inv_ent_trh_AddItemFromDef(ATR_EMPTY_ARGS,ent,NULL,inv_trh_GetBestBagForItemDef(ent, pDef, NULL, 1, true, pExtract),-1,pBackground->ppchGrantedItems[i],1, 0, 0, ItemAdd_Silent, NULL, pExtract);
	}
	for (i = 0; i < eaSize(&pBackground->ppchPowerTreeNodeChoices); i++)
	{
		const char *pchNode = pBackground->ppchPowerTreeNodeChoices[i];
		if (pchNode && *pchNode)
		{
			PTNodeDef* pNodeDef = powertreenodedef_Find(pchNode);
			PowerTreeDef* pTreeDef = powertree_TreeDefFromNodeDef(pNodeDef);
			if (!(pTreeDef && pNodeDef))
			{
				return false;
			}
			else
			{
				NOCONST(PTNode) *pNode = entity_PowerTreeNodeIncreaseRankHelper(PARTITION_IN_TRANSACTION, ent, NULL, pTreeDef->pchName, pchNode, false, false, false, NULL);
				if (!pNode)
				{
					return false;
				}
			}
		}
	}
	return true;
}


bool OVERRIDE_LATELINK_gameSpecific_PostInitNewCharacter(NOCONST(Entity) *ent, Login2CharacterCreationData* characterCreationData, GameAccountDataExtract* pExtract)
{
	return CharacterBackground_DoNewCharacterInitialization(ent, characterCreationData, pExtract);
}

bool OVERRIDE_LATELINK_gameSpecific_PreInitNewCharacter(NOCONST(Entity) *ent, Login2CharacterCreationData* characterCreationData, GameAccountDataExtract* pExtract)
{
	return true;
}

AUTO_TRANSACTION
ATR_LOCKS(pEnt, ".Pplayer.Playertype");
enumTransactionOutcome trPlayerTypeConversion(ATR_ARGS, NOCONST(Entity)* pEnt, int newPlayerTypeInt)
{
    ItemChangeReason reason = {0};
    PlayerType newPlayerType = (PlayerType)newPlayerTypeInt;

    if(asl_trh_ValidPlayerTypeConversion(ATR_PASS_ARGS,pEnt,newPlayerType)!=TRANSACTION_OUTCOME_SUCCESS)
        return TRANSACTION_OUTCOME_FAILURE;

    if(ISNULL(pEnt->pPlayer))
        TRANSACTION_RETURN_LOG_FAILURE("Cannot convert non-players");

    // Actually change the type
    pEnt->pPlayer->playerType = newPlayerType;

    return TRANSACTION_OUTCOME_SUCCESS;
}

// Handles project-specific part of Character PlayerType conversion, should return
//  false if something went wrong.
S32 OVERRIDE_LATELINK_login_ConvertCharacterPlayerType(Entity *pEnt, int accountPlayerType, TransactionReturnCallback cbFunc, void *cbData)
{
    TransactionReturnVal *pReturn;
    bool bReturnValue = true;

    if( !pEnt || !pEnt->pPlayer || !entGetAccountID(pEnt) )
    {
        return false;
    }

    // Can't convert to the same type
    if(pEnt->pPlayer->playerType == accountPlayerType)
    {
        //Cannot convert this character again
        return false;
    }

    if( accountPlayerType == kPlayerType_Premium || accountPlayerType == kPlayerType_Standard )
    {
        // Call actual transaction and return true
        pReturn = objCreateManagedReturnVal(cbFunc, cbData);
        AutoTrans_trPlayerTypeConversion(pReturn,GetAppGlobalType(), entGetType(pEnt),entGetContainerID(pEnt), accountPlayerType);

        bReturnValue = true; //Already true, but added for emphasis
    }
    else
    {
        //Cannot convert to unknown types
        bReturnValue = false;
    }

    return bReturnValue;
}