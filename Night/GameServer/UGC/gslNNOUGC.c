#include "gslNNOUGC.h"

#include "../AppServerLib/pub/aslResourceDBPub.h"
#include "Character.h"
#include "CostumeCommon.h"
#include "CostumeCommonLoad.h"
#include "Entity.h"
#include "EntityLib.h"
#include "Expression.h"
#include "GameAccountDataCommon.h"
#include "GameServerLib.h"
#include "GameStringFormat.h"
#include "GlobalTypes.h"
#include "JobManagerSupport.h"
#include "LoggedTransactions.h"
#include "NNOGameServer.h"
#include "NNOUGCCommon.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCResource.h"
#include "NotifyCommon.h"
#include "Player.h"
#include "ResourceDBSupport.h"
#include "ResourceDBUtils.h"
#include "ResourceInfo.h"
#include "ResourceManager.h"
#include "StaticWorld/WorldGridLoadPrivate.h"
#include "StaticWorld/WorldGridPrivate.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "UGCError.h"
#include "UGCProjectCommon.h"
#include "UGCProjectUtils.h"
#include "WorldGrid.h"
#include "contact_common.h"
#include "encounter_common.h"
#include "file.h"
#include "gslContact.h"
#include "gslEncounter.h"
#include "gslEntity.h"
#include "gslInteractable.h"
#include "gslMapTransfer.h"
#include "gslMapVariable.h"
#include "gslMechanics.h"
#include "gslMission.h"
#include "gslMission_transact.h"
#include "gslNNOUGCGenesisMissions.h"
#include "gslNamedPoint.h"
#include "gslOpenMission.h"
#include "gslPatrolRoute.h"
#include "gslSavedPet.h"
#include "gslSpawnPoint.h"
#include "gslUGC.h"
#include "gslUGCTransactions.h"
#include "logging.h"
#include "mapstate_common.h"
#include "mission_common.h"
#include "sysutil.h"
#include "trivia.h"
#include "windefinclude.h"
#include "wlUGC.h"

#include "Allegiance.h"
#include "crypt.h"

#include "AutoGen/UGCProjectCommon_h_ast.h"
#include "AutoGen/CostumeCommon_h_ast.h"
#include "AutoGen/GameClientLib_autogen_ClientCmdWrappers.h"
#include "Autogen/MapDescription_h_ast.h"

#include "autogen/GameServerLib_autotransactions_autogen_wrappers.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_GameSystems););

/// Structure used by DoPlayDialogTree for callbacks
typedef struct PlayDialogTreeData {
	UGCProjectData* ugcProj;
	EntityRef entRef;
	U32 dialogTreeID;
	int promptID;
} PlayDialogTreeData;

typedef struct MissionCompleteStep
{
	bool isOpenMission;
	char* objectiveName;
} MissionCompleteStep;

void missionCompleteStepDestroy( MissionCompleteStep* step )
{
	StructFreeStringSafe( &step->objectiveName );
	free( step );
}

typedef bool (*ObjectiveFn)(UGCMissionObjective* obj, UserData userData);

static bool ugcMissionPushNeedsToComplete( MissionCompleteStep*** out_steps, UGCMissionObjective** rootObjectives, UGCMissionObjective** objectives, bool foundObj,
										   ObjectiveFn fn, UserData data, bool isOpenMission )
{
	int it;
	for( it = eaSize( &objectives ) - 1; it >= 0; --it ) {
		UGCMissionObjective* objective = objectives[ it ];

		if( foundObj ) {
			MissionCompleteStep* step = calloc( 1, sizeof( *step ));

			step->isOpenMission = isOpenMission;
			step->objectiveName = StructAllocString( ugcMissionObjectiveLogicalNameTemp( objective ));
			eaInsert( out_steps, step, 0 );
		}
		foundObj = ugcMissionPushNeedsToComplete( out_steps, rootObjectives, objective->eaChildren, foundObj, fn, data, isOpenMission );

		if( fn( objective, data )) {
			foundObj = true;
		}
	}

	return foundObj;
}

static MissionCompleteStep** ugcMissionNeedsToComplete( UGCProjectData* proj, U32 objectiveID )
{
	MissionCompleteStep** accum = NULL;
	UGCMissionObjective* objective = ugcObjectiveFind( proj->mission->objectives, objectiveID );

	assert( objectiveID );

	if( !objective ) {
		return accum;
	} else {
		const char* objectiveMapName = ugcObjectiveInternalMapName( proj, objective );

		if( objectiveMapName ) {
			// advance to that map
			{
				UGCMissionObjective** personalObjectives = NULL;
				ugcMissionTransmogrifyObjectives( proj, proj->mission->objectives, NULL, false, &personalObjectives );
				ugcMissionPushNeedsToComplete( &accum, personalObjectives, personalObjectives, false,
											   ugcObjectiveIsCompleteMapOnMap, (UserData)objectiveMapName, false );
				eaDestroyStruct( &personalObjectives, parse_UGCMissionObjective );
			}

			// advance to that specific objective
			{
				UGCMissionObjective** openObjectives = NULL;
				ugcMissionTransmogrifyObjectives( proj, proj->mission->objectives, objectiveMapName, false, &openObjectives );
				ugcMissionPushNeedsToComplete( &accum, openObjectives, openObjectives, false,
											   ugcObjectiveHasIDRaw, (UserData)(uintptr_t)objectiveID, true );
				eaDestroyStruct( &openObjectives, parse_UGCMissionObjective );
			}
		} else {
			// advance to that specific objective
			{
				UGCMissionObjective** personalObjectives = NULL;
				ugcMissionTransmogrifyObjectives( proj, proj->mission->objectives, NULL, false, &personalObjectives );
				ugcMissionPushNeedsToComplete( &accum, personalObjectives, personalObjectives, false,
											   ugcObjectiveHasIDRaw, (UserData)(uintptr_t)objectiveID, false );
				eaDestroyStruct( &personalObjectives, parse_UGCMissionObjective );
			}
		}
	}

	return accum;
}

typedef struct MissionStartObjectiveData
{
	ContainerID entContainerID;
	const char* openMissionName;			// AST( POOLED_STRING )
	const char* missionName;				// AST( POOLED_STRING )
	MissionCompleteStep** completeSteps;

	MissionStartObjectiveCB fn;
	UserData data;
} MissionStartObjectiveData;

static void ugcMissionStartObjectiveFinishCB( TransactionReturnVal* retVal, UserData rawData )
{
	MissionStartObjectiveData* data = (MissionStartObjectiveData*)rawData;
	MissionCompleteStep** completeSteps = data->completeSteps;

	Entity *playerEnt = entFromContainerIDAnyPartition( GLOBALTYPE_ENTITYPLAYER, data->entContainerID );
	if(playerEnt)
	{
		MissionInfo* info = (playerEnt ? mission_GetInfoFromPlayer( playerEnt ) : NULL);

		OpenMission* openMission = openmission_GetFromName(entGetPartitionIdx(playerEnt),data->openMissionName );
		Mission* mission = mission_GetMissionByName( info, data->missionName );

		if( mission && info && retVal && retVal->eOutcome == TRANSACTION_OUTCOME_SUCCESS ) {
			int it;

			// complete up to the right objective
			for( it = 0; it != eaSize( &completeSteps ); ++it ) {
				MissionCompleteStep* completeStep = completeSteps[ it ];

				if( completeStep->isOpenMission ) {
					if( openMission ) {
						char afterPromptObjectiveName[ 256 ];
						Mission* phase;

						phase = mission_FindChildByName( openMission->pMission, completeStep->objectiveName );
						if( phase ) {
							mission_CompleteMission( playerEnt, phase, true, NULL );
						}

						sprintf( afterPromptObjectiveName, "AfterPrompt_%s", completeStep->objectiveName );
						phase = mission_FindChildByName( openMission->pMission, afterPromptObjectiveName );
						if( phase ) {
							mission_CompleteMission( playerEnt, phase, true, NULL );
						}
					}
				} else {
					char afterPromptObjectiveName[ 256 ];

					Mission* phase;

					phase = mission_FindChildByName( mission, completeStep->objectiveName );
					if( phase ) {
						mission_CompleteMission( playerEnt, phase, true, NULL );
					}

					sprintf( afterPromptObjectiveName, "AfterPrompt_%s", completeStep->objectiveName );
					phase = mission_FindChildByName( mission, afterPromptObjectiveName );
					if( phase ) {
						mission_CompleteMission( playerEnt, phase, true, NULL );
					}
				}
			}
			data->fn( true, data->data );
		} else {
			if( retVal && retVal->eOutcome == TRANSACTION_OUTCOME_SUCCESS ) {
				AssertOrAlertWarning( "UGC_MISSION_ADD_TRANS", "Mission %s expected to be on the player, but is not there.  Entity: %d, MissionInfo: %d, OpenMission %s: %d",
									  data->missionName, !!playerEnt, !!info, data->openMissionName, !!openMission );
			}

			data->fn( false, data->data );
		}
	}

	eaDestroyEx( &completeSteps, missionCompleteStepDestroy );
	free( data );
}

void ugcMissionStartObjective( Entity* playerEnt, UGCProjectData* proj, const char* map_name, U32 objectiveID, MissionStartObjectiveCB fn, UserData userData )
{
	char missionName[ RESOURCE_NAME_MAX_SIZE ];
	char openMissionName[ RESOURCE_NAME_MAX_SIZE ];
	MissionDef* missionDef = NULL;
	MissionInfo* info;
	bool mapIsCryptic;

	if( !proj || !objectiveID ) {
		// no objective to start
		fn( true, userData );
		return;
	}

	mapIsCryptic = strStartsWith( map_name, proj->ns_name );

	sprintf( missionName, "%s:Mission", proj->ns_name );
	sprintf( openMissionName, "%s_Mission_%s_Openmission", map_name, g_UGCMissionName );
	missionDef = RefSystem_ReferentFromString( g_MissionDictionary, missionName );
	if( !playerEnt || !missionDef ) {
		fn( false, userData );
		return;
	}

	info = mission_GetInfoFromPlayer( playerEnt );
	if( info ) {
		MissionStartObjectiveData* cbData = calloc( 1, sizeof( *cbData ));
		Mission* mission = mission_GetMissionByName( info, missionName );

		cbData->entContainerID = entGetContainerID( playerEnt );
		cbData->openMissionName = allocAddString( openMissionName );
		cbData->missionName = allocAddString( missionName );
		cbData->completeSteps = ugcMissionNeedsToComplete( proj, objectiveID );
		cbData->fn = fn;
		cbData->data = userData;

		if( mission ) {
			missioninfo_DropMission( playerEnt, info, mission );
		}
		missioninfo_AddMissionByName(entGetPartitionIdx(playerEnt), info, missionName, ugcMissionStartObjectiveFinishCB, cbData );
	} else {
		fn( false, userData );
	}
}

static bool ugcDialogTreeStartBlock( Entity* playerEnt, UGCProjectData* proj, U32 componentID, int promptID )
{
	UGCComponent* component = ugcComponentFindByID( proj->components, componentID );

	if( component ) {
		char contactName[ RESOURCE_NAME_MAX_SIZE ];
		char promptDialogName[ 256 ];
		ContactDef* contactDef;

		sprintf( contactName, "%s:Mission", proj->ns_name );
		if( promptID >= 0 ) {
			sprintf( promptDialogName, "Prompt_%d_%d", componentID, promptID );
		} else {
			sprintf( promptDialogName, "Prompt_%d", componentID );
		}

		contactDef = contact_DefFromName( contactName );
		if( contactDef ) {
			contact_InteractBegin( playerEnt, NULL, contactDef, promptDialogName, NULL );
			return true;
		}
	}

	return false;
}

/// Fill out implicit information based on what is given
void gslUGCPlayPreprocess( UGCProjectData* ugc_proj, const char **map_name, U32* objective_id, const char **spawn_name )
{
	if( nullStr( *map_name ) && !*objective_id ) {
		return;
	}
	FixupStructLeafFirst( parse_UGCProjectData, ugc_proj, FIXUPTYPE_POST_TEXT_READ, NULL );

	if( nullStr( *map_name )) {
		UGCMissionObjective* objective = ugcObjectiveFind( ugc_proj->mission->objectives, *objective_id );
		UGCComponent* component = NULL;

		// decend down all compound objectives
		while( objective && objective->type == UGCOBJ_ALL_OF || objective->type == UGCOBJ_IN_ORDER ) {
			objective = eaGet( &objective->eaChildren, 0 );
		}

		if( objective ) {
			component = ugcComponentFindByID( ugc_proj->components, objective->componentID );
		}
		if( component && component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
			component = ugcComponentFindByID( ugc_proj->components, component->uActorID );
		}

		if( component ) {
			if( ugcComponentIsOnMap( component, NULL, false )) {
				*map_name = component->sPlacement.pcExternalMapName;
			} else if( !nullStr( component->sPlacement.pcMapName ) && component->sPlacement.uRoomID != GENESIS_UNPLACED_ID ) {
				char mapNameBuffer[ RESOURCE_NAME_MAX_SIZE ];
				sprintf( mapNameBuffer, "%s:%s", ugc_proj->ns_name, component->sPlacement.pcMapName );
				*map_name = allocAddString( mapNameBuffer );
			} else {
				// couldn't do it!
				return;
			}
		}
	}
	if( !*objective_id ) {
		char mapNameSansNS[ RESOURCE_NAME_MAX_SIZE ];
		UGCMissionObjective* objective;

		if( resExtractNameSpace_s( *map_name, NULL, 0, SAFESTR( mapNameSansNS ))) {
			objective = ugcObjectiveFindOnMap( ugc_proj->mission->objectives, ugc_proj->components, mapNameSansNS );
		} else {
			objective = ugcObjectiveFindOnCrypticMap( ugc_proj->mission->objectives, ugc_proj->components, mapNameSansNS );
		}

		if( objective ) {
			*objective_id = objective->id;
		}
	}
	if ( spawn_name && nullStr(*spawn_name) && resNamespaceIsUGC(*map_name))
	{
		// Get start spawn
		UGCComponent *spawn_component = NULL;
		UGCMissionMapLink *link = ugcMissionFindLinkByObjectiveID(ugc_proj, *objective_id, true);
		if (link)
			spawn_component = ugcComponentFindByID(ugc_proj->components, link->uSpawnComponentID);
		if (spawn_component)
		{
			char buf[256];
			sprintf(buf, "UGC_%s", ugcComponentGetLogicalNameTemp(spawn_component));
			*spawn_name = allocAddString(buf);
		}
	}
}

static void gslUGC_DoPlayDialogTreeAfterResdictReload( TimedCallback* cb, F32 time, UserData rawData )
{
	PlayDialogTreeData* data = rawData;
	Entity *pEntity = entFromEntityRefAnyPartition( data->entRef );

	if(   ugcDefaultsDialogStyle() == UGC_DIALOG_STYLE_WINDOW
		  && !ugcDialogTreeStartBlock( pEntity, data->ugcProj, data->dialogTreeID, data->promptID )) {
		gslUGC_DoPlayCB( false, (UserData)(intptr_t)data->entRef );
	} else {
		gslUGC_DoPlayCB( true, (UserData)(intptr_t)data->entRef );
	}

	StructDestroy( parse_UGCProjectData, data->ugcProj );
	free( data );
}

void gslUGC_DoPlayDialogTree( Entity* pEntity, UGCProjectData* ugc_proj, U32 dialog_tree_id, int prompt_id )
{
	char previewMapName[ RESOURCE_NAME_MAX_SIZE ];
	UGCComponent* dialogComponent;
	UGCComponent* actorComponent = NULL;
	UGCProjectData dialog_only_proj = { 0 };
	const char* prefabMapName;

	log_printf( LOG_UGC, "%s: ns=%s, dialog_tree_id=%d, prompt_id=%d",
				__FUNCTION__, ugc_proj->ns_name, dialog_tree_id, prompt_id );

	FixupStructLeafFirst( parse_UGCProjectData, ugc_proj, FIXUPTYPE_POST_TEXT_READ, NULL );
	dialogComponent = ugcComponentFindByID( ugc_proj->components, dialog_tree_id );

	if(   actorComponent && actorComponent->eMapType != UGC_MAP_TYPE_ANY
		  && !ugcMapTypeIsGround( actorComponent->eMapType )) {
		prefabMapName = "Ugc_Editing_SpaceMap";
	} else {
		prefabMapName = "Ugc_Editing_Map";
	}

	TellControllerWeMayBeStallyForNSeconds(giUGCEnterPreviewStallySeconds, "EnterPreview");
	if( !dialogComponent ) {
		UGCPlayResult res = { UGC_PLAY_NO_DIALOG_TREE };
		ClientCmd_gclUGCProcessPlayResult(pEntity, &res);
		return;
	}
	actorComponent = ugcComponentFindByID( ugc_proj->components, dialogComponent->uActorID );
	if( dialogComponent->uActorID && !actorComponent ) {
		UGCPlayResult res = { UGC_PLAY_NO_DIALOG_TREE };
		ClientCmd_gclUGCProcessPlayResult( pEntity, &res );
		return;
	}

	if (ugc_proj != gGSLState.pLastPlayData)
	{
		StructDestroySafe(parse_UGCProjectData, &gGSLState.pLastPlayData);
		gGSLState.pLastPlayData = StructClone(parse_UGCProjectData, ugc_proj);
	}

	sprintf( previewMapName, "%s:DialogPreview", ugc_proj->ns_name );

	// To decrease generation time create a minimal project:
	dialog_only_proj.ns_name = StructAllocString( ugc_proj->ns_name );
	dialog_only_proj.project = StructClone( parse_UGCProjectInfo, ugc_proj->project );
	dialog_only_proj.mission = StructCreate( parse_UGCMission );
	dialog_only_proj.mission->name = ugc_proj->mission->name;
	dialog_only_proj.components = StructCreate( parse_UGCComponentList );
	dialog_only_proj.components->pcName = ugc_proj->components->pcName;
	dialog_only_proj.components->stComponentsById = stashTableCreateInt( 50 );
	eaIndexedEnable( &dialog_only_proj.components->eaComponents, parse_UGCComponent );

	// Use all costumes, since they may be referenced by the dialog
	eaCopyStructs( &ugc_proj->costumes, &dialog_only_proj.costumes, parse_UGCCostume );

	// Single map to preview on
	{
		UGCMap* previewMap = StructCreate( parse_UGCMap );
		eaPush( &dialog_only_proj.maps, previewMap );

		previewMap->pcName = allocAddString( previewMapName );
		previewMap->pcDisplayName = StructAllocString( "Dialog Preview" );
		previewMap->pPrefab = StructCreate( parse_UGCGenesisPrefab );
		previewMap->pPrefab->map_name = allocAddString( prefabMapName );
	}

	// Generate only this dialog tree and an actor to say it.
	{
		UGCComponent* newDialogComponent = ugcComponentOpClone( &dialog_only_proj, dialogComponent );
		UGCComponent* newActorComponent = ugcComponentOpCreate( &dialog_only_proj, UGC_COMPONENT_TYPE_CONTACT, 0 );

		if( !newDialogComponent->pStartWhen ) {
			newDialogComponent->pStartWhen = StructCreate( parse_UGCWhen );
		}
		newDialogComponent->pStartWhen->eType = UGCWHEN_MISSION_START;
		newDialogComponent->uActorID = newActorComponent->uID;
		newDialogComponent->bIsDefault = true;

		newActorComponent->sPlacement.pcMapName = StructAllocString( "DialogPreview" );
		if( actorComponent ) {
			StructCopyString( &newActorComponent->pcVisibleName, actorComponent->pcVisibleName );
			StructCopyString( &newActorComponent->pcCostumeName, actorComponent->pcCostumeName );
		}
		{
			ZoneMapEncounterObjectInfo* spawnZeniInfo = zeniObjectFind( prefabMapName, "Ugc_Start_Spawn" );
			ZoneMapEncounterObjectInfo* zeniInfo = zeniObjectFind( prefabMapName, "Ugc_Contact_Preview" );
			assert( spawnZeniInfo && zeniInfo );
			copyVec3( zeniInfo->pos, newActorComponent->sPlacement.vPos );
			newActorComponent->sPlacement.vPos[ 1 ] -= spawnZeniInfo->pos[ 1 ];
			newActorComponent->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_ABSOLUTE;
			newActorComponent->sPlacement.uRoomID = 0;
		}
	}

	log_printf( LOG_UGC, "%s: Minimal project generated, ns=%s",
				__FUNCTION__, ugc_proj->ns_name );

	if( !gslUGC_ProjectBudgetAllowsGenerate( ugc_proj )) {
		UGCPlayResult res = { UGC_PLAY_BUDGET_ERROR };
		ClientCmd_gclUGCProcessPlayResult( pEntity, &res );
		log_printf( LOG_UGC, "%s: Budget failed, ns=%s", __FUNCTION__, ugcProjectDataGetNamespace( ugc_proj ));
		TellControllerToLog( STACK_SPRINTF( __FUNCTION__ ": Budget failed, ns=%s", ugcProjectDataGetNamespace( ugc_proj )));
		return;
	}

	// Generate:
	TellControllerToLog( STACK_SPRINTF( __FUNCTION__ ": About to generate, ns=%s", ugc_proj->ns_name ));
	ugcProjectGenerateOnServer(&dialog_only_proj);

	log_printf( LOG_UGC, "%s: After ugcProjectGenerateOnServer()", __FUNCTION__ );
	TellControllerToLog( STACK_SPRINTF( __FUNCTION__ ": Generate done, ns=%s", ugc_proj->ns_name ));

	gslUGC_TransferToMapWithDelay(pEntity, previewMapName, NULL, NULL, NULL, NULL, 0);

	// set variable overrides before we load the map so the variables get inited correctly
	eaClearStruct( &g_eaMapVariableOverrides, parse_WorldVariable );
	if( resNamespaceIsUGC( previewMapName )) {
		WorldVariable* missionNumVar = StructCreate( parse_WorldVariable );
		WorldVariable* baseLevelVar = StructCreate( parse_WorldVariable );

		missionNumVar->pcName = allocAddString( "Mission_Num" );
		missionNumVar->eType = WVAR_INT;
		missionNumVar->iIntVal = 1;
		eaPush( &g_eaMapVariableOverrides, missionNumVar );

		baseLevelVar->pcName = allocAddString( "BaseLevel" );
		baseLevelVar->eType = WVAR_INT;
		baseLevelVar->iIntVal = encounter_getTeamLevelInRange( pEntity, NULL, false );
		eaPush( &g_eaMapVariableOverrides, baseLevelVar );
	}

	{
		PlayDialogTreeData* data = calloc( 1, sizeof( *data ));
		data->ugcProj = StructClone( parse_UGCProjectData, &dialog_only_proj );
		data->entRef = entGetRef( pEntity );
		data->dialogTreeID = dialog_tree_id;
		data->promptID = prompt_id;

		TimedCallback_Run( gslUGC_DoPlayDialogTreeAfterResdictReload, data, 0.5 );
	}
	gGSLState.bCurrentlyInUGCPreviewMode = true;
	StructReset( parse_UGCProjectData, &dialog_only_proj );
}

UGCProjectData *gslUGC_LoadProjectDataWithInfo(const char *namespace, UGCProjectInfo *pProjectInfo)
{
	char buf[RESOURCE_NAME_MAX_SIZE];
	UGCProjectData *pProjectData = NULL;

	pProjectData = UGC_LoadProjectData(namespace, "");
	if (pProjectData==NULL)
	{
		// Couldn't load?  We still need to create an empty project.
		// We must tolerate this as it may happen if the UGC editor was left without saving a new project.
		pProjectData = StructCreate(parse_UGCProjectData);
	}

	// Get rid of the loaded project info as it may be out-of-date.
 	StructDestroy(parse_UGCProjectInfo, pProjectData->project);
	pProjectData->project = pProjectInfo;

	// Clean up namespace changes on components. This NEEDS to be much more extensive.
	//  Currently we are getting bugs when importing projects possibly due to the insufficiency of this code.
	if (!pProjectData->components)
	{
		pProjectData->components = StructCreate(parse_UGCComponentList);
		sprintf(buf, "%s:Default", namespace);
		pProjectData->components->pcName = allocAddString(buf);
	}
	if (!pProjectData->mission)
	{
		pProjectData->mission = StructCreate(parse_UGCMission);
		sprintf(buf, "%s:Mission", namespace);
		pProjectData->mission->name = allocAddString(buf);
	}
	StructCopyString(&pProjectData->ns_name, namespace);

	return pProjectData;
}

static void gslUGC_RenameResource(const char **objKey, const char *pPublishNameSpace)
{
	char objName[RESOURCE_NAME_MAX_SIZE];
	char objNameSpace[RESOURCE_NAME_MAX_SIZE];
	static char newName[RESOURCE_NAME_MAX_SIZE];

	resExtractNameSpace(*objKey, objNameSpace, objName);
	if (pPublishNameSpace)
		sprintf(newName, "%s:%s", pPublishNameSpace, objName);
	else
		strcpy(newName, objName);

	*objKey = allocAddString(newName);
}


void gslUGC_RenameProjectNamespace(UGCProjectData *pData, const char *strNamespace)
{
	StructCopyString(&pData->ns_name, strNamespace);
	gslUGC_RenameResource(&pData->project->pcName, strNamespace);
	gslUGC_RenameResource(&pData->mission->name, strNamespace);
	gslUGC_RenameResource(&pData->components->pcName, strNamespace);
	FOR_EACH_IN_EARRAY(pData->maps, UGCMap, map)
	{
		gslUGC_RenameResource(&map->pcName, strNamespace);
	}
	FOR_EACH_END;
	FOR_EACH_IN_EARRAY(pData->costumes, UGCCostume, costume)
	{
		gslUGC_RenameResource(&costume->astrName, strNamespace);
	}
	FOR_EACH_END;
}

static struct {
	FILE *error_file;
	int error_count;
	bool error_found;
	const char *project_name;
} g_UGCErrorInfo;

static void gslUGCValidateProjectsErrorCB(ErrorMessage *errMsg, void *userdata)
{
	if (!g_UGCErrorInfo.error_found)
		fprintf(g_UGCErrorInfo.error_file, "*** PROJECT %s HAS GENERATION ERRORS:\n", g_UGCErrorInfo.project_name);

	fprintf(g_UGCErrorInfo.error_file, "%s\n", errMsg->estrMsg);
	g_UGCErrorInfo.error_found = true;
	g_UGCErrorInfo.error_count++;
}

void gslUGCValidateProjects(const char *filename)
{
	int length, i, project_error_count = 0;
	char *file_data = fileAlloc(filename, &length);
	char **proj_list = NULL;
	FILE *fErrorOut;

	ugcResourceInfoPopulateDictionary();

	if (!file_data)
	{
		Alertf("Failed to read file.");
		return;
	}
	{
		char buf[MAX_PATH], outfilename[MAX_PATH];
		sprintf(buf, "UGC/UGC_Project_Errors.txt");
		fileLocateWrite(buf, outfilename);
		makeDirectoriesForFile(outfilename);
		fErrorOut = fopen(outfilename, "w");
	}
	if (!fErrorOut)
	{
		Alertf("Failed to open error file for writing.");
		fclose(fErrorOut);
		SAFE_FREE(file_data);
		return;
	}

	DivideString(file_data, "\t\n\r", &proj_list, DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS);
	for (i = 0; i < eaSize(&proj_list); i += 2)
	{
		char cmd[256];
		int numDialogsDeleted = 0, numCostumesReset = 0, numObjectivesReset = 0;
		UGCRuntimeStatus *validateRet = NULL;
		const char *localdir = fileLocalDataDir();
		char dirname[MAX_PATH];
		UGCProjectData *data;
		bool bFoundError = false;
		int fixupFlags = 0;

		sprintf( dirname, "%s/data/ns/%s/ugc", localdir, proj_list[i+1]);
		data = ugcProjectLoadFromDir(dirname);

		if (!data)
		{
			sprintf(cmd, "patchclient -skipdeleteme -server UGCBackupLosGatos -port 7257 -project holodeck_startrekugc -branch 0 -sync -root %s -prefix data/ns/%s", localdir, proj_list[i+1]);
			system(cmd);
			data = ugcProjectLoadFromDir(dirname);
		}

		if (!data)
			continue;

		ugcEditorFixupProjectData(data, &numDialogsDeleted, &numCostumesReset, &numObjectivesReset, &fixupFlags);
		ugcProjectFixupDeprecated(data, true);

		// Validate the project
		ugcSetIsRepublishing(true);
		validateRet = StructCreate(parse_UGCRuntimeStatus);
		ugcSetStageAndAdd(validateRet, "UGC Validate");
		ugcValidateProject(data);
		ugcClearStage();
		ugcSetIsRepublishing(false);

		if (ugcStatusHasErrors(validateRet, UGC_ERROR))
		{
			int error_count = 0;
			if (!bFoundError)
				fprintf(fErrorOut, "*** PROJECT %s HAS ERRORS:\n", proj_list[i+1]);
			bFoundError = true;
			FOR_EACH_IN_EARRAY(validateRet->stages, UGCRuntimeStage, stage)
			{
				if (eaSize(&stage->errors) > 0)
				{
					fprintf(fErrorOut, "## In stage %s:\n", stage->name);
					FOR_EACH_IN_EARRAY(stage->errors, UGCRuntimeError, error)
					{
						char *buf = NULL;
						estrCreate(&buf);
						ParserWriteText(&buf, parse_UGCRuntimeError, error, 0, 0, 0);
						fprintf(fErrorOut, "%s\n", buf);
						estrDestroy(&buf);
						error_count++;
					}
					FOR_EACH_END;
				}
			}
			FOR_EACH_END;
			fprintf(fErrorOut, "\n\n");
			Errorf("Project %s has %d validation errors", proj_list[i+1], error_count);
		}
		else
		{
			g_UGCErrorInfo.error_file = fErrorOut;
			g_UGCErrorInfo.error_count = 0;
			g_UGCErrorInfo.error_found = false;
			g_UGCErrorInfo.project_name = proj_list[i+1];
			ErrorfPushCallback(gslUGCValidateProjectsErrorCB, NULL);
			ugcProjectGenerateOnServer(data);
			ErrorfPopCallback();

			if (g_UGCErrorInfo.error_count > 0)
			{
				Errorf("Project %s has %d generation errors", proj_list[i+1], g_UGCErrorInfo.error_count);
			}
		}
		StructDestroy(parse_UGCRuntimeStatus, validateRet);

		StructDestroy(parse_UGCProjectData, data);

		if (bFoundError)
			project_error_count++;

		printf("UGC: *** Finished processing namespace %s (%d/%d)\n", proj_list[i+1], i/2+1, eaSize(&proj_list)/2);
	}

	fprintf(fErrorOut, "UGC: *** %d PROJECTS WITH ERRORS.\n", project_error_count);

	fclose(fErrorOut);
	SAFE_FREE(file_data);
	eaDestroyEx(&proj_list, NULL);

	printf("UGC: Done.");
}

/// Step by step processes to go from UGCComponent -> Game-level struct
///
/// Inspired by this easy step by step process to draw an owl:
///
///  1. Draw some cicles
///  2. Draw the rest of the fucking owl
///
///                               `,                 
///                               :,       ,         
///                               :`.`   `.:         
///                              .,.:....,:          
///                              ,:;:,,,,,.          
///       .`.``                 ..;.:,:::;.          
///      ..    .                . .:';::.;.          
///      .      .               . :o.:.`o,:          
///      `       .              .,,:.,.:::.          
///     `        ,              ,.  .,:;.`:          
///       ````.  ,             ,:;::;: ,`.::         
///      :       `             ;,.:,. '::,:;`        
///     ` .     :..            :.:`  `.::,,;,        
///    .   ..`,`  `.           ,```  `:,,::;:        
///    `                       ,`..` `...,,,;        
///    `            `          ...` ``..`,.:;:       
///                  `          `. `. `.`..,;:       
///    .             `        `` ........`.:,:       
///    .             .        ```.`.```.....,;       
///    ``                     .` ```.` . ,`,.;       
///     `             `       .```.`   ` .`.`,       
///      `            `       `.`.```` . ``.,.       
///      ,           ``        `.``.`` `.` .,`       
///       `          `         `...`.` ````..`       
///       ``         `    :`   ``.`.`` .` ...        
///          `      .      `..:`.`.` `  ` `..        
///            `   ,       .,.,.: .`` ` `` `.        
///                         `,:`., ` `  .` `         
///                              ` `:;:,,:,..,       
///                                ;.,,,:,,:,``,,,. `
///                                `;.,...,: ,,``:``.
///                                 ```...`     .; ` 
///                                   `.,`;          
///                                   ... '`         
///                                    ::,'.         
///                                     ;;::         
///
static bool gslUGC_ComponentFindGameInteractable( UGCProjectData* ugcProj, UGCComponent* component, GameInteractable** out_ppInteractable )
{
	char logicalName[ 256 ] = "";
	GameInteractable* gi = NULL;
	*out_ppInteractable = NULL;

	// Reach through multiple layers of code to get to the WorldInteractionEntry in two easy steps!!

	// 1. Figure out the base logical name for the GroupDef that will
	// be generated.
	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_OBJECT:
			sprintf( logicalName, "Object_%d", component->uID );
		xcase UGC_COMPONENT_TYPE_CLUSTER_PART:
			sprintf( logicalName, "Cluster_Part_%d", component->uID );
		xcase UGC_COMPONENT_TYPE_TELEPORTER_PART:
			sprintf( logicalName, "Teleporter_Part_%d", component->uID );
		xcase UGC_COMPONENT_TYPE_REWARD_BOX:
			sprintf( logicalName, "Reward_Box_%d", component->uID );
		xcase UGC_COMPONENT_TYPE_COMBAT_JOB:
			sprintf( logicalName, "Combat_Job_%d", component->uID );
		xcase UGC_COMPONENT_TYPE_SOUND:
			sprintf( logicalName, "Sound_%d", component->uID );
		xcase UGC_COMPONENT_TYPE_RESPAWN:
			sprintf( logicalName, "Respawn_%d", component->uID );

		xcase UGC_COMPONENT_TYPE_TRAP: {
			// MJF May/15/2013 -- The trap is only in the "right"
			// place for self-contained traps.
			GroupDef* def = objectLibraryGetGroupDef( component->iObjectLibraryId, false );
			UGCTrapProperties *properties = def ? ugcTrapGetProperties( def ) : NULL;
			if( SAFE_MEMBER( properties, pSelfContained )) {
				sprintf( logicalName, "Trap_%d", component->uID );
			}
			StructDestroySafe( parse_UGCTrapProperties, &properties );
		}
		xcase UGC_COMPONENT_TYPE_TRAP_EMITTER: {
			// MJF May/15/2013 -- The emitter actually becomes the
			// full trap, for non-self contained.
			UGCComponent* trap = ugcComponentFindByID( ugcProj->components, component->uParentID );
			if( trap ) {
				sprintf( logicalName, "Trap_%d", trap->uID );
			}
		}
		xcase UGC_COMPONENT_TYPE_TRAP_TRIGGER: {
			UGCComponent* trap = ugcComponentFindByID( ugcProj->components, component->uParentID );
			if( trap ) {
				sprintf( logicalName, "Trap_%d_Armed", trap->uID );
			}
		}
	}
	if( nullStr( logicalName )) {
		return false;
	}

	// 2. Try each possible full logical name based on all the mission
	// names.
	if( !gi ) {
		char buffer[ 256 ];
		sprintf( buffer, "Ugc_%s", logicalName );
		gi = interactable_GetByName( buffer, NULL );
	}
	if( !gi ) {
		char buffer[ 256 ];
		sprintf( buffer, "Shared_%s", logicalName );
		gi = interactable_GetByName( buffer, NULL );
	}
	if( !gi ) {
		return false;
	}

	*out_ppInteractable = gi;
	return true;
}

static bool gslUGC_ComponentFindGameNamedPoint( UGCProjectData* ugcProj, UGCComponent* component, GameNamedPoint** out_ppPoint )
{
	char logicalName[ 256 ] = "";
	GameNamedPoint* gp = NULL;
	*out_ppPoint = NULL;

	// Reach through multiple layers of code to get to the WorldInteractionEntry in two easy steps!!

	// 1. Figure out the base logical name for the GroupDef that will
	// be generated.
	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_ROOM_MARKER:
			sprintf( logicalName, "Room_Marker_%d", component->uID );

		xcase UGC_COMPONENT_TYPE_TRAP_TARGET: {
			UGCComponent* trap = ugcComponentFindByID( ugcProj->components, component->uParentID );
			if( trap ) {
				sprintf( logicalName, "Trap_%d_Target_%d", trap->uID, component->iTrapEmitterIndex );
			}
		}
	}
	if( nullStr( logicalName )) {
		return false;
	}

	// 2. Try each possible full logical name based on all the mission
	// names.
	if( !gp ) {
		char buffer[ 256 ];
		sprintf( buffer, "Ugc_%s", logicalName );
		gp = namedpoint_GetByName( buffer, NULL );
	}
	if( !gp ) {
		char buffer[ 256 ];
		sprintf( buffer, "Shared_%s", logicalName );
		gp = namedpoint_GetByName( buffer, NULL );
	}
	if( !gp ) {
		return false;
	}

	*out_ppPoint = gp;
	return true;
}

static bool gslUGC_ComponentFindGameSpawnPoint( UGCProjectData* ugcProj, UGCComponent* component, GameSpawnPoint** out_ppPoint )
{
	char logicalName[ 256 ] = "";
	GameSpawnPoint* gsp = NULL;
	*out_ppPoint = NULL;

	// Reach through multiple layers of code to get to the WorldInteractionEntry in two easy steps!!

	// 1. Figure out the base logical name for the GroupDef that will
	// be generated.
	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_SPAWN:
			sprintf( logicalName, "Spawn_%d", component->uID );
	}
	if( nullStr( logicalName )) {
		return false;
	}

	// 2. Try each possible full logical name based on all the mission
	// names.
	if( !gsp ) {
		char buffer[ 256 ];
		sprintf( buffer, "Ugc_%s", logicalName );
		gsp = spawnpoint_GetByName( buffer, NULL );
	}
	if( !gsp ) {
		char buffer[ 256 ];
		sprintf( buffer, "Shared_%s", logicalName );
		gsp = spawnpoint_GetByName( buffer, NULL );
	}
	if( !gsp ) {
		return false;
	}

	*out_ppPoint = gsp;
	return true;
}

static bool gslUGC_ComponentFindGameEncounterAndActorIndex( UGCProjectData* ugcProj, UGCComponent* component, GameEncounter** out_ppEncounter, int* out_actorIndex )
{
	char logicalName[ 256 ] = "";
	GameEncounter* ge = NULL;
	int actorIndex = -1;
	*out_ppEncounter = NULL;
	*out_actorIndex = -1;

	// Reach through multiple layers of code to get to the WorldInteractionEntry in two easy steps!!

	// 1. Figure out the base logical name for the GroupDef that will
	// be generated.
	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_CONTACT:
			sprintf( logicalName, "Contact_%d", component->uID );
			actorIndex = 0;
		xcase UGC_COMPONENT_TYPE_ACTOR: {
			UGCComponent* kill = ugcComponentFindByID( ugcProj->components, component->uParentID );
			if( kill ) {
				sprintf( logicalName, "Kill_%d", kill->uID );
				actorIndex = eaiFind( &kill->uChildIDs, component->uID );
			}
		}
	}
	if( nullStr( logicalName ) || actorIndex < 0 ) {
		return false;
	}

	// 2. Try each possible full logical name based on all the mission
	// names.
	if( !ge ) {
		char buffer[ 256 ];
		sprintf( buffer, "Ugc_%s", logicalName );
		ge = encounter_GetByName( buffer, NULL );
	}
	if( !ge ) {
		char buffer[ 256 ];
		sprintf( buffer, "Shared_%s", logicalName );
		ge = encounter_GetByName( buffer, NULL );
	}
	if( !ge ) {
		return false;
	}

	*out_ppEncounter = ge;
	*out_actorIndex = actorIndex;
	return true;
}

static bool gslUGC_ComponentFindGamePatrolRouteAndPointIndex( UGCProjectData* ugcProj, UGCComponent* component, GamePatrolRoute** out_ppPatrol, int* out_pointIndex )
{
	char logicalName[ 256 ] = "";
	GamePatrolRoute* gpr = NULL;
	int pointIndex = -1;
	*out_ppPatrol = NULL;
	*out_pointIndex = -1;

	// Reach through multiple layers of code to get to the GamePatrolRoute in two easy steps!!

	// 1. Figure out the base logical name for the GroupDef that will
	// be generated.
	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_PATROL_POINT: {
			UGCComponent* kill = ugcComponentFindByID( ugcProj->components, component->uPatrolParentID );
			if( kill ) {
				sprintf( logicalName, "Kill_%d_Patrol", kill->uID );
				pointIndex = eaiFind( &kill->eaPatrolPoints, component->uID );
			}
		}
	}
	if( nullStr( logicalName ) || pointIndex < 0 ) {
		return false;
	}

	// 2. Try each possible full logical name based on all the mission
	// names.
	if( !gpr ) {
		char buffer[ 256 ];
		sprintf( buffer, "Ugc_%s", logicalName );
		gpr = patrolroute_GetByName( buffer, NULL );
	}
	if( !gpr ) {
		char buffer[ 256 ];
		sprintf( buffer, "Shared_%s", logicalName );
		gpr = patrolroute_GetByName( buffer, NULL );
	}
	if( !gpr ) {
		return false;
	}

	*out_ppPatrol = gpr;
	*out_pointIndex = pointIndex;
	return true;
}

void gslUGC_ProjectAddPlayComponentData( int iPartitionIdx, UGCProjectData* ugcProj, UGCPlayComponentData*** peaComponentData )
{
	int it;
	if( !SAFE_MEMBER( ugcProj, components )) {
		return;
	}

	for( it = 0; it != eaSize( &ugcProj->components->eaComponents ); ++it ) {
		UGCComponent* component = ugcProj->components->eaComponents[ it ];
		GameInteractable* gi;
		GameNamedPoint* gp;
		GameSpawnPoint* gsp;
		GameEncounter* ge;
		GamePatrolRoute* gpr;
		int actorIndex;
		int pointIndex;

		if( gslUGC_ComponentFindGameInteractable( ugcProj, component, &gi )) {
			UGCPlayComponentData* componentData = StructCreate( parse_UGCPlayComponentData );
			componentData->componentID = component->uID;
			copyMat4( gi->pWorldEntry->base_entry.bounds.world_matrix, componentData->componentMat4 );
			eaPush( peaComponentData, componentData );
		} else if( gslUGC_ComponentFindGameNamedPoint( ugcProj, component, &gp )) {
			UGCPlayComponentData* componentData = StructCreate( parse_UGCPlayComponentData );
			componentData->componentID = component->uID;
			identityMat3( componentData->componentMat4 );
			copyVec3( gp->pWorldPoint->point_pos, componentData->componentMat4[ 3 ]);
			eaPush( peaComponentData, componentData );
		} else if( gslUGC_ComponentFindGameSpawnPoint( ugcProj, component, &gsp )) {
			UGCPlayComponentData* componentData = StructCreate( parse_UGCPlayComponentData );
			componentData->componentID = component->uID;
			identityMat3( componentData->componentMat4 );
			copyVec3( gsp->pWorldPoint->spawn_pos, componentData->componentMat4[ 3 ]);
			eaPush( peaComponentData, componentData );
		} else if( gslUGC_ComponentFindGameEncounterAndActorIndex( ugcProj, component, &ge, &actorIndex )) {
			WorldActorProperties* wap = eaGet( &ge->pWorldEncounter->properties->eaActors, actorIndex );
			if( wap ) {
				UGCPlayComponentData* componentData = StructCreate( parse_UGCPlayComponentData );
				componentData->componentID = component->uID;
				createMat3YPR( componentData->componentMat4, wap->vRot );
				copyVec3( wap->vPos, componentData->componentMat4[ 3 ]);
				eaPush( peaComponentData, componentData );
			}
		} else if( gslUGC_ComponentFindGamePatrolRouteAndPointIndex( ugcProj, component, &gpr, &pointIndex )) {
			WorldPatrolPointProperties* wppp = eaGet( &gpr->pWorldRoute->properties->patrol_points, pointIndex );
			if( wppp ) {
				UGCPlayComponentData* componentData = StructCreate( parse_UGCPlayComponentData );
				componentData->componentID = component->uID;
				identityMat3( componentData->componentMat4 );
				copyVec3( wppp->pos, componentData->componentMat4[ 3 ]);
				eaPush( peaComponentData, componentData );
			}
		}
	}
}

void gslUGC_DoPlayingEditorHideComponent( Entity* ent, int componentID )
{
	UGCProjectData* ugcProj;
	UGCComponent* component;
	GameInteractable* gi;
	GameEncounter* ge;
	int actorIndex;

	ugcProj = gGSLState.pLastPlayData;
	if( !ugcProj ) {
		return;
	}
	component = ugcComponentFindByID( ugcProj->components, componentID );
	if( !component ) {
		return;
	}

	if( gslUGC_ComponentFindGameInteractable( ugcProj, component, &gi )) {
		interactable_SetHideState( entGetPartitionIdx( ent ), gi, true, 0, true );
	} else if( gslUGC_ComponentFindGameEncounterAndActorIndex( ugcProj, component, &ge, &actorIndex )) {
		Entity* entActor = encounter_GetActorEntityByIndex( entGetPartitionIdx( ent ), ge, actorIndex );
		if( entActor ) {
			entSetCodeFlagBits( entActor, ENTITYFLAG_DONOTFADE );
			gslQueueEntityDestroy( entActor );
		}
	}
}

void gslUGC_DoRespawnAtFullHealth( Entity* pEntity )
{
	NWCureAll( pEntity );
}

bool gslUGC_ProjectBudgetAllowsGenerate( UGCProjectData* ugcProj )
{
	UGCBudgetValidateState state;
	UGCRuntimeStatus status = { 0 };
	ugcSetStageAndAdd( &status, "BudgetAllowsGenerate" );		

	state = ugcValidateBudgets( ugcProj );

	ugcClearStage();
	StructReset( parse_UGCRuntimeStatus, &status );
	return (state != UGC_BUDGET_HARD_LIMIT);
}
