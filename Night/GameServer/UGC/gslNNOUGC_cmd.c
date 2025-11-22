#include "gslNNOUGC.h"

#include "Entity.h"
#include "Character.h"
#include "CharacterRespecServer.h"
#include "NNOUGCCommon.h"
#include "NNOUGCResource.h"
#include "Reward.h"
#include "UGCError.h"
#include "UGCProjectCommon.h"
#include "UGCProjectCommon_h_ast.h"
#include "cmdServerCharacter.h"
#include "file.h"
#include "gslUgcTransactions.h"
#include "itemServer.h"
#include "inventoryCommon.h"
#include "itemTransaction.h"

#include "AutoGen/GameServerLib_autotransactions_autogen_wrappers.h"

static void FreezeUGCProjectFixupPrompt(UGCDialogTreePrompt *prompt, const char *oldName, const char *newName)
{
	if (resNamespaceBaseNameEq(prompt->pcPromptCostume, oldName))
	{
		StructCopyString(&prompt->pcPromptCostume, newName);
	}
}

static void FreezeUGCProjectFixupPromptBlock(UGCDialogTreeBlock *block, const char *oldName, const char *newName)
{
	FreezeUGCProjectFixupPrompt(&block->initialPrompt, oldName, newName);
	FOR_EACH_IN_EARRAY(block->prompts, UGCDialogTreePrompt, prompt)
	{
		FreezeUGCProjectFixupPrompt(prompt, oldName, newName);
	}
	FOR_EACH_END;
}

void DoFreezeUGCProject(Entity *pEntity, UGCProjectData *data, UGCFreezeProjectInfo *pInfo)
{
	InfoForUGCProjectSaveOrPublish *pInfoForPublish = NULL;
	if (isProductionMode())
		return;

	if (!data)
	{
		//ClientCmd_UGCEditorUpdatePublishStatus(pEntity, false, "No UGCProjectData");
		return;
	}

	// Fixup map names
	FOR_EACH_IN_EARRAY(pInfo->eaMaps, UGCFreezeProjectMapInfo, pMapInfo)
	{
		// Find map
		UGCMap *pMap = NULL;
		FOR_EACH_IN_EARRAY(data->maps, UGCMap, map)
		{
			if (pMapInfo->astrInternalMapName == map->pcName)
			{
				pMap = map;
				break;
			}
		}
		FOR_EACH_END;
		if (!pMap)
		{
			// Error!
			return;
		}

		FOR_EACH_IN_EARRAY(data->components->eaComponents, UGCComponent, component)
		{
			if (resNamespaceBaseNameEq(component->sPlacement.pcMapName, pMap->pcName))
			{
				StructCopyString(&component->sPlacement.pcMapName, pMapInfo->pcOutMapName);
			}
		}
		FOR_EACH_END;
		pMap->pcName = allocAddString(pMapInfo->pcOutMapName);
	}
	FOR_EACH_END;

	// Fixup costumes
	FOR_EACH_IN_EARRAY(pInfo->eaCostumes, UGCFreezeProjectCostumeInfo, pCostumeInfo)
	{
		// Find map
		UGCCostume *pCostume = NULL;
		FOR_EACH_IN_EARRAY(data->costumes, UGCCostume, costume)
		{
			if (pCostumeInfo->astrInternalCostumeName == costume->astrName)
			{
				pCostume = costume;
				break;
			}
		}
		FOR_EACH_END;
		if (!pCostume)
		{
			// Error!
			return;
		}

		FOR_EACH_IN_EARRAY(data->components->eaComponents, UGCComponent, component)
		{
			if (resNamespaceBaseNameEq(component->pcPromptCostumeName, pCostume->astrName))
			{
				StructCopyString(&component->pcPromptCostumeName, pCostumeInfo->pcOutCostumeName);
			}
			if (resNamespaceBaseNameEq(component->pcCostumeName, pCostume->astrName))
			{
				StructCopyString(&component->pcCostumeName, pCostumeInfo->pcOutCostumeName);
			}
			FreezeUGCProjectFixupPromptBlock(&component->dialogBlock, pCostume->astrName, pCostumeInfo->pcOutCostumeName);
			FOR_EACH_IN_EARRAY(component->blocksV1, UGCDialogTreeBlock, block)
			{
				FreezeUGCProjectFixupPromptBlock(block, pCostume->astrName, pCostumeInfo->pcOutCostumeName);
			}
			FOR_EACH_END;
		}
		FOR_EACH_END;
		pCostume->astrName = allocAddString(pCostumeInfo->pcOutCostumeName);
	}
	FOR_EACH_END;

	// Do validate of the project
	{
		UGCRuntimeStatus* status = StructCreate( parse_UGCRuntimeStatus );
		
		ugcSetStageAndAdd( status, "UGC Validate" );
		ugcValidateProject( data );

		if( ugcValidateErrorfIfStatusHasErrors( status ))
		{
			StructDestroySafe( parse_UGCRuntimeStatus, &status );
			//ClientCmd_UGCEditorUpdatePublishStatus(pEntity, false, "Project has errors");
			return;
		}
		
		StructDestroySafe( parse_UGCRuntimeStatus, &status );
	}
	
	pInfoForPublish = StructCreate(parse_InfoForUGCProjectSaveOrPublish);
	estrDestroy(&pInfoForPublish->pPublishNameSpace);
	StructCopyString(&data->project_prefix, pInfo->pcProjectPrefix);

	// Rename this map and save the new resources
	if (!gslUGC_ForkNamespace(data, pInfoForPublish, false))
	{
		StructDestroy(parse_InfoForUGCProjectSaveOrPublish, pInfoForPublish);
		//ClientCmd_UGCEditorUpdatePublishStatus(pEntity, false, "gslUGC_ForkNamespace failed");
		return;
	}

	StructDestroy(parse_InfoForUGCProjectSaveOrPublish, pInfoForPublish);

	//ClientCmd_UGCEditorUpdatePublishStatus(pEntity, true, NULL);
}

typedef struct PlayerRespecCBData
{
	EntityRef erEnt;
	
	const char *pchClassName;

} PlayerRespecCBData;

// Called when the set level transaction is completed
static void gslUGC_SetExpLevelThenRespecCallback(TransactionReturnVal* returnVal, PlayerRespecCBData *pCBData)
{
	Entity *pEntity = entFromEntityRefAnyPartition(pCBData->erEnt);

	// Make sure the entity is still accessible and the transaction was successful
	if (pEntity && pEntity->pChar && returnVal && returnVal->eOutcome == TRANSACTION_OUTCOME_SUCCESS)
	{ 
		// we have to set the combet level here so that the character_tick doesn't see the level has changed 
		// and try to autobuy powerTree nodes. We are going to be potentially changing our class and powertrees
		// so we want to autobuy powertree nodes once we are done with the respec
		pEntity->pChar->iLevelCombat = entity_GetSavedExpLevel(pEntity);
		
		PlayerRespecAsArchetype( pEntity, pCBData->pchClassName);
	}

	// Clean up
	free(pCBData);
}

static void gslUGC_SetExpLevelThenRespec(Entity *pEntity, const char* pchArchetype, int iLevelValue)
{
	if ( pEntity )
	{
		PlayerRespecCBData *pCBData = malloc(sizeof(PlayerRespecCBData));

		F32 iNumericValue = NUMERIC_AT_LEVEL(iLevelValue);
		ItemChangeReason reason = {0};

		pCBData->erEnt = entGetRef(pEntity);
		pCBData->pchClassName = pchArchetype;
		
		inv_FillItemChangeReason(&reason, pEntity, "Internal:SetExpLevelUsingCharacterPath", NULL);

		itemtransaction_SetNumeric(pEntity, gConf.pcLevelingNumericItem, iNumericValue, 
									&reason, gslUGC_SetExpLevelThenRespecCallback, pCBData);
	}
}

void gslUGC_DoRespecCharacter( Entity* ent, int allegianceDefaultsIndex, const char* className, int levelValue )
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	UGCPerAllegianceDefaults* allegianceDefaults = eaGet( &defaults->allegiance, allegianceDefaultsIndex );

	if( allegianceDefaults ) 
	{
		int classIt;
		int levelIt;

		className = allocFindString( className );
	
		for( classIt = 0; classIt != eaSize( &allegianceDefaults->respecClasses ); ++classIt ) 
		{
			UGCRespecClass* class = allegianceDefaults->respecClasses[ classIt ];
			if( REF_STRING_FROM_HANDLE( class->hRespecClassName ) == className ) 
			{
				for( levelIt = 0; levelIt != eaSize( &class->eaLevels ); ++levelIt ) 
				{
					UGCRespecClassLevel* level = class->eaLevels[ levelIt ];
					if( level->iLevel == levelValue ) 
					{
						gslUGC_SetExpLevelThenRespec(ent, REF_STRING_FROM_HANDLE(class->hRespecClassName), level->iLevel);
						
						InventoryClear( ent );
						GrantRewardTable( ent, REF_STRING_FROM_HANDLE( level->hRewardTable ));
						return;
					}
				}
			}
		}
	}
}
