//// UGC common routines
////
//// Structures & functions internally used by the UGC system by both client
//// and server.
#include "NNOUGCCommon.h"

#include "AutoTransDefs.h"
#include "Color.h"
#include "CostumeCommon.h"
#include "CostumeCommonLoad.h"
#include "CostumeCommonTailor.h"
#include "FolderCache.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCResource.h"
#include "Quat.h"
#include "ResourceInfo.h"
#include "ResourceSearch.h"
#include "StateMachine.h"
#include "StaticWorld/WorldGridLoadPrivate.h"
#include "StaticWorld/grouputil.h"
#include "StringCache.h"
#include "StringFormat.h"
#include "StringUtil.h"
#include "SubStringSearchTree.h"
#include "TextFilter.h"
#include "TokenStore.h"
#include "UGCAchievements.h"
#include "UGCCommon.h"
#include "UGCCommon_h_ast.h"
#include "NNOUGCInteriorCommon.h"
#include "UGCInteriorCommon.h"
#include "UGCProjectCommon.h"
#include "UGCProjectUtils.h"
#include "UtilitiesLib.h"
#include "WorldGrid.h"
#include "beaconFile.h"
#include "encounter_common.h"
#include "entCritter.h"
#include "file.h"
#include "fileutil.h"
#include "fileutil2.h"
#include "logging.h"
#include "mathutil.h"
#include "structInternals.h"
#include "textparser.h"
#include "timing.h"
#include "wlExclusionGrid.h"
#include "wlUGC.h"
#include "allegiance.h"

#include "CostumeCommon_h_ast.h" 
#include "UGCProjectCommon_h_ast.h"

#include "CombatEnums.h"
#include "crypt.h"
#include "sysUtil.h"

#ifdef GAMESERVER
#include "objTransactions.h"
#endif

#ifdef GAMECLIENT
#include "UIGen.h"
#endif

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

int ugcPerfDebug = false;
DictionaryHandle g_UGCResourceInfoDict = NULL;
const char* g_UGCMissionName = "UGC";
const char* g_UGCMissionNameCompleted = "UGC_Completed";


static void ugcValidateGroupDef( UGCResourceInfo* resInfo, GroupDef* def );
static void ugcValidateGroupDefLate( UGCResourceInfo* resInfo, GroupDef* def );
static void ugcValidateZoneMapInfo( UGCResourceInfo* resInfo, ZoneMapInfo* zmapInfo );

//// UGC Resource info

bool bUGCResourceValidation = false;
AUTO_CMD_INT( bUGCResourceValidation, UGCResourceValidation ) ACMD_CMDLINE;


static void ugcResourceInfoValidate( UGCResourceInfo* resInfo )
{
	const char* displayedResourceName = resInfo->pResInfo->resourceName;

	if (ugcHasTag(resInfo->pResInfo->resourceTags, "donotvalidate"))
	{
		// Special case (perhaps the editing map) Do not run validation
		return;
	}

	if( resInfo->pResInfo->resourceDict == allocFindString( "ObjectLibrary" )) {
		GroupDef* def = objectLibraryGetGroupDefByName( displayedResourceName, false );
		if( def ) {
			displayedResourceName = def->name_str;
			ugcValidateGroupDef( resInfo, def );
		}
	}

	if( resInfo->pResInfo->resourceDict == allocFindString( "ZoneMap" )) {
		ZoneMapInfo* zmapInfo = zmapInfoGetByPublicName( displayedResourceName );
		if( zmapInfo ) {
			ugcValidateZoneMapInfo( resInfo, zmapInfo );
		}
	}

	if( !resInfo->pUGCProperties ) {
		Errorf( "UGC tagged %s %s does not appear to have any UGC properties.",
				resInfo->pResInfo->resourceDict, displayedResourceName );
	} else if( bUGCResourceValidation ) {
		if( !IS_HANDLE_ACTIVE( resInfo->pUGCProperties->dVisibleName.hMessage )) {
			Errorf( "UGC tagged %s %s does not have a VisibleName set.",
					resInfo->pResInfo->resourceDict, displayedResourceName );
		}
		if(   !resInfo->pUGCProperties->bNoDescription
			  && resInfo->pResInfo->resourceDict != allocFindString( "Texture" )
			  && resInfo->pResInfo->resourceDict != allocFindString( "PlayerCostume" )
			  && !IS_HANDLE_ACTIVE( resInfo->pUGCProperties->dDescription.hMessage )) {
			Errorf( "UGC tagged %s %s does not have a Description set.",
					resInfo->pResInfo->resourceDict, displayedResourceName );
		}
	}
}

/// Late validation function, for checking references between
/// dictionaries.
static void ugcResourceInfoValidateLate( UGCResourceInfo* resInfo )
{
	if( ugcHasTag( resInfo->pResInfo->resourceTags, "donotvalidate" )) {
		return;
	}

	if( resInfo->pResInfo->resourceDict == allocFindString( "ObjectLibrary" )) {
		GroupDef* def = objectLibraryGetGroupDefByName( resInfo->pResInfo->resourceName, false );
		if( def ) {
			ugcValidateGroupDefLate( resInfo, def );
		}
	}
}

static int ugcResourceInfoValidateCB( enumResourceValidateType type, const char* dictName, const char* resourceName, UGCResourceInfo *resInfo, U32 userID )
{
	switch( type ) {
		// Post text reading: do fixup that should happen before binning.  Generate expressions, do most post-processing here
		xcase RESVALIDATE_POST_TEXT_READING:
			ugcResourceInfoValidate( resInfo );
			return VALIDATE_HANDLED;

		// Post binning: gets run each time read from bin.  Populate NO_AST fields here
		xcase RESVALIDATE_POST_BINNING:

		// Final location: after moving to shared memory.  Fix up pointers here
		xcase RESVALIDATE_FINAL_LOCATION:

		xcase RESVALIDATE_CHECK_REFERENCES:
			ugcResourceInfoValidateLate( resInfo );
			return VALIDATE_HANDLED;
	}

	return VALIDATE_NOT_HANDLED;
}

AUTO_RUN_LATE;
void ugcRegisterDictionary( void )
{
	DictionaryHandle ugcAccountDict = NULL;

	g_UGCResourceInfoDict = RefSystem_RegisterSelfDefiningDictionary( UGC_DICTIONARY_RESOURCE_INFO, false, parse_UGCResourceInfo, true, true, NULL );
	resDictManageValidation( g_UGCResourceInfoDict, ugcResourceInfoValidateCB );
	resDictMaintainInfoIndex( g_UGCResourceInfoDict, NULL, NULL, ".ResInfo.ResourceTags", NULL, NULL );

#if !defined(GAMESERVER)
	ugcAccountDict = RefSystem_RegisterSelfDefiningDictionary( GlobalTypeToCopyDictionaryName( GLOBALTYPE_UGCACCOUNT ), false, parse_UGCAccount, false, false, NULL );
	resDictProvideMissingResources( ugcAccountDict );
	resDictRequestMissingResources( ugcAccountDict, RES_DICT_KEEP_NONE, true, resClientRequestSendReferentCommand );
#endif
}

typedef void (*UGCResourceInfoAddDataFn)( UGCResourceInfo* resourceInfo );

AUTO_STRUCT;
typedef struct UGCResourceInfoDef {
	const char *logical_name;				AST( NAME(LogicalName) POOL_STRING STRUCTPARAM )
	const char *dictionary_name;			AST( NAME(Dictionary) POOL_STRING )
	const char *tags;						AST( NAME(Tags) POOL_STRING )
	WorldUGCProperties ugcProps;			AST( EMBEDDED_FLAT )
} UGCResourceInfoDef;
extern ParseTable parse_UGCResourceInfoDef[];
#define TYPE_parse_UGCResourceInfoDef UGCResourceInfoDef

AUTO_STRUCT;
typedef struct UGCResourceInfoDefList {
	UGCResourceInfoDef **resources;	AST( NAME(Resource) )
	UGCResourceInfoDef **fallbacks;	AST( NAME(Fallback) )
} UGCResourceInfoDefList;
extern ParseTable parse_UGCResourceInfoDefList[];
#define TYPE_parse_UGCResourceInfoDefList UGCResourceInfoDefList

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void ugcResourceInfoGenerateActorProperties(GroupDef *group, UGCResourceInfo *ugcInfo)
{
	int actor_idx;
	eaDestroyStruct(&ugcInfo->pUGCProperties->groupDefProps.eaEncounterActors, parse_WorldUGCActorProperties);
	if (group->property_structs.encounter_properties)
	{
		EncounterTemplate *enctemplate = GET_REF(group->property_structs.encounter_properties->hTemplate);
		while (enctemplate && eaSize(&enctemplate->eaActors) == 0)
			enctemplate = GET_REF(enctemplate->hParent);
		if (!enctemplate)
			return;
		for (actor_idx = 0; actor_idx < eaSize(&enctemplate->eaActors); actor_idx++)
		{
			CritterDef *critter_def = GET_REF(enctemplate->eaActors[actor_idx]->critterProps.hCritterDef);
			if(critter_def)
			{
				int costume_idx;
				WorldUGCActorProperties *actor_properties = StructCreate(parse_WorldUGCActorProperties);
				CritterGroup *critter_group_def = GET_REF(critter_def->hGroup);
				actor_properties->pcRankName = critter_def->pcRank;
				actor_properties->pcClass = StructAllocString(critter_def->pchClass);
				StructCopyAll(parse_DisplayMessage, &critter_def->displayNameMsg, &actor_properties->displayNameMsg);

				if(isValidMessage(GET_REF(critter_def->hGroupOverrideDisplayNameMsg.hMessage)))
					StructCopyAll(parse_DisplayMessage, &critter_def->hGroupOverrideDisplayNameMsg, &actor_properties->groupDisplayNameMsg);
				else if(critter_group_def)
					StructCopyAll(parse_DisplayMessage, &critter_group_def->displayNameMsg, &actor_properties->groupDisplayNameMsg);

				for(costume_idx = 0; costume_idx < eaSize(&critter_def->ppCostume); costume_idx++)
				{
					WorldUGCActorCostumeProperties *pCostume = StructCreate(parse_WorldUGCActorCostumeProperties);
					PlayerCostume *pPlayerCostume = GET_REF(critter_def->ppCostume[costume_idx]->hCostumeRef);
					COPY_HANDLE(pCostume->hCostumeRef, critter_def->ppCostume[costume_idx]->hCostumeRef);
					pCostume->pcCostumeName = StructAllocString(pPlayerCostume->pcName);
					eaPush(&actor_properties->eaCostumes, pCostume);
				}
				eaPush(&ugcInfo->pUGCProperties->groupDefProps.eaEncounterActors, actor_properties);
			}
			else
			{
				WorldUGCActorProperties *actor_properties = StructCreate(parse_WorldUGCActorProperties);
				actor_properties->pcRankName = enctemplate->eaActors[actor_idx]->critterProps.pcRank;
				eaPush(&ugcInfo->pUGCProperties->groupDefProps.eaEncounterActors, actor_properties);
			}
		}
	}
}

static void ugcResourceInfoPopulateForDictionary( const char* dictionaryName, UGCResourceInfoAddDataFn fn, UGCResourceInfoDefList* list )
{
	const char *dict_name_pooled = allocFindString(dictionaryName);
	char save_filename[MAX_PATH];
	const char *save_filename_pooled;
	bool isObjectLibrary = (stricmp( dictionaryName, "ObjectLibrary" ) == 0);

	sprintf(save_filename, "%s/UGC/%s.ugcdict", "tempbin", dictionaryName);
	save_filename_pooled = allocAddFilename(save_filename);

	// Check for primary resources
	FOR_EACH_IN_EARRAY(list->resources, UGCResourceInfoDef, resource) {
		if( dict_name_pooled == resource->dictionary_name ) {
			const char* name = resource->logical_name;
			Referent referent = NULL;
			int defId = 0;

			if( isObjectLibrary ) {
				GroupDef* def = objectLibraryGetGroupDefByName( name, false );
				char buffer[ RESOURCE_NAME_MAX_SIZE ];

				if( def ) {
					defId = def->name_uid;
					sprintf( buffer, "%d", defId );
					name = allocAddString( buffer );
					referent = def;
				}
			} else {
				referent = RefSystem_ReferentFromString( dictionaryName, name );
			}

			if( referent ) {
				const char *name_pooled = allocFindString(name);
				UGCResourceInfo* ugcInfo = StructCreate( parse_UGCResourceInfo );
				char nameBuffer[ RESOURCE_NAME_MAX_SIZE ];

				sprintf( nameBuffer, "%s__%s", dictionaryName, name );
				ugcInfo->pcName = allocAddString( nameBuffer );
				ugcInfo->pcFilename = save_filename_pooled;
				ugcInfo->pResInfo = StructCreate( parse_ResourceInfo );
				ugcInfo->pResInfo->resourceDict = allocAddString( resource->dictionary_name );
				ugcInfo->pResInfo->resourceName = allocAddString( name );
				ugcInfo->pResInfo->resourceID = defId;
				ugcInfo->pResInfo->resourceTags = allocAddString( resource->tags );
				ugcInfo->pUGCProperties = StructClone( parse_WorldUGCProperties, &resource->ugcProps );
				if( !ugcInfo->pUGCProperties ) {
					ugcInfo->pUGCProperties = StructCreate( parse_WorldUGCProperties );
				}
				eaDestroyStruct( &ugcInfo->pResInfo->ppReferences, parse_ResourceReference );

				if( fn ) {
					fn( ugcInfo );
				}

				if( RefSystem_ReferentFromString( UGC_DICTIONARY_RESOURCE_INFO, ugcInfo->pcName )) {
					Errorf( "Duplicate UGCResInfo fonud for resource %s.", ugcInfo->pcName );
				} else {
					RefSystem_AddReferent( UGC_DICTIONARY_RESOURCE_INFO, ugcInfo->pcName, ugcInfo );
				}

				if( nullStr( ugcInfo->pResInfo->resourceTags )) {
					Errorf( "UGCResInfo for Resource %s %s, but it does not keep a tags list",
							resource->dictionary_name, resource->logical_name);
				}
				else 
				{	
					if (ugcHasTag(ugcInfo->pResInfo->resourceTags, "donotvalidate"))
					{
						// Do not validate if we are something special (like the editing maps)
					}
					else
					{
						if (!ugcHasTag(ugcInfo->pResInfo->resourceTags, "UGC"))
						{
							Errorf( "UGCResInfo for Resource %s %s, but it is not tagged for UGC",
								resource->dictionary_name, resource->logical_name);
						}
					}
				}
			} else {
				Errorf( "UGCResInfo for Resource %s %s, but resource does not exist",
						resource->dictionary_name, resource->logical_name);
			}
		}
	} FOR_EACH_END;

	// Check for fallbacks
	FOR_EACH_IN_EARRAY(list->fallbacks, UGCResourceInfoDef, fallback)
	{

		if (  fallback->dictionary_name == dict_name_pooled
			  && RefSystem_ReferentFromString( fallback->dictionary_name, fallback->logical_name ))
		{
			ResourceInfo* info = resGetInfo( fallback->dictionary_name, fallback->logical_name );
			char resName[ RESOURCE_NAME_MAX_SIZE ];
			sprintf( resName, "%s__%s", fallback->dictionary_name, fallback->logical_name);

			if( info && !RefSystem_ReferentFromString( UGC_DICTIONARY_RESOURCE_INFO, resName ))
			{
				UGCResourceInfo* ugcInfo = StructCreate( parse_UGCResourceInfo );

				ugcInfo->pcName = allocAddString( resName );
				ugcInfo->pcFilename = save_filename_pooled;
				ugcInfo->pResInfo = StructClone( parse_ResourceInfo, info );
				ugcInfo->pResInfo->resourceTags = allocAddString( fallback->tags );

				ugcInfo->pUGCProperties = StructClone( parse_WorldUGCProperties, &fallback->ugcProps );
				eaDestroyStruct( &ugcInfo->pResInfo->ppReferences, parse_ResourceReference );
				
				if( fn ) {
					fn( ugcInfo );
				}

				if( RefSystem_ReferentFromString( UGC_DICTIONARY_RESOURCE_INFO, ugcInfo->pcName )) {
					ErrorFilenamef( ugcInfo->pcFilename, "Dupilcate UGCResInfo found for resource %s.", ugcInfo->pcName );
				} else {
					RefSystem_AddReferent( UGC_DICTIONARY_RESOURCE_INFO, ugcInfo->pcName, ugcInfo );
				}
			}
		}
	}
	FOR_EACH_END;

	ParserWriteTextFileFromDictionary(save_filename_pooled, UGC_DICTIONARY_RESOURCE_INFO, 0, 0);
	binNotifyTouchedOutputFile( save_filename_pooled );
}

static void ugcResourceInfoPopulateFromList( const char *dictionaryName, UGCResourceInfoAddDataFn fn, UGCResourceInfoDefList *list )
{
	const char *dict_name_pooled = allocFindString(dictionaryName);
	char save_filename[MAX_PATH];
	const char *save_filename_pooled;

	sprintf(save_filename, "%s/UGC/%s.ugcdict", "tempbin", dictionaryName);
	save_filename_pooled = allocAddFilename(save_filename);

	FOR_EACH_IN_EARRAY(list->resources, UGCResourceInfoDef, resource)
	{
		if (resource->dictionary_name == dict_name_pooled)
		{
			UGCResourceInfo* ugcInfo = StructCreate(parse_UGCResourceInfo);
			char nameBuffer[ RESOURCE_NAME_MAX_SIZE ];

			sprintf(nameBuffer, "%s__%s", dictionaryName, resource->logical_name);
			ugcInfo->pcName = allocAddString( nameBuffer );
			ugcInfo->pcFilename = save_filename_pooled;
			ugcInfo->pResInfo = StructCreate(parse_ResourceInfo);
			ugcInfo->pResInfo->resourceDict = dict_name_pooled;
			ugcInfo->pResInfo->resourceName = resource->logical_name;
			ugcInfo->pResInfo->resourceTags = resource->tags;

			ugcInfo->pUGCProperties = StructClone(parse_WorldUGCProperties, &resource->ugcProps);
			
			if( fn ) {
				fn( ugcInfo );
			}

			if( RefSystem_ReferentFromString( UGC_DICTIONARY_RESOURCE_INFO, ugcInfo->pcName )) {
				ErrorFilenamef( ugcInfo->pcFilename, "Dupilcate UGCResInfo found for resource %s.", ugcInfo->pcName );
			} else {
				RefSystem_AddReferent( UGC_DICTIONARY_RESOURCE_INFO, ugcInfo->pcName, ugcInfo );
			}
		}
	}
	FOR_EACH_END;

	ParserWriteTextFileFromDictionary(save_filename_pooled, UGC_DICTIONARY_RESOURCE_INFO, 0, 0);
	binNotifyTouchedOutputFile( save_filename_pooled );
}

static void ugcResourceAddFSMMetadata( UGCResourceInfo* info )
{
	const char* resName = info->pResInfo->resourceName;
	FSM* fsm = RefSystem_ReferentFromString( "FSM", resName );
	WorldUGCFSMProperties* fsmProps = &info->pUGCProperties->fsmProps;

	if( fsm ) {
		int it;

		info->pFSMMetadata = StructCreate( parse_UGCFSMMetadata );
		
		for( it = 0; it != eaSize( &fsmProps->eaExternVars ); ++it ) {
			UGCFSMExternVarDef* ugcVarDef = fsmProps->eaExternVars[ it ];
			FSMExternVar* fsmVar = fsmExternVarFromName( fsm, ugcVarDef->name, "Encounter" );
			UGCFSMExternVar* ugcVar;

			if( !fsmVar ) {
				ErrorFilenamef( info->pcFilename, "FSM %s -- UGC specifies properties for var %s, but it does not exist.",
								resName, ugcVarDef->name );
				continue;
			}

			ugcVar = StructCreate( parse_UGCFSMExternVar );
			ugcVar->astrName = allocAddString( ugcVarDef->name );
			ugcVar->type = fsmVar->type;
			ugcVar->scType = StructAllocString( fsmVar->scType );
			StructCopyFields( parse_UGCFSMExternVarDef, ugcVarDef, &ugcVar->defProps, 0, 0 );
			eaPush( &info->pFSMMetadata->eaExternVars, ugcVar );
		}
	}
}

static void ugcResourceAddCostumeMetadata( UGCResourceInfo* info )
{
	const char* resName = info->pResInfo->resourceName;
	PlayerCostume* costume = RefSystem_ReferentFromString( "PlayerCostume", resName );
	if( ugcDefaultsCostumeEditorStyle() != UGC_COSTUME_EDITOR_STYLE_NEVERWINTER ) {
		return;
	}

	info->pCostumeMetadata = StructCreate( parse_UGCCostumeMetadata );

	if( strstri( info->pResInfo->resourceTags, "Costume,")) {
		info->pCostumeMetadata->pFullCostume = StructClone( parse_PlayerCostume, costume );
	}

	if( strstri( info->pResInfo->resourceTags, "CostumeItem," )) {
		eaCopyStructs( &costume->eaParts, &info->pCostumeMetadata->eaItemParts, parse_PCPart );
	}
}

static bool ugcAddGroupDefMetadataHelper( UGCResourceInfo* resInfo, GroupDef *def, GroupInfo *info, GroupInheritedInfo *inherited_info, bool needs_entry )
{
	if( SAFE_MEMBER( def->property_structs.path_node_properties, bUGCNode )) {
		ZoneMapMetadataPathNode* pathNode = StructCreate( parse_ZoneMapMetadataPathNode );
		pathNode->defUID = def->name_uid;
		copyVec3( info->world_matrix[ 3 ], pathNode->pos );
		FOR_EACH_IN_EARRAY_FORWARDS( def->property_structs.path_node_properties->eaConnections, WorldPathEdge, connection ) {
			ZoneMapMetadataPathEdge* pathEdge = StructCreate( parse_ZoneMapMetadataPathEdge );
			eaPush( &pathNode->eaConnections, pathEdge );
			pathEdge->uOther = connection->uOther;
		} FOR_EACH_END;

		if( !resInfo->pDefMetadata ) {
			resInfo->pDefMetadata = StructCreate( parse_UGCGroupDefMetadata );
		}
		eaPush( &resInfo->pDefMetadata->eaPathNodes, pathNode );
	}

	return true;
}

static void ugcResourceAddGroupDefMetadata( UGCResourceInfo* info )
{
	const char* resName = info->pResInfo->resourceName;
	GroupDef* def = objectLibraryGetGroupDefByName( resName, false );

	if( !def ) {
		return;
	}

	ugcResourceInfoGenerateActorProperties(def, info);

	if(   ugcHasTagType( info->pResInfo->resourceTags, "ObjectLibrary", "Cluster" )
		  || ugcHasTagType( info->pResInfo->resourceTags, "ObjectLibrary", "Teleporter" ) ) {
		int childIt;

		if( !info->pDefMetadata ) {
			info->pDefMetadata = StructCreate( parse_UGCGroupDefMetadata );
		}
		for( childIt = 0; childIt != eaSize( &def->children ); ++childIt ) {
			const GroupChild* child = def->children[ childIt ];
			UGCDefChildMetadata* childData = StructCreate( parse_UGCDefChildMetadata );
			eaPush( &info->pDefMetadata->eaClusterChildren, childData );

			childData->defUID = child->name_uid;
			childData->astrDefDebugName = allocAddString( child->debug_name );
			copyVec3( child->pos, childData->pos );
			childData->rot = child->rot[ 1 ];
		}
	}

	groupTreeTraverse( NULL, def, NULL, NULL, ugcAddGroupDefMetadataHelper, info, false, false );
}

void ugcResourceInfoPopulateDictionary( void )
{
	UGCResourceInfoDefList list = { 0 };
	char dir_filename[MAX_PATH];

	if (ugcResourceInfosPopulated)
	{
		return;
	}

	loadstart_printf("UGC Resource Info...");

	if (IsGameServerSpecificallly_NotRelatedTypes() && !isProductionMode())
	{
		// Renegerate the UGCResourceInfo dictionary files into a temp directory

		loadstart_printf("Populate Dictionary...");

		// Load override properties to apply
		ParserLoadFiles("genesis/", ".ugcresinfo", NULL, PARSER_OPTIONALFLAG, parse_UGCResourceInfoDefList, &list );
		FOR_EACH_IN_REFDICT( "ZoneMapEncounterInfo", ZoneMapEncounterInfo, zeni ) {
			int it;
			for( it = 0; it != eaSize( &zeni->objects ); ++it ) {
				ZoneMapEncounterObjectInfo* zeniObj = zeni->objects[ it ];
				if( zeniObjIsUGCData( zeniObj ) && IS_HANDLE_ACTIVE( zeniObj->displayName ) && IS_HANDLE_ACTIVE( zeniObj->ugcContactCostume )) {
					UGCResourceInfoDef* fallback = StructCreate( parse_UGCResourceInfoDef );
					fallback->logical_name = REF_STRING_FROM_HANDLE( zeniObj->ugcContactCostume );
					fallback->dictionary_name = allocAddString( "PlayerCostume" );
					fallback->tags = allocAddString( "UGC" );
					COPY_HANDLE( fallback->ugcProps.dVisibleName.hMessage, zeniObj->displayName );
					COPY_HANDLE( fallback->ugcProps.dDescription.hMessage, zeniObj->displayDetails );
					eaPush( &list.fallbacks, fallback );
				}
			}
		} FOR_EACH_END;

		ugcResourceInfoPopulateForDictionary( "AIAnimList", NULL, &list );
		ugcResourceInfoPopulateForDictionary( "FSM", ugcResourceAddFSMMetadata, &list );
		ugcResourceInfoPopulateForDictionary( "PetContactList", NULL, &list );
		ugcResourceInfoPopulateForDictionary( UGC_DICTIONARY_SOUND, NULL, &list );
		ugcResourceInfoPopulateForDictionary( UGC_DICTIONARY_SOUND_DSP, NULL, &list );
		ugcResourceInfoPopulateForDictionary( "PlayerCostume", ugcResourceAddCostumeMetadata, &list );
		ugcResourceInfoPopulateForDictionary( "ZoneMap", NULL, &list );
		ugcResourceInfoPopulateForDictionary( "ObjectLibrary", ugcResourceAddGroupDefMetadata, &list );
		ugcResourceInfoPopulateForDictionary( "Cutscene", NULL, &list );
		ugcResourceInfoPopulateForDictionary( "RewardTable", NULL, &list );

		ugcResourceInfoPopulateFromList( "SkyInfo", NULL, &list );
		ugcResourceInfoPopulateFromList( "Texture", NULL, &list );
		ugcResourceInfoPopulateFromList( "HeadshotStyleDef", NULL, &list );

		StructReset(parse_UGCResourceInfoDefList, &list);

		// Clear dictionary so we can load it without errors below
		RefSystem_ClearDictionary(UGC_DICTIONARY_RESOURCE_INFO, false);

		loadend_printf("done...");
	}

	// Now load the dictionary. In dev mode, this will write out the bin file. In production it will load from the bin.
	sprintf(dir_filename, "%s/UGC/", "tempbin");
	resLoadResourcesFromDisk(UGC_DICTIONARY_RESOURCE_INFO, dir_filename, ".ugcdict", "UGCResourceInfo.bin", PARSER_BINS_ARE_SHARED);

	if(IsGameServerSpecificallly_NotRelatedTypes() && !isProductionMode())
	{
		loadstart_printf("Check References...");

		resValidateCheckAllReferencesForDictionary(UGC_DICTIONARY_RESOURCE_INFO);

		loadend_printf("done.");
	}

	loadend_printf("done.");

	ugcResourceInfosPopulated = true;
}

void ugcLoadDictionaries( void )
{
	static bool ugc_dict_loaded = false;
	if( ugc_dict_loaded ) {
		return;
	}

	ugcLoadTagTypeLibrary();
	ugc_dict_loaded = true;
}

/// The following function exists so that to be as similar to
/// ResourceDictionary loading as possible.
///
/// Just like dictionary loading, you use SetUpResourceLoaderParse() to modify ResourceLoaderStruct, then ParserLoadFiles(), then clear the name from ResourceLoaderStruct().
///
/// See ParserLoadFilesToDictionary to see what this is based on.
static bool ParserLoadFilesForUGCBackwardCompatibility(const char* dirs, const char* filemask, int flags, ParseTable pti[], ResourceLoaderStruct* structptr)
{
	bool toRet = false;
	SetUpResourceLoaderParse( pti[0].name, ParserGetTableSize( pti ), pti, NULL );
	toRet = ParserLoadFiles( dirs, filemask, NULL, flags, parse_ResourceLoaderStruct, structptr );
	parse_ResourceLoaderStruct[0].name = NULL;

	return toRet;
}

/// StructReset analagous to above.
static void StructResetForUGCBackwardCompatibility( ParseTable pti[], ResourceLoaderStruct* structptr)
{
	SetUpResourceLoaderParse( pti[0].name, ParserGetTableSize( pti ), pti, NULL );
	StructReset( parse_ResourceLoaderStruct, structptr );
	parse_ResourceLoaderStruct[0].name = NULL;
}

UGCProjectData *ugcProjectLoadFromDir( const char *dir )
{
	ResourceLoaderStruct projectList = { 0 };
	ResourceLoaderStruct missionList = { 0 };
	ResourceLoaderStruct mapList = { 0 };
	ResourceLoaderStruct componentListList = { 0 }; //< this name makes me sad
	ResourceLoaderStruct costumeList = { 0 };
	UGCProjectData* data = NULL;
	
	// Simulate dictionary loading for backward compatibility with
	// early STO projects.
	if( !ParserLoadFilesForUGCBackwardCompatibility( dir, ".project", 0, parse_UGCProjectInfo, &projectList )) {
//		Errorf( "ugcProjectLoadFromDir: Failed during .project file loading." );
//		goto error;

		// 
		// [WOLF 23Jul12]  We need to account for the first time a new project is created in the editor.
		//   We will get here and attempt to load files which definitely do not exist, so this should
		//   not be treated as an error condition and we should return an 'empty' initialized project.
		// Eventually we will be making the information available as to if we are creating a new project
		//   or not and we can distinguish if we need to error or not.

		return (StructCreate( parse_UGCProjectData ));
	}

	if( !ParserLoadFilesForUGCBackwardCompatibility( dir, ".ugcmission", 0, parse_UGCMission, &missionList )) {
		Errorf( "ugcProjectLoadFromDir: Failed during .ugcmission file loading." );
		goto error;
	}

	if( !ParserLoadFilesForUGCBackwardCompatibility( dir, ".ugcmap", PARSER_OPTIONALFLAG, parse_UGCMap, &mapList )) {
		Errorf( "ugcProjectLoadFromDir: Failed during .ugcmap file loading." );
		goto error;
	}

	if( !ParserLoadFilesForUGCBackwardCompatibility( dir, ".components", 0, parse_UGCComponentList, &componentListList )) {
		Errorf( "ugcProjectLoadFromDir: Failed during .components file loading." );
		goto error;
	}

	if( !ParserLoadFilesForUGCBackwardCompatibility( dir, ".ugccostume", PARSER_OPTIONALFLAG, parse_UGCCostume, &costumeList )) {
		Errorf( "ugcProjectLoadFromDir: Failed during .ugcmission file loading." );
		goto error;
	}

	if( eaSize( &projectList.earrayOfStructs ) != 1 ) {
		Errorf( "ugcProjectLoadFromDir: Found %d .project files, expected only 1.", eaSize( &projectList.earrayOfStructs ));
		goto error;
	}
	if( eaSize( &missionList.earrayOfStructs ) != 1 ) {
		Errorf( "ugcProjectLoadFromDir: Found %d .ugcmission files, expected only 1.", eaSize( &missionList.earrayOfStructs ));
		goto error;
	}
	if( eaSize( &componentListList.earrayOfStructs ) != 1 ) {
		Errorf( "ugcProjectLoadFromDir: Found %d .components files, expected only 1.", eaSize( &componentListList.earrayOfStructs ));
	}

	// Build the UGCProjectData now
	data = StructCreate( parse_UGCProjectData );
	data->project = projectList.earrayOfStructs[ 0 ];
	data->maps = (UGCMap**)mapList.earrayOfStructs;
	data->mission = missionList.earrayOfStructs[ 0 ];
	data->components = componentListList.earrayOfStructs[ 0 ];
	data->costumes = (UGCCostume**)costumeList.earrayOfStructs;

	projectList.earrayOfStructs[ 0 ] = NULL;
	mapList.earrayOfStructs = NULL;
	missionList.earrayOfStructs[ 0 ] = NULL;
	componentListList.earrayOfStructs[ 0 ] = NULL;
	costumeList.earrayOfStructs = NULL;

error:
	StructResetForUGCBackwardCompatibility( parse_UGCProjectInfo, &projectList );
	StructResetForUGCBackwardCompatibility( parse_UGCMission, &missionList );
	StructResetForUGCBackwardCompatibility( parse_UGCMap, &mapList );
	StructResetForUGCBackwardCompatibility( parse_UGCComponentList, &componentListList );
	StructResetForUGCBackwardCompatibility( parse_UGCCostume, &costumeList );
	
	return data;
}

//// Defaults

static UGCPerProjectDefaults *g_UGCDefaults = NULL;

static void ugcReloadDefaults(const char *path, int UNUSED_when)
{
	loadstart_printf("Reloading UGCPerProjectDefaults...");
	fileWaitForExclusiveAccess(path);
	errorLogFileIsBeingReloaded(path);
	if(!ParserLoadFiles(NULL, "genesis/ugc_defaults.txt", "UGCDefaults.bin", PARSER_BINS_ARE_SHARED, parse_UGCPerProjectDefaults, g_UGCDefaults))
		Errorf("Error reloading UGCPerProjectDefaults");

	loadend_printf(" done");
}

UGCPerProjectDefaults *ugcGetDefaults()
{
	if(!g_UGCDefaults)
	{
		g_UGCDefaults = StructCreate(parse_UGCPerProjectDefaults);

		if(!ParserLoadFiles(NULL, "genesis/ugc_defaults.txt", "UGCDefaults.bin", PARSER_BINS_ARE_SHARED, parse_UGCPerProjectDefaults, g_UGCDefaults))
			Errorf("Error loading UGCPerProjectDefaults");

		if(isDevelopmentMode())
			FolderCacheSetCallback(FOLDER_CACHE_CALLBACK_UPDATE, "genesis/ugc_defaults.txt", ugcReloadDefaults);
	}

	return g_UGCDefaults;
}

int ugcGetAllegianceDefaultsIndex( const UGCProjectData* ugcProj )
{
	UGCPerProjectDefaults* config = ugcGetDefaults();
	const char* projectFaction = NULL;
	int allegianceIt;

	if( !ugcProj ) {
		return -1;
	}

	if(   SAFE_MEMBER( ugcProj->project, pRestrictionProperties )
		  && eaSize( &ugcProj->project->pRestrictionProperties->eaFactions ) == 1
		  && !nullStr( ugcProj->project->pRestrictionProperties->eaFactions[ 0 ]->pcFaction )) {
		projectFaction = ugcProj->project->pRestrictionProperties->eaFactions[ 0 ]->pcFaction;
	}

	if (!projectFaction)
	{
		projectFaction = allocAddString("NO_ALLEGIANCE");
	}

	// Can only do the allegiance mapping if there is a single valid allegiance to use
	for( allegianceIt = 0; allegianceIt != eaSize( &config->allegiance ); ++allegianceIt ) {
		UGCPerAllegianceDefaults* allegianceConfig = config->allegiance[ allegianceIt ];

		if( allegianceConfig->allegianceName == projectFaction ) {
			return allegianceIt;
		}
	}

	return -1;
}

UGCPerAllegianceDefaults *ugcGetAllegianceDefaults(const UGCProjectData* ugcProj)
{
	UGCPerProjectDefaults* config = ugcGetDefaults();
	return eaGet( &config->allegiance, ugcGetAllegianceDefaultsIndex( ugcProj ));
}


const char* ugcGetDefaultMapName( const UGCProjectData* data )
{
	UGCPerAllegianceDefaults* defaults = ugcGetAllegianceDefaults( data );

	if (defaults && defaults->pcDefaultCrypticMap)
		return defaults->pcDefaultCrypticMap;
	else
		return "Kfr_Sol_Starbase_Ground";
}

UGCProjectBudget *ugcFindBudget(UGCBudgetType type, UGCComponentType component_type)
{
	static UGCProjectBudget sBudget = { 0 };
	UGCPerProjectDefaults *defaults = ugcGetDefaults();
	if (!defaults)
		return NULL;
	FOR_EACH_IN_EARRAY(defaults->ppDefaultBudgets, UGCProjectBudget, budget)
	{
		if (budget->eType == type && (type != UGC_BUDGET_TYPE_COMPONENT || budget->eComponentType == component_type))
		{
			StructCopyAll(parse_UGCProjectBudget, budget, &sBudget);
			// This is where account-based overrides would be applied
			return &sBudget;
		}
	}
	FOR_EACH_END;
	return NULL;
}

UGCDialogStyle ugcDefaultsDialogStyle(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return UGC_DIALOG_STYLE_IN_WORLD;
	} else {
		return UGC_DIALOG_STYLE_WINDOW;
	}
}

bool ugcDefaultsMapTransitionsSpecifyDoor(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

bool ugcDefaultsSingleMissionEnabled(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

bool ugcDefaultsIsSeriesEditorEnabled(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

bool ugcDefaultsIsItemEditorEnabled(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

bool ugcDefaultsSearchFiltersByPlayerLevel(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return false;
	} else {
		return true;
	}
}

bool ugcDefaultsMapLinkSkipCompletedMaps(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

bool ugcDefaultsPreviewImagesAndOverworldMapEnabled(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

bool ugcDefaultsMissionReturnEnabled(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

UGCCostumeEditorStyle ugcDefaultsCostumeEditorStyle(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return UGC_COSTUME_EDITOR_STYLE_NEVERWINTER;
	} else {
		return UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR;
	}
}

UGCCostumeSkeletonSlotDef* ugcDefaultsCostumeSkeletonDef( const char* skeletonName )
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	int it;
	
	skeletonName = allocAddString( skeletonName );
	for( it = 0; it != eaSize( &defaults->eaCostumeSkeletonDefs ); ++it ) {
		if( defaults->eaCostumeSkeletonDefs[ it ]->astrName == skeletonName ) {
			return defaults->eaCostumeSkeletonDefs[ it ];
		}
	}

	return NULL;
}


UGCCostumeRegionDef* ugcDefaultsCostumeRegionDef( const char* regionName )
{
	regionName = allocFindString( regionName );
	if( regionName ) {
		UGCPerProjectDefaults* defaults = ugcGetDefaults();
		int it;
		
		for( it = 0; it != eaSize( &defaults->eaCostumeRegionDefs ); ++it ) {
			UGCCostumeRegionDef* def = defaults->eaCostumeRegionDefs[ it ];
			if( def->astrName == regionName ) {
				return def;
			}
		}
	}

	return NULL;
}

UGCCheckedAttribDef* ugcDefaultsCheckedAttribDef( const char* attribName )
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	int it;

	attribName = allocAddString( attribName );
	for( it = 0; it != eaSize( &defaults->checkedAttribs ); ++it ) {
		if( defaults->checkedAttribs[ it ]->name == attribName ) {
			return defaults->checkedAttribs[ it ];
		}
	}

	return NULL;
}

const char* ugcDefaultsFallbackPromptText( void )
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	return defaults->strFallbackPromptText;
}

const char* ugcDefaultsGetAllegianceRestriction( void )
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	return SAFE_MEMBER( defaults, pcAllegianceRestriction );
}

bool ugcDefaultsAuthorAllowsFeaturedBlocksEditing( void )
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return false;
	} else {
		return true;
	}
}

bool ugcDefaultsIsMapLinkIncludeTeammatesEnabled(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

bool ugcDefaultsIsColoredPromptButtonsEnabled(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

bool ugcDefaultsIsOverworldMapLinkAllowed(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

bool ugcDefaultsIsPathNodesEnabled( void )
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

// This causes a single reward box (or Super Chest) to be placed on the last map of a UGC project.
// If true, there is only ever going to be one loot-able reward box at the end of the UGC project mission,
// either on an internal map or the cryptic world (external map), depending on which is last in the UGC project Story.
bool ugcDefaultsIsFinalRewardBoxSupported(void)
{
	return stricmp( GetProductName(), "Night" ) == 0;
}

// If a final reward box is supported, get the Object Library piece to use for internal map reward boxes
const char *ugcDefaultsGetRewardBoxObjlib(void)
{
	return "Ugc_Treasure_Chest_Good_Objlib";
}

// If a final reward box is supported, get the Contact name to use for reward boxes
const char *ugcDefaultsGetRewardBoxContact(void)
{
	return "Mission_Reward_Chest";
}

// If a final reward box is supported, get the Reward Table name to use for the reward box
const char *ugcDefaultsGetRewardBoxReward(void)
{
	return "Ugc_Rewards_General_Super_Chest";
}

// If a final reward box is supported, get the Display string to use for the reward box objective and waypoint
const char *ugcDefaultsGetRewardBoxDisplay(void)
{
	return "Collect Reward";
}

// If a final reward box is supported, get the Display string to use for the floater text once the reward box has been looted
const char *ugcDefaultsGetRewardBoxLootedDisplay(void)
{
	return "Reward Collected";
}

bool ugcIsSpaceEnabled(void)
{
	if (stricmp(GetProductName(), "StarTrek") == 0)
	{
		return true;
	}
	return false;
}

bool ugcIsAllegianceEnabled(void)
{
	if (stricmp(GetProductName(), "StarTrek") == 0)
	{
		return true;
	}
	return false;
}

bool ugcIsFixedLevelEnabled(void)
{
	return false;
}

void ugcDefaultsFillAllegianceList(AllegianceList *list)
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	int i;

	if(eaSize(&list->refArray)
	   || (eaSize( &defaults->allegiance ) == 1 && stricmp( defaults->allegiance[0]->allegianceName, "NO_ALLEGIANCE" ) == 0))
		return;

	for( i = 0; i != eaSize( &defaults->allegiance ); ++i )
	{
		AllegianceRef *ar = StructCreate(parse_AllegianceRef);

		SET_HANDLE_FROM_STRING(g_hAllegianceDict, defaults->allegiance[i]->allegianceName, ar->hDef);
		
		eaPush(&list->refArray, ar);
	}
}

bool ugcIsCheckedAttribEnabled(void)
{
	if( stricmp( GetProductName(), "Night" ) == 0 ) {
		return true;
	} else {
		return false;
	}
}

bool ugcIsInteriorEditorEnabled(void)
{
	if (stricmp(GetProductName(), "Night") == 0)
	{
		return true;
	}
	return false;
}

bool ugcIsDialogWithObjectEnabled(void)
{
	if (stricmp(GetProductName(), "Night") == 0)
	{
		return true;
	}
	return false;
}

bool ugcIsMissionItemsEnabled(void)
{
	if (stricmp(GetProductName(), "Night") == 0)
	{
		return true;
	}
	return false;
}

bool ugcIsChatAL0(void)
{
	if (stricmp(GetProductName(), "Night") == 0)
	{
		return true;
	}
	return false;
}

const char*** ugcCheckedAttribModel(void)
{
	if( !ugcIsCheckedAttribEnabled() ) {
		return NULL;
	} else {
		UGCPerProjectDefaults* config = ugcGetDefaults();
		static const char** model = NULL;
		int it;
		eaPush( &model, NULL );
		for( it = 0; it != eaSize( &config->checkedAttribs ); ++it ) {
			eaPush( &model, config->checkedAttribs[ it ]->name );
		}

		return &model;
	}
}

bool ugcMapCanCustomizeBackdrop(UGCMapType map_type)
{
	if (map_type == UGC_MAP_TYPE_SPACE || map_type == UGC_MAP_TYPE_PREFAB_SPACE)
		return true;
	if (stricmp(GetProductName(), "Night") == 0)
	{
		return true;
	}
	return false;
}

bool ugcIsLegacyHeightSnapEnabled()
{
	if (stricmp(GetProductName(), "StarTrek") == 0)
	{
		return true;
	}
	return false;
}

//// Component utility operations

const char *ugcComponentTypeGetName(UGCComponentType eType)
{
	return StaticDefineIntRevLookup( UGCComponentTypeEnum, eType );
}

const char *ugcComponentTypeGetDisplayName(UGCComponentType eType, bool bShort)
{
	switch (eType)
	{
		case UGC_COMPONENT_TYPE_KILL:
			return TranslateMessageKey( "UGC_Editor.Encounter" );
		case UGC_COMPONENT_TYPE_ACTOR:
			return TranslateMessageKey( "UGC_Editor.Actor" );
		case UGC_COMPONENT_TYPE_CONTACT:
			return TranslateMessageKey( "UGC_Editor.Contact" );
		case UGC_COMPONENT_TYPE_DIALOG_TREE:
			return TranslateMessageKey( "UGC_Editor.DialogTree" );
		case UGC_COMPONENT_TYPE_SPAWN:
			return TranslateMessageKey( "UGC_Editor.Spawn" );
		case UGC_COMPONENT_TYPE_COMBAT_JOB:
			return TranslateMessageKey( "UGC_Editor.CombatJob" );
		case UGC_COMPONENT_TYPE_WHOLE_MAP:
			return TranslateMessageKey( "UGC_Editor.WholeMap" );
		case UGC_COMPONENT_TYPE_RESPAWN:
			return TranslateMessageKey( "UGC_Editor.Respawn" );
		case UGC_COMPONENT_TYPE_SOUND:
			return TranslateMessageKey( "UGC_Editor.Sound" );
		case UGC_COMPONENT_TYPE_OBJECT:
			return TranslateMessageKey( "UGC_Editor.Object" );
		case UGC_COMPONENT_TYPE_CLUSTER_PART:
			return TranslateMessageKey( "UGC_Editor.ClusterPart" );
		case UGC_COMPONENT_TYPE_DESTRUCTIBLE:
			return TranslateMessageKey( "UGC_Editor.Destructable" );
		case UGC_COMPONENT_TYPE_ROOM:
			return TranslateMessageKey( "UGC_Editor.Room" );
		case UGC_COMPONENT_TYPE_ROOM_DOOR:
			return TranslateMessageKey( "UGC_Editor.RoomDoor" );
		case UGC_COMPONENT_TYPE_FAKE_DOOR:
			return TranslateMessageKey( "UGC_Editor.FakeDoor" );
		case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
			return TranslateMessageKey( "UGC_Editor.Building" );
		case UGC_COMPONENT_TYPE_PLANET:
			return TranslateMessageKey( "UGC_Editor.Planet" );
		case UGC_COMPONENT_TYPE_ROOM_MARKER:
			if( bShort ) {
				return TranslateMessageKey( "UGC_Editor.RoomMarker_Short" );
			} else {
				return TranslateMessageKey( "UGC_Editor.RoomMarker" );
			}
		case UGC_COMPONENT_TYPE_EXTERNAL_DOOR:
			return TranslateMessageKey( "UGC_Editor.ExternalDoor" );
		case UGC_COMPONENT_TYPE_PATROL_POINT:
			return TranslateMessageKey( "UGC_Editor.PatrolPoint" );
		case UGC_COMPONENT_TYPE_TRAP:
			return TranslateMessageKey( "UGC_Editor.Trap" );
		case UGC_COMPONENT_TYPE_TRAP_TARGET:
			return TranslateMessageKey( "UGC_Editor.TrapTarget" );
		case UGC_COMPONENT_TYPE_TRAP_TRIGGER:
			return TranslateMessageKey( "UGC_Editor.TrapTrigger" );
		case UGC_COMPONENT_TYPE_TRAP_EMITTER:
			return TranslateMessageKey( "UGC_Editor.TrapEmitter" );
		case UGC_COMPONENT_TYPE_TELEPORTER:
			return TranslateMessageKey( "UGC_Editor.Teleporter" );
		case UGC_COMPONENT_TYPE_TELEPORTER_PART:
			return TranslateMessageKey( "UGC_Editor.TeleporterPart" );
		case UGC_COMPONENT_TYPE_CLUSTER:
			return TranslateMessageKey( "UGC_Editor.Cluster" );
		case UGC_COMPONENT_TYPE_REWARD_BOX:
			return TranslateMessageKey( "UGC_Editor.RewardBox" );
	}
	return TranslateMessageKey( "UGC_Editor.UnknownComponentType" );
}

/// Gets the visible name that should be generated for this component.
void ugcComponentGetDisplayNameSafe(char* out, int out_size, const UGCProjectData* ugcProj, const UGCComponent* component, bool bForGeneration)
{
	if( !nullStr( component->pcVisibleName )) {
		if( !bForGeneration ) {
			sprintf_s( SAFESTR2(out), "%s (#%d)", component->pcVisibleName, component->uID );
		} else {
			sprintf_s( SAFESTR2(out), "%s", component->pcVisibleName );
		}
	} else {
		ugcComponentGetDisplayNameDefaultSafe( out, out_size, ugcProj, component, bForGeneration );
	}
}

/// Gets the default name for this component -- what would be used if
/// the pcVisibleName field was not filled out.
///
/// If you are not setting text entry default text, you should not be
/// calling this function.
void ugcComponentGetDisplayNameDefaultSafe(char* out, int out_size, const UGCProjectData* ugcProj, const UGCComponent* component, bool bForGeneration)
{
	char defaultStrBuffer[ 256 ];
	const char* defaultStr = NULL;

	if( component->sPlacement.bIsExternalPlacement ) {
		ZoneMapEncounterObjectInfo* zeniObj = zeniObjectFind( component->sPlacement.pcExternalMapName, component->sPlacement.pcExternalObjectName );
		if( zeniObj ) {
			defaultStr = TranslateMessageRef( zeniObj->displayName );
		}
	} else if( component->iObjectLibraryId
			   && component->eType != UGC_COMPONENT_TYPE_TELEPORTER_PART
			   && component->eType != UGC_COMPONENT_TYPE_RESPAWN
			   && component->eType != UGC_COMPONENT_TYPE_REWARD_BOX ) {
		const WorldUGCProperties* ugcProps = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", component->iObjectLibraryId );
		if( ugcProps ) {
			defaultStr = TranslateDisplayMessage( ugcProps->dDefaultName );
			if( !defaultStr )
				defaultStr = TranslateDisplayMessage( ugcProps->dVisibleName );
		}

		if( !defaultStr ) {
			GroupDef* def = objectLibraryGetGroupDef( component->iObjectLibraryId, false );
			if( def ) {
				sprintf( defaultStrBuffer, "%s (UNTRANSLATED)", def->name_str );
			} else {
				sprintf( defaultStrBuffer, "%d (UNTRANSLATED)", component->iObjectLibraryId );
			}
			defaultStr = defaultStrBuffer;
		}
	} else if( component->eType == UGC_COMPONENT_TYPE_ACTOR ) {
		UGCComponent* parentComponent = ugcComponentFindByID( ugcProj->components, component->uParentID );
		if(parentComponent && parentComponent->iObjectLibraryId && parentComponent->eType == UGC_COMPONENT_TYPE_KILL)
		{
			const WorldUGCProperties* ugcProps = ugcResourceGetUGCPropertiesInt("ObjectLibrary", parentComponent->iObjectLibraryId);
			if(ugcProps) {
				int index;
				for(index = 0; index < eaiSize(&parentComponent->uChildIDs); index++)
					if(ugcComponentFindByID(ugcProj->components, parentComponent->uChildIDs[index]) == component)
						break;
				if(index < eaiSize(&parentComponent->uChildIDs))
					defaultStr = TranslateDisplayMessage(ugcProps->groupDefProps.eaEncounterActors[index]->displayNameMsg);
			}
		}
	} else if( !nullStr( component->pcCostumeName )) {
		defaultStr = ugcCostumeSpecifierGetDisplayName( ugcProj, component->pcCostumeName );
	}

	if( nullStr( defaultStr )) {
		if( !bForGeneration ) {
			defaultStr = ugcComponentTypeGetDisplayName( component->eType, false );
		} else {
			defaultStr = "";
		}
	}
	assert( defaultStr );

	{
		if( !bForGeneration ) {
			char* estr = NULL;
			ugcFormatMessageKey( &estr, "UGC.ComponentFormat",
								 STRFMT_STRING( "ComponentName", defaultStr ),
								 STRFMT_INT( "ComponentID", component->uID ),
								 STRFMT_END );

			strcpy_s( SAFESTR2(out), estr );
			estrDestroy( &estr );
		} else {
			sprintf_s( SAFESTR2(out), "%s", defaultStr );
		}
	}
}


bool ugcComponentCalcBounds( UGCComponentList* list, UGCComponent* component, Vec3 out_boundsMin, Vec3 out_boundsMax)
{
	// MJF NOTE: Only editor copies of GroupDefs have bounds
	// correctly calculated.  This makes sense.
	//
	// However, I think it would be better if the UGC editor
	// maintained its own dictionary of GroupDefs with only the fields
	// it cares about and did not polute the WorldEditor's dictionary.
	//
	// But for now this should be fine.

	float fRadius;

	// MJF (Oct/3/2012): When dragging a new component, the parent is
	// not set yet.
	if( component->eType == UGC_COMPONENT_TYPE_TELEPORTER_PART && component->uParentID ) {
		UGCComponent* parentComponent = ugcComponentFindByID( list, component->uParentID );

		// Get the parent def so the object library doesn't complain about referencing private children
		objectLibraryGetGroupDef( parentComponent->iObjectLibraryId, true );
	}
	// MJF May/15/2013: Trap emitters are actually the entire trap
	if( component->eType == UGC_COMPONENT_TYPE_TRAP_EMITTER && component->uParentID ) {
		UGCComponent* parentComponent = ugcComponentFindByID( list, component->uParentID );
		return ugcComponentCalcBoundsForObjLib( parentComponent->iObjectLibraryId, out_boundsMin, out_boundsMax, &fRadius );
	}

	return ugcComponentCalcBoundsForObjLib( component->iObjectLibraryId, out_boundsMin, out_boundsMax, &fRadius);
}

bool ugcComponentCalcBoundsForObjLib( int objlibId, Vec3 out_boundsMin, Vec3 out_boundsMax, float *pOut_Radius)
{
	GroupDef* editingDef;
	if( !objlibId ) {
		setVec3( out_boundsMin, 0, 0, 0 );
		setVec3( out_boundsMax, 0, 0, 0 );
		if (pOut_Radius) *pOut_Radius = 0.0f;
		return false;
	}

	editingDef = objectLibraryGetGroupDef( objlibId, true );
	if( editingDef ) {
		copyVec3( editingDef->bounds.min, out_boundsMin );
		copyVec3( editingDef->bounds.max, out_boundsMax );
		if (pOut_Radius) *pOut_Radius = editingDef->bounds.radius;
		return true;
	} else {
		setVec3( out_boundsMin, 0, 0, 0 );
		setVec3( out_boundsMax, 0, 0, 0 );
		if (pOut_Radius) *pOut_Radius = 0.0f;
		return false;
	}
}

bool ugcComponentLayoutCompatible(UGCComponentType component_type, UGCMapType map_type)
{
	switch (component_type)
	{
	case UGC_COMPONENT_TYPE_KILL:
	case UGC_COMPONENT_TYPE_ACTOR:
	case UGC_COMPONENT_TYPE_CONTACT:
	case UGC_COMPONENT_TYPE_SPAWN:
	case UGC_COMPONENT_TYPE_RESPAWN:
	case UGC_COMPONENT_TYPE_OBJECT:
	case UGC_COMPONENT_TYPE_SOUND:
	case UGC_COMPONENT_TYPE_DESTRUCTIBLE:
	case UGC_COMPONENT_TYPE_ROOM_MARKER:
	case UGC_COMPONENT_TYPE_DIALOG_TREE:
	case UGC_COMPONENT_TYPE_PATROL_POINT:
	case UGC_COMPONENT_TYPE_TELEPORTER:
	case UGC_COMPONENT_TYPE_TELEPORTER_PART:
		return true;
	case UGC_COMPONENT_TYPE_COMBAT_JOB:
	case UGC_COMPONENT_TYPE_TRAP:
	case UGC_COMPONENT_TYPE_TRAP_TARGET:
	case UGC_COMPONENT_TYPE_TRAP_TRIGGER:
	case UGC_COMPONENT_TYPE_TRAP_EMITTER:
		return (map_type == UGC_MAP_TYPE_GROUND || map_type == UGC_MAP_TYPE_PREFAB_GROUND ||
			map_type == UGC_MAP_TYPE_INTERIOR || map_type == UGC_MAP_TYPE_PREFAB_INTERIOR || map_type == UGC_MAP_TYPE_ANY);
	case UGC_COMPONENT_TYPE_WHOLE_MAP:
		return false;
	case UGC_COMPONENT_TYPE_ROOM:
	case UGC_COMPONENT_TYPE_ROOM_DOOR:
	case UGC_COMPONENT_TYPE_FAKE_DOOR:
	case UGC_COMPONENT_TYPE_REWARD_BOX:
		return true;
	case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
	case UGC_COMPONENT_TYPE_CLUSTER:
	case UGC_COMPONENT_TYPE_CLUSTER_PART:
		return (map_type == UGC_MAP_TYPE_GROUND || map_type == UGC_MAP_TYPE_PREFAB_GROUND || map_type == UGC_MAP_TYPE_ANY);
	case UGC_COMPONENT_TYPE_PLANET:
		return (map_type == UGC_MAP_TYPE_SPACE || map_type == UGC_MAP_TYPE_PREFAB_SPACE || map_type == UGC_MAP_TYPE_ANY);
	case UGC_COMPONENT_TYPE_EXTERNAL_DOOR:
	default:
		return false;
	}
}

bool componentStateCanBecomeHidden(UGCComponentType type)
{
	if(   type == UGC_COMPONENT_TYPE_OBJECT
		  || type == UGC_COMPONENT_TYPE_SOUND
		  || type == UGC_COMPONENT_TYPE_BUILDING_DEPRECATED
		  || type == UGC_COMPONENT_TYPE_PLANET
		  || type == UGC_COMPONENT_TYPE_TRAP
		  || type == UGC_COMPONENT_TYPE_ROOM_DOOR
		  || type == UGC_COMPONENT_TYPE_CONTACT
		  || type == UGC_COMPONENT_TYPE_KILL
		  || type == UGC_COMPONENT_TYPE_CLUSTER_PART ) {
		return true;
	}
	return false;
}

bool componentStateCanHaveCheckedAttrib(UGCComponentType type)
{
	if (type == UGC_COMPONENT_TYPE_SOUND || type == UGC_COMPONENT_TYPE_OBJECT || type == UGC_COMPONENT_TYPE_CLUSTER_PART)
	{
		return true;
	}
	return false;
}

bool ugcComponentStateCanBeEdited(UGCProjectData* ugcProj, UGCComponent* component)
{
	if(   component->eType != UGC_COMPONENT_TYPE_KILL
		  && component->eType != UGC_COMPONENT_TYPE_CONTACT
		  && component->eType != UGC_COMPONENT_TYPE_OBJECT
		  && component->eType != UGC_COMPONENT_TYPE_SOUND
		  && component->eType != UGC_COMPONENT_TYPE_DESTRUCTIBLE
		  && component->eType != UGC_COMPONENT_TYPE_TRAP
		  && component->eType != UGC_COMPONENT_TYPE_ROOM_DOOR
		  && component->eType != UGC_COMPONENT_TYPE_BUILDING_DEPRECATED
		  && component->eType != UGC_COMPONENT_TYPE_PLANET
		  && component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE
		  && component->eType != UGC_COMPONENT_TYPE_CLUSTER_PART ) {
		return false;
	}

	if (ugcMissionFindLinkByExitComponent( ugcProj, component->uID )) {
		return false;
	}
	if (  component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR
		  && ugcObjectiveFindComponentRelated( ugcProj->mission->objectives, ugcProj->components, component->uID )) {
		return false;
	}

	return true;
}

bool ugcComponentCanReparent(UGCComponentType type)
{
	if(   type == UGC_COMPONENT_TYPE_ACTOR
		  || type == UGC_COMPONENT_TYPE_TRAP_TARGET
		  || type == UGC_COMPONENT_TYPE_TRAP_TRIGGER
		  || type == UGC_COMPONENT_TYPE_TRAP_EMITTER
		  || type == UGC_COMPONENT_TYPE_ROOM
		  || type == UGC_COMPONENT_TYPE_TELEPORTER_PART
		  || type == UGC_COMPONENT_TYPE_CLUSTER_PART ) {
		return false;
	}
	return true;
}

F32 ugcComponentGetWorldHeight(const UGCComponent *component, const UGCComponentList *components)
{
	const UGCComponent *parent_component = NULL;
	F32 component_height = 0;
	parent_component = component;
	while (parent_component)
	{
		component_height += parent_component->sPlacement.vPos[1];
		parent_component = ugcComponentFindByID((UGCComponentList*)components, parent_component->uParentID);
	}
	return component_height;
}

static StashTable g_UGCPrefabMapPlatforms = NULL;

HeightMapExcludeGrid *ugcMapGetPrefabPlatforms(const char *map_name)
{
	HeightMapExcludeGrid *ret = NULL;
	UGCMapPlatformData *platform_data;

	if (!g_UGCPrefabMapPlatforms)
		g_UGCPrefabMapPlatforms = stashTableCreateAddress(16);

	map_name = allocAddString(map_name);
	if (stashFindPointer(g_UGCPrefabMapPlatforms, map_name, &ret))
		return ret;

	ugcPlatformDictionaryLoad();

	platform_data = RefSystem_ReferentFromString(UGC_PLATFORM_INFO_DICT, map_name);
	if (platform_data)
	{
		ret = ugcMapEditorGenerateExclusionGrid(platform_data);
		stashAddPointer(g_UGCPrefabMapPlatforms, map_name, ret, true);
	}
	return ret;
}

bool ugcComponentAllowFreePlacement(UGCComponentType type)
{
	if (type == UGC_COMPONENT_TYPE_ACTOR ||
		type == UGC_COMPONENT_TYPE_KILL ||
		type == UGC_COMPONENT_TYPE_CONTACT ||
		type == UGC_COMPONENT_TYPE_SPAWN ||
		type == UGC_COMPONENT_TYPE_REWARD_BOX ||
		type == UGC_COMPONENT_TYPE_RESPAWN ||
		type == UGC_COMPONENT_TYPE_COMBAT_JOB ||
		type == UGC_COMPONENT_TYPE_PATROL_POINT ||
		type == UGC_COMPONENT_TYPE_ROOM_DOOR ||
		type == UGC_COMPONENT_TYPE_FAKE_DOOR)
		return false;
	return true;
}

bool ugcComponentAllow3DRotation(UGCComponentType type)
{
	return (type == UGC_COMPONENT_TYPE_OBJECT
			|| type == UGC_COMPONENT_TYPE_CLUSTER_PART
			|| type == UGC_COMPONENT_TYPE_REWARD_BOX
			|| type == UGC_COMPONENT_TYPE_TRAP_EMITTER
			|| type == UGC_COMPONENT_TYPE_TELEPORTER_PART
			|| type == UGC_COMPONENT_TYPE_RESPAWN);
}

static UGCComponentValidPosition **ugcComponentFindValidPlatforms(const UGCProjectData *ugcProj, UGCBacklinkTable* pBacklinkTable, const UGCComponent *component, const Vec3 world_pos)
{
	static int test_num = 1;
	UGCComponentValidPosition **ret = NULL;
	const UGCComponent** eaMapComponents = NULL;

	devassertmsg(component->eType != UGC_COMPONENT_TYPE_ROOM_DOOR && component->eType != UGC_COMPONENT_TYPE_FAKE_DOOR,
		"Doors do not need to be positioned above platforms. Use ugcComponentFindValidDoorSlots instead of ugcComponentFindValidPlatforms.");
	ugcBacklinkTableGetMapComponents( ugcProj, pBacklinkTable, component->sPlacement.pcMapName, &eaMapComponents );

	FOR_EACH_IN_EARRAY(eaMapComponents, const UGCComponent, other_component)
	{
		if (  other_component != component
			  && !ugcComponentHasParent( ugcProj->components, other_component, component->uID ))
		{
			UGCRoomInfo *room_info = ugcRoomGetRoomInfo(SAFE_MEMBER(other_component, iObjectLibraryId));
			if (room_info && eaSize(&room_info->platform_grids) > 0)
			{
				F32* positions = NULL;
				Mat4 item_matrix;
				identityMat4(item_matrix);

				FOR_EACH_IN_EARRAY(room_info->platform_grids, HeightMapExcludeGrid, grid)
				{
					if (!grid)
						continue;

					// Calculate position relative to our platforms
					ugcRoomConvertWorldToLocal(other_component, world_pos, item_matrix[3]);

					exclusionCalculateObjectHeight(grid, item_matrix, 100, test_num++, true, -1, &positions, NULL);

					if (eafSize(&positions) != 0)
					{
						UGCComponentValidPosition *result;

						if (component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR ||
								component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR)
						{
							int pos_idx;
							int valid_door_idx = -1;
							F32 valid_door_dist = 1000;

							for (pos_idx = 0; pos_idx < eafSize(&positions); pos_idx++)
							{
								int door_idx = -1;
								Vec2 pos = { item_matrix[3][0], item_matrix[3][2] };
								F32 dist = ugcRoomGetNearestDoor(other_component, room_info, pos, &door_idx);
								if (dist < UGC_KIT_GRID && dist < valid_door_dist)
								{
									UGCComponent *door_component = NULL;
									UGCDoorSlotState door_state = ugcRoomGetDoorSlotState(ugcProj->components, other_component, door_idx, &door_component, NULL, NULL, NULL);
									if (door_state == UGC_DOOR_SLOT_EMPTY
										|| (door_state == UGC_DOOR_SLOT_OCCUPIED && door_component == component))
									{
										valid_door_dist = dist;
										valid_door_idx = door_idx;
									}
								}
							}
							if (valid_door_idx == -1)
							{
								eafDestroy(&positions);
								continue; // No door here
							}

							result = calloc(1, sizeof(UGCComponentValidPosition));

							result->room_door = valid_door_idx;

							// Snap to door
							{
								Vec3 door_local_pos;
								ugcRoomGetDoorLocalPos(room_info, valid_door_idx, door_local_pos);
								ugcRoomConvertLocalToWorld(other_component, door_local_pos, result->position);
							}
							result->rotation = other_component->sPlacement.vRotPYR[1] + ugcRoomGetDoorLocalRot(room_info, valid_door_idx);
						}
						else
						{
							result = calloc(1, sizeof(UGCComponentValidPosition));
							result->rotation = component->sPlacement.vRotPYR[1];
							copyVec3(world_pos, result->position);
						}
						result->platform_height = positions[0];
						result->room_id = other_component->uID;
						result->room_level = FOR_EACH_IDX(room_info->platform_grids, grid);
						eaPush(&ret, result);

						eafDestroy(&positions);
					}
				}
				FOR_EACH_END;
			}
		}
	}
	FOR_EACH_END;

	eaDestroy( &eaMapComponents );

	return ret;
}

static UGCComponentValidPosition **ugcComponentFindValidDoorSlots(const UGCProjectData *ugcProj, const UGCComponent *component, const Vec3 world_pos)
{
	UGCComponentValidPosition **ret = NULL;

	devassertmsg(component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR || component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR,
		"Only doors can be positioned in door slots. Use ugcComponentFindValidPlatforms instead of ugcComponentFindValidDoorSlots.");

	UGC_FOR_EACH_COMPONENT_OF_TYPE_ON_MAP(ugcProj->components, UGC_COMPONENT_TYPE_ROOM, component->sPlacement.pcMapName, room_component)
	{
		if (  room_component != component && !ugcComponentHasParent( ugcProj->components, room_component, component->uID ))
		{
			UGCRoomInfo *room_info = ugcRoomGetRoomInfo(SAFE_MEMBER(room_component, iObjectLibraryId));
			if (room_info)
			{
				Mat4 item_matrix;
				identityMat4(item_matrix);

				// Calculate relative position
				ugcRoomConvertWorldToLocal(room_component, world_pos, item_matrix[3]);

				{
					UGCComponentValidPosition *result = NULL;

					int valid_door_idx = -1;
					F32 valid_door_dist = 1000;
					
					int door_idx = -1;
					Vec2 pos = { item_matrix[3][0], item_matrix[3][2] };
					F32 dist = ugcRoomGetNearestDoor(room_component, room_info, pos, &door_idx);
					if (dist < UGC_KIT_GRID && dist < valid_door_dist)
					{
						UGCComponent *door_component = NULL;
						UGCDoorSlotState door_state = ugcRoomGetDoorSlotState(ugcProj->components, room_component, door_idx, &door_component, NULL, NULL, NULL);
						if (door_state == UGC_DOOR_SLOT_EMPTY
							|| (door_state == UGC_DOOR_SLOT_OCCUPIED && door_component == component))
						{
							valid_door_dist = dist;
							valid_door_idx = door_idx;
						}
					}
				
					if (valid_door_idx == -1)
						continue; // No door here

					result = calloc(1, sizeof(UGCComponentValidPosition));

					result->room_door = valid_door_idx;

					// Snap to door
					{
						Vec3 door_local_pos;
						ugcRoomGetDoorLocalPos(room_info, valid_door_idx, door_local_pos);
						ugcRoomConvertLocalToWorld(room_component, door_local_pos, result->position);
						result->room_level = -1;
					}
					result->rotation = room_component->sPlacement.vRotPYR[1] + ugcRoomGetDoorLocalRot(room_info, valid_door_idx);

					result->room_id = room_component->uID;
					eaPush(&ret, result);
				}
			}
		}
	}
	UGC_FOR_EACH_COMPONENT_END;

	return ret;
}

UGCComponentValidPosition **ugcComponentFindValidPositions(const UGCProjectData *ugcProj, UGCBacklinkTable* pBacklinkTable, const UGCComponent *component, const Vec3 world_pos)
{
	if(component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR || component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR)
		return ugcComponentFindValidDoorSlots(ugcProj, component, world_pos);
	else
		return ugcComponentFindValidPlatforms(ugcProj, pBacklinkTable, component, world_pos);
}

bool ugcComponentIsValidPosition(const UGCProjectData *ugcProj, UGCBacklinkTable* pBacklinkTable, const UGCComponent *component, const Vec3 world_pos_unsnapped, U32 *room_levels, bool snapping, F32 snap_xz, F32 snap_y, UGCComponentValidPosition *out_position)
{
	static int test_num = 1;
	UGCMapType map_type;
	UGCMap *map;
	Vec3 min_pos, max_pos;
	Vec3 component_min, component_max;
	Vec3 world_pos;

	if (snapping)
	{
		world_pos[0] = floor(world_pos_unsnapped[0]/snap_xz+0.5f)*snap_xz;
		world_pos[1] = floor(world_pos_unsnapped[1]/snap_y+0.5f)*snap_y;
		world_pos[2] = floor(world_pos_unsnapped[2]/snap_xz+0.5f)*snap_xz;
	}
	else
	{
		copyVec3(world_pos_unsnapped, world_pos);
	}

	if (out_position)
	{
		// Initialize output
		copyVec3(world_pos, out_position->position);
		out_position->rotation = component->sPlacement.vRotPYR[1];
		out_position->room_id = 0;
		out_position->room_door = -1;
		out_position->room_level = -1;
	}

	map = ugcMapFindByName(ugcProj, component->sPlacement.pcMapName);
	if (!map)
		return true; // Unplaced. We check for this elsewhere

	map_type = ugcMapGetType(map);

	copyVec3(world_pos, component_min);
	copyVec3(world_pos, component_max);
	if (component->eType == UGC_COMPONENT_TYPE_ROOM)
	{
		UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);
		if (room_info)
		{
			int irot = ROT_TO_QUADRANT(RAD(component->sPlacement.vRotPYR[1]));
			IVec2 room_min, room_max;
			ugcRoomRotateBounds(room_info->footprint_min, room_info->footprint_max, irot, room_min, room_max);
			component_min[0] = world_pos[0] + room_min[0]*UGC_ROOM_GRID;
			component_min[2] = world_pos[2] + room_min[1]*UGC_ROOM_GRID;
			component_max[0] = world_pos[0] + room_max[0]*UGC_ROOM_GRID;
			component_max[2] = world_pos[2] + room_max[1]*UGC_ROOM_GRID;
		}
	}
	
	// Do bounds check first
	ugcMapComponentValidBounds( min_pos, max_pos, ugcProj, pBacklinkTable, map, component );
	if (component_min[0] < min_pos[0] ||
		component_max[0] > max_pos[0] ||
		component_min[2] < min_pos[2] ||
		component_max[2] > max_pos[2])
		return false;

	// These components are always in valid positions, because they
	// serve no actual purpose.
	if( component->eType == UGC_COMPONENT_TYPE_KILL || component->eType == UGC_COMPONENT_TYPE_CLUSTER ) {
		return true;
	}

	switch (map_type)
	{
		xcase UGC_MAP_TYPE_PREFAB_INTERIOR: case UGC_MAP_TYPE_PREFAB_GROUND: {
			const WorldUGCProperties* props = ugcResourceGetUGCProperties( "ZoneMap", map->pPrefab->map_name );
			bool allow_terrain = !SAFE_MEMBER( props, bMapOnlyPlatformsAreLegal );

			// Do a collision check against the platform grid
			HeightMapExcludeGrid *platform_grid = ugcMapGetPrefabPlatforms(map->pPrefab->map_name);
			if (platform_grid && !allow_terrain)
			{
				F32* positions = NULL;
				Mat4 item_matrix;
				identityMat4(item_matrix);
				item_matrix[3][0] = world_pos[0];
				item_matrix[3][2] = world_pos[2];
				exclusionCalculateObjectHeight(platform_grid, item_matrix, 100, test_num++, true, -1, &positions, NULL);

				if (eafSize(&positions) == 0)
					return false;
				eafDestroy(&positions);
			}
			return true;
		}
		xcase UGC_MAP_TYPE_INTERIOR: {
			UGCComponentValidPosition **results;
			UGCComponentValidPosition *best_result = NULL;

			// Rooms can be placed anywhere; cannot be parented to a room
			if (component->eType == UGC_COMPONENT_TYPE_ROOM)
				return true;

			results = ugcComponentFindValidPositions(ugcProj, pBacklinkTable, component, world_pos);

			FOR_EACH_IN_EARRAY(results, UGCComponentValidPosition, result)
			{
				int level_idx;
				int level = component->sPlacement.iRoomLevel;
				UGCComponent *room_component = ugcComponentFindByID(ugcProj->components, result->room_id);
				if (room_component)
				{
					for (level_idx = 0; level_idx < eaiSize(&room_levels); level_idx += 2)
					{
						if (room_levels[level_idx] == result->room_id)
						{
							level = room_levels[level_idx+1];
						}
					}

					if (room_component->eType == UGC_COMPONENT_TYPE_ROOM && (result->room_level == level || result->room_level < 0 || level < 0))
					{
						best_result = result;
					}
					else if (room_component->eType != UGC_COMPONENT_TYPE_ROOM && (room_component->sPlacement.iRoomLevel == level || room_component->sPlacement.iRoomLevel < -1 || level < 0))
					{
						best_result = result;
						break;
					}
				}
			}
			FOR_EACH_END;

			if (best_result)
			{
				if (out_position)
				{
					memcpy(out_position, best_result, sizeof(UGCComponentValidPosition));
				}

				eaDestroyEx(&results, NULL);
				return true;
			}
			eaDestroyEx(&results, NULL);

			// Not in a room; only allow certain components through
			return ugcComponentAllowFreePlacement(component->eType);
		}

		default:
			// For all other region types, allow placement anywhere
			return true;
	}
}

void ugcComponentSetValidPosition(UGCProjectData *ugcProj, UGCComponent *component, UGCComponentValidPosition *position)
{
	copyVec3(position->position, component->sPlacement.vPos);
	component->sPlacement.vRotPYR[1] = position->rotation;
	if(component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR || component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR)
	{
		component->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE;
		component->sPlacement.iRoomLevel = -1;
		component->iRoomDoorID = position->room_door;
	}
	else
	{
		component->sPlacement.iRoomLevel = position->room_level;
		component->iRoomDoorID = -1;
	}

	if (component->uParentID != position->room_id && ugcComponentCanReparent(component->eType)) {
		ugcComponentOpSetParent(ugcProj, component, position->room_id);
	}
}

void ugcComponentGetValidPosition(UGCComponentValidPosition* out_position, UGCProjectData* ugcProj, UGCComponent* component)
{
	copyVec3(component->sPlacement.vPos, out_position->position);
	out_position->rotation = component->sPlacement.vRotPYR[1];
	out_position->room_level = component->sPlacement.iRoomLevel;
	{
		UGCComponent* room = ugcComponentGetRoomParent( ugcProj->components, component );
		out_position->room_id = SAFE_MEMBER( room, uID );
	}
	out_position->room_door = component->iRoomDoorID;
}

UGCMapType ugcMapGetTypeEx(const UGCMap *map, bool noPrefabType)
{
	UGCPerProjectDefaults *defaults = ugcGetDefaults();
	if(!map || !defaults)
		return UGC_MAP_TYPE_ANY;

	if (map->pSpace)
		return UGC_MAP_TYPE_SPACE;
	else if (map->pPrefab)
	{
		if (stricmp(defaults->pcCustomInteriorMap, map->pPrefab->map_name) == 0)
			return UGC_MAP_TYPE_INTERIOR;
		return ugcMapGetPrefabType(map->pPrefab->map_name, noPrefabType);
	}
	else if (map->pUnitializedMap)
	{
		return map->pUnitializedMap->eType;
	}

	return UGC_MAP_TYPE_ANY;
}

UGCMapType ugcMapGetPrefabType(const char *map_name, bool noPrefabType)
{
	UGCPerProjectDefaults *defaults = ugcGetDefaults();
	ZoneMapInfo *zminfo = worldGetZoneMapByPublicName(map_name);

	// Handle the special maps that mean custom types.
	if( defaults ) {
		if( stricmp( map_name, defaults->pcCustomInteriorMap ) == 0 ) {
			return UGC_MAP_TYPE_INTERIOR;
		}
	}
	
	if (zminfo)
	{
		ZoneMapEncounterRegionInfo *zeni_region = ugcGetZoneMapDefaultRegion(map_name);
		if (zeni_region)
		{
			WorldRegion **regions = zmapInfoGetWorldRegions(zminfo);
			FOR_EACH_IN_EARRAY(regions, WorldRegion, region)
			{
				if (worldRegionGetRegionName( region ) == zeni_region->regionName)
				{
					return ugcWorldRegionGetPrefabType(zminfo, region, noPrefabType);
				}
			}
			FOR_EACH_END;
		}
		ErrorFilenamef(zmapInfoGetFilename(zminfo), "Map missing default UGC region.");
	}
	else
	{
		ErrorFilenamef(zmapInfoGetFilename(zminfo), "Map not found.");
	}
	return UGC_MAP_TYPE_ANY;
}

UGCMapType ugcWorldRegionGetPrefabType(const ZoneMapInfo* zminfo, const WorldRegion* region, bool noPrefabType)
{
	ResourceInfo* resInfo = ugcResourceGetInfo( "ZoneMap", zmapInfoGetPublicName( zminfo ));
	if( !region || !resInfo ) {
		return UGC_MAP_TYPE_ANY;
	}
		
	switch (worldRegionGetType(region))
	{
		case WRT_Space: case WRT_SectorSpace:
			if( noPrefabType ) {
				return UGC_MAP_TYPE_SPACE;
			} else {
				return UGC_MAP_TYPE_PREFAB_SPACE;
			}
		case WRT_Ground:
			if (ugcHasTagType(resInfo->resourceTags, "ZoneMap", "PrefabInteriorMap"))
				if( noPrefabType ) {
					return UGC_MAP_TYPE_INTERIOR;
				} else {
					return UGC_MAP_TYPE_PREFAB_INTERIOR;
				}
			else
				if( noPrefabType ) {
					return UGC_MAP_TYPE_GROUND;
				} else {
					return UGC_MAP_TYPE_PREFAB_GROUND;
				}
		case WRT_Indoor:
			if( noPrefabType ) {
				return UGC_MAP_TYPE_INTERIOR;
			} else {
				return UGC_MAP_TYPE_PREFAB_INTERIOR;
			}
		default:
			ErrorFilenamef(zmapInfoGetFilename(zminfo), "Couldn't detect default UGC region type for map.");
			return UGC_MAP_TYPE_ANY;
	}
}

const char* ugcMapGetDisplayName( UGCProjectData* ugcProj, const char* mapName )
{
	if( !mapName ) {
		return NULL;
	} else if( !resNamespaceIsUGC( mapName )) {
		ZoneMapInfo* zmapInfo = zmapInfoGetByPublicName( mapName );
		const char* mapDisplayName = NULL;

		if( zmapInfo ) {
			mapDisplayName = TranslateMessageKey( zmapInfoGetDisplayNameMsgKey( zmapInfo ));
		}

		if( mapDisplayName ) {
			return mapDisplayName;
		} else {
			return "<MISSING>";
		}
	} else {
		UGCMap* map = ugcMapFindByName( ugcProj, mapName );
		if( !SAFE_MEMBER( map, pcDisplayName )) {
			return "<MISSING>";
		} else {
			return map->pcDisplayName;
		}
	}
}

const char* ugcMapMissionName( UGCProjectData* data, UGCMissionObjective* objective )
{
	UGCMissionMapLink* link = NULL;
	assert( !ugcDefaultsSingleMissionEnabled() );

	if( objective ) {
		link = ugcMissionFindLinkByObjectiveID( data, objective->id, false );

		if( !link && objective->type == UGCOBJ_IN_ORDER && eaSize( &objective->eaChildren )) {
			link = ugcMissionFindLinkByObjectiveID( data, objective->eaChildren[0]->id, false );
		}
	}

	return ugcMapMissionLinkName( data, link );
}

const char* ugcMapMissionLinkName( UGCProjectData* data, UGCMissionMapLink* link )
{
	static char buffer[ 256 ];
	assert( !ugcDefaultsSingleMissionEnabled() );

	if( link && !nullStr( link->strOpenMissionName )) {
		return link->strOpenMissionName;
	} else {
		sprintf( buffer, "<UNNAMED TASK>" );
	}

	return buffer;
}

const char* ugcMapMissionLinkReturnText( const UGCMissionMapLink* link )
{
	if( link && !nullStr( link->strReturnText )) {
		return link->strReturnText;
	} else {
		return "Go to Next Map";
	}
}

bool ugcMapIsGround(UGCMap *map)
{
	return ugcMapTypeIsGround(ugcMapGetType(map));
}

bool ugcMapTypeIsGround(UGCMapType type)
{
	return type==UGC_MAP_TYPE_GROUND || type==UGC_MAP_TYPE_INTERIOR || type==UGC_MAP_TYPE_PREFAB_GROUND || type==UGC_MAP_TYPE_PREFAB_INTERIOR;
}

UGCMap *ugcMapFindByName( const UGCProjectData* projData, const char *map_name)
{
	FOR_EACH_IN_EARRAY(projData->maps, UGCMap, map)
	{
		if (resNamespaceBaseNameEq(map->pcName, map_name))
			return map;
	}
	FOR_EACH_END;
	return NULL;
}

UGCCostume *ugcCostumeFindByName( UGCProjectData* projData, const char *costume_name)
{
	FOR_EACH_IN_EARRAY(projData->costumes, UGCCostume, costume)
	{
		if (resNamespaceBaseNameEq(costume->astrName, costume_name))
			return costume;
	}
	FOR_EACH_END;
	return NULL;
}

UGCItem *ugcItemFindByName( UGCProjectData* projData, const char *item_name)
{
	FOR_EACH_IN_EARRAY(projData->items, UGCItem, item)
	{
		if (resNamespaceBaseNameEq(item->astrName, item_name))
			return item;
	}
	FOR_EACH_END;
	return NULL;
}

const char* ugcItemGetIconName( UGCItem* item )
{
	if( !nullStr( SAFE_MEMBER( item, strIcon ))) {
		return item->strIcon;
	} else {
		return "CF_Icon_NoPreview";
	}
}

bool ugcCostumeSpecifierExists( UGCProjectData* projData, const char* costume_name )
{
	if( nullStr( costume_name )) {
		return false;
	} else {
		UGCCostume* pUGCCostume = ugcCostumeFindByName( projData, costume_name );
		ResourceInfo *pCostumeInfo = NULL;

		if( !resNamespaceIsUGC( costume_name )) {
			pCostumeInfo = ugcResourceGetInfo("PlayerCostume", costume_name);
		}

		return pUGCCostume != NULL || pCostumeInfo != NULL;
	}
}

const char* ugcCostumeSpecifierGetDisplayName( const UGCProjectData* ugcProj, const char* costumeName )
{
	static char buffer[ 256 ];
	
	if( nullStr( costumeName )) {
		return NULL;
	} else {
		const UGCCostume *pUGCCostume = ugcCostumeFindByName((UGCProjectData*)ugcProj, costumeName);
		if (pUGCCostume) {
			if( !nullStr( pUGCCostume->pcDisplayName )) {
				return pUGCCostume->pcDisplayName;
			} else {
				return "<UNNAMED>";
			}
		} else {
			const WorldUGCProperties* props = ugcResourceGetUGCProperties("PlayerCostume", costumeName);
			const char* text = NULL;
			
			if (props)
				text = TranslateDisplayMessage(props->dVisibleName);
			if( !text ) {
				const char* allocedName = allocFindString( costumeName );
				// check on the zenis
				if( allocedName ) {
					FOR_EACH_IN_REFDICT( "ZoneMapEncounterInfo", ZoneMapEncounterInfo, zeni ) {
						int it;
						for( it = 0; it != eaSize( &zeni->objects ); ++it ) {
							ZoneMapEncounterObjectInfo* zeniObj = zeni->objects[ it ];
							if(   allocedName == REF_STRING_FROM_HANDLE( zeniObj->ugcContactCostume )
								  && IS_HANDLE_ACTIVE( zeniObj->displayName )) {
								text = TranslateMessageRef( zeniObj->displayName );
								break;
							}
						}
					} FOR_EACH_END;
				}
				
				if( !text ) {
					sprintf( buffer, "%s (UNTRANSLATED)", costumeName );
					text = buffer;
				}
			}

			return text;
		}
	}
}

PCPart* ugcCostumeMetadataGetPartByBone( UGCCostumeMetadata* pCostume, const char* astrBoneName )
{
	int i;

	if( !pCostume ) {
		return NULL;
	}

	astrBoneName = allocAddString( astrBoneName );
	for( i = eaSize( &pCostume->eaItemParts ) - 1; i >= 0; --i ) {
		if( astrBoneName == REF_STRING_FROM_HANDLE( pCostume->eaItemParts[ i ]->hBoneDef )) {
			return pCostume->eaItemParts[ i ];
		}
	}
	return NULL;
}

static void ugcCostumeApplyCostumeSlot( NOCONST(PlayerCostume)* out_pCostume, const UGCCostumeSlotDef* slotDef, const UGCCostumeSlot* slot )
{
	int* eaColors = slot->eaColors;
	const char** eaBones = slotDef->eaBones;
	UGCCostumeMetadata* boneGroupMetadata = ugcResourceGetCostumeMetadata( slot->astrCostume );
	int it;

	if( !boneGroupMetadata ) {
		return;
	}
	
	for( it = 0; it != eaSize( &eaBones ); ++it ) {
		PCBoneDef* bone = RefSystem_ReferentFromString( "PCBoneDef", eaBones[ it ]);
		PCPart* src = ugcCostumeMetadataGetPartByBone( boneGroupMetadata, eaBones[ it ]);
		NOCONST(PCPart)* dest = costumeTailor_GetPartByBone( out_pCostume, bone, NULL );
		assert( bone );

		if( src ) {
			assert( dest );
			StructCopyAllDeConst( parse_PCPart, src, dest );

			if( eaColors ) {
				RGBAToU8Color( dest->color0, eaColors[ 0 ]);
				RGBAToU8Color( dest->color1, eaColors[ 1 ]);
				RGBAToU8Color( dest->color2, eaColors[ 2 ]);
				RGBAToU8Color( dest->color3, eaColors[ 3 ]);

				dest->color0[3] = 0xFF;
				dest->color1[3] = 0xFF;
				dest->color2[3] = 0xFF;
				dest->color3[3] = 0xFF;
			}
		}
	}
}


bool ugcCostumeFindBodyScaleDef( const char* skelName, const char* name, bool allowBasic, bool allowAdvanced )
{
	PCSkeletonDef* skeletonDef = RefSystem_ReferentFromString( "CostumeSkeleton", skelName );

	if( skeletonDef ) {
		int regionIt;
		for( regionIt = 0; regionIt != eaSize( &skeletonDef->eaRegions ); ++regionIt ) {
			const char* regionName = REF_STRING_FROM_HANDLE( skeletonDef->eaRegions[ regionIt ]->hRegion );
			UGCCostumeRegionDef* regionDef = ugcDefaultsCostumeRegionDef( regionName );

			if( !regionDef ) {
				continue;
			}
			if( allowBasic ) {
				if( eaFindString( &regionDef->nwBasic.eaBodyScales, name ) >= 0 ) {
					return true;
				}
			}
			if( allowAdvanced ) {
				if( eaFindString( &regionDef->nwAdvanced.eaBodyScales, name ) >= 0 ) {
					return true;
				}
			}
		}
	}

	return false;
}


bool ugcCostumeFindScaleInfoDef( const char* skelName, const char* name, bool allowBasic, bool allowAdvanced )
{
	PCSkeletonDef* skeletonDef = RefSystem_ReferentFromString( "CostumeSkeleton", skelName );

	if( skeletonDef ) {
		int regionIt;
		for( regionIt = 0; regionIt != eaSize( &skeletonDef->eaRegions ); ++regionIt ) {
			const char* regionName = REF_STRING_FROM_HANDLE( skeletonDef->eaRegions[ regionIt ]->hRegion );
			UGCCostumeRegionDef* regionDef = ugcDefaultsCostumeRegionDef( regionName );

			if( !regionDef ) {
				continue;
			}
			if( allowBasic ) {
				if( eaFindString( &regionDef->nwBasic.eaScaleInfos, name ) >= 0 ) {
					return true;
				}
			}
			if( allowAdvanced ) {
				if( eaFindString( &regionDef->nwAdvanced.eaScaleInfos, name ) >= 0 ) {
					return true;
				}
			}
		}
	}

	return false;
}


UGCCostumeNWPartDef* ugcCostumeFindPartDef( const char* skelName, const char* name, bool allowBasic, bool allowAdvanced )
{
	PCSkeletonDef* skeletonDef = RefSystem_ReferentFromString( "CostumeSkeleton", skelName );

	if( skeletonDef ) {
		int regionIt;
		for( regionIt = 0; regionIt != eaSize( &skeletonDef->eaRegions ); ++regionIt ) {
			const char* regionName = REF_STRING_FROM_HANDLE( skeletonDef->eaRegions[ regionIt ]->hRegion );
			UGCCostumeRegionDef* regionDef = ugcDefaultsCostumeRegionDef( regionName );

			if( regionDef ) {
				if( allowBasic ) {
					int it;
					for( it = 0; it != eaSize( &regionDef->nwBasic.eaParts ); ++it ) {
						UGCCostumeNWPartDef* partDef = regionDef->nwBasic.eaParts[ it ];
						if( partDef->astrName == name ) {
							return partDef;
						}
					}
				}
				if( allowAdvanced ) {
					int it;
					for( it = 0; it != eaSize( &regionDef->nwAdvanced.eaParts ); ++it ) {
						UGCCostumeNWPartDef* partDef = regionDef->nwAdvanced.eaParts[ it ];
						if( partDef->astrName == name ) {
							return partDef;
						}
					}
				}
			}
		}
	}

	return NULL;
}

UGCCostumeSlotDef* ugcCostumeFindSlotDef( const char* skelName, const char* astrName )
{
	UGCCostumeSkeletonSlotDef* skelDef = ugcDefaultsCostumeSkeletonDef( skelName );

	astrName = allocFindString( astrName );
	if( skelDef && astrName ) {
		int it;
		for( it = 0; it != eaSize( &skelDef->eaSlotDef ); ++it ) {
			UGCCostumeSlotDef* slotDef = skelDef->eaSlotDef[ it ];
			if( slotDef->astrName == astrName ) {
				return slotDef;
			}
		}
	}

	return NULL;
}

UGCCostumeScale* ugcCostumeFindBodyScale( UGCCostume* ugcCostume, const char* scaleName )
{
	int it;

	scaleName = allocFindString( scaleName );
	for( it = 0; it != eaSize( &ugcCostume->data.eaBodyScales ); ++it ) {
		UGCCostumeScale* bodyScale = ugcCostume->data.eaBodyScales[ it ];
		if( bodyScale->astrName == scaleName ) {
			return bodyScale;
		}
	}

	return NULL;
}

UGCCostumeScale* ugcCostumeFindScaleInfo( UGCCostume* ugcCostume, const char* scaleName )
{
	int it;

	scaleName = allocFindString( scaleName );
	for( it = 0; it != eaSize( &ugcCostume->data.eaScales ); ++it ) {
		UGCCostumeScale* scale = ugcCostume->data.eaScales[ it ];
		if( scale->astrName == scaleName ) {
			return scale;
		}
	}

	return NULL;
}

UGCCostumePart* ugcCostumeFindPart( UGCCostume* ugcCostume, const char* boneName )
{
	int it;

	boneName = allocFindString( boneName );
	
	for( it = 0; it != eaSize( &ugcCostume->data.eaParts ); ++it ) {
		if( boneName == ugcCostume->data.eaParts[ it ]->astrBoneName ) {
			return ugcCostume->data.eaParts[ it ];
		}
	}

	return NULL;
}

UGCCostumeSlot* ugcCostumeFindSlot( UGCCostume* costume, const char* astrName )
{
	int it;
	for( it = 0; it != eaSize( &costume->data.eaSlots ); ++it ) {
		if( astrName == costume->data.eaSlots[ it ]->astrSlot ) {
			return costume->data.eaSlots[ it ];
		}
	}

	return NULL;
}

PlayerCostume* ugcCostumeGeneratePlayerCostume( UGCCostume* ugcCostume, UGCCostumeOverride* override, const char* namespace )
{
	PlayerCostume* playerCostume;
	NOCONST(PlayerCostume)* playerCostumeNoConst;

	if( ugcDefaultsCostumeEditorStyle() == UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR ) {
		if( override && override->type == UGC_COSTUME_OVERRIDE_ENTIRE_COSTUME ) {
			playerCostume = StructClone( parse_PlayerCostume, override->entireCostume );
		} else {
			playerCostume = StructClone( parse_PlayerCostume, ugcCostume->pPlayerCostume );
		}
		playerCostumeNoConst = CONTAINER_NOCONST( PlayerCostume, playerCostume );
		playerCostumeNoConst->pcName = NULL;
		playerCostumeNoConst->pcScope = NULL;
		playerCostumeNoConst->eCostumeType = kPCCostumeType_UGC;
		playerCostumeNoConst->eDefaultColorLinkAll = false;
	} else {
		UGCCostumeMetadata* costumeData = ugcResourceGetCostumeMetadata( ugcCostume->data.astrPresetCostumeName );
		PlayerCostume* basicCostume = SAFE_MEMBER( costumeData, pFullCostume );
		UGCCostumeSkeletonSlotDef* skeletonDef = NULL;

		if( !basicCostume ) {
			return NULL;
		}
		skeletonDef = ugcDefaultsCostumeSkeletonDef( REF_STRING_FROM_HANDLE( basicCostume->hSkeleton ));

		playerCostume = StructClone( parse_PlayerCostume, basicCostume );
		playerCostumeNoConst = CONTAINER_NOCONST( PlayerCostume, playerCostume );
		playerCostumeNoConst->pcName = NULL;
		playerCostumeNoConst->pcScope = NULL;
		
		playerCostumeNoConst->eCostumeType = kPCCostumeType_Unrestricted;
		REMOVE_HANDLE( playerCostume->hSpecies );
		costumeTailor_FillAllBones( playerCostumeNoConst, NULL, NULL, NULL, true, false, true );

		if( skeletonDef ) {
			int it;
				
			for( it = 0; it != eaSize( &ugcCostume->data.eaBodyScales ); ++it ) {
				UGCCostumeScale* scale = ugcCostume->data.eaBodyScales[ it ];

				if( ugcCostumeFindBodyScaleDef( skeletonDef->astrName, scale->astrName, true, ugcCostume->data.isAdvanced )) {
					int bodyScaleIndex = costumeTailor_FindBodyScaleInfoIndexByName( playerCostume, scale->astrName );
					eafSet( &playerCostumeNoConst->eafBodyScales, scale->value, bodyScaleIndex );
				}
			}
			for( it = 0; it != eaSize( &ugcCostume->data.eaScales ); ++it ) {
				UGCCostumeScale* scale = ugcCostume->data.eaScales[ it ];

				if( ugcCostumeFindScaleInfoDef( skeletonDef->astrName, scale->astrName, true, ugcCostume->data.isAdvanced )) { 
					NOCONST(PCScaleValue)* costumeScale = costumeTailor_FindScaleValueByNameNoConst( playerCostumeNoConst, scale->astrName );
					if( !costumeScale ) {
						costumeScale = StructCreateNoConst( parse_PCScaleValue );
						costumeScale->pcScaleName = allocAddString( scale->astrName );
						eaPush( &playerCostumeNoConst->eaScaleValues, costumeScale );
					}
						
					costumeScale->fValue = scale->value;
				}
			}
			for( it = 0; it != eaSize( &ugcCostume->data.eaParts ); ++it ) {
				UGCCostumePart* part = ugcCostume->data.eaParts[ it ];
				UGCCostumeNWPartDef* partDef = ugcCostumeFindPartDef( skeletonDef->astrName, part->astrBoneName, true, ugcCostume->data.isAdvanced );

				if( partDef ) {
					PCBoneDef* partBone = RefSystem_ReferentFromString( "PCBoneDef", part->astrBoneName );
					NOCONST(PCPart)* costumePart = NULL;

					if( partBone ) {
						costumePart = costumeTailor_GetPartByBone( playerCostumeNoConst, partBone, NULL );
						if( !costumePart ) {
							costumePart = StructCreateNoConst( parse_PCPart );
							SET_HANDLE_FROM_REFERENT( "PCBoneDef", partBone, costumePart->hBoneDef );
							eaPush( &playerCostumeNoConst->eaParts, costumePart );
						}
					}

					if( costumePart ) {
						if( partDef->enableGeometry && !nullStr( part->astrGeometryName )) {
							SET_HANDLE_FROM_STRING( "CostumeGeometry", part->astrGeometryName, costumePart->hGeoDef );
						}
						if( partDef->enableMaterial && !nullStr( part->astrMaterialName )) {
							SET_HANDLE_FROM_STRING( "CostumeMaterial", part->astrMaterialName, costumePart->hMatDef );
						}
						if( partDef->enableTextures && !nullStr( part->astrTexture0Name )) {
							SET_HANDLE_FROM_STRING( "CostumeTexture", part->astrTexture0Name, costumePart->hPatternTexture );
						}
						if( partDef->enableTextures && !nullStr( part->astrTexture1Name )) {
							SET_HANDLE_FROM_STRING( "CostumeTexture", part->astrTexture1Name, costumePart->hDetailTexture );
						}
						if( partDef->enableTextures && !nullStr( part->astrTexture2Name )) {
							SET_HANDLE_FROM_STRING( "CostumeTexture", part->astrTexture2Name, costumePart->hSpecularTexture );
						}
						if( partDef->enableTextures && !nullStr( part->astrTexture3Name )) {
							SET_HANDLE_FROM_STRING( "CostumeTexture", part->astrTexture3Name, costumePart->hDiffuseTexture );
						}
						if( partDef->enableColors && part->colors[ 0 ]) {
							RGBAToU8Color( costumePart->color0, part->colors[ 0 ]);
						}
						if( partDef->enableColors && part->colors[ 1 ]) {
							RGBAToU8Color( costumePart->color1, part->colors[ 1 ]);
						}
						if( partDef->enableColors && part->colors[ 2 ]) {
							RGBAToU8Color( costumePart->color2, part->colors[ 2 ]);
						}
						if( partDef->enableColors && part->colors[ 3 ]) {
							RGBAToU8Color( costumePart->color3, part->colors[ 3 ]);
						}

						if( override && override->astrName == part->astrBoneName ) {
							switch( override->type ) {
								xcase UGC_COSTUME_OVERRIDE_PART_GEOMETRY:
									SET_HANDLE_FROM_STRING( "CostumeGeometry", override->strValue, costumePart->hGeoDef );
								xcase UGC_COSTUME_OVERRIDE_PART_MATERIAL:
									SET_HANDLE_FROM_STRING( "CostumeMaterial", override->strValue, costumePart->hMatDef );
								xcase UGC_COSTUME_OVERRIDE_PART_TEXTURE0:
									SET_HANDLE_FROM_STRING( "CostumeTexture", override->strValue, costumePart->hPatternTexture );
								xcase UGC_COSTUME_OVERRIDE_PART_TEXTURE1:
									SET_HANDLE_FROM_STRING( "CostumeTexture", override->strValue, costumePart->hDetailTexture );
								xcase UGC_COSTUME_OVERRIDE_PART_TEXTURE2:
									SET_HANDLE_FROM_STRING( "CostumeTexture", override->strValue, costumePart->hSpecularTexture );
								xcase UGC_COSTUME_OVERRIDE_PART_TEXTURE3:
									SET_HANDLE_FROM_STRING( "CostumeTexture", override->strValue, costumePart->hDiffuseTexture );
								
								xcase UGC_COSTUME_OVERRIDE_PART_COLOR: {
									if( override->colorIndex == 0 ) {
										RGBAToU8Color( costumePart->color0, override->iValue );
									} else if( override->colorIndex == 1 ) {
										RGBAToU8Color( costumePart->color1, override->iValue );
									} else if( override->colorIndex == 2 ) {
										RGBAToU8Color( costumePart->color2, override->iValue );
									} else if( override->colorIndex == 3 ) {
										RGBAToU8Color( costumePart->color3, override->iValue );
									}
								}
							}
						}
					}
				}
			}
			for( it = 0; it != eaSize( &skeletonDef->eaSlotDef ); ++it ) {
				UGCCostumeSlotDef* def = skeletonDef->eaSlotDef[ it ];
				UGCCostumeSlot* slot = ugcCostumeFindSlot( ugcCostume, def->astrName );
				UGCCostumeSlot overrideSlot = { 0 };

				if( override && override->type == UGC_COSTUME_OVERRIDE_SLOT && override->astrName == def->astrName ) {
					overrideSlot.astrSlot = override->astrName;
					overrideSlot.astrCostume = override->strValue;
					slot = &overrideSlot;
				}
				if(   override && slot && override->type == UGC_COSTUME_OVERRIDE_SLOT_COLOR
					  && override->astrName == def->astrName ) {
					StructCopyAll( parse_UGCCostumeSlot, slot, &overrideSlot );
					eaiSet( &overrideSlot.eaColors, override->iValue, override->colorIndex );
					slot = &overrideSlot;
				}
				
				if( slot ) {
					ugcCostumeApplyCostumeSlot( playerCostumeNoConst, def, slot );
				}

				StructReset( parse_UGCCostumeSlot, &overrideSlot );
			}

			if( override && override->type == UGC_COSTUME_OVERRIDE_SKIN_COLOR ) {
				RGBAToU8Color( playerCostumeNoConst->skinColor, override->iValue );
			} else {
				RGBAToU8Color( playerCostumeNoConst->skinColor, ugcCostume->data.skinColor );
			}

			if( !override || override->type == UGC_COSTUME_OVERRIDE_NONE ) {
				playerCostumeNoConst->pcStance = ugcCostume->data.astrStance;
			}
			playerCostumeNoConst->fHeight = ugcCostume->data.fHeight;
		}
	}

	if( playerCostume ) {
		CONTAINER_NOCONST( PlayerCostume, playerCostume )->pcName = ugcCostume->astrName;

		if( namespace ) {
			char path[ MAX_PATH ];
			sprintf( path, "Maps/%s/Costumes", namespace );
			CONTAINER_NOCONST( PlayerCostume, playerCostume )->pcScope = allocAddString( path );
		}

		costumeTailor_MakeCostumeValid( playerCostumeNoConst, GET_REF( playerCostume->hSpecies ), NULL, NULL, true, true, false, NULL, true, NULL, false, NULL );		
	}

	return playerCostume;
}

void ugcCostumeRevertToPreset( UGCCostume* ugcCostume, const char* presetName )
{
	PlayerCostume* preset = ugcResourceGetCostumeMetadata( presetName )->pFullCostume;

	ugcCostume->data.astrPresetCostumeName = allocAddString( presetName );
	ugcCostume->data.fHeight =  preset->fHeight;
	ugcCostume->data.astrStance = preset->pcStance;
	ugcCostume->data.skinColor = u8ColorToRGBA( preset->skinColor );
	ugcCostume->data.isAdvanced = false;
	
	eaClearStruct( &ugcCostume->data.eaSlots, parse_UGCCostumeSlot );
	eaClearStruct( &ugcCostume->data.eaBodyScales, parse_UGCCostumeScale );
	eaClearStruct( &ugcCostume->data.eaScales, parse_UGCCostumeScale );
	eaClearStruct( &ugcCostume->data.eaParts, parse_UGCCostumePart );

	// Filling these out is handled in fixup
}

ResourceSearchResult *ugcProjectSearchRequest( UGCProjectData* projData, ResourceSearchRequest *request, UGCMapType map_type)
{
	ResourceSearchResult *result = StructCreate(parse_ResourceSearchResult);
	result->iRequest = request->iRequest;

	// If searching for costumes, iterate the costumes
	if (stricmp(request->pcType, "PlayerCostume") == 0) {
		FOR_EACH_IN_EARRAY(projData->costumes, UGCCostume, costume)
		{
			// Region types are STO-specific
			bool type_matches = true;
			if (ugcIsSpaceEnabled())
			{
				switch (map_type)
				{
				case UGC_MAP_TYPE_SPACE:
				case UGC_MAP_TYPE_PREFAB_SPACE:
					type_matches = costume->eRegion == (U32)StaticDefineIntGetInt(CharClassTypesEnum, "Space");
					break;
				case UGC_MAP_TYPE_GROUND:
				case UGC_MAP_TYPE_PREFAB_GROUND:
				case UGC_MAP_TYPE_INTERIOR:
				case UGC_MAP_TYPE_PREFAB_INTERIOR:
					type_matches = costume->eRegion == (U32)StaticDefineIntGetInt(CharClassTypesEnum, "Ground");
					break;
				}
			}
			if (type_matches)
			{
				ResourceSearchResultRow *row = StructCreate(parse_ResourceSearchResultRow);
				row->pcName = StructAllocString(costume->astrName);
				row->pcType = StructAllocString("UGCCostume");
				eaPush(&result->eaRows, row);
			}
		}
		FOR_EACH_END;
	}

	if (stricmp(request->pcType, "ItemDef") == 0) {
		FOR_EACH_IN_EARRAY(projData->items, UGCItem, item)
		{
			ResourceSearchResultRow *row = StructCreate(parse_ResourceSearchResultRow);
			row->pcName = StructAllocString(item->astrName);
			row->pcType = StructAllocString("UGCItem");
			eaPush(&result->eaRows, row);
		}
		FOR_EACH_END;
	}

	if (stricmp(request->pcType, "Trap") == 0) {
		ResourceSearchRequest objlibRequest = *request;
		ResourceSearchResult* objlibResult;
		int it;

		objlibRequest.pcType = "ObjectLibrary";
		objlibResult = ugcResourceSearchRequest( &objlibRequest );

		for( it = 0; it != eaSize( &objlibResult->eaRows ); ++it ) {
			ResourceSearchResultRow* row = objlibResult->eaRows[ it ];
			ResourceInfo* defInfo = ugcResourceGetInfo( "ObjectLibrary", row->pcName );
			char powerGroupName[256];

			if( defInfo && ugcGetTagValue( defInfo->resourceTags, "type", SAFESTR( powerGroupName ))) {
				UGCTrapPowerGroup* powerGroup = RefSystem_ReferentFromString( UGC_DICTIONARY_TRAP_POWER_GROUP, powerGroupName );
				if( powerGroup ) {
					int powerIt;
					for( powerIt = 0; powerIt != eaSize( &powerGroup->eaPowerNames ); ++powerIt ) {
						ResourceSearchResultRow* resultRow = StructCreate( parse_ResourceSearchResultRow );
						char buffer[ RESOURCE_NAME_MAX_SIZE ];

						sprintf( buffer, "%s,%s", row->pcName, powerGroup->eaPowerNames[ powerIt ]);
						resultRow->pcName = StructAllocString( buffer );
						resultRow->pcType = StructAllocString( "Trap" );
						eaPush( &result->eaRows, resultRow );
					}
				}
			}
		}

		StructDestroySafe( parse_ResourceSearchResult, &objlibResult );
	}

	if( stricmp( request->pcType, "CheckedAttrib" ) == 0 ) {
		UGCPerProjectDefaults* config = ugcGetDefaults();
		int it;
		for( it = 0; it != eaSize( &config->checkedAttribs ); ++it ) {
			UGCCheckedAttribDef* checkedAttribDef = config->checkedAttribs[ it ];
			ResourceSearchResultRow* resultRow = StructCreate( parse_ResourceSearchResultRow );
			resultRow->pcName = StructAllocString( checkedAttribDef->name );
			resultRow->pcType = StructAllocString( "CheckedAttrib" );
			eaPush( &result->eaRows, resultRow );
		}
	}

	// "Special" components
	if( stricmp( request->pcType, "Special" ) == 0 ) {
		UGCPerProjectDefaults* config = ugcGetDefaults();

		FOR_EACH_IN_EARRAY_FORWARDS(config->eaSpecialComponents, UGCSpecialComponentDef, def)
		{
			ResourceSearchResultRow* resultRow;
			if (def->bSpaceOnly && map_type != UGC_MAP_TYPE_SPACE && map_type != UGC_MAP_TYPE_PREFAB_SPACE)
				continue;
			if (def->bGroundOnly && map_type != UGC_MAP_TYPE_GROUND && map_type != UGC_MAP_TYPE_PREFAB_GROUND
				&& map_type != UGC_MAP_TYPE_INTERIOR && map_type != UGC_MAP_TYPE_PREFAB_INTERIOR)
				continue;
			if (def->eRestrictToMapType && def->eRestrictToMapType != map_type)
				continue;

			resultRow = StructCreate( parse_ResourceSearchResultRow );
			resultRow->pcType = StructAllocString( "Special" );
			resultRow->pcName = StructAllocString( def->pcLabel );
			eaPush( &result->eaRows, resultRow );
		}
		FOR_EACH_END;
	}

	return result;
}

static bool ugcResourceDictFilter( ResourceInfo* resInfo, const char* dictName )
{
	UGCResourceInfo* ugcInfo = RefSystem_ReferentFromString( UGC_DICTIONARY_RESOURCE_INFO, resInfo->resourceName );

	if( ugcInfo && ugcInfo->pResInfo && ugcInfo->pResInfo->resourceDict == dictName ) {
		return true;
	} else {
		return false;
	}
}

ResourceSearchResult* ugcResourceSearchRequest(const ResourceSearchRequest* request)
{
	if( stricmp( request->pcType, "Trap" ) == 0 ) {
		return StructCreate( parse_ResourceSearchResult );
	} else {
		ResourceSearchResult* result;
		ResourceSearchRequest realRequest = *request;
		if( realRequest.pcType ) {
			realRequest.filterFn = ugcResourceDictFilter;
			realRequest.filterData = (char*)allocAddString(realRequest.pcType);
		}
		realRequest.pcType = UGC_DICTIONARY_RESOURCE_INFO;

		result = handleResourceSearchRequest( &realRequest );
		FOR_EACH_IN_EARRAY(result->eaRows, ResourceSearchResultRow, row) {
			UGCResourceInfo* ugcInfo = RefSystem_ReferentFromString(UGC_DICTIONARY_RESOURCE_INFO, row->pcName);
			if( ugcInfo ) {
				StructCopyString( &row->pcName, ugcInfo->pResInfo->resourceName );
				StructCopyString( &row->pcType, ugcInfo->pResInfo->resourceDict );
			} else {
				StructDestroy( parse_ResourceSearchResultRow, row );
				eaRemove( &result->eaRows, FOR_EACH_IDX( result->eaRows, row ));
			}
		} FOR_EACH_END;

		return result;
	}
}

UGCComponent *ugcComponentFindByLogicalName(UGCComponentList *list, const char *name)
{
	return ugcComponentFindByID(list, ugcComponentNameGetID( name ));
}

UGCComponent *ugcComponentFindDefaultPromptForID(UGCComponentList *list, U32 id )
{
	int it;
	for( it = 0; it != eaSize( &list->eaComponents ); ++it ) {
		UGCComponent* component = list->eaComponents[ it ];
		if( component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE ) {
			continue;
		}

		if( component->uActorID == id && component->bIsDefault ) {
			return component;
		}
	}

	return NULL;
}

UGCComponent *ugcComponentFindPromptForID( UGCComponentList* list, U32 id )
{
	int it;
	for( it = 0; it != eaSize( &list->eaComponents ); ++it ) {
		UGCComponent* component = list->eaComponents[ it ];
		if( component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE ) {
			continue;
		}

		if( component->uActorID == id ) {
			return component;
		}
	}

	return NULL;
}

static int ugcComponentDialogTreeSortByBlockIndex( const UGCComponent** ppDialog1, const UGCComponent** ppDialog2 )
{
	return (*ppDialog1)->dialogBlock.blockIndex - (*ppDialog2)->dialogBlock.blockIndex;
}

UGCComponent** ugcComponentFindPopupPromptsForWhen( UGCComponentList* list, UGCWhenType type, U32 id )
{
	UGCComponent** accum = NULL;

	int it;
	for( it = 0; it != eaSize( &list->eaComponents ); ++it ) {
		UGCComponent* component = list->eaComponents[ it ];
		if( component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE ) {
			continue;
		}

		if( ugcComponentStartWhenType(component) == type && eaiGet( &component->eaObjectiveIDs, 0 ) == id ) {
			eaPush( &accum, component );
		}
	}

	eaQSort( accum, ugcComponentDialogTreeSortByBlockIndex );
	return accum;
}

UGCComponent** ugcComponentFindPopupPromptsForObjectiveStart(UGCComponentList *list, U32 id)
{
	return ugcComponentFindPopupPromptsForWhen( list, UGCWHEN_OBJECTIVE_START, id );
}

UGCComponent** ugcComponentFindPopupPromptsForObjectiveComplete(UGCComponentList *list, U32 id)
{
	return ugcComponentFindPopupPromptsForWhen( list, UGCWHEN_OBJECTIVE_COMPLETE, id );
}

UGCComponent** ugcComponentFindPopupPromptsForMissionStart(UGCComponentList *list)
{
	return ugcComponentFindPopupPromptsForWhen( list, UGCWHEN_MISSION_START, 0 );
}

UGCComponent** ugcComponentFindPopupPromptsForWhenInDialog(UGCComponentList *list, const UGCComponent* component)
{
	if( !component->pStartWhen ) {
		return NULL;
	} else {
		switch( component->pStartWhen->eType ) {
			xcase UGCWHEN_MISSION_START: case UGCWHEN_OBJECTIVE_START:
			case UGCWHEN_OBJECTIVE_COMPLETE:
				return ugcComponentFindPopupPromptsForWhen( list, component->pStartWhen->eType, ea32Get( &component->eaObjectiveIDs, 0 ));

			xdefault: {
				UGCComponent** accum = NULL;
				eaPush( &accum, (UGCComponent*)component );
				return accum;
			}
		}
	}
}

UGCComponent *ugcComponentFindPromptByContactAndObjective(UGCComponentList *list, U32 contactID, U32 objectiveID )
{
	int it;
	for( it = 0; it != eaSize( &list->eaComponents ); ++it ) {
		UGCComponent* component = list->eaComponents[ it ];
		if( component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE ) {
			continue;
		}

		if( component->uActorID == contactID && eaiFind( &component->eaObjectiveIDs, objectiveID ) >= 0 ) {
			return component;
		}
	}

	return NULL;
}

// The backlink table
typedef struct UGCBacklinkTable
{
	StashTable triggerTable;
	StashTable mapComponentsByMapName;
	StashTable dialogsByContactID;
} UGCBacklinkTable;

AUTO_STRUCT;
typedef struct UGCBacklinkTableComponentEntry
{
	U32* eaComponentIDs;
} UGCBacklinkTableComponentEntry;
extern ParseTable parse_UGCBacklinkTableComponentEntry[];
#define TYPE_parse_UGCBacklinkTableComponentEntry UGCBacklinkTableComponentEntry

#define TRIGGER_TABLE_ID_BITS 16
#define TRIGGER_TABLE_SUBID_BITS 16

#define TRIGGER_TABLE_ID_MASK ((1u << TRIGGER_TABLE_ID_BITS) - 1)
#define TRIGGER_TABLE_SUBID_MASK ((1u << TRIGGER_TABLE_SUBID_BITS) - 1)

static int TRIGGER_TABLE_KEY( U32 id, U32 subid )
{
	return ((id & TRIGGER_TABLE_ID_MASK)
			| ((subid & TRIGGER_TABLE_SUBID_MASK) << TRIGGER_TABLE_ID_BITS));
}

void ugcBacklinkTableDestroy( UGCBacklinkTable** ppTable )
{
	if( !*ppTable ) {
		return;
	}

	stashTableClearStruct( (*ppTable)->triggerTable, NULL, parse_UGCBacklinkTableComponentEntry );
	stashTableClearStruct( (*ppTable)->mapComponentsByMapName, NULL, parse_UGCBacklinkTableComponentEntry );
	stashTableClearStruct( (*ppTable)->dialogsByContactID, NULL, parse_UGCBacklinkTableComponentEntry );
	stashTableDestroy( (*ppTable)->triggerTable );
	stashTableDestroy( (*ppTable)->mapComponentsByMapName );
	stashTableDestroy( (*ppTable)->dialogsByContactID );
	SAFE_FREE( *ppTable );
}

static void ugcBacklinkTableInternalAddTrigger( UGCBacklinkTable* pBacklinkTable, U32 triggeringID, U32 triggeredID )
{
	UGCBacklinkTableComponentEntry* entry = NULL;
	assert( (triggeringID & TRIGGER_TABLE_ID_MASK) == triggeringID || triggeringID == -1 );
	stashIntFindPointer( pBacklinkTable->triggerTable, TRIGGER_TABLE_KEY( triggeringID, 0 ), &entry );

	if( !entry ) {
		entry = StructCreate( parse_UGCBacklinkTableComponentEntry );
		stashIntAddPointer( pBacklinkTable->triggerTable, triggeringID, entry, true );
	}

	ea32PushUnique( &entry->eaComponentIDs, triggeredID );
}

static void ugcBacklinkTableInternalAddDialogPromptTrigger( UGCBacklinkTable* pBacklinkTable, U32 triggeringDialogID, int triggeringPromptID, U32 triggeredID )
{
	UGCBacklinkTableComponentEntry* entry = NULL;
	assert( ((triggeringDialogID & TRIGGER_TABLE_ID_MASK) == triggeringDialogID || triggeringDialogID == -1)
			&& ((triggeringPromptID & TRIGGER_TABLE_SUBID_MASK) == (U32)triggeringPromptID || triggeringPromptID == -1) );
	stashIntFindPointer( pBacklinkTable->triggerTable, TRIGGER_TABLE_KEY( triggeringDialogID, triggeringPromptID ), &entry );

	if( !entry ) {
		entry = StructCreate( parse_UGCBacklinkTableComponentEntry );
		stashIntAddPointer( pBacklinkTable->triggerTable, TRIGGER_TABLE_KEY( triggeringDialogID, triggeringPromptID ), entry, true );
	}

	ea32PushUnique( &entry->eaComponentIDs, triggeringDialogID );
}

static void ugcBacklinkTableRefreshWhenHelper( UGCBacklinkTable* pBacklinkTable, UGCComponent* component, UGCWhen* when )
{
	if( !when ) {
		return;
	}

	if( when->eType == UGCWHEN_COMPONENT_COMPLETE || when->eType == UGCWHEN_COMPONENT_REACHED ) {
		int it;
		for( it = 0; it != ea32Size( &when->eauComponentIDs ); ++it ) {
			ugcBacklinkTableInternalAddTrigger( pBacklinkTable, when->eauComponentIDs[ it ], component->uID );
		}
	} else if( when->eType == UGCWHEN_CURRENT_COMPONENT_COMPLETE ) {
		ugcBacklinkTableInternalAddTrigger( pBacklinkTable, component->uID, component->uID );
	} else if( when->eType == UGCWHEN_DIALOG_PROMPT_REACHED ) {
		FOR_EACH_IN_EARRAY( when->eaDialogPrompts, UGCWhenDialogPrompt, whenPrompt ) {
			ugcBacklinkTableInternalAddDialogPromptTrigger( pBacklinkTable, whenPrompt->uDialogID, whenPrompt->iPromptID, component->uID );
		} FOR_EACH_END;
	}
}

static void ugcBacklinkTableRefreshPromptHelper( UGCBacklinkTable* pBacklinkTable, UGCComponent* dialogTree, UGCDialogTreePrompt* prompt )
{
	FOR_EACH_IN_EARRAY( prompt->eaActions, UGCDialogTreePromptAction, promptAction ) {
		ugcBacklinkTableRefreshWhenHelper( pBacklinkTable, dialogTree, promptAction->pShowWhen );
		ugcBacklinkTableRefreshWhenHelper( pBacklinkTable, dialogTree, promptAction->pHideWhen );
	} FOR_EACH_END;
}

/// Refresh all backlinks in BACKLINK-TABLE, so other functions can
/// quickly access the list of triggers and components on a map.
void ugcBacklinkTableRefresh( UGCProjectData* ugcProj, UGCBacklinkTable** ppBacklinkTable )
{
	if( !*ppBacklinkTable ) {
		*ppBacklinkTable = calloc( 1, sizeof( **ppBacklinkTable ));
		(*ppBacklinkTable)->triggerTable = stashTableCreateInt( 100 );
		(*ppBacklinkTable)->mapComponentsByMapName = stashTableCreateWithStringKeys( 20, StashDeepCopyKeys_NeverRelease );
		(*ppBacklinkTable)->dialogsByContactID = stashTableCreateInt( 100 );
	}	
	stashTableClearStruct( (*ppBacklinkTable)->triggerTable, NULL, parse_UGCBacklinkTableComponentEntry );
	stashTableClearStruct( (*ppBacklinkTable)->mapComponentsByMapName, NULL, parse_UGCBacklinkTableComponentEntry );
	stashTableClearStruct( (*ppBacklinkTable)->dialogsByContactID, NULL, parse_UGCBacklinkTableComponentEntry );

	if( !ugcProj ) {
		return;
	}

	FOR_EACH_IN_EARRAY( ugcProj->components->eaComponents, UGCComponent, component ) {
		ugcBacklinkTableRefreshWhenHelper( *ppBacklinkTable, component, component->pStartWhen );
		ugcBacklinkTableRefreshWhenHelper( *ppBacklinkTable, component, component->pHideWhen );
		
		if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
			int promptIt;
			ugcBacklinkTableRefreshPromptHelper( *ppBacklinkTable, component, &component->dialogBlock.initialPrompt );
			for( promptIt = 0; promptIt != eaSize( &component->dialogBlock.prompts ); ++promptIt ) {
				ugcBacklinkTableRefreshPromptHelper( *ppBacklinkTable, component, component->dialogBlock.prompts[ promptIt ]);
			}

			// Index dialogs by contact ID
			if( component->uActorID ) {
				UGCBacklinkTableComponentEntry* entry = NULL;
				stashIntFindPointer( (*ppBacklinkTable)->dialogsByContactID, component->uActorID, &entry );
				if( !entry ) {
					entry = StructCreate( parse_UGCBacklinkTableComponentEntry );
					stashIntAddPointer( (*ppBacklinkTable)->dialogsByContactID, component->uActorID, entry, true );
				}
				eaiPush( &entry->eaComponentIDs, component->uID );
			}
		}

		FOR_EACH_IN_EARRAY( component->fsmProperties.eaExternVarsV1, UGCFSMVar, externVar ) {
			if( externVar->pWhenVal ) {
				ugcBacklinkTableRefreshWhenHelper( *ppBacklinkTable, component, externVar->pWhenVal );
			}
		} FOR_EACH_END;

		// Index map components too.
		if( !component->sPlacement.bIsExternalPlacement && component->sPlacement.uRoomID != GENESIS_UNPLACED_ID ) {
			char mapNameSansNS[ RESOURCE_NAME_MAX_SIZE ];
			resExtractNameSpace_s( component->sPlacement.pcMapName, NULL, 0, SAFESTR( mapNameSansNS ));

			if( !nullStr( mapNameSansNS )) {
				UGCBacklinkTableComponentEntry* entry = NULL;
				stashFindPointer( (*ppBacklinkTable)->mapComponentsByMapName, mapNameSansNS, &entry );
				
				if( !entry ) {
					entry = StructCreate( parse_UGCBacklinkTableComponentEntry );
					stashAddPointer( (*ppBacklinkTable)->mapComponentsByMapName, mapNameSansNS, entry, true );
				}

				ea32Push( &entry->eaComponentIDs, component->uID );
			}
		}
	} FOR_EACH_END;
}

/// Return if any component has state changed when
/// completing/reaching/etc. the component with id TRIGGERING-ID.
///
/// For dialogs, TRIGGERING-PROMPT-ID lets you query about a specific
/// prompt.
bool ugcBacklinkTableFindTrigger( UGCBacklinkTable* pTable, U32 triggeringID, U32 triggeringPromptID )
{
	UGCBacklinkTableComponentEntry* entry = NULL;
	stashIntFindPointer( pTable->triggerTable, TRIGGER_TABLE_KEY( triggeringID, triggeringPromptID ), &entry );
	if( entry && eaiSize( &entry->eaComponentIDs )) {
		return true;
	} else {
		return false;
	}
}

/// Fill OUT-COMPONENTS with a list of all components that have their
/// state changed when completing/reaching/etc. the component with id
/// TRIGGERING-ID.
///
/// For dialogs, TRIGGERING-PROMPT-ID lets you query about a specific
/// prompt.
void ugcBacklinkTableFindAllTriggers( UGCProjectData* ugcProj, UGCBacklinkTable* pTable, U32 triggeringID, int triggeringPromptID, UGCComponent ***out_components )
{
	UGCBacklinkTableComponentEntry* entry = NULL;
	stashIntFindPointer( pTable->triggerTable, TRIGGER_TABLE_KEY( triggeringID, triggeringPromptID ), &entry );
	
	if( entry && eaiSize( &entry->eaComponentIDs )) {
		int it;
		for( it = 0; it != eaiSize( &entry->eaComponentIDs ); ++it ) {
			UGCComponent* component = ugcComponentFindByID( ugcProj->components, entry->eaComponentIDs[ it ]);
			if( component ) {
				eaPush( out_components, component );
			}
		}
	}
}

/// Const version of the function above.
void ugcBacklinkTableFindAllTriggersConst( const UGCProjectData* ugcProj, UGCBacklinkTable* pTable, U32 triggeringID, int triggeringPromptID, const UGCComponent ***out_components )
{
	ugcBacklinkTableFindAllTriggers( (UGCProjectData*)ugcProj, pTable, triggeringID, triggeringPromptID, (UGCComponent***)out_components );
}

/// Fill OUT-COMPONENTS with a list of all components on the project
/// map specified by MAP-NAME.
void ugcBacklinkTableGetMapComponents( const UGCProjectData* ugcProj, UGCBacklinkTable* pTable, const char* mapName, const UGCComponent*** out_components )
{
	UGCBacklinkTableComponentEntry* entry = NULL;
	char mapNameSansNS[ RESOURCE_NAME_MAX_SIZE ];

	resExtractNameSpace_s( mapName, NULL, 0, SAFESTR( mapNameSansNS ));
	stashFindPointer( pTable->mapComponentsByMapName, mapNameSansNS, &entry );

	if( entry && eaiSize( &entry->eaComponentIDs )) {
		int it;
		for( it = 0; it != eaiSize( &entry->eaComponentIDs ); ++it ) {
			UGCComponent* component = ugcComponentFindByID( ugcProj->components, entry->eaComponentIDs[ it ]);
			if( component ) {
				eaPush( out_components, component );
			}
		}
	}
}

/// Fill OUT-COMPONENTS with a list of all dialog trees that list CONTACT-ID as the speaker.
void ugcBacklinkTableGetDialogTreesForComponent( const UGCProjectData* ugcProj, const UGCBacklinkTable* pTable, U32 contactID, const UGCComponent*** out_components )
{
	UGCBacklinkTableComponentEntry* entry = NULL;
	stashIntFindPointer( pTable->dialogsByContactID, contactID, &entry );

	if( entry && eaiSize( &entry->eaComponentIDs )) {
		int it;
		for( it = 0; it != eaiSize( &entry->eaComponentIDs ); ++it ) {
			UGCComponent* component = ugcComponentFindByID( ugcProj->components, entry->eaComponentIDs[ it ]);
			if( component ) {
				eaPush( out_components, component );
			}
		}
	}
}

const char *ugcComponentGetLogicalNameTemp(const UGCComponent *component)
{
	static char name_buf[4096];

	if(component)
		sprintf_s(SAFESTR(name_buf), "%s_%d", ugcComponentTypeGetName(component->eType), component->uID);
	else
		strcpy_s(SAFESTR(name_buf), "<MISSING>");

	return name_buf;
}

U32 ugcComponentNameGetID(const char* name)
{
	char* beforeId = strrchr(name, '_');
	if( beforeId ) {
		return atoi( beforeId + 1 );
	} else {
		return 0;
	}
}

U32 ugcPromptNameGetID(const char* name)
{
	char* beforeId = strrchr(name, '_');
	if( beforeId ) {
		return atoi( beforeId + 1 );
	} else {
		return 0;
	}
}

bool ugcComponentIsUsedInObjectives( UGCComponentList *list, UGCComponent* component, UGCMissionObjective** objectives )
{
	int it;
	for( it = 0; it != eaSize( &objectives ); ++it ) {
		UGCMissionObjective* objective = objectives[ it ];
		bool objectiveHasComponent = (objective->type == UGCOBJ_COMPLETE_COMPONENT || objective->type == UGCOBJ_UNLOCK_DOOR);

		if( objectiveHasComponent && (objective->componentID == component->uID || ea32Find( &objective->extraComponentIDs, component->uID ) >= 0) ) {
			return true;
		}

		if( ugcComponentIsUsedInObjectives( list, component, objective->eaChildren )) {
			return true;
		}
	}

	if (component->uParentID)
	{
		UGCComponent *parent_component = ugcComponentFindByID(list, component->uParentID);
		if (parent_component)
			return ugcComponentIsUsedInObjectives( list, parent_component, objectives );
	}

	return false;
}

bool ugcComponentIsUsedInLinks( UGCComponent* component, UGCMission* mission )
{
	FOR_EACH_IN_EARRAY(mission->map_links, UGCMissionMapLink, link)
	{
		if (link->uDoorComponentID == component->uID ||
			link->uSpawnComponentID == component->uID)
			return true;
	}
	FOR_EACH_END;
	return false;
}

UGCWhenType ugcComponentStartWhenType( const UGCComponent *component )
{
	if (!component->pStartWhen)
		return UGCWHEN_OBJECTIVE_IN_PROGRESS; // For backward compatibility with dialog tree PromptWhen

	return component->pStartWhen->eType;
}

UGCDialogTreePrompt* ugcDialogTreeGetPrompt( UGCDialogTreeBlock* block, int promptID )
{
	if( promptID < 0 ) {
		return &block->initialPrompt;
	} else {
		int it;
		for( it = 0; it != eaSize( &block->prompts ); ++it ) {
			if( block->prompts[ it ]->uid == (U32)promptID ) {
				return block->prompts[ it ];
			}
		}
	}

	return NULL;
}

/// Return if COMPONENT should have a patrol route.
bool ugcComponentHasPatrol( const UGCComponent* component, WorldPatrolRouteType* out_routeType )
{
	const WorldUGCProperties* ugcProps = ugcResourceGetUGCProperties( "FSM", component->fsmProperties.pcFSMNameRef );
	if( SAFE_MEMBER( ugcProps, fsmProps.bHasPatrol )) {
		if( out_routeType ) {
			*out_routeType = ugcProps->fsmProps.ePatrolType;
		}
		return true;
	} else {
		if( out_routeType ) {
			*out_routeType = 0;
		}
		return false;
	}
}

/// Return if COMPONENT is placed on MAP-NAME.
///
/// Note: Dialog Tree components will never return true, regardless of
/// the value in MAP-NAME.
bool ugcComponentIsOnMap( const UGCComponent* component, const char* mapName, bool includeUnplaced )
{
	if( !includeUnplaced && component->sPlacement.uRoomID == GENESIS_UNPLACED_ID ) {
		return false;
	} else if( !mapName ) {
		return component->sPlacement.bIsExternalPlacement;
	} else {
		return !component->sPlacement.bIsExternalPlacement && resNamespaceBaseNameEq( mapName, component->sPlacement.pcMapName );
	}
}

UGCMapType ugcComponentMapType( const UGCProjectData* ugcProj, const UGCComponent* component )
{
	if( component->sPlacement.uRoomID == GENESIS_UNPLACED_ID ) {
		return UGC_MAP_TYPE_ANY;
	} else if( component->sPlacement.bIsExternalPlacement ) {
		ZoneMapInfo* zoneInfo = worldGetZoneMapByPublicName( component->sPlacement.pcExternalMapName );
		ZoneMapEncounterObjectInfo* zeniInfo = zeniObjectFind( component->sPlacement.pcExternalMapName,
															   component->sPlacement.pcExternalObjectName );
		if( zoneInfo && zeniInfo ) {
			WorldRegion** regions = zmapInfoGetWorldRegions( zoneInfo );
			FOR_EACH_IN_EARRAY(regions, WorldRegion, region) {
				if( worldRegionGetRegionName( region ) == zeniInfo->regionName ) {
					return ugcWorldRegionGetPrefabType( zoneInfo, region, false );
				}
			} FOR_EACH_END;
		}

		return UGC_MAP_TYPE_ANY;
	} else {
		UGCMap* map = ugcMapFindByName( (UGCProjectData*)ugcProj, component->sPlacement.pcMapName );
		return ugcMapGetType( map );
	}
}

UGCComponent *ugcComponentFindMapSpawn( UGCComponentList *list, const char *pcMapName )
{
	UGC_FOR_EACH_COMPONENT_OF_TYPE(list, UGC_COMPONENT_TYPE_SPAWN, component)
	{
		if (component->uParentID == 0 &&
			resNamespaceBaseNameEq(component->sPlacement.pcMapName, pcMapName))
		{
			return component;
		}
	}
	UGC_FOR_EACH_COMPONENT_END;

	return NULL;
}

const char* ugcCostumeHandleString( UGCProjectData* ugcProj, const char* name )
{
	char nameSansNS[ RESOURCE_NAME_MAX_SIZE ];
	if( resExtractNameSpace_s( name, NULL, 0, SAFESTR( nameSansNS ))) {
		char newName[ RESOURCE_NAME_MAX_SIZE ];
		sprintf( newName, "%s:%s", ugcProj->ns_name, nameSansNS );

		return allocAddString( newName );
	} else {
		return allocAddString( name );
	}
}

// Returns the last reward box component in the list. There is assumed to be only one. This is ensured in fixup.
UGCComponent *ugcComponentFindFinalRewardBox( UGCComponentList *list )
{
	UGCComponent *reward_box_component = NULL;
	UGC_FOR_EACH_COMPONENT_OF_TYPE(list, UGC_COMPONENT_TYPE_REWARD_BOX, component)
	{
		assertmsg(!reward_box_component, "There exists more than 1 project reward chest, when there should only ever be 1!");
		reward_box_component = component;
	}
	UGC_FOR_EACH_COMPONENT_END;

	return reward_box_component;
}

void ugcComponentOpReset(UGCProjectData *data, UGCComponent *component, UGCMapType map_type, bool keep_object)
{
	int i;
	UGCComponentList *list = data->components;
	UGCPerProjectDefaults *defaults = ugcGetDefaults();

	// Deinit fields that may no longer apply
	if (!keep_object)
	{
		component->iObjectLibraryId = 0;
		component->iPlanetRingId = 0;
		StructFreeStringSafe(&component->pcCostumeName);
		StructFreeStringSafe(&component->pcPromptCostumeName);
		
		ugcComponentOpDeleteChildren(data, component, true);
	}

	zeroVec3( component->sPlacement.vRotPYR );

	// Initialize fields to defaults that aren't set
	switch (component->eType)
	{
	case UGC_COMPONENT_TYPE_KILL:
		if(!component->fsmProperties.pcFSMNameRef)
			component->fsmProperties.pcFSMNameRef = StructAllocString(defaults->pcBehavior);

		if (!keep_object)
		{
			if (ugcMapTypeIsGround(map_type))
				component->iObjectLibraryId = objectLibraryUIDFromObjName(defaults->pcInteriorKillObject);
			else
				component->iObjectLibraryId = objectLibraryUIDFromObjName(defaults->pcSpaceKillObject);
		}
		break;
	case UGC_COMPONENT_TYPE_CONTACT:
		if(!component->fsmProperties.pcFSMNameRef)
			component->fsmProperties.pcFSMNameRef = StructAllocString(defaults->pcNoCombatBehavior);
		break;
	case UGC_COMPONENT_TYPE_ACTOR:
		break;
	case UGC_COMPONENT_TYPE_DIALOG_TREE:
		break;
	case UGC_COMPONENT_TYPE_WHOLE_MAP:
		break;
	case UGC_COMPONENT_TYPE_SPAWN:
		break;
	case UGC_COMPONENT_TYPE_COMBAT_JOB:
		break;
	case UGC_COMPONENT_TYPE_RESPAWN:
		break;
	case UGC_COMPONENT_TYPE_OBJECT:
		if (!keep_object)
		{
			if (ugcMapTypeIsGround(map_type))
				component->iObjectLibraryId = objectLibraryUIDFromObjName(defaults->pcInteriorDetailObject);
			else
				component->iObjectLibraryId = objectLibraryUIDFromObjName(defaults->pcSpaceDetailObject);
		}
		break;
	case UGC_COMPONENT_TYPE_SOUND:
		if (!keep_object)
			component->strSoundEvent = NULL;
		break;
	case UGC_COMPONENT_TYPE_DESTRUCTIBLE:
		if (!keep_object)
		{
			if (ugcMapTypeIsGround(map_type))
				component->iObjectLibraryId = objectLibraryUIDFromObjName(defaults->pcInteriorDetailObject);
			else
				component->iObjectLibraryId = objectLibraryUIDFromObjName(defaults->pcSpaceDetailObject);
		}
		break;
	case UGC_COMPONENT_TYPE_ROOM:
		break;
	case UGC_COMPONENT_TYPE_ROOM_DOOR:
	case UGC_COMPONENT_TYPE_FAKE_DOOR:
		break;
	case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
		if (!keep_object)
			component->iObjectLibraryId = objectLibraryUIDFromObjName(defaults->pcGroundBuildingObject);
		break;
	case UGC_COMPONENT_TYPE_PLANET:
		if (!keep_object)
		{
			component->iObjectLibraryId = objectLibraryUIDFromObjName(defaults->pcSpacePlanetObject);
			component->iPlanetRingId = objectLibraryUIDFromObjName(defaults->pcSpaceRingObject);
		}
		break;
	case UGC_COMPONENT_TYPE_ROOM_MARKER:
		component->fVolumeRadius = 50.f;
		break;
	case UGC_COMPONENT_TYPE_EXTERNAL_DOOR:
		break;
	case UGC_COMPONENT_TYPE_PATROL_POINT:
		break;
	case UGC_COMPONENT_TYPE_TRAP:
		if (!keep_object)
		{
			component->iObjectLibraryId = objectLibraryUIDFromObjName(defaults->pcTrapObject);
		}
		break;
	case UGC_COMPONENT_TYPE_TRAP_TARGET:
		break;
	case UGC_COMPONENT_TYPE_TRAP_TRIGGER:
		break;
	case UGC_COMPONENT_TYPE_TRAP_EMITTER:
		break;
	case UGC_COMPONENT_TYPE_TELEPORTER: case UGC_COMPONENT_TYPE_TELEPORTER_PART:
		break;
	case UGC_COMPONENT_TYPE_CLUSTER: case UGC_COMPONENT_TYPE_CLUSTER_PART:
		break;
		
	default:
		assert( false );
	}

	for(i=ea32Size(&component->uChildIDs)-1; i>=0; i--)
	{
		ugcComponentOpReset(data, ugcComponentFindByID(data->components, component->uChildIDs[i]), map_type, keep_object);
	}
}

static void ugcComponentOpFixSnapOnPlacement(UGCComponent *component, UGCMap *map)
{
	// Given that we are 'moving' the component to the given map. Fix up its snap type.
	//  We rely on this being set whenever a component is placed on a map.
	// Note: We should only see DEFAULT on legacy components.


	// Dialog trees are considered unspecified since they don't actually get placed.
	if (component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE)
	{
		component->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_UNSPECIFIED;
		return;
	}
	
	if (map)
	{
		UGCMapType mapType = ugcMapGetType(map);
		
		if (mapType==UGC_MAP_TYPE_ANY || mapType==UGC_MAP_TYPE_GROUND)
		{
			// Untyped map, or 'unused' ground map. Leave things alone.
		}
		else if (mapType==UGC_MAP_TYPE_PREFAB_INTERIOR || mapType==UGC_MAP_TYPE_PREFAB_GROUND)
		{
			// Only allow DEFAULT, TERRAIN, WORLDGEO, ABSOLUTE. Default to Terrain
			if (component->sPlacement.eSnap!=COMPONENT_HEIGHT_SNAP_LEGACY &&
				component->sPlacement.eSnap!=COMPONENT_HEIGHT_SNAP_TERRAIN && 
				component->sPlacement.eSnap!=COMPONENT_HEIGHT_SNAP_WORLDGEO && 
				component->sPlacement.eSnap!=COMPONENT_HEIGHT_SNAP_ABSOLUTE)
			{
				component->sPlacement.eSnap=COMPONENT_HEIGHT_SNAP_TERRAIN;
				if(ugcComponentSupportsNormalSnapping(component) && ugcComponentPlacementNormalSnappingActive(&component->sPlacement))
				{
					const WorldUGCProperties *ugc_properties = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", component->iObjectLibraryId );
					if(ugc_properties)
						component->sPlacement.bSnapNormal = ugc_properties->groupDefProps.bDefaultSnapNormal;
				}
			}
		}
		else if (mapType==UGC_MAP_TYPE_INTERIOR)
		{
			// Only allow ROOM_ABSOLUTE, ROOM_PARENTED. Default to ROOM_ABSOLUTE
			if (component->sPlacement.eSnap!=COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE &&
				component->sPlacement.eSnap!=COMPONENT_HEIGHT_SNAP_ROOM_PARENTED)
			{
				component->sPlacement.eSnap=COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE;
			}
		}
		else if (mapType==UGC_MAP_TYPE_PREFAB_SPACE || mapType==UGC_MAP_TYPE_SPACE)
		{
			// Only allow DEFAULT, ABSOLUTE. Default to Absolute
			if (component->sPlacement.eSnap!=COMPONENT_HEIGHT_SNAP_LEGACY &&
				component->sPlacement.eSnap!=COMPONENT_HEIGHT_SNAP_ABSOLUTE)
			{
				component->sPlacement.eSnap=COMPONENT_HEIGHT_SNAP_ABSOLUTE;
			}
		}
	}
	else
	{
		component->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_UNSPECIFIED;
	}
}

void ugcComponentOpSetPlacement(UGCProjectData *data, UGCComponent *component, UGCMap *map, U32 room_id)
{
	int i;

	StructFreeStringSafe(&component->sPlacement.pcMapName);
	if (map)
	{
		char ns[RESOURCE_NAME_MAX_SIZE], base[RESOURCE_NAME_MAX_SIZE];
		if (resExtractNameSpace(map->pcName, ns, base))
		{
			StructCopyString(&component->sPlacement.pcMapName, base);
		}
		component->eMapType = ugcMapGetType(map);
	}
	else
	{
		component->eMapType=UGC_MAP_TYPE_ANY;
	}
	
	component->sPlacement.uRoomID = room_id;

	ugcComponentOpFixSnapOnPlacement(component, map);

	// Move all the children as well
	for (i = 0; i < eaiSize(&component->uChildIDs); i++)
	{
		UGCComponent *child_component = ugcComponentFindByID(data->components, component->uChildIDs[i]);
		ugcComponentOpSetPlacement(data, child_component, map, room_id);
	}
}

U32 ugcComponentMakeUniqueID(UGCComponentList *list)
{
	bool found;
	U32 gen_id = 0;

	do
	{
		found = false;
		gen_id++;
		FOR_EACH_IN_EARRAY(list->eaComponents, UGCComponent, com)
		{
			if (com->uID == gen_id)
			{
				found = true;
				break;
			}
		}
		FOR_EACH_END;
	} while (found);
	return gen_id;
}

bool ugcComponentCreateClusterChildren( UGCProjectData* ugcProj, UGCComponent* component )
{
	UGCGroupDefMetadata* pDefData = ugcResourceGetGroupDefMetadataInt( component->iObjectLibraryId );
	UGCMap *map = ugcMapFindByName( ugcProj, component->sPlacement.pcMapName );
	bool createdOrDeletedComponents = false;

	assert( component->eType == UGC_COMPONENT_TYPE_TELEPORTER || component->eType == UGC_COMPONENT_TYPE_CLUSTER );
	
	if( pDefData ) {
		Mat3 componentMat;
		// Make sure we have the right number of children
		int it;

		createMat3DegYPR( componentMat, component->sPlacement.vRotPYR );
		for( it = eaiSize( &component->uChildIDs ); it < eaSize( &pDefData->eaClusterChildren ); ++it ) {
			UGCDefChildMetadata* pChildData = pDefData->eaClusterChildren[ it ];
			UGCComponentType newChildType;
			UGCComponent* newChild;

			if( component->eType == UGC_COMPONENT_TYPE_TELEPORTER ) {
				newChildType = UGC_COMPONENT_TYPE_TELEPORTER_PART;
			} else if( component->eType == UGC_COMPONENT_TYPE_CLUSTER ) {
				newChildType = UGC_COMPONENT_TYPE_CLUSTER_PART;
			} else {
				assert( 0 );
			}
					
			newChild = ugcComponentOpCreate( ugcProj, newChildType, component->uID );
			ugcComponentOpSetPlacement( ugcProj, newChild, map, UGC_TOPLEVEL_ROOM_ID );

			newChild->sPlacement.eSnap = component->sPlacement.eSnap;
			if(ugcComponentSupportsNormalSnapping(newChild) && ugcComponentPlacementNormalSnappingActive(&component->sPlacement))
			{
				const WorldUGCProperties* ugcProps = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", component->iObjectLibraryId );
				if(ugcProps)
					newChild->sPlacement.bSnapNormal = ugcProps->groupDefProps.bDefaultSnapNormal;
			}
			newChild->iObjectLibraryId = pChildData->defUID;

			if( component->eType == UGC_COMPONENT_TYPE_TELEPORTER ) {
				Vec3 childOffset = { 0, 0, 5 * it };
				mulVecMat3( childOffset, componentMat, newChild->sPlacement.vPos );
				addVec3( newChild->sPlacement.vPos, component->sPlacement.vPos, newChild->sPlacement.vPos );
				newChild->sPlacement.vPos[ 2 ] += 5 * it;
				copyVec3( component->sPlacement.vRotPYR, newChild->sPlacement.vRotPYR );
			} else if( component->eType == UGC_COMPONENT_TYPE_CLUSTER ) {
				mulVecMat3( pChildData->pos, componentMat, newChild->sPlacement.vPos );
				addVec3( newChild->sPlacement.vPos, component->sPlacement.vPos, newChild->sPlacement.vPos );
				copyVec3( component->sPlacement.vRotPYR, newChild->sPlacement.vRotPYR );
				newChild->sPlacement.vRotPYR[ 1 ] += DEG( pChildData->rot );
			} else {
				assert( 0 );
			} 
			createdOrDeletedComponents = true;
		}

		for( it = eaiSize( &component->uChildIDs ); it > eaSize( &pDefData->eaClusterChildren ); -- it ) {
			int childIndex = eaSize( &pDefData->eaClusterChildren );
			UGCComponent* child = ugcComponentFindByID( ugcProj->components, component->uChildIDs[ childIndex ]);
			if( ugcComponentOpDelete( ugcProj, child, false )) {
				createdOrDeletedComponents = true;
			}
		}

		// Fill in all the child IDs
		for( it = 0; it != eaiSize( &component->uChildIDs ); ++it ) {
			UGCComponent* child = ugcComponentFindByID( ugcProj->components, component->uChildIDs[ it ]);
			int childObjectLibraryID = pDefData->eaClusterChildren[ it ]->defUID;
			const WorldUGCProperties* childUGCProps = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", childObjectLibraryID );
				
			child->iObjectLibraryId = childObjectLibraryID;
			if(   childUGCProps && child->sPlacement.eSnap != COMPONENT_HEIGHT_SNAP_ABSOLUTE
				  && child->sPlacement.eSnap != COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE ) {
				child->sPlacement.vPos[ 1 ] = childUGCProps->fMapDefaultHeight;
			}
		}
	}

	return createdOrDeletedComponents;
}


bool ugcComponentCreateTrapChildren( UGCProjectData* data, UGCComponent* component )
{
	GroupDef *def = objectLibraryGetGroupDef(component->iObjectLibraryId, false);
	UGCTrapProperties *properties = def ? ugcTrapGetProperties(def) : NULL;
	UGCMap *map = ugcMapFindByName(data, component->sPlacement.pcMapName);
	bool createdOrDeletedComponents = false;

	assert( component->eType == UGC_COMPONENT_TYPE_TRAP );

	if( map && properties ) {
		int emitter_idx, child_idx;
		UGCComponent *found_trigger = NULL;
		UGCComponent *found_emitter = NULL;

		// Clear target indices
		for (child_idx = 0; child_idx < eaiSize(&component->uChildIDs); child_idx++)
		{
			UGCComponent *child = ugcComponentFindByID(data->components, component->uChildIDs[child_idx]);
			if (child && child->eType == UGC_COMPONENT_TYPE_TRAP_TARGET)
			{
				child->iTrapEmitterIndex = -1;
			}
			else if (child && child->eType == UGC_COMPONENT_TYPE_TRAP_TRIGGER)
			{
				found_trigger = child;
			}
			else if (child && child->eType == UGC_COMPONENT_TYPE_TRAP_EMITTER)
			{
				found_emitter = child;
			}
		}

		for (emitter_idx = 0; emitter_idx < eaSize(&properties->eaEmitters); emitter_idx++)
		{
			if (properties->eaEmitters[emitter_idx])
			{
				// Assign or create a child
				UGCComponent *target_component = NULL;
				for (child_idx = 0; child_idx < eaiSize(&component->uChildIDs); child_idx++)
				{
					UGCComponent *child = ugcComponentFindByID(data->components, component->uChildIDs[child_idx]);
					if (child && child->eType == UGC_COMPONENT_TYPE_TRAP_TARGET && child->iTrapEmitterIndex == -1)
					{
						target_component = child;
						break;
					}
				}
				if (!target_component)
				{
					target_component = ugcComponentOpCreate(data, UGC_COMPONENT_TYPE_TRAP_TARGET, component->uID);
					assert(target_component);
					StructCopy( parse_UGCComponentPlacement, &component->sPlacement, &target_component->sPlacement, 0, 0, 0 );
					target_component->sPlacement.vPos[0] += properties->eaEmitters[emitter_idx]->pos[0];
					target_component->sPlacement.vPos[2] += properties->eaEmitters[emitter_idx]->pos[2];
					zeroVec3( target_component->sPlacement.vRotPYR );

					// Offset the target by a little more than the trigger
					target_component->sPlacement.vPos[2] += 7;
					createdOrDeletedComponents = true;
				}
				target_component->iTrapEmitterIndex = emitter_idx;
			}
		}

		// Clear unnecessary children
		for (child_idx = 0; child_idx < eaiSize(&component->uChildIDs); child_idx++)
		{
			UGCComponent *child = ugcComponentFindByID(data->components, component->uChildIDs[child_idx]);
			if (child && child->eType == UGC_COMPONENT_TYPE_TRAP_TARGET && child->iTrapEmitterIndex == -1)
			{
				if( ugcComponentOpDelete(data, child, false)) {
					createdOrDeletedComponents = true;
				}
			}
		}

		// Create a trigger if necessary
		if (!found_trigger && !properties->pSelfContained)
		{
			UGCComponent *new_trigger = ugcComponentOpCreate(data, UGC_COMPONENT_TYPE_TRAP_TRIGGER, component->uID);
			assert(new_trigger);
			StructCopy( parse_UGCComponentPlacement, &component->sPlacement, &new_trigger->sPlacement, 0, 0, 0 );
			new_trigger->sPlacement.vPos[2] += 5;
			zeroVec3( new_trigger->sPlacement.vRotPYR );
			createdOrDeletedComponents = true;
		}
		else if (found_trigger && properties->pSelfContained)
		{
			if( ugcComponentOpDelete(data, found_trigger, false)) {
				createdOrDeletedComponents = true;
			}
		}

				
		if (!found_emitter && !properties->pSelfContained)
		{
			UGCComponent *new_emitter = ugcComponentOpCreate(data, UGC_COMPONENT_TYPE_TRAP_EMITTER, component->uID);
			assert(new_emitter);
			StructCopy( parse_UGCComponentPlacement, &component->sPlacement, &new_emitter->sPlacement, 0, 0, 0 );
			new_emitter->sPlacement.vPos[2] -= 5;
			zeroVec3( new_emitter->sPlacement.vRotPYR );
			createdOrDeletedComponents = true;
		}
		else if (found_emitter && properties->pSelfContained)
		{
			if( ugcComponentOpDelete(data, found_emitter, false)) {
				createdOrDeletedComponents = true;
			}
		}
	}
			
	StructDestroy(parse_UGCTrapProperties, properties);
	return createdOrDeletedComponents;
}

bool ugcComponentCreateActorChildren( UGCProjectData* data, UGCComponent* component )
{
	GroupDef* def = objectLibraryGetGroupDef(component->iObjectLibraryId, false);
	WorldEncounterProperties* props = SAFE_MEMBER( def, property_structs.encounter_properties );
	bool createdOrDeletedComponents = false;
	bool initiallyHadNoChildren = (ea32Size( &component->uChildIDs ) == 0);

	assert( component->eType == UGC_COMPONENT_TYPE_KILL );
	
	if(!props) {
		if( ea32Size( &component->uChildIDs )) {
			createdOrDeletedComponents = true;
		}
		ugcComponentOpDeleteChildren(data, component, true);
		return createdOrDeletedComponents;
	}

	if(ea32Size(&component->uChildIDs)>eaSize(&props->eaActors)) {
		// We have more actors than we need, delete them
		int i;
		for(i=ea32Size(&component->uChildIDs)-1; i>=eaSize(&props->eaActors); i--) {
			ugcComponentOpDelete(data, ugcComponentFindByID(data->components, component->uChildIDs[i]), true);
			createdOrDeletedComponents = true;
		}
	} else if(ea32Size(&component->uChildIDs)<eaSize(&props->eaActors)) {
		// We have fewer actors than we need, create them
		int i;
		for(i=ea32Size(&component->uChildIDs); i<eaSize(&props->eaActors); i++) {
			ugcComponentOpCreate(data, UGC_COMPONENT_TYPE_ACTOR, component->uID);
			createdOrDeletedComponents = true;
		}
	}

	// If any change was made, we need to fixup all the existing
	// children back to their default state.
	if( initiallyHadNoChildren ) {
		int i;
		UGCMap *parent_map = ugcMapFindByName(data, component->sPlacement.pcMapName);
		for( i = 0; i != ea32Size( &component->uChildIDs ); ++i ) {
			UGCComponent *child_component = ugcComponentFindByID( data->components, component->uChildIDs[ i ]);
			const WorldUGCProperties* ugcProps = ugcResourceGetUGCPropertiesInt("ObjectLibrary", component->iObjectLibraryId);
			WorldActorProperties *actorProps = props->eaActors[i];

			ugcComponentOpReset(data, child_component, ugcMapGetType(parent_map), false);
			StructCopy(parse_UGCComponentPlacement, &component->sPlacement, &child_component->sPlacement, 0, 0, 0);
			child_component->sPlacement.vPos[0] += actorProps->vPos[0];
			child_component->sPlacement.vPos[2] -= actorProps->vPos[2];

			// Select a random costume to wear.
			if(ugcProps)
			{
				int c, count = 0;
				for(c = 0; c < eaSize(&ugcProps->groupDefProps.eaEncounterActors[i]->eaCostumes); c++)
					if(ugcResourceGetUGCProperties("PlayerCostume", ugcProps->groupDefProps.eaEncounterActors[i]->eaCostumes[c]->pcCostumeName))
						count++;
				if(count > 0)
				{
					WorldUGCActorCostumeProperties *pCostumeProps = NULL;
					int costume = randomIntRange(0, count - 1);
					count = 0;
					for(c = 0; c < eaSize(&ugcProps->groupDefProps.eaEncounterActors[i]->eaCostumes); c++)
					{
						if(ugcResourceGetUGCProperties("PlayerCostume", ugcProps->groupDefProps.eaEncounterActors[i]->eaCostumes[c]->pcCostumeName))
						{
							if(count == costume)
							{
								pCostumeProps = ugcProps->groupDefProps.eaEncounterActors[i]->eaCostumes[c];
								break;
							}
							count++;
						}
					}
					if(pCostumeProps)
						child_component->pcCostumeName = StructAllocString(pCostumeProps->pcCostumeName);
					devassert(child_component->pcCostumeName);
				}
			}
		}
	}

	return createdOrDeletedComponents;
}

UGCComponent **ugcComponentFindPlacements(UGCComponentList *list, const char *map_name, U32 room_id)
{
	UGCComponent **ret_array = NULL;
	char ns[RESOURCE_NAME_MAX_SIZE], base[RESOURCE_NAME_MAX_SIZE];
	int idx = 0;
	bool searching_for_unplaced = (room_id == GENESIS_UNPLACED_ID);

	resExtractNameSpace(map_name, ns, base);
	assert( base[0] );

	FOR_EACH_IN_EARRAY(list->eaComponents, UGCComponent, component)
	{
		if (ugcComponentIsOnMap(component, NULL, false))
		{
			continue;
		}
		if (searching_for_unplaced && component->uParentID != 0)
		{
			continue;
		}
		if (searching_for_unplaced && component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE)
		{
			continue;
		}

		if (ugcComponentLayoutCompatible(component->eType, UGC_MAP_TYPE_ANY))
		{
			if (stricmp(component->sPlacement.pcMapName, base) == 0 &&
				(component->sPlacement.uRoomID == room_id || room_id == UGC_ANY_ROOM_ID))
			{
				eaPush(&ret_array, component);
			}
		}
	}
	FOR_EACH_END;
	return ret_array;
}

UGCComponent *ugcComponentOpCreate(UGCProjectData *data, UGCComponentType type, U32 parentID)
{
	UGCComponent *new_component;
	UGCComponentList *list = data->components;
	new_component = StructCreate(parse_UGCComponent);

	new_component->uID = ugcComponentMakeUniqueID(list);
	new_component->uParentID = parentID;
	new_component->eType = type;
	new_component->bDisplayNameWasFixed=true;	// We are of the new design, so the displayName has been deprecated and the valid data is in the VisibleName
	new_component->bPatrolPointsFixed_FromComponentPosition = true;

	if (new_component->uParentID)
	{
		UGCComponent *parentComponent = ugcComponentFindByID(list, new_component->uParentID);
		ea32Push(&parentComponent->uChildIDs, new_component->uID);
	}

	// DO NOT COPY / PASTE THIS CODE!!!!  THIS MEANS YOU!!!!!!11one
	//
	// See struct UGCComponentList for explanations.
	eaIndexedAdd( &(UGCComponent**)list->eaComponents, new_component );
	stashIntAddPointer( list->stComponentsById, new_component->uID, new_component, true );

	// Start with unspecified snap
	new_component->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_UNSPECIFIED;

	if (new_component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP)
	{
		ugcComponentOpSetPlacement(data, new_component, NULL, UGC_TOPLEVEL_ROOM_ID);
	}
	else
	{
		ugcComponentOpSetPlacement(data, new_component, NULL, GENESIS_UNPLACED_ID);
	}

	return new_component;
}

void ugcComponentOpSetParent(UGCProjectData *data, UGCComponent *component, U32 parentID)
{
	UGCComponentList *list = data->components;
	UGCComponent *parentComponent;

	assert(parentID != component->uID);

	if (component->uParentID &&
		(parentComponent = ugcComponentFindByID(list, component->uParentID)) != NULL)
	{
		ea32FindAndRemoveFast(&parentComponent->uChildIDs, component->uID);
	}

	component->uParentID = parentID;

	if (component->uParentID &&
		(parentComponent = ugcComponentFindByID(list, component->uParentID)) != NULL)
	{
		ea32Push(&parentComponent->uChildIDs, component->uID);
	}
}

UGCComponent *ugcComponentOpDuplicate(UGCProjectData *data, UGCComponent *src, U32 parentID)
{
	UGCComponent *new_component;
	UGCComponentList *list = data->components;

	new_component = StructClone(parse_UGCComponent, src);
	assert(new_component);

	new_component->uID = ugcComponentMakeUniqueID(list);
	new_component->uParentID = parentID;

	if (new_component->uParentID)
	{
		UGCComponent *parentComponent = ugcComponentFindByID(list, new_component->uParentID);
		ea32Push(&parentComponent->uChildIDs, new_component->uID);
	}

	// DO NOT COPY / PASTE THIS CODE!!!!  THIS MEANS YOU!!!!!!11one
	//
	// See struct UGCComponentList for explanations.
	eaIndexedAdd( &(UGCComponent**)list->eaComponents, new_component );
	stashIntAddPointer( list->stComponentsById, new_component->uID, new_component, true );

	eaiDestroy(&new_component->uChildIDs);

	StructCopyString(&new_component->pcVisibleName, src->pcVisibleName);

	return new_component;
}

UGCComponent *ugcComponentOpClone(UGCProjectData *data, UGCComponent* component)
{
	UGCComponent *new_component;
	UGCComponentList *list = data->components;
	new_component = StructClone(parse_UGCComponent, component);
	assert(new_component);

	new_component->uID = ugcComponentMakeUniqueID(list);

	if (new_component->uParentID)
	{
		UGCComponent *parentComponent = ugcComponentFindByID(list, new_component->uParentID);
		ea32Push(&parentComponent->uChildIDs, new_component->uID);
	}

	// DO NOT COPY / PASTE THIS CODE!!!!  THIS MEANS YOU!!!!!!11one
	//
	// See struct UGCComponentList for explanations.
	eaIndexedAdd( &(UGCComponent**)list->eaComponents, new_component );
	stashIntAddPointer( list->stComponentsById, new_component->uID, new_component, true );

	return new_component;
}

static UGCComponentType ugcComponentTypeForZeniObj( ZoneMapEncounterObjectInfo* object )
{
	if( object->interactType == WL_ENC_CONTACT ) {
		return UGC_COMPONENT_TYPE_CONTACT;
	}

	switch( object->type ) {
		case WL_ENC_INTERACTABLE:
			switch( object->interactType ) {
				case WL_ENC_DOOR: return UGC_COMPONENT_TYPE_EXTERNAL_DOOR;
				case WL_ENC_CLICKIE: return UGC_COMPONENT_TYPE_OBJECT;
				case WL_ENC_DESTRUCTIBLE: return UGC_COMPONENT_TYPE_DESTRUCTIBLE;
				case WL_ENC_REWARD_BOX: return UGC_COMPONENT_TYPE_REWARD_BOX;
			}

		xcase WL_ENC_ENCOUNTER:
			return UGC_COMPONENT_TYPE_KILL;

		xcase WL_ENC_NAMED_VOLUME: return UGC_COMPONENT_TYPE_ROOM_MARKER;
	}

	return -1;
}

UGCComponent* ugcComponentOpExternalObjectFind(UGCProjectData* data, const char* zmapName, const char* logicalName)
{
	int it;
	for( it = 0; it != eaSize( &data->components->eaComponents ); ++it ) {
		UGCComponent* component = data->components->eaComponents[ it ];
		UGCComponentPlacement* placement = &component->sPlacement;

		if(   placement->bIsExternalPlacement && stricmp( placement->pcExternalMapName, zmapName ) == 0
			  && stricmp( placement->pcExternalObjectName, logicalName ) == 0 ) {
			return component;
		}
	}

	return NULL;
}

UGCComponent* ugcComponentOpExternalObjectFindOrCreate(UGCProjectData* data, const char* zmapName, const char* logicalName)
{
	ZoneMapEncounterObjectInfo* zeniObj;
	UGCComponent* component = ugcComponentOpExternalObjectFind( data, zmapName, logicalName );

	if( component ) {
		return component;
	}

	if (stricmp(logicalName, "WHOLE_MAP") == 0)
	{
		component = ugcComponentOpCreate( data, UGC_COMPONENT_TYPE_WHOLE_MAP, 0);
		StructReset( parse_UGCComponentPlacement, &component->sPlacement);
		component->sPlacement.bIsExternalPlacement = true;
		component->sPlacement.pcExternalMapName = StructAllocString( zmapName );
		component->sPlacement.pcExternalObjectName = StructAllocString( "WHOLE_MAP" );

		return component;
	}
	else if (stricmp(logicalName, "MISSION_RETURN") == 0)
	{
		component = ugcComponentOpCreate( data, UGC_COMPONENT_TYPE_SPAWN, 0);
		StructReset( parse_UGCComponentPlacement, &component->sPlacement);
		component->sPlacement.bIsExternalPlacement = true;
		component->sPlacement.pcExternalMapName = StructAllocString( zmapName );
		component->sPlacement.pcExternalObjectName = StructAllocString( "MISSION_RETURN" );

		return component;
	}

	zeniObj = zeniObjectFind( zmapName, logicalName );
	if( zeniObj ) {
		component = ugcComponentOpCreate( data, ugcComponentTypeForZeniObj( zeniObj ), 0);
		StructReset( parse_UGCComponentPlacement, &component->sPlacement);
		component->iObjectLibraryId = 0;
		component->sPlacement.bIsExternalPlacement = true;
		component->sPlacement.pcExternalMapName = StructAllocString( zmapName );
		component->sPlacement.pcExternalObjectName = StructAllocString( logicalName );

		return component;
	}

	return NULL;
}


bool ugcComponentOpDelete(UGCProjectData *data, UGCComponent *component, bool force)
{
	UGCMission *mission = data->mission;
	UGCComponentList *list = data->components;
	U32 id = component->uID;
	UGCMissionObjective *objective;

	if (objective = ugcObjectiveFindComponentRelated(data->mission->objectives, data->components, id))
	{
		if (!force)
		{
			if (component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP)
			{
				// WHOLE MAP components should never be set to unplaced. Leave them alone
			}
			else
			{
				// Move to "unplaced"
				UGCMap* map = ugcMapFindByName( data, component->sPlacement.pcMapName );
				ugcComponentOpSetPlacement(data, component, map, GENESIS_UNPLACED_ID);
				if( component->uParentID ) {
					UGCComponent* parentComponent = ugcComponentFindByID( data->components, component->uParentID );
					if( parentComponent ) {
						eaiFindAndRemove( &parentComponent->uChildIDs, component->uID );
					}
					component->uParentID = 0;
				}
			}
			return false;
		}

		// Remove the objective's reference
		objective->componentID = 0;
		assert( false );
	}

	// DO NOT COPY / PASTE THIS CODE!!!!  THIS MEANS YOU!!!!!!11one
	//
	// See struct UGCComponentList for explanations.
	eaRemove( &(UGCComponent**)list->eaComponents, eaIndexedFindUsingInt( &list->eaComponents, id ));
	stashIntRemovePointer( list->stComponentsById, id, NULL );
	if(component->uParentID)
	{
		UGCComponent* parentComponent = ugcComponentFindByID(list, component->uParentID);
		if(parentComponent)
			ea32FindAndRemoveFast(&parentComponent->uChildIDs, id);
	}

	FOR_EACH_IN_EARRAY(list->eaComponents, UGCComponent, other_component)
	{
		if (other_component->uActorID == id)
		{
			other_component->uActorID = 0;
		}
	}
	FOR_EACH_END;

	if(ea32Size(&component->uChildIDs))
		ugcComponentOpDeleteChildren(data, component, force);

	StructDestroy(parse_UGCComponent, component);
	return true;
}

void ugcComponentOpDeleteChildren(UGCProjectData *data, UGCComponent *component, bool force)
{
	int i;

	for(i=ea32Size(&component->uChildIDs)-1; i>=0; i--)
	{
		ugcComponentOpDelete(data, ugcComponentFindByID(data->components, component->uChildIDs[i]), force);
	}
}

const char* ugcComponentGroundCostumeName( UGCProjectData* ugcProj, UGCComponent* component )
{
	if( component->sPlacement.bIsExternalPlacement ) {
		ZoneMapEncounterObjectInfo* zeniInfo = zeniObjectFind( component->sPlacement.pcExternalMapName, component->sPlacement.pcExternalObjectName );
		if( zeniInfo && IS_HANDLE_ACTIVE( zeniInfo->ugcContactCostume )) {
			return REF_STRING_FROM_HANDLE( zeniInfo->ugcContactCostume );
		}
	} else {
		UGCMapType mapType = ugcComponentMapType( ugcProj, component );
		if( mapType == UGC_MAP_TYPE_INTERIOR || mapType == UGC_MAP_TYPE_PREFAB_INTERIOR || mapType == UGC_MAP_TYPE_PREFAB_GROUND ) {
			return component->pcCostumeName;
		}
	}

	return NULL;
}

// Does allegiance filtering

bool ugcProjectFilterAllegiance(const UGCProjectData *data, const char *value, int object_id)
{
	GroupDef *def = object_id ? objectLibraryGetGroupDef(object_id, false) : NULL;
	ResourceInfo* defInfo = ugcResourceGetInfoInt( "ObjectLibrary", object_id );
	const char *found_faction = NULL;
	char **tag_list = NULL;
	WorldUGCRestrictionProperties* projRestrictions = data ? data->project->pRestrictionProperties : NULL;
	UGCPerProjectDefaults *defaults = ugcGetDefaults();

	if (!def || (defInfo==NULL) || !projRestrictions || eaSize(&projRestrictions->eaFactions) != 1)
		return true;
	
	if (defInfo->resourceTags)
	{
		DivideString(defInfo->resourceTags, ",", &tag_list, DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS);
		FOR_EACH_IN_EARRAY(tag_list, char, tag)
		{
			if (strStartsWith(tag, "Faction_"))
			{
				found_faction = &tag[8];
				break;
			}
		}
		FOR_EACH_END;
	}

	FOR_EACH_IN_EARRAY(defaults->allegiance, UGCPerAllegianceDefaults, allegiance)
	{
		if (stricmp(allegiance->allegianceName, projRestrictions->eaFactions[0]->pcFaction) == 0)
		{
			FOR_EACH_IN_EARRAY(allegiance->relations, UGCAllegianceRelation, relation)
			{
				if (stricmp(relation->pcFilterValue, value) == 0 &&
					stricmp_safe(relation->pcTagName, found_faction) == 0)
				{
					eaDestroyEx(&tag_list, NULL); // found_faction points into this array
					return true;
				}
			}
			FOR_EACH_END;
			eaDestroyEx(&tag_list, NULL); // found_faction points into this array
			return false;
		}
	}
	FOR_EACH_END;

	eaDestroyEx(&tag_list, NULL); // found_faction points into this array
	return true;
}

/// Default door heuristics

// Returns a sensible default start spawn name for a given map, using links,
// doors, and component information.

const char *ugcProjectGetMapStartSpawnTemp(UGCProjectData *data, UGCMap *map)
{
	static char ret_buf[RESOURCE_NAME_MAX_SIZE];
	const char *map_name = map->pcName;

	UGC_FOR_EACH_COMPONENT_OF_TYPE(data->components, UGC_COMPONENT_TYPE_SPAWN, component)
	{
		if (component->sPlacement.uRoomID != GENESIS_UNPLACED_ID &&
			resNamespaceBaseNameEq(component->sPlacement.pcMapName, map_name))
		{
			sprintf(ret_buf, "%s_%s", g_UGCMissionName, ugcComponentGetLogicalNameTemp(component));
			return ret_buf;
		}
	}
	UGC_FOR_EACH_COMPONENT_END;

	// Last fallback: Let Genesis system generate a Start Spawn
	return "UGC_SPAWN_INTERNAL";
}

ContainerID ugcProjectContainerID( UGCProjectData *data )
{
	const char* ns = data->ns_name;
	assert( namespaceIsUGC( ns ));
	return UGCProject_GetContainerIDFromUGCNamespace( data->ns_name );
}

MissionPlayType ugcProjectPlayType( UGCProjectData* ugcProj )
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();

	int it;
	for( it = 0; it != eaSize( &ugcProj->components->eaComponents ); ++it ) {
		UGCComponent* component = ugcProj->components->eaComponents[ it ];
		if( component->eType == UGC_COMPONENT_TYPE_KILL ) {
			return defaults->combatType;
		}
	}

	return defaults->nonCombatType;
}

char* ugcAllocSMFString( const char* str, bool allowComplex )
{
	if( nullStr( str )) {
		return NULL;
	} else {
		char* estr = estrCreateFromStr( str );
		char* accum = NULL;

		// ORDERING HERE IS IMPORTANT!  DO NOT REORDER OR YOU WILL SUMMON
		// SECURITY HOLE DEMONS THAT WILL POKE YOU WITH THEIR PITCHFORKS.

		// 0. Remove the ampersands.
		estrReplaceOccurrences( &estr, "&", "&amp;" );

		// 1.  Remove anything that could look like a SMF tag
		estrReplaceOccurrences( &estr, "<", "&lt;" );
		estrReplaceOccurrences( &estr, ">", "&gt;" );
		estrReplaceOccurrences( &estr, "{", "&lcb;" );
		estrReplaceOccurrences( &estr, "}", "&rcb;" );

		if( allowComplex ) {
			// 2. Convert our BB-code style tags to SMF tagsand
			estrReplaceOccurrences_CaseInsensitive( &estr, "[MissionInfo]", "<font style=Mission_Info>" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[/MissionInfo]", "</font>" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[OOC]", "<font style=OOC>" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[/OOC]", "</font>" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[Nickname]", "{CharName}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[FirstName]", "{FormalFirstName}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[LastName]", "{FormalLastName}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[Rank]", "{CharacterRank}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[ShipName]", "{CurrentShipName}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[ShipFullName]", "{CurrentShipFormalName}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[ShipRegistry]", "{CurrentShipRegistry}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[ShipType]", "{CurrentShipType}" );

			// boff replacements
			estrReplaceOccurrences_CaseInsensitive( &estr, "[BoffTacShip]", "{Player.Pet.Tactical_Space}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[BoffSciShip]", "{Player.Pet.Science_Space}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[BoffEngShip]", "{Player.Pet.Engineering_Space}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[BoffTacTeam]", "{Player.Pet.Tactical_Ground}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[BoffSciTeam]", "{Player.Pet.Science_Ground}" );
			estrReplaceOccurrences_CaseInsensitive( &estr, "[BoffEngTeam]", "{Player.Pet.Engineering_Ground}" );

			// 3. Convert newlines to <br>s
			estrReplaceOccurrences( &estr, "\n", "<br>" );
		} else {
			char* newlinePtr = strchr( estr, '\n' );
			if( newlinePtr ) {
				estrSetSize( &estr, newlinePtr - estr );
			}
		}

		accum = StructAllocString( estr );
		estrDestroy( &estr );
		return accum;
	}
}

PlayerCostume* ugcPlayerCostumeFromString( UGCProjectData* data, const char* costumeName )
{
	int it;
	for( it = 0; it != eaSize( &data->costumes ); ++it ) {
		if( stricmp( data->costumes[ it ]->astrName, costumeName ) == 0 ) {
			return data->costumes[ it ]->pPlayerCostume;
		}
	}

    return RefSystem_ReferentFromString( g_hPlayerCostumeDict, costumeName );
}

WorldVariableDef** ugcGetDefaultVariableDefs(void)
{
	UGCPerProjectDefaults* config = ugcGetDefaults();

	if( config ) {
		return config->variableDefs;
	} else {
		return NULL;
	}
}

MissionPlayType ugcDefaultsGetNonCombatType( void )
{
	UGCPerProjectDefaults* config = ugcGetDefaults();
	return SAFE_MEMBER( config, nonCombatType );
}

UGCSpecialComponentDef* ugcDefaultsSpecialComponentDef( const char* specialName )
{
	UGCPerProjectDefaults* config = ugcGetDefaults();
	if( config ) {
		FOR_EACH_IN_EARRAY( config->eaSpecialComponents, UGCSpecialComponentDef, specialDef ) {
			if( stricmp( specialDef->pcLabel, specialName ) == 0 ) {
				return specialDef;
			}
		} FOR_EACH_END;
	}

	return NULL;
}

static bool ugcGroupDefPathsMatch( GroupDef* def, int* fullIdx, int fullIdxSize, char** scopePath, int scopePathSize )
{
	if( scopePathSize > fullIdxSize ) {
		return false;
	} else {
		GroupDef* defIt = def;
		int it;
		for( it = 0; it < scopePathSize; ++it ) {
			GroupChild* child = defIt->children[ fullIdx[ it ]];

			if( !isdigit( scopePath[ it ][ 0 ])) {
				// DO A LOOKUP HERE
				return ugcGroupDefPathsMatch( defIt, fullIdx + it, fullIdxSize - it, scopePath + it, scopePathSize - it );
			} else if( (U32)atoi( scopePath[ it ]) != child->uid_in_parent ) {
				return false;
			}

			defIt = groupChildGetDef( defIt, child, false );
			if( !defIt ) {
				return false;
			}
		}

		return true;
	}
}

typedef struct RoomHelperData {
	GroupDef* rootDef;
	UGCResourceInfo* rootResInfo;
	UGCRoomInfo* roomInfo;
} RoomHelperData;

static char* ugcGroupDefFindLogicalName( RoomHelperData* data, GroupDef* def, GroupInheritedInfo* inheritedInfo )
{
	int i;
	int *child_path = NULL;
	char *scope_name;

	// MJF: This code is horribly unperformant.  It would be
	// beneficial to make this use eaiPush() and not memmove in a
	// loop.
	for (i = eaiSize( &inheritedInfo->idxs_in_parent)-1; i > 0; --i)
	{
		eaiInsert(&child_path, inheritedInfo->parent_defs[i-1]->children[inheritedInfo->idxs_in_parent[i]]->uid_in_parent, 0);
	}

	if (groupDefFindScopeNameByFullPath(inheritedInfo->parent_defs[0], child_path, eaiSize(&child_path), &scope_name))
	{
		return scope_name;
	}

	return NULL;
}


static bool ugcValidateGroupDefRoomHelper(RoomHelperData* data, GroupDef* def, GroupInfo* info, GroupInheritedInfo* inheritedInfo, bool needsEntry )
{
	GroupDef* rootDef = data->rootDef;
	UGCRoomInfo* roomInfo = data->roomInfo;

	// Check that the room sound properties can be overridden
	if( SAFE_MEMBER( def->property_structs.room_properties, eRoomType ) == WorldRoomType_Room ) {
		if( SAFE_MEMBER( def->property_structs.client_volume.sound_volume_properties, event_name_override_param ) != allocAddString( "Room_Tone" ) ) {
			ErrorFilenamef( rootDef->filename, "GroupDef %s - Room %s (%d) must have Sound attributes with 'Event Override Param' set to 'Room_Tone'.",
				rootDef->name_str, def->name_str, def->name_uid );
		}
		if( SAFE_MEMBER( def->property_structs.client_volume.sound_volume_properties, dsp_name_override_param ) != allocAddString( "Room_DSP" ) ) {
			ErrorFilenamef( rootDef->filename, "GroupDef %s - Room %s (%d) must have Sound attributes with 'DSP Override Param' set to 'Room_DSP'.",
				rootDef->name_str, def->name_str, def->name_uid );
		}
	}

	// Check explicit footprint volumes are on the grid.
	//
	// (We don't need to check implicit volumes, because the room
	// volume not being on the grid would force the doors to not be on
	// the grid.
	if( def->property_structs.volume != NULL && groupIsVolumeType( def, "UGCRoomFootprint" )) {
		F32 pyr[3];
		Vec3 volumeMin;
		Vec3 volumeMax;

		getMat3YPR( info->world_matrix, pyr );
		mulVecMat4( def->property_structs.volume->vBoxMin, info->world_matrix, volumeMin );
		mulVecMat4( def->property_structs.volume->vBoxMax, info->world_matrix, volumeMax );

		if( !nearz( pyr[ 0 ]) || !nearz( pyr[ 2 ])) {
			ErrorFilenamef( rootDef->filename, "GroupDef %s - Footprint volume %s (%d) pitch and roll must both be zero.",
							rootDef->name_str, def->name_str, def->name_uid );
		}
		if( !near_multiple( pyr[ 1 ], HALFPI )) {
			ErrorFilenamef( rootDef->filename, "GroupDef %s - Footprint volume %s (%d) yaw must be a multiple of 90 degrees.",
							rootDef->name_str, def->name_str, def->name_uid );
		}
		if(   !near_multiple( volumeMin[ 0 ], UGC_ROOM_GRID ) || !near_multiple( volumeMax[ 0 ], UGC_ROOM_GRID )
			  || !near_multiple( volumeMin[ 2 ], UGC_ROOM_GRID ) || !near_multiple( volumeMax[ 2 ], UGC_ROOM_GRID )) {
			ErrorFilenamef( rootDef->filename, "GroupDef %s - Footprint volume %s (%d)'s X,Z must be kept to the UGC grid of %f.",
							rootDef->name_str, def->name_str, def->name_uid, UGC_ROOM_GRID );
		}
	}

	if( SAFE_MEMBER( def->property_structs.ugc_room_object_properties, eType ) == UGC_ROOM_OBJECT_DOOR ) {
		// Check all doors are on the grid
		F32 pyr[ 3 ];
		F32* pos = info->world_matrix[ 3 ];
		Vec3 unrotatedPos;

		getMat3YPR( info->world_matrix, pyr );

		mulVecMat3Transpose( pos, info->world_matrix, unrotatedPos );

		if( !nearz( pyr[ 0 ]) || !nearz( pyr[ 2 ])) {
			ErrorFilenamef( rootDef->filename, "GroupDef %s - Door %s (%d) anchor pitch and roll must both be zero.",
							rootDef->name_str, def->name_str, def->name_uid );
		}
		if( !near_multiple( pyr[ 1 ], HALFPI )) {
			ErrorFilenamef( rootDef->filename, "GroupDef %s - Door %s (%d) yaw must be a multiple of 90 degrees.",
							rootDef->name_str, def->name_str, def->name_uid );
		}

		if( data->rootResInfo->pUGCProperties->groupDefProps.bRoomDoorsEverywhere ) {
			// Why this error treats X,Z differently requires a graphic:
			//
			// A VALID ROOM. DOORS HAVE A BUILT IN ROTATION TO POINT
			// OUTSIDE OF THE ROOM
			// 
			//	 ---------- VALID DOOR -- delta = (0, 1.5), rot = 0
			//	 V
			// +-*-+
			// |   |
			// |   * <----- VALID DOOR -- delta = (.5, 1),  rot = 90
			// |   |
			// +---+---+
			// |   |   |
			// | *<-------- PIVOT POINT
			// |   |   |
			// +---+---+
			//
			// A COMMON BADLY SETUP ROOM
			// 
			//	 ---------- VALID DOOR -- delta = (-.5, 1), rot = 0
			//	 V
			// +-*-+
			// |   |
			// |   * <----- VALID DOOR -- delta = (0, .5),  rot = 90
			// |   |
			// +---*---+
			// |   ^   |
			// |   -------- PIVOT POINT
			// |   |   |
			// +---+---+
			if( !near_multiple( unrotatedPos[ 0 ], UGC_KIT_GRID ) || !near_multiple( unrotatedPos[ 2 ] - UGC_KIT_GRID / 2, UGC_KIT_GRID )) {
				ErrorFilenamef( rootDef->filename, "GroupDef %s - Door %s (%d)'s X,Z must be kept to the UGC kit grid of %f.  This is a requirement for all Doors Anywhere rooms. (If this happens but the X,Z are on the room grid of %f, the pivot point is probably not set up correctly.)",
								rootDef->name_str, def->name_str, def->name_uid, UGC_KIT_GRID, UGC_ROOM_GRID );
			}
		} else {
			if( !near_multiple( pos[ 0 ], UGC_ROOM_GRID ) || !near_multiple( pos[ 2 ], UGC_ROOM_GRID )) {
				ErrorFilenamef( rootDef->filename, "GroupDef %s - Door %s (%d)'s X,Z must be kept to the room grid of %f",
								rootDef->name_str, def->name_str, def->name_uid, UGC_ROOM_GRID );
			}
		}
		if( !nearz( pos[ 1 ])) {
			ErrorFilenamef( rootDef->filename, "GroupDef %s - Door %s (%d)'s Y must be exactly the same as the room.",
							rootDef->name_str, def->name_str, def->name_uid );
		}

		// Check that all doors have a parent room volume
		{
			bool foundRoom = false;
			int it;
			for( it = 0; it != eaSize( &inheritedInfo->parent_defs ); ++it ) {
				GroupDef* parentDef = inheritedInfo->parent_defs[ it ];
				
				if( SAFE_MEMBER( parentDef->property_structs.room_properties, eRoomType ) == WorldRoomType_Room ) {
					foundRoom = true;
					break;
				}
			}

			if( !foundRoom ) {
				ErrorFilenamef( rootDef->filename, "GroupDef %s - Door %s (%d) should be a child of the room volume.",
								rootDef->name_str, def->name_str, def->name_uid );
			}
		}

		// Check that inheritence is set up correctly
		{
			char* logicalName = ugcGroupDefFindLogicalName( data, def, inheritedInfo );
			bool foundInheritence = false;
			if( logicalName ) {
				int it;
				for( it = 0; it < GROUP_CHILD_MAX_PARAMETERS && info->parameters[ it ].parameter_name; ++it ) {
					if( stricmp( logicalName, info->parameters[ it ].inherit_value_name ) == 0 ) {
						foundInheritence = true;
					}
				}
			}

			if( !foundInheritence ) {
				ErrorFilenamef( rootDef->filename, "GroupDef %s - Door %s (%d) must be set up to inherit a value based on its logical name, %s.",
								rootDef->name_str, def->name_str, def->name_uid, logicalName );
			}
		}
	}

	// Check that the Prepop set and detail sets are all direct children
	if( SAFE_MEMBER( def->property_structs.ugc_room_object_properties, eType ) == UGC_ROOM_OBJECT_DETAIL_SET ) {
		if( eaSize( &inheritedInfo->parent_defs ) > 2 ) {
			ErrorFilenamef( rootDef->filename, "GroupDef %s - Detail Set %s (%d) must be a direct child of the room.",
							rootDef->name_str, def->name_str, def->name_uid );
		}
	}
	if( SAFE_MEMBER( def->property_structs.ugc_room_object_properties, eType ) == UGC_ROOM_OBJECT_PREPOP_SET ) {
		if( eaSize( &inheritedInfo->parent_defs ) > 2 ) {
			ErrorFilenamef( rootDef->filename, "GroupDef %s - Population Set %s (%d) must be a direct child of the room.",
							rootDef->name_str, def->name_str, def->name_uid );
		}
	}

	// Validate that all platforms are of type Volume_AllSides or Volume_Floor
	if( def->property_structs.terrain_exclusion_properties ) {
		if(   def->property_structs.terrain_exclusion_properties->platform_type != WorldPlatformType_Volume_AllSides
			  && def->property_structs.terrain_exclusion_properties->platform_type != WorldPlatformType_Volume_Floor ) {
			ErrorFilenamef( rootDef->filename, "GroupDef %s - Platform %s (%d) must be of type Volume_AllSides or Volume_Floor.",
							rootDef->name_str, def->name_str, def->name_uid );
		}
	}

	return true;
}

static bool ugcValidateGroupDefNonRoomHelper(GroupDef* rootDef, GroupDef* def, GroupInfo* info, GroupInheritedInfo* inheritedInfo, bool needsEntry )
{
	if(   SAFE_MEMBER( def->property_structs.ugc_room_object_properties, eType ) == UGC_ROOM_OBJECT_DOOR ) {
		ErrorFilenamef( rootDef->filename, "GroupDef %s - All rooms must have the tag \"Room\".  (Assuming this is a room because it includes a door.)",
						rootDef->name_str );
	}

	return true;
}

void ugcValidateGroupDef( UGCResourceInfo* resInfo, GroupDef* def )
{
	bool isUGC = false;
	bool isRoom = false;
	bool isDoor = false;
	bool isCluster = false;
	bool isTeleporter = false;

	{
		char** tags = NULL;
		int it;
		DivideString( resInfo->pResInfo->resourceTags, ",", &tags, DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS );
		for( it = 0; it != eaSize( &tags ); ++it ) {
			const char* tag = tags[ it ];
			if( stricmp( tag, "ugc" ) == 0 ) {
				isUGC = true;
			}
			if( stricmp( tag, "Room" ) == 0 ) {
				isRoom = true;
			}
			if( stricmp( tag, "RoomDoor" ) == 0 ) {
				isDoor = true;
			}
			if( stricmp( tag, "Cluster" ) == 0 ) {
				isCluster = true;
			}
			if( stricmp( tag, "Teleporter" ) == 0 ) {
				isTeleporter = true;
			}
		}
		eaDestroy( &tags );
	}

	if( !isUGC ) {
		return;
	}

	if( isRoom ) {
		UGCRoomInfo* roomInfo = ugcRoomAllocRoomInfo( def );

		// Validate a room info is created
		if( !roomInfo ) {
			ErrorFilenamef( def->filename, "GroupDef %s - Tagged as room, but the room basics (Platform, Volume, Footprint, etc.) are not all there.", def->name_str );
			return;
		}

		// Validate the pivot point is in the center of the footprint
		if(   abs( roomInfo->footprint_min[0] + roomInfo->footprint_max[0] ) > 3
			  || abs( roomInfo->footprint_min[1] + roomInfo->footprint_max[1] ) > 3 ) {
			ErrorFilenamef( def->filename, "GroupDef %s - Pivot point must be near the center of the room.", def->name_str );
		}

		// Validate that there is a footprint
		if(   roomInfo->footprint_min[0] > roomInfo->footprint_max[0]
			  || roomInfo->footprint_min[1] > roomInfo->footprint_max[1] ) {
			ErrorFilenamef( def->filename, "GroupDef %s - No footprint detected.  For rectangle rooms, the room volume can be a footprint.",
							def->name_str );
		}

		{
			RoomHelperData data;
			data.rootDef = def;
			data.rootResInfo = resInfo;
			data.roomInfo = roomInfo;
			groupTreeTraverse( NULL, def, NULL, NULL, ugcValidateGroupDefRoomHelper, &data, true, true );
		}
		ugcRoomFreeRoomInfo( roomInfo );
	} else {
		groupTreeTraverse( NULL, def, NULL, NULL, ugcValidateGroupDefNonRoomHelper, def, true, true );
	}

	if( isDoor ) {
		// Validate the number of children is correct
		if( eaSize( &def->children ) != UGC_OBJLIB_ROOMDOOR_NUM_CHILDREN ) {
			ErrorFilenamef( def->filename, "GroupDef %s - Door must have exactly %d children, #%d is a fake door, #%d is a locked door, #%d is an unlocked door.",
							def->name_str,
							UGC_OBJLIB_ROOMDOOR_NUM_CHILDREN, UGC_OBJLIB_ROOMDOOR_FAKE_CHILD + 1,
							UGC_OBJLIB_ROOMDOOR_LOCKED_CHILD + 1, UGC_OBJLIB_ROOMDOOR_UNLOCKED_CHILD + 1 );
		}
	}

	if( isTeleporter ) {
		// Validate that the children are named as expected
		int it;
		for( it = 0; it != eaSize( &def->children ); ++it ) {
			if( !groupDefFindScopeNameByFullPath( def, &def->children[ it ]->uid_in_parent, 1, NULL )) {
				ErrorFilenamef( def->filename, "GroupDef %s, Child %s - All of the teleporter's immediate children must be named.  You probably should make the interactive parts of the teleporter immediate children.",
								def->name_str, def->children[ it ]->debug_name );
			}
		}
	}
}

void ugcValidateGroupDefLate( UGCResourceInfo* resInfo, GroupDef* def )
{
	bool isUGC = false;
	bool isRoom = false;
	bool isDoor = false;
	bool isCluster = false;
	bool isEncounter = false;
	{
		char** tags = NULL;
		int it;
		DivideString( resInfo->pResInfo->resourceTags, ",", &tags, DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS );
		for( it = 0; it != eaSize( &tags ); ++it ) {
			const char* tag = tags[ it ];
			if( stricmp( tag, "ugc" ) == 0 ) {
				isUGC = true;
			}
			if( stricmp( tag, "Room" ) == 0 ) {
				isRoom = true;
			}
			if( stricmp( tag, "RoomDoor" ) == 0 ) {
				isDoor = true;
			}
			if( stricmp( tag, "Cluster" ) == 0 ) {
				isCluster = true;
			}
			if( stricmp( tag, "Encounter" ) == 0 ) {
				isEncounter = true;
			}
		}
		eaDestroyEx( &tags, NULL );
	}

	if( !isUGC ) {
		return;
	}

	if( isRoom ) {
		UGCRoomInfo* roomInfo = ugcRoomAllocRoomInfo( def );

		// Validate all populate set objects are tagged for UGC.
		if( roomInfo ) {
			int setIt;
			int objectIt;
			for( setIt = 0; setIt != eaSize( &roomInfo->populates ); ++setIt ) {
				UGCRoomPopulateDef* set = roomInfo->populates[ setIt ];
				for( objectIt = 0; objectIt != eaSize( &set->eaObjects ); ++objectIt ) {
					UGCRoomPopulateObject* object = set->eaObjects[ objectIt ];
					ResourceInfo* objectResInfo = ugcResourceGetInfoInt( "ObjectLibrary", object->iGroupUID );

					if( object->iGroupUID > 0 ) {
						ErrorFilenamef( def->filename, "Room %s, Populate Set %s - Object %s (%d) is not an object library piece.  It was probably inadvertantly instanced.",
										def->name_str, TranslateMessageRef( set->hDisplayName ), object->astrGroupDebugName, object->iGroupUID );
					} else if( !objectResInfo || !ugcHasTag( objectResInfo->resourceTags, "UGC" )) {
						ErrorFilenamef( def->filename, "Room %s, Populate Set %s - Object %s (%d) is not available in UGC.",
										def->name_str, TranslateMessageRef( set->hDisplayName ), object->astrGroupDebugName, object->iGroupUID);
					}
				}
			}
		}
		
		ugcRoomFreeRoomInfo( roomInfo );
	}

	if( isCluster ) {
		UGCGroupDefMetadata* defMetadata = ugcResourceGetGroupDefMetadataInt( def->name_uid );
		if(!defMetadata) {
			ErrorFilenamef( def->filename, "Cluster %s -- No UGCGroupDefMetadata found.", def->name_str );
		} else if( eaSize( &defMetadata->eaClusterChildren ) == 0 ) {
			ErrorFilenamef( def->filename, "Cluster %s -- No cluster children were found.", def->name_str );
		} else {
			FOR_EACH_IN_EARRAY( defMetadata->eaClusterChildren, UGCDefChildMetadata, child ) {
				GroupDef* childDef = objectLibraryGetGroupDef( child->defUID, false );
				ResourceInfo* childMetadata = ugcResourceGetInfoInt( "ObjectLibrary", child->defUID );

				if( !childDef ) {
					ErrorFilenamef( def->filename, "Cluster %s -- Child #%d (Objlib %d) does not exist!",
									def->name_str, FOR_EACH_IDX( defMetadata->eaClusterChildren, child ) + 1, child->defUID );
				} else if( !childMetadata || !ugcHasTag( childMetadata->resourceTags, "Detail" )) {
					ErrorFilenamef( def->filename, "Cluster %s -- Child %s is not tagged as a Detail object for UGC.",
									def->name_str, childDef->name_str );
				}
			} FOR_EACH_END;
		}
	}

	if( isEncounter ) {
		if (!def->property_structs.encounter_properties)
		{
			ErrorFilenamef( def->filename, "Encounter %s -- No Encounter attributes exist!", def->name_str );
		}
		else
		{
			// Commenting out for now, until we get rid of the errors.
			// Then, we will blame the CritterDef file for the costume errors.
			/*EncounterTemplate *rootenctemplate = GET_REF(def->property_structs.encounter_properties->hTemplate);
			EncounterTemplate *enctemplate = rootenctemplate;
			while (enctemplate && eaSize(&enctemplate->eaActors) == 0)
				enctemplate = GET_REF(enctemplate->hParent);
			if (!enctemplate)
			{
				ErrorFilenamef( def->filename, "Encounter %s -- Encounter Template (%s) contains no Actors!",
					def->name_str, rootenctemplate->pcName );
			}
			else
			{
				int actor_idx;
				for (actor_idx = 0; actor_idx < eaSize(&enctemplate->eaActors); actor_idx++)
				{
					CritterDef *critter_def = GET_REF(enctemplate->eaActors[actor_idx]->critterProps.hCritterDef);
					if(critter_def)
					{
						int costume_idx;
						CritterGroup *critter_group_def = GET_REF(critter_def->hGroup);

						for(costume_idx = 0; costume_idx < eaSize(&critter_def->ppCostume); costume_idx++)
						{
							PlayerCostume *pPlayerCostume = GET_REF(critter_def->ppCostume[costume_idx]->hCostumeRef);
							if(!ugcResourceGetUGCProperties("PlayerCostume", pPlayerCostume->pcName))
							{
								ErrorFilenamef( def->filename, "Encounter %s -- Encounter Template (%s) contains an Actor (%s) using a random Player Costume (%s) that is not itself tagged for UGC!",
									def->name_str, rootenctemplate->pcName, enctemplate->eaActors[actor_idx]->pcName, pPlayerCostume->pcName );
							}
						}
					}
				}
			}*/
		}
	}
}

void ugcValidateZoneMapInfo( UGCResourceInfo* resInfo, ZoneMapInfo* zmapInfo )
{
	if( zmapInfoGetUsedInUGC( zmapInfo ) != ZMAP_UGC_USED_AS_ASSET ) {
		ErrorFilenamef( zmapInfoGetFilename( zmapInfo ), "ZoneMap %s - Map file is not marked in the Environment Editor as \"UGC Used as Asset\".", zmapInfoGetPublicName( zmapInfo ));
	}
}

AUTO_STARTUP(UGC) ASTRT_DEPS(WorldLibZone);
void ugcStartup( void )
{
	if( !IsClient() || gbMakeBinsAndExit ) {
		ugcLoadDictionaries();
	}
}

static UGCComponentPatrolPoint** ugcPatrolCreateDoorPointsMaybe( const UGCProjectData* ugcProj, const UGCComponent* prev, const UGCComponent* next )
{
	UGCRoomInfo* nextInfo;
	int doorIt;

	if( !prev || !next ) {
		return NULL;
	}

	assert( prev->eType == UGC_COMPONENT_TYPE_ROOM && next->eType == UGC_COMPONENT_TYPE_ROOM );
	nextInfo = ugcRoomGetRoomInfo( next->iObjectLibraryId );

	for( doorIt = 0; doorIt != eaSize( &nextInfo->doors ); ++doorIt ) {
		UGCComponent* connected = NULL;
		UGCComponent* doorComponent = NULL;
		UGCDoorSlotState state = ugcRoomGetDoorSlotState( ugcProj->components, next, doorIt, &doorComponent, NULL, &connected, NULL );
		// The connection is only valid if this is a door openning to
		// the specified room, or in English:
		//
		// * The door slot is occupied
		// * The door slot connects to the previous room
		// * The door slot's door component has no object library piece (it is an openning).
		if(   state == UGC_DOOR_SLOT_OCCUPIED && connected && doorComponent
			  && connected == prev && !doorComponent->iObjectLibraryId ) {
			UGCComponentPatrolPoint** points = NULL;
			UGCComponentPatrolPoint* point;
			Vec3 doorLocalPos;
			Vec3 rawOffset = { 0, 0, 5 };
			Vec3 offset;

			// MJF: Using a matrix here seems wasteful...
			{
				Mat3 rotMatrix;
				identityMat3( rotMatrix );
				yawMat3( RAD( ugcRoomGetDoorLocalRot( nextInfo, doorIt )), rotMatrix );
				mulVecMat3( rawOffset, rotMatrix, offset );
			}

			point = StructCreate( parse_UGCComponentPatrolPoint );
			ugcRoomGetDoorLocalPos( nextInfo, doorIt, doorLocalPos );
			addVec3( doorLocalPos, offset, doorLocalPos );
			ugcRoomConvertLocalToWorld( next, doorLocalPos, point->pos );
			eaPush( &points, point );

			point = StructCreate( parse_UGCComponentPatrolPoint );
			ugcRoomGetDoorLocalPos( nextInfo, doorIt, doorLocalPos );
			subVec3( doorLocalPos, offset, doorLocalPos );
			ugcRoomConvertLocalToWorld( next, doorLocalPos, point->pos );
			eaPush( &points, point );

			return points;
		}
	}

	return NULL;
}

UGCComponentPatrolPath *ugcComponentGetPatrolPath(const UGCProjectData *ugcProj, const UGCComponent *component, const UGCComponentPatrolPoint** positionOverrides)
{
	UGCComponentPatrolPath *path = StructCreate(parse_UGCComponentPatrolPath);
	UGCComponent* firstPatrolPoint;
	UGCComponent* startRoomComponent;
	UGCComponent* prevRoomComponent;
	int i;

	if( eaiSize( &component->eaPatrolPoints ) == 0 ) {
		return path;
	}
	ugcComponentHasPatrol( component, &path->patrolType );
	firstPatrolPoint = ugcComponentFindByID( ugcProj->components, component->eaPatrolPoints[ 0 ]);
	startRoomComponent = ugcComponentGetRoomParent( ugcProj->components, firstPatrolPoint );
	prevRoomComponent = ugcComponentGetRoomParent( ugcProj->components, firstPatrolPoint );
	
	for( i = 0; i < eaiSize( &component->eaPatrolPoints ); i++ ) {
		const UGCComponentPatrolPoint* override = NULL;
		UGCComponent* roomComponent = NULL;
		{
			int overrideIt;
			for( overrideIt = 0; overrideIt != eaSize( &positionOverrides ); ++overrideIt ) {
				if( positionOverrides[ overrideIt ]->componentID == component->eaPatrolPoints[i] ) {
					override = positionOverrides[ overrideIt ];
					break;
				}
			}
		}

		if( override ) {
			eaPush( &path->points, StructClone( parse_UGCComponentPatrolPoint, override ));
			roomComponent = ugcComponentFindByID( ugcProj->components, override->roomID );
		} else {
			UGCComponent* pointComponent = ugcComponentFindByID(ugcProj->components, component->eaPatrolPoints[i]);

			if( pointComponent ) {
				UGCComponentPatrolPoint *point = StructCreate( parse_UGCComponentPatrolPoint );
				eaPush(&path->points, point);

				point->componentID = pointComponent->uID;
				copyVec3( pointComponent->sPlacement.vPos, point->pos );
				roomComponent = ugcComponentGetRoomParent( ugcProj->components, pointComponent );
				if( roomComponent ) {
					point->roomID = roomComponent->uID;
				}
			}
		}

		if( !prevRoomComponent ) {
			prevRoomComponent = roomComponent;
		}

		if( roomComponent != prevRoomComponent ) {
			UGCComponentPatrolPoint** doorPoints = ugcPatrolCreateDoorPointsMaybe( ugcProj, prevRoomComponent, roomComponent );
			int last = eaSize( &path->points ) - 1;
			if( doorPoints ) {
				eaInsertEArray( &path->points, &doorPoints, last );
				eaDestroy( &doorPoints );
			} else {
				path->points[ last ]->prevConnectionInvalid = true;
			}
			prevRoomComponent = roomComponent;
		}
	}

	if( prevRoomComponent != startRoomComponent && path->patrolType == PATROL_CIRCLE ) {
		UGCComponentPatrolPoint** doorPoints = ugcPatrolCreateDoorPointsMaybe( ugcProj, prevRoomComponent, startRoomComponent );
		if( doorPoints ) {
			eaPushEArray( &path->points, &doorPoints );
			eaDestroy( &doorPoints );
		} else {
			int last = eaSize( &path->points ) - 1;
			path->points[ last ]->nextConnectionInvalid = true;
		}
	}

	return path;
}

static bool ugcTrapGetEmittersHelper( UGCTrapProperties *list, GroupDef *def, GroupInfo *info, GroupInheritedInfo *inheritedInfo, bool needsEntry )
{
	if (strStartsWith(def->name_str, "emitter_"))
	{
		int emitter_id;
		if (sscanf(def->name_str, "Emitter_%d", &emitter_id) == 1)
		{
			UGCTrapPointData *emitter = StructCreate(parse_UGCTrapPointData);
			emitter->id = emitter_id-1;
			copyVec3(info->world_matrix[3], emitter->pos);
			if (eaSize(&list->eaEmitters) < emitter->id+1)
				eaSetSize(&list->eaEmitters, emitter->id+1);
			list->eaEmitters[emitter->id] = emitter;
		}
	}
	if (stricmp(def->name_str, "TrapTriggerVolume") == 0 && !list->pSelfContained &&
		def->property_structs.volume)
	{
		list->pSelfContained = StructCreate(parse_UGCTrapSelfContained);
		list->pSelfContained->pVolume = StructClone(parse_GroupVolumeProperties, def->property_structs.volume);
	}
	return true;
}

UGCTrapProperties *ugcTrapGetProperties(GroupDef *def)
{
	if( def ) {
		Mat4 id_matrix;
		UGCTrapProperties *list = StructCreate(parse_UGCTrapProperties);
		identityMat4(id_matrix);
		groupTreeTraverse(NULL, def, id_matrix, NULL, ugcTrapGetEmittersHelper, list, true, true);
		return list;
	} else {
		return NULL;
	}
}

UGCComponent* ugcTrapFindEmitter( UGCProjectData* ugcProj, UGCComponent* trap )
{
	int it;
	if( !trap ) {
		return NULL;
	}
	for( it = 0; it != eaiSize( &trap->uChildIDs ); ++it ) {
		UGCComponent* trapChild = ugcComponentFindByID( ugcProj->components, trap->uChildIDs[ it ]);
		if( trapChild && trapChild->eType == UGC_COMPONENT_TYPE_TRAP_EMITTER ) {
			return trapChild;
		}
	}
	return NULL;
}

UGCAssetTagCategory *ugcSkyGetSlotCategory(UGCMapType map_type)
{
	UGCAssetTagType *tag_type = NULL;
	UGCAssetTagCategory *slot_category = NULL;
	switch (map_type)
	{
		case UGC_MAP_TYPE_SPACE:
		case UGC_MAP_TYPE_PREFAB_SPACE:
			tag_type = RefSystem_ReferentFromString("TagType", "SpaceSkyLayer");
			break;
		case UGC_MAP_TYPE_GROUND:
		case UGC_MAP_TYPE_PREFAB_GROUND:
			tag_type = RefSystem_ReferentFromString("TagType", "GroundSkyLayer");
			break;
		default:
			tag_type = RefSystem_ReferentFromString("TagType", "InteriorSkyLayer");
			break;
	}
	if (tag_type)
	{
		FOR_EACH_IN_EARRAY(tag_type->eaCategories, UGCAssetTagCategory, category)
		{
			if (stricmp(category->pcName, "Slot") == 0)
			{
				slot_category = category;
				break;
			}
		}
		FOR_EACH_END;
	}
	return slot_category;
}

UGCFSMVar* ugcComponentBehaviorGetFSMVar(UGCComponent *component, const char* name)
{
	name = allocAddString(name);
	FOR_EACH_IN_EARRAY( component->fsmProperties.eaExternVarsV1, UGCFSMVar, var ) {
		if( var->astrName == name ) {
			return var;
		}
	} FOR_EACH_END;

	return NULL;
}

const char* ugcDialogTreePromptCameraPos( const UGCDialogTreePrompt* prompt )
{
	UGCPerProjectDefaults *defaults = ugcGetDefaults();

	return (defaults->pcDialogTreePromptCameraPosition && defaults->pcDialogTreePromptCameraPosition[0])
		? defaults->pcDialogTreePromptCameraPosition
		: "Contact_Base";
}

bool ugcTetheringAllowed()
{
	UGCPerProjectDefaults *defaults = ugcGetDefaults();

	return defaults->bTetheringAllowed;
}

bool ugcComponentSupportsNormalSnapping(UGCComponent* component)
{
	if(ugcGetDefaults()->bExteriorsAllowNormalSnapping)
		if(component->eMapType == UGC_MAP_TYPE_PREFAB_GROUND)
			return (component->eType == UGC_COMPONENT_TYPE_OBJECT || component->eType == UGC_COMPONENT_TYPE_CLUSTER_PART);
	return false;
}

bool ugcComponentPlacementNormalSnappingActive(UGCComponentPlacement* placement)
{
	return (placement->eSnap == COMPONENT_HEIGHT_SNAP_WORLDGEO || placement->eSnap == COMPONENT_HEIGHT_SNAP_TERRAIN);
}

// Whether or not this project (STO, NW) requires UGC messages to be translated
bool ugcDefaultsRequireTranslatedMessages()
{
	if(stricmp(GetProductName(), "Night") == 0)
		return true;
	else
		return false;
}

void ugcFormatMessageKey(char **estrResult, const char *pcKey, ...)
{
	VA_START(args, pcKey);
	estrClear(estrResult);
	FormatMessageKeyV(estrResult, pcKey, args);
	VA_END();
}

void ugcConcatMessageKey(char **estrResult, const char *pcKey, ...)
{
	VA_START(args, pcKey);
	estrClear(estrResult);
	FormatMessageKeyV(estrResult, pcKey, args);
	VA_END();
}

// Wrapper around FormatMessageKeyDefault that clears the result so it can be reused in one function a lot.
// This also gives UGC a hook to change Errorf behavior when a Message is untranslated. STO will not care, NW will care.
void ugcFormatMessageKeyDefault(char **estrResult, const char *pcKey, const char *pcDefault, ...)
{
	VA_START(args, pcDefault);
		estrClear(estrResult);
		if(ugcDefaultsRequireTranslatedMessages())
			FormatMessageKeyV(estrResult, pcKey, args); // This call generates Errorfs if Message is not found in Dictionary or is untranslated to current locale.
		else
			FormatMessageKeyDefaultV(estrResult, pcKey, NULL_TO_EMPTY(pcDefault), args); // This call will never generate an Errorf
	VA_END();
}

bool ugcGetBoundingVolumeFromPoints(UGCBoundingVolume* out_boundingVolume, F32 *points)
{
	int i;
	F32 angle, dist_sum;
	F32 best_angle = -1.f, best_dist_sum;
	F32 best_max_width=0, best_max_depth=0;
	Vec3 min = { 30000,  30000,  30000};
	Vec3 max = {-30000, -30000, -30000};
	F32 ratio;
	
	setVec3(out_boundingVolume->center, -9001, -9001, -9001);
	zeroVec3(out_boundingVolume->extents[0]);
	zeroVec3(out_boundingVolume->extents[1]);
	out_boundingVolume->rot = 0;

	if(eafSize(&points) == 0) {
		return false;
	}

	assert(eafSize(&points)%3 == 0);

	//Find the mid point
	for ( i=0; i < eafSize(&points); i+=3 )
	{
		Vec3 point;
		point[0] = points[i+0];
		point[1] = points[i+1];
		point[2] = points[i+2];
		MINVEC3(point, min, min);
		MAXVEC3(point, max, max);
	}
	addVec3(min, max, out_boundingVolume->center);
	scaleVec3(out_boundingVolume->center, 0.5f, out_boundingVolume->center);

	//Set vertical portion of extents
	out_boundingVolume->extents[0][1] = min[1] - out_boundingVolume->center[1] - 1;
	out_boundingVolume->extents[1][1] = max[1] - out_boundingVolume->center[1] + 1;

	// Find the best angle
	best_dist_sum = FLT_MAX;
	for ( angle = 0; angle < (180-15); angle += 15)
	{
		Vec2 angle_vec = {cos(RAD(angle)), sin(RAD(angle))};
		Vec2 angle_vec_norm = {-angle_vec[1], angle_vec[0]};
		F32 max_width=0, max_depth=0;

		dist_sum = 0;
		for ( i=0; i < eafSize(&points); i+=3 )
		{
			Vec2 point_vec = {points[i+0] - out_boundingVolume->center[0], points[i+2] - out_boundingVolume->center[2]};
			F32 dist_from_line = ABS(dotVec2(angle_vec_norm, point_vec));
			F32 dist_from_mid = ABS(dotVec2(angle_vec, point_vec));

			dist_sum += dist_from_line;

			if(dist_from_line > max_depth)
				max_depth = dist_from_line;
			if(dist_from_mid > max_width)
				max_width = dist_from_mid;			
		}
		if(dist_sum < best_dist_sum)
		{
			best_dist_sum = dist_sum;
			best_angle = angle;
			best_max_width = max_width;
			best_max_depth = max_depth;
		}
	}
	assert(best_angle >= 0);//We better have found an angle

	//Expand the ellipse
	//This basically finds the square that surrounds the circle that surrounds the square of half width = best_max_width
	//Then it stretches the square to be proportional to the original rectangle
	best_max_width = MAX(best_max_width, 50.0f);
	best_max_depth = MAX(best_max_depth, 50.0f);
	ratio = best_max_depth/best_max_width;
	out_boundingVolume->extents[1][0] = sqrt(SQR(best_max_width)*2);
	out_boundingVolume->extents[0][0] = -out_boundingVolume->extents[1][0];
	out_boundingVolume->extents[1][2] = out_boundingVolume->extents[1][0]*ratio;
	out_boundingVolume->extents[0][2] = -out_boundingVolume->extents[1][2];

	//Setup the mat
	out_boundingVolume->rot = RAD(-best_angle);

	return true;
}

DictionaryHandle g_UGCTagTypeDictionary = NULL;
bool ugcResourceInfosPopulated = false;


ResourceInfo* ugcResourceGetInfo(const char *dictName, const char *objName)
{
	char objNameBuffer[ RESOURCE_NAME_MAX_SIZE ];
	UGCResourceInfo* resInfo = NULL;

	sprintf( objNameBuffer, "%s__%s", dictName, objName );
	resInfo = RefSystem_ReferentFromString( UGC_DICTIONARY_RESOURCE_INFO, objNameBuffer );

	return SAFE_MEMBER( resInfo, pResInfo );
}

ResourceInfo* ugcResourceGetInfoInt(const char *dictName, int objName)
{
	char buffer[ RESOURCE_NAME_MAX_SIZE ];
	sprintf( buffer, "%d", objName );
	return ugcResourceGetInfo( dictName, buffer );
}

const WorldUGCProperties *ugcResourceGetUGCProperties(const char *dictName, const char *objName)
{
	char objNameBuffer[ RESOURCE_NAME_MAX_SIZE ];
	UGCResourceInfo *data;

	sprintf( objNameBuffer, "%s__%s", dictName, objName );
	data = RefSystem_ReferentFromString(UGC_DICTIONARY_RESOURCE_INFO, objNameBuffer);
	return SAFE_MEMBER(data, pUGCProperties);
}

const WorldUGCProperties* ugcResourceGetUGCPropertiesInt(const char* dictName, int objName)
{
	char buffer[ RESOURCE_NAME_MAX_SIZE ];
	sprintf( buffer, "%d", objName );
	return ugcResourceGetUGCProperties( dictName, buffer );
}

UGCFSMMetadata* ugcResourceGetFSMMetadata(const char *objName)
{
	const char* dictName = "FSM";
	char objNameBuffer[ RESOURCE_NAME_MAX_SIZE ];
	UGCResourceInfo *data;

	sprintf( objNameBuffer, "%s__%s", dictName, objName );
	data = RefSystem_ReferentFromString(UGC_DICTIONARY_RESOURCE_INFO, objNameBuffer);
	return SAFE_MEMBER(data, pFSMMetadata);
}

UGCCostumeMetadata* ugcResourceGetCostumeMetadata( const char* objName )
{
	const char* dictName = "PlayerCostume";
	char objNameBuffer[ RESOURCE_NAME_MAX_SIZE ];
	UGCResourceInfo *data;

	sprintf( objNameBuffer, "%s__%s", dictName, objName );
	data = RefSystem_ReferentFromString(UGC_DICTIONARY_RESOURCE_INFO, objNameBuffer);
	return SAFE_MEMBER(data, pCostumeMetadata);
}

UGCGroupDefMetadata* ugcResourceGetGroupDefMetadataInt( int objName )
{
	const char* dictName = "ObjectLibrary";
	char objNameBuffer[ RESOURCE_NAME_MAX_SIZE ];
	UGCResourceInfo* data;

	sprintf( objNameBuffer, "%s__%d", dictName, objName );
	data = RefSystem_ReferentFromString( UGC_DICTIONARY_RESOURCE_INFO, objNameBuffer );
	return SAFE_MEMBER( data, pDefMetadata );
}

UGCFSMExternVar* ugcResourceGetFSMExternVar( const char* objName, const char* varName )
{
	UGCFSMMetadata* fsmMetadata = ugcResourceGetFSMMetadata( objName );
	int it;
	varName = allocFindString( varName );
	for( it = 0; it != eaSize( &fsmMetadata->eaExternVars ); ++it ) {
		if( fsmMetadata->eaExternVars[ it ]->astrName == varName ) {
			return fsmMetadata->eaExternVars[ it ];
		}
	}

	return NULL;
}

//////////////////////////////////////////////
// Tag Validation
//////////////////////////////////////////////

AUTO_RUN;
void ugcInitTagTypeLibrary(void)
{
	g_UGCTagTypeDictionary = RefSystem_RegisterSelfDefiningDictionary( "TagType", false, parse_UGCAssetTagType, true, false, NULL);
}

// MJF TODO: Move the wlUGC.c dictionary loading to ugcLoadDictionaries()
void ugcLoadTagTypeLibrary()
{
	static bool tagtype_loaded = false;
	if (!areEditorsPossible() || tagtype_loaded)
		return;
	resLoadResourcesFromDisk( g_UGCTagTypeDictionary, "genesis/tagtypes", ".tagtype", "UGCTagTypes.bin", RESOURCELOAD_SHAREDMEMORY | PARSER_OPTIONALFLAG );
	tagtype_loaded = true;
}

bool ugcHasTagType(const char *tags, const char *dict_name, const char *tag_type)
{
	int i;
	char **tag_list = NULL;
	bool found_ugc = false, found_tag = false;
	UGCAssetTagType *prop_type = RefSystem_ReferentFromString("TagType", tag_type);

	if (!prop_type || stricmp(prop_type->pcDictName, dict_name) != 0)
		return false; // Tag type missing or doesn't go with this dictionary

	DivideString(tags, ",", &tag_list, DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS);
	ugcLoadTagTypeLibrary();
	for(i = 0; i < eaSize(&tag_list); i++)
	{
		char *tag = tag_list[i];
		if (stricmp(tag, "UGC") == 0)
		{
			found_ugc = true;
		}
		else if (stricmp(tag, tag_type) == 0)
		{
			found_tag = true;
		}
	}
	eaDestroyEx(&tag_list, NULL);
	return found_ugc && (found_tag || !prop_type->bFilterType);
}

bool ugcHasTag(const char *tags, const char *tag)
{
	char **tag_list = NULL;
	int i;
	DivideString( tags, ",", &tag_list, DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS );
	for(i = 0; i < eaSize(&tag_list); i++)
	{
		if (stricmp(tag, tag_list[i]) == 0)
		{
			eaDestroyEx(&tag_list, NULL);
			return true;
		}
	}

	eaDestroyEx(&tag_list, NULL);
	return false;
}

bool ugcGetTagValue(const char *tags, const char *category, char *value, size_t value_size)
{
	char **tag_list = NULL;
	int i;
	DivideString( tags, ",", &tag_list, DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS );
	for(i = 0; i < eaSize(&tag_list); i++)
	{
		char *tag = tag_list[i];
		if (value && strnicmp(tag, category, strlen(category)) == 0)
		{
			strcpy_s(SAFESTR2(value), &tag[strlen(category)+1]);
			eaDestroyEx(&tag_list, NULL);
			return true;
		}
	}

	eaDestroyEx(&tag_list, NULL);
	return false;
}

// JFinder - This was only called looking at GroupDef's tags.  UGC no
// longer looks at those tags.  Commenting out the function in case
// some of this validation is useful to roll in.
// 
// bool ugcValidateTags(const char *object_name, const char *object_filename,
// 					 const char *tags, const char *default_type, const char *dict_name)
// {
// 	int i;
// 	UGCAssetTagType **types = NULL;
// 	char **tag_list = NULL;
// 	char *estr_errors = NULL;
// 	char *estr_types = NULL;
// 	bool found_ugc = false;
// 	if (isProductionMode() || !areEditorsPossible())
// 		return false;
// 	estrCreate(&estr_errors);
// 	DivideString(tags, ",", &tag_list, DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS);
// 	ugcLoadTagTypeLibrary();
	
// 	// Gather types
// 	if (default_type)
// 	{
// 		UGCAssetTagType *prop_type;
// 		prop_type = RefSystem_ReferentFromString("TagType", default_type);
// 		if (prop_type && stricmp(prop_type->pcDictName, dict_name) == 0)
// 		{
// 			eaPush(&types, prop_type);
// 		}
// 	}
// 	for(i = 0; i < eaSize(&tag_list); i++)
// 	{
// 		char *tag = tag_list[i];
// 		if (stricmp(tag, "UGC") == 0)
// 		{
// 			found_ugc = true;
// 			continue;
// 		}
// 		else
// 		{
// 			UGCAssetTagType *prop_type;
// 			prop_type = RefSystem_ReferentFromString("TagType", tag);
// 			if (prop_type)
// 			{
// 				if( stricmp( prop_type->pcDictName, dict_name ) == 0 ) {
// 					eaPush( &types, prop_type );
// 				} else if( stricmp( prop_type->pcDictName, "Trap" ) == 0 && stricmp( dict_name, "ObjectLibrary" ) == 0 ) {
// 					eaPush( &types, prop_type );
// 				}
// 			}
// 		}
// 	}
// 	if (!found_ugc)
// 	{
// 		eaDestroyEx(&tag_list, NULL);
// 		eaDestroy(&types);
// 		estrDestroy(&estr_errors);
// 		return false;
// 	}
// 	for(i = 0; i < eaSize(&tag_list); i++)
// 	{
// 		char *tag = tag_list[i];
// 		bool found = false;
// 		if (stricmp(tag, "UGC") == 0)
// 		{
// 			continue;
// 		}
// 		else if (stricmp(tag, "genesischallenge") == 0)
// 		{
// 			continue;
// 		}
// 		FOR_EACH_IN_EARRAY(types, UGCAssetTagType, type)
// 		{
// 			if (stricmp(tag, type->pcName) == 0)
// 			{
// 				found = true;
// 				break;
// 			}
// 			FOR_EACH_IN_EARRAY(type->eaCategories, UGCAssetTagCategory, category)
// 			{
// 				char buf[128];
// 				sprintf(buf, "%s_", category->pcName);
// 				if (strStartsWith(tag, buf))
// 				{
// 					FOR_EACH_IN_EARRAY(category->eaTags, UGCAssetTag, cat_tag)
// 					{
// 						sprintf(buf, "%s_%s", category->pcName, cat_tag->pcName);
// 						if (stricmp(tag, buf) == 0)
// 						{
// 							found = true;
// 							break;
// 						}
// 					}
// 					FOR_EACH_END;
// 				}
// 				if (found)
// 					break;
// 			}
// 			FOR_EACH_END;
// 			if (found)
// 				break;
// 		}
// 		FOR_EACH_END;
// 		if (found)
// 			continue;

// 		if (estrLength(&estr_errors) > 0)
// 			estrAppend2(&estr_errors, ",");
// 		estrAppend2(&estr_errors, tag);
// 	}
// 	if (estrLength(&estr_errors) > 0)
// 	{
// 		estrCreate(&estr_types);
// 		FOR_EACH_IN_EARRAY(types, UGCAssetTagType, type)
// 		{
// 			if (estrLength(&estr_types) > 0)
// 				estrAppend2(&estr_types, ",");
// 			estrAppend2(&estr_types, type->pcName);
// 		}
// 		FOR_EACH_END;
// 		ErrorFilenamef(object_filename, "Object '%s' has improper tags '%s' for a UGC group of type '%s'.", object_name, estr_errors, estr_types);
// 		estrDestroy(&estr_types);
// 	}
// 	eaDestroyEx(&tag_list, NULL);
// 	eaDestroy(&types);
// 	estrDestroy(&estr_errors);
// 	return true;
// }


// JFinder - This validation is commented out because it can not
// be done here.  Tags have been moved into .ugcresinfo files, and
// they are not accessable in WorldLib.
// 
// void ugcValidateZoneMapTags(void)
// {
	
// 	Short term plan: Disable these errors.
	
// 	Long term plan: Move this validation up into CrossRoads.
	
// 	if (wlIsServer())
// 	{
// 		FOR_EACH_IN_REFDICT("ZoneMap", ZoneMapInfo, zminfo)
// 		{
// 			ResourceInfo* resInfo = ugcResourceGetInfo( "ZoneMap", zminfo->map_name );
// 			const char* tags = SAFE_MEMBER( resInfo, resourceTags );
			
// 			if (tags && ugcValidateTags(zminfo->map_name, zminfo->filename, tags, NULL, "ZoneMap"))
// 			{
// 				Vec3 spawn_pos;
// 				if (ugcHasTagType(tags, "ZoneMap", "PrefabGroundMap"))
// 				{
// 					ZoneMapEncounterRoomInfo *playable = ugcGetZoneMapPlayableVolume(zminfo->map_name);
// 					if (!playable)
// 						ErrorFilenamef(zminfo->filename, "Ground map has no playable volume!");
// 				}
// 				if (!ugcGetZoneMapSpawnPoint(zminfo->map_name, spawn_pos, NULL))
// 				{
// 					ErrorFilenamef(zminfo->filename, "Map has no UGC default spawn point!");
// 				}
// 				else
// 				{
// 					ZoneMapEncounterRegionInfo *region_info = ugcGetZoneMapDefaultRegion(zminfo->map_name);
// 					if (!region_info)
// 						ErrorFilenamef(zminfo->filename, "Map has no UGC default region!");
// 					else if (region_info->regionName != NULL)
// 						ErrorFilenamef(zminfo->filename, "Map UGC default region (%s) is not the Default region!", region_info->regionName);
// 				}
// 			}
// 		}
// 		FOR_EACH_END;
// 	}
// }

void ugcZoneMapRoomGetPlatforms(int room_id, int room_level, ExclusionVolumeGroup ***out_volumes)
{
	GroupDef *def = objectLibraryGetGroupDef(room_id, false);
	if (def)
	{
		exclusionGetDefVolumeGroups(def, out_volumes, false, room_level);
	}
}

HeightMapExcludeGrid *ugcMapEditorGenerateExclusionGrid(UGCMapPlatformData *platform_data)
{
	HeightMapExcludeGrid *grid = exclusionGridCreate(0, 0, 1, 1);
	FOR_EACH_IN_EARRAY(platform_data->platform_groups, UGCMapPlatformGroup, platform_group)
	{
		ExclusionObject *object = calloc(1, sizeof(ExclusionObject));
		identityMat4(object->mat);
		object->volume_group = calloc(1, sizeof(ExclusionVolumeGroup));
		object->max_radius = 1e8;
		object->volume_group_owned = true;
		copyMat4(platform_group->mat, object->volume_group->mat_offset);

		FOR_EACH_IN_EARRAY(platform_group->volumes, ExclusionVolume, volume_st) {
			eaPush(&object->volume_group->volumes, StructClone(parse_ExclusionVolume, volume_st));
		} FOR_EACH_END;

		exclusionGridAddObject(grid, object, 1e8, false);
	}
	FOR_EACH_END;
	return grid;
}

bool ugcGetZoneMapSpawnPoint(const char *map_name, Vec3 out_spawn_pos, Quat* pOut_spawn_orientation)
{
	ZoneMapEncounterObjectInfo *info = zeniObjectFind(map_name, "UGC_Start_Spawn");
	if (info)
	{
		copyVec3(info->pos, out_spawn_pos);
		if( pOut_spawn_orientation ) {
			copyQuat(info->qOrientation, *pOut_spawn_orientation);
		}
		return true;
	}
	else
	{
		zeroVec3( out_spawn_pos );
		if( pOut_spawn_orientation ) {
			unitQuat( *pOut_spawn_orientation );
		}
		return false;
	}
}

ZoneMapEncounterRegionInfo *ugcGetZoneMapDefaultRegion(const char *map_name)
{
	Vec3 spawn_pos = { 0, 0, 0 };
	ZoneMapEncounterInfo* zeniInfo = RefSystem_ReferentFromString( "ZoneMapEncounterInfo", map_name );

	if (!zeniInfo)
		return NULL;

	ugcGetZoneMapSpawnPoint(map_name, spawn_pos, NULL);
	FOR_EACH_IN_EARRAY(zeniInfo->regions, ZoneMapEncounterRegionInfo, region)
	{
		if (pointBoxCollision(spawn_pos, region->min, region->max))
			return region;
	}
	FOR_EACH_END;

	return NULL;
}

ZoneMapEncounterRoomInfo *ugcGetZoneMapPlayableVolume(const char *map_name)
{
	ZoneMapEncounterInfo* zeniInfo = RefSystem_ReferentFromString( "ZoneMapEncounterInfo", map_name );

	if (!zeniInfo)
		return NULL;

	if (eaSize(&zeniInfo->playable_volumes) != 1)
		return NULL;

	return zeniInfo->playable_volumes[0];
}

SecondaryZoneMap **ugcGetZoneMapSecondaryMaps(const char *map_name)
{
	ZoneMapEncounterInfo* zeniInfo = RefSystem_ReferentFromString("ZoneMapEncounterInfo", map_name);

	if(!zeniInfo)
		return NULL;

	return zeniInfo->secondary_maps;
}

#define INTERIOR_LAYOUT_SIZE 1200
#define INTERIOR_LAYOUT_HEIGHT 600
#define SPACE_LAYOUT_SIZE 8000
#define SPACE_LAYOUT_HEIGHT 5000

/// Get the valid bounds for COMPONENT, assuming it would get placed on MAP.
///
/// See also: ugcGetZoneMapPlaceableBounds (a lower level version of this)
void ugcMapComponentValidBounds( Vec3 out_min, Vec3 out_max, const UGCProjectData* ugcProj, UGCBacklinkTable* pBacklinkTable, const UGCMap* map, const UGCComponent* component )
{
	
	if( map->pPrefab ) {
		bool hasMissionLink;
		bool hasTriggerLink;

		if( component ) {
			const UGCMissionObjective* objective = ugcObjectiveFindComponentRelatedUsingTableConst( ugcProj, pBacklinkTable, component->uID );
			const UGCComponent** triggerList = NULL;
			ugcBacklinkTableFindAllTriggersConst( ugcProj, pBacklinkTable, component->uID, 0, &triggerList );

			hasMissionLink = (objective != NULL);
			hasTriggerLink = (eaSize( &triggerList ) != 0);
			
			eaDestroy( &triggerList );
		} else {
			hasMissionLink = false;
			hasTriggerLink = false;
		}
		
		if(   (!component || component->eType == UGC_COMPONENT_TYPE_OBJECT
			   || component->eType == UGC_COMPONENT_TYPE_BUILDING_DEPRECATED
			   || component->eType == UGC_COMPONENT_TYPE_CLUSTER
			   || component->eType == UGC_COMPONENT_TYPE_CLUSTER_PART)
			  && !hasMissionLink && !hasTriggerLink ) {
			ugcGetZoneMapPlaceableBounds( out_min, out_max, map->pPrefab->map_name, false );
		} else {
			ugcGetZoneMapPlaceableBounds( out_min, out_max, map->pPrefab->map_name, true );
		}
	} else {
		switch( ugcMapGetType( map )) {
			xcase UGC_MAP_TYPE_INTERIOR:
				setVec3( out_min, -INTERIOR_LAYOUT_SIZE / 2, -INTERIOR_LAYOUT_HEIGHT / 2, -INTERIOR_LAYOUT_SIZE / 2 );
				setVec3( out_max, +INTERIOR_LAYOUT_SIZE / 2, +INTERIOR_LAYOUT_HEIGHT / 2, +INTERIOR_LAYOUT_SIZE / 2 );

			xcase UGC_MAP_TYPE_SPACE:
				setVec3( out_min, -SPACE_LAYOUT_SIZE / 2, -SPACE_LAYOUT_HEIGHT / 2, -SPACE_LAYOUT_SIZE / 2 );
				setVec3( out_max, +SPACE_LAYOUT_SIZE / 2, +SPACE_LAYOUT_HEIGHT / 2, +SPACE_LAYOUT_SIZE / 2 );

			xdefault:
				setVec3( out_min, 0, 0, 0 );
				setVec3( out_max, 0, 0, 0 );
		}
	}
}


/// Get the effective bounds of the map named MAP-NAME.
///
/// RESTRICT-TO-PLAYABLE should be set for things that the player must
/// be able to reach, like anything required by the mission.
///
/// See also: ugcMapComponentValidBounds (a higher-level version of this)
void ugcGetZoneMapPlaceableBounds( Vec3 out_min, Vec3 out_max, const char* map_name, bool restrictToPlayable )
{
	ZoneMapEncounterRegionInfo* defaultRegion = ugcGetZoneMapDefaultRegion( map_name );
	ZoneMapEncounterRoomInfo* playableVolume = NULL;

	if( !defaultRegion ) {
		zeroVec3( out_min );
		zeroVec3( out_max );
	} else if( eaSize( &defaultRegion->rooms ) == 1 && !restrictToPlayable ) {
		copyVec3( defaultRegion->rooms[ 0 ]->min, out_min );
		copyVec3( defaultRegion->rooms[ 0 ]->max, out_max );
	} else if( playableVolume = ugcGetZoneMapPlayableVolume( map_name )) {
		copyVec3( playableVolume->min, out_min );
		copyVec3( playableVolume->max, out_max );
	} else {
		copyVec3( defaultRegion->min, out_min );
		copyVec3( defaultRegion->max, out_max );
	}
}

//// Interior layout room door names

const char *ugcIntLayoutGetRoomName(U32 id)
{
	static char name_buf[24];
	if (id == GENESIS_UNPLACED_ID)
		sprintf(name_buf, "__UNPLACED__");
	else if (id == UGC_TOPLEVEL_ROOM_ID)
		sprintf(name_buf, "__SPACE__");
	else
		sprintf(name_buf, "ROOM_%d", id);
	return name_buf;
}

const char *ugcIntLayoutDoorGetClickyLogicalName(const char *room_name, const char *door_name, const char *layout_name)
{
	static char name_buf[RESOURCE_NAME_MAX_SIZE];
	sprintf(name_buf, "DoorCap_%s_%s_%s", door_name, room_name, layout_name);
	return name_buf;
}

const char *ugcIntLayoutDoorGetSpawnLogicalName(const char *room_name, const char *door_name, const char *layout_name)
{
	static char name_buf[RESOURCE_NAME_MAX_SIZE];
	sprintf(name_buf, "DoorCap_%s_%s_%s_SPAWN", door_name, room_name, layout_name);
	return name_buf;
}

void ugcPlatformDictionaryLoad(void)
{
	static bool bPlatformDictionaryLoaded = false;
	if (!bPlatformDictionaryLoaded)
	{
		RefSystem_ClearDictionary(g_UGCPlatformInfoDict, true);
		resLoadResourcesFromDisk(g_UGCPlatformInfoDict, "tempbin/maps", "ugc.platforms", "UGCPlatforms.bin", PARSER_BINS_ARE_SHARED);
		bPlatformDictionaryLoaded = true;
	}
}

void ugcProjectDataNameSpaceChange(UGCProjectData *pUGCProjectData, const char *new_namespace)
{
	char buf[RESOURCE_NAME_MAX_SIZE];

	StructCopyString(&pUGCProjectData->ns_name, new_namespace);

	// Rename project
	sprintf(buf, "%s:%s", new_namespace, new_namespace);
	pUGCProjectData->project->pcName = allocAddString(buf);
	sprintf(buf, "ns/%s/UGC/%s.project", new_namespace, new_namespace);
	pUGCProjectData->project->pcFilename = allocAddString(buf);

	// Rename other assets
	ugcEditorImportProjectSwitchNamespace(&pUGCProjectData->mission->name, new_namespace);
	ugcEditorImportProjectSwitchNamespace(&pUGCProjectData->components->pcName, new_namespace);

	FOR_EACH_IN_EARRAY(pUGCProjectData->maps, UGCMap, map)
	{
		ugcEditorImportProjectSwitchNamespace(&map->pcName, new_namespace);
	}
	FOR_EACH_END;

	FOR_EACH_IN_EARRAY(pUGCProjectData->costumes, UGCCostume, costume)
	{
		ugcEditorImportProjectSwitchNamespace(&costume->astrName, new_namespace);
	}
	FOR_EACH_END;
}

bool ugcPowerPropertiesIsUsedInUGC( WorldPowerVolumeProperties* props )
{
	const char* powerName;

	if( !props ) {
		return false;
	}

	powerName = REF_STRING_FROM_HANDLE( props->power );
	return (stricmp( powerName, "System_Volume_Kill" ) == 0
			|| stricmp( powerName, "Environmental_Lava" ) == 0
			|| stricmp( powerName, "Environmental_Fire" ) == 0
			|| stricmp( powerName, "Environmental_SpellplagueFire" ) == 0
			|| stricmp( powerName, "Environmental_SpellplagueRift") == 0
			|| stricmp( powerName, "System_Volume_Drawning" ) == 0);
}

void UGCTagFillAllKeysAndValues(const char ***keys, S32 **values)
{
	UGCTag *eaiUGCTags = ugcGetDefaults()->eaiUGCTags;
	const char **eaKeys = NULL;
	S32 *eaiValues;

	DefineFillAllKeysAndValues(UGCTagEnum, keys, values);

	if(eaiSize(&eaiUGCTags))
	{
		eaiValues = *values;
		if(keys) eaKeys = *keys;
		FOR_EACH_IN_EARRAY_INT(eaiValues, S32, value)
		{
			if(eaiFind(&eaiUGCTags, value) < 0)
			{
				eaiRemove(&eaiValues, FOR_EACH_IDX(eaiValues, value));
				if(eaKeys)
					eaRemove(&eaKeys, FOR_EACH_IDX(eaiValues, value));
			}
		}
		FOR_EACH_END;
	}
}

#include "NNOUGCCommon_h_ast.c"
#include "NNOUGCCommon_c_ast.c"
