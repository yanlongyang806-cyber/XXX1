//// UGC user-facing resources
////
//// UGC has a collection of resources that it uses to create
//// underlying missions, ZoneMaps, costumes, etc.  This is where you
//// can place all the UGC-specific dictionaries. These structures should
//// only be used in the UGC editor and to generate the underlying Genesis
//// structures; they are never used in generation or loaded directly during
//// the game.

#include "NNOUGCResource.h"

#include "CombatEnums.h"
#include "CostumeCommon.h"
#include "File.h"
#include "GlobalTypes.h"
#include "ReferenceSystem.h"
#include "ResourceManager.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "wlUGC.h"

#include "AutoGen/UGCProjectCommon_h_ast.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_GameSystems););

/// Resource dictionary setup code

DictionaryHandle g_UGCSoundDictionary		= NULL;
DictionaryHandle g_UGCSoundDSPDictionary	= NULL;
DictionaryHandle g_UGCTrapPowerGroupDictionary = NULL;

static int ugcSoundValidateCB(enumResourceValidateType eType, const char *pDictName, const char *pResourceName, void *pResource, U32 userID)
{
	return VALIDATE_NOT_HANDLED;
}

static int ugcSoundDSPValidateCB(enumResourceValidateType eType, const char *pDictName, const char *pResourceName, void *pResource, U32 userID)
{
	return VALIDATE_NOT_HANDLED;
}

static int ugcTrapPowerGroupValidateCB(enumResourceValidateType eType, const char *pDictName, const char *pResourceName, void *pResource, U32 userID)
{
	return VALIDATE_NOT_HANDLED;
}

AUTO_RUN;
void ugcResourceRegisterDictionaries(void)
{
	g_UGCSoundDictionary = RefSystem_RegisterSelfDefiningDictionary(UGC_DICTIONARY_SOUND, false, parse_UGCSound, true, true, NULL);
	g_UGCSoundDSPDictionary = RefSystem_RegisterSelfDefiningDictionary(UGC_DICTIONARY_SOUND_DSP, false, parse_UGCSoundDSP, true, true, NULL);
	if(IsServer()) {
		resDictProvideMissingResources( g_UGCSoundDictionary );
		resDictManageValidation( g_UGCSoundDictionary, ugcSoundValidateCB );

		resDictProvideMissingResources( g_UGCSoundDSPDictionary );
		resDictManageValidation( g_UGCSoundDSPDictionary, ugcSoundDSPValidateCB );
	}
	else if(IsClient()) {
		resDictRequestMissingResources( g_UGCSoundDictionary, 8, false, resClientRequestSendReferentCommand );
		resDictRequestMissingResources( g_UGCSoundDSPDictionary, 8, false, resClientRequestSendReferentCommand );
	}

	g_UGCTrapPowerGroupDictionary = RefSystem_RegisterSelfDefiningDictionary(UGC_DICTIONARY_TRAP_POWER_GROUP, false, parse_UGCTrapPowerGroup, true, true, NULL);
	if(IsServer()) {
		resDictProvideMissingResources( g_UGCTrapPowerGroupDictionary );
		resDictManageValidation( g_UGCTrapPowerGroupDictionary, ugcTrapPowerGroupValidateCB );
	}
	else if(IsClient()) {
		resDictRequestMissingResources( g_UGCTrapPowerGroupDictionary, 8, false, resClientRequestSendReferentCommand );
	}
}



void ugcResourceLoadLibrary( void )
{
	static bool loaded = false;

	if( loaded ) {
		return;
	}

	if (!RefSystem_DoesDictionaryExist("SkyInfo"))
		createServerSkyDictionary(); // need a SkyInfo dictionary for the ZoneMaps to parse correctly
	
	// Resources specific to UGC (not player-created)

	resLoadResourcesFromDisk(g_UGCSoundDictionary, NULL, "genesis/ugc_sounds.txt", "UGCSounds.bin", 
		PARSER_OPTIONALFLAG | PARSER_BINS_ARE_SHARED );

	resLoadResourcesFromDisk(g_UGCSoundDSPDictionary, NULL, "genesis/ugc_sound_dsps.txt", "UGCSoundDSPs.bin", 
		PARSER_OPTIONALFLAG | PARSER_BINS_ARE_SHARED );

	resLoadResourcesFromDisk(g_UGCTrapPowerGroupDictionary, NULL, "genesis/ugc_trap_power_groups.txt", "UGCTrapPowerGroups.bin", 
		PARSER_OPTIONALFLAG | PARSER_BINS_ARE_SHARED );

	loaded = true;
}

UGCProjectInfo* ugcProjectDataGetProjectInfo( UGCProjectData* ugcProj )
{
	return ugcProj->project;
}

const char* ugcProjectDataGetNamespace( UGCProjectData* ugcProj )
{
	return ugcProj->ns_name;
}

void ugcProjectDataGetInitialMapAndSpawn( const char** out_strInitialMapName, const char** out_strInitialSpawnPoint, UGCProjectData* ugcProj )
{
	*out_strInitialMapName = ugcProj->mission->strInitialMapName;
	*out_strInitialSpawnPoint = ugcProj->mission->strInitialSpawnPoint;
}

char **ugcProjectDataGetMaps( UGCProjectData* ugcProj )
{
	char **ppMapNames = NULL;
	FOR_EACH_IN_EARRAY(ugcProj->maps, UGCMap, map)
	{
		char userNameSpace[RESOURCE_NAME_MAX_SIZE], baseName[RESOURCE_NAME_MAX_SIZE];
		if(resExtractNameSpace(map->pcName, userNameSpace, baseName))
			eaPush(&ppMapNames, strdup(baseName));
	}
	FOR_EACH_END;
	return ppMapNames;
}

// This function is used only by STO.
void ugcProjectDataGetSTOGrantPrompt( const char** out_strCostumeName, const char** out_strPetCostume, const char** out_strBodyText, UGCProjectData* ugcProj )
{
	*out_strCostumeName = ugcProj->mission->sGrantPrompt.pcPromptCostume;
	*out_strPetCostume = REF_STRING_FROM_HANDLE( ugcProj->mission->sGrantPrompt.hPromptPetCostume );
	*out_strBodyText = ugcProj->mission->sGrantPrompt.pcPromptBody;
}

static bool ugcResource_GetAudioAssets_HandleString(const char *pcAddString, const char ***peaStrings)
{
	if (pcAddString)
	{
		bool bDup = false;
		FOR_EACH_IN_EARRAY(*peaStrings, const char, pcHasString) {
			if (strcmpi(pcHasString, pcAddString) == 0) {
				bDup = true;
			}
		} FOR_EACH_END;
		if (!bDup) {
			eaPush(peaStrings, strdup(pcAddString));
		}
		return true;
	}
	return false;
}

void ugcResource_GetAudioAssets(const char **ppcType, const char ***peaStrings, U32 *puiNumData, U32 *puiNumDataWithAudio)
{
	UGCSound *pUGCSound;
	ResourceIterator rI;

	ugcResourceLoadLibrary();

	*ppcType = strdup("UGCSound");

	resInitIterator(g_UGCSoundDictionary, &rI);
	while (resIteratorGetNext(&rI, NULL, &pUGCSound))
	{
		*puiNumData = *puiNumData + 1;
		if (ugcResource_GetAudioAssets_HandleString(pUGCSound->strSoundName, peaStrings)) {
			*puiNumDataWithAudio = *puiNumDataWithAudio + 1;
		}
	}
	resFreeIterator(&rI);
}

static void fixupUGCComponentListStashTable( UGCComponentList* pComponents )
{
	if( !pComponents->stComponentsById ) {
		pComponents->stComponentsById = stashTableCreateInt( eaSize( &pComponents->eaComponents ));
	}
	stashTableClear( pComponents->stComponentsById );

	FOR_EACH_IN_EARRAY_FORWARDS( pComponents->eaComponents, UGCComponent, component ) {
		stashIntAddPointer( pComponents->stComponentsById, component->uID, component, true );
	} FOR_EACH_END;
}

TextParserResult fixupUGCComponentList( UGCComponentList* pComponents, enumTextParserFixupType eType, void* pExtraData )
{
	switch( eType ) {
		xcase FIXUPTYPE_POST_TEXT_READ: case FIXUPTYPE_POST_BIN_READ:
			fixupUGCComponentListStashTable( pComponents );

		// FIXUPTYPE_POST_STRUCTCOPY, FIXUPTYPE_PRE_STRUCTCOPY_DEST
		// are both ugly #define's that needs to be its own case.
		xcase FIXUPTYPE_PRE_STRUCTCOPY_DEST:
			if( pComponents->stComponentsById ) {
				stashTableDestroy( pComponents->stComponentsById );
				pComponents->stComponentsById = NULL;
			}
			
		xcase FIXUPTYPE_POST_STRUCTCOPY:
			pComponents->stComponentsById = NULL;
			fixupUGCComponentListStashTable( pComponents );

		xcase FIXUPTYPE_DESTRUCTOR:
			if( pComponents->stComponentsById ) {
				stashTableDestroy( pComponents->stComponentsById );
				pComponents->stComponentsById = NULL;
			}
	}
	
	return PARSERESULT_SUCCESS;
}

#include "NNOUGCResource_h_ast.c"
