#include "gslNNOUGCGenesisMissions.h"

#include "EString.h"
#include "Expression.h"
#include "GameEvent.h"
#include "GameServerLib.h"
#include "NNOUGCCommon.h"
#include "NNOUGCResource.h"
#include "NotifyCommon.h"
#include "ResourceInfo.h"
#include "ResourceSystem_Internal.h"
#include "StateMachine.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "UGCError.h"
#include "UGCProjectUtils.h"
#include "WorldGrid.h"
#include "contact_common.h"
#include "encounter_common.h"
#include "error.h"
#include "file.h"
#include "gslNNOUGC.h"
#include "gslNNOUGCGenesisStructs.h"
#include "interaction_common.h"
#include "mission_common.h"
#include "mission_enums.h"
#include "tokenstore.h"
#include "wlGenesisMissionsGameStructs.h"
#include "wlUGC.h"


#include "Autogen/NotifyEnum_h_ast.h"


AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

/// A structure containing the global context needed when
/// transmogrifying info
typedef struct UGCGenesisTransmogrifyMissionContext {
	ZoneMapInfo* zmap_info;
	UGCGenesisMapDescription* map_desc;
	UGCGenesisMissionDescription* mission_desc;

	// accumulating data
	UGCGenesisZoneMission* zone_mission_accum;
} UGCGenesisTransmogrifyMissionContext;

typedef struct ObjectFSMTempData {
	UGCGenesisFSM *fsm;
	FSMState *state;
	const char *objName;
} ObjectFSMTempData;

typedef struct ObjectFSMData {
	char* challengeName;
	ObjectFSMTempData **fsmsAndStates;
	UGCGenesisFSM **fsms;
} ObjectFSMData;

AUTO_STRUCT;
typedef struct UGCGenesisMissionExtraMessages {
	const char* filename;						AST(CURRENTFILE)
	DisplayMessage** messages;					AST(NAME("ExtraMessage"))
} UGCGenesisMissionExtraMessages;
extern ParseTable parse_UGCGenesisMissionExtraMessages[];
#define TYPE_parse_UGCGenesisMissionExtraMessages UGCGenesisMissionExtraMessages

/// A structure containing the global context needed when generating
/// info
typedef struct UGCGenesisMissionContext {
	ZoneMapInfo* zmap_info;
	UGCGenesisZoneMapData* genesis_data;
	UGCGenesisZoneMission* zone_mission;
	int mission_num;
	UGCGenesisMissionPrompt** extra_prompts;
	bool is_ugc;
	const char *project_prefix;

	// accumulating data
	MissionDef*** optional_objectives_accum;
	MissionDef* root_mission_accum;
	ContactDef*** contacts_accum;
	FSM*** fsm_accum;
	UGCGenesisMissionExtraMessages* extra_messages_accum;
	UGCGenesisMissionRequirements* req_accum;
} UGCGenesisMissionContext;

#define langCreateMessage(...) youShouldCall_ugcGenesisCreateMessage

/// Objectives
static void ugcGenesisTransmogrifyObjectiveFixup( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisMissionObjective* objective_desc );

static void ugcGenesisCreateObjective( MissionDef** outStartObjective, MissionDef** outEndObjective, UGCGenesisMissionContext* context, MissionDef* grantingObjective, UGCGenesisMissionObjective* objective_desc );
static MissionDef* ugcGenesisCreateObjectiveForPart( UGCGenesisMissionContext* context, MissionDef* objectiveMissionDef, UGCGenesisMissionObjective* objective_desc, int partIndex );
static MissionDef* ugcGenesisCreateObjectiveOptional( UGCGenesisMissionContext* context, GameEvent** completeEvents, int count, char* debug_name );
static MissionDef* ugcGenesisCreateObjectiveOptionalExpr( UGCGenesisMissionContext* context, char* exprText, char* debug_name );
static void ugcGenesisAccumObjectivesInOrder( SA_PARAM_NN_VALID MissionDef *accum, SA_PARAM_NN_VALID UGCGenesisMissionContext* context, SA_PARAM_OP_VALID MissionDef* grantingObjective, UGCGenesisMissionObjective** objective_descs );
static void ugcGenesisAccumObjectivesAllOf( SA_PARAM_NN_VALID MissionDef *accum, SA_PARAM_NN_VALID UGCGenesisMissionContext* context, SA_PARAM_OP_VALID MissionDef* grantingObjective, UGCGenesisMissionObjective** objective_descs );
static void ugcGenesisAccumObjectivesBranch( SA_PARAM_NN_VALID MissionDef *accum, SA_PARAM_NN_VALID UGCGenesisMissionContext* context, SA_PARAM_OP_VALID MissionDef* grantingObjective, UGCGenesisMissionObjective** objective_descs );
static void ugcGenesisAccumFailureExpr( MissionDef* accum, const char* exprText );
static WorldInteractionPropertyEntry* ugcGenesisClickieMakeInteractionEntry( UGCGenesisMissionContext* context, UGCRuntimeErrorContext* error_context, UGCGenesisMissionChallengeClickie* clickie, UGCCheckedAttrib* checked_attrib );
static void ugcGenesisAccumMissionMap( SA_PARAM_NN_VALID UGCGenesisMissionContext* context, const char* mapName );

/// Prompts
static void ugcGenesisTransmogrifyPromptFixup( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisMissionPrompt* prompt );
static void ugcGenesisTransmogrifyPortalFixup( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisMissionPortal* portal );
static void ugcGenesisCreatePrompt( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt );
static SpecialDialogBlock* ugcGenesisCreatePromptBlock( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, int blockIndex );
static WorldGameActionProperties* ugcGenesisCreatePromptAction( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt );
static UGCGenesisMissionPrompt* ugcGenesisTransmogrifyFindPrompt( UGCGenesisTransmogrifyMissionContext* context, char* prompt_name );
static UGCGenesisMissionPrompt* ugcGenesisFindPrompt( UGCGenesisMissionContext* context, char* prompt_name );
static UGCGenesisMissionPromptBlock* ugcGenesisFindPromptBlock( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, char* block_name );
static char** ugcGenesisPromptBlockNames( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, bool isComplete );
static char* ugcGenesisSpecialDialogBlockNameTemp( const char* promptName, const char* blockName );

static ContactDef* ugcGenesisInternContactDef( UGCGenesisMissionContext* context, const char* contact_name );
static void ugcGenesisCreatePromptOptionalAction( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, const char* visibleExpr );
static WorldOptionalActionVolumeEntry* ugcGenesisCreatePromptOptionalActionEntry( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, const char* visibleExpr );
static WorldInteractionPropertyEntry* ugcGenesisCreatePromptInteractionEntry( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, const char* visibleExpr );

/// FSMs
static void ugcGenesisTransmogrifyFSMFixup( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisFSM* gfsm);
static void ugcGenesisBucketFSM(UGCGenesisMissionContext *context, ObjectFSMData ***dataAray, UGCGenesisFSM *fsm);
static void ugcGenesisCreateFSM(UGCGenesisMissionContext *context, ObjectFSMData *data);
const char* ugcGenesisExternVarGetMsgKey(UGCGenesisMissionContext *context, const char* varPrefix, WorldVariableDef* def);

/// Challenges
static UGCGenesisMissionZoneChallenge* ugcGenesisTransmogrifyChallenge( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisMissionChallenge* challenge );
static void ugcGenesisCreateChallenge( UGCGenesisMissionContext* context, UGCGenesisMissionZoneChallenge* challenge );
static void ugcGenesisChallengeNameFixup( UGCGenesisTransmogrifyMissionContext* context, char** challengeName );

/// Portals
static void ugcGenesisCreatePortal( UGCGenesisMissionContext* context, UGCGenesisMissionPortal* portal, bool isReversed );

/// When conditions
static char* ugcGenesisWhenExprText( UGCGenesisMissionContext* context, UGCGenesisWhen* when, UGCRuntimeErrorContext* debugContext, const char* debugFieldName, bool isEncounter );
static char* ugcGenesisWhenExprTextRaw(
		UGCGenesisMissionContext* context, const char* overrideZmapName, UGCGenesisMissionGenerationType overrideGenerationType,
		const char* overrideMissionName, UGCGenesisMissionZoneChallenge** overrideChallenges,
		UGCGenesisWhen* when, UGCRuntimeErrorContext* debugContext, const char* debugFieldName, bool isEncounter );
static void ugcGenesisWhenMissionExprTextAndEvents( char** outEstr, GameEvent*** outEvents, bool* outShowCount, UGCGenesisMissionContext* context, UGCGenesisWhen* when, UGCRuntimeErrorContext* debugContext, const char* debugFieldName );
static void ugcGenesisWhenMissionWaypointObjects( char*** out_makeVolumeObjects, MissionWaypoint*** out_waypoints, UGCGenesisMissionContext* context, UGCGenesisMissionWaypointMode mode, UGCGenesisWhen* when, UGCRuntimeErrorContext* debugContext, const char* debugFieldName );
static MissionWaypoint* ugcGenesisCreateMissionWaypointForChallengeName( UGCGenesisMissionContext* context, const char* challengeName );
static MissionWaypoint* ugcGenesisCreateMissionWaypointForExternalChallenge( UGCGenesisWhenExternalChallenge* challenge );

/// Checked Attribs
static char* ugcGenesisCheckedAttribText( UGCGenesisMissionContext* context, UGCCheckedAttrib* attrib, UGCRuntimeErrorContext* debugContext, const char* debugFieldName, bool isTeam );

/// Event fillers
///
/// TomY ENCOUNTER_HACK these functions can be greatly cleaned up one
/// the encounter hack is done.
static void ugcGenesisWriteText( char** estr, GameEvent* event, bool escaped );
static GameEvent* ugcGenesisCompleteChallengeEvent( GenesisChallengeType challengeType, const char* challengeName, bool useGroup, const char* zmapName );
static GameEvent* ugcGenesisReachLocationEvent( const char* layoutName, const char* roomOrChallengeName, const char* missionName, const char* zmapName );
static GameEvent* ugcGenesisReachLocationEventRaw( const char* zmapName, const char* volumeName );
static GameEvent* ugcGenesisKillCritterEvent( const char* critterDefName, const char* zmapName );
static GameEvent* ugcGenesisKillCritterGroupEvent( const char* critterGroupName, const char* zmapName );
static GameEvent* ugcGenesisTalkToContactEvent( char* contactName );
static GameEvent* ugcGenesisPromptEvent( char* dialogName, char* blockName, bool isComplete, const char* missionName, const char* zmapName, const char* challengeName );
static GameEvent* ugcGenesisExternalPromptEvent( char* dialogName, char* contactName, bool isComplete );
static GameEvent* ugcGenesisCompleteObjectiveEvent( UGCGenesisMissionObjective *obj, const char* zmapName );

/// Misc utils
static const void ugcGenesisMissionUpdateFilename( UGCGenesisMissionContext* context, MissionDef* mission );
static bool ugcGenesisTransmogrifyMissionValidate( UGCGenesisTransmogrifyMissionContext* context );
static bool ugcGenesisTransmogrifySharedChallengesValidate( UGCGenesisTransmogrifyMissionContext* context );
static bool ugcGenesisTransmogrifyChallengeValidate( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisMissionChallenge* challenge );
static bool ugcGenesisGenerateMissionValidate( UGCGenesisMissionContext* context );
static void ugcGenesisGenerateMissionValidateObjective( UGCGenesisMissionContext* context, StashTable table, bool* fatalAccum, UGCGenesisMissionObjective* objective );
static void ugcGenesisGenerateMissionPlayerData( UGCGenesisMissionContext* context, MissionDef* accum );
static void ugcGenesisMessageFillKeys( UGCGenesisMissionContext* context );
static void ugcGenesisContactMessageFillKeys( ContactDef * accum );
static void ugcGenesisParamsMessageFillKeys( UGCGenesisMissionContext* context, UGCGenesisProceduralObjectParams* params, int* messageIt );
static void ugcGenesisInteractParamsMessageFillKeys( UGCGenesisMissionContext* context, const char* challengeName, UGCGenesisInteractObjectParams* params, int* messageIt );
static void ugcGenesisInstancedParamsMessageFillKeys( UGCGenesisMissionContext* context, const char* challengeName, UGCGenesisInstancedObjectParams* params, int* messageIt );
static ContactMissionOffer* ugcGenesisInternMissionOffer( UGCGenesisMissionContext* context, const char* mission_name, bool isReturnOnly );
static UGCGenesisMissionRequirements* ugcGenesisInternRequirement( UGCGenesisMissionContext* context );
static UGCGenesisMissionExtraVolume* ugcGenesisInternExtraVolume( UGCGenesisMissionContext* context, const char* volume_name );
static UGCGenesisProceduralObjectParams* ugcGenesisInternRoomRequirementParams( UGCGenesisMissionContext* context, const char* layout_name, const char* room_name );
static UGCGenesisMissionRoomRequirements* ugcGenesisInternRoomRequirements( UGCGenesisMissionContext* context, const char* layout_name, const char* room_name );
static UGCGenesisProceduralObjectParams* ugcGenesisInternStartRoomRequirementParams( UGCGenesisMissionContext* context );
static UGCGenesisInstancedObjectParams* ugcGenesisInternChallengeRequirementParams( UGCGenesisMissionContext* context, const char* challenge_name );
static UGCGenesisInteractObjectParams* ugcGenesisInternInteractRequirementParams( UGCGenesisMissionContext* context, const char* challenge_name );
static UGCGenesisProceduralObjectParams* ugcGenesisInternVolumeRequirementParams( UGCGenesisMissionContext* context, const char* challenge_name );
static WorldInteractionPropertyEntry* ugcGenesisCreateInteractableChallengeRequirement( UGCGenesisMissionContext* context, const char* challenge_name );
static WorldInteractionPropertyEntry* ugcGenesisCreateInteractableChallengeVolumeRequirement( UGCGenesisMissionContext* context, const char* challenge_name );
static DialogBlock* ugcGenesisCreateDialogBlock( UGCGenesisMissionContext* context, char* dialogText, const char* astrAnimList );
static void ugcGenesisRefSystemUpdate( DictionaryHandleOrName dict, const char* key, void* obj );
static void ugcGenesisRunValidate( DictionaryHandleOrName dict, const char* key, void* obj );
static void ugcGenesisWriteTextFileFromDictionary( const char* filename, DictionaryHandleOrName dict );
static void ugcGenesisCreateMessage( UGCGenesisMissionContext* context, DisplayMessage* dispMessage, const char* defaultString );
static void ugcGenesisPushAllRoomNames( UGCGenesisMissionContext* context, char*** nameList );
static UGCGenesisMissionChallenge* ugcGenesisFindChallenge(UGCGenesisMapDescription* map_desc, UGCGenesisMissionDescription* mission_desc, const char* challenge_name, bool* outIsShared);

/// name generation
static const char* ugcGenesisMissionName( UGCGenesisMissionContext* context, bool playerSpecific );
//In header file: const char* ugcGenesisMissionNameRaw( const char* zmapName, const char* genesisMissionName, bool isOpenMission );
static const char* ugcGenesisContactName( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt );

/// Convert MISSION, which describes a single mission, into a
/// more concrete form.
///
/// Returns a list of extra requirements to the map.
UGCGenesisZoneMission* ugcGenesisTransmogrifyMission(ZoneMapInfo* zmap_info, UGCGenesisMapDescription* map_desc, int mission_num)
{
	UGCGenesisTransmogrifyMissionContext context = { zmap_info, map_desc, map_desc->missions[ mission_num ]};

	context.zone_mission_accum = StructCreate( parse_UGCGenesisZoneMission );

	if( !ugcGenesisTransmogrifyMissionValidate( &context )) {
		return NULL;
	}

	StructCopyAll( parse_UGCGenesisMissionZoneDescription, &context.mission_desc->zoneDesc, &context.zone_mission_accum->desc );
	context.zone_mission_accum->bTrackingEnabled = map_desc->is_tracking_enabled;

	// Misc. fixup
	{
		UGCGenesisMissionStartDescription* startDesc = &context.zone_mission_accum->desc.startDescription;
		int it;

		for( it = 0; it != eaSize( &startDesc->eaExitChallenges ); ++it ) {
			ugcGenesisChallengeNameFixup( &context, &startDesc->eaExitChallenges[ it ]);
		}
		if( !nullStr( startDesc->pcContinueChallenge )) {
			ugcGenesisChallengeNameFixup( &context, &startDesc->pcContinueChallenge );
		}
	}

	// Mission transmogrification
	{
		int it;
		for( it = 0; it != eaSize( &context.zone_mission_accum->desc.eaObjectives ); ++it ) {
			ugcGenesisTransmogrifyObjectiveFixup( &context, context.zone_mission_accum->desc.eaObjectives[ it ]);
		}
	}
	
	// Contact transmogrification
	{
		int it;
		for( it = 0; it != eaSize( &context.zone_mission_accum->desc.eaPrompts ); ++it ) {
			ugcGenesisTransmogrifyPromptFixup( &context, context.zone_mission_accum->desc.eaPrompts[ it ]);
		}
	}

	// FSM transmogrification
	{
		FOR_EACH_IN_EARRAY(context.zone_mission_accum->desc.eaFSMs, UGCGenesisFSM, gfsm)
		{
			ugcGenesisTransmogrifyFSMFixup(&context, gfsm);
		}
		FOR_EACH_END
	}

	FOR_EACH_IN_EARRAY(context.zone_mission_accum->desc.eaPortals, UGCGenesisMissionPortal, portal)
	{
		ugcGenesisTransmogrifyPortalFixup( &context, portal );
	}
	FOR_EACH_END;

	// challenge transmogrification
	{
		int it;
		for( it = 0; it != eaSize( &context.mission_desc->eaChallenges ); ++it ) {
			UGCGenesisMissionChallenge* challenge = context.mission_desc->eaChallenges[ it ];
			UGCGenesisMissionZoneChallenge* zoneChallenge = ugcGenesisTransmogrifyChallenge( &context, challenge );
			eaPush( &context.zone_mission_accum->eaChallenges, zoneChallenge );
		}
	}
	
	return context.zone_mission_accum;
}

/// Transmoform all the non-mission specific challenges into a more
/// concrete form.
UGCGenesisMissionZoneChallenge** ugcGenesisTransmogrifySharedChallenges(ZoneMapInfo* zmap_info, UGCGenesisMapDescription* map_desc)
{
	UGCGenesisTransmogrifyMissionContext context = { zmap_info, map_desc, NULL };
	UGCGenesisMissionZoneChallenge** accum = NULL;

	if( !ugcGenesisTransmogrifySharedChallengesValidate( &context )) {
		return NULL;
	}
	
	{
		int it;
		for( it = 0; it != eaSize( &context.map_desc->shared_challenges ); ++it ) {
			UGCGenesisMissionChallenge* challenge = context.map_desc->shared_challenges[ it ];
			UGCGenesisMissionZoneChallenge* zoneChallenge = ugcGenesisTransmogrifyChallenge( &context, challenge );

			if( zoneChallenge ) {
				eaPush( &accum, zoneChallenge );
			}
		}
	}

	return accum;
}

/// Add a ProceduralEncounterProperties for CHALLENGE to PEP-LIST.
///
/// Needed by TomY ENCOUNTER_HACK.
void ugcGenesisTransmogrifyChallengePEPHack( UGCGenesisMapDescription* map_desc, int mission_num, UGCGenesisMissionChallenge* challenge, UGCGenesisProceduralEncounterProperties*** outPepList )
{
	UGCGenesisMissionDescription* mission;
	int encounterIt;

	if( mission_num >= 0 ) {
		mission = map_desc->missions[ mission_num ];
	} else {
		mission = NULL;
	}

	for( encounterIt = 0; encounterIt < challenge->iCount; ++encounterIt ) {
		UGCGenesisProceduralEncounterProperties* encounter_property = StructCreate( parse_UGCGenesisProceduralEncounterProperties );
		char encounterName[ 256 ];

		if( !mission ) {
			sprintf( encounterName, "Shared_%s_%02d", challenge->pcName, encounterIt );
		} else {
			sprintf( encounterName, "%s_%s_%02d", mission->zoneDesc.pcName, challenge->pcName, encounterIt );
		}
		encounter_property->encounter_name = allocAddString( encounterName );
		if( mission ) {
			encounter_property->genesis_mission_name = StructAllocString( mission->zoneDesc.pcName );
			encounter_property->genesis_open_mission = (mission->zoneDesc.generationType != UGCGenesisMissionGenerationType_PlayerMission);
		}
		encounter_property->genesis_mission_num = mission_num;

		StructCopyAll(parse_UGCGenesisWhen, &challenge->spawnWhen, &encounter_property->spawn_when);
		{
			int challengeIt = 0;
			for( challengeIt = 0; challengeIt != eaSize( &encounter_property->spawn_when.eaChallengeNames ); ++challengeIt ) {
				bool challengeIsShared;
				UGCGenesisMissionChallenge* spawnWhenChallenge = ugcGenesisFindChallenge( map_desc, mission, encounter_property->spawn_when.eaChallengeNames[ challengeIt ], &challengeIsShared );
				if( spawnWhenChallenge ) {
					UGCGenesisMissionZoneChallenge* spawnWhenChallengeInfo = StructCreate( parse_UGCGenesisMissionZoneChallenge );
					char buffer[ 256 ];

					assert( mission || challengeIsShared );
					if( challengeIsShared ) {
						sprintf( buffer, "Shared_%s", spawnWhenChallenge->pcName );
					} else {
						sprintf( buffer, "%s_%s", mission->zoneDesc.pcName, spawnWhenChallenge->pcName );
					}
					spawnWhenChallengeInfo->pcName = StructAllocString( buffer );
								
					spawnWhenChallengeInfo->eType = spawnWhenChallenge->eType;
					spawnWhenChallengeInfo->iNumToComplete = spawnWhenChallenge->iCount;
					
					eaPush( &encounter_property->when_challenges, spawnWhenChallengeInfo );
					StructCopyString(&encounter_property->spawn_when.eaChallengeNames[ challengeIt ], buffer);
				} else {
					ugcRaiseError( UGC_ERROR, ugcMakeTempErrorContextChallenge(challenge->pcName, SAFE_MEMBER(mission, zoneDesc.pcName), challenge->pcLayoutName),
									   "SpawnWhen references challenge \"%s\", but it does not exist.",
									   challenge->spawnWhen.eaChallengeNames[ challengeIt ]);
				}
			}
		}
						
		eaPush( outPepList, encounter_property );
	}
}

/// Return an earray of all reference keys currently in DICT that
/// belong to ZMAP.
///
/// Do not free the strings in the earray.  They are pooled!
static const char** ugcGenesisListReferences( const char* zmap_filename, DictionaryHandleOrName dict )
{
	const char** accum = NULL;
	ResourceDictionaryInfo* dictInfo = resDictGetInfo( dict );
	char path[ MAX_PATH ];
	strcpy( path, zmap_filename );
	getDirectoryName( path );
	strcat( path, "/" );

	{
		int it;
		for( it = 0; it != eaSize( &dictInfo->ppInfos ); ++it ) {
			ResourceInfo* info = dictInfo->ppInfos[ it ];

			if( strStartsWith( info->resourceLocation, path )) {
				eaPush( &accum, info->resourceName );
			}
		}
	}

	return accum;
}

/// Delete all missions associated with zmap
void ugcGenesisDeleteMissions(const char* zmap_filename)
{
	ResourceActionList actions = { 0 };
	const char** oldMissionNames = ugcGenesisListReferences( zmap_filename, g_MissionDictionary );
	const char** oldContactNames = ugcGenesisListReferences( zmap_filename, g_ContactDictionary );

	resSetDictionaryEditMode( g_MissionDictionary, true );
	resSetDictionaryEditMode( g_ContactDictionary, true );
	resSetDictionaryEditMode( gMessageDict, true );

	{
		int it;
		for( it = 0; it != eaSize( &oldContactNames ); ++it ) {
			resAddRequestLockResource( &actions, g_ContactDictionary, oldContactNames[ it ], NULL );
			resAddRequestSaveResource( &actions, g_ContactDictionary, oldContactNames[ it ], NULL );
		}
		for( it = 0; it != eaSize( &oldMissionNames ); ++it ) {
			resAddRequestLockResource( &actions, g_MissionDictionary, oldMissionNames[ it ], NULL );
			resAddRequestSaveResource( &actions, g_MissionDictionary, oldMissionNames[ it ], NULL );
		}
	}

	actions.bDisableValidation = true;
	resRequestResourceActions( &actions );

	if( actions.eResult != kResResult_Success ) {
		int it;
		for( it = 0; it != eaSize( &actions.ppActions ); ++it ) {
			if( actions.ppActions[ it ]->eResult == kResResult_Success ) {
				continue;
			}

			ugcRaiseErrorInternalCode( UGC_ERROR, "%s Resource: %s -- %s",
										   actions.ppActions[ it ]->pDictName,
										   actions.ppActions[ it ]->pResourceName,
										   actions.ppActions[ it ]->estrResultString );
		}
	}

	StructDeInit( parse_ResourceActionList, &actions );
	eaDestroy( &oldMissionNames );
	eaDestroy( &oldContactNames );
}

void ugcGenesisDestroyObjectFSMData(ObjectFSMData *data)
{
	free( data->challengeName );
	eaDestroyEx(&data->fsmsAndStates, NULL);
	eaDestroy(&data->fsms);

	free(data);
}

const char *ugcGenesisMissionRoomVolumeName(const char* layout_name, const char *room_name, const char *mission_name)
{
	char buf[256];
	sprintf(buf, "%s_%s_%s", layout_name, mission_name, room_name);
	return allocAddString(buf);
}

const char *ugcGenesisMissionChallengeVolumeName(const char *challenge_name, const char *mission_name)
{
	char buf[256];
	sprintf(buf, "%s_VOLUME", challenge_name);
	return allocAddString(buf);
}

const char *ugcGenesisMissionVolumeName(const char* layout_name, const char *mission_name)
{
	char buf[256];
	sprintf(buf, "%s_%s_OptActs", layout_name, mission_name);
	return allocAddString(buf);
}

/// Generate mission/contact information for ZONE-MISSION.
UGCGenesisMissionRequirements* ugcGenesisGenerateMission(
		ZoneMapInfo* zmap_info, UGCGenesisZoneMapData* genesis_data, int mission_num, UGCGenesisZoneMission* mission, UGCGenesisMissionAdditionalParams* additionalParams,
		bool is_ugc, const char *project_prefix, bool write_mission)
{
	UGCGenesisMissionContext context = { zmap_info, genesis_data, mission, mission_num };
	ContactDef** contactsAccum = NULL;
	MissionDef* missionAccum = NULL;
	MissionDef* playerSpecificMissionAccum = NULL;
	MissionDef** optionalObjectivesAccum = NULL;
	FSM** fsmAccum = NULL;

	context.is_ugc = is_ugc;
	context.contacts_accum = &contactsAccum;
	context.fsm_accum = &fsmAccum;
	context.extra_messages_accum = StructCreate( parse_UGCGenesisMissionExtraMessages );
	{
		char buffer[ MAX_PATH ];
		char zmapDirectory[ MAX_PATH ];
		
		strcpy( zmapDirectory, zmapInfoGetFilename( zmap_info ));
		getDirectoryName( zmapDirectory );
		sprintf( buffer, "%s/messages/%sExtra", zmapDirectory, mission->desc.pcName );
		context.extra_messages_accum->filename = allocAddFilename( buffer );
	}
	
	context.req_accum = StructCreate( parse_UGCGenesisMissionRequirements );
	context.req_accum->missionName = StructAllocString( context.zone_mission->desc.pcName );
	context.optional_objectives_accum = &optionalObjectivesAccum;
	context.project_prefix = project_prefix;

	if( !ugcGenesisGenerateMissionValidate( &context )) {
		return NULL;
	}

	#ifndef GAMESERVER
	{
		assert( !write_mission );
	}
	#endif

	// Door generation
	{
		UGCGenesisMissionStartDescription* startDesc = &context.zone_mission->desc.startDescription;
		
		char missionSucceeded[ 1024 ];
		char missionNotSucceeded[ 1024 ];
		if( context.zone_mission->desc.generationType != UGCGenesisMissionGenerationType_PlayerMission ) {
			sprintf( missionSucceeded, "OpenMissionStateSucceeded(\"%s\")", ugcGenesisMissionName( &context, false ));
		} else {
			sprintf( missionSucceeded, "HasCompletedMission(\"%s\") or MissionStateSucceeded(\"%s\")",
					 ugcGenesisMissionName( &context, false ), ugcGenesisMissionName( &context, false ));
		}
		sprintf( missionNotSucceeded, "not (%s)", missionSucceeded );

		{
			UGCGenesisMissionPrompt* missionReturnPrompt = ugcGenesisFindPrompt( &context, "MissionReturn" );

			switch( startDesc->eExitFrom ) {
				case UGCGenesisMissionExitFrom_Anywhere:
					// nothing to do

				xcase UGCGenesisMissionExitFrom_Challenge: {
					int it;
					for( it = 0; it != eaSize( &startDesc->eaExitChallenges ); ++it ) {
						char* exitChallenge = startDesc->eaExitChallenges[ it ]; 
						WorldInteractionPropertyEntry* entry = ugcGenesisCreateInteractableChallengeRequirement( &context, exitChallenge );
						if(   exitChallenge && startDesc->pcContinueChallenge
							  && stricmp( exitChallenge, startDesc->pcContinueChallenge ) == 0 ) {
							entry->pInteractCond = exprCreateFromString( missionNotSucceeded, NULL );
						}
				
						if( missionReturnPrompt ) {
							entry->pcInteractionClass = allocAddString( "CLICKABLE" );
							entry->pActionProperties = StructCreate( parse_WorldActionInteractionProperties );
							eaPush( &entry->pActionProperties->successActions.eaActions, ugcGenesisCreatePromptAction( &context, missionReturnPrompt ));
						} else {
							entry->pcInteractionClass = allocAddString( "DOOR" );
							entry->pDoorProperties = StructCreate( parse_WorldDoorInteractionProperties );
							StructReset( parse_WorldVariableDef, &entry->pDoorProperties->doorDest );
							entry->pDoorProperties->doorDest.eType = WVAR_MAP_POINT;
							entry->pDoorProperties->doorDest.eDefaultType = WVARDEF_SPECIFY_DEFAULT;
							entry->pDoorProperties->doorDest.pSpecificValue = StructCreate(parse_WorldVariable);
							entry->pDoorProperties->doorDest.pSpecificValue->eType = WVAR_MAP_POINT;
							entry->pDoorProperties->doorDest.pSpecificValue->pcStringVal = StructAllocString( "MissionReturn" );
							COPY_HANDLE( entry->pDoorProperties->hTransSequence, startDesc->hExitTransitionOverride );
						}
					}
				}

				xcase UGCGenesisMissionExitFrom_DoorInRoom: case UGCGenesisMissionExitFrom_Entrance:
					// handled elsewhere, nothing to do here
					;
			}
		}

		if( startDesc->bContinue ) {
			UGCGenesisMissionPrompt* missionContinuePrompt = ugcGenesisFindPrompt( &context, "MissionContinue" );

			switch( startDesc->eContinueFrom ) {
				case UGCGenesisMissionExitFrom_Anywhere:
					// nothing to do

				xcase UGCGenesisMissionExitFrom_Challenge: {
					WorldInteractionPropertyEntry* entry = ugcGenesisCreateInteractableChallengeRequirement( &context, startDesc->pcContinueChallenge );
					entry->pInteractCond = exprCreateFromString( missionSucceeded, NULL );

					if( missionContinuePrompt ) {
						entry->pcInteractionClass = allocAddString( "CLICKABLE" );
						entry->pActionProperties = StructCreate( parse_WorldActionInteractionProperties );
						eaPush( &entry->pActionProperties->successActions.eaActions, ugcGenesisCreatePromptAction( &context, missionContinuePrompt ));
					} else {
						entry->pcInteractionClass = allocAddString( "DOOR" );
						entry->pDoorProperties = StructCreate( parse_WorldDoorInteractionProperties );
						entry->pDoorProperties->doorDest.eType = WVAR_MAP_POINT;
						entry->pDoorProperties->doorDest.eDefaultType = WVARDEF_SPECIFY_DEFAULT;
						entry->pDoorProperties->doorDest.pSpecificValue = StructCreate(parse_WorldVariable);
						entry->pDoorProperties->doorDest.pSpecificValue->eType = WVAR_MAP_POINT;
						entry->pDoorProperties->doorDest.pSpecificValue->pcZoneMap = StructAllocString( context.zone_mission->desc.startDescription.pcContinueMap );
						COPY_HANDLE(entry->pDoorProperties->hTransSequence, startDesc->hContinueTransitionOverride );

						{
							int i;
							for( i = 0; i != eaSize( &context.zone_mission->desc.startDescription.eaContinueVariables ); ++i ) {
								WorldVariable* continueVar = context.zone_mission->desc.startDescription.eaContinueVariables[ i ];
								WorldVariableDef* continueVarDef = StructCreate( parse_WorldVariableDef );

								continueVarDef->pcName = continueVar->pcName;
								continueVarDef->eType = continueVar->eType;
								continueVarDef->eDefaultType = WVARDEF_SPECIFY_DEFAULT;
								continueVarDef->pSpecificValue = StructClone( parse_WorldVariable, continueVar );
								eaPush( &entry->pDoorProperties->eaVariableDefs, continueVarDef );
							}
						}
					}
				}

				xcase UGCGenesisMissionExitFrom_DoorInRoom:
					if( missionContinuePrompt ) {
						UGCGenesisProceduralObjectParams* params = ugcGenesisInternRoomRequirementParams( &context, startDesc->pcContinueLayout, startDesc->pcContinueRoom );
						ugcGenesisProceduralObjectSetOptionalActionVolume( params );
						eaPush( &params->optionalaction_volume_properties->entries,
								ugcGenesisCreatePromptOptionalActionEntry( &context, missionContinuePrompt, missionSucceeded ));
					} else {
						ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextMission(context.zone_mission->desc.pcName),
											  "ContinueFrom door (as clickie) not supported yet." );
					}

				xcase UGCGenesisMissionExitFrom_Entrance:
					if( missionContinuePrompt ) {
						UGCGenesisProceduralObjectParams* params = ugcGenesisInternStartRoomRequirementParams( &context );
						ugcGenesisProceduralObjectSetOptionalActionVolume( params );
						eaPush( &params->optionalaction_volume_properties->entries,
								ugcGenesisCreatePromptOptionalActionEntry( &context, missionContinuePrompt, missionSucceeded ));
					} else {
						ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextMission(context.zone_mission->desc.pcName),
											  "ContinueFrom door (as clickie) not supported yet." );
					}
			}
		}
	}

	// Portal generation
	{
		int it;
		for( it = 0; it != eaSize( &context.zone_mission->desc.eaPortals ); ++it ) {
			UGCGenesisMissionPortal* portal = context.zone_mission->desc.eaPortals[ it ];

			ugcGenesisCreatePortal( &context, portal, false );
			if( portal->eType != UGCGenesisMissionPortal_OneWayOutOfMap ) {
				ugcGenesisCreatePortal( &context, portal, true );
			}
		}
	}
	
	// Mission generation
	{
		// base mission info
		missionAccum = StructCreate( parse_MissionDef );
		missionAccum->name = ugcGenesisMissionName( &context, false );
		if( context.zmap_info ) {
			missionAccum->genesisZonemap = StructAllocString( zmapInfoGetPublicName( context.zmap_info ));
		}
		COPY_HANDLE( missionAccum->hCategory, context.zone_mission->desc.hCategory );
		missionAccum->version = 1;
		missionAccum->comments = StructAllocString( "Autogenerated Genesis mission." );

		StructCopyAll( parse_MissionLevelDef, &context.zone_mission->desc.levelDef, &missionAccum->levelDef );

		if( context.zone_mission->desc.pcDisplayName ) {
			ugcGenesisCreateMessage( &context, &missionAccum->displayNameMsg, context.zone_mission->desc.pcDisplayName );
		}
		ugcGenesisCreateMessage( &context, &missionAccum->uiStringMsg, context.zone_mission->desc.pcShortText );
		missionAccum->eReturnType = MissionReturnType_Message;
		ugcGenesisCreateMessage( &context, &missionAccum->msgReturnStringMsg, context.zone_mission->desc.strReturnText );

		// open mission info
		if( context.zone_mission->desc.generationType != UGCGenesisMissionGenerationType_PlayerMission ) {
			UGCGenesisMissionRequirements* missionReq = ugcGenesisInternRequirement( &context );

			missionAccum->missionType = MissionType_OpenMission;

			// Push all layout names onto the open mission volume
			{
				int it;
				for( it = 0; it != eaSize( &genesis_data->solar_systems ); ++it ) {
					eaPush( &missionAccum->eaOpenMissionVolumes, StructAllocString(
									ugcGenesisMissionVolumeName( genesis_data->solar_systems[ it ]->layout_name, missionReq->missionName )));
				}
				for( it = 0; it != eaSize( &genesis_data->genesis_interiors ); ++it ) {
					eaPush( &missionAccum->eaOpenMissionVolumes, StructAllocString(
									ugcGenesisMissionVolumeName( genesis_data->genesis_interiors[ it ]->layout_name, missionReq->missionName )));
				}
				if( genesis_data->genesis_exterior ) {
					eaPush( &missionAccum->eaOpenMissionVolumes, StructAllocString(
									ugcGenesisMissionVolumeName( genesis_data->genesis_exterior->layout_name, missionReq->missionName )));
				}
				if( genesis_data->genesis_exterior_nodes ) {
					eaPush( &missionAccum->eaOpenMissionVolumes, StructAllocString(
									ugcGenesisMissionVolumeName( genesis_data->genesis_exterior_nodes->layout_name, missionReq->missionName )));
				}
				for( it = 0; it != eaSize( &genesis_data->genesis_shared ); ++it ) {
					eaPush( &missionAccum->eaOpenMissionVolumes, StructAllocString(
									ugcGenesisMissionVolumeName( genesis_data->genesis_shared[ it ]->layout_name, missionReq->missionName )));
				}
			}
			
			if( !missionReq->params ) {
				missionReq->params = StructCreate( parse_UGCGenesisProceduralObjectParams );
			}
			ugcGenesisProceduralObjectSetEventVolume(missionReq->params);
			missionAccum->autoGrantOnMap = allocAddString( zmapInfoGetPublicName( context.zmap_info ));
			
			missionAccum->needsReturn = false;

			if( context.zone_mission->desc.generationType != UGCGenesisMissionGenerationType_OpenMission_NoPlayerMission ) {
				// And create the minimal player specific mission
				playerSpecificMissionAccum = StructCreate( parse_MissionDef );
				playerSpecificMissionAccum->name = ugcGenesisMissionName( &context, true );
				playerSpecificMissionAccum->genesisZonemap = StructAllocString( zmapInfoGetPublicName( context.zmap_info ));
				COPY_HANDLE( playerSpecificMissionAccum->hCategory, context.zone_mission->desc.hCategory );
				playerSpecificMissionAccum->version = 1;
				playerSpecificMissionAccum->comments = StructAllocString( "Autogenerated Genesis mission." );

				StructCopyAll( parse_MissionLevelDef, &context.zone_mission->desc.levelDef, &playerSpecificMissionAccum->levelDef );

				if( context.zone_mission->desc.pOpenMissionDescription->pcPlayerSpecificDisplayName ) {
					ugcGenesisCreateMessage( &context, &playerSpecificMissionAccum->displayNameMsg, context.zone_mission->desc.pOpenMissionDescription->pcPlayerSpecificDisplayName );
				}
				ugcGenesisCreateMessage( &context, &playerSpecificMissionAccum->uiStringMsg, context.zone_mission->desc.pOpenMissionDescription->pcPlayerSpecificShortText );

				// listen for the open mission completing
				playerSpecificMissionAccum->doNotUncomplete = true;
				{
					char buffer[ 256 ];
					playerSpecificMissionAccum->meSuccessCond = StructCreate( parse_MissionEditCond );
					playerSpecificMissionAccum->meSuccessCond->type = MissionCondType_Expression;
					sprintf( buffer, "OpenMissionMapCredit(\"%s\")", ugcGenesisMissionName( &context, false ));
					playerSpecificMissionAccum->meSuccessCond->valStr = StructAllocString( buffer );
				}

				ugcGenesisGenerateMissionPlayerData( &context, playerSpecificMissionAccum );
			}
		} else {
			ugcGenesisGenerateMissionPlayerData( &context, missionAccum );
		}
		
		missionAccum->ePlayType = context.zone_mission->desc.ePlayType;
		missionAccum->ugcProjectID = context.zone_mission->desc.ugcProjectID;
		missionAccum->eAuthorSource = context.zone_mission->desc.eAuthorSource;
		if( playerSpecificMissionAccum ) {
			playerSpecificMissionAccum->ePlayType = context.zone_mission->desc.ePlayType;
			playerSpecificMissionAccum->ugcProjectID = context.zone_mission->desc.ugcProjectID;
			playerSpecificMissionAccum->eAuthorSource = context.zone_mission->desc.eAuthorSource;
		}

		context.root_mission_accum = missionAccum;
		if( context.zone_mission->desc.generationType == UGCGenesisMissionGenerationType_PlayerMission ) {
			context.root_mission_accum->bObjectiveMapsIncludesAllStaticMaps = true;
		}
		ugcGenesisAccumObjectivesInOrder( missionAccum, &context, NULL, context.zone_mission->desc.eaObjectives );

		if( additionalParams ) {
			// add the extra properties
			{
				int it;
				for( it = 0; it != eaSize( &additionalParams->eaInteractableOverrides ); ++it ) {
					eaPush( &missionAccum->ppInteractableOverrides,
							StructClone( parse_InteractableOverride, additionalParams->eaInteractableOverrides[ it ]));
				}
			}

			// add the mission offer overrides
			{
				int it;
				for( it = 0; it != eaSize( &additionalParams->eaMissionOfferOverrides ); ++it ) {
					eaPush( &missionAccum->ppMissionOfferOverrides,
							StructClone( parse_MissionOfferOverride, additionalParams->eaMissionOfferOverrides[ it ]));
				}
			}
			// add ImageMenuItemOverrides
			{
				int i;
				for( i = 0; i != eaSize( &additionalParams->eaImageMenuItemOverrides ); ++i ) {
					eaPush( &missionAccum->ppImageMenuItemOverrides,
							StructClone( parse_ImageMenuItemOverride, additionalParams->eaImageMenuItemOverrides[ i ]));
				}
			}
			// add success actions
			{
				int i;
				for( i = 0; i != eaSize( &additionalParams->eaSuccessActions ); ++i ) {
					eaPush( &missionAccum->ppSuccessActions,
							StructClone( parse_WorldGameActionProperties, additionalParams->eaSuccessActions[ i ]));
				}
			}
		}
		
		ugcGenesisMissionUpdateFilename( &context, missionAccum );

		if( playerSpecificMissionAccum ) {
			ugcGenesisMissionUpdateFilename( &context, playerSpecificMissionAccum );
		}
	}

	// Contact generation
	{
		int it;
		for( it = 0; it != eaSize( &context.zone_mission->desc.eaPrompts ); ++it ) {
			UGCGenesisMissionPrompt* prompt = context.zone_mission->desc.eaPrompts[ it ];
			ugcGenesisCreatePrompt( &context, prompt );
		}
		for( it = 0; it != eaSize( &context.extra_prompts ); ++it ) {
			UGCGenesisMissionPrompt* prompt = context.extra_prompts[ it ];
			ugcGenesisCreatePrompt( &context, prompt );
		}

		for( it = 0; it != eaSize( &contactsAccum ); ++it ) {
			ContactDef* def = contactsAccum[ it ];
			ugcGenesisContactMessageFillKeys( def );
		}
	}

	// FSM generation
	{
		int i;
		ObjectFSMData **fsmData = NULL;
		// Figure out all the FSMs placed on each actor
		for(i=0; i<eaSize(&context.zone_mission->desc.eaFSMs); i++)
		{
			UGCGenesisFSM *fsm = context.zone_mission->desc.eaFSMs[i];
			ugcGenesisBucketFSM(&context, &fsmData, fsm);
		}

		// Build an FSM from all the parts (may not generate an FSM in simple cases)
		for(i=0; i<eaSize(&fsmData); i++)
		{
			ObjectFSMData *data = fsmData[i];
			ugcGenesisCreateFSM(&context, data);
		}
		eaDestroyEx(&fsmData, ugcGenesisDestroyObjectFSMData);
	}

	// Challenge generation
	{
		int it;
		for( it = 0; it != eaSize( &context.zone_mission->eaChallenges ); ++it ) {
			UGCGenesisMissionZoneChallenge* challenge = context.zone_mission->eaChallenges[ it ];
			ugcGenesisCreateChallenge( &context, challenge );
		}
	}

	// Mission drop generation
	{
		if(!missionAccum->params)
			missionAccum->params = StructCreate(parse_MissionDefParams);
		FOR_EACH_IN_EARRAY_FORWARDS(context.zone_mission->desc.eaMissionDrops, UGCGenesisMissionDrop, genesisMissionDrop)
		{
			MissionDrop* missionDrop = StructCreate(parse_MissionDrop);
			missionDrop->type = MissionDropTargetType_EncounterGroup_AllCombatantsDead;
			missionDrop->whenType = MissionDropWhenType_DuringMission;
			missionDrop->pchMapName = genesisMissionDrop->astrMapName;
			missionDrop->value = genesisMissionDrop->astrEncounterLogicalName;
			missionDrop->RewardTableName = REF_STRING_FROM_HANDLE(genesisMissionDrop->hReward);
			eaPush(&missionAccum->params->missionDrops, StructClone(parse_MissionDrop, missionDrop));
		}
		FOR_EACH_END;
	}

	eaPushEArray( &missionAccum->subMissions, &optionalObjectivesAccum );
	{
		int it;
		for( it = 0; it != eaSize( &optionalObjectivesAccum ); ++it ) {
			WorldGameActionProperties* grantObjective = StructCreate( parse_WorldGameActionProperties );
			grantObjective->eActionType = WorldGameActionType_GrantSubMission;
			grantObjective->pGrantSubMissionProperties = StructCreate( parse_WorldGrantSubMissionActionProperties );
			grantObjective->pGrantSubMissionProperties->pcSubMissionName = allocAddString( optionalObjectivesAccum[ it ]->name );
			eaPush( &missionAccum->ppOnStartActions, grantObjective );
		}
	}
	eaDestroy( &optionalObjectivesAccum );

	if( context.zmap_info ) {
		ugcGenesisMessageFillKeys( &context );
	}
	ugcGenesisMissionMessageFillKeys( missionAccum, NULL );
	if( playerSpecificMissionAccum ) {
		ugcGenesisMissionMessageFillKeys( playerSpecificMissionAccum, NULL );
	}

	/// EVERYTHING IS FULLY GENERATED

	// This has to happen even on the client, as the layers point to these keys (Not needed in UGC since we commit to layers immediately)
	if( context.zmap_info ) {
		if( !context.is_ugc ) {
			langApplyEditorCopySingleFile( parse_UGCGenesisMissionRequirements, context.req_accum, true, !write_mission );
		}
		langApplyEditorCopySingleFile( parse_UGCGenesisMissionExtraMessages, context.extra_messages_accum, true, !write_mission );
	} else {
		if(   context.req_accum->roomRequirements || context.req_accum->challengeRequirements
			  || context.req_accum->extraVolumes || context.req_accum->params ) {
			ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextMission( context.zone_mission->desc.pcName ),
								  "Mission generated map requirements, but there is no associated map.  Likely the mission will not work." );
		}	
	}

	if( write_mission ) 
	{
		ResourceActionList tempList = {0};
		int it;
		resSetDictionaryEditMode( gFSMDict, true );
		resSetDictionaryEditMode( g_MissionDictionary, true );
		resSetDictionaryEditMode( g_ContactDictionary, true );
		resSetDictionaryEditMode( gMessageDict, true );

		for( it = 0; it != eaSize( &contactsAccum ); ++it ) 
		{
			resAddRequestLockResource(&tempList, g_ContactDictionary, contactsAccum[ it ]->name, contactsAccum[ it ]);
			resAddRequestSaveResource(&tempList, g_ContactDictionary, contactsAccum[ it ]->name, contactsAccum[ it ]);
		}

		for( it = 0; it <eaSize(&fsmAccum); it++)
		{
			if(!fsmGenerate(fsmAccum[it]))
			{
				ugcRaiseErrorContext(UGC_ERROR, ugcMakeTempErrorContextMission(context.zone_mission->desc.pcName), 
									 "FSM %s failed to generate", fsmAccum[it]->name);
			}

			resAddRequestLockResource(&tempList, gFSMDict, fsmAccum[ it ]->name, fsmAccum[ it ]);
			resAddRequestSaveResource(&tempList, gFSMDict, fsmAccum[ it ]->name, fsmAccum[ it ]);
		}

		resAddRequestLockResource(&tempList, g_MissionDictionary, missionAccum->name, missionAccum);
		resAddRequestSaveResource(&tempList, g_MissionDictionary, missionAccum->name, missionAccum);
		
		if (playerSpecificMissionAccum)
		{
			resAddRequestLockResource(&tempList, g_MissionDictionary, playerSpecificMissionAccum->name, playerSpecificMissionAccum);
			resAddRequestSaveResource(&tempList, g_MissionDictionary, playerSpecificMissionAccum->name, playerSpecificMissionAccum);
		}

		tempList.bDisableValidation = true;
		TellControllerToLog( STACK_SPRINTF( __FUNCTION__ ": About to request resource actions, name=%s", missionAccum->name ));
		resRequestResourceActions(&tempList);
		TellControllerToLog( STACK_SPRINTF( __FUNCTION__ ": Resource actions done, name=%s", missionAccum->name ));

		if (tempList.eResult != kResResult_Success)
		{
			for (it = 0; it < eaSize(&tempList.ppActions); it++)
			{
				if (tempList.ppActions[it]->eResult == kResResult_Success)
				{
					continue;
				}
				ugcRaiseErrorInternalCode( UGC_ERROR, "%s Resource: %s -- %s",
											   tempList.ppActions[it]->pDictName,
											   tempList.ppActions[it]->pResourceName,
											   tempList.ppActions[it]->estrResultString);
			}
		}

		StructDeInit(parse_ResourceActionList, &tempList);
	}

	eaDestroyStruct( &contactsAccum, parse_ContactDef );
	eaDestroyStruct( &fsmAccum, parse_FSM );
	StructDestroy( parse_MissionDef, missionAccum );
	StructDestroy( parse_MissionDef, playerSpecificMissionAccum );
	StructDestroySafe( parse_UGCGenesisMissionExtraMessages, &context.extra_messages_accum );
	
	return context.req_accum;
}

/// Transmogrify a generic objective
void ugcGenesisTransmogrifyObjectiveFixup( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisMissionObjective* objective_desc )
{
	char** challengeNames = SAFE_MEMBER( objective_desc, succeedWhen.eaChallengeNames );
	int it;

	if( !objective_desc ) {
		return;
	}

	for( it = 0; it != eaSize( &challengeNames ); ++it ) {
		UGCGenesisMissionChallenge* challenge = ugcGenesisFindChallenge( context->map_desc, context->mission_desc, challengeNames[ it ], NULL );
		
		if( !challenge ) {
			ugcRaiseError( UGC_ERROR, ugcMakeTempErrorContextObjective( objective_desc->pcName, SAFE_MEMBER(context->mission_desc, zoneDesc.pcName ) ),
							   "Objective references challenge \"%s\", but it does not exist.",
							   challengeNames[ it ]);
			return;
		}
		
		ugcGenesisChallengeNameFixup( context, &challengeNames[ it ]);
	}

	if( objective_desc->succeedWhen.pcPromptChallengeName ) {
		ugcGenesisChallengeNameFixup( context, &objective_desc->succeedWhen.pcPromptChallengeName );
	}

	for( it = 0; it != eaSize( &objective_desc->eaChildren ); ++it ) {
		ugcGenesisTransmogrifyObjectiveFixup( context, objective_desc->eaChildren[ it ]);
	}
}

/// Create a new objective, stash in OUT-START-OBJECTIVE.  Stash the
/// final objective in the chain in OUT-END-OBJECTIVE.
///
/// If there is not a seperate ending objective,
/// OUT-START-OBJECTIVE == OUT-END-OBJECTIVE
void ugcGenesisCreateObjective( MissionDef** outStartObjective, MissionDef** outEndObjective, UGCGenesisMissionContext* context, MissionDef* grantingObjective, UGCGenesisMissionObjective* objective_desc )
{
	MissionDef* accum = StructCreate( parse_MissionDef );
	
	accum->name = allocAddString( objective_desc->pcName );
	ugcGenesisCreateMessage( context, &accum->uiStringMsg, objective_desc->pcShortText );
	eaCopyStructs( &objective_desc->eaOnStartActions, &accum->ppOnStartActions, parse_WorldGameActionProperties );

	if( !nullStr( objective_desc->pcSuccessFloaterText )) {
		WorldGameActionProperties* successFloater = StructCreate( parse_WorldGameActionProperties );
		eaPush( &accum->ppSuccessActions, successFloater );

		successFloater->eActionType = WorldGameActionType_SendFloaterMsg;
		successFloater->pSendFloaterProperties = StructCreate( parse_WorldSendFloaterActionProperties );
		ugcGenesisCreateMessage( context, &successFloater->pSendFloaterProperties->floaterMsg, objective_desc->pcSuccessFloaterText );
		setVec3( successFloater->pSendFloaterProperties->vColor, 0, 0, 0.886275 );  //< Color copied from gvProgressColor
	}

	// Only ItemCount objectives right now can be uncompleted.
	if( objective_desc->succeedWhen.type != UGCGenesisWhen_ItemCount ) {
		accum->doNotUncomplete = true;
	} else {
		accum->doNotUncomplete = false;
	}

	// Optional reward table on success
	accum->params = StructCreate( parse_MissionDefParams );
	accum->params->OnsuccessRewardTableName = REF_STRING_FROM_HANDLE( objective_desc->hReward );

	{
		UGCRuntimeErrorContext* debugContext = ugcMakeTempErrorContextObjective( objective_desc->pcName, SAFE_MEMBER(context->zone_mission, desc.pcName) );

		// Handle success when
		if( objective_desc->succeedWhen.type == UGCGenesisWhen_AllOf ) {
			ugcGenesisAccumObjectivesAllOf( accum, context, grantingObjective, objective_desc->eaChildren );
		} else if( objective_desc->succeedWhen.type == UGCGenesisWhen_InOrder ) {
			ugcGenesisAccumObjectivesInOrder( accum, context, grantingObjective, objective_desc->eaChildren );
		} else if( objective_desc->succeedWhen.type == UGCGenesisWhen_Branch ) {
			ugcGenesisAccumObjectivesBranch( accum, context, grantingObjective, objective_desc->eaChildren );
		} else {
			bool showCount = false;
			char* expr = NULL;
			ugcGenesisWhenMissionExprTextAndEvents(
					&expr, &accum->eaTrackedEvents, &showCount,
					context, &objective_desc->succeedWhen, debugContext, "SucceedWhen" );
			accum->meSuccessCond = StructCreate( parse_MissionEditCond );
			accum->meSuccessCond->type = MissionCondType_Expression;
			accum->meSuccessCond->valStr = StructAllocString( expr );
			accum->meSuccessCond->showCount = showCount ? MDEShowCount_Show_Count : MDEShowCount_Normal;
			estrDestroy( &expr );
		}
		
		// If this completes a on a clickie and it is scoped to this
		// objective, we create optional subobjectives per clickie to
		// scope each one.
		if( objective_desc->succeedWhen.type == UGCGenesisWhen_ExternalChallengeComplete ) {
			int it;
			for( it = 0; it != eaSize( &objective_desc->succeedWhen.eaExternalChallenges ); ++it ) {
				UGCGenesisWhenExternalChallenge* externalChallenge = objective_desc->succeedWhen.eaExternalChallenges[ it ];
				
				if( externalChallenge->eType == GenesisChallenge_Clickie && externalChallenge->pClickie && IS_HANDLE_ACTIVE( externalChallenge->pClickie->hInteractionDef )) {
					MissionDef* challengeGateAccum = ugcGenesisCreateObjectiveForPart( context, accum, objective_desc, it );

					// MJF Aug/16/2012: This is done here, which is
					// outside the normal location in
					// genesisWhenMissionWaypointObjects, because
					// otherwise the waypoint won't go away as you
					// interact with things.
					if( objective_desc->eWaypointMode ) {
						if( objective_desc->eWaypointMode == UGCGenesisMissionWaypointMode_Points ) {
							eaPush( &challengeGateAccum->eaWaypoints, ugcGenesisCreateMissionWaypointForExternalChallenge( externalChallenge ));
						} else {
							ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextObjective( objective_desc->pcName, SAFE_MEMBER( context->zone_mission, desc.pcName ) ),
												  "External clicky objective's waypoint mode must be \" None\" or \"Points\"." );
						}
					}

					// interactable overrides
					{
						InteractableOverride* interactAccum = StructCreate( parse_InteractableOverride );
						char gateExpr[ 1024 ];
						interactAccum->pcMapName = allocAddString( externalChallenge->pcMapName );
						interactAccum->pcInteractableName = allocAddString( externalChallenge->pcName );
						interactAccum->pPropertyEntry = ugcGenesisClickieMakeInteractionEntry( context, debugContext, externalChallenge->pClickie, externalChallenge->succeedCheckedAttrib );

						interactAccum->pPropertyEntry->bOverrideInteract = true;
						sprintf( gateExpr, "MissionStateInProgress(\"%s::%s\")", ugcGenesisMissionName( context, true ), challengeGateAccum->name );
						interactAccum->pPropertyEntry->pInteractCond = exprCreateFromString( gateExpr, NULL );
						eaPush( &context->root_mission_accum->ppInteractableOverrides, interactAccum );

						if( externalChallenge->succeedCheckedAttrib ) {
							char* exprText = ugcGenesisCheckedAttribText( context, externalChallenge->succeedCheckedAttrib, ugcMakeTempErrorContextObjective( objective_desc->pcName, SAFE_MEMBER( context->zone_mission, desc.pcName )), "SucceedCheckedAttrib", false );

							if( exprText ) {
								interactAccum->pPropertyEntry->pSuccessCond = exprCreateFromString( exprText, NULL );
							}

							estrDestroy( &exprText );
						}
					}
				}
			}
		}

		// MJF Oct/9/2012: This is done here, which is outside the
		// normal location in genesisWhenMissionWaypointObjects,
		// because otherwise the waypoint won't go away as you
		// interact with things.
		if(  objective_desc->succeedWhen.type == UGCGenesisWhen_ChallengeComplete
			 && objective_desc->eWaypointMode == UGCGenesisMissionWaypointMode_Points ) {
			int it;
			for( it = 0; it != eaSize( &objective_desc->succeedWhen.eaChallengeNames ); ++it ) {
				MissionDef* partAccum = ugcGenesisCreateObjectiveForPart( context, accum, objective_desc, it );

				eaPush( &partAccum->eaWaypoints, ugcGenesisCreateMissionWaypointForChallengeName( context, objective_desc->succeedWhen.eaChallengeNames[ it ]));
			}
		}

		// Handle waypoints
		if( objective_desc->eWaypointMode ) {
			char** objectNames = NULL;
			MissionWaypoint** waypoints = NULL;

			ugcGenesisWhenMissionWaypointObjects( &objectNames, &waypoints, context, objective_desc->eWaypointMode, &objective_desc->succeedWhen,
												  debugContext, "SucceedWhen" );
			if( objectNames ) {
				char volumeName[ 256 ];
				UGCGenesisMissionExtraVolume* extraVolumeAccum;
				MissionWaypoint* waypointAccum;

				sprintf( volumeName, "%s_%s_Waypoint", SAFE_MEMBER( context->zone_mission, desc.pcName ), objective_desc->pcName );
				extraVolumeAccum = ugcGenesisInternExtraVolume( context, volumeName );
				extraVolumeAccum->objects = objectNames;
				objectNames = NULL;

				waypointAccum = StructCreate( parse_MissionWaypoint );
				waypointAccum->type = MissionWaypointType_AreaVolume;
				waypointAccum->name = StructAllocString( volumeName );
				waypointAccum->mapName = allocAddString( zmapInfoGetPublicName( context->zmap_info ));
				eaPush(&accum->eaWaypoints, waypointAccum);
			}

			eaPushEArray( &accum->eaWaypoints, &waypoints );
			eaDestroy( &waypoints );

			{
				const char** mapNames = NULL;
				int it;
				for( it = 0; it != eaSize( &accum->eaWaypoints ); ++it ) {
					eaPushUnique( &mapNames, allocAddString( accum->eaWaypoints[ it ]->mapName ));
				}
				for( it = 0; it != eaSize( &mapNames ); ++it ) {
					MissionMap* missionMapAccum = StructCreate( parse_MissionMap );
					missionMapAccum->pchMapName = mapNames[ it ];
					eaPush( &accum->eaObjectiveMaps, missionMapAccum );
				}
				eaDestroy( &mapNames );
			}

			// Only personal missions have map waypoints.  But we need
			// to put every map this mission is on there so the
			// objectives can display on each map.
			if( context->zone_mission->desc.generationType == UGCGenesisMissionGenerationType_PlayerMission ) {
				int it;
				for( it = 0; it != eaSize( &accum->eaWaypoints ); ++it ) {
					MissionWaypoint* waypoint = accum->eaWaypoints[ it ];
					if( resNamespaceIsUGC( waypoint->mapName )) {
						ugcGenesisAccumMissionMap( context, waypoint->mapName );
					}
				}
			}
		}
		eaPushStructs( &accum->eaWaypoints, &objective_desc->eaExtraWaypoints, parse_MissionWaypoint );
	}
	
	// Handle timeout
	accum->uTimeout = objective_desc->uTimeout;
	if( objective_desc->uTimeout ) {
		if( accum->meSuccessCond && accum->meSuccessCond->type == MissionCondType_And ) {
			ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextObjective( objective_desc->pcName, SAFE_MEMBER( context->zone_mission, desc.pcName ) ),
								  "Objective type does not support auto-succeed on timer." );
		} else {
			MissionEditCond* timerCond = StructCreate( parse_MissionEditCond );

			timerCond->type = MissionCondType_Expression;
			timerCond->valStr = StructAllocString( "TimeExpired()" );

			if( !accum->meSuccessCond ) {
				accum->meSuccessCond = timerCond;
			} else {
				MissionEditCond* otherCond = accum->meSuccessCond;
				MissionEditCond* metaCond = StructCreate( parse_MissionEditCond );

				metaCond->type = MissionCondType_Or;
				eaPush( &metaCond->subConds, timerCond );
				eaPush( &metaCond->subConds, otherCond );
				accum->meSuccessCond = metaCond;
			}
		}
	}

	// Return and cleanup
	*outStartObjective = accum;
	eaPush( &context->root_mission_accum->subMissions, accum );

	// If there are any prompts that follow this objective, don't
	// start the next objective until those prompts are done.
	{
		const char** completePromptNames = NULL;
		int it;
		assert(context->zone_mission);
		for( it = 0; it != eaSize( &context->zone_mission->desc.eaPrompts ); ++it ) {
			UGCGenesisMissionPrompt* prompt = context->zone_mission->desc.eaPrompts[ it ];

			if( prompt->showWhen.type == UGCGenesisWhen_ObjectiveComplete ) {
				if( eaFindString( &prompt->showWhen.eaObjectiveNames, objective_desc->pcName ) != -1 ) {
					eaPush( &completePromptNames, prompt->pcName );
				}
			}
		}
		
		if( !eaSize( &completePromptNames )) {
			*outEndObjective = accum;
		} else {
			MissionDef* endAccum = StructCreate( parse_MissionDef );
			char endAccumName[ 256 ];

			sprintf( endAccumName, "AfterPrompt_%s", objective_desc->pcName );
			endAccum->name = allocAddString( endAccumName );
			endAccum->doNotUncomplete = true;
			{
				UGCGenesisWhen whenAfterPrompt = { 0 };
				UGCRuntimeErrorContext* debugContext = ugcMakeTempErrorContextObjective( objective_desc->pcName, SAFE_MEMBER( context->zone_mission, desc.pcName ) );
				char* expr = NULL;

				whenAfterPrompt.type = UGCGenesisWhen_PromptCompleteAll;
				whenAfterPrompt.eaPromptNames = (char**)completePromptNames;
				ugcGenesisWhenMissionExprTextAndEvents(
						&expr, &endAccum->eaTrackedEvents, NULL,
						context, &whenAfterPrompt, debugContext, "SucceedWhen" );
				endAccum->meSuccessCond = StructCreate( parse_MissionEditCond );
				endAccum->meSuccessCond->type = MissionCondType_Expression;
				endAccum->meSuccessCond->valStr = StructAllocString( expr );
				estrDestroy( &expr );
			}

			{
				WorldGameActionProperties* grantEndObjective = StructCreate( parse_WorldGameActionProperties );
				grantEndObjective->eActionType = WorldGameActionType_GrantSubMission;
				grantEndObjective->pGrantSubMissionProperties = StructCreate( parse_WorldGrantSubMissionActionProperties );
				grantEndObjective->pGrantSubMissionProperties->pcSubMissionName = allocAddString( endAccumName );
				eaPush( &accum->ppSuccessActions, grantEndObjective );
			}
			
			*outEndObjective = endAccum;
			eaPush( &context->root_mission_accum->subMissions, endAccum );
		}
	}
}

/// Create an objective that completes on part of OBJECTIVE-DESC.
///
/// This exists to hang object-specific waypoints or interact states
/// off of.
MissionDef* ugcGenesisCreateObjectiveForPart( UGCGenesisMissionContext* context, MissionDef* objectiveDef, UGCGenesisMissionObjective* objective_desc, int partIndex )
{
	MissionDef* accum = StructCreate( parse_MissionDef );
	UGCGenesisWhen when = { 0 };
	char* expr = NULL;
	UGCRuntimeErrorContext* debugContext = ugcMakeErrorContextObjective( objective_desc->pcName, SAFE_MEMBER(context->zone_mission, desc.pcName) );

	{
		char buffer[ 256 ];
		sprintf( buffer, "%s_Part%d", objective_desc->pcName, partIndex + 1 );
		accum->name = allocAddString( buffer );
	}
	accum->doNotUncomplete = true;
	
	switch( objective_desc->succeedWhen.type ) {
		xcase UGCGenesisWhen_ChallengeComplete:
			when.type = UGCGenesisWhen_ChallengeComplete;
			eaPush( &when.eaChallengeNames, objective_desc->succeedWhen.eaChallengeNames[ partIndex ]);
			
		xcase UGCGenesisWhen_ExternalChallengeComplete:
			when.type = UGCGenesisWhen_ExternalChallengeComplete;
			eaPush( &when.eaExternalChallenges, objective_desc->succeedWhen.eaExternalChallenges[ partIndex ]);
			
		xdefault:
			ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextObjective( objective_desc->pcName, SAFE_MEMBER( context->zone_mission, desc.pcName )),
								  "ugcGenesisCreateObjectiveForPart called with unsupported when type %d", objective_desc->succeedWhen.type );
	}
	
	ugcGenesisWhenMissionExprTextAndEvents(
			&expr, &accum->eaTrackedEvents, NULL,
			context, &when, debugContext, "SucceedWhen" );
	
	estrInsert( &expr, 0, "(", 1 );
	{
		char* exprFunc = "";
		if( context->zone_mission->desc.generationType != UGCGenesisMissionGenerationType_PlayerMission ) {
			exprFunc = "OpenMissionStateSucceeded";
		} else {
			exprFunc = "MissionStateSucceeded";
		}
		estrConcatf( &expr, ") or %s(\"%s::%s\")",
					 exprFunc, ugcGenesisMissionName( context, true ), objectiveDef->name );
	}
					
	accum->meSuccessCond = StructCreate( parse_MissionEditCond );
	accum->meSuccessCond->type = MissionCondType_Expression;
	accum->meSuccessCond->valStr = StructAllocString( expr );
	estrDestroy( &expr );
	eaDestroy( &when.eaChallengeNames );
	eaDestroy( &when.eaExternalChallenges );

	// Hook it up to the existing mission
	eaPush( &context->root_mission_accum->subMissions, accum );
	{
		WorldGameActionProperties* grantAccum = StructCreate( parse_WorldGameActionProperties );
		grantAccum->eActionType = WorldGameActionType_GrantSubMission;
		grantAccum->pGrantSubMissionProperties = StructCreate( parse_WorldGrantSubMissionActionProperties );
		grantAccum->pGrantSubMissionProperties->pcSubMissionName = allocAddString( accum->name );

		eaPush( &objectiveDef->ppOnStartActions, grantAccum );
	}

	StructDestroy( parse_UGCRuntimeErrorContext, debugContext );

	return accum;
}


/// Create an optional objective that completes when COMPLETE-EVENT.
///
/// This is useful for FSM-like behavior.
MissionDef* ugcGenesisCreateObjectiveOptional( UGCGenesisMissionContext* context, GameEvent** completeEvents, int count, char* debug_name )
{
	MissionDef* accum = StructCreate( parse_MissionDef );
	char buffer[ 1024 ];

	sprintf( buffer, "Optional_%d_%s",
			 eaSize( context->optional_objectives_accum ),
			 debug_name );
	
	accum->name = allocAddString( buffer );
	accum->doNotUncomplete = true;
	accum->meSuccessCond = StructCreate( parse_MissionEditCond );
	accum->meSuccessCond->type = MissionCondType_Expression;
	{
		char* exprAccum = NULL;
		int it;
		for( it = 0; it != eaSize( &completeEvents ); ++it ) {
			GameEvent* missionEvent = StructClone( parse_GameEvent, completeEvents[ it ]);
			
			sprintf( buffer, "Optional_%d", it );
			missionEvent->pchEventName = allocAddString( buffer );
			estrConcatf( &exprAccum, "%sMissionEventCount(\"%s\")",
						 (exprAccum ? " + " : ""),
						 buffer);
			
			eaPush( &accum->eaTrackedEvents, missionEvent );
		}
		estrConcatf( &exprAccum, " >= %d", count );
		
		accum->meSuccessCond->valStr = StructAllocString( exprAccum );
		estrDestroy( &exprAccum );
	}

	eaPush( context->optional_objectives_accum, accum );

	return accum;
}

/// Create an optional objective that completes when EXPR-TEXT is true.
///
/// This is useful for FSM-like behavior.
MissionDef* ugcGenesisCreateObjectiveOptionalExpr( UGCGenesisMissionContext* context, char* exprText, char* debug_name )
{
	MissionDef* accum = StructCreate( parse_MissionDef );
	char buffer[ 1024 ];

	sprintf( buffer, "Optional_%d_%s",
			 eaSize( context->optional_objectives_accum ),
			 debug_name );
	
	accum->name = allocAddString( buffer );
	accum->doNotUncomplete = true;
	accum->meSuccessCond = StructCreate( parse_MissionEditCond );
	accum->meSuccessCond->type = MissionCondType_Expression;
	accum->meSuccessCond->valStr = StructAllocString( exprText );

	eaPush( context->optional_objectives_accum, accum );

	return accum;
}

/// Add to ACCUM all the objectives in OBJECTIVE-DESCS.  Each one
/// grants the next one.
void ugcGenesisAccumObjectivesInOrder( MissionDef* accum, UGCGenesisMissionContext* context, MissionDef* grantingObjective, UGCGenesisMissionObjective** objective_descs )
{
	const char* completeObjectiveName = NULL;
	int numObjectives = eaSize( &objective_descs );
	if( numObjectives != 0 ) {
		MissionDef** startObjectives = NULL;
		MissionDef** endObjectives = NULL;
		MissionDef** nextObjectives = NULL;
		MissionDef* grantingObjectiveIt = grantingObjective;

		int it;
		for( it = 0; it != numObjectives; ++it ) {
			MissionDef* startObjective;
			MissionDef* endObjective;

			ugcGenesisCreateObjective( &startObjective, &endObjective, context, grantingObjectiveIt, objective_descs[ it ]);
			if( !objective_descs[ it ]->bOptional ) {
				grantingObjectiveIt = endObjective;
			}
			eaPush( &startObjectives, startObjective );
			eaPush( &endObjectives, endObjective );
		}
		for( it = numObjectives - 1; it >= 0; --it ) {
			MissionDef* startObjective = startObjectives[ it ];
			MissionDef* endObjective = endObjectives[ it ];

			if( !objective_descs[ it ]->bOptional ) {
				int nextIt;
				for( nextIt = 0; nextIt != eaSize( &nextObjectives ); ++nextIt ) {
					MissionDef* nextObjective = nextObjectives[ nextIt ];
						
					WorldGameActionProperties* grantObjective = StructCreate( parse_WorldGameActionProperties );
					grantObjective->eActionType = WorldGameActionType_GrantSubMission;
					grantObjective->pGrantSubMissionProperties = StructCreate( parse_WorldGrantSubMissionActionProperties );
					grantObjective->pGrantSubMissionProperties->pcSubMissionName = allocAddString( nextObjective->name );
					eaPush( &endObjective->ppSuccessActions, grantObjective );
				}

				eaClear( &nextObjectives );

				if( !completeObjectiveName ) {
					completeObjectiveName = endObjective->name;
				}
			}

			eaPush( &nextObjectives, startObjective );
		}

		eaDestroy( &startObjectives );
		eaDestroy( &endObjectives );

		// grant the first ones
		{
			int nextIt;
			for( nextIt = 0; nextIt != eaSize( &nextObjectives ); ++nextIt ) {
				MissionDef* nextObjective = nextObjectives[ nextIt ];
						
				WorldGameActionProperties* grantObjective = StructCreate( parse_WorldGameActionProperties );
				grantObjective->eActionType = WorldGameActionType_GrantSubMission;
				grantObjective->pGrantSubMissionProperties = StructCreate( parse_WorldGrantSubMissionActionProperties );
				grantObjective->pGrantSubMissionProperties->pcSubMissionName = allocAddString( nextObjective->name );

				if( accum->uiStringMsg.pEditorCopy ) {
					eaPush( &accum->ppOnStartActions, grantObjective );
				} else if( grantingObjective ) {
					eaPush( &grantingObjective->ppSuccessActions, grantObjective );
				} else {
					eaPush( &context->root_mission_accum->ppOnStartActions, grantObjective );
				}
			}
		}

		eaDestroy( &nextObjectives );
	}

	// can't use the default of "all" objectives, since I will be
	// adding optional sub-missions.
	accum->meSuccessCond = StructCreate( parse_MissionEditCond );
	if( completeObjectiveName ) {
		accum->meSuccessCond->type = MissionCondType_Objective;
		accum->meSuccessCond->valStr = StructAllocString( completeObjectiveName );
	} else {
		accum->meSuccessCond->type = MissionCondType_And;
	}
}

/// Add to ACCUM all the objectives in OBJECTIVE-DESCS. All of them
/// get granted at once
void ugcGenesisAccumObjectivesAllOf( MissionDef *accum, UGCGenesisMissionContext* context, MissionDef* grantingObjective, UGCGenesisMissionObjective** objective_descs )
{
	int numObjectives = eaSize( &objective_descs );
	
	if( numObjectives > 0 ) {
		const char** endObjectiveNames = NULL;
		eaSetSize( &endObjectiveNames, numObjectives );
	
		{
			int it;
			for( it = 0; it != numObjectives; ++it ) {
				MissionDef* startObjective;
				MissionDef* endObjective;
				WorldGameActionProperties* grantObjective = StructCreate( parse_WorldGameActionProperties );

				ugcGenesisCreateObjective( &startObjective, &endObjective, context, grantingObjective, objective_descs[ it ]);

				grantObjective->eActionType = WorldGameActionType_GrantSubMission;
				grantObjective->pGrantSubMissionProperties = StructCreate( parse_WorldGrantSubMissionActionProperties );
				grantObjective->pGrantSubMissionProperties->pcSubMissionName = allocAddString( objective_descs[ it ]->pcName );

				if( accum->uiStringMsg.pEditorCopy ) {
					eaPush( &accum->ppOnStartActions, grantObjective );
				} else if( grantingObjective ) {
					eaPush( &grantingObjective->ppSuccessActions, grantObjective );
				} else {
					eaPush( &context->root_mission_accum->ppOnStartActions, grantObjective );
				}

				endObjectiveNames[ it ] = endObjective->name;
			}
		}

		
		// can't use the default of "all" objectives, since I will be
		// adding optional sub-missions.
		if( accum ) {
			accum->meSuccessCond = StructCreate( parse_MissionEditCond );
			accum->meSuccessCond->type = MissionCondType_And;
			{
				int it;
				for( it = 0; it != numObjectives; ++it ) {
					if( !objective_descs[ it ]->bOptional ) {
						MissionEditCond* objectiveCond = StructCreate( parse_MissionEditCond );
						objectiveCond->type = MissionCondType_Objective;

						assert( it < eaSize( &endObjectiveNames ));
						objectiveCond->valStr = StructAllocString( endObjectiveNames[ it ]);
					
						eaPush( &accum->meSuccessCond->subConds, objectiveCond );
					}
				}
			}
		}

		eaDestroy( &endObjectiveNames );
	}
}

/// Add to ACCUM all the objectives in OBJECTIVE-DESCS. All of them
/// get granted at once, only one needs to be completed for this to
/// succeed.  If ever one of the objectives succeeds, then all the
/// other objectives fail.
void ugcGenesisAccumObjectivesBranch( MissionDef *accum, UGCGenesisMissionContext* context, MissionDef* grantingObjective, UGCGenesisMissionObjective** objective_descs )
{
	int numObjectives = eaSize( &objective_descs );

	// MJF TODO: Remove me when infrastructure for branching missions is
	// better.
	//
	// MJF TODO: add per-branch reward tables
	ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextObjective( accum->name, SAFE_MEMBER( context->zone_mission, desc.pcName ) ),
						  "Branching objectives are not yet supported." );
	return;

	/*
	if( numObjectives > 0 ) {
		MissionDef** startObjectives = NULL;
		const char** objectiveAdvanceNames = NULL;
		const char** endObjectiveNames = NULL;
		eaSetSize( &startObjectives, numObjectives );
		eaSetSize( &objectiveAdvanceNames, numObjectives );
		eaSetSize( &endObjectiveNames, numObjectives );
	
		{
			int it;
			for( it = 0; it != numObjectives; ++it ) {
				UGCGenesisMissionObjective* objectiveDesc = objective_descs[ it ];
				
				MissionDef* startObjective;
				MissionDef* endObjective;
				WorldGameActionProperties* grantObjective = StructCreate( parse_WorldGameActionProperties );

				ugcGenesisCreateObjective( &startObjective, &endObjective, context, accum, objectiveDesc );

				grantObjective->eActionType = WorldGameActionType_GrantSubMission;
				grantObjective->pGrantSubMissionProperties = StructCreate( parse_WorldGrantSubMissionActionProperties );
				grantObjective->pGrantSubMissionProperties->pcSubMissionName = allocAddString( objective_descs[ it ]->pcName );
				eaPush( &accum->ppOnStartActions, grantObjective );

				startObjectives[ it ] = startObjective;
				if( objectiveDesc->succeedWhen.type == UGCGenesisWhen_InOrder ) {
					if( eaSize( &objectiveDesc->eaChildren ) > 0) {
						if( SAFE_MEMBER( objectiveDesc->eaChildren[ 0 ], pcName )) {
							objectiveAdvanceNames[ it ] = objectiveDesc->eaChildren[ 0 ]->pcName;
						}
					}
				} else {
					ugcRaiseError( UGC_ERROR, ugcMakeTempErrorContextObjective( accum->name, SAFE_MEMBER( context->zone_mission, desc.pcName ) ),
									   "Branch objectives can only have InOrder children, one for each branch." );
				}				
				
				endObjectiveNames[ it ] = endObjective->name;
			}
		}

		// Add failures if any of the other objetives advanced
		{
			int it;
			int otherIt;
			for( it = 0; it != numObjectives; ++it ) {
				MissionDef* startObjective = startObjectives[ it ];
				char* exprText = NULL;
				for( otherIt = 0; otherIt != numObjectives; ++otherIt ) {
					if( it == otherIt ) {
						continue;
					}

					estrConcatf( &exprText, "%sMissionStateSucceeded(\"%s::%s\")",
								 (exprText ? " or " : ""),
								 ugcGenesisMissionName( context, false ), objectiveAdvanceNames[ otherIt ]);
				}

				genesisAccumFailureExpr( startObjective, exprText );
				estrDestroy( &exprText );
			}
		}

		accum->meSuccessCond = StructCreate( parse_MissionEditCond );
		accum->meSuccessCond->type = MissionCondType_Or;
		{
			int it;
			for( it = 0; it != numObjectives; ++it ) {
				if( !objective_descs[ it ]->bOptional ) {
					MissionEditCond* objectiveCond = StructCreate( parse_MissionEditCond );
					objectiveCond->type = MissionCondType_Objective;

					assert( it < eaSize( &endObjectiveNames ));
					objectiveCond->valStr = StructAllocString( endObjectiveNames[ it ]);
					
					eaPush( &accum->meSuccessCond->subConds, objectiveCond );
				}
			}
		}

		eaDestroy( &startObjectives );
		eaDestroy( &objectiveAdvanceNames );
		eaDestroy( &endObjectiveNames );
	}
	*/
}

/// Add to ACCUM an expression that when true causes the objective to
/// fail.
///
/// If there is already a failure expression, it will get or'd with this one.
void ugcGenesisAccumFailureExpr( MissionDef* accum, const char* exprText )
{
	MissionEditCond* newCond = StructCreate( parse_MissionEditCond );
	newCond->type = MissionCondType_Expression;
	newCond->valStr = StructAllocString( exprText );
		
	if( !accum->meFailureCond ) {
		accum->meFailureCond = newCond;
	} else if( accum->meFailureCond->type == MissionCondType_Or ) {
		eaPush( &accum->meFailureCond->subConds, newCond );
	} else {
		MissionEditCond* orCond = StructCreate( parse_MissionEditCond );
		orCond->type = MissionCondType_Or;
		eaPush( &orCond->subConds, accum->meFailureCond );
		eaPush( &orCond->subConds, newCond );
		accum->meFailureCond = orCond;
	} 
}

/// Generate a WorldInteractionPropertyEntry for the clickie
/// properties.
///
/// This is used for creating InteractOverrides as well as
/// requirements.
WorldInteractionPropertyEntry* ugcGenesisClickieMakeInteractionEntry( UGCGenesisMissionContext* context, UGCRuntimeErrorContext* error_context, UGCGenesisMissionChallengeClickie* clickie, UGCCheckedAttrib* checked_attrib )
{
	WorldInteractionPropertyEntry* interactionEntry = StructCreate( parse_WorldInteractionPropertyEntry );

	assert(clickie);

	interactionEntry->pcInteractionClass = allocAddString( "FROMDEFINITION" );
	COPY_HANDLE( interactionEntry->hInteractionDef, clickie->hInteractionDef );

	if( clickie->pcInteractText || clickie->pcSuccessText || clickie->pcFailureText ) {
		interactionEntry->pTextProperties = StructCreate( parse_WorldTextInteractionProperties );
		if( clickie->pcInteractText ) {
			ugcGenesisCreateMessage( context, &interactionEntry->pTextProperties->interactOptionText,
								  clickie->pcInteractText );
		}
		if( clickie->pcSuccessText ) {
			ugcGenesisCreateMessage( context, &interactionEntry->pTextProperties->successConsoleText,
								  clickie->pcSuccessText );
		}
		if( clickie->pcFailureText ) {
			ugcGenesisCreateMessage( context, &interactionEntry->pTextProperties->failureConsoleText,
								  clickie->pcFailureText );
		}
	}

	if( IS_HANDLE_ACTIVE( clickie->hInteractAnim )) {
		interactionEntry->pAnimationProperties = StructCreate( parse_WorldAnimationInteractionProperties );
		COPY_HANDLE( interactionEntry->pAnimationProperties->hInteractAnim,
					 clickie->hInteractAnim );
	} else {
		interactionEntry->pAnimationProperties = StructCreate( parse_WorldAnimationInteractionProperties );
	}

	if (IS_HANDLE_ACTIVE( clickie->hRewardTable )) {
		interactionEntry->pRewardProperties = StructCreate( parse_WorldRewardInteractionProperties );
		COPY_HANDLE( interactionEntry->pRewardProperties->hRewardTable, clickie->hRewardTable);
	}

	if (clickie->bConsumeSuccessItem)
	{
		if (SAFE_MEMBER(checked_attrib, astrItemName))
		{
			WorldGameActionProperties *action = StructCreate(parse_WorldGameActionProperties);
			char nsName[ RESOURCE_NAME_MAX_SIZE ];
			char buffer[ RESOURCE_NAME_MAX_SIZE ];

			if (!interactionEntry->pActionProperties)
				interactionEntry->pActionProperties = StructCreate(parse_WorldActionInteractionProperties);

			action->eActionType = WorldGameActionType_TakeItem;
			action->pTakeItemProperties = StructCreate(parse_WorldTakeItemActionProperties);
			action->pTakeItemProperties->iCount = 1;

			if( resExtractNameSpace_s( zmapInfoGetPublicName( context->zmap_info ), SAFESTR( nsName ), NULL, 0 )) {
				sprintf( buffer, "%s:%s", nsName, checked_attrib->astrItemName );
			} else {
				sprintf( buffer, "%s", checked_attrib->astrItemName );
			}
			SET_HANDLE_FROM_STRING("ItemDef", buffer, action->pTakeItemProperties->hItemDef);

			eaPush(&interactionEntry->pActionProperties->successActions.eaActions, action);
		}
		else
		{
			ugcRaiseErrorContext( UGC_ERROR, error_context,
								  "Challenge consumes required item on interact success, "
								  "but no required item has been specified." );
		}
	}

	return interactionEntry;
}

/// Add this MissionMap to the overall mission.  This makes sure that
/// the mission text displays correctly on those maps.
///
/// See [NNO-19835].
void ugcGenesisAccumMissionMap( SA_PARAM_NN_VALID UGCGenesisMissionContext* context, const char* mapName )
{
	MissionMap* clone = StructCreate( parse_MissionMap );
	int it;
	clone->pchMapName = allocAddString( mapName );
	clone->bHideGotoString = true;

	for( it = 0; it != eaSize( &context->root_mission_accum->eaObjectiveMaps ); ++it ) {
		MissionMap* existing = context->root_mission_accum->eaObjectiveMaps[ it ];

		if( StructCompare( parse_MissionMap, existing, clone, 0, 0, 0 ) == 0 ) {
			// found it, nothing to add
			StructDestroy( parse_MissionMap, clone );
			return;
		}
	}

	eaPush( &context->root_mission_accum->eaObjectiveMaps, clone );
}


/// Fixup all the messages in the mission requirements, replacing the
/// key, description, and scope
void ugcGenesisMessageFillKeys( UGCGenesisMissionContext* context )
{
	int messageIt = 0;
	
	if( context->req_accum->params ) {
		ugcGenesisParamsMessageFillKeys( context, context->req_accum->params, &messageIt );
	}

	{
		int it;
		for( it = 0; it != eaSize( &context->req_accum->roomRequirements ); ++it ) {
			if( context->req_accum->roomRequirements[ it ]->params ) {
				ugcGenesisParamsMessageFillKeys( context, context->req_accum->roomRequirements[ it ]->params, &messageIt );
			}
		}
	}

	{
		int it;
		for( it = 0; it != eaSize( &context->req_accum->challengeRequirements ); ++it ) {
			UGCGenesisMissionChallengeRequirements* req = context->req_accum->challengeRequirements[ it ];
			if( req->params ) {
				ugcGenesisInstancedParamsMessageFillKeys( context, req->challengeName, req->params, &messageIt );
			}
			if( req->interactParams ) {
				ugcGenesisInteractParamsMessageFillKeys( context, req->challengeName, req->interactParams, &messageIt );
			}
			if( req->volumeParams ) {
				ugcGenesisParamsMessageFillKeys( context, req->volumeParams, &messageIt );
			}
		}
	}

	{
		char buffer[ MAX_PATH ];
		char zmapDirectory[ MAX_PATH ];
		
		strcpy( zmapDirectory, zmapInfoGetFilename( context->zmap_info ));
		getDirectoryName( zmapDirectory );
		sprintf( buffer, "%s/messages/%s", zmapDirectory, context->zone_mission->desc.pcName );
		context->req_accum->messageFilename = allocAddFilename( buffer );
	}
}


/// Fixup all the messages in ACCUM, replacing the key, description,
/// and scope.
void ugcGenesisMissionMessageFillKeys( MissionDef * accum, const char* root_mission_name )
{
	char missionName[256];
	char missionNameWithSubmission[256];
	char keyBuffer[256];
	char scopeBuffer[256];
	char nsPrefix[256];
	char ns[RESOURCE_NAME_MAX_SIZE], base[RESOURCE_NAME_MAX_SIZE];

	// Format for keys is Missiondef.<ROOT_MISSION_NAME>[::<SUBMISSION_NAME>].<KEY>
	// Format for scopse is Missiondef/<ROOT_MISSION_NAME>
	if( !root_mission_name )
	{
		if (resExtractNameSpace(accum->name, ns, base))
		{
			strcpy( missionName, base );
			strcpy( missionNameWithSubmission, base );
			sprintf( nsPrefix, "%s:", ns );
		}
		else
		{
			strcpy( missionName, accum->name );
			strcpy( missionNameWithSubmission, accum->name );
			strcpy( nsPrefix, "" );
		}
	}
	else
	{
		if (resExtractNameSpace(root_mission_name, ns, base))
		{
			strcpy( missionName, accum->name );
			sprintf( missionNameWithSubmission, "%s::%s", base, accum->name );
			sprintf( nsPrefix, "%s:", ns );
		}
		else
		{
			sprintf( missionName, "%s", accum->name );
			sprintf( missionNameWithSubmission, "%s::%s", root_mission_name, accum->name );
			strcpy( nsPrefix, "" );
		}
	}
	strchrReplace(missionName, '/', '_');
	strchrReplace(missionName, '\\', '_');
	strchrReplace(missionName, '.', '_');
	strchrReplace(missionNameWithSubmission, '/', '_');
	strchrReplace(missionNameWithSubmission, '\\', '_');
	strchrReplace(missionNameWithSubmission, '.', '_');
	{
		char* resFix = NULL;
		if( resFixName( missionName, &resFix )) {
			strcpy( missionName, resFix );
			estrDestroy( &resFix );
		}
		if( resFixName( missionNameWithSubmission, &resFix )) {
			strcpy( missionNameWithSubmission, resFix );
			estrDestroy( &resFix );
		}
	}

	sprintf( scopeBuffer, "Missiondef/%s", missionName );
	if( accum->displayNameMsg.pEditorCopy ) {
		sprintf( keyBuffer, "%sMissiondef.%s.%s", nsPrefix, missionNameWithSubmission, "Displayname" );
		langFixupMessageWithTerseKey( accum->displayNameMsg.pEditorCopy,
			MKP_MISSIONNAME,
						  keyBuffer,
						  "This is the display name for a MissionDef.",
						  scopeBuffer );

	}
	if( accum->uiStringMsg.pEditorCopy ) {
		sprintf( keyBuffer, "%sMissiondef.%s.%s", nsPrefix, missionNameWithSubmission, "Uistring" );
		langFixupMessageWithTerseKey( accum->uiStringMsg.pEditorCopy,
			MKP_MISSIONUISTR,
						  keyBuffer,
						  "This is the UI String for a MissionDef.",
						  scopeBuffer );
	}
	if( accum->summaryMsg.pEditorCopy ) {
		sprintf( keyBuffer, "%sMissiondef.%s.%s", nsPrefix, missionNameWithSubmission, "Summary" );
		langFixupMessageWithTerseKey( accum->summaryMsg.pEditorCopy,
			MKP_MISSIONSUMMARY,
						  keyBuffer,
						  "This is the Mission Summary string for a MissionDef.",
						  scopeBuffer );
	}
	if( accum->detailStringMsg.msg.pEditorCopy ) {
		sprintf( keyBuffer, "%sMissiondef.%s.%s", nsPrefix, missionNameWithSubmission, "Detailstring" );
		langFixupMessageWithTerseKey( accum->detailStringMsg.msg.pEditorCopy,
			MKP_MISSIONDETAIL,
						  keyBuffer,
						  "This is the detail string for a MissionDef.",
						  scopeBuffer );
	}
	if( accum->msgReturnStringMsg.pEditorCopy ) {
		sprintf( keyBuffer, "%sMissiondef.%s.%s", nsPrefix, missionNameWithSubmission, "Returnstring" );
		langFixupMessageWithTerseKey( accum->msgReturnStringMsg.pEditorCopy,
			MKP_MISSIONRETURN,
						  keyBuffer,
						  "This is a string on a MissionDef that describes how to turn in the Mission.",
						  scopeBuffer );
	}

	{
		int actionIt = 0;		
		int it;
		for( it = 0; it != eaSize( &accum->ppOnStartActions ); ++it, ++actionIt ) {
			if( SAFE_MEMBER( accum->ppOnStartActions[ it ]->pSendFloaterProperties, floaterMsg.pEditorCopy )) {
				sprintf( keyBuffer, "%sMissiondef.%s.Action_%d.Floater", nsPrefix,
						 missionNameWithSubmission, actionIt );
				sprintf( scopeBuffer, "Missiondef/%s", missionNameWithSubmission );
				langFixupMessage( accum->ppOnStartActions[ it ]->pSendFloaterProperties->floaterMsg.pEditorCopy,
								  keyBuffer,
								  "This is a parameter for a \"SendFloaterMsg\" action that occurs for a MissionDef.",
								  scopeBuffer );
			}
		}
		for( it = 0; it != eaSize( &accum->ppSuccessActions ); ++it, ++actionIt ) {
			if( SAFE_MEMBER( accum->ppSuccessActions[ it ]->pSendFloaterProperties, floaterMsg.pEditorCopy )) {
				sprintf( keyBuffer, "%sMissiondef.%s.Action_%d.Floater", nsPrefix,
						 missionNameWithSubmission, actionIt );
				sprintf( scopeBuffer, "Missiondef/%s", missionNameWithSubmission );
				langFixupMessage( accum->ppSuccessActions[ it ]->pSendFloaterProperties->floaterMsg.pEditorCopy,
								  keyBuffer,
								  "This is a parameter for a \"SendFloaterMsg\" action that occurs for a MissionDef.",
								  scopeBuffer );
			}
		}
		for( it = 0; it != eaSize( &accum->ppFailureActions ); ++it, ++actionIt ) {
			if( SAFE_MEMBER( accum->ppFailureActions[ it ]->pSendFloaterProperties, floaterMsg.pEditorCopy )) {
				sprintf( keyBuffer, "%sMissiondef.%s.Action_%d.Floater", nsPrefix,
						 missionNameWithSubmission, actionIt );
				sprintf( scopeBuffer, "Missiondef/%s", missionNameWithSubmission );
				langFixupMessage( accum->ppFailureActions[ it ]->pSendFloaterProperties->floaterMsg.pEditorCopy,
								  keyBuffer,
								  "This is a parameter for a \"SendFloaterMsg\" action that occurs for a MissionDef.",
								  scopeBuffer );
			}
		}
		for( it = 0; it != eaSize( &accum->ppInteractableOverrides ); ++it ) {
			InteractableOverride* override = accum->ppInteractableOverrides[ it ];

			sprintf( keyBuffer, "%sMissiondef.%s.%s",
					 nsPrefix, missionNameWithSubmission,
					 (override->pcMapName ? override->pcMapName : "NO_MAP") );
			sprintf( scopeBuffer, "Missiondef/%s", missionNameWithSubmission );
			interaction_FixupMessages( accum->ppInteractableOverrides[ it ]->pPropertyEntry, scopeBuffer, keyBuffer, override->pcInteractableName, it );
		}
		for( it = 0; it != eaSize( &accum->ppSpecialDialogOverrides ); ++it ) {
			SpecialDialogOverride* override = accum->ppSpecialDialogOverrides[ it ];
			int blockIt;
			int gameActionIt;
				
			if( !override->pSpecialDialog ) {
				continue;
			}

			sprintf( keyBuffer, "%sMissiondef.%s.SpecialDialog.%d",
					 nsPrefix, missionNameWithSubmission, it );
			sprintf( scopeBuffer, "Missiondef/%s", missionNameWithSubmission );
			langFixupMessage( override->pSpecialDialog->displayNameMesg.pEditorCopy, keyBuffer, "Description for contact special dialog action.", scopeBuffer );

			for( blockIt = 0; blockIt != eaSize( &override->pSpecialDialog->dialogBlock ); ++blockIt ) {
				sprintf( keyBuffer, "%sMissiondef.%s.SpecialDialog.%d.%d",
						 nsPrefix, missionNameWithSubmission, it, blockIt );
				sprintf( scopeBuffer, "Missiondef/%s", missionNameWithSubmission );
				langFixupMessage( override->pSpecialDialog->dialogBlock[ blockIt ]->displayTextMesg.msg.pEditorCopy, keyBuffer, "Mission-specific dialog for contact.", scopeBuffer );
				
			}

			for( actionIt = 0; actionIt != eaSize( &override->pSpecialDialog->dialogActions ); ++actionIt ) {
				SpecialDialogAction* dialogAction = override->pSpecialDialog->dialogActions[ actionIt ];

				if( dialogAction->displayNameMesg.pEditorCopy ) {
					sprintf( keyBuffer, "%sMissionDef.%s.Specialdialogaction.%d.%d",
							 nsPrefix, missionNameWithSubmission, it, actionIt );
					sprintf( scopeBuffer, "Missiondef/%s", missionNameWithSubmission );
					langFixupMessage( dialogAction->displayNameMesg.pEditorCopy,
									  keyBuffer,
									  "Mission-specific dialog for contact",
									  scopeBuffer );
				}

				for( gameActionIt = 0; gameActionIt != eaSize( &dialogAction->actionBlock.eaActions ); ++gameActionIt ) {
					WorldGameActionProperties* gameAction = dialogAction->actionBlock.eaActions[ gameActionIt ];
					if( gameAction->pSendNotificationProperties ) {
						sprintf( keyBuffer, "%sMissionDef.%s.Specialdialogname.%d.%d.Action_%d.Notify",
								 nsPrefix, missionNameWithSubmission, it, actionIt, gameActionIt );
						sprintf( scopeBuffer, "Missiondef/%s", missionNameWithSubmission );
						langFixupMessage( gameAction->pSendNotificationProperties->notifyMsg.msg.pEditorCopy,
										  keyBuffer,
										  "Dummy Message generated by Genesis so the the notification is sent to the client.",
										  scopeBuffer );
					}
				}
			}
		}
		for( it = 0; it != eaSize( &accum->ppImageMenuItemOverrides ); ++it ) {
			ImageMenuItemOverride* override = accum->ppImageMenuItemOverrides[ it ];

			if( !override->pImageMenuItem ) {
				continue;
			}

			sprintf( keyBuffer, "%sMissionDef.%s.ImageMenuItem.%d.Name",
					 nsPrefix, missionNameWithSubmission, it );
			sprintf( scopeBuffer, "Missiondef/%s", missionNameWithSubmission );
			langFixupMessage( override->pImageMenuItem->name.pEditorCopy,
							  keyBuffer,
							  "Dummy Message generated by Genesis",
							  scopeBuffer );
		}
	}

	{
		int it;
		for( it = 0; it != eaSize( &accum->subMissions ); ++it ) {
			ugcGenesisMissionMessageFillKeys(
					accum->subMissions[ it ],
					root_mission_name ? root_mission_name : accum->name );
		}
	}
}

/// Fixup all the messages in ACCUM, replacing the key, description
/// and scope.
void ugcGenesisContactMessageFillKeys( ContactDef* accum )
{
	char keyBuffer[256];
	char nsPrefix[256];
	char contactName[256];
	resExtractNameSpace(accum->name, nsPrefix, contactName);
	if(nsPrefix[0])
		strcat(nsPrefix, ":");

	{
		int dialogIt;
		int actionIt;
		int subDialogIt;
		int gameActionIt;
		for( dialogIt = 0; dialogIt != eaSize( &accum->specialDialog ); ++dialogIt ) {
			SpecialDialogBlock* dialogBlock = accum->specialDialog[ dialogIt ];

			if( dialogBlock->displayNameMesg.pEditorCopy ) {
				sprintf( keyBuffer, "%sContactdef.%s.Specialdialogname.%d",
						 nsPrefix, contactName, dialogIt );
				langFixupMessage( dialogBlock->displayNameMesg.pEditorCopy,
								  keyBuffer,
								  "Mission-specific dialog for contact",
								  "Contactdef" );
			}
			for( subDialogIt = 0; subDialogIt != eaSize( &dialogBlock->dialogBlock); subDialogIt++)
			{
				if( dialogBlock->dialogBlock[subDialogIt]->displayTextMesg.msg.pEditorCopy ) {
					if(subDialogIt == 0) {
						sprintf( keyBuffer, "%sContactdef.%s.Specialdialog.%d",
							nsPrefix, contactName, dialogIt);
					} else {
						sprintf( keyBuffer, "%sContactdef.%s.Specialdialog.%d.%d",
							 nsPrefix, contactName, dialogIt , subDialogIt);
					}
					langFixupMessage( dialogBlock->dialogBlock[subDialogIt]->displayTextMesg.msg.pEditorCopy,
									  keyBuffer,
									  "Mission-specific dialog for contact",
									  "Contactdef" );
				}
			}

			for( actionIt = 0; actionIt != eaSize( &dialogBlock->dialogActions ); ++actionIt ) {
				SpecialDialogAction* dialogAction = dialogBlock->dialogActions[ actionIt ];

				if( dialogAction->displayNameMesg.pEditorCopy ) {
					sprintf( keyBuffer, "%sContactdef.%s.Specialdialogaction.%d.%d",
							 nsPrefix, contactName, dialogIt, actionIt );
					langFixupMessage( dialogAction->displayNameMesg.pEditorCopy,
									  keyBuffer,
									  "Mission-specific dialog for contact",
									  "Contactdef" );
				}

				for( gameActionIt = 0; gameActionIt != eaSize( &dialogAction->actionBlock.eaActions ); ++gameActionIt ) {
					WorldGameActionProperties* gameAction = dialogAction->actionBlock.eaActions[ gameActionIt ];
					if( gameAction->pSendNotificationProperties ) {
						sprintf( keyBuffer, "%sContactdef.%s.Specialdialogname.%d.%d.Action_%d.Notify",
								 nsPrefix, contactName, dialogIt, actionIt, gameActionIt );
						langFixupMessage( gameAction->pSendNotificationProperties->notifyMsg.msg.pEditorCopy,
										  keyBuffer,
										  "Dummy Message generated by Genesis so the the notification is sent to the client.",
										  "Contactdef" );
					}
				}
			}
		}
	}
	{
		int offerIt;
		int dialogIt;

		for( offerIt = 0; offerIt != eaSize( &accum->offerList ); ++offerIt ) {
			ContactMissionOffer* offer = accum->offerList[ offerIt ];

			for( dialogIt = 0; dialogIt != eaSize( &offer->offerDialog ); ++dialogIt ) {
				DialogBlock* dialog = offer->offerDialog[ dialogIt ];
				if( dialog->displayTextMesg.msg.pEditorCopy ) {
					sprintf( keyBuffer, "%sContactdef.%s.Missionoffer.%d.Offer.%d",
							 nsPrefix, contactName, offerIt, dialogIt );
					langFixupMessage( dialog->displayTextMesg.msg.pEditorCopy,
									  keyBuffer,
									  "Contact's dialog when offering mission to the player",
									  "Contactdef" );
				}
			}
			for( dialogIt = 0; dialogIt != eaSize( &offer->inProgressDialog ); ++dialogIt ) {
				DialogBlock* dialog = offer->inProgressDialog[ dialogIt ];
				if( dialog->displayTextMesg.msg.pEditorCopy ) {
					sprintf( keyBuffer, "%sContactdef.%s.Missionoffer.%d.Inprogress.%d",
							 nsPrefix, contactName, offerIt, dialogIt );
					langFixupMessage( dialog->displayTextMesg.msg.pEditorCopy,
									  keyBuffer,
									  "Contact's dialog if player returns while mission is still in progress",
									  "Contactdef" );
				}
			}
			for( dialogIt = 0; dialogIt != eaSize( &offer->completedDialog ); ++dialogIt ) {
				DialogBlock* dialog = offer->completedDialog[ dialogIt ];
				if( dialog->displayTextMesg.msg.pEditorCopy ) {
					sprintf( keyBuffer, "%sContactdef.%s.Missionoffer.%d.Completeddialog.%d",
							 nsPrefix, contactName, offerIt, dialogIt );
					langFixupMessage( dialog->displayTextMesg.msg.pEditorCopy,
									  keyBuffer,
									  "Contact's dialog when player returns after completing mission",
									  "Contactdef" );
				}
			}
			for( dialogIt = 0; dialogIt != eaSize( &offer->failureDialog ); ++dialogIt ) {
				DialogBlock* dialog = offer->failureDialog[ dialogIt ];
				if( dialog->displayTextMesg.msg.pEditorCopy ) {
					sprintf( keyBuffer, "%sContactdef.%s.Missionoffer.%d.Failuredialog.%d",
							 nsPrefix, contactName, offerIt, dialogIt );
					langFixupMessage( dialog->displayTextMesg.msg.pEditorCopy,
									  keyBuffer,
									  "Contact's dialog when player returns after failing mission",
									  "Contactdef" );
				}
			}
		}
	}
}

/// Fixup all the messages in PARAMS, replacing the key, description
/// and scope.
void ugcGenesisParamsMessageFillKeys( UGCGenesisMissionContext* context, UGCGenesisProceduralObjectParams* params, int* messageIt )
{
	char keyBuffer[4096];
	char zmapName[RESOURCE_NAME_MAX_SIZE];
	const char* zmapPublicname = zmapInfoGetPublicName( context->zmap_info );
	int it;
	char scopeBuffer[256];

	strcpy(zmapName, zmapPublicname);
	strchrReplace(zmapName, '/', '_');
	strchrReplace(zmapName, '\\', '_');
	strchrReplace(zmapName, '.', '_');
			
	if( params->optionalaction_volume_properties ) {
		int entryIt;
		for( entryIt = 0; entryIt != eaSize( &params->optionalaction_volume_properties->entries ); ++entryIt ) {
			WorldOptionalActionVolumeEntry* entry = params->optionalaction_volume_properties->entries[ entryIt ];
			
			if( entry->display_name_msg.pEditorCopy ) {
				sprintf( keyBuffer, "%s.OptionalAction.Autogen_%s.%d", zmapName, context->zone_mission->desc.pcName, (*messageIt)++ );
				langFixupMessage( entry->display_name_msg.pEditorCopy,
								  keyBuffer,
								  "Optional action button text",
								  "OptionalAction" );
			}
		}
	}
	it = 0;
	if (params->interaction_properties)
	{
		FOR_EACH_IN_EARRAY(params->interaction_properties->eaEntries, WorldInteractionPropertyEntry, entry)
		{
			WorldTextInteractionProperties* textProperties = entry->pTextProperties;

			if( SAFE_MEMBER( textProperties, interactOptionText.pEditorCopy )) {
				sprintf( keyBuffer, "%s.Interactoptiontext%d.Autogen_%s.%d", zmapName, it, context->zone_mission->desc.pcName, (*messageIt)++ );
				sprintf( scopeBuffer, "Interactoptiontext%d", it );
				langFixupMessage( textProperties->interactOptionText.pEditorCopy,
								  keyBuffer,
								  "Interact text for a clickie",
								  scopeBuffer );
			}
			if( SAFE_MEMBER( textProperties, successConsoleText.pEditorCopy )) {
				sprintf( keyBuffer, "%s.Successconsoletext%d.Autogen_%s.%d", zmapName, it, context->zone_mission->desc.pcName, (*messageIt)++ );
				sprintf( scopeBuffer, "Successconsoletext%d", it );
				langFixupMessage( textProperties->successConsoleText.pEditorCopy,
								  keyBuffer,
								  "Success console text for a clickie",
								  scopeBuffer );
			}
			if( SAFE_MEMBER( textProperties, failureConsoleText.pEditorCopy )) {
				sprintf( keyBuffer, "%s.Failureconsoletext%d.Autogen_%s.%d", zmapName, it, context->zone_mission->desc.pcName, (*messageIt)++ );
				sprintf( scopeBuffer, "Failureconsoletext%d", it );
				langFixupMessage( textProperties->failureConsoleText.pEditorCopy,
								  keyBuffer,
								  "Failure console text for a clickie",
								  scopeBuffer );
			}
			it++;
		}
		FOR_EACH_END;
	}
}

/// Fixup all the messages in PARAMS, replacing the key, description
/// and scope.
void ugcGenesisInteractParamsMessageFillKeys( UGCGenesisMissionContext* context, const char* challengeName, UGCGenesisInteractObjectParams* params, int* messageIt )
{
	char keyBuffer[4096];
	char scopeBuffer[256];
	char zmapName[RESOURCE_NAME_MAX_SIZE];
	const char* zmapPublicname = zmapInfoGetPublicName( context->zmap_info );

	strcpy(zmapName, zmapPublicname);
	strchrReplace(zmapName, '/', '_');
	strchrReplace(zmapName, '\\', '_');
	strchrReplace(zmapName, '.', '_');

	if( params->displayNameMsg.pEditorCopy ) {
		sprintf( keyBuffer, "%s.Displaynamebasic.Autogen_%s", zmapName, context->zone_mission->desc.pcName );
		langFixupMessage( params->displayNameMsg.pEditorCopy,
						  keyBuffer,
						  NULL,
						  "Displaynamebasic" );
	}

	{
		int it;
		for( it = 0; it != eaSize( &params->eaInteractionEntries ); ++it ) {
			WorldInteractionPropertyEntry* entry = params->eaInteractionEntries[ it ];
			WorldTextInteractionProperties* textProperties = entry->pTextProperties;

			if( SAFE_MEMBER( textProperties, interactOptionText.pEditorCopy )) {
				sprintf( keyBuffer, "%s.Interactoptiontext%d.Autogen_%s.%d", zmapName, it, context->zone_mission->desc.pcName, (*messageIt)++ );
				sprintf( scopeBuffer, "Interactoptiontext%d", it );
				langFixupMessage( textProperties->interactOptionText.pEditorCopy,
								  keyBuffer,
								  "Interact text for a clickie",
								  scopeBuffer );
			}
			if( SAFE_MEMBER( textProperties, successConsoleText.pEditorCopy )) {
				sprintf( keyBuffer, "%s.Successconsoletext%d.Autogen_%s.%d", zmapName, it, context->zone_mission->desc.pcName, (*messageIt)++ );
				sprintf( scopeBuffer, "Successconsoletext%d", it );
				langFixupMessage( textProperties->successConsoleText.pEditorCopy,
								  keyBuffer,
								  "Success console text for a clickie",
								  scopeBuffer );
			}
			if( SAFE_MEMBER( textProperties, failureConsoleText.pEditorCopy )) {
				sprintf( keyBuffer, "%s.Failureconsoletext%d.Autogen_%s.%d", zmapName, it, context->zone_mission->desc.pcName, (*messageIt)++ );
				sprintf( scopeBuffer, "Failureconsoletext%d", it );
				langFixupMessage( textProperties->failureConsoleText.pEditorCopy,
								  keyBuffer,
								  "Failure console text for a clickie",
								  scopeBuffer );
			}
		}
	}
}

/// Fixup all the messages in PARAMS, replacing the key, description
/// and scope.
void ugcGenesisInstancedParamsMessageFillKeys( UGCGenesisMissionContext* context, const char* challengeName, UGCGenesisInstancedObjectParams* params, int* messageIt )
{
	char keyBuffer[4096];
	char scopeBuffer[256];
	char zmapName[RESOURCE_NAME_MAX_SIZE];
	const char* zmapPublicname = zmapInfoGetPublicName( context->zmap_info );

	strcpy(zmapName, zmapPublicname);
	strchrReplace(zmapName, '/', '_');
	strchrReplace(zmapName, '\\', '_');
	strchrReplace(zmapName, '.', '_');
	
	if (params->pContact)
	{
		sprintf( keyBuffer, "%s.Contact_Displayname.Autogen_%s.%d", zmapName, context->zone_mission->desc.pcName, (*messageIt)++ );
		sprintf( scopeBuffer, "Contact_Displayname");
		langFixupMessage( params->pContact->contactName.pEditorCopy,
							keyBuffer,
							"Contact name",
							scopeBuffer );
	}
	if(eaSize(&params->eaChildParams))
	{
		FOR_EACH_IN_EARRAY(params->eaChildParams, UGCGenesisInstancedChildParams, child)
		{
			if(!child)
				continue;

			sprintf( keyBuffer, "%s.Actor_Crittergroup_Displayname.Autogen_%s.%d", zmapName, context->zone_mission->desc.pcName, (*messageIt)++ );
			sprintf( scopeBuffer, "Actor Displayname Crittergroup");
			langFixupMessage( child->critterGroupDisplayNameMsg.pEditorCopy,
				keyBuffer,
				"Actor Critter Group Display Name",
				scopeBuffer );

			sprintf( keyBuffer, "%s.Actor_Displayname.Autogen_%s.%d", zmapName, context->zone_mission->desc.pcName, (*messageIt)++ );
			sprintf( scopeBuffer, "Actor Displayname");
			langFixupMessage( child->displayNameMsg.pEditorCopy,
								keyBuffer,
								"Actor Display Name",
								scopeBuffer );
		}
		FOR_EACH_END
	}
}

/// Find the challenge for the challenge named CHALLENGE-NAME.
UGCGenesisMissionZoneChallenge* ugcGenesisFindZoneChallenge( UGCGenesisZoneMapData* zmap_data, UGCGenesisZoneMission* zone_mission, const char* challenge_name )
{
	return ugcGenesisFindZoneChallengeRaw( zmap_data, zone_mission, NULL, challenge_name );
}

/// Find the challenge for the challenge named CHALLENGE-NAME.
///
/// This function exists for TomY ENCOUNTER_HACK.
UGCGenesisMissionZoneChallenge* ugcGenesisFindZoneChallengeRaw( UGCGenesisZoneMapData* zmap_data, UGCGenesisZoneMission* zone_mission, UGCGenesisMissionZoneChallenge** override_challenges, const char* challenge_name )
{
	if( override_challenges ) {
		int it;
		for( it = 0; it != eaSize( &override_challenges ); ++it ) {
			UGCGenesisMissionZoneChallenge* challenge = override_challenges[ it ];
			if( stricmp( challenge->pcName, challenge_name ) == 0 ) {
				return challenge;
			}
		}
	} else {
		int it;
		for( it = 0; it != eaSize( &zone_mission->eaChallenges ); ++it ) {
			UGCGenesisMissionZoneChallenge* challenge = zone_mission->eaChallenges[ it ];
			if( stricmp( challenge->pcName, challenge_name ) == 0 ) {
				return challenge;
			}
		}
		if (zmap_data)
		{
			for( it = 0; it != eaSize( &zmap_data->genesis_shared_challenges ); ++it ) {
				UGCGenesisMissionZoneChallenge* challenge = zmap_data->genesis_shared_challenges[ it ];
				if( stricmp( challenge->pcName, challenge_name ) == 0 ) {
					return challenge;
				}
			}
		}
	}
	
	return NULL;
}

/// Find a mission prompt with the specified name.
UGCGenesisMissionPrompt* ugcGenesisTransmogrifyFindPrompt( UGCGenesisTransmogrifyMissionContext* context, char* prompt_name )
{
	if( context->mission_desc ) {
		int it;
		for( it = 0; it != eaSize( &context->mission_desc->zoneDesc.eaPrompts ); ++it ) {
			UGCGenesisMissionPrompt* prompt = context->mission_desc->zoneDesc.eaPrompts[ it ];
			if( stricmp( prompt->pcName, prompt_name ) == 0 ) {
				return prompt;
			}
		}
	}

	return NULL;
}

/// Find a mission prompt with the specified name
UGCGenesisMissionPrompt* ugcGenesisFindPrompt( UGCGenesisMissionContext* context, char* prompt_name )
{
	int it;
	for( it = 0; it != eaSize( &context->zone_mission->desc.eaPrompts ); ++it ) {
		UGCGenesisMissionPrompt* prompt = context->zone_mission->desc.eaPrompts[ it ];
		if( stricmp( prompt->pcName, prompt_name ) == 0 ) {
			return prompt;
		}
	}
	for( it = 0; it != eaSize( &context->extra_prompts ); ++it ) {
		UGCGenesisMissionPrompt* prompt = context->extra_prompts[ it ];
		if( stricmp( prompt->pcName, prompt_name ) == 0 ) {
			return prompt;
		}
	}

	return NULL;
}

UGCGenesisMissionPromptBlock* ugcGenesisFindPromptBlock( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, char* block_name )
{
	if( prompt ) {
		if( nullStr( block_name )) {
			return &prompt->sPrimaryBlock;
		} else {
			int it;
			for( it = 0; it != eaSize( &prompt->namedBlocks ); ++it ) {
				UGCGenesisMissionPromptBlock* block = prompt->namedBlocks[ it ];
				if( stricmp( block->name, block_name ) == 0 ) {
					return block;
				}
			}
		}
	}

	return NULL;
}

static bool ugcGenesisPromptBlockHasComplete( UGCGenesisMissionPromptBlock* block )
{
	int it;
	for( it = 0; it != eaSize( &block->eaActions ); ++it ) {
		if( nullStr( block->eaActions[ it ]->pcNextBlockName )) {
			return true;
		}
	}
	if( eaSize( &block->eaActions ) == 0 ) {
		return true;
	}

	return false;
}

char** ugcGenesisPromptBlockNames( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, bool isComplete )
{
	char** accum = NULL;
	int it;

	if( isComplete ) {
		if( ugcGenesisPromptBlockHasComplete( &prompt->sPrimaryBlock )) {
			eaPush( &accum, NULL );
		}
		for( it = 0; it != eaSize( &prompt->namedBlocks ); ++it ) {
			UGCGenesisMissionPromptBlock* block = prompt->namedBlocks[ it ];

			if( block->name && ugcGenesisPromptBlockHasComplete( block )) {
				eaPush( &accum, prompt->namedBlocks[ it ]->name );
			}
		}
	} else {
		eaPush( &accum, NULL );
	}
	
	return accum;
}

char* ugcGenesisSpecialDialogBlockNameTemp( const char* promptName, const char* blockName )
{
	static char buffer[ 512 ];

	if( blockName ) {
		sprintf( buffer, "%s_%s", promptName, blockName );
	} else {
		sprintf( buffer, "%s", promptName );
	}

	return buffer;
}

static char* StructAllocStringFunc( const char* str ) { return StructAllocString( str ); }

/// Return a cannonical copy of a ZoneChallenge for CHALLENGE.
UGCGenesisMissionZoneChallenge* ugcGenesisTransmogrifyChallenge( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisMissionChallenge* challenge )
{
	char fullChallengeName[ 256 ];
	if( context->mission_desc ) {
		sprintf( fullChallengeName, "%s_%s",
				 context->mission_desc->zoneDesc.pcName, challenge->pcName );
	} else {
		sprintf( fullChallengeName, "Shared_%s", challenge->pcName );
	}
	
	{
		UGCGenesisMissionZoneChallenge* zoneChallenge = StructCreate( parse_UGCGenesisMissionZoneChallenge );
		zoneChallenge->pcName = StructAllocString( fullChallengeName );
		zoneChallenge->pcLayoutName = StructAllocString(challenge->pcLayoutName);
		zoneChallenge->eType = challenge->eType;
		zoneChallenge->pSourceContext = ugcMakeErrorContextChallenge( challenge->pcName, SAFE_MEMBER( context->mission_desc, zoneDesc.pcName ), challenge->pcLayoutName );
		zoneChallenge->bForceNamedObject = challenge->bForceNamedObject;
		zoneChallenge->iNumToComplete = challenge->iCount;

		eaCopyStructs(&challenge->eaChildren, &zoneChallenge->eaChildren, parse_UGCGenesisPlacementChildParams);
		zoneChallenge->bChildrenAreGroupDefs = challenge->bChildrenAreGroupDefs;

		eaCopyStructs(&challenge->eaTraps, &zoneChallenge->eaTraps, parse_UGCGenesisMissionTrap);
		zoneChallenge->pcTrapObjective = StructAllocString(challenge->pcTrapObjective);

		StructCopyAll( parse_UGCGenesisWhen, &challenge->spawnWhen, &zoneChallenge->spawnWhen );
		StructCopyAll( parse_UGCGenesisWhen, &challenge->clickieVisibleWhen, &zoneChallenge->clickieVisibleWhen );
		zoneChallenge->succeedCheckedAttrib = StructClone( parse_UGCCheckedAttrib, challenge->succeedCheckedAttrib );
		{
			int it;
			for( it = eaSize( &zoneChallenge->spawnWhen.eaChallengeNames ) - 1; it >= 0; --it ) {
				char** challengeName = &zoneChallenge->spawnWhen.eaChallengeNames[ it ];
				bool isShared;

				if( !ugcGenesisFindChallenge( context->map_desc, context->mission_desc, *challengeName, &isShared )) {
					ugcRaiseError( UGC_ERROR, zoneChallenge->pSourceContext,
									   "Challenge references challenge "
									   "%s in spawn when, but can not find it.  (If %s is "
									   "shared, then it can only reference other shared "
									   "challenges.)",
									   *challengeName, challenge->pcName );
					StructFreeString( *challengeName );
					eaRemove( &zoneChallenge->spawnWhen.eaChallengeNames, it );
				} else {
					ugcGenesisChallengeNameFixup( context, challengeName );
				}
			}

			if( zoneChallenge->spawnWhen.pcPromptChallengeName ) {
				ugcGenesisChallengeNameFixup( context, &zoneChallenge->spawnWhen.pcPromptChallengeName );
			}
		}
		zoneChallenge->pClickie = StructClone( parse_UGCGenesisMissionChallengeClickie, challenge->pClickie );
		zoneChallenge->pContact = StructClone( parse_UGCGenesisContactParams, challenge->pContact );
		zoneChallenge->bIsVolume = (challenge->pVolume != NULL);

		return zoneChallenge;
	}
}

/// Create requirements for each challenge. Does not actually place a
/// challenge since that is handled by the GenerateGeometry step.
void ugcGenesisCreateChallenge( UGCGenesisMissionContext* context, UGCGenesisMissionZoneChallenge* challenge )
{
	bool is_spawn_object = (challenge->eType == GenesisChallenge_Encounter2 || challenge->eType == GenesisChallenge_Contact);

	int roomIt;
	for( roomIt = 0; roomIt != eaSize( &challenge->spawnWhen.eaRooms ); ++roomIt ) {
		UGCGenesisProceduralObjectParams* params = ugcGenesisInternRoomRequirementParams( context, challenge->spawnWhen.eaRooms[ roomIt ]->layoutName, challenge->spawnWhen.eaRooms[ roomIt ]->roomName );
		ugcGenesisProceduralObjectSetEventVolume( params );
	}

	// Generate expressions
	if( challenge->spawnWhen.type != UGCGenesisWhen_MapStart || (is_spawn_object && context->mission_num >= 0) ) {
		if( challenge->eType == GenesisChallenge_Encounter ) {
			// nothing to do -- handled via TomY ENCOUNTER_HACK
			return;
		} else {
			UGCGenesisProceduralEncounterProperties pep = { 0 };

			pep.encounter_name = NULL;
			pep.genesis_mission_name = context->zone_mission->desc.pcName;
			pep.genesis_open_mission = (context->zone_mission->desc.generationType != UGCGenesisMissionGenerationType_PlayerMission);
			pep.genesis_mission_num = context->mission_num;
			StructCopyAll(parse_UGCGenesisWhen, &challenge->spawnWhen, &pep.spawn_when);
			{
				int it = 0;
				for( it = 0; it != eaSize( &pep.spawn_when.eaChallengeNames ); ++it ) {
					UGCGenesisMissionZoneChallenge* zoneChallenge = ugcGenesisFindZoneChallenge( context->genesis_data, context->zone_mission, challenge->spawnWhen.eaChallengeNames[ it ]);
					if( zoneChallenge ) {
						eaPush( &pep.when_challenges, zoneChallenge );
					}
				}
			}

			if( is_spawn_object ) {
				UGCGenesisInstancedObjectParams* params = ugcGenesisInternChallengeRequirementParams( context, challenge->pcName );
					
				// This should only happen if there's a duplicate
				// challenge name, but that should have been
				// checked for earlier.
				assert( !params->encounterSpawnCond );
				
				params->encounterSpawnCond = ugcGenesisCreateEncounterSpawnCond( context, zmapInfoGetPublicName( context->zmap_info ), &pep);
				params->encounterDespawnCond = ugcGenesisCreateEncounterDespawnCond( context, zmapInfoGetPublicName( context->zmap_info ), &pep );
			} else {
				UGCGenesisInteractObjectParams* params = ugcGenesisInternInteractRequirementParams( context, challenge->pcName );
				assert( !params->interactWhenCond );
				params->interactWhenCond = ugcGenesisCreateChallengeSpawnCond( context, zmapInfoGetPublicName( context->zmap_info ), &pep);
			}

			eaDestroy( &pep.when_challenges );
		}
	}
	if( challenge->clickieVisibleWhen.type != UGCGenesisWhen_MapStart || challenge->clickieVisibleWhen.checkedAttrib  ) {
		Expression* expr = NULL;
		UGCGenesisInteractObjectParams* params = ugcGenesisInternInteractRequirementParams( context, challenge->pcName );

		{
			UGCGenesisProceduralEncounterProperties pep = { 0 };

			pep.encounter_name = NULL;
			pep.genesis_mission_name = context->zone_mission->desc.pcName;
			pep.genesis_open_mission = (context->zone_mission->desc.generationType != UGCGenesisMissionGenerationType_PlayerMission);
			pep.genesis_mission_num = context->mission_num;
			StructCopyAll(parse_UGCGenesisWhen, &challenge->clickieVisibleWhen, &pep.spawn_when);
			{
				int it = 0;
				for( it = 0; it != eaSize( &pep.spawn_when.eaChallengeNames ); ++it ) {
					UGCGenesisMissionZoneChallenge* zoneChallenge = ugcGenesisFindZoneChallenge( context->genesis_data, context->zone_mission, challenge->spawnWhen.eaChallengeNames[ it ]);
					if( zoneChallenge ) {
						eaPush( &pep.when_challenges, zoneChallenge );
					}
				}
			}

			expr = ugcGenesisCreateChallengeSpawnCond( context, zmapInfoGetPublicName( context->zmap_info ), &pep);
			eaDestroy( &pep.when_challenges );
		}

		// This should only happen if there's a duplicate
		// challenge name, but that should have been checked for
		// earlier.
		assert( !params->clickieVisibleWhenCond );
		params->clickieVisibleWhenCond = expr;

		if( challenge->clickieVisibleWhen.checkedAttrib ) {
			params->clickieVisibleWhenCondPerEnt = true;
		}
	}

	if( challenge->succeedCheckedAttrib ) {
		char* exprText = ugcGenesisCheckedAttribText( context, challenge->succeedCheckedAttrib, ugcMakeTempErrorContextChallenge( challenge->pcName, SAFE_MEMBER2( context, zone_mission, desc.pcName ), NULL ), "SucceedCheckedAttrib", false );

		if( exprText && challenge->eType == GenesisChallenge_Clickie ) {
			UGCGenesisInteractObjectParams* params = ugcGenesisInternInteractRequirementParams( context, challenge->pcName );
			params->succeedWhenCond = exprCreateFromString( exprText, NULL );
		} else if( exprText && challenge->bIsVolume ) {
			UGCGenesisProceduralObjectParams* params = ugcGenesisInternVolumeRequirementParams( context, challenge->pcName );
			ugcGenesisProceduralObjectSetEventVolume( params );
			params->event_volume_properties->entered_cond = exprCreateFromString( exprText, NULL );
		} else {
			ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextChallenge( challenge->pcName, SAFE_MEMBER2( context, zone_mission, desc.pcName ), NULL ),
								  "SucceedSkillCheck is set, but challenge is not a clicky or volume." );
		}

		estrDestroy( &exprText );
	}

	if( challenge->pClickie ) {
		UGCGenesisInteractObjectParams* params = ugcGenesisInternInteractRequirementParams( context, challenge->pcName );
		ugcGenesisCreateMessage( context, &params->displayNameMsg, challenge->pClickie->strVisibleName );
	}

	if( challenge->pClickie && IS_HANDLE_ACTIVE( challenge->pClickie->hInteractionDef )) {
		UGCGenesisInteractObjectParams* params = ugcGenesisInternInteractRequirementParams( context, challenge->pcName );
		params->bIsUGCDoor = challenge->pClickie->bIsUGCDoor;
		eaPush( &params->eaInteractionEntries, ugcGenesisClickieMakeInteractionEntry( context, ugcMakeTempErrorContextChallenge( challenge->pcName, SAFE_MEMBER2( context, zone_mission, desc.pcName ), NULL ), challenge->pClickie, challenge->succeedCheckedAttrib ));
	}
	
	if (challenge->pContact)
	{
		UGCGenesisInstancedObjectParams* params = ugcGenesisInternChallengeRequirementParams( context, challenge->pcName );
		if (!params->pContact)
		{
			params->pContact = StructCreate(parse_UGCGenesisMissionContactRequirements);
		}
		ugcGenesisCreateMessage( context, &params->pContact->contactName, challenge->pContact->pcContactName);
		COPY_HANDLE(params->pContact->hCostume, challenge->pContact->hCostume);
	}

	if(eaSize(&challenge->eaChildren))
	{
		UGCGenesisInstancedObjectParams* params = ugcGenesisInternChallengeRequirementParams( context, challenge->pcName );

		FOR_EACH_IN_EARRAY_FORWARDS(challenge->eaChildren, UGCGenesisPlacementChildParams, child) {
			UGCGenesisInstancedChildParams *instanced = NULL;
			instanced = eaGet(&params->eaChildParams, FOR_EACH_IDX(0, child));
			
			if(!instanced)
				eaSet(&params->eaChildParams, instanced = StructCreate(parse_UGCGenesisInstancedChildParams), FOR_EACH_IDX(0, child));

			if(!nullStr(child->actor_params.pcActorCritterGroupName))
				ugcGenesisCreateMessage(context, &instanced->critterGroupDisplayNameMsg, child->actor_params.pcActorCritterGroupName);

			if(!nullStr(child->actor_params.pcActorName))
				ugcGenesisCreateMessage(context, &instanced->displayNameMsg, child->actor_params.pcActorName);
		} FOR_EACH_END;
		params->bChildParamsAreGroupDefs = challenge->bChildrenAreGroupDefs;
	}

	{
		char *volume_entered_expr = NULL;
		char *clicky_expr = NULL;
		FOR_EACH_IN_EARRAY(challenge->eaTraps, UGCGenesisMissionTrap, trap)
		{
			if (trap->bOnVolumeEntered)
			{
				if (volume_entered_expr)
					estrAppend2(&volume_entered_expr, ";\n");
				estrConcatf(&volume_entered_expr, "ActivatePowerPointToPoint(\"%s\",\"default\",\"Namedpoint:%s_%s\",\"Namedpoint:%s_%s\")", trap->pcPowerName, context->zone_mission->desc.pcName, trap->pcEmitterChallenge, context->zone_mission->desc.pcName, trap->pcTargetChallenge);
			}
			else
			{
				if (clicky_expr)
					estrAppend2(&clicky_expr, ";\n");
				estrConcatf(&clicky_expr, "ActivatePowerPointToPoint(\"%s\",\"default\",\"Namedpoint:%s_%s\",\"Namedpoint:%s_%s\")", trap->pcPowerName, context->zone_mission->desc.pcName, trap->pcEmitterChallenge, context->zone_mission->desc.pcName, trap->pcTargetChallenge);
			}
		}
		FOR_EACH_END;

		if (volume_entered_expr)
		{
			UGCGenesisProceduralObjectParams* params = ugcGenesisInternVolumeRequirementParams( context, challenge->pcName );
			ugcGenesisProceduralObjectSetActionVolume(params);

			params->action_volume_properties->entered_action = exprCreateFromString(volume_entered_expr, NULL);
			{
				char condition_str[1024];
				if (challenge->pcTrapObjective)
					sprintf(condition_str, "OpenMissionStateInProgress(\"%s::%s\") and ClickableGetVisibleChild(\"%s_TRAP\") = 0", ugcGenesisMissionName( context, false ), challenge->pcTrapObjective, challenge->pcName);
				else
					sprintf(condition_str, "ClickableGetVisibleChild(\"%s_TRAP\") = 0", challenge->pcName);
				params->action_volume_properties->entered_action_cond = exprCreateFromString(condition_str, NULL);
			}

			estrDestroy(&volume_entered_expr);
		}
		if (clicky_expr)
		{
			UGCGenesisInteractObjectParams* params = ugcGenesisInternInteractRequirementParams( context, challenge->pcName );
			if (eaSize(&params->eaInteractionEntries) > 0)
			{
				WorldInteractionPropertyEntry *entry = params->eaInteractionEntries[0];
				if (!entry->pActionProperties)
					entry->pActionProperties = StructCreate(parse_WorldActionInteractionProperties);
				entry->pActionProperties->pSuccessExpr = exprCreateFromString(clicky_expr, NULL);
			}

			estrDestroy(&clicky_expr);
		}
	}
}

/// Fixup CHALLENGE-NAME for the transmogrify pass.
void ugcGenesisChallengeNameFixup( UGCGenesisTransmogrifyMissionContext* context, char** challengeName )
{
	char* oldChallengeName = *challengeName;
	char newChallengeName[ 256 ];
	bool challengeIsShared;
	ugcGenesisFindChallenge( context->map_desc, context->mission_desc, *challengeName, &challengeIsShared );
	
	if( challengeIsShared ) {
		sprintf( newChallengeName, "Shared_%s", oldChallengeName );
	} else {
		sprintf( newChallengeName, "%s_%s", context->mission_desc->zoneDesc.pcName, oldChallengeName );
	}

	*challengeName = StructAllocString( newChallengeName );
	StructFreeString( oldChallengeName );
}

#define ugcGenesisMissionPortalSpawnTargetName(buffer,portal,isTargetStart,mission_name,layout_name) ugcGenesisMissionPortalSpawnTargetName_s(SAFESTR(buffer),portal,isTargetStart,mission_name,layout_name)
static const char* ugcGenesisMissionPortalSpawnTargetName_s(char* buffer, int buffer_size, UGCGenesisMissionPortal* portal, bool isTargetStart, const char* mission_name, const char *layout_name)
{
	const char* layoutName;
	const char* roomName;
	const char* spawnName;
	
	if( isTargetStart ) {
		layoutName = portal->pcStartLayout;
		roomName = portal->pcStartRoom;
		spawnName = portal->pcStartSpawn;
	} else {
		if( portal->eType == UGCGenesisMissionPortal_OneWayOutOfMap ) {
			layoutName = NULL;
			roomName = NULL;
			spawnName = portal->pcEndRoom;
		} else {
			if( portal->eType == UGCGenesisMissionPortal_BetweenLayouts ) {
				layoutName = portal->pcEndLayout;
			} else {
				layoutName = portal->pcStartLayout;
			}
			roomName = portal->pcEndRoom;
			spawnName = portal->pcEndSpawn;
		}
	}

	if( spawnName ) {
		return spawnName;
	} else {
		if(portal->eUseType == UGCGenesisMissionPortal_Door) {
			if(isTargetStart && portal->pcStartDoor && portal->pcStartDoor[0])
				strcpy_s(SAFESTR2(buffer), ugcIntLayoutDoorGetSpawnLogicalName(roomName, portal->pcStartDoor, layout_name));
			else if (!isTargetStart && portal->pcEndDoor && portal->pcEndDoor[0])
				strcpy_s(SAFESTR2(buffer), ugcIntLayoutDoorGetSpawnLogicalName(roomName, portal->pcEndDoor, layout_name));
			else
				sprintf_s(SAFESTR2(buffer), "Door_%s_%s_%s_%s_%s", mission_name, portal->pcName, (isTargetStart ? "Start": "End"), roomName, layout_name );
		} else {
			sprintf_s(SAFESTR2(buffer), "%s_%s", layoutName, roomName );
		}
		return buffer;
	}
}

/// Create a PORTAL between two rooms.
///
/// If IS-REVERSED is set, then go from end to start, otherwise go
/// from start to end.
void ugcGenesisCreatePortal( UGCGenesisMissionContext* context, UGCGenesisMissionPortal* portal, bool isReversed )
{
	const char* missionName = context->zone_mission->desc.pcName;
	const char* startLayout;
	const char* endLayout;
	const char* startRoom;
	const char* endRoomPoint;
	const char* endZmap;
	const char* warpText;
	const char* startClickable = NULL;
	char endRoomPointBuffer[ 512 ];
	bool startVolume = false;

	if( !isReversed ) {
		startLayout = portal->pcStartLayout;
		if( portal->eType == UGCGenesisMissionPortal_BetweenLayouts ) {
			endLayout = portal->pcEndLayout;
		} else {
			endLayout = portal->pcStartLayout;
		}
		endRoomPoint = ugcGenesisMissionPortalSpawnTargetName( endRoomPointBuffer, portal, isReversed, missionName, endLayout );
		endZmap = portal->pcEndZmap;
		warpText = portal->pcWarpToEndText;
		startVolume = portal->bStartUseVolume;
		startClickable = portal->pcStartClickable;
		startRoom = portal->pcStartRoom;
	} else {
		if( portal->eType == UGCGenesisMissionPortal_BetweenLayouts ) {
			startLayout = portal->pcEndLayout;
		} else {
			startLayout = portal->pcStartLayout;
		}
		endLayout = portal->pcStartLayout;
		endRoomPoint = ugcGenesisMissionPortalSpawnTargetName( endRoomPointBuffer, portal, isReversed, missionName, endLayout );
		endZmap = NULL;
		warpText = portal->pcWarpToStartText;
		startVolume = portal->bEndUseVolume;
		startClickable = portal->pcEndClickable;
		startRoom = portal->pcEndRoom;
	}

	if(portal->eUseType == UGCGenesisMissionPortal_Door) {
		UGCGenesisMissionRoomRequirements *roomReq = ugcGenesisInternRoomRequirements( context, startLayout, startRoom );
		UGCGenesisMissionDoorRequirements *newDoor = StructCreate(parse_UGCGenesisMissionDoorRequirements);
		char newDoorNameBuffer[ 512 ];

		if(isReversed && portal->pcEndDoor && portal->pcEndDoor[0])
			strcpy(newDoorNameBuffer, portal->pcEndDoor);
		else if (!isReversed && portal->pcStartDoor && portal->pcStartDoor[0])
			strcpy(newDoorNameBuffer, portal->pcStartDoor);
		else
			sprintf(newDoorNameBuffer, "%s_%s_%s", missionName, portal->pcName, (isReversed ? "End" : "Start"));
		newDoor->doorName = StructAllocString(newDoorNameBuffer);
		eaPush(&roomReq->doors, newDoor);

		startClickable = ugcIntLayoutDoorGetClickyLogicalName(startRoom, newDoor->doorName, startLayout);
	}

	if (startClickable)
	{
		WorldInteractionPropertyEntry* entry;
		if (startVolume)
			entry = ugcGenesisCreateInteractableChallengeVolumeRequirement( context, startClickable );
		else
			entry = ugcGenesisCreateInteractableChallengeRequirement( context, startClickable );

		entry->pTextProperties = StructCreate(parse_WorldTextInteractionProperties);
		ugcGenesisCreateMessage( context, &entry->pTextProperties->interactOptionText, warpText );

		entry->pcInteractionClass = allocAddString( "DOOR" );
		entry->pDoorProperties = StructCreate( parse_WorldDoorInteractionProperties );
		StructReset( parse_WorldVariableDef, &entry->pDoorProperties->doorDest );
		entry->pDoorProperties->doorDest.eType = WVAR_MAP_POINT;
		entry->pDoorProperties->doorDest.eDefaultType = WVARDEF_SPECIFY_DEFAULT;
		entry->pDoorProperties->doorDest.pSpecificValue = StructCreate( parse_WorldVariable );
		entry->pDoorProperties->doorDest.pSpecificValue->eType = WVAR_MAP_POINT;
		entry->pDoorProperties->doorDest.pSpecificValue->pcZoneMap = StructAllocString( endZmap );
		entry->pDoorProperties->doorDest.pSpecificValue->pcStringVal = StructAllocString( endRoomPoint );
		eaCopyStructs( &portal->eaEndVariables, &entry->pDoorProperties->eaVariableDefs, parse_WorldVariableDef );

//		COPY_HANDLE( entry->pDoorProperties->hTransSequence, startDesc->hExitTransitionOverride );
	}
	else
	{
		UGCGenesisProceduralObjectParams* params = ugcGenesisInternRoomRequirementParams( context, startLayout, startRoom );
		WorldOptionalActionVolumeEntry* entry = StructCreate( parse_WorldOptionalActionVolumeEntry );
		WorldGameActionProperties* actionProps = StructCreate( parse_WorldGameActionProperties );
		UGCRuntimeErrorContext* debugContext = ugcMakeTempErrorContextPortal( portal->pcName, SAFE_MEMBER( context->zone_mission, desc.pcName ), portal->pcStartLayout );
		ugcGenesisProceduralObjectSetOptionalActionVolume( params );

		actionProps->eActionType = WorldGameActionType_Warp;
		actionProps->pWarpProperties = StructCreate( parse_WorldWarpActionProperties );
		eaPush( &entry->actions.eaActions, actionProps );


		{
			char* whenText = ugcGenesisWhenExprText( context, &portal->when, debugContext, "When", false );
			if( whenText ) {
				entry->visible_cond = exprCreateFromString( whenText, NULL );
			}
		}
				
		ugcGenesisCreateMessage( context, &entry->display_name_msg, warpText );
		entry->category_name = allocAddString( "Warp" );
				
		actionProps->pWarpProperties->warpDest.eType = WVAR_MAP_POINT;
		actionProps->pWarpProperties->warpDest.eDefaultType = WVARDEF_SPECIFY_DEFAULT;
		actionProps->pWarpProperties->warpDest.pSpecificValue = StructCreate( parse_WorldVariable );
		actionProps->pWarpProperties->warpDest.pSpecificValue->eType = WVAR_MAP_POINT;
		actionProps->pWarpProperties->warpDest.pSpecificValue->pcZoneMap = StructAllocString( endZmap );
		actionProps->pWarpProperties->warpDest.pSpecificValue->pcStringVal = StructAllocString( endRoomPoint );
		eaCopyStructs( &portal->eaEndVariables, &actionProps->pWarpProperties->eaVariableDefs, parse_WorldVariableDef );

		eaPush( &params->optionalaction_volume_properties->entries, entry );
	}
}

/// Create a global expression that corresponds to WHEN.
///
/// Don't call this if you are going to put the expression on a
/// mission.  For that you need to call
/// ugcGenesisWhenMissionExprTextAndEvents()
char* ugcGenesisWhenExprText( UGCGenesisMissionContext* context, UGCGenesisWhen* when, UGCRuntimeErrorContext* debugContext, const char* debugFieldName, bool isEncounter )
{
	return ugcGenesisWhenExprTextRaw( context, NULL, -1, NULL, NULL, when, debugContext, debugFieldName, isEncounter );
}

/// Create a global expression that corresponds to WHEN.
///
/// This exists to support TomY ENCOUNTER_HACK.  The EncounterHacks
/// can pass in via OVERRIDE-* the data usually stored in CONTEXT.
char* ugcGenesisWhenExprTextRaw( UGCGenesisMissionContext* context, const char* overrideZmapName, UGCGenesisMissionGenerationType overrideGenerationType, const char* overrideMissionName, UGCGenesisMissionZoneChallenge** overrideChallenges,
								 UGCGenesisWhen* when, UGCRuntimeErrorContext* debugContext, const char* debugFieldName, bool isEncounter )
{
	char* accum = NULL;
	const char* missionName;
	const char* playerMissionName;
	const char* zmapName;
	const char* shortMissionName;
	UGCGenesisMissionGenerationType generationType;
	
	// If OVERRIDE-GENERATION-TYPE is set, that will override the type
	// in CONTEXT.
	if( overrideGenerationType != -1 ) {
		generationType = overrideGenerationType;
	} else {
		assert( context );
		generationType = context->zone_mission->desc.generationType;
	}

	// If OVERRIDE-ZMAP-NAME is set, that will override the name in
	// CONTEXT.
	if( overrideZmapName ) {
		zmapName = overrideZmapName;
	} else {
		assert( context );
		zmapName = context->zmap_info ? zmapInfoGetPublicName( context->zmap_info ) : NULL;
	}
	
	// If OVERRIDE-MISSION-NAME is set, that will override the mission
	// name in CONTEXT.
	if( overrideMissionName ) {
		shortMissionName = overrideMissionName;
		missionName = ugcGenesisMissionNameRaw( zmapName, overrideMissionName, (generationType != UGCGenesisMissionGenerationType_PlayerMission) );
		playerMissionName = ugcGenesisMissionNameRaw( zmapName, overrideMissionName, false );
	} else {
		assert( context );
		shortMissionName = context->zone_mission->desc.pcName;
		missionName = ugcGenesisMissionName( context, false );
		playerMissionName = ugcGenesisMissionName( context, true );
	}

	/// Okay, all overrides are finished

	switch( when->type ) {
		case UGCGenesisWhen_MapStart:
			// nothing to do -- the default expr should work
			
		xcase UGCGenesisWhen_Manual: case UGCGenesisWhen_ExternalRewardBoxLooted: case UGCGenesisWhen_RewardBoxLooted:
			estrConcatStatic( &accum, "0" );
			
		xcase UGCGenesisWhen_MissionComplete:
			if( generationType == UGCGenesisMissionGenerationType_PlayerMission ) {
				if( isEncounter ) {
					estrConcatf( &accum, "EntCount(GetNearbyPlayersForEnc().EntCropMissionStateSucceeded(\"%s\")) or EntCount(GetNearbyPlayersForEnc().EntCropHasCompletedMission(\"%s\"))",
								 playerMissionName, playerMissionName );
				} else {
					estrConcatf( &accum, "MissionStateSucceeded(\"%s\") or HasCompletedMission(\"%s\")",
								 playerMissionName, playerMissionName );
				}
			} else {
				estrConcatf( &accum, "OpenMissionStateSucceeded(\"%s\")", missionName );
		   }
			
		xcase UGCGenesisWhen_MissionNotInProgress:
			if( generationType == UGCGenesisMissionGenerationType_OpenMission_NoPlayerMission ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext,
									  "%s is PlayerMissionNotInProgress, but there is no player mission.",
									  debugFieldName );
				estrConcatStatic( &accum, "0" );
			} else {
				if( isEncounter ) {
					estrConcatf( &accum, "not (EntCount(GetNearbyPlayersForEnc().EntCropMissionStateInProgress(\"%s\")) or EntCount(GetNearbyPlayersForEnc().EntCropMissionStateSucceeded(\"%s\")) or EntCount(GetNearbyPlayersForEnc().EntCropHasCompletedMission(\"%s\")))",
								 playerMissionName, playerMissionName, playerMissionName );
				} else {
					estrConcatf( &accum, "not (MissionStateInProgress(\"%s\") or MissionStateSucceeded(\"%s\") or HasCompletedMission(\"%s\"))",
								 playerMissionName, playerMissionName, playerMissionName );
				}
			}
			
		xcase UGCGenesisWhen_ObjectiveComplete: case UGCGenesisWhen_ObjectiveCompleteAll: {
			if( eaSize( &when->eaObjectiveNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type ObjectiveComplete, but references no objectives.",
									  debugFieldName );
			} else {
				const char* conjunction;
				int it;

				if( when->type == UGCGenesisWhen_ObjectiveComplete ) {
					conjunction = " or ";
				} else {
					conjunction = " and ";
				}
				for( it = 0; it != eaSize( &when->eaObjectiveNames ); ++it ) {
					if( generationType == UGCGenesisMissionGenerationType_PlayerMission ) {
						if( isEncounter ) {
							estrConcatf( &accum, "%sEntCount(GetNearbyPlayersForEnc().EntCropMissionStateSucceeded(\"%s::%s\"))",
										 (it != 0) ? conjunction : "",
										 missionName, when->eaObjectiveNames[ it ]);
						} else {
							estrConcatf( &accum, "%sMissionStateSucceeded(\"%s::%s\")",
										 (it != 0) ? conjunction : "",
										 missionName, when->eaObjectiveNames[ it ]);
						}
					} else {
						estrConcatf( &accum, "%sOpenMissionStateSucceeded(\"%s::%s\")",
									 (it != 0) ? conjunction : "",
									 missionName, when->eaObjectiveNames[ it ]);
					}
				}
			}
		}

		xcase UGCGenesisWhen_ObjectiveInProgress: {
			if( eaSize( &when->eaObjectiveNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type ObjectiveInProgress, but references no objectives.",
									  debugFieldName );
			} else {
				int it;
				for( it = 0; it != eaSize( &when->eaObjectiveNames ); ++it ) {
					if( generationType == UGCGenesisMissionGenerationType_PlayerMission ) {
						if( isEncounter ) {
							estrConcatf( &accum, "%sEntCount(GetNearbyPlayersForEnc().EntCropMissionStateInProgress(\"%s::%s\"))",
										 (it != 0) ? " or " : "",
										 missionName, when->eaObjectiveNames[ it ]);
						} else {
							estrConcatf( &accum, "%sMissionStateInProgress(\"%s::%s\")",
										 (it != 0 ) ? " or " : "",
										 missionName, when->eaObjectiveNames[ it ]);
						}
					} else {
						estrConcatf( &accum, "%sOpenMissionStateInProgress(\"%s::%s\")",
									 (it != 0) ? " or " : "",
									 missionName, when->eaObjectiveNames[ it ]);
					}
				}
			}
		}
			
		xcase UGCGenesisWhen_PromptStart: case UGCGenesisWhen_PromptComplete: case UGCGenesisWhen_PromptCompleteAll: {
			bool isComplete = (when->type != UGCGenesisWhen_PromptStart);
			if( eaSize( &when->eaPromptNames ) == 0 && eaSize( &when->eaPromptBlocks ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type PromptComplete, but references no prompts.",
									  debugFieldName );
			} else {
				const char* conjunction;
				int it;
				int blockIt;

				if( when->type == UGCGenesisWhen_PromptComplete ) {
					conjunction = " or ";
				} else {
					conjunction = " and ";
				}
				
				for( it = 0; it != eaSize( &when->eaPromptNames ); ++it ) {
					UGCGenesisMissionPrompt* prompt = ugcGenesisFindPrompt( context, when->eaPromptNames[ it ]);

					if( !prompt ) {
						ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s references prompt \"%s\", but it does not exist.",
											  debugFieldName, when->eaPromptNames[ it ]);
					} else {
						char** completeBlockNames = ugcGenesisPromptBlockNames( context, prompt, isComplete );
						char* promptMapChallenge;

						if( prompt->pcChallengeName && !eaSize( &prompt->eaExternalMapNames )) {
							promptMapChallenge = prompt->pcChallengeName;
						} else {
							promptMapChallenge = NULL;
						}
					
						if( eaSize( &completeBlockNames )) {
							estrConcatf( &accum, "%s(", (estrLength( &accum ) ? conjunction : "") );
						}
						for( blockIt = 0; blockIt != eaSize( &completeBlockNames ); ++blockIt ) {
					
							if( isEncounter ) {
								estrConcatf( &accum, "%sEventCount(\"",
											 (blockIt != 0 ? " or " : "") );
								ugcGenesisWriteText( &accum, ugcGenesisPromptEvent( when->eaPromptNames[ it ], completeBlockNames[ blockIt ], isComplete,
																			  shortMissionName, zmapName, promptMapChallenge ), true );
								estrConcatStatic( &accum, "\")" );
							} else {
								estrConcatf( &accum, "%sHasRecentlyCompletedContactDialog(\"%s\", \"%s\")",
											 (blockIt != 0 ? " or " : ""),
											 ugcGenesisContactNameRaw( zmapName, shortMissionName, promptMapChallenge ),
											 ugcGenesisSpecialDialogBlockNameTemp( when->eaPromptNames[ it ], completeBlockNames[ blockIt ]));
							}
						}
						if( eaSize( &completeBlockNames )) {
							estrConcatf( &accum, ")" );
						}
					
						eaDestroy( &completeBlockNames );
					}
				}
				for( it = 0; it != eaSize( &when->eaPromptBlocks ); ++it ) {
					UGCGenesisWhenPromptBlock* whenPB = when->eaPromptBlocks[ it ];
					UGCGenesisMissionPrompt* prompt = ugcGenesisFindPrompt( context, whenPB->promptName );
					UGCGenesisMissionPromptBlock* block = ugcGenesisFindPromptBlock( context, prompt, whenPB->blockName );

					if( !prompt || !block ) {
						ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s references prompt \"%s\", block \"%s\", but it does not exist.",
											  debugFieldName, whenPB->promptName, whenPB->blockName );
					} else {
						char* promptMapChallenge;

						if( prompt->pcChallengeName && !eaSize( &prompt->eaExternalMapNames )) {
							promptMapChallenge = prompt->pcChallengeName;
						} else {
							promptMapChallenge = NULL;
						}
						
						if( isEncounter ) {
							estrConcatf( &accum, "%sEventCount(\"",
										 (estrLength( &accum ) ? conjunction : "") );
							ugcGenesisWriteText( &accum, ugcGenesisPromptEvent( whenPB->promptName, whenPB->blockName, isComplete,
																		  shortMissionName, zmapName, promptMapChallenge ), true );
							estrConcatf( &accum, "\")" );
						} else {
							estrConcatf( &accum, "%sHasRecentlyCompletedContactDialog(\"%s\", \"%s\")",
										 (it != 0 ? " or " : ""),
										 ugcGenesisContactNameRaw( zmapName, shortMissionName, promptMapChallenge ),
										 ugcGenesisSpecialDialogBlockNameTemp( whenPB->promptName, whenPB->blockName ));
						}
					}
				}
			}
		}

		xcase UGCGenesisWhen_ExternalPromptComplete: {
			if( eaSize( &when->eaExternalPrompts ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type ExternalPromptComplete, but references no prompts.",
									  debugFieldName );
			} else {
				int it;
				for( it = 0; it != eaSize( &when->eaExternalPrompts ); ++it ) {
					if( isEncounter ) {
						estrConcatf( &accum, "%sEventCount(\"",
									 (it != 0 ? " or " : "") );
						ugcGenesisWriteText( &accum, ugcGenesisExternalPromptEvent( when->eaExternalPrompts[ it ]->pcPromptName, when->eaExternalPrompts[ it ]->pcContactName, true ), true );
						estrConcatStatic( &accum, "\")" );
					} else {
						estrConcatf( &accum, "%sHasRecentlyCompletedContactDialog(\"%s\", \"%s\")",
									 (it != 0 ? " or " : ""),
									 when->eaExternalPrompts[ it ]->pcContactName,
									 when->eaExternalPrompts[ it ]->pcPromptName );
					}
				}
			}
		}
			
		xcase UGCGenesisWhen_ContactComplete: {
			if( eaSize( &when->eaContactNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type ContactComplete, but references no contacts.",
									  debugFieldName );
			} else {
				int it;
				for( it = 0; it != eaSize( &when->eaContactNames ); ++it ) {
					if( isEncounter ) {
						estrConcatf( &accum, "%sEventCount(\"",
									 (it != 0) ? " or " : "" );
						ugcGenesisWriteText( &accum, ugcGenesisTalkToContactEvent( when->eaContactNames[ it ]), true );
						estrConcatStatic( &accum, "\")" );
					} else {
						estrConcatf( &accum, "%sHasRecentlyCompletedContactDialog(\"%s\", \"%s\")",
									 (it != 0 ? " or " : ""),
									 when->eaContactNames[ it ], "" );
					}
				}
			}
		}
			
		xcase UGCGenesisWhen_ChallengeComplete: {
			if( eaSize( &when->eaChallengeNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type ChallengeComplete, but references no challenges.",
									  debugFieldName );
			} else {
				GameEvent** challengeCompletes = NULL;
				int challengeCount = 0;
				int it;
				for( it = 0; it != eaSize( &when->eaChallengeNames ); ++it ) {
					UGCGenesisMissionZoneChallenge* challenge = ugcGenesisFindZoneChallengeRaw( SAFE_MEMBER( context, genesis_data ), SAFE_MEMBER( context, zone_mission ), overrideChallenges, when->eaChallengeNames[ it ]);
					if( !challenge ) {
						ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s references challenge \"%s\", but it does not exist.",
											  debugFieldName, when->eaChallengeNames[ it ]);
					} else if( challenge->eType == GenesisChallenge_None ) {
						ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s references challenge \"%s\" with no type.",
											  debugFieldName, when->eaChallengeNames[ it ]);
					} else {
						eaPush( &challengeCompletes, ugcGenesisCompleteChallengeEvent( challenge->eType, challenge->pcName, true, zmapName ));
						challengeCount += challenge->iNumToComplete;
					}
				}

				for( it = 0; it != eaSize( &challengeCompletes ); ++it ) {
					estrConcatf( &accum, "%sEventCount(\"",
								 (it != 0) ? " + " : "" );
					ugcGenesisWriteText( &accum, challengeCompletes[ it ], true );
					estrConcatf( &accum, "\")" );
				}
				estrConcatf( &accum, " >= %d",
							 (when->iChallengeNumToComplete ? when->iChallengeNumToComplete : challengeCount) );
				eaDestroy( &challengeCompletes );
			}
		}

		xcase UGCGenesisWhen_ExternalChallengeComplete: {
			if( eaSize( &when->eaExternalChallenges ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type ExternalChallengeComplete, but references no external challenges.",
									  debugFieldName );
			} else {
				GameEvent** challengeCompletes = NULL;
				int challengeCount = 0;
				int it;
				for( it = 0; it != eaSize( &when->eaExternalChallenges ); ++it ) {
					UGCGenesisWhenExternalChallenge* challenge = when->eaExternalChallenges[ it ];
					GameEvent* event = ugcGenesisCompleteChallengeEvent( challenge->eType, challenge->pcName, false, challenge->pcMapName );
					event->tMatchSourceTeam = TriState_Yes;
					eaPush( &challengeCompletes, event );
					challengeCount += 1;
				}

				for( it = 0; it != eaSize( &challengeCompletes ); ++it ) {
					estrConcatf( &accum, "%sEventCount(\"",
								 (it != 0) ? " + " : "" );
					ugcGenesisWriteText( &accum, challengeCompletes[ it ], true );
					estrConcatf( &accum, "\")" );
				}
				estrConcatf( &accum, " >= %d", challengeCount );
				eaDestroy( &challengeCompletes );
			}
		}
			
		xcase UGCGenesisWhen_ChallengeAdvance: {
			if( !isEncounter ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "ChallengeAdvance is only allowed on encounters." );
			} else if( eaSize( &when->eaChallengeNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type ChallengeAdvance, but references no challenges.",
									  debugFieldName );
			} else {
				GameEvent** challengeCompletes = NULL;
				int it;
				for( it = 0; it != eaSize( &when->eaChallengeNames ); ++it ) {
					UGCGenesisMissionZoneChallenge* challenge = ugcGenesisFindZoneChallengeRaw( SAFE_MEMBER( context, genesis_data ), SAFE_MEMBER( context, zone_mission ), overrideChallenges, when->eaChallengeNames[ it ]);
					if( !challenge ) {
						ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s references challenge \"%s\", but it does not exist.",
											  debugFieldName, when->eaChallengeNames[ it ]);
					} else {
						eaPush( &challengeCompletes, ugcGenesisCompleteChallengeEvent( challenge->eType, challenge->pcName, true, zmapName ));
					}
				}

				for( it = 0; it != eaSize( &challengeCompletes ); ++it ) {
					estrConcatf( &accum, "%sEventCountSinceSpawn(\"",
								 (it != 0) ? " or " : "" );
					ugcGenesisWriteText( &accum, challengeCompletes[ it ], true );
					estrConcatf( &accum, "\")" );
				}
			}
		}
			
		xcase UGCGenesisWhen_RoomEntry: {
			int it;
			for( it = 0; it != eaSize( &when->eaRooms ); ++it ) {
				UGCGenesisProceduralObjectParams* params;

				if( context ) {
					params = ugcGenesisInternRoomRequirementParams( context, when->eaRooms[ it ]->layoutName, when->eaRooms[ it ]->roomName );
				} else {
					params = NULL;
				}

				estrConcatf( &accum, "%sEventCount(\"",
							 (it != 0) ? " or " : "" );
				ugcGenesisWriteText( &accum, ugcGenesisReachLocationEvent( when->eaRooms[ it ]->layoutName,
																	 when->eaRooms[ it ]->roomName,
																	 shortMissionName, zmapName ),
								  true );
				estrConcatStatic( &accum, "\")" );

				if( context ) {
					ugcGenesisProceduralObjectSetEventVolume( params );
				}
			}

			if( when->type == UGCGenesisWhen_RoomEntryAll ) {
				estrConcatf( &accum, " >= %d", eaSize( &when->eaRooms ));
			}
		}

		xcase UGCGenesisWhen_ExternalRoomEntry: {
			int it;
			for( it = 0; it != eaSize( &when->eaRooms ); ++it ) {
				estrConcatf( &accum, "%sEventCount(\"",
							 (it != 0) ? " or " : "" );
				ugcGenesisWriteText( &accum, ugcGenesisReachLocationEventRaw( when->eaExternalRooms[ it ]->pcMapName, when->eaExternalRooms[ it ]->pcName ),
								  true );
				estrConcatStatic( &accum, "\")" );
			}
		}

		xcase UGCGenesisWhen_RoomEntryAll: {
			int it;
			for( it = 0; it != eaSize( &when->eaRooms ); ++it ) {
				UGCGenesisProceduralObjectParams* params;

				if( context ) {
					params = ugcGenesisInternRoomRequirementParams( context, when->eaRooms[ it ]->layoutName, when->eaRooms[ it ]->roomName );
				} else {
					params = NULL;
				}

				estrConcatf( &accum, "%s(EventCount(\"",
							 (it != 0) ? " + " : "" );
				ugcGenesisWriteText( &accum, ugcGenesisReachLocationEvent( when->eaRooms[ it ]->layoutName,
																	 when->eaRooms[ it ]->roomName,
																	 shortMissionName, zmapName ),
								  true );
				estrConcatf( &accum, "\") > 0)" );

				if( context ) {
					ugcGenesisProceduralObjectSetEventVolume( params );
				}
			}

			estrConcatf( &accum, " >= %d", eaSize( &when->eaRooms ));
		}

		xcase UGCGenesisWhen_CritterKill: {
			if( eaSize( &when->eaCritterDefNames ) + eaSize( &when->eaCritterGroupNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is CritterKill, but no critters are specified.",
									  debugFieldName );
				estrConcatStatic( &accum, "0" );
			} else {
				int it;
				for( it = 0; it != eaSize( &when->eaCritterDefNames ); ++it ) {
					estrConcatf( &accum, "%sEventCount(\"",
								 (estrLength( &accum ) ? " + " : "") );
					ugcGenesisWriteText( &accum, ugcGenesisKillCritterEvent( when->eaCritterDefNames[ it ], zmapName ), true );
					estrConcatStatic( &accum, "\")" );
				}
				for( it = 0; it != eaSize( &when->eaCritterGroupNames ); ++it ) {
					estrConcatf( &accum, "%sEventCount(\"",
								 (estrLength( &accum ) ? " + " : "") );
					ugcGenesisWriteText( &accum, ugcGenesisKillCritterGroupEvent( when->eaCritterGroupNames[ it ], zmapName ), true );
					estrConcatStatic( &accum, "\")" );
				}

				estrConcatf( &accum, " >= %d", MAX( 1, when->iCritterNumToComplete ));
			}
		}

		xcase UGCGenesisWhen_ItemCount:
			if( eaSize( &when->eaItemDefNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is ItemCount, but no items are specified.",
									  debugFieldName );
				estrConcatStatic( &accum, "0" );
			} else {
				if( isEncounter ) {
					estrConcatf( &accum, "EntCount(GetNearbyPlayersForEnc().EntCropExpr({" );
				}
				
				{
					int it;
					for( it = 0; it != eaSize( &when->eaItemDefNames ); ++it ) {
						estrConcatf( &accum, "%sPlayerItemCount(\"%s\")",
									 (it != 0 ? " + " : ""),
									 when->eaItemDefNames[ it ]);
					}
					estrConcatf( &accum, " >= %d", when->iItemCount );
				}

				if( isEncounter ) {
					estrConcatf( &accum, "}))" );
				}
			}

		xcase UGCGenesisWhen_ExternalOpenMissionComplete:
			if( eaSize( &when->eaExternalMissionNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is ExternalOpenMissionComplete, but no missions are specified.",
									  debugFieldName );
				estrConcatStatic( &accum, "0" );
			} else {
				if( isEncounter ) {
					estrConcatf( &accum, "EntCount(GetNearbyPlayersForEnc().EntCropExpr({" );
				}

				{
					int it;
					for( it = 0; it != eaSize( &when->eaExternalMissionNames ); ++it ) {
						estrConcatf( &accum, "%sOpenMissionMapCredit(\"%s\")",
									 (it != 0) ? " or " : "",
									 when->eaExternalMissionNames[ it ]);
					}
				}

				if( isEncounter ) {
					estrConcatf( &accum, "}))" );
				}
			}

		xcase UGCGenesisWhen_ExternalMapStart:
			if(when->bAnyCrypticMap)
				estrConcatf( &accum, "NOT IsOnUGCMap()");
			else
			{
				if( eaSize( &when->eaExternalMapNames ) == 0 ) {
					ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is ExternalMapStart, but no map is specified",
										  debugFieldName );
					estrConcatStatic( &accum, "0" );
				} else {
					int it;
					for( it = 0; it != eaSize( &when->eaExternalMapNames ); ++it ) {
						estrConcatf( &accum, "%sIsOnMapNamed(%s)",
										(it != 0) ? " or " : "",
										when->eaExternalMapNames[ it ]);
					}
				}
			}

		xcase UGCGenesisWhen_ReachChallenge: {
			int it;
			for( it = 0; it != eaSize( &when->eaChallengeNames ); ++it ) {
				if (context) {
					UGCGenesisProceduralObjectParams *params = ugcGenesisInternVolumeRequirementParams( context, when->eaChallengeNames[it]);
					ugcGenesisProceduralObjectSetEventVolume(params);
				}

				estrConcatf( &accum, "%sEventCount(\"",
							 (it != 0) ? " or " : "" );
				ugcGenesisWriteText( &accum, ugcGenesisReachLocationEvent( NULL, when->eaChallengeNames[ it ],
																	 shortMissionName, zmapName ),
								  true );
				estrConcatStatic( &accum, "\")" );
			}
		}


		xdefault: {
			char* estr = NULL;
			ParserWriteText( &estr, parse_UGCRuntimeErrorContext, debugContext, 0, 0, 0 );
			ugcRaiseErrorInternalCode( UGC_FATAL_ERROR, "%s's %s uses unimplemented When type %s",
									   estr, debugFieldName, StaticDefineIntRevLookup( UGCGenesisWhenTypeEnum, when->type ));
			estrDestroy( &estr );
		}
	}
	
	if( when->checkedAttrib ) {
		char* exprText = ugcGenesisCheckedAttribText( context, when->checkedAttrib, debugContext, debugFieldName, false );

		if( exprText && estrLength( &accum )) {
			estrInsertf( &accum, 0, "(" );
			estrConcatf( &accum, ") and %s", exprText );
		} else if( exprText ) {
			estrConcatf( &accum, "%s", exprText );
		}

		estrDestroy( &exprText );
	}

	if( when->bNot ) {
		estrInsertf( &accum, 0, "not (" );
		estrConcatf( &accum, ")" );
	}

	return accum;
}

/// Create an expression and events suitable for missions that
/// corresponds to WHEN.
void ugcGenesisWhenMissionExprTextAndEvents( char** outEstr, GameEvent*** outEvents, bool* outShowCount, UGCGenesisMissionContext* context, UGCGenesisWhen* when, UGCRuntimeErrorContext* debugContext, const char* debugFieldName )
{
	const char* missionName = ugcGenesisMissionName( context, false );
	const char* playerMissionName = ugcGenesisMissionName( context, true );
	const char* zmapName = context->zmap_info ? zmapInfoGetPublicName( context->zmap_info ) : NULL;
	const char* shortMissionName = context->zone_mission->desc.pcName;
	UGCGenesisMissionGenerationType generationType = context->zone_mission->desc.generationType;

	bool isForUGC = (context && context->is_ugc);
	bool dummyOutShowCount;

	// outShowCount can be NULL, which means we just want to throw it away
	if( !outShowCount ) {
		outShowCount = &dummyOutShowCount;
	}

	estrClear( outEstr );
	eaClearStruct( outEvents, parse_GameEvent );
	
	switch( when->type ) {
		// Conditions that shouldn't work for missions.
		case UGCGenesisWhen_MissionComplete:
		case UGCGenesisWhen_MissionNotInProgress: case UGCGenesisWhen_ChallengeAdvance:
			ugcRaiseErrorContext( UGC_ERROR, debugContext, "Missions can not use %s for %s.",
								  StaticDefineIntRevLookup( UGCGenesisWhenTypeEnum, when->type ), debugFieldName );

		xcase UGCGenesisWhen_MapStart: 
			if( !isForUGC ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "Missions can not use %s for %s.",
									  StaticDefineIntRevLookup( UGCGenesisWhenTypeEnum, when->type ), debugFieldName );
			} else {
				estrConcatf( outEstr, "1" );
			}
			
		xcase UGCGenesisWhen_Manual: case UGCGenesisWhen_ExternalRewardBoxLooted: case UGCGenesisWhen_RewardBoxLooted:
			if( !isForUGC ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "Missions can not use %s for %s.",
									  StaticDefineIntRevLookup( UGCGenesisWhenTypeEnum, when->type ), debugFieldName );
			} else {
				estrConcatf( outEstr, "0" );
			}

		xcase UGCGenesisWhen_ObjectiveComplete: case UGCGenesisWhen_ObjectiveCompleteAll:
		case UGCGenesisWhen_ObjectiveInProgress:
			if( !isForUGC ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "Missions can not use %s for %s.",
									  StaticDefineIntRevLookup( UGCGenesisWhenTypeEnum, when->type ), debugFieldName );
			} else if( eaSize( &when->eaObjectiveNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "Objective is of type %s, but references no objectives.",
									  StaticDefineIntRevLookup( UGCGenesisWhenTypeEnum, when->type ));
			} else {
				const char* conjunction;
				const char* exprFunc;

				if( when->type == UGCGenesisWhen_ObjectiveCompleteAll ) {
					conjunction = " or ";
				} else {
					conjunction = " and ";
				}

				if( when->type == UGCGenesisWhen_ObjectiveInProgress ) {
					if( context->zone_mission->desc.generationType != UGCGenesisMissionGenerationType_PlayerMission ) {
						exprFunc = "OpenMissionStateInProgress";
					} else {
						exprFunc = "MissionStateInProgress";
					}
				} else {
					if( context->zone_mission->desc.generationType != UGCGenesisMissionGenerationType_PlayerMission ) {
						exprFunc = "OpenMissionStateSucceeded";
					} else {
						exprFunc = "MissionStateSucceeded";
					}
				}

				{
					int it;
					for( it = 0 ; it != eaSize( &when->eaObjectiveNames ); ++it ) {
						estrConcatf( outEstr, "%s%s(\"%s::%s\")",
									 (it != 0) ? conjunction : "", exprFunc,
									 missionName, when->eaObjectiveNames[ it ]);
					}
				}
			}
			
		xcase UGCGenesisWhen_PromptStart: case UGCGenesisWhen_PromptComplete:
		case UGCGenesisWhen_PromptCompleteAll: {
			bool isComplete = (when->type != UGCGenesisWhen_PromptStart);
			if( eaSize( &when->eaPromptNames ) == 0 && eaSize( &when->eaPromptBlocks ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type PromptComplete, but references no prompts.",
									  debugFieldName );
			} else {
				const char* conjunction;
				int it;
				int blockIt;

				if( when->type == UGCGenesisWhen_PromptComplete ) {
					conjunction = " or ";
				} else {
					conjunction = " and ";
				}
				
				for( it = 0; it != eaSize( &when->eaPromptNames ); ++it ) {
					if(   stricmp( when->eaPromptNames[ it ], "MissionReturn" ) == 0
						  || stricmp( when->eaPromptNames[ it ], "MissionContinue" ) == 0 ) {
						ugcRaiseErrorContext( UGC_ERROR, debugContext, "Missions can not reference MissionReturn and MissionContinue prompts." );
					} else {
						UGCGenesisMissionPrompt* prompt = ugcGenesisFindPrompt( context, when->eaPromptNames[ it ]);

						if( !prompt ) {
							ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s references prompt \"%s\", but it does not exist.",
												  debugFieldName, when->eaPromptNames[ it ]);
						} else {
							char** completeBlockNames = ugcGenesisPromptBlockNames( context, prompt, isComplete );

							if( eaSize( &completeBlockNames )) {
								estrConcatf( outEstr, "%s(", (it != 0 ? conjunction : "") );
							}
							for( blockIt = 0; blockIt != eaSize( &completeBlockNames ); ++blockIt ) {
								char* promptMapChallenge;
								GameEvent* event;
								char eventName[ 256 ];

								if( prompt->pcChallengeName && !eaSize( &prompt->eaExternalMapNames )) {
									promptMapChallenge = prompt->pcChallengeName;
								} else {
									promptMapChallenge = NULL;
								}

								event = ugcGenesisPromptEvent( when->eaPromptNames[ it ], completeBlockNames[ blockIt ], isComplete,
															shortMissionName, zmapName, promptMapChallenge );
								sprintf( eventName, "Prompt_Complete_%s_%d",
										 when->eaPromptNames[ it ], eaSize( outEvents ) + 1 );
								estrConcatf( outEstr, "%sMissionEventCount(\"%s\")",
											 (blockIt != 0 ? " or " : ""), eventName );
								event->pchEventName = allocAddString( eventName );
								eaPush( outEvents, event );
							}
							if( eaSize( &completeBlockNames )) {
								estrConcatf( outEstr, ")" );
							}

							eaDestroy( &completeBlockNames );
						}
					}
				}

				for( it = 0; it != eaSize( &when->eaPromptBlocks ); ++it ) {
					UGCGenesisWhenPromptBlock* whenPB = when->eaPromptBlocks[ it ];
					UGCGenesisMissionPrompt* prompt = ugcGenesisFindPrompt( context, whenPB->promptName );
					UGCGenesisMissionPromptBlock* block = ugcGenesisFindPromptBlock( context, prompt, whenPB->blockName );

					if( !prompt || !block ) {
						ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s references prompt \"%s\", block \"%s\", but it does not exist.",
											  debugFieldName, whenPB->promptName, whenPB->blockName );
					} else {
						char* promptMapChallenge;
						GameEvent* event;
						char eventName[ 256 ];

						if( prompt->pcChallengeName && !eaSize( &prompt->eaExternalMapNames )) {
							promptMapChallenge = prompt->pcChallengeName;
						} else {
							promptMapChallenge = NULL;
						}

						event = ugcGenesisPromptEvent( whenPB->promptName, whenPB->blockName, isComplete,
													shortMissionName, zmapName, promptMapChallenge );
						sprintf( eventName, "Prompt_Complete_%s_%s_%d",
								 whenPB->promptName, whenPB->blockName, eaSize( outEvents ) + 1 );
						estrConcatf( outEstr, "%sMissionEventCount(\"%s\")",
									 (estrLength( outEstr ) ? conjunction : ""), eventName );
						event->pchEventName = allocAddString( eventName );
						eaPush( outEvents, event );
					}
				}
			}
		}

		xcase UGCGenesisWhen_ExternalPromptComplete: {
			if( eaSize( &when->eaExternalPrompts ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type ExternalPromptComplete, but references no prompts.",
									  debugFieldName );
			} else {
				int it;
				for( it = 0; it != eaSize( &when->eaExternalPrompts ); ++it ) {
					GameEvent* event = ugcGenesisExternalPromptEvent( when->eaExternalPrompts[ it ]->pcPromptName, when->eaExternalPrompts[ it ]->pcContactName, true );
					char eventName[ 256 ];
						
					sprintf( eventName, "Prompt_Complete_%s_%d",
							 when->eaExternalPrompts[ it ]->pcContactName, eaSize( outEvents ) + 1 );
					estrConcatf( outEstr, "%sMissionEventCount(\"%s\")",
								 (it != 0 ? " or " : ""), eventName );
					event->pchEventName = allocAddString( eventName );
					eaPush( outEvents, event );
				}
			}
		}
			
		xcase UGCGenesisWhen_ContactComplete: {
			if( eaSize( &when->eaContactNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type ContactComplete, but references no contacts.",
									  debugFieldName );
			} else {
				int it;
				for( it = 0; it != eaSize( &when->eaContactNames ); ++it ) {
					GameEvent* event = ugcGenesisTalkToContactEvent( when->eaContactNames[ it ]);
					char eventName[ 256 ];
						
					sprintf( eventName, "Contact_Complete_%s_%d",
							 when->eaContactNames[ it ], eaSize( outEvents ) + 1 );
					estrConcatf( outEstr, "%sMissionEventCount(\"%s\")",
								 (it != 0 ? " or " : ""), eventName );
					event->pchEventName = allocAddString( eventName );
					eaPush( outEvents, event );
				}
			}
		}

		xcase UGCGenesisWhen_ChallengeComplete: {
			if( eaSize( &when->eaChallengeNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type ChallengeComplete, but references no challenges.",
									  debugFieldName );
			} else {
				GameEvent** challengeCompletes = NULL;
				int challengeCount = 0;
				int it;
				for( it = 0; it != eaSize( &when->eaChallengeNames ); ++it ) {
					UGCGenesisMissionZoneChallenge* challenge = ugcGenesisFindZoneChallengeRaw( SAFE_MEMBER( context, genesis_data ), SAFE_MEMBER( context, zone_mission ), NULL, when->eaChallengeNames[ it ]);
					if( !challenge ) {
						ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s references challenge \"%s\", but it does not exist.",
											  debugFieldName, when->eaChallengeNames[ it ]);
					} else if( challenge->eType == GenesisChallenge_None ) {
						ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s references challenge \"%s\" with no type.",
											  debugFieldName, when->eaChallengeNames[ it ]);
					} else {
						GameEvent* event = ugcGenesisCompleteChallengeEvent( challenge->eType, challenge->pcName, true, zmapName );
						char eventName[ 256 ];

						sprintf( eventName, "Complete_Challenge_%s_%d",
								 when->eaChallengeNames[ it ],
								 eaSize( outEvents ) + eaSize( &challengeCompletes ) + 1 );
						event->pchEventName = allocAddString( eventName );
						
						eaPush( &challengeCompletes, event );
						challengeCount += challenge->iNumToComplete;
					}
				}

				for( it = 0; it != eaSize( &challengeCompletes ); ++it ) {
					estrConcatf( outEstr, "%sMissionEventCount(\"%s\")",
								 (it != 0) ? " + " : "",
								 challengeCompletes[ it ]->pchEventName );
					eaPush( outEvents, challengeCompletes[ it ]);
				}
				estrConcatf( outEstr, " >= %d",
							 (when->iChallengeNumToComplete ? when->iChallengeNumToComplete : challengeCount) );
				*outShowCount = (challengeCount > 1);
				eaDestroy( &challengeCompletes );
			}
		}
			
		xcase UGCGenesisWhen_ExternalChallengeComplete: {
			if( eaSize( &when->eaExternalChallenges ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is of type ExternalChallengeComplete, but references no external challenges.",
									  debugFieldName );
			} else {
				GameEvent** challengeCompletes = NULL;
				int challengeCount = 0;
				int it;
				for( it = 0; it != eaSize( &when->eaExternalChallenges ); ++it ) {
					UGCGenesisWhenExternalChallenge* challenge = when->eaExternalChallenges[ it ];
					
					GameEvent* event = ugcGenesisCompleteChallengeEvent( challenge->eType, challenge->pcName, false, challenge->pcMapName );
					char eventName[ 256 ];

					sprintf( eventName, "External_Challenge_Complete_%d",
								 eaSize( outEvents ) + eaSize( &challengeCompletes ) + 1 );
					event->pchEventName = allocAddString( eventName );
						
					eaPush( &challengeCompletes, event );
					challengeCount += 1;
				}

				for( it = 0; it != eaSize( &challengeCompletes ); ++it ) {
					estrConcatf( outEstr, "%sMissionEventCount(\"%s\")",
								 (it != 0) ? " + " : "",
								 challengeCompletes[ it ]->pchEventName );
					eaPush( outEvents, challengeCompletes[ it ]);
				}
				estrConcatf( outEstr, " >= %d", challengeCount );
				*outShowCount = (challengeCount > 1 );
				eaDestroy( &challengeCompletes );
			}
		}
		
		xcase UGCGenesisWhen_RoomEntry: {
			int it;
			for( it = 0; it != eaSize( &when->eaRooms ); ++it ) {
				UGCGenesisProceduralObjectParams* params;

				if( context ) {
					params = ugcGenesisInternRoomRequirementParams( context, when->eaRooms[ it ]->layoutName, when->eaRooms[ it ]->roomName );
				} else {
					params = NULL;
				}

				{
					GameEvent* event = ugcGenesisReachLocationEvent( when->eaRooms[ it ]->layoutName,
																  when->eaRooms[ it ]->roomName,
																  shortMissionName, zmapName );
					char eventName[ 256 ];

					sprintf( eventName, "Room_Entry_%s_%s_%d",
							 when->eaRooms[ it ]->layoutName, when->eaRooms[ it ]->roomName, eaSize( outEvents ) + 1 );

					estrConcatf( outEstr, "%sMissionEventCount(\"%s\")",
								 (it != 0 ? " or " : ""), eventName );
					event->pchEventName = allocAddString( eventName );
					eaPush( outEvents, event );
				}

				if( context ) {
					ugcGenesisProceduralObjectSetEventVolume( params );
				}
			}
		}

		xcase UGCGenesisWhen_ExternalRoomEntry: {
			int it;
			for( it = 0; it != eaSize( &when->eaExternalRooms ); ++it ) {
				GameEvent* event = ugcGenesisReachLocationEventRaw( when->eaExternalRooms[ it ]->pcMapName,
																 when->eaExternalRooms[ it ]->pcName );
				char eventName[ 256 ];

				sprintf( eventName, "External_Room_Entry_%s_%s_%d",
						 when->eaExternalRooms[ it ]->pcMapName, when->eaExternalRooms[ it ]->pcName, eaSize( outEvents ) + 1 );
				estrConcatf( outEstr, "%sMissionEventCount(\"%s\")",
							 (it != 0 ? " or " : ""), eventName );
				event->pchEventName = allocAddString( eventName );
				eaPush( outEvents, event );
			}
		}

		xcase UGCGenesisWhen_RoomEntryAll: {
			int it;
			for( it = 0; it != eaSize( &when->eaRooms ); ++it ) {
				UGCGenesisProceduralObjectParams* params;

				if( context ) {
					params = ugcGenesisInternRoomRequirementParams( context, when->eaRooms[ it ]->layoutName, when->eaRooms[ it ]->roomName );
				} else {
					params = NULL;
				}

				{
					GameEvent* event = ugcGenesisReachLocationEvent( when->eaRooms[ it ]->layoutName,
																  when->eaRooms[ it ]->roomName,
																  shortMissionName, zmapName );
					char eventName[ 256 ];

					sprintf( eventName, "Room_Entry_%s_%s_%d",
							 when->eaRooms[ it ]->layoutName, when->eaRooms[ it ]->roomName, eaSize( outEvents ) + 1 );

					estrConcatf( outEstr, "%s(MissionEventCount(\"%s\") > 0)",
								 (it != 0 ? " + " : ""), eventName );
					event->pchEventName = allocAddString( eventName );
					eaPush( outEvents, event );
				}

				if( context ) {
					ugcGenesisProceduralObjectSetEventVolume( params );
				}
			}

			estrConcatf( outEstr, " >= %d", eaSize( &when->eaRooms ));
			*outShowCount = (eaSize( &when->eaRooms ) > 1);
		}

		xcase UGCGenesisWhen_CritterKill: {
			if( eaSize( &when->eaCritterDefNames ) + eaSize( &when->eaCritterGroupNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is CritterKill, but no critters are specified.",
									  debugFieldName );
				estrConcatStatic( outEstr, "0" );
			} else {
				int it;
				for( it = 0; it != eaSize( &when->eaCritterDefNames ); ++it ) {
					GameEvent* event = ugcGenesisKillCritterEvent( when->eaCritterDefNames[ it ], zmapName );
					char eventName[ 256 ];

					sprintf( eventName, "Critter_Kill_%s_%d",
							 when->eaCritterDefNames[ it ], eaSize( outEvents ) + 1 );
					estrConcatf( outEstr, "%sMissionEventCount(\"%s\")",
								 (estrLength( outEstr ) ? " + " : ""), eventName );
					event->pchEventName = allocAddString( eventName );
					eaPush( outEvents, event );
				}
				for( it = 0; it != eaSize( &when->eaCritterGroupNames ); ++it ) {
					GameEvent* event = ugcGenesisKillCritterEvent( when->eaCritterGroupNames[ it ], zmapName );
					char eventName[ 256 ];

					sprintf( eventName, "Critter_Group_%s_%d",
							 when->eaCritterGroupNames[ it ], eaSize( outEvents ) + 1 );
					estrConcatf( outEstr, "%sMissionEventCount(\"%s\")",
								 (estrLength( outEstr ) ? " + " : ""), eventName );
					event->pchEventName = allocAddString( eventName );
					eaPush( outEvents, event );
				}

				estrConcatf( outEstr, " >= %d", MAX( 1, when->iCritterNumToComplete ));
				*outShowCount = (when->iCritterNumToComplete > 1);
			}
		}

		xcase UGCGenesisWhen_ItemCount:
			if( eaSize( &when->eaItemDefNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is ItemCount, but no items are specified.",
									  debugFieldName );
				estrConcatStatic( outEstr, "0" );
			} else {
				int it;
				for( it = 0; it != eaSize( &when->eaItemDefNames ); ++it ) {
					estrConcatf( outEstr, "%sPlayerItemCount(\"%s\")",
								 (estrLength( outEstr ) ? " + " : ""),
								 when->eaItemDefNames[ it ]);
				}
				estrConcatf( outEstr, " >= %d", when->iItemCount );
				*outShowCount = (when->iItemCount > 1 );
			}

		xcase UGCGenesisWhen_ExternalOpenMissionComplete:
			if( eaSize( &when->eaExternalMissionNames ) == 0 ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is ExternalOpenMissionComplete, but no missions are specified.",
									  debugFieldName );
				estrConcatStatic( outEstr, "0" );
			} else {
				int it;
				for( it = 0; it != eaSize( &when->eaExternalMissionNames ); ++it ) {
					estrConcatf( outEstr, "%sOpenMissionMapCredit(\"%s\")",
								 (it != 0) ? " or " : "",
								 when->eaExternalMissionNames[ it ]);
				}
			}
			
		xcase UGCGenesisWhen_ExternalMapStart:
			if(when->bAnyCrypticMap)
				estrConcatf( outEstr, "NOT IsOnUGCMap()" );
			else
			{
				if( eaSize( &when->eaExternalMapNames ) == 0 ) {
					ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s is ExternalMapStart, but no map is specified",
										  debugFieldName );
					estrConcatStatic( outEstr, "0" );
				} else {
					int it;
					for( it = 0; it != eaSize( &when->eaExternalMapNames ); ++it ) {
						estrConcatf( outEstr, "%sIsOnMapNamed(\"%s\")",
									 (it != 0) ? " or " : "",
									 when->eaExternalMapNames[ it ]);
					}
				}
			}

		xcase UGCGenesisWhen_ReachChallenge: {
			int it;
			for( it = 0; it != eaSize( &when->eaChallengeNames ); ++it ) {
				if (context) {
					UGCGenesisProceduralObjectParams *params = ugcGenesisInternVolumeRequirementParams( context, when->eaChallengeNames[it]);
					ugcGenesisProceduralObjectSetEventVolume(params);
				}

				{
					GameEvent* event = ugcGenesisReachLocationEvent( NULL, when->eaChallengeNames[it],
																  shortMissionName, zmapName );
					char eventName[ 256 ];

					sprintf( eventName, "Reach_Challenge_%s_%d",
							 when->eaChallengeNames[it], eaSize( outEvents ) + 1 );

					estrConcatf( outEstr, "%sMissionEventCount(\"%s\")",
								 (it != 0 ? " or " : ""), eventName );
					event->pchEventName = allocAddString( eventName );
					eaPush( outEvents, event );
				}
			}
		}


		xdefault: {
			char* estr = NULL;
			ParserWriteText( &estr, parse_UGCRuntimeErrorContext, debugContext, 0, 0, 0 );
			ugcRaiseErrorInternalCode( UGC_FATAL_ERROR, "%s's %s uses unimplemented When type %s",
									   estr, debugFieldName, StaticDefineIntRevLookup( UGCGenesisWhenTypeEnum, when->type ));
			estrDestroy( &estr );
		}
	}
	
	if( when->checkedAttrib ) {
		ugcRaiseErrorContext( UGC_ERROR, debugContext, "%s has a checked attrib, which is not allowed on a mission.",
							  debugFieldName );
	}

	if( when->bNot ) {
		estrInsertf( outEstr, 0, "not (" );
		estrConcatf( outEstr, ")" );
	}

	// If the mission is a player mission, then this mission could be
	// shared by a bunch of unrelated people
	if( generationType == UGCGenesisMissionGenerationType_PlayerMission ) {
		int eventIt;
		for( eventIt = 0; eventIt != eaSize( outEvents ); ++eventIt ) {
			(*outEvents)[ eventIt ]->tMatchSourceTeam = TriState_Yes;
		}
	}
}

/// Create an expression that corresponds to ATTRIB.
char* ugcGenesisCheckedAttribText( UGCGenesisMissionContext* context, UGCCheckedAttrib* attrib, UGCRuntimeErrorContext* debugContext, const char* debugFieldName, bool isTeam )
{
	char* accum = NULL;

	if( attrib ) {
		if( !nullStr( attrib->astrItemName )) {
			char ns[ RESOURCE_NAME_MAX_SIZE ];
			char nsPrefix[ RESOURCE_NAME_MAX_SIZE ];
			if( resExtractNameSpace_s( context->root_mission_accum->name, SAFESTR( ns ), NULL, 0 )) {
				sprintf( nsPrefix, "%s:", ns );
			} else {
				sprintf( nsPrefix, "" );
			}
			estrPrintf( &accum, "%sPlayerItemCount(\"%s%s\")",
						(estrLength( &accum ) ? " and " : ""),
						nsPrefix, attrib->astrItemName );
		}

		if( !nullStr( attrib->astrSkillName )) {
			UGCCheckedAttribDef* checkedAttrib = ugcDefaultsCheckedAttribDef( attrib->astrSkillName );
			if( checkedAttrib ) {
				if( !isTeam ) {
					estrPrintf( &accum, "%sGetPlayerEnt().%s",
								(estrLength( &accum ) ? " and " : ""),
								checkedAttrib->playerExprText );
				} else {
					estrPrintf( &accum, "%sGetTeamEntsAll().%s",
								(estrLength( &accum ) ? " and " : ""),
								checkedAttrib->teamExprText );
				}
			} else {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "Trying to set %s to CheckedAttrib %s, but that CheckedAttrib does not exist.",
									  debugFieldName, attrib->astrSkillName );
			}
		}

		if( attrib->bNot ) {
			if( estrLength( &accum )) {
				estrInsertf( &accum, 0, "not (" );
				estrPrintf( &accum, ")" );
			}
		}
	}

	return accum;
}

/// Return an array of names for objects that should be in a waypoint lists.
void ugcGenesisWhenMissionWaypointObjects( char*** out_makeVolumeObjects, MissionWaypoint*** out_waypoints, UGCGenesisMissionContext* context, UGCGenesisMissionWaypointMode mode, UGCGenesisWhen* when, UGCRuntimeErrorContext* debugContext, const char* debugFieldName )
{
	bool isForUGC = (context && context->is_ugc);
	char** accum = NULL;
	MissionWaypoint** waypointsAccum = NULL;

	if( mode != UGCGenesisMissionWaypointMode_Points && mode != UGCGenesisMissionWaypointMode_AutogeneratedVolume ) {
		ugcRaiseErrorContext( UGC_ERROR, debugContext, "Unknown WaypointMode %d", mode );
		return;
	}

	switch( when->type ) {
		// Conditions that shouldn't work for missions.
		case UGCGenesisWhen_MissionComplete:
		case UGCGenesisWhen_MissionNotInProgress: case UGCGenesisWhen_ObjectiveComplete: case UGCGenesisWhen_ObjectiveCompleteAll:
		case UGCGenesisWhen_ObjectiveInProgress: case UGCGenesisWhen_ChallengeAdvance:
			ugcRaiseErrorContext( UGC_ERROR, debugContext, "Missions can not use %s for %s.",
								  StaticDefineIntRevLookup( UGCGenesisWhenTypeEnum, when->type ), debugFieldName );

		xcase UGCGenesisWhen_MapStart: case UGCGenesisWhen_Manual:
			if( !isForUGC ) {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "Missions can not use %s for %s.",
									  StaticDefineIntRevLookup( UGCGenesisWhenTypeEnum, when->type ), debugFieldName );
			}

		xcase UGCGenesisWhen_PromptComplete: case UGCGenesisWhen_PromptCompleteAll:
			if( when->pcPromptChallengeName ) {
				if( when->pcPromptMapName ) {
					// It is actually on a specific map, but the prompt was internally generated.
					if( mode == UGCGenesisMissionWaypointMode_Points ) {
						MissionWaypoint* waypointAccum = StructCreate( parse_MissionWaypoint );
						eaPush( &waypointsAccum, waypointAccum );
						
						waypointAccum->type = MissionWaypointType_Clicky;
						waypointAccum->name = StructAllocString( when->pcPromptChallengeName );
						waypointAccum->mapName = allocAddString( when->pcPromptMapName );
					} else {
						ugcRaiseErrorContext( UGC_ERROR, debugContext, "PromptComplete on specific map must used Points waypoint" );
					}
				} else {
					if( mode == UGCGenesisMissionWaypointMode_AutogeneratedVolume ) {
						eaPush( &accum, StructAllocString( when->pcPromptChallengeName ));
					} else if( mode == UGCGenesisMissionWaypointMode_Points ) {
						eaPush( &waypointsAccum, ugcGenesisCreateMissionWaypointForChallengeName( context, when->pcPromptChallengeName ));
					}
				}
			} else {
				ugcGenesisPushAllRoomNames( context, &accum );
			}

		xcase UGCGenesisWhen_ContactComplete:
			if( mode == UGCGenesisMissionWaypointMode_AutogeneratedVolume ) {
				ugcGenesisPushAllRoomNames( context, &accum );
			} else {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "ContactComplete must use AutogeneratedVolume waypoint" );
			}

		xcase UGCGenesisWhen_ChallengeComplete: {
			int it;
				if( mode == UGCGenesisMissionWaypointMode_AutogeneratedVolume ) {
					for( it = 0; it != eaSize( &when->eaChallengeNames ); ++it ) {
						eaPush( &accum, StructAllocString( when->eaChallengeNames[ it ]));
					}
				} else if( mode == UGCGenesisMissionWaypointMode_Points ) {
					// MJF Oct/9/2012: This is not done here, because if
					// it was the waypoints would not go away as you
					// interacted with them.  Instead, each subobjective
					// generated will have a waypoint on it.
					//
					// See the call to
					// ugcGenesisCreateMissionWaypointForChallengeName()
					// in ugcGenesisCreateObjective().
				}
			}

		xcase UGCGenesisWhen_ReachChallenge: {
			int it;
			for( it = 0; it != eaSize( &when->eaChallengeNames ); ++it ) {
				if( mode == UGCGenesisMissionWaypointMode_AutogeneratedVolume ) {
					eaPush( &accum, StructAllocString( ugcGenesisMissionChallengeVolumeName( when->eaChallengeNames[ it ],
																							 context->zone_mission->desc.pcName )));
				} else if( mode == UGCGenesisMissionWaypointMode_Points ) {
					eaPush( &waypointsAccum, ugcGenesisCreateMissionWaypointForChallengeName( context, when->eaChallengeNames[ it ]));
				}
			}
		}

		xcase UGCGenesisWhen_RoomEntry: case UGCGenesisWhen_RoomEntryAll: {
			if( mode == UGCGenesisMissionWaypointMode_AutogeneratedVolume ) {
				int it;
				for( it = 0; it != eaSize( &when->eaRooms ); ++it ) {
					eaPush( &accum, StructAllocString( ugcGenesisMissionRoomVolumeName( when->eaRooms[ it ]->layoutName, when->eaRooms[ it ]->roomName,
																						context->zone_mission->desc.pcName )));
				}
			} else {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "RoomEntry must use AutogeneratedVolume waypoint" );
			}
		}

		xcase UGCGenesisWhen_CritterKill: {
			if( mode == UGCGenesisMissionWaypointMode_AutogeneratedVolume ) {
				int it;
				for( it = 0; it != eaSize( &context->zone_mission->eaChallenges ); ++it ) {
					UGCGenesisMissionZoneChallenge* challenge = context->zone_mission->eaChallenges[ it ];
					if( challenge->eType == GenesisChallenge_Encounter || challenge->eType == GenesisChallenge_Encounter2 ) {
						eaPush( &accum, StructAllocString( challenge->pcName ));
					}
				}
			} else {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "CritterKill must use AutogeneratedVolume waypoint" );
			}
		}
		
		xcase UGCGenesisWhen_ItemCount: {
			if( mode == UGCGenesisMissionWaypointMode_AutogeneratedVolume ) {
				int it;
				for( it = 0; it != eaSize( &context->zone_mission->eaChallenges ); ++it ) {
					UGCGenesisMissionZoneChallenge* challenge = context->zone_mission->eaChallenges[ it ];
					eaPush( &accum, StructAllocString( challenge->pcName ));
				}
			} else {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "ItemCount must use AutogeneratedVolume waypoint" );
			}
		}

		xcase UGCGenesisWhen_ExternalOpenMissionComplete: case UGCGenesisWhen_ExternalMapStart: {
			if( mode == UGCGenesisMissionWaypointMode_Points ) {
				int it;
				for( it = 0; it != eaSize( &when->eaExternalMapNames ); ++it ) {
					MissionWaypoint* waypointAccum = StructCreate( parse_MissionWaypoint );
					eaPush( &waypointsAccum, waypointAccum );
						
					waypointAccum->type = MissionWaypointType_None;
					waypointAccum->mapName = allocAddString( when->eaExternalMapNames[ it ]);
					waypointAccum->astrAlternateMapName = allocAddString( when->astrExternalAlternateMapForWaypoint );
				}
			} else {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "ExternalOpenMissionComplete and ExternalMapStart must use Points waypoints" );
			}
		}

		xcase UGCGenesisWhen_ExternalChallengeComplete: {
			if( mode == UGCGenesisMissionWaypointMode_Points ) {
				// MJF Aug/16/2012: This is not done here, because if
				// it was the waypoints would not go away as you
				// interacted with them.  Instead, each subobjective
				// generated will have a waypoint on it.
				//
				// See the call to
				// ugcGenesisCreateMissionWaypointForExternalChallenge()
				// in ugcGenesisCreateObjective().
			} else {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "ExternalChallengeComplete must used Points waypoint" );
			}
		}
			
			
		xcase UGCGenesisWhen_ExternalRewardBoxLooted: case UGCGenesisWhen_RewardBoxLooted: {
			if( mode == UGCGenesisMissionWaypointMode_Points ) {
				int it;
				for( it = 0; it != eaSize( &when->eaExternalChallenges ); ++it ) {
					UGCGenesisWhenExternalChallenge* challenge = when->eaExternalChallenges[ it ];
					eaPush( &waypointsAccum, ugcGenesisCreateMissionWaypointForExternalChallenge( challenge ));
				}
			} else {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "RewardBoxLooted must used Points waypoint" );
			}
		}

		xcase UGCGenesisWhen_ExternalPromptComplete: {
			if( mode == UGCGenesisMissionWaypointMode_Points ) {
				int it;
				for( it = 0; it != eaSize( &when->eaExternalPrompts ); ++it ) {
					UGCGenesisWhenExternalPrompt* prompt = when->eaExternalPrompts[ it ];
					if( prompt->pcEncounterName && prompt->pcEncounterMapName ) {
						MissionWaypoint* waypointAccum = StructCreate( parse_MissionWaypoint );
						eaPush( &waypointsAccum, waypointAccum );
						
						waypointAccum->type = MissionWaypointType_Encounter;
						waypointAccum->name = StructAllocString( prompt->pcEncounterName );
					waypointAccum->mapName = allocAddString( prompt->pcEncounterMapName );
					}
				}
			} else {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "ExternalPromptComplete must used Points waypoint" );
			}
		}
			
		xcase UGCGenesisWhen_ExternalRoomEntry: {
			if( mode == UGCGenesisMissionWaypointMode_Points ) {
				int it;
				for( it = 0; it != eaSize( &when->eaExternalRooms ); ++it ) {
					UGCGenesisWhenExternalRoom* room = when->eaExternalRooms[ it ];
					MissionWaypoint* waypointAccum = StructCreate( parse_MissionWaypoint );
					eaPush( &waypointsAccum, waypointAccum );
					
					waypointAccum->type = MissionWaypointType_Volume;
					waypointAccum->name = StructAllocString( room->pcName );
					waypointAccum->mapName = allocAddString( room->pcMapName );
				}
			} else {
				ugcRaiseErrorContext( UGC_ERROR, debugContext, "ExternalPromptComplete must used Points waypoint" );
			}
		}

		xdefault: {
			char* estr = NULL;
			ParserWriteText( &estr, parse_UGCRuntimeErrorContext, debugContext, 0, 0, 0 );
			ugcRaiseErrorInternalCode( UGC_FATAL_ERROR, "%s's %s uses unimplemented When type %s",
										   estr, debugFieldName, StaticDefineIntRevLookup( UGCGenesisWhenTypeEnum, when->type ));
			estrDestroy( &estr );
		}
	}

	*out_makeVolumeObjects = accum;
	*out_waypoints = waypointsAccum;
}

/// Create a waypoint for an internal challenge
MissionWaypoint* ugcGenesisCreateMissionWaypointForChallengeName( UGCGenesisMissionContext* context, const char* challengeName )
{
	MissionWaypoint* waypointAccum = StructCreate( parse_MissionWaypoint );
	UGCGenesisMissionZoneChallenge* challenge = ugcGenesisFindZoneChallenge( context->genesis_data, context->zone_mission, challengeName );

	if( challenge ) {
		switch( challenge->eType ) {
			case GenesisChallenge_Clickie: case GenesisChallenge_Destructible:
				waypointAccum->type = MissionWaypointType_Clicky;

			xcase GenesisChallenge_Encounter: case GenesisChallenge_Encounter2:
			case GenesisChallenge_Contact:
				waypointAccum->type = MissionWaypointType_Encounter;

			xdefault:
				waypointAccum->type = MissionWaypointType_Clicky;
		}
	} else {
		waypointAccum->type = MissionWaypointType_Clicky;
	}
	waypointAccum->name = StructAllocString( challengeName );
	waypointAccum->mapName = allocAddString( zmapInfoGetPublicName( context->zmap_info ));

	return waypointAccum;
}

/// Create a waypoint for an external challenge.
MissionWaypoint* ugcGenesisCreateMissionWaypointForExternalChallenge( UGCGenesisWhenExternalChallenge* challenge )
{
	MissionWaypoint* waypointAccum = StructCreate( parse_MissionWaypoint );

	switch( challenge->eType ) {
		case GenesisChallenge_Clickie: case GenesisChallenge_Destructible:
			waypointAccum->type = MissionWaypointType_Clicky;
						
		xcase GenesisChallenge_Encounter: case GenesisChallenge_Encounter2:
		case GenesisChallenge_Contact:
			waypointAccum->type = MissionWaypointType_Encounter;
						
		xdefault:
			waypointAccum->type = MissionWaypointType_None;
	}
	waypointAccum->name = StructAllocString( challenge->pcName );
	waypointAccum->mapName = allocAddString( challenge->pcMapName );

	return waypointAccum;
}



/// Validate that this context has all the information needed to
/// transmogrify.
bool ugcGenesisTransmogrifyMissionValidate( UGCGenesisTransmogrifyMissionContext* context )
{
	bool fatalAccum = false;

	if (!zmapInfoGetStartSpawnName(context->zmap_info) &&
		!context->mission_desc->zoneDesc.startDescription.pcStartRoom &&
		!context->map_desc->space_ugc &&
		!context->map_desc->prefab_ugc)
	{
		ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextMission(context->mission_desc->zoneDesc.pcName), "Mission has no starting room!");
	}

	// Validate challenges
	{
		int it;
		StashTable table = stashTableCreateWithStringKeys( 4, StashDefault );
		for( it = 0; it != eaSize( &context->mission_desc->eaChallenges ); ++it ) {
			UGCGenesisMissionChallenge* challenge = context->mission_desc->eaChallenges[ it ];
			
			if( !stashAddInt( table, challenge->pcName, it, false )) {
				int firstIndex;
				stashFindInt( table, challenge->pcName, &firstIndex );
				ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextChallenge( challenge->pcName, SAFE_MEMBER( context->mission_desc, zoneDesc.pcName ), challenge->pcLayoutName ),
									  "Duplicate challenge found at index %d and %d.",
									  firstIndex + 1, it + 1 );
				fatalAccum = true;
			}

			if( !ugcGenesisTransmogrifyChallengeValidate( context, challenge )) {
				fatalAccum = true;
			}
		}
		for( it = 0; it != eaSize( &context->map_desc->shared_challenges ); ++it ) {
			UGCGenesisMissionChallenge* challenge = context->map_desc->shared_challenges[ it ];
			
			if( !stashAddInt( table, challenge->pcName, it | 0xF00D0000, false )) {
				int firstIndex;
				bool firstShared;
				stashFindInt( table, challenge->pcName, &firstIndex );

				if( (firstIndex & 0xFFFF0000) == 0xF00D0000 ) {
					firstShared = true;
					firstIndex &= 0xFFFF;
				} else {
					firstShared = false;
				}
				
				ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextChallenge( challenge->pcName, SAFE_MEMBER( context->mission_desc, zoneDesc.pcName ), challenge->pcLayoutName ),
									  "Duplicate challenge found at index %d%s and %d (shared).",
									  firstIndex + 1, (firstShared ? " (shared)" : ""), it + 1 );
				fatalAccum = true;
			}
		}
		stashTableDestroy( table );
	}

	// Validate prompts
	{
		int it;
		StashTable table = stashTableCreateWithStringKeys( 4, StashDefault );
		const char** mapStartPrompts = NULL;
		for( it = 0; it != eaSize( &context->mission_desc->zoneDesc.eaPrompts ); ++it ) {
			UGCGenesisMissionPrompt* prompt = context->mission_desc->zoneDesc.eaPrompts[ it ];
			
			if( !stashAddInt( table, prompt->pcName, it, false )) {
				int firstIndex;
				stashFindInt( table, prompt->pcName, &firstIndex );
				ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextPrompt( prompt->pcName, NULL, SAFE_MEMBER( context->mission_desc, zoneDesc.pcName ), prompt->pcLayoutName ),
									  "Duplicate prompt found at index %d and %d.",
									  firstIndex + 1, it + 1 );
			}

			if( prompt->showWhen.type == UGCGenesisWhen_MapStart && !prompt->pcChallengeName ) {
				eaPush( &mapStartPrompts, prompt->pcName );
			}
		}

		if( eaSize( &mapStartPrompts ) > 1 ) {
			for( it = 0; it != eaSize( &mapStartPrompts ); ++it ) {
				ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextPrompt( mapStartPrompts[ it ], NULL, SAFE_MEMBER( context->mission_desc, zoneDesc.pcName ), NULL),
									  "Multiple prompts show at MapStart." );
			}
		}
		eaDestroy( &mapStartPrompts );

		stashTableDestroy( table );
	}

	// Validate FSMs
	{
		int it;
		StashTable table = stashTableCreateWithStringKeys( 4, StashDefault );
		for( it = 0; it != eaSize( &context->mission_desc->zoneDesc.eaFSMs ); ++it ) {
			UGCGenesisFSM* gfsm = context->mission_desc->zoneDesc.eaFSMs[ it ];

			if( !stashAddInt( table, gfsm->pcName, it, false )) {
				int firstIndex;
				stashFindInt( table, gfsm->pcName, &firstIndex );
				ugcRaiseErrorContext(	UGC_FATAL_ERROR, ugcMakeTempErrorContextPrompt( gfsm->pcName, NULL, SAFE_MEMBER( context->mission_desc, zoneDesc.pcName ), NULL),
										"Duplicate prompt found at index %d and %d.",
										firstIndex + 1, it + 1 );
			}
		}

		stashTableDestroy( table );
	}

	return !fatalAccum;
}

/// Validate that this context has all the shared missions needed to
/// transmogrify.
bool ugcGenesisTransmogrifySharedChallengesValidate( UGCGenesisTransmogrifyMissionContext* context )
{
	bool fatalAccum = false;
	int it;

	for( it = 0; it != eaSize( &context->map_desc->shared_challenges ); ++it ) {
		UGCGenesisMissionChallenge* challenge = context->map_desc->shared_challenges[ it ];

		if( !ugcGenesisTransmogrifyChallengeValidate( context, challenge )) {
			fatalAccum = true;
		}
	}
	
	return !fatalAccum;
}


/// Validate that this challenge has all the data needed to
/// transmogrify.
bool ugcGenesisTransmogrifyChallengeValidate( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisMissionChallenge* challenge )
{
	bool fatalAccum = false;
	bool isShared = (context->mission_desc == NULL);
	UGCRuntimeErrorContext* contextBuffer = ugcMakeErrorContextChallenge( challenge->pcName, SAFE_MEMBER( context->mission_desc, zoneDesc.pcName ), challenge->pcLayoutName );

	// Warn if the challenge is using anything that does not get
	// persisted correctly with player missions.
	if(   challenge->spawnWhen.type != UGCGenesisWhen_MapStart
		  && challenge->spawnWhen.type != UGCGenesisWhen_ObjectiveInProgress
		  && challenge->spawnWhen.type != UGCGenesisWhen_ObjectiveComplete
		  && challenge->spawnWhen.type != UGCGenesisWhen_ObjectiveCompleteAll
		  && challenge->spawnWhen.type != UGCGenesisWhen_ChallengeAdvance
		  && challenge->spawnWhen.type != UGCGenesisWhen_Manual
		  && !resNamespaceIsUGC( zmapInfoGetPublicName( context->zmap_info ))) {
		ugcRaiseErrorContext( UGC_WARNING, contextBuffer,
							  "Spawn when is set to %s, this is error prone.  "
							  "Consider using ObjectiveInProgress or ObjectiveComplete "
							  "instead.",
							  StaticDefineIntRevLookup( UGCGenesisWhenTypeEnum, challenge->spawnWhen.type ));
	}
	
	if( isShared && challenge->spawnWhen.type != UGCGenesisWhen_MapStart ) {
		ugcRaiseErrorContext( UGC_FATAL_ERROR, contextBuffer, "Shared challenges must spawn with MapStart." );
		fatalAccum = true;
	} else if( challenge->spawnWhen.type == UGCGenesisWhen_PromptComplete ) {
		// UGC does its own checking to make sure you can't create uncompletable missions.
		if( !resNamespaceIsUGC( zmapInfoGetPublicName( context->zmap_info ))) {
			// If the challenge's when condition is a prompt, but that
			// prompt is not permanently shown, it is possible to get
			// the map into a state where the prompt would never need
			// to show.  They should be using ObjectiveComplete or
			// ObjectiveInProgress on these conditions anyway.
			int it;
			for( it = 0; it != eaSize( &challenge->spawnWhen.eaPromptNames ); ++it ) {
				const char* neverSpawnMessage = "Challenge may never spawn!  Consider changing the challenge's SpawnWhen to ObjectiveInProgress.";
				UGCGenesisMissionPrompt* prompt = ugcGenesisTransmogrifyFindPrompt( context, challenge->spawnWhen.eaPromptNames[ it ]);
			
				if( !prompt ) {
					ugcRaiseErrorContext( UGC_FATAL_ERROR, contextBuffer, "%s  Challenge will not spawn unless prompt %s is shown, but the prompt does not exist.",
										  neverSpawnMessage, challenge->spawnWhen.eaPromptNames[ it ]);
				} else if( !prompt->bOptional ) {
					ugcRaiseErrorContext( UGC_FATAL_ERROR, contextBuffer, "%s  Challenge will not spawn unless prompt %s is shown, but the prompt does not have a button so it may never show.",
										  neverSpawnMessage, prompt->pcName );
				} else if( prompt->bOptional && prompt->bOptionalHideOnComplete ) {
					ugcRaiseErrorContext( UGC_FATAL_ERROR, contextBuffer, "%s  Challenge will not spawn unless prompt %s is shown, but the prompt is marked HideOnComplete.",
										  neverSpawnMessage, prompt->pcName );
				} else if( prompt->showWhen.type == UGCGenesisWhen_MissionNotInProgress ) {
					ugcRaiseErrorContext( UGC_FATAL_ERROR, contextBuffer, "%s  Challenge will not spawn unless prompt %s is shown, but the prompt will only show if a mission is not in progress.",
										  neverSpawnMessage, prompt->pcName );
				}
			}
		}
	}

	StructDestroy( parse_UGCRuntimeErrorContext, contextBuffer );
	return !fatalAccum;
}

/// Validate that this context has all the information needed to
/// generate.
bool ugcGenesisGenerateMissionValidate( UGCGenesisMissionContext* context )
{
	bool fatalAccum = false;
	
	if( nullStr( context->zone_mission->desc.pcName )) {
		char buffer[256];
		sprintf( buffer, "#%d", context->mission_num );
		ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextMission(buffer), "Mission missing mission name." );
		fatalAccum = true;
		return !fatalAccum;
	}

	if( !context->zmap_info ) {
		if( context->zone_mission->desc.generationType != UGCGenesisMissionGenerationType_PlayerMission ) {
			ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextMission( context->zone_mission->desc.pcName ),
								  "Mission is set to generate an open mission, but this mission is not tied to any map." );
			fatalAccum = true;
		}
		if( eaSize( &context->zone_mission->eaChallenges )) {
			ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextMission( context->zone_mission->desc.pcName ),
								  "Mission has challenges, but this mission is not tied to any map." );
			fatalAccum = true;
		}
		if( context->zone_mission->desc.grantDescription.eGrantType == UGCGenesisMissionGrantType_MapEntry ) {
			ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextMission( context->zone_mission->desc.pcName ),
								  "Mission is to be granted on map entry, but this mission is not tied to any map." );
			fatalAccum = true;
		}
	}
	
	
	if(   context->genesis_data
		  && !(eaSize(&context->genesis_data->genesis_interiors) > 0 && context->genesis_data->genesis_interiors[0]->override_positions)
		  && (eaSize(&context->genesis_data->genesis_shared) == 0)
		  && nullStr( context->zone_mission->desc.startDescription.pcStartRoom )) {
		ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextMission(context->zone_mission->desc.pcName), "No starting room.");
	}

	{
		int it;
		StashTable table = stashTableCreateWithStringKeys( 4, StashDefault );
		for( it = 0; it != eaSize( &context->zone_mission->eaChallenges ); ++it ) {
			UGCGenesisMissionZoneChallenge* challenge = context->zone_mission->eaChallenges[ it ];
			
			if( !stashAddInt( table, challenge->pcName, it, false )) {
				int firstIndex;
				stashFindInt( table, challenge->pcName, &firstIndex );
				ugcRaiseErrorContext( UGC_FATAL_ERROR, 
									  ugcMakeTempErrorContextChallenge( challenge->pcName, SAFE_MEMBER( context->zone_mission, desc.pcName ), challenge->pcLayoutName ), 
									  "Duplicate challenge found at index %d and %d.",
									  firstIndex + 1, it + 1 );
			}
		}

		stashTableDestroy( table );
	}

	{
		StashTable table = stashTableCreateWithStringKeys( 4, StashDefault );
		int it;
		for( it = 0; it != eaSize( &context->zone_mission->desc.eaObjectives ); ++it ) {
			ugcGenesisGenerateMissionValidateObjective( context, table, &fatalAccum, context->zone_mission->desc.eaObjectives[ it ]);
		}
		stashTableDestroy( table );
	}

	{
		int it;
		StashTable table = stashTableCreateWithStringKeys( 4, StashDefault );
		for( it = 0; it != eaSize( &context->zone_mission->desc.eaPrompts ); ++it ) {
			UGCGenesisMissionPrompt* prompt = context->zone_mission->desc.eaPrompts[ it ];
			
			if( !stashAddInt( table, prompt->pcName, it, false )) {
				int firstIndex;
				stashFindInt( table, prompt->pcName, &firstIndex );
				ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextPrompt( prompt->pcName, NULL, SAFE_MEMBER( context->zone_mission, desc.pcName ), prompt->pcLayoutName ),
									  "Duplicate prompt found at index %d and %d.",
									  firstIndex + 1, it + 1 );
			}
		}

		stashTableDestroy( table );
	}

	return !fatalAccum;
}

/// Validate a specific UGCGenesisMissionObjective.
///
/// TABLE is used to keep a set of objective names unique.
void ugcGenesisGenerateMissionValidateObjective( UGCGenesisMissionContext* context, StashTable table, bool* fatalAccum, UGCGenesisMissionObjective* objective )
{
	if( !stashAddPointer( table, objective->pcName, objective->pcName, false )) {
		ugcRaiseErrorContext( UGC_FATAL_ERROR, ugcMakeTempErrorContextObjective( objective->pcName, SAFE_MEMBER( context->zone_mission, desc.pcName ) ),
							  "Duplicate objective found." );
		*fatalAccum = true;
	}

	{
		int it;
		for( it = 0; it != eaSize( &objective->eaChildren ); ++it ) {
			ugcGenesisGenerateMissionValidateObjective( context, table, fatalAccum, objective->eaChildren[ it ]);
		}
	}
}

/// Fill out player-specific data for the mission into MISSION.
void ugcGenesisGenerateMissionPlayerData( UGCGenesisMissionContext* context, MissionDef* mission )
{
	UGCGenesisMissionGrantDescription* grantDescription = &context->zone_mission->desc.grantDescription;
	
	if( grantDescription->eGrantType == UGCGenesisMissionGrantType_RandomNPC ) {
		mission->missionType = MissionType_AutoAvailable;
	} else {																	
		mission->missionType = MissionType_Normal;							
	}
	mission->eShareable = context->zone_mission->desc.eShareable;
	mission->bDisableCompletionTracking = !context->zone_mission->bTrackingEnabled;

	ugcGenesisCreateMessage( context, &mission->summaryMsg, context->zone_mission->desc.pcSummaryText );
	ugcGenesisCreateMessage( context, &mission->detailStringMsg.msg, context->zone_mission->desc.pcDescriptionText );

	if( nullStr( context->zone_mission->desc.startDescription.pcEntryFromMapName ) != nullStr( context->zone_mission->desc.startDescription.pcEntryFromInteractableName )) {
		ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextMission(context->zone_mission->desc.pcName), "Only one of EntryFromMapName and EntryFromInteractableName is set, both or neither must be set.");
	} else if( !nullStr( context->zone_mission->desc.startDescription.pcEntryFromMapName ) && !nullStr( context->zone_mission->desc.startDescription.pcEntryFromInteractableName )) {
		// Add the interactable override
		{
			InteractableOverride* interactAccum = StructCreate( parse_InteractableOverride );
			char buffer[ 256 ];
			interactAccum->pPropertyEntry = StructCreate( parse_WorldInteractionPropertyEntry );
			interactAccum->pPropertyEntry->pDoorProperties = StructCreate( parse_WorldDoorInteractionProperties );

			interactAccum->pcMapName = allocAddString( context->zone_mission->desc.startDescription.pcEntryFromMapName );
			interactAccum->pcInteractableName = allocAddString( context->zone_mission->desc.startDescription.pcEntryFromInteractableName );
			interactAccum->pPropertyEntry->pcInteractionClass = allocAddString( "Door" );
			if( grantDescription->eGrantType != UGCGenesisMissionGrantType_MapEntry ) {
				sprintf( buffer, "MissionStateInProgress(\"%s\")", mission->name );
				interactAccum->pPropertyEntry->pInteractCond = exprCreateFromString( buffer, NULL );
			}
			interactAccum->pPropertyEntry->pDoorProperties->doorDest.eType = WVAR_MAP_POINT;
			interactAccum->pPropertyEntry->pDoorProperties->doorDest.eDefaultType = WVARDEF_SPECIFY_DEFAULT;
			interactAccum->pPropertyEntry->pDoorProperties->doorDest.pSpecificValue = StructCreate(parse_WorldVariable);
			interactAccum->pPropertyEntry->pDoorProperties->doorDest.pSpecificValue->eType = WVAR_MAP_POINT;
			interactAccum->pPropertyEntry->pDoorProperties->doorDest.pSpecificValue->pcZoneMap = StructAllocString( zmapInfoGetPublicName( context->zmap_info ));
			{
				WorldVariableDef* varAccum = StructCreate( parse_WorldVariableDef );
				varAccum->pSpecificValue = StructCreate( parse_WorldVariable );
				
				varAccum->pSpecificValue->pcName = varAccum->pcName = allocAddString( "Mission_Num" );
				varAccum->pSpecificValue->eType = varAccum->eType = WVAR_INT;
				varAccum->eDefaultType = WVARDEF_SPECIFY_DEFAULT;
				varAccum->pSpecificValue->iIntVal = context->mission_num;
				eaPush( &interactAccum->pPropertyEntry->pDoorProperties->eaVariableDefs, varAccum );
			}				
			eaPush( &mission->ppInteractableOverrides, interactAccum );
		}

		// And its location
		{
			MissionWaypoint* waypointAccum = StructCreate( parse_MissionWaypoint );

			waypointAccum->type = MissionWaypointType_Clicky;
			waypointAccum->name = StructAllocString( context->zone_mission->desc.startDescription.pcEntryFromInteractableName );
			waypointAccum->mapName = allocAddString( context->zone_mission->desc.startDescription.pcEntryFromMapName );

			eaPush(&mission->eaWaypoints, waypointAccum);
		}
	}

	mission->params = StructCreate( parse_MissionDefParams );
	mission->params->OnsuccessRewardTableName = REF_STRING_FROM_HANDLE( context->zone_mission->desc.hReward );
	mission->params->NumericRewardScale = context->zone_mission->desc.rewardScale;
	
	// grant mission info
	switch( grantDescription->eGrantType ) {
		case UGCGenesisMissionGrantType_MapEntry: {
			UGCGenesisMissionRequirements* grantReq = ugcGenesisInternRequirement( context );
			char grantExpr[ 1024 ];
			
			if( !grantReq->params ) {
				grantReq->params = StructCreate( parse_UGCGenesisProceduralObjectParams );
			}
			ugcGenesisProceduralObjectSetActionVolume( grantReq->params );

			sprintf( grantExpr, "GrantMission(\"%s\")", ugcGenesisMissionName( context, true ));			
			if( grantReq->params->action_volume_properties->entered_action ) {
				exprAppendStringLines( grantReq->params->action_volume_properties->entered_action, grantExpr );
			} else {
				grantReq->params->action_volume_properties->entered_action = exprCreateFromString( grantExpr, NULL );
			}
		}
			
		xcase UGCGenesisMissionGrantType_RandomNPC:
			// nothing else to do

		xcase UGCGenesisMissionGrantType_Manual:
			// nothing else to do
					
		xcase UGCGenesisMissionGrantType_Contact: {
			const UGCGenesisMissionGrant_Contact* contact = grantDescription->pGrantContact;
			ContactMissionOffer* offerAccum = ugcGenesisInternMissionOffer( context, context->zone_mission->desc.pcName, false );
					
			offerAccum->allowGrantOrReturn = ContactMissionAllow_GrantOnly;
			eaPush( &offerAccum->offerDialog, ugcGenesisCreateDialogBlock( context, contact->pcOfferText, NULL ));
			eaPush( &offerAccum->inProgressDialog, ugcGenesisCreateDialogBlock( context, contact->pcInProgressText, NULL ));
		}
	}

	switch( grantDescription->eTurnInType ) {
		case UGCGenesisMissionTurnInType_Automatic:
			mission->needsReturn = false;

		xcase UGCGenesisMissionTurnInType_GrantingContact: case UGCGenesisMissionTurnInType_DifferentContact: {
			const UGCGenesisMissionTurnIn_Contact* contact = grantDescription->pTurnInContact;
			bool isSameContact = (grantDescription->eTurnInType == UGCGenesisMissionTurnInType_GrantingContact);
			ContactMissionOffer* offerAccum = ugcGenesisInternMissionOffer( context, context->zone_mission->desc.pcName, !isSameContact );

			if( isSameContact ) {
				// The contact should already exist, filled out above.
				if( offerAccum->allowGrantOrReturn != ContactMissionAllow_GrantOnly ) {
					ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextMission(context->zone_mission->desc.pcName), "Return type is GrantingContact, but a contact does not grant this mission!");
				}
				
				offerAccum->allowGrantOrReturn = ContactMissionAllow_GrantAndReturn;
			} else {
				offerAccum->allowGrantOrReturn = ContactMissionAllow_ReturnOnly;
			}
			mission->needsReturn = true;
			mission->eReturnType = MissionReturnType_Message;
			ugcGenesisCreateMessage( context, &mission->msgReturnStringMsg, contact->pcMissionReturnText );
					
			eaPush( &offerAccum->completedDialog, ugcGenesisCreateDialogBlock( context, contact->pcCompletedText, NULL ));
		}
	}

	switch( grantDescription->eFailType ) {
		case UGCGenesisMissionFailType_Never:
			// nothing to do

		xcase UGCGenesisMissionFailType_Timeout:
			mission->uTimeout = grantDescription->iFailTimeoutSeconds;
			ugcGenesisAccumFailureExpr( mission, "TimeExpired()" );
			{
				WorldGameActionProperties* floatieAccum = StructCreate( parse_WorldGameActionProperties );
				floatieAccum->eActionType = WorldGameActionType_SendFloaterMsg;
				floatieAccum->pSendFloaterProperties = StructCreate( parse_WorldSendFloaterActionProperties );
				ugcGenesisCreateMessage( context, &floatieAccum->pSendFloaterProperties->floaterMsg, "Out of time." );
				floatieAccum->pSendFloaterProperties->floaterMsg.bEditorCopyIsServer = true;
				setVec3( floatieAccum->pSendFloaterProperties->vColor, 226.0 / 255.0, 0, 0 );

				eaPush( &mission->ppFailureActions, floatieAccum );
			}

		xdefault:
			FatalErrorf( "not yet implemented" );
	}

	// cooldown info
	if( grantDescription->bRepeatable ) {
		mission->repeatable = true;
		mission->fRepeatCooldownHours = grantDescription->fRepeatCooldownHours;
		mission->fRepeatCooldownHoursFromStart = grantDescription->fRepeatCooldownHoursFromStart;
		mission->uRepeatCooldownCount = grantDescription->iRepeatCooldownCount;
		mission->bRepeatCooldownBlockTime = grantDescription->bRepeatCooldownBlockTime;
	}

	// requires info
	if( eaSize( &grantDescription->eaRequiresMissions )) {
		char* requiresExprAccum = NULL;
		int it;

		for( it = 0; it != eaSize( &grantDescription->eaRequiresMissions ); ++it ) {
			const char* missionName = grantDescription->eaRequiresMissions[ it ];

			estrConcatf( &requiresExprAccum, "%sHasCompletedMission(\"%s\")",
						 (it != 0 ? " and " : ""),
						 missionName );
		}

		mission->missionReqs = exprCreateFromString( requiresExprAccum, NULL );
		estrDestroy( &requiresExprAccum );
	}
}

/// Fill out MISSION's filename and scope.
const void ugcGenesisMissionUpdateFilename( UGCGenesisMissionContext* context, MissionDef* mission )
{
	if( context->zmap_info ) {
		char path[ MAX_PATH ];
		char nameSpace[RESOURCE_NAME_MAX_SIZE];
		char baseName[RESOURCE_NAME_MAX_SIZE];
	
		resExtractNameSpace_s(zmapInfoGetFilename( context->zmap_info ), NULL, 0, SAFESTR(path));
		getDirectoryName( path );

		strcat( path, "/missions" );
		mission->scope = allocAddFilename( path );
	
		if (resExtractNameSpace(mission->name, nameSpace, baseName))
			sprintf( path, "%s:%s/%s.mission", nameSpace, mission->scope, baseName);
		else
			strcatf( path, "/%s.mission", mission->name );
		mission->filename = allocAddFilename( path );
	} else {
		// Right now, only UGC is using this feature
		char path[ MAX_PATH ];
		sprintf( path, "Maps/%s", context->project_prefix);
		mission->scope = allocAddFilename(path);
	}
}

/// Return an alloc'd name for a mission.
///
/// If PLAYER-SPECIFIC is true, then return the name of the player
/// specific mission.  This only is different when running the
/// transmogrifier on an open mission, in which case the open mission
/// has _OpenMission at the end.
const char* ugcGenesisMissionName( UGCGenesisMissionContext* context, bool playerSpecific )
{
	if( context->zmap_info ) {
		return ugcGenesisMissionNameRaw( zmapInfoGetPublicName( context->zmap_info ),
									  context->zone_mission->desc.pcName,
									  !playerSpecific && context->zone_mission->desc.generationType != UGCGenesisMissionGenerationType_PlayerMission );
	} else {
		return allocAddString( context->zone_mission->desc.pcName );
	}
}

/// Return an alloc'd name for a contact
const char* ugcGenesisContactName( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt )
{
	return ugcGenesisContactNameRaw( (context->zmap_info ? zmapInfoGetPublicName( context->zmap_info ) : NULL),
								  context->zone_mission->desc.pcName,
								  SAFE_MEMBER( prompt, pcChallengeName ));
}

/// Return a ContactDef with name CONTACT_NAME.
ContactDef* ugcGenesisInternContactDef( UGCGenesisMissionContext* context, const char* contact_name )
{
	int it;
	for( it = 0; it != eaSize( context->contacts_accum ); ++it ) {
		if( (*context->contacts_accum)[ it ]->name == contact_name ) {
			return (*context->contacts_accum)[ it ];
		}
	}

	{
		ContactDef* accum = StructCreate( parse_ContactDef );
		char path[ MAX_PATH ];

		accum->name = allocAddString( contact_name );
		accum->genesisZonemap = StructAllocString( zmapInfoGetPublicName( context->zmap_info ));
		accum->type = ContactType_SingleDialog;

		if( context->zmap_info ) {
			char noNameSpacePath[ MAX_PATH ];
			strcpy( path, zmapInfoGetFilename( context->zmap_info ));
			getDirectoryName( path );
			strcat( path, "/contacts" );
			resExtractNameSpace_s(path, NULL, 0, SAFESTR(noNameSpacePath));
			accum->scope = allocAddFilename( noNameSpacePath );
		} else {
			sprintf( path, "Maps/%s", context->project_prefix );
			accum->scope = allocAddFilename( path );
		}
		
		eaPush( context->contacts_accum, accum );
		return accum;
	}
}

/// Create the Optional Action entry for PROMPT, using VISIBLE-EXPR as
/// the condition which triggers visibility, and hook it up to show
/// anywhere in the map.
void ugcGenesisCreatePromptOptionalAction( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, const char* visibleExpr )
{
	if( eaSize( &prompt->eaExternalMapNames ) == 0 ) {
		UGCGenesisMissionRequirements* req = ugcGenesisInternRequirement( context );
		WorldOptionalActionVolumeEntry* entry = ugcGenesisCreatePromptOptionalActionEntry( context, prompt, visibleExpr );

		if (!req->params) {
			req->params = StructCreate(parse_UGCGenesisProceduralObjectParams);
		}
		ugcGenesisProceduralObjectSetOptionalActionVolume( req->params );
		eaPush( &req->params->optionalaction_volume_properties->entries, entry );
	} else {
		int mapIt;
		int it;
		for( mapIt = 0; mapIt != eaSize( &prompt->eaExternalMapNames ); ++mapIt ) {
			const char* externalMapName = prompt->eaExternalMapNames[ mapIt ];
			
			ZoneMapEncounterInfo* zeni = RefSystem_ReferentFromString( "ZoneMapEncounterInfo", externalMapName );
			char** volumeNames = SAFE_MEMBER( zeni, volume_logical_name );
			WorldInteractionPropertyEntry* entry = ugcGenesisCreatePromptInteractionEntry( context, prompt, visibleExpr );

			for( it = 0; it != eaSize( &volumeNames ); ++it ) {
				InteractableOverride* volumeInteractable = StructCreate( parse_InteractableOverride );
				volumeInteractable->pcInteractableName = allocAddString( volumeNames[ it ]);
				volumeInteractable->pcMapName = allocAddString( externalMapName );
				volumeInteractable->pPropertyEntry = StructClone( parse_WorldInteractionPropertyEntry, entry );

				eaPush( &context->root_mission_accum->ppInteractableOverrides, volumeInteractable );
			}
			
			StructDestroy( parse_WorldInteractionPropertyEntry, entry );
		}
	}
}

/// Create the Optional Action entry for PROMPT, using VISIBLE-EXPR as
/// the condition which triggers visibility.
///
/// Unlike ugcGenesisCreatePromptOptionalAction(), this does NOT hook up
/// the entry to any requirements, you must hook it up yourself.
WorldOptionalActionVolumeEntry* ugcGenesisCreatePromptOptionalActionEntry( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, const char* visibleExpr )
{
	WorldOptionalActionVolumeEntry* accum = StructCreate( parse_WorldOptionalActionVolumeEntry );
	
	assert( prompt->bOptional );

	if( !prompt->bOptionalHideOnComplete ) {
		accum->visible_cond = (!nullStr( visibleExpr ) ? exprCreateFromString( visibleExpr, NULL ) : NULL);
	} else {
		UGCGenesisMissionPrompt* hidePrompt;
		char* estr = NULL;

		if( nullStr( prompt->pcOptionalHideOnCompletePrompt )) {
			hidePrompt = prompt;
		} else {
			hidePrompt = ugcGenesisFindPrompt( context, prompt->pcOptionalHideOnCompletePrompt );

			if( !hidePrompt ) {
				ugcRaiseError( UGC_ERROR, ugcMakeTempErrorContextPrompt( prompt->pcName, NULL, SAFE_MEMBER( context->zone_mission, desc.pcName ), prompt->pcLayoutName ),
								   "Prompt should hide when prompt \"%s\" completes, but no such prompt exists.",
								   prompt->pcOptionalHideOnCompletePrompt );
			}
		}

		if( visibleExpr ) {
			estrPrintf( &estr, "(%s) and ", visibleExpr );
		}

		if( hidePrompt ) {
			char** completeBlockNames = ugcGenesisPromptBlockNames( context, prompt, true );
			int it;
			estrConcatf( &estr, "(" );
			for( it = 0; it != eaSize( &completeBlockNames ); ++it ) {
				estrConcatf( &estr, "%sHasRecentlyCompletedContactDialog(\"%s\",\"%s\") = 0",
							 (it ? " and " : ""),
							 ugcGenesisContactName( context, prompt ),
							 ugcGenesisSpecialDialogBlockNameTemp( hidePrompt->pcName, completeBlockNames[ it ]));
			}
			estrConcatf( &estr, ")" );
			eaDestroy( &completeBlockNames );
		} else {
			estrConcatf( &estr, "1" );
		}

		accum->visible_cond = exprCreateFromString( estr, NULL );
		estrDestroy( &estr );
	}
	ugcGenesisCreateMessage( context, &accum->display_name_msg, prompt->pcOptionalButtonText );
	accum->auto_execute = prompt->bOptionalAutoExecute;
	if( accum->auto_execute ) {
		accum->enabled_cond = exprCreateFromString( "not PlayerIsInCombat()", NULL );
	}
	accum->category_name = StructAllocString( prompt->pcOptionalCategoryName );
	accum->priority = prompt->eOptionalPriority;
	
	eaPush( &accum->actions.eaActions, ugcGenesisCreatePromptAction( context, prompt ));

	return accum;
}

/// Create the Optional Action entry for PROMPT, using VISIBLE-EXPR as
/// the condition which triggers visibility.
///
/// Unlike ugcGenesisCreatePromptOptionalAction(), this does NOT hook up
/// the entry to any requirements, you must hook it up yourself.
WorldInteractionPropertyEntry* ugcGenesisCreatePromptInteractionEntry( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, const char* visibleExpr )
{
	WorldInteractionPropertyEntry* accum = StructCreate( parse_WorldInteractionPropertyEntry );
	accum->pcInteractionClass = allocAddString( "CONTACT" );
	assert( prompt->bOptional );

	if( !prompt->bOptionalHideOnComplete ) {
		accum->pInteractCond = (!nullStr( visibleExpr ) ? exprCreateFromString( visibleExpr, NULL ) : NULL);
	} else {
		UGCGenesisMissionPrompt* hidePrompt;
		char* estr = NULL;

		if( nullStr( prompt->pcOptionalHideOnCompletePrompt )) {
			hidePrompt = prompt;
		} else {
			hidePrompt = ugcGenesisFindPrompt( context, prompt->pcOptionalHideOnCompletePrompt );

			if( !hidePrompt ) {
				ugcRaiseError( UGC_ERROR, ugcMakeTempErrorContextPrompt( prompt->pcName, NULL, SAFE_MEMBER( context->zone_mission, desc.pcName ), prompt->pcLayoutName ),
								   "Prompt should hide when prompt \"%s\" completes, but no such prompt exists.",
								   prompt->pcOptionalHideOnCompletePrompt );
			}
		}

		if( visibleExpr ) {
			estrPrintf( &estr, "(%s) and ", visibleExpr );
		}
		
		if( prompt->bOptionalAutoExecute ) {
			estrConcatf( &estr, "not PlayerIsInCombat() and " );
		}

		if( hidePrompt ) {
			char** completeBlockNames = ugcGenesisPromptBlockNames( context, hidePrompt, true );
			int it;

			estrConcatf( &estr, "(" );
			for( it = 0; it != eaSize( &completeBlockNames ); ++it ) {
				estrConcatf( &estr, "%sHasRecentlyCompletedContactDialog(\"%s\",\"%s\") = 0",
							 (it ? " and " : ""),
							 ugcGenesisContactName( context, prompt ),
							 ugcGenesisSpecialDialogBlockNameTemp( hidePrompt->pcName, completeBlockNames[ it ]));
			}
			estrConcatf( &estr, ")" );
			eaDestroy( &completeBlockNames );
		} else {
			estrConcatf( &estr, "1" );
		}

		accum->pInteractCond = exprCreateFromString( estr, NULL );
		estrDestroy( &estr );
	}

	accum->pTextProperties = StructCreate( parse_WorldTextInteractionProperties );
	ugcGenesisCreateMessage( context, &accum->pTextProperties->interactOptionText, prompt->pcOptionalButtonText );
	
	accum->bAutoExecute = prompt->bOptionalAutoExecute;
	accum->pcCategoryName = StructAllocString( prompt->pcOptionalCategoryName );
	accum->iPriority = prompt->eOptionalPriority;

	accum->pContactProperties = StructCreate( parse_WorldContactInteractionProperties );
	SET_HANDLE_FROM_STRING( g_ContactDictionary, ugcGenesisContactName( context, prompt ),
							accum->pContactProperties->hContactDef );
	accum->pContactProperties->pcDialogName = StructAllocString( prompt->pcName );

	return accum;
}

/// Return a ContactMissionOffer for MISSION-NAME.
///
/// If IS-RETURN-ONLY, then this Contact will not be shared with any
/// granting mission offers.
ContactMissionOffer* ugcGenesisInternMissionOffer( UGCGenesisMissionContext* context, const char* mission_name, bool isReturnOnly )
{
	char buffer[256];
	char* fix = NULL;
	const char* contactName;

	sprintf( buffer, "%s%s",
			 ugcGenesisContactName( context, NULL ),
			 (isReturnOnly ? "_Return" : "") );

	if( resFixName( buffer, &fix )) {
		contactName = allocAddString( fix );
		estrDestroy( &fix );
	} else {
		contactName = allocAddString( buffer );
	}

	{
		ContactDef* contactAccum = ugcGenesisInternContactDef( context, contactName );
		int it;
		ContactMissionOffer** eaOfferList = NULL;

		contact_GetMissionOfferList(contactAccum, NULL, &eaOfferList);

		contactAccum->type = ContactType_List;
		
		for( it = 0; it != eaSize( &eaOfferList ); ++it ) {
			ContactMissionOffer* pOffer = eaOfferList[ it ];
			if( stricmp( REF_STRING_FROM_HANDLE( pOffer->missionDef ), ugcGenesisMissionName( context, true )) == 0 ) {
				eaDestroy(&eaOfferList);
				return pOffer;
			}
		}

		if(eaOfferList)
			eaDestroy(&eaOfferList);

		{
			ContactMissionOffer* offerAccum = StructCreate( parse_ContactMissionOffer );
			SET_HANDLE_FROM_STRING( g_MissionDictionary, ugcGenesisMissionName( context, true ), offerAccum->missionDef );
			eaPush( &contactAccum->offerList, offerAccum );
			return offerAccum;
		}
	}
}

/// Return a requirement for the mission.
UGCGenesisMissionRequirements* ugcGenesisInternRequirement( UGCGenesisMissionContext* context )
{
	assert( context->req_accum );

	return context->req_accum;
}

/// Return the extra volume named VOLUME-NAME.
///
/// If no such extra volume exists, create one.
UGCGenesisMissionExtraVolume* ugcGenesisInternExtraVolume( UGCGenesisMissionContext* context, const char* volume_name )
{
	UGCGenesisMissionRequirements* req = ugcGenesisInternRequirement( context );
	int it;
	for( it = 0; it != eaSize( &req->extraVolumes ); ++it ) {
		UGCGenesisMissionExtraVolume* extraVolume = req->extraVolumes[ it ];
		if( stricmp( volume_name, extraVolume->volumeName ) == 0 ) {
			return extraVolume;
		}
	}

	{
		UGCGenesisMissionExtraVolume* accum = StructCreate( parse_UGCGenesisMissionExtraVolume );
		accum->volumeName = StructAllocString( volume_name );
		eaPush( &req->extraVolumes, accum );
		return accum;
	}
}

/// Return a room requirement params for the room with name ROOM_NAME.
UGCGenesisProceduralObjectParams* ugcGenesisInternRoomRequirementParams( UGCGenesisMissionContext* context, const char* layout_name, const char* room_name )
{
	assert( context->req_accum );

	{
		int it;
		for( it = 0; it != eaSize( &context->req_accum->roomRequirements ); ++it ) {
			UGCGenesisMissionRoomRequirements* roomReq = context->req_accum->roomRequirements[ it ]; 
			if( stricmp( roomReq->layoutName, layout_name ) == 0 && stricmp( roomReq->roomName, room_name ) == 0 ) {
				return roomReq->params;
			}
		}
	}

	{
		UGCGenesisMissionRoomRequirements* newRoomReq = StructCreate( parse_UGCGenesisMissionRoomRequirements );
		newRoomReq->layoutName = StructAllocString( layout_name );
		newRoomReq->roomName = StructAllocString( room_name );
		newRoomReq->params = StructCreate( parse_UGCGenesisProceduralObjectParams );
		eaPush( &context->req_accum->roomRequirements, newRoomReq );
		return newRoomReq->params;
	}
}

/// Return a room requirement for the room with name ROOM_NAME.
UGCGenesisMissionRoomRequirements* ugcGenesisInternRoomRequirements( UGCGenesisMissionContext* context, const char* layout_name, const char* room_name )
{
	assert( context->req_accum );

	{
		int it;
		for( it = 0; it != eaSize( &context->req_accum->roomRequirements ); ++it ) {
			UGCGenesisMissionRoomRequirements* roomReq = context->req_accum->roomRequirements[ it ]; 
			if( stricmp( roomReq->layoutName, layout_name ) == 0 && stricmp( roomReq->roomName, room_name ) == 0 ) {
				return roomReq;
			}
		}
	}

	{
		UGCGenesisMissionRoomRequirements* newRoomReq = StructCreate( parse_UGCGenesisMissionRoomRequirements );
		newRoomReq->layoutName = StructAllocString( layout_name );
		newRoomReq->roomName = StructAllocString( room_name );
		newRoomReq->params = StructCreate( parse_UGCGenesisProceduralObjectParams );
		eaPush( &context->req_accum->roomRequirements, newRoomReq );
		return newRoomReq;
	}
}

/// Return a room requirement for the starting room.
UGCGenesisProceduralObjectParams* ugcGenesisInternStartRoomRequirementParams( UGCGenesisMissionContext* context )
{
	return ugcGenesisInternRoomRequirementParams( context, context->zone_mission->desc.startDescription.pcStartLayout, context->zone_mission->desc.startDescription.pcStartRoom );
}

/// Return a challenge requirement for the challenge with name
/// CHALLENGE_NAME.
UGCGenesisInstancedObjectParams* ugcGenesisInternChallengeRequirementParams( UGCGenesisMissionContext* context, const char* challenge_name )
{
	assert( context->req_accum );

	// MJF TODO: remove reference to syntactical recognition of shared
	// challenges
	if( strStartsWith( challenge_name, "Shared_" )) {
		static UGCGenesisMissionChallengeRequirements req = { 0 };

		ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextChallenge( challenge_name, SAFE_MEMBER( context->zone_mission, desc.pcName ), NULL ),
							  "Trying to specify mission-specific requirements is not supported for shared challenges." );
		StructReset( parse_UGCGenesisMissionChallengeRequirements, &req );
	}

	{
		int it;
		for( it = 0; it != eaSize( &context->req_accum->challengeRequirements ); ++it ) {
			UGCGenesisMissionChallengeRequirements* challengeReq = context->req_accum->challengeRequirements[ it ];
			if( stricmp( challengeReq->challengeName, challenge_name ) == 0 ) {
				if (!challengeReq->params)
					challengeReq->params = StructCreate( parse_UGCGenesisInstancedObjectParams );
				return challengeReq->params;
			}
		}
	}

	{
		UGCGenesisMissionChallengeRequirements* newChallengeReq = StructCreate( parse_UGCGenesisMissionChallengeRequirements );
		newChallengeReq->challengeName = StructAllocString( challenge_name );
		newChallengeReq->params = StructCreate( parse_UGCGenesisInstancedObjectParams );
		eaPush( &context->req_accum->challengeRequirements, newChallengeReq );
		return newChallengeReq->params;
	}
}

/// Return a challenge interact requirement for the challenge with name
/// CHALLENGE_NAME.
UGCGenesisInteractObjectParams* ugcGenesisInternInteractRequirementParams( UGCGenesisMissionContext* context, const char* challenge_name )
{
	assert( context->req_accum );

	// MJF TODO: remove reference to syntaticall recognition of shared
	// challenges
	if( strStartsWith( challenge_name, "Shared_" )) {
		static UGCGenesisMissionChallengeRequirements req = { 0 };

		ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextChallenge( challenge_name, SAFE_MEMBER( context->zone_mission, desc.pcName ), NULL ),
							  "Trying to specify mission-specific requirements is not supported for shared challenges." );
		StructReset( parse_UGCGenesisMissionChallengeRequirements, &req );
	}

	{
		int it;
		for( it = 0; it != eaSize( &context->req_accum->challengeRequirements ); ++it ) {
			UGCGenesisMissionChallengeRequirements* challengeReq = context->req_accum->challengeRequirements[ it ];
			if( stricmp( challengeReq->challengeName, challenge_name ) == 0 ) {
				if (!challengeReq->interactParams)
					challengeReq->interactParams = StructCreate( parse_UGCGenesisInteractObjectParams );
				return challengeReq->interactParams;
			}
		}
	}

	{
		UGCGenesisMissionChallengeRequirements* newChallengeReq = StructCreate( parse_UGCGenesisMissionChallengeRequirements );
		newChallengeReq->challengeName = StructAllocString( challenge_name );
		newChallengeReq->interactParams = StructCreate( parse_UGCGenesisInteractObjectParams );
		newChallengeReq->interactParams->bDisallowVolume = true;
		eaPush( &context->req_accum->challengeRequirements, newChallengeReq );
		return newChallengeReq->interactParams;
	}
}

/// Return a volume requirement for the challenge with name
/// CHALLENGE_NAME.
UGCGenesisProceduralObjectParams* ugcGenesisInternVolumeRequirementParams( UGCGenesisMissionContext* context, const char* challenge_name )
{
	assert( context->req_accum );

	// MJF TODO: remove reference to syntaticall recognition of shared
	// challenges
	if( strStartsWith( challenge_name, "Shared_" )) {
		static UGCGenesisMissionChallengeRequirements req = { 0 };

		ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextChallenge( challenge_name, SAFE_MEMBER( context->zone_mission, desc.pcName ), NULL ),
							  "Trying to specify mission-specific requirements is not supported for shared challenges." );
		StructReset( parse_UGCGenesisMissionChallengeRequirements, &req );
	}

	{
		int it;
		for( it = 0; it != eaSize( &context->req_accum->challengeRequirements ); ++it ) {
			UGCGenesisMissionChallengeRequirements* challengeReq = context->req_accum->challengeRequirements[ it ];
			if( stricmp( challengeReq->challengeName, challenge_name ) == 0 ) {
				if (!challengeReq->volumeParams)
					challengeReq->volumeParams = StructCreate( parse_UGCGenesisProceduralObjectParams );
				return challengeReq->volumeParams;
			}
		}
	}

	{
		UGCGenesisMissionChallengeRequirements* newChallengeReq = StructCreate( parse_UGCGenesisMissionChallengeRequirements );
		newChallengeReq->challengeName = StructAllocString( challenge_name );
		newChallengeReq->volumeParams = StructCreate( parse_UGCGenesisProceduralObjectParams );
		eaPush( &context->req_accum->challengeRequirements, newChallengeReq );
		return newChallengeReq->volumeParams;
	}
}

/// Return an interactable property entry that should be filled out
/// for a requirement for CHALLENGE_NAME.
WorldInteractionPropertyEntry* ugcGenesisCreateInteractableChallengeRequirement( UGCGenesisMissionContext* context, const char* challenge_name )
	
{
	UGCGenesisInteractObjectParams* params = ugcGenesisInternInteractRequirementParams( context, challenge_name );
	WorldInteractionPropertyEntry* entry = StructCreate( parse_WorldInteractionPropertyEntry );

	eaPush( &params->eaInteractionEntries, entry );
	return entry;
}

/// Return an interactable property entry that should be filled out
/// for a requirement for CHALLENGE_NAME's volume.
WorldInteractionPropertyEntry* ugcGenesisCreateInteractableChallengeVolumeRequirement( UGCGenesisMissionContext* context, const char* challenge_name )
	
{
	UGCGenesisProceduralObjectParams* params = ugcGenesisInternVolumeRequirementParams( context, challenge_name );
	WorldInteractionPropertyEntry* entry = StructCreate( parse_WorldInteractionPropertyEntry );

	if (!params->interaction_properties)
		params->interaction_properties = StructCreate(parse_WorldInteractionProperties);
	eaPush( &params->interaction_properties->eaEntries, entry );
	return entry;
}

/// Return a dialog block with text DIALOG-TEXT
DialogBlock* ugcGenesisCreateDialogBlock( UGCGenesisMissionContext* context, char* dialogText, const char* astrAnimList )
{
	DialogBlock* accum = StructCreate( parse_DialogBlock );
	if( nullStr( dialogText )) {
		dialogText = " ";
	}
	
	ugcGenesisCreateMessage( context, &accum->displayTextMesg.msg, dialogText );
	if( astrAnimList ) {
		SET_HANDLE_FROM_STRING( "AIAnimList", astrAnimList, accum->hAnimList );
	}

	return accum;
}

void ugcGenesisRefSystemUpdate( DictionaryHandleOrName dict, const char* key, void* obj )
{
	void* oldObj = RefSystem_ReferentFromString( dict, key );

	if( oldObj ) {
		RefSystem_MoveReferent( obj, oldObj );
	} else {
		RefSystem_AddReferent( dict, key, obj );
	}
}

void ugcGenesisRunValidate( DictionaryHandleOrName dict, const char* key, void* obj )
{
	resRunValidate(RESVALIDATE_POST_TEXT_READING, dict, key, obj, -1, NULL);
	resRunValidate(RESVALIDATE_POST_BINNING, dict, key, obj, -1, NULL);
	resRunValidate(RESVALIDATE_FINAL_LOCATION, dict, key, obj, -1, NULL);
}

/// Call ParserWriteTextFileFromDictionary, and display error dialogs
/// as appropriate.
void ugcGenesisWriteTextFileFromDictionary( const char* filename, DictionaryHandleOrName dict )
{
	if( filename && !ParserWriteTextFileFromDictionary( filename, dict, 0, 0 )) {
		ErrorFilenamef( filename, "Unable to write out file.  Is it not checked out?" );
	}
}

/// Create a DisplayMessage containing DEFAULT-STRING.
///
/// This also applies substitution rules, which take substring like
/// {Thing} and replaces it with the name for Thing.
void ugcGenesisCreateMessage( UGCGenesisMissionContext* context, DisplayMessage* dispMessage, const char* defaultString )
{
	if( defaultString ) {
		char* estrDefaultString = estrCreateFromStr( defaultString );

		// {Genesis.MissionName}
		if( context && !context->is_ugc ) {
			estrReplaceOccurrences_CaseInsensitive( &estrDefaultString, "{Genesis.MissionName}", context->zone_mission->desc.pcDisplayName );
		}

		// {Genesis.MapName}
		if( context && !context->is_ugc ) {
			DisplayMessage* mapNameDispMsg = zmapInfoGetDisplayNameMessage( context->zmap_info );
			const char* mapName = NULL;

			if( mapNameDispMsg->pEditorCopy ) {
				mapName = SAFE_MEMBER( mapNameDispMsg->pEditorCopy, pcDefaultString );
			} else {
				mapName = SAFE_MEMBER( GET_REF( mapNameDispMsg->hMessage ), pcDefaultString );
			}
		
			if( !mapName ) {
				mapName = zmapInfoGetPublicName( context->zmap_info );
			}

			estrReplaceOccurrences_CaseInsensitive( &estrDefaultString, "{Genesis.MapName}", mapName );
			estrReplaceOccurrences_CaseInsensitive( &estrDefaultString, "{Genesis.SystemName}", mapName );
		}

		// prevent macroexpansion of langCreateMessage on this ONE line.
		if(!dispMessage->pEditorCopy)
			dispMessage->pEditorCopy = (langCreateMessage)( NULL, NULL, NULL, estrDefaultString );
		else
			dispMessage->pEditorCopy->pcDefaultString = StructAllocString(estrDefaultString);

		dispMessage->bEditorCopyIsServer = true;

		estrDestroy( &estrDefaultString );
	}
}

static void ugcGenesisPushRoomNames( char* missionName, UGCGenesisZoneMapRoom** rooms, UGCGenesisZoneMapPath** paths, char*** nameList )
{
	int it;
	for( it = 0; it != eaSize( &rooms ); ++it ) {
		char buffer[ 256 ];
		sprintf( buffer, "%s_%s", missionName, rooms[ it ]->room.name );
		eaPush( nameList, strdup( buffer ));
	}
	for( it = 0; it != eaSize( &paths ); ++it ) {
		char buffer[ 256 ];
		sprintf( buffer, "%s_%s", missionName, paths[ it ]->path.name );
		eaPush( nameList, strdup( buffer ));
	}
}

void ugcGenesisPushAllRoomNames( UGCGenesisMissionContext* context, char*** nameList )
{
	int i;
	char* missionName = context->zone_mission->desc.pcName;

	if( context->genesis_data ) {
		for ( i=0; i < eaSize(&context->genesis_data->solar_systems); i++ ) {
			int pointListIt;
			int pointIt;
			UGCGenesisSolSysZoneMap *solar_system = context->genesis_data->solar_systems[ i ];
			UGCGenesisShoebox* shoebox = &solar_system->shoebox;
			for( pointListIt = 0; pointListIt != eaSize( &shoebox->point_lists ); ++pointListIt ) {
				UGCGenesisShoeboxPointList* pointList = solar_system->shoebox.point_lists[ pointListIt ];
				for( pointIt = 0; pointIt != eaSize( &pointList->points ); ++pointIt ) {
					UGCGenesisShoeboxPoint* point = pointList->points[ pointIt ];
					char buffer[ 256 ];
					sprintf( buffer, "%s_%s", missionName, point->name  );
					eaPush( nameList, strdup( buffer ));
				}
			}
		}
	
		for ( i=0; i < eaSize(&context->genesis_data->genesis_interiors); i++ )
		{
			UGCGenesisZoneMapRoom** rooms = context->genesis_data->genesis_interiors[ i ]->rooms;
			UGCGenesisZoneMapPath** paths = context->genesis_data->genesis_interiors[ i ]->paths;
			ugcGenesisPushRoomNames(missionName, rooms, paths, nameList);
		}
		if( context->genesis_data->genesis_exterior ) {
			UGCGenesisZoneMapRoom** rooms = context->genesis_data->genesis_exterior->rooms;
			UGCGenesisZoneMapPath** paths = context->genesis_data->genesis_exterior->paths;
			ugcGenesisPushRoomNames(missionName, rooms, paths, nameList);
		}
	}
}

void ugcGenesisTransmogrifyWhenFixup( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisWhen *when, UGCRuntimeErrorContext *error_context )
{
	int it;
	for( it = 0; it != eaSize( &when->eaChallengeNames ); ++it ) {
		char* oldChallengeName = when->eaChallengeNames[ it ];
		bool isShared;

		if( ugcGenesisFindChallenge( context->map_desc, context->mission_desc, oldChallengeName, &isShared )) {
			char newChallengeName[ 256 ];
			sprintf( newChallengeName, "%s_%s",
					 (isShared ? "Shared" : context->mission_desc->zoneDesc.pcName),
					 oldChallengeName );
			when->eaChallengeNames[ it ] = StructAllocString( newChallengeName );
			StructFreeString( oldChallengeName );
		} else {
			ugcRaiseError( UGC_ERROR, error_context,
							   "When references challenge \"%s\", but it does not exist.",
							   when->eaChallengeNames[ it ]);
			eaRemove( &when->eaChallengeNames, it );
			--it;
		}
	}

	if( when->pcPromptChallengeName ) {
		ugcGenesisChallengeNameFixup( context, &when->pcPromptChallengeName );
	}

}

/// Transmogrify a prompt
void ugcGenesisTransmogrifyPromptFixup( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisMissionPrompt* prompt )
{
	ugcGenesisTransmogrifyWhenFixup(context, &prompt->showWhen, ugcMakeTempErrorContextPrompt( prompt->pcName, NULL, SAFE_MEMBER(context->mission_desc, zoneDesc.pcName ), prompt->pcLayoutName ));

	if( !nullStr( prompt->pcChallengeName )) {
		ugcGenesisChallengeNameFixup( context, &prompt->pcChallengeName );
	}
}

/// Transmogrify an FSM
void ugcGenesisTransmogrifyFSMFixup(UGCGenesisTransmogrifyMissionContext* context, UGCGenesisFSM *gfsm)
{
	if(!nullStr(gfsm->pcChallengeLogicalName))
	{
		ugcGenesisChallengeNameFixup(context, &gfsm->pcChallengeLogicalName);
	}
}

/// Transmogrify a portal
void ugcGenesisTransmogrifyPortalFixup( UGCGenesisTransmogrifyMissionContext* context, UGCGenesisMissionPortal *portal )
{
	ugcGenesisTransmogrifyWhenFixup(context, &portal->when, ugcMakeTempErrorContextPortal( portal->pcName, SAFE_MEMBER(context->mission_desc, zoneDesc.pcName ), portal->pcStartLayout ));
}

/// Create a prompt for a UGCGenesisMissionPrompt
void ugcGenesisCreatePrompt( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt )
{
	const char* contactName = ugcGenesisContactName( context, prompt );
	ContactDef* defAccum = ugcGenesisInternContactDef( context, contactName );
	
	SpecialDialogBlock* primarySpecialDialog = ugcGenesisCreatePromptBlock( context, prompt, -1 );
	SpecialDialogBlock** specialDialogs = NULL;
	{
		int it;
		for( it = 0; it != eaSize( &prompt->namedBlocks ); ++it ) {
			eaPush( &specialDialogs, ugcGenesisCreatePromptBlock( context, prompt, it ));
		}
	}

	if( prompt->showWhen.type != UGCGenesisWhen_Manual )
	{
		UGCRuntimeErrorContext* debugContext
			= ugcMakeTempErrorContextPrompt( prompt->pcName, NULL, SAFE_MEMBER( context->zone_mission, desc.pcName ), prompt->pcLayoutName );
		char* exprText;

		// Prompts that are are objective complete of a single
		// objective should actually be during the AfterPrompt_
		// objective.
		//
		// NOTE: may be better to fix up all these prompts in a
		// preprocessing pass.
		{
			UGCGenesisWhen showWhen = { 0 };
			StructCopyAll( parse_UGCGenesisWhen, &prompt->showWhen, &showWhen );

			if( showWhen.type == UGCGenesisWhen_ObjectiveComplete && eaSize( &showWhen.eaObjectiveNames ) == 1 ) {
				char afterPromptObjectiveName[ 256 ];

				sprintf( afterPromptObjectiveName, "AfterPrompt_%s", showWhen.eaObjectiveNames[ 0 ]);
				showWhen.type = UGCGenesisWhen_ObjectiveInProgress;
				StructCopyString( &showWhen.eaObjectiveNames[ 0 ], afterPromptObjectiveName );
			}
			
			exprText = ugcGenesisWhenExprText( context, &showWhen, debugContext, "ShowWhen", false );
			StructDeInit( parse_UGCGenesisWhen, &showWhen );
		}
		
		if( prompt->pcExternalContactName ) {
			primarySpecialDialog->bUsesLocalCondExpression = true;
			primarySpecialDialog->pCondition = exprCreateFromString( exprText, NULL );
		} else if( prompt->pcChallengeName ) {
			if( eaSize( &prompt->eaExternalMapNames ) == 0 ) { 
				UGCGenesisInstancedObjectParams* params = ugcGenesisInternChallengeRequirementParams( context, prompt->pcChallengeName );
				UGCGenesisMissionPromptExprPair* pair;
				if (!params->pContact)
				{
					params->pContact = StructCreate(parse_UGCGenesisMissionContactRequirements);
				}
				pair = StructCreate( parse_UGCGenesisMissionPromptExprPair );
				pair->name = StructAllocString( prompt->pcName );
				pair->exprText = StructAllocString( exprText );
				eaPush(&params->pContact->eaPrompts, pair);
				StructCopyString(&params->pContact->pcContactFileName, contactName);
			} else {
				InteractableOverride* contactProp = StructCreate( parse_InteractableOverride );
				WorldInteractionPropertyEntry* entry = StructCreate( parse_WorldInteractionPropertyEntry );
				eaPush( &context->root_mission_accum->ppInteractableOverrides, contactProp );
				contactProp->pPropertyEntry = entry;

				contactProp->pcMapName = allocAddString( prompt->eaExternalMapNames[ 0 ]);
				contactProp->pcInteractableName = allocAddString( prompt->pcChallengeName );

				// interaction properties
				entry->pcInteractionClass = allocAddString( "CONTACT" );
				entry->pContactProperties = StructCreate( parse_WorldContactInteractionProperties );
				SET_HANDLE_FROM_STRING( g_ContactDictionary, ugcGenesisContactName( context, prompt ),
										entry->pContactProperties->hContactDef );
				entry->pContactProperties->pcDialogName = StructAllocString( prompt->pcName );
				entry->pTextProperties = StructCreate( parse_WorldTextInteractionProperties );
				entry->pInteractCond = exprCreateFromString( exprText, NULL );

				if( eaSize( &prompt->eaExternalMapNames ) > 1 ) {
					ugcRaiseErrorContext( UGC_ERROR, debugContext,
										  "Trying to set interaction properties "
										  "on a specific external contact with "
										  "multiple maps specified." );
				}
			}
		} else {
			if( !prompt->bOptional ) {
				// Making it an optional action prevents prompts from not
				// showing due to there already being a prompt up.  This is
				// better because prompts may be necesarry for missions to
				// work.
				//
				// One exception for UGC -- if this is the start prompt for an
				// external map, we'll assume wherever you got the
				// mission is a safe area.
				if( !exprText && !context->zmap_info ) {
					eaPush( &context->root_mission_accum->ppOnStartActions, ugcGenesisCreatePromptAction( context, prompt ));
				} else {
					UGCGenesisMissionPrompt promptAsOptional = { 0 };
					StructCopyAll( parse_UGCGenesisMissionPrompt, prompt, &promptAsOptional );
					promptAsOptional.bOptional = true;
					promptAsOptional.pcOptionalButtonText = StructAllocString( ugcDefaultsFallbackPromptText() );
					promptAsOptional.bOptionalAutoExecute = true;
					promptAsOptional.bOptionalHideOnComplete = true;
					promptAsOptional.eOptionalPriority = WorldOptionalActionPriority_Low;

					ugcGenesisCreatePromptOptionalAction( context, &promptAsOptional, exprText );

					StructDeInit( parse_UGCGenesisMissionPrompt, &promptAsOptional );
				}
			} else {
				ugcGenesisCreatePromptOptionalAction( context, prompt, exprText );
			}
		}

		estrDestroy( &exprText );
	}

	if( prompt->pcExternalContactName ) {
		SpecialDialogOverride* override = StructCreate( parse_SpecialDialogOverride );
		override->pcContactName = prompt->pcExternalContactName;
		override->pSpecialDialog = primarySpecialDialog;
		eaPush( &context->root_mission_accum->ppSpecialDialogOverrides, override );

		{
			int otherIt;
			for( otherIt = 0; otherIt != eaSize( &specialDialogs ); ++otherIt ) {
				SpecialDialogBlock* otherDialog = specialDialogs[ otherIt ];
				SpecialDialogOverride* otherOverride = StructCreate( parse_SpecialDialogOverride );

				otherDialog->bUsesLocalCondExpression = true;
				otherDialog->pCondition = exprCreateFromString( "0", NULL );
				otherOverride->pcContactName = prompt->pcExternalContactName;
				otherOverride->pSpecialDialog = otherDialog;
				eaPush( &context->root_mission_accum->ppSpecialDialogOverrides, otherOverride );
			}
		}
	} else {
		eaPush( &defAccum->specialDialog, primarySpecialDialog );
		eaPushEArray( &defAccum->specialDialog, &specialDialogs );
	}
	
	eaDestroy( &specialDialogs );
}

SpecialDialogBlock* ugcGenesisCreatePromptBlock( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt, int blockIndex )
{
	UGCGenesisMissionPromptBlock* block = (blockIndex < 0 ? &prompt->sPrimaryBlock : prompt->namedBlocks[ blockIndex ]);
	SpecialDialogBlock* dialogAccum = StructCreate( parse_SpecialDialogBlock );

	dialogAccum->name = allocAddString( ugcGenesisSpecialDialogBlockNameTemp( prompt->pcName, block->name ));
	ugcGenesisMissionCostumeToContactCostume( &block->costume, &dialogAccum->costumePrefs );
	dialogAccum->costumePrefs.pchHeadshotStyle = allocAddString( block->pchHeadshotStyle );
	COPY_HANDLE( dialogAccum->hCutSceneDef, block->hCutsceneDef );
	dialogAccum->eIndicator = SpecialDialogIndicator_Important;
	dialogAccum->eFlags = block->eDialogFlags;
	dialogAccum->bDelayIfInCombat = true;
	
	ugcGenesisCreateMessage( context, &dialogAccum->displayNameMesg, block->pcTitleText );

	{
		int it;
		for( it = 0; it != eaSize( &block->eaBodyText ); ++it ) {
			eaPush( &dialogAccum->dialogBlock, ugcGenesisCreateDialogBlock( context, block->eaBodyText[ it ], REF_STRING_FROM_HANDLE( block->hAnimList )));

			if( it == 0 ) {
				int val = StaticDefineIntGetInt( ContactAudioPhrasesEnum, block->pcPhrase );
				if( val >= 0 ) {
					dialogAccum->dialogBlock[0]->ePhrase = val;
				}
			}
		}
	}

	{
		int actionIt;
		for( actionIt = 0; actionIt != eaSize( &block->eaActions ); ++actionIt ) {
			UGCGenesisMissionPromptAction* promptAction = block->eaActions[ actionIt ];
			SpecialDialogAction* actionAccum = StructCreate( parse_SpecialDialogAction );

			{
				char* estrText = NULL;
				estrPrintf( &estrText, "%s", promptAction->pcText ? promptAction->pcText : "Continue" );
				if( promptAction->astrStyleName ) {
					estrInsertf( &estrText, 0, "<font style=%s>", promptAction->astrStyleName );
					estrConcatf( &estrText, "</font>" );
				}
				ugcGenesisCreateMessage( context, &actionAccum->displayNameMesg, estrText );
				estrDestroy( &estrText );
			}
			{
				UGCRuntimeErrorContext* debugContext
					= ugcMakeTempErrorContextPrompt( prompt->pcName, NULL, SAFE_MEMBER( context->zone_mission, desc.pcName ), prompt->pcLayoutName );
				char* whenText = ugcGenesisWhenExprText( context, &promptAction->when, debugContext, "When", false );
				if( whenText ) {
					actionAccum->condition = exprCreateFromString( whenText, NULL );
				}
			}
			
			if(   stricmp( promptAction->pcNextPromptName, "MissionReturn" ) == 0
				  && stricmp( GetShortProductName(), "ST" ) == 0 ) {
				WorldGameActionProperties* returnAction = StructCreate( parse_WorldGameActionProperties );
				
				returnAction->eActionType = WorldGameActionType_SendNotification;
				returnAction->pSendNotificationProperties = StructCreate( parse_WorldSendNotificationActionProperties );
				returnAction->pSendNotificationProperties->pchNotifyType = StaticDefineIntRevLookup(NotifyTypeEnum, kNotifyType_RequestLeaveMap);
				ugcGenesisCreateMessage( context, &returnAction->pSendNotificationProperties->notifyMsg.msg, "XXX" );
				eaPush( &actionAccum->actionBlock.eaActions, returnAction );
			} else {
				if( !nullStr( promptAction->pcNextBlockName )) {
					char buffer[ 256 ];
					assert( nullStr( promptAction->pcNextPromptName ));

					sprintf( buffer, "%s_%s", prompt->pcName, promptAction->pcNextBlockName );

					if( prompt->pcExternalContactName ) {
						char overrideBuffer[ 256 ];
						sprintf( overrideBuffer, "%s/%s", ugcGenesisMissionName( context, false ), buffer );
						actionAccum->dialogName = allocAddString( overrideBuffer );
					} else {
						actionAccum->dialogName = allocAddString( buffer );
					}
				} else {
					UGCGenesisMissionPrompt* nextPrompt = ugcGenesisFindPrompt( context, promptAction->pcNextPromptName );
					
					if( nextPrompt ) {
						if( prompt->pcExternalContactName && prompt == nextPrompt ) {
							char overrideBuffer[ 256 ];
							sprintf( overrideBuffer, "%s/%s", ugcGenesisMissionName( context, false ), promptAction->pcNextPromptName );
							actionAccum->dialogName = allocAddString( overrideBuffer );
						} else {
							actionAccum->dialogName = allocAddString( promptAction->pcNextPromptName );
						}
					}
				}
			
				StructCopyAll( parse_WorldGameActionBlock, &promptAction->actionBlock, &actionAccum->actionBlock );
				if( promptAction->bGrantMission ) {
					WorldGameActionProperties* grantAccum = StructCreate( parse_WorldGameActionProperties );
					grantAccum->eActionType = WorldGameActionType_GrantMission;
					grantAccum->pGrantMissionProperties = StructCreate( parse_WorldGrantMissionActionProperties );
					SET_HANDLE_FROM_STRING( g_MissionDictionary, ugcGenesisMissionName( context, true ),
											grantAccum->pGrantMissionProperties->hMissionDef );
					eaPush( &actionAccum->actionBlock.eaActions, grantAccum );
				}
			}
			
			actionAccum->bSendComplete = !promptAction->bDismissAction && nullStr( promptAction->pcNextBlockName );

			if( promptAction->enabledCheckedAttrib ) {
				char* attribText = ugcGenesisCheckedAttribText( context, promptAction->enabledCheckedAttrib, ugcMakeTempErrorContextPrompt( prompt->pcName, dialogAccum->name, SAFE_MEMBER( context->zone_mission, desc.pcName ), NULL ), "EnabledCheckedAttrib", true );

				if( attribText ) {
					UGCCheckedAttribDef* checkedAttrib = ugcDefaultsCheckedAttribDef( promptAction->enabledCheckedAttrib->astrSkillName );
			
					actionAccum->canChooseCondition = exprCreateFromString( attribText, NULL );

					if( checkedAttrib && checkedAttrib->displayName ) {
						char* estrBuffer = NULL;
						estrPrintf( &estrBuffer, "(%s) %s", checkedAttrib->displayName, actionAccum->displayNameMesg.pEditorCopy->pcDefaultString );
						StructCopyString( &actionAccum->displayNameMesg.pEditorCopy->pcDefaultString, estrBuffer );
						estrDestroy( &estrBuffer );
					}
				}

				estrDestroy( &attribText );
			}
			
			eaPush( &dialogAccum->dialogActions, actionAccum );
		}
	}

	return dialogAccum;
}

void ugcGenesisBucketFSM(UGCGenesisMissionContext *context, ObjectFSMData ***dataArray, UGCGenesisFSM *fsm)
{
	if(!nullStr(fsm->pcChallengeLogicalName))
	{
		UGCGenesisInstancedObjectParams* params = ugcGenesisInternChallengeRequirementParams( context, fsm->pcChallengeLogicalName );
		char *name = NULL;
		ObjectFSMData *data = NULL;
			
		FOR_EACH_IN_EARRAY(*dataArray, ObjectFSMData, test)
		{
			if(stricmp(test->challengeName, fsm->pcChallengeLogicalName ) == 0)
			{
				data = test;
				break;
			}
		}
		FOR_EACH_END;

		if(!data)
		{
			data = calloc(1, sizeof(ObjectFSMData));
			data->challengeName = strdup( fsm->pcChallengeLogicalName );
			eaPush(dataArray, data);
		}

		eaPush(&data->fsms, fsm);
	}
}

FSMState* ugcFsmCreateState(FSM *fsm, const char *stateName)
{
	FSMState *state = StructCreate(parse_FSMState);
	state->name = allocAddString(stateName);

	eaPush(&fsm->states, state);

	return state;
}

void ugcFsmStateSetAction(FSM *fsm, FSMState *state, const char* expr)
{
	if(!nullStr(expr))
		state->action = exprCreateFromString(expr, fsm->fileName);
}

void ugcFsmStateSetOnEntry(FSM *fsm, FSMState *state, const char* onEntry, const char* onEntryFirst)
{
	if(!nullStr(onEntry))
		state->onEntry = exprCreateFromString(onEntry, fsm->fileName);
	if(!nullStr(onEntryFirst))
		state->onFirstEntry = exprCreateFromString(onEntryFirst, fsm->fileName);
}

FSMTransition* ugcFsmStateAddTransition(FSM *fsm, FSMState *state, FSMState *target, const char* cond, const char* action)
{
	FSMTransition *transition = StructCreate(parse_FSMTransition);

	if(!nullStr(cond))
		transition->expr = exprCreateFromString(cond, fsm->fileName);
	if(!nullStr(action))
		transition->action = exprCreateFromString(action, fsm->fileName);
	transition->targetName = (char*)target->name;

	eaPush(&state->transitions, transition);

	return transition;
}

S32 ugcGenesisSortFSMs(UGCGenesisMissionContext *context, ObjectFSMData *data)
{
	int curObj = 0;
	int matched0th = false;
	int curFSM;
	ObjectFSMTempData *tmpData = NULL;
	
	for(curFSM=0; curFSM<eaSize(&data->fsms); curFSM++)
	{
		UGCGenesisFSM *fsm = data->fsms[curFSM];

		if(fsm->activeWhen.type==UGCGenesisWhen_MapStart)
		{
			tmpData = calloc(1, sizeof(ObjectFSMTempData));
			tmpData->fsm = fsm;

			if(curObj==0)
				matched0th = true;

			eaPush(&data->fsmsAndStates, tmpData);
		}
	}

	curObj = 0;
	for(curObj=0; curObj<eaSize(&context->zone_mission->desc.eaObjectives); curObj++)
	{
		int i;
		const char* objName = context->zone_mission->desc.eaObjectives[curObj]->pcName;
		int dontPush = false;

		tmpData = NULL;
		for(curFSM=0; curFSM<eaSize(&data->fsms); curFSM++)
		{
			UGCGenesisFSM *fsm = data->fsms[curFSM];

			if(!eaSize(&fsm->activeWhen.eaObjectiveNames))
				continue;

			for(i=0; i<eaSize(&fsm->activeWhen.eaObjectiveNames); i++)
			{
				if(!stricmp(fsm->activeWhen.eaObjectiveNames[i], objName))
				{
					if(i!=0)  // Only push one for the initial
						dontPush = true;
					else
					{
						tmpData = calloc(1, sizeof(ObjectFSMTempData));
						tmpData->fsm = fsm;

						if(curObj==0)
							matched0th = true;
					}
					break;
				}
			}
		}

		if(dontPush)
			continue;

		if(!tmpData)
			tmpData = calloc(1, sizeof(ObjectFSMTempData));

		tmpData->objName = objName;

		eaPush(&data->fsmsAndStates, tmpData);
	}

	eaDestroy(&data->fsms);  // Clean up and ensure earray isn't used

	return matched0th;
}

FSM* ugcGenesisGenerateSubFSM(UGCGenesisMissionContext *context, UGCGenesisFSM *gfsm)
{
	/*
	Refactor, or copy if not possible, the code that makes the primary FSM
	*/

	assert(0);
	return NULL;
}

const char* ugcGenesisExternVarGetMsgKey(UGCGenesisMissionContext *context, const char* varPrefix, WorldVariableDef* def)
{
	char keyBuffer[1024];
	sprintf(keyBuffer, "%s.ExternVar.%s", varPrefix, def->pcName);
	return allocAddString(keyBuffer);
}

void ugcGenesisFSMStateProcessVars(UGCGenesisMissionContext *context, const char* varPrefix, WorldVariableDef **vars, char **estrOut)
{
	estrClear(estrOut);
	FOR_EACH_IN_EARRAY(vars, WorldVariableDef, var)
	{
		char *valueStr = NULL;
		char *typeStr = NULL;
		MultiVal val = {0};

		var->pSpecificValue->eType = var->eType;

		worldVariableToMultival(NULL, var->pSpecificValue, &val);
		MultiValToEString(&val, &valueStr);

		switch(var->eType)
		{
			xcase WVAR_STRING: {
				typeStr = "String";
			}
			xcase WVAR_INT: {
				typeStr = "Int";
			}
			xcase WVAR_FLOAT: {
				typeStr = "Float";
			}
			xcase WVAR_ANIMATION: {
				typeStr = "String";
			}
			xcase WVAR_MESSAGE: {
				DisplayMessage fsmMessage = { 0 };
				char scopeBuffer[ 256 ];
				typeStr = "String";

				// Have to reset the value every time, in case it's a republish, which changes the namespace
				estrPrintf(&valueStr, "%s", ugcGenesisExternVarGetMsgKey(context, varPrefix, var));
				sprintf( scopeBuffer, "ExternVar%d", FOR_EACH_IDX( vars, var ));

				// Also need to store the message
				ugcGenesisCreateMessage( context, &fsmMessage, var->pSpecificValue->pcStringVal );
				langFixupMessage( fsmMessage.pEditorCopy, valueStr, "Message for extern var", scopeBuffer );
				
				eaPush( &context->extra_messages_accum->messages, StructClone( parse_DisplayMessage, &fsmMessage ));
				StructReset( parse_DisplayMessage, &fsmMessage );
			}
		}

		if(!stricmp(typeStr, "String"))
			estrConcatf(estrOut, "OverrideExtern%sVarCurState(\"encounter\", \"%s\",\"%s\"); ", typeStr, var->pcName, valueStr);
		else
			estrConcatf(estrOut, "OverrideExtern%sVarCurState(\"encounter\", \"%s\",%s); ", typeStr, var->pcName, valueStr);

		estrDestroy(&valueStr);
		MultiValClear(&val);
	}
	FOR_EACH_END
}

UGCGenesisMissionObjective* ugcGenesisFindMissionObjective(UGCGenesisMissionContext *context, const char* name)
{
	FOR_EACH_IN_EARRAY(context->zone_mission->desc.eaObjectives, UGCGenesisMissionObjective, obj)
	{
		if(!stricmp(obj->pcName, name))
			return obj;
	}
	FOR_EACH_END

	return NULL;
}

void ugcGenesisWhenFSMExprTextAndEventsFromObjective(UGCGenesisMissionContext *context, const char *objectiveName, char **condOut, char **actionOut, char **eventListenOut)
{
	char *evStr = NULL;
	UGCGenesisMissionObjective *objective = ugcGenesisFindMissionObjective(context, objectiveName);
	GameEvent *ev = ugcGenesisCompleteObjectiveEvent( objective, zmapInfoGetPublicName(context->zmap_info));

	gameevent_WriteEventEscaped(ev, &evStr);
	// Our 'trigger' only happens when the last challenge is done.
	estrPrintf(eventListenOut, "GlobalEventAddListenAliased(\"%s\", \"%s\"); ", evStr, objectiveName);
	estrPrintf(condOut, "CheckMessage(\"%s\")>0", objectiveName);
	estrPrintf(actionOut, "ClearMessage(\"%s\")", objectiveName);

	estrDestroy(&evStr);
	StructDestroySafe(parse_GameEvent, &ev);
}

void ugcGenesisWhenFSMExprTextAndEvents(UGCGenesisMissionContext *context, UGCGenesisWhen *when, char **condOut, char **actionOut, char **eventListenOut, int start)
{
	switch(when->type)
	{
		xcase UGCGenesisWhen_MapStart: {
			
		}
		xcase UGCGenesisWhen_ObjectiveInProgress : {
			char *evStr = NULL;
			char *objName = start ? eaHead(&when->eaObjectiveNames) : eaTail(&when->eaObjectiveNames);

			ugcGenesisWhenFSMExprTextAndEventsFromObjective(context, objName, condOut, actionOut, eventListenOut);
		}
		xdefault : {
			assert(0);
		}
	}
}

/// Fill out FSM's name, filename, and scope with namespace info.
const void ugcGenesisFSMUpdateNames( UGCGenesisMissionContext* context, char *pcName, FSM* fsm)
{
	if( context->zmap_info ) {
		char path[ MAX_PATH ];
		char nameSpace[RESOURCE_NAME_MAX_SIZE];
		char name[RESOURCE_NAME_MAX_SIZE];
		char scope[RESOURCE_NAME_MAX_SIZE];

		if (!resExtractNameSpace_s(zmapInfoGetFilename( context->zmap_info ), SAFESTR(nameSpace), SAFESTR(path)))
		{
			nameSpace[0] = '\0';
			strcpy(path, zmapInfoGetFilename( context->zmap_info ));
		}
		getDirectoryName( path );
		strcat( path, "/FSM" );

		strcpy(scope, "UGCMap");
		strcpy_s(&scope[6], RESOURCE_NAME_MAX_SIZE-6, &path[4]);
		fsm->scope = allocAddFilename( scope );
		fsm->group = allocAddFilename( "UGCMap" );

		if (nameSpace[0])
		{
			char path2[ MAX_PATH ];
			sprintf(name, "%s:%s", nameSpace, pcName);
			fsm->name = allocAddString(name);
			sprintf(path2, NAMESPACE_PATH"%s/%s/%s.fsm", nameSpace, path, pcName);
			fsm->fileName = allocAddFilename(path2);
		}
		else
		{
			fsm->name = allocAddString(pcName);
			strcatf(path, "/%s.fsm", pcName);
			fsm->fileName = allocAddFilename(path);
		}
	}
	else
		assert(0);
}

bool ugcGenesisExternVarIsDefault(WorldVariableDef *var)
{
	if(var->eType==WVAR_NONE)
		return true;

	switch(var->eType)
	{
		xcase WVAR_INT: {
			if(var->pSpecificValue->iIntVal==0)
				return true;
		}
		xcase WVAR_FLOAT: {
			if(var->pSpecificValue->fFloatVal==0)
				return true;
		}
		xcase WVAR_STRING: {
			if(nullStr(var->pSpecificValue->pcStringVal))
				return true;
		}
		xcase WVAR_ANIMATION: {
			if(nullStr(var->pSpecificValue->pcStringVal))
				return true;
		}
		xcase WVAR_MESSAGE: {
			if(nullStr(var->pSpecificValue->pcStringVal))
				return true;
		}
		xdefault: {
			assert(0);
		}
	}

	return false;
}

void ugcGenesisFSMPruneExternVars(UGCGenesisFSM *fsm)
{
	FOR_EACH_IN_EARRAY(fsm->eaVarDefs, WorldVariableDef, var)
	{
		if(ugcGenesisExternVarIsDefault(var))
		{
			eaRemove(&fsm->eaVarDefs, FOR_EACH_IDX(0, var));
			StructDestroy(parse_WorldVariableDef, var);
		}
	}
	FOR_EACH_END
}
 
void ugcGenesisCreateFSM(UGCGenesisMissionContext *context, ObjectFSMData *data)
{
	FOR_EACH_IN_EARRAY(data->fsms, UGCGenesisFSM, fsm)
	{
		ugcGenesisFSMPruneExternVars(fsm);
	}
	FOR_EACH_END

	if(eaSize(&data->fsms)==1 && 
		data->fsms[0]->activeWhen.type==UGCGenesisWhen_MapStart &&
		!eaSize(&data->fsms[0]->eaVarDefs))
	{
		// Simple case, referencing a single FSM, with no conditions, at all times
		UGCGenesisFSM *gfsm = data->fsms[0];
		UGCGenesisInstancedObjectParams* params = ugcGenesisInternChallengeRequirementParams( context, data->challengeName );

		params->pcFSMName = StructAllocString(gfsm->pcFSMName);
	}
	else
	{
		// Here we need to actually build an FSM
		StashTable statesOnEntry;
		FSM *topFSM = StructCreate(parse_FSM);
		char* filename = NULL;
		FSMState *stateAmbient, *stateCombat;
		int needsAmbientFromTrans = false;
		char *startOnEntryFirst = NULL;
		char *name = NULL;

		ugcGenesisSortFSMs(context, data);

		statesOnEntry = stashTableCreateAddress(10);

		estrPrintf(&name, "UGCFSM_%s", data->challengeName);
		ugcGenesisFSMUpdateNames(context, name, topFSM);
		topFSM->comment = StructAllocString("Autogenerated FSM from UGC");

		stateAmbient = ugcFsmCreateState(topFSM, "Ambient");
		ugcFsmStateSetAction(topFSM, stateAmbient, "Ambient()");

		stateCombat = ugcFsmCreateState(topFSM, "Combat");
		ugcFsmStateSetAction(topFSM, stateCombat, "Combat()");

		ugcFsmStateAddTransition(topFSM, stateAmbient, stateCombat, "DefaultEnterCombat()", "");
		ugcFsmStateAddTransition(topFSM, stateCombat, stateAmbient, "DefaultDropOutOfCombat()", "");

		// Create all states
		FOR_EACH_IN_EARRAY_FORWARDS(data->fsmsAndStates, ObjectFSMTempData, fsmAndState)
		{
			char varPrefixBuffer[ 1024 ];
			char *varStr = NULL;

			if(!fsmAndState || !fsmAndState->fsm)  // Empty objective
				continue;

			estrPrintf(&name, "SubFSM_%s", fsmAndState->fsm->pcName);

			fsmAndState->state = ugcFsmCreateState(topFSM, name);

			sprintf( varPrefixBuffer, "%s_SubFSM_%s", topFSM->name, fsmAndState->fsm->pcName );
			ugcGenesisFSMStateProcessVars(context, varPrefixBuffer, fsmAndState->fsm->eaVarDefs, &varStr);
			if(fsmAndState->fsm->pcFSMName)
				SET_HANDLE_FROM_STRING(gFSMDict, fsmAndState->fsm->pcFSMName, fsmAndState->state->subFSM);
			else
				devassert(0);

			stashAddressAddPointer(statesOnEntry, fsmAndState->state, varStr, true);
		}
		FOR_EACH_END

		ANALYSIS_ASSUME(data->fsmsAndStates && data->fsmsAndStates[0]);
		if(!data->fsmsAndStates[0]->fsm)
		{
			needsAmbientFromTrans = true;
		}
		else
		{
			int ambIdx = eaFind(&topFSM->states, stateAmbient);
			int stIdx = eaFind(&topFSM->states, data->fsmsAndStates[0]->state);
			eaSwap(&topFSM->states, ambIdx, stIdx);
			needsAmbientFromTrans = false;
		}

		FOR_EACH_IN_EARRAY_FORWARDS(data->fsmsAndStates, ObjectFSMTempData, fsmAndState)
		{
			FSMState *state;
			FSMState *stateNext;
			ObjectFSMTempData *fsmAndStateNext;
			ObjectFSMTempData *fsmAndStatePrev;
			UGCGenesisFSM *next;
			UGCGenesisFSM *gfsm;
			char *transCond = NULL;
			char *transAction = NULL; 
			char *listenExprs = NULL;

			if(!fsmAndState || !fsmAndState->fsm)
				continue;

			gfsm = fsmAndState->fsm;
			state = fsmAndState->state;

			fsmAndStatePrev = eaGet(&data->fsmsAndStates, FOR_EACH_IDX(0, fsmAndState)-1);
			devassert(!fsmAndStatePrev || !fsmAndStatePrev->fsm || fsmAndStatePrev->fsm->activeWhen.type!=UGCGenesisWhen_MapStart);

			fsmAndStateNext = eaGet(&data->fsmsAndStates, FOR_EACH_IDX(0, fsmAndState)+1);
			next = fsmAndStateNext ? fsmAndStateNext->fsm : NULL;
			stateNext = fsmAndStateNext ? fsmAndStateNext->state : NULL;
			
			if(gfsm->pcFSMName)
				SET_HANDLE_FROM_STRING(gFSMDict, gfsm->pcFSMName, state->subFSM);
			
			if(needsAmbientFromTrans && fsmAndState->fsm->activeWhen.type!=UGCGenesisWhen_MapStart)
			{
				devassert(fsmAndStatePrev && !fsmAndStatePrev->fsm);
				ugcGenesisWhenFSMExprTextAndEventsFromObjective(context, fsmAndStatePrev->objName, &transCond, &transAction, &listenExprs);
				estrAppend(&startOnEntryFirst, &listenExprs);

				ugcFsmStateAddTransition(topFSM, stateAmbient, state, transCond, transAction);
			}

			ugcGenesisWhenFSMExprTextAndEvents(context, &gfsm->activeWhen, &transCond, &transAction, &listenExprs, false);
			estrAppend(&startOnEntryFirst, &listenExprs);

			if(next)
			{
				needsAmbientFromTrans = false;
				ugcFsmStateAddTransition(topFSM, state, stateNext, transCond, transAction);
			}
			else
			{
				needsAmbientFromTrans = true;
				ugcFsmStateAddTransition(topFSM, state, stateAmbient, transCond, transAction);
			}

			estrDestroy(&transAction);
			estrDestroy(&transCond);
			estrDestroy(&listenExprs);
		}
		FOR_EACH_END

		if(data->fsmsAndStates[0]->fsm && data->fsmsAndStates[0]->fsm->activeWhen.type==UGCGenesisWhen_MapStart)
		{
			int stateIt;
			int transitionIt;
			for( stateIt = eaSize(&topFSM->states) - 1; stateIt >= 0; --stateIt) {
				for( transitionIt = eaSize(&topFSM->states[stateIt]->transitions) - 1; transitionIt >= 0; --transitionIt) {
					FSMTransition* transition = topFSM->states[stateIt]->transitions[transitionIt];
					if(transition->targetName == stateAmbient->name) {
						StructDestroy( parse_FSMTransition, transition );
						eaRemove( &topFSM->states[stateIt]->transitions, transitionIt );
					}
				}
			}
			
			eaSetSize(&topFSM->states, 1);
			StructDestroy(parse_FSMState, stateAmbient);
			StructDestroy(parse_FSMState, stateCombat);
		}

		if(!nullStr(startOnEntryFirst))
			topFSM->states[0]->onFirstEntry = exprCreateFromString(startOnEntryFirst, topFSM->fileName);

		if(stashGetCount(statesOnEntry))
		{
			StashTableIterator iter;
			StashElement elem;
			stashGetIterator(statesOnEntry, &iter);

			while(stashGetNextElement(&iter, &elem))
			{
				FSMState *state = stashElementGetKey(elem);
				char *entry = stashElementGetPointer(elem);

				ugcFsmStateSetOnEntry(topFSM, state, entry, NULL);
			}
		}

		eaPush(context->fsm_accum, topFSM);

		{
			UGCGenesisInstancedObjectParams* params = ugcGenesisInternChallengeRequirementParams( context, data->challengeName );
			params->pcFSMName = StructAllocString(topFSM->name);
		}

		estrDestroy(&name);
	}
}

/// Create a WorldGameActionProperties that will show the prompt PROMPT. 
WorldGameActionProperties* ugcGenesisCreatePromptAction( UGCGenesisMissionContext* context, UGCGenesisMissionPrompt* prompt )
{
	WorldGameActionProperties* accum = StructCreate( parse_WorldGameActionProperties );

	accum->eActionType = WorldGameActionType_Contact;
	accum->pContactProperties = StructCreate( parse_WorldContactActionProperties );
	SET_HANDLE_FROM_STRING( g_ContactDictionary, ugcGenesisContactName( context, prompt ),
							accum->pContactProperties->hContactDef );
	accum->pContactProperties->pcDialogName = StructAllocString( prompt->pcName );

	return accum;
}

static void ugcGenesisEnsureObjectHasInteractProperties(GroupDef *def, bool disallow_volume)
{
	bool defIsVolume = (def->property_structs.volume && !def->property_structs.volume->bSubVolume) && !disallow_volume;
	if( defIsVolume ) {
		if( !def->property_structs.server_volume.interaction_volume_properties ) {
			def->property_structs.server_volume.interaction_volume_properties = StructCreate( parse_WorldInteractionProperties );
		}

		groupDefAddVolumeType(def, "Interaction");
	} else {
		if( !def->property_structs.interaction_properties ) {
			def->property_structs.interaction_properties = StructCreate( parse_WorldInteractionProperties );
		}
	}

	// Assuming interaction is only added for clickies, this may
	// have transformed a NONE challenge into a clickie challenge.
	if( !def->property_structs.genesis_challenge_properties ) {
		def->property_structs.genesis_challenge_properties = StructCreate( parse_WorldGenesisChallengeProperties );
	}
	if( def->property_structs.genesis_challenge_properties->type == GenesisChallenge_None ) {
		def->property_structs.genesis_challenge_properties->type = GenesisChallenge_Clickie;
	}
}

void ugcGenesisApplyObjectVisibilityParams(GroupDef *def, UGCGenesisInteractObjectParams *interact_params, char* challenge_name, UGCRuntimeErrorContext* debugContext)
{
	const char* clickableStr = allocAddString( "CLICKABLE" );
	const char* destructibleStr = allocAddString( "DESTRUCTIBLE" );
	const char* fromDefinitionStr = allocAddString( "FROMDEFINITION" );
	const char* namedObjectStr = allocAddString( "NAMEDOBJECT" );
	const char* contactStr = allocAddString( "CONTACT" );
	const char* doorStr = allocAddString( "DOOR" );
	if (interact_params->clickieVisibleWhenCond) {
		WorldInteractionPropertyEntry* interactEntry = NULL;
		int it;

		ugcGenesisEnsureObjectHasInteractProperties(def, interact_params->bDisallowVolume);

		if( !eaSize( &def->property_structs.interaction_properties->eaEntries )) {
			eaPush( &def->property_structs.interaction_properties->eaEntries, StructCreate( parse_WorldInteractionPropertyEntry ));
			def->property_structs.interaction_properties->eaEntries[ 0 ]->pcInteractionClass = namedObjectStr;
		}

		for( it = 0; it != eaSize( &def->property_structs.interaction_properties->eaEntries ); ++it ) {
			WorldInteractionPropertyEntry* entry = def->property_structs.interaction_properties->eaEntries[ it ];
			if(   entry->pcInteractionClass == clickableStr || entry->pcInteractionClass == destructibleStr
				  || entry->pcInteractionClass == fromDefinitionStr || entry->pcInteractionClass == namedObjectStr
				  || entry->pcInteractionClass == contactStr || entry->pcInteractionClass == doorStr ) {
				interactEntry = entry;
				break;
			}
		}
		
		if( !interactEntry ) {
			ugcRaiseErrorContext( UGC_FATAL_ERROR, debugContext,
								  "Trying to set a clicky visible "
								  "when condition on an interactable that is "
								  "not a clickie nor a destructable." );
			return;
		}

		if( interactEntry->pcInteractionClass == destructibleStr ) {
			ugcRaiseErrorContext( UGC_FATAL_ERROR, debugContext,
								  "Clicky visible when on destructables is not supported." );
		} else {
			if( interactEntry->pVisibleExpr ) {
				StructDestroy( parse_Expression, interactEntry->pVisibleExpr );
			}
			interactEntry->pVisibleExpr = StructClone( parse_Expression, interact_params->clickieVisibleWhenCond );
			def->property_structs.interaction_properties->bEvalVisExprPerEnt = interact_params->clickieVisibleWhenCondPerEnt;
			interactEntry->bOverrideVisibility = true;
			if( interact_params->clickieVisibleWhenCondPerEnt ) {
				def->property_structs.physical_properties.eCameraCollType = WLCCT_NoCamCollision;
				def->property_structs.physical_properties.eGameCollType = WLGCT_FullyPermeable;
				def->property_structs.physical_properties.bPhysicalCollision = false;
				def->property_structs.physical_properties.bSplatsCollision = false;
			}
		}
	}
}

void ugcGenesisApplyInteractObjectDoorParams(GroupDef *def, UGCGenesisInteractObjectParams *interact_params, UGCRuntimeErrorContext *debugContext)
{
	bool defIsVolume = (def->property_structs.volume && !def->property_structs.volume->bSubVolume) && !interact_params->bDisallowVolume;
	if (eaSize(&interact_params->eaInteractionEntries) != 1)
	{
		ugcRaiseErrorContext( UGC_FATAL_ERROR, debugContext,
							  "Trying to set more than one interaction entry on a "
							  "UGC door.");
	}
	else
	{
		WorldInteractionPropertyEntry *existing_entry = NULL;
		if (defIsVolume) {
			if (!def->property_structs.server_volume.interaction_volume_properties
				|| eaSize(&def->property_structs.server_volume.interaction_volume_properties->eaEntries) != 1)
			{
				ugcRaiseErrorContext( UGC_FATAL_ERROR, debugContext,
									  "Trying to set UGC door interact properties on a volume "
									  "that has no existing interaction properties.");
			}
			else
			{
				existing_entry = def->property_structs.server_volume.interaction_volume_properties->eaEntries[0];
			}
		} else {
			if (!def->property_structs.interaction_properties
				|| eaSize(&def->property_structs.interaction_properties->eaEntries) != 1)
			{
				ugcRaiseErrorContext( UGC_FATAL_ERROR, debugContext,
									  "Trying to set UGC door interact properties on an interactable "
									  "that has no existing interaction properties.");
			}
			else
			{
				existing_entry = def->property_structs.interaction_properties->eaEntries[0];
			}
		}
		if (existing_entry)
		{
			WorldInteractionPropertyEntry *new_entry = interact_params->eaInteractionEntries[0];

			REMOVE_HANDLE(new_entry->hInteractionDef);
			new_entry->pcInteractionClass = existing_entry->pcInteractionClass;
			new_entry->bOverrideInteract = existing_entry->bOverrideInteract;
			new_entry->bOverrideVisibility = existing_entry->bOverrideVisibility;
			new_entry->bOverrideCategoryPriority = existing_entry->bOverrideCategoryPriority;
			new_entry->bExclusiveInteraction = existing_entry->bExclusiveInteraction;
			new_entry->bUseExclusionFlag = existing_entry->bUseExclusionFlag;
			new_entry->bDisablePowersInterrupt = existing_entry->bDisablePowersInterrupt;
			if (existing_entry->pTimeProperties)
			{
				new_entry->pTimeProperties = StructClone(parse_WorldTimeInteractionProperties, existing_entry->pTimeProperties);
			}
			if (existing_entry->pMotionProperties)
			{
				new_entry->pMotionProperties = StructClone(parse_WorldMotionInteractionProperties, existing_entry->pMotionProperties);
			}
			if (existing_entry->pActionProperties)
			{
				if (!new_entry->pActionProperties)
					new_entry->pActionProperties = StructCreate(parse_WorldActionInteractionProperties);
				if (existing_entry->pActionProperties->pSuccessExpr)
				{
					new_entry->pActionProperties->pSuccessExpr = StructClone(parse_Expression, existing_entry->pActionProperties->pSuccessExpr);
				}

				if (existing_entry->pActionProperties->pFailureExpr)
				{
					new_entry->pActionProperties->pFailureExpr = StructClone(parse_Expression, existing_entry->pActionProperties->pFailureExpr);
				}

				FOR_EACH_IN_EARRAY(existing_entry->pActionProperties->successActions.eaActions, WorldGameActionProperties, action)
				{
					eaPush(&new_entry->pActionProperties->successActions.eaActions, StructClone(parse_WorldGameActionProperties, action));
				}
				FOR_EACH_END;
			}
			if (existing_entry->pAnimationProperties)
			{
				StructDestroy(parse_WorldAnimationInteractionProperties, new_entry->pAnimationProperties);
				new_entry->pAnimationProperties = StructClone(parse_WorldAnimationInteractionProperties, existing_entry->pAnimationProperties);
			}
			if (existing_entry->pGateProperties)
			{
				StructDestroy(parse_WorldGateInteractionProperties, new_entry->pGateProperties);
				new_entry->pGateProperties = StructClone(parse_WorldGateInteractionProperties, existing_entry->pGateProperties);
			}

			if (existing_entry->pSuccessCond)
			{
				ugcRaiseErrorContext( UGC_FATAL_ERROR, debugContext,
									  "UGC door interaction object cannot have an "
									  "existing success condition.");
			}
		}
	}
}

void ugcGenesisApplyInteractObjectParams(const char *zmap_name, GroupDef *def, UGCGenesisInteractObjectParams *interact_params, char* challenge_name, UGCRuntimeErrorContext* debugContext)
{
	const char* clickableStr = allocAddString( "CLICKABLE" );
	const char* destructibleStr = allocAddString( "DESTRUCTIBLE" );
	const char* fromDefinitionStr = allocAddString( "FROMDEFINITION" );
	const char* namedObjectStr = allocAddString( "NAMEDOBJECT" );
	const char* gateStr = allocAddString( "GATE" );
	bool defIsVolume = (def->property_structs.volume && !def->property_structs.volume->bSubVolume) && !interact_params->bDisallowVolume;

	if (interact_params->bIsUGCDoor)
	{
		ugcGenesisApplyInteractObjectDoorParams(def, interact_params, debugContext);
	}

	// This *MUST* be valid on non-interactables, so that plain ol'
	// object library pieces can be made interactable.
	if( eaSize( &interact_params->eaInteractionEntries ) > 0 ) {
		ugcGenesisEnsureObjectHasInteractProperties(def, interact_params->bDisallowVolume);

		if (defIsVolume) {
			int it;
			// This is needed because other interaction properties will override the door
			eaDestroyStruct( &def->property_structs.server_volume.interaction_volume_properties->eaEntries, parse_WorldInteractionPropertyEntry );

			for( it = 0; it != eaSize( &interact_params->eaInteractionEntries ); ++it ) {
				eaPush( &def->property_structs.server_volume.interaction_volume_properties->eaEntries, StructClone( parse_WorldInteractionPropertyEntry, interact_params->eaInteractionEntries[ it ]));
			}
		}
		else
		{
			WorldMotionInteractionProperties* origMotionProperties = NULL;
			int it;

			// This is needed because other interaction properties will override the door
			StructCopyAll( parse_DisplayMessage, &interact_params->displayNameMsg, &def->property_structs.interaction_properties->displayNameMsg );

			if( eaGet( &def->property_structs.interaction_properties->eaEntries, 0 )) {
				origMotionProperties = StructClone( parse_WorldMotionInteractionProperties, def->property_structs.interaction_properties->eaEntries[ 0 ]->pMotionProperties );
			}
			
			eaDestroyStruct( &def->property_structs.interaction_properties->eaEntries, parse_WorldInteractionPropertyEntry );

			for( it = 0; it != eaSize( &interact_params->eaInteractionEntries ); ++it ) {
				WorldInteractionPropertyEntry* entry = StructClone( parse_WorldInteractionPropertyEntry, interact_params->eaInteractionEntries[ it ]);
				if( origMotionProperties ) {
					entry->pMotionProperties = StructClone( parse_WorldMotionInteractionProperties, origMotionProperties );
				}
				eaPush( &def->property_structs.interaction_properties->eaEntries, entry );
			}

			StructDestroySafe( parse_WorldMotionInteractionProperties, &origMotionProperties );
		}
	}

	if( interact_params->interactWhenCond ) {
		WorldInteractionPropertyEntry* interactEntry = NULL;
		if( defIsVolume ) {
			if( def->property_structs.server_volume.interaction_volume_properties ) {
				int it;
				for( it = 0; it != eaSize( &def->property_structs.server_volume.interaction_volume_properties->eaEntries ); ++it ) {
					WorldInteractionPropertyEntry* entry = def->property_structs.server_volume.interaction_volume_properties->eaEntries[ it ];
					if(   entry->pcInteractionClass == clickableStr || entry->pcInteractionClass == destructibleStr
						  || entry->pcInteractionClass == fromDefinitionStr ) {
						interactEntry = entry;
						break;
					}
				}
			}
		} else if (def->property_structs.interaction_properties) {
			int it;
			for( it = 0; it != eaSize( &def->property_structs.interaction_properties->eaEntries ); ++it ) {
				WorldInteractionPropertyEntry* entry = def->property_structs.interaction_properties->eaEntries[ it ];
				if(   entry->pcInteractionClass == clickableStr || entry->pcInteractionClass == destructibleStr
					  || entry->pcInteractionClass == fromDefinitionStr || entry->pcInteractionClass == gateStr ) {
					interactEntry = entry;
					break;
				}
			}
		}
		if( !interactEntry ) {
			ugcRaiseErrorContext( UGC_FATAL_ERROR, debugContext,
								  "Trying to set an interact when "
								  "condition on an interactable that is "
								  "not a clickie nor a destructable." );
			return;
		}

		if( interactEntry->pcInteractionClass == destructibleStr ) {
			ugcRaiseErrorContext( UGC_FATAL_ERROR, debugContext,
								  "Interact when on destructables is not supported." );
			// NOT CURRENTLY SUPPORTED
			// // Destructables don't have a "targetable when" field, so
			// // use pVisibleExpr to achieve nearly the same
			// // effect.
			// if( interactEntry->pVisibleExpr ) {
			// 	StructDestroy( parse_Expression, interactEntry->pVisibleExpr );
			// }
			// interactEntry->pVisibleExpr = StructClone( parse_Expression, interact_params->interactWhenCond );
		} else {
			if( interactEntry->pInteractCond ) {
				StructDestroy( parse_Expression, interactEntry->pInteractCond );
			}
			interactEntry->pInteractCond = StructClone( parse_Expression, interact_params->interactWhenCond );
			interactEntry->bOverrideInteract = true;
		}
	}

	if( interact_params->succeedWhenCond ) {
		WorldInteractionPropertyEntry* interactEntry = NULL;
		if( defIsVolume ) {
			if( def->property_structs.server_volume.interaction_volume_properties ) {
				int it;
				for( it = 0; it != eaSize( &def->property_structs.server_volume.interaction_volume_properties->eaEntries ); ++it ) {
					WorldInteractionPropertyEntry* entry = def->property_structs.server_volume.interaction_volume_properties->eaEntries[ it ];
					if(   entry->pcInteractionClass == clickableStr || entry->pcInteractionClass == destructibleStr
						  || entry->pcInteractionClass == fromDefinitionStr ) {
						interactEntry = entry;
						break;
					}
				}
			}
		} else if (def->property_structs.interaction_properties) {
			int it;
			for( it = 0; it != eaSize( &def->property_structs.interaction_properties->eaEntries ); ++it ) {
				WorldInteractionPropertyEntry* entry = def->property_structs.interaction_properties->eaEntries[ it ];
				if(   entry->pcInteractionClass == clickableStr || entry->pcInteractionClass == destructibleStr
					  || entry->pcInteractionClass == fromDefinitionStr || entry->pcInteractionClass == gateStr ) {
					interactEntry = entry;
					break;
				}
			}
		}

		if( !interactEntry ) {
			ugcRaiseErrorContext( UGC_FATAL_ERROR, debugContext,
								  "Trying to set a succeed "
								  "when condition on an interactable that is "
								  "not a clickie nor a destructable." );
			return;
		}

		if( interactEntry->pcInteractionClass == destructibleStr ) {
			ugcRaiseErrorContext( UGC_FATAL_ERROR, debugContext,
								  "Succeed when on destructables is not supported." );
			// NOT CURRENTLY SUPPORTED
			// // Destructables don't have a "targetable when" field, so
			// // use pVisibleExpr to achieve nearly the same
			// // effect.
			// if( interactEntry->pVisibleExpr ) {
			// 	StructDestroy( parse_Expression, interactEntry->pVisibleExpr );
			// }
			// interactEntry->pVisibleExpr = StructClone( parse_Expression, interact_params->succeedWhenCond );
		} else {
			if( interactEntry->pSuccessCond ) {
				StructDestroy( parse_Expression, interactEntry->pSuccessCond );
			}
			interactEntry->pSuccessCond = StructClone( parse_Expression, interact_params->succeedWhenCond );
			interactEntry->bOverrideInteract = true;
		}
	}

}

/// Callback function to apply our mission-specific parameters to an instanced Challenge groupdef
void ugcGenesisApplyInstancedObjectParams(const char *zmap_name,
									   GroupDef *def, UGCGenesisInstancedObjectParams *params, UGCGenesisInteractObjectParams *interact_params, char* challenge_name, UGCRuntimeErrorContext* debugContext)
{
	if (interact_params)
		ugcGenesisApplyInteractObjectParams(zmap_name, def, interact_params, challenge_name, debugContext);

	if( params->encounterSpawnCond ) {
		if( def->property_structs.encounter_properties ) {
			/// CHALLENGE2 SUPPORT
			WorldEncounterSpawnProperties* spawnProps;
			if( !def->property_structs.encounter_properties->pSpawnProperties ) {
				ZoneMapInfo* zmapInfo = zmapGetInfo(layerGetZoneMap(def->def_lib->zmap_layer));
				WorldEncounterSpawnProperties* props = StructCreate( parse_WorldEncounterSpawnProperties );
				props->eSpawnRadiusType = encounter_GetSpawnRadiusTypeFromProperties(zmapInfo, NULL);
				props->eRespawnTimerType = encounter_GetRespawnTimerTypeFromProperties(zmapInfo, NULL);
				def->property_structs.encounter_properties->pSpawnProperties = props;
			}
			spawnProps = def->property_structs.encounter_properties->pSpawnProperties;

			spawnProps->pSpawnCond = StructClone( parse_Expression, params->encounterSpawnCond );
			spawnProps->pDespawnCond = StructClone( parse_Expression, params->encounterDespawnCond );
		} else {
			ugcRaiseErrorContext( UGC_ERROR, debugContext,
								  "Trying to set a spawn when "
								  "condition on something that is not an encounter." );
		}
	}

	if( params->has_patrol ) {
		if( def->property_structs.encounter_properties ) {
			/// CHALLENGE2 SUPPORT
			char patrolName[ 256 ];
			sprintf( patrolName, "%s_Patrol", challenge_name );
			def->property_structs.encounter_properties->pcPatrolRoute = StructAllocString( patrolName );
		} else {
			ugcRaiseErrorContext( UGC_FATAL_ERROR, debugContext,
								  "Trying to set a patrol on a non-encounter." );
		}
	}

	if (params->pContact)
	{
		WorldInteractionProperties* interactProps = NULL;
		if( def->property_structs.encounter_properties ) {
			WorldActorProperties *actor = eaGet(&def->property_structs.encounter_properties->eaActors, 0);
			if(!actor)
				eaSet(&def->property_structs.encounter_properties->eaActors, actor = StructCreate(parse_WorldActorProperties), 0);

			actor->pcName = allocAddString("Actor_1");

			if(params->pContact->contactName.pEditorCopy && !nullStr(params->pContact->contactName.pEditorCopy->pcDefaultString))
			{
				if(actor->displayNameMsg.pEditorCopy && !nullStr(actor->displayNameMsg.pEditorCopy->pcDefaultString))
					ugcRaiseErrorContext( UGC_ERROR, debugContext, "Setting a message on a contact when there was already one");
				else
					StructCopyAll(parse_DisplayMessage, &params->pContact->contactName, &actor->displayNameMsg);
			}

			if(!actor->pInteractionProperties)
				actor->pInteractionProperties = StructCreate(parse_WorldInteractionProperties);

			interactProps = actor->pInteractionProperties;
		} else {
			// Plain ol' object, make interactable
			if( !def->property_structs.interaction_properties ) {
				def->property_structs.interaction_properties = StructCreate( parse_WorldInteractionProperties );
			}

			interactProps = def->property_structs.interaction_properties;
		}

		if( interactProps ) {
			eaDestroyStruct(&interactProps->eaEntries, parse_WorldInteractionPropertyEntry);
			FOR_EACH_IN_EARRAY(params->pContact->eaPrompts, UGCGenesisMissionPromptExprPair, prompt)
			{
				WorldInteractionPropertyEntry *entry = StructCreate(parse_WorldInteractionPropertyEntry);
				entry->pcInteractionClass = allocAddString("Contact");

				if( !nullStr( prompt->exprText )) {
					entry->bOverrideInteract = true;
					entry->pInteractCond = exprCreateFromString( prompt->exprText, NULL );
				}
			
				entry->pContactProperties = StructCreate(parse_WorldContactInteractionProperties);
				entry->pContactProperties->pcDialogName = StructAllocString(prompt->name);
				SET_HANDLE_FROM_STRING("ContactDef", params->pContact->pcContactFileName, entry->pContactProperties->hContactDef);
			
				eaPush(&interactProps->eaEntries, entry);
			}
			FOR_EACH_END;
		} else {
			ugcRaiseErrorContext( UGC_ERROR, debugContext,
								  "Trying to set a Prompt interact on something that is not handled." );
		}
	}
}

void ugcGenesisApplyInstancedAndVisibilityObjectParams(const char *zmap_name,
	GroupDef *def, UGCGenesisInstancedObjectParams *params, UGCGenesisInteractObjectParams *interact_params, char* challenge_name, UGCRuntimeErrorContext* debugContext)
{
	ugcGenesisApplyInstancedObjectParams(zmap_name, def, params, interact_params, challenge_name, debugContext);
	if( interact_params ) {
		ugcGenesisApplyObjectVisibilityParams(def, interact_params, challenge_name, debugContext);
	}
}

/// For TomY ENCOUNTER_HACK only.
UGCGenesisMissionPrompt* ugcGenesisFindPromptPEPHack( UGCGenesisMissionDescription* missionDesc, char* prompt_name )
{
	int it;
	if( !missionDesc ) {
		return NULL;
	}
	
	for( it = 0; it != eaSize( &missionDesc->zoneDesc.eaPrompts ); ++it ) {
		UGCGenesisMissionPrompt* prompt = missionDesc->zoneDesc.eaPrompts[ it ];
		if( stricmp( prompt->pcName, prompt_name ) == 0 ) {
			return prompt;
		}
	}

	return NULL;
}

/// Simple wrapper around ugcGenesisCreateEncounterSpawnCondText()
Expression* ugcGenesisCreateEncounterSpawnCond( UGCGenesisMissionContext* context, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties)
{
	char* estr = NULL;
	ugcGenesisCreateEncounterSpawnCondText( context, &estr, zmapName, properties );

	if( estr ) {
		Expression* expr = exprCreateFromString( estr, NULL );
		estrDestroy( &estr );
		return expr;
	} else {
		return NULL;
	}
}

/// An expression that is always the opposite of what
/// ugcGenesisCreateEncounterSpawnCond() would evaluate to.
///
/// This exists because encounters do not check their spawn condition
/// when spawned (unlike clickies).
Expression* ugcGenesisCreateEncounterDespawnCond( UGCGenesisMissionContext* context, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties)
{
	char* estr = NULL;
	ugcGenesisCreateEncounterDespawnCondText( context, &estr, zmapName, properties );

	if( estr ) {
		Expression* expr;
		
		expr = exprCreateFromString( estr, NULL );
		estrDestroy( &estr );
		return expr;
	} else {
		return NULL;
	}
}

/// Hack callback to generate an encounter spawn condition from a
/// UGCGenesisProceduralEncounterProperties
///
/// TomY ENCOUNTER_HACK
void ugcGenesisCreateEncounterSpawnCondText( UGCGenesisMissionContext* context, char** estr, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties)
{
	ugcGenesisCreateChallengeSpawnCondText( context, estr, zmapName, properties, true );
	
	if( properties->genesis_mission_num >= 0 ) {
		if( *estr ) {
			char* baseExprText;

			strdup_alloca( baseExprText, *estr );
			estrPrintf( estr, "GetMapVariableInt(\"Mission_Num\") = %d and HasNearbyPlayersForEnc() and (%s)",
						properties->genesis_mission_num, baseExprText );
		} else {
			estrPrintf( estr, "GetMapVariableInt(\"Mission_Num\") = %d and HasNearbyPlayersForEnc()",
						properties->genesis_mission_num );
		}
	}
}

/// Hack callback to generate an encounter spawn condition from a
/// UGCGenesisProceduralEncounterProperties
///
/// TomY ENCOUNTER_HACK
void ugcGenesisCreateEncounterDespawnCondText( UGCGenesisMissionContext* context, char** estr, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties)
{
	ugcGenesisCreateChallengeSpawnCondText( context, estr, zmapName, properties, true );

	if( *estr ) {
		estrInsertf( estr, 0, "NOT (" );
		estrConcatf( estr, ")" );
	}
}

/// Simple wrapper around ugcGenesisCreateChallengeSpawnCondText()
Expression* ugcGenesisCreateChallengeSpawnCond( UGCGenesisMissionContext* context, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties )
{
	char* estr = NULL;
	ugcGenesisCreateChallengeSpawnCondText( context, &estr, zmapName, properties, false );

	if( estr ) {
		Expression* expr = exprCreateFromString( estr, NULL );
		estrDestroy( &estr );
		return expr;
	} else {
		return NULL;
	}
}

/// Function used to generate a challenge spawn condition from a
/// UGCGenesisProceduralEncounterProperties.
void ugcGenesisCreateChallengeSpawnCondText( UGCGenesisMissionContext* context, char** estr, const char* zmapName, UGCGenesisProceduralEncounterProperties *properties, bool isEncounter )
{
	const char* missionName = ugcGenesisMissionNameRaw( zmapName, properties->genesis_mission_name, properties->genesis_open_mission );
	estrClear(estr);

	if( properties->genesis_mission_num < 0 && properties->spawn_when.type != UGCGenesisWhen_MapStart ) {
		ugcRaiseErrorContext( UGC_ERROR, ugcMakeTempErrorContextChallenge( properties->encounter_name, SAFE_MEMBER2( context, zone_mission, desc.pcName ), NULL ),
							  "Shared challenges can not have spawn when conditions." );
	} else {
		UGCRuntimeErrorContext* debugContext = ugcMakeTempErrorContextChallenge( properties->encounter_name, SAFE_MEMBER2( context, zone_mission, desc.pcName ), NULL );
		char* exprText;
		
		exprText = ugcGenesisWhenExprTextRaw( context, zmapName,
										   (properties->genesis_open_mission ? UGCGenesisMissionGenerationType_OpenMission : UGCGenesisMissionGenerationType_PlayerMission),
										   properties->genesis_mission_name, properties->when_challenges,
										   &properties->spawn_when, debugContext, "SpawnWhen", isEncounter );

		estrDestroy( estr );
		*estr = exprText;
	}
}

/// Write to ESTR the GameEvent specified by EVENT.
///
/// Automatically frees EVENT for you, so you can put this on one line
/// like this:
///
/// ugcGenesisWriteText( &str, ugcGenesisCompleteChallengeEvent( challenge, ... ))
void ugcGenesisWriteText( char** estr, GameEvent* event, bool escaped )
{
	char* eventEStr = NULL;
	ParserWriteText( &eventEStr, parse_GameEvent, event, 0, 0, 0 );
	
	if( escaped ) {
		estrAppendEscaped( estr, eventEStr );
	} else {
		estrAppend( estr, &eventEStr );
	}
	estrDestroy( &eventEStr );
	StructDestroy( parse_GameEvent, event );
}

/// Create a GameEvent that will trigger when OBJECTIVE-DESC is
/// completed.
GameEvent* ugcGenesisCompleteChallengeEvent( GenesisChallengeType challengeType, const char* challengeName, bool useGroup, const char* zmapName )
{
	GameEvent* event = StructCreate( parse_GameEvent );
	char groupName[ 256 ];

	sprintf( groupName, "LogGrp_%s", challengeName );
	
	event->pchMapName = allocAddString( zmapName );
		
	switch( challengeType ) {		
		case GenesisChallenge_Encounter: case GenesisChallenge_Encounter2:
			event->type = EventType_EncounterState;
			event->encState = EncounterState_Success;
			if( useGroup ) {
				event->pchTargetEncGroupName = allocAddString( groupName );
			} else {
				event->pchTargetStaticEncName = allocAddString( challengeName );
			}

		xcase GenesisChallenge_Clickie:
			event->type = EventType_InteractSuccess;
			event->tSourceIsPlayer = TriState_Yes;
			if( useGroup ) {
				event->pchClickableGroupName = StructAllocString( groupName );
			} else {
				event->pchClickableName = StructAllocString( challengeName );
			}

		xcase GenesisChallenge_Destructible:
			event->type = EventType_Kills;
			event->tSourceIsPlayer = TriState_Yes;
			event->pchTargetEncGroupName = StructAllocString( groupName );

		xdefault:
			FatalErrorf( "not yet implemented" );
	}

	return event;
}

/// Create a GameEvent that will trigger when objective is completed.
GameEvent* ugcGenesisCompleteObjectiveEvent( UGCGenesisMissionObjective *obj, const char* zmapName )
{
	GameEvent* ev = StructCreate( parse_GameEvent );
	
	ev->pchMapName = allocAddString( zmapName );
	ev->type = EventType_MissionState;
	ev->missionState = MissionState_Succeeded;
	ev->pchMapName = allocAddString(zmapName);
	ev->pchMissionRefString = allocAddString(obj->pcName);

	return ev;
}

/// Create a GameEvent that will trigger when ROOM-NAME for
/// MISSION-NAME is reached.
GameEvent* ugcGenesisReachLocationEvent( const char* layoutName, const char* roomOrChallengeName, const char* missionName, const char* zmapName )
{
	if (!layoutName)
		return ugcGenesisReachLocationEventRaw( zmapName, ugcGenesisMissionChallengeVolumeName( roomOrChallengeName, missionName ));
	else
		return ugcGenesisReachLocationEventRaw( zmapName, ugcGenesisMissionRoomVolumeName( layoutName, roomOrChallengeName, missionName ));
}

/// Create a GameEvent that will trigger when VOLUME-NAME is entered.
///
/// Low level wrapper around GameEvent structure.
GameEvent* ugcGenesisReachLocationEventRaw( const char* zmapName, const char* volumeName )
{
	GameEvent* event = StructCreate( parse_GameEvent );
	event->pchMapName = allocAddString( zmapName );
	event->type = EventType_VolumeEntered;
	event->tSourceIsPlayer = TriState_Yes;
	event->pchVolumeName = StructAllocString( volumeName );

	return event;
}

/// Create a GameEvent that will trigger when killing any
/// CRITTER-DEF-NAME.
GameEvent* ugcGenesisKillCritterEvent( const char* critterDefName, const char* zmapName )
{
	GameEvent* event = StructCreate( parse_GameEvent );	
	event->pchMapName = allocAddString( zmapName );
	event->type = EventType_Kills;
	event->pchTargetCritterName = StructAllocString( critterDefName );

	return event;
}

/// Create a GameEvent that will trigger when killing any critter in
/// the group CRITTER-GROUP-NAME.
GameEvent* ugcGenesisKillCritterGroupEvent( const char* critterGroupName, const char* zmapName )
{
	GameEvent* event = StructCreate( parse_GameEvent );	
	event->pchMapName = allocAddString( zmapName );
	event->type = EventType_Kills;
	event->pchTargetCritterGroupName = StructAllocString( critterGroupName );

	return event;
}

/// Create a GameEvent that will trigger when talking to a specific
/// contact.  The contact can be on any ZoneMap.
GameEvent* ugcGenesisTalkToContactEvent( char* contactName )
{
	GameEvent* event = StructCreate( parse_GameEvent );	
	event->type = EventType_InteractSuccess;
	event->tSourceIsPlayer = TriState_Yes;
	event->pchContactName = allocAddString( contactName );

	return event;
}

/// Create a GameEvent that will trigger when finishing a special
/// dialog using COSTUME-NAME and DIALOG-NAME.
GameEvent* ugcGenesisPromptEvent( char* dialogName, char* blockName, bool isComplete, const char* missionName, const char* zmapName, const char* challengeName )
{
	GameEvent* event = StructCreate( parse_GameEvent );
	if( isComplete ) {
		event->type = EventType_ContactDialogComplete;
	} else {
		event->type = EventType_ContactDialogStart;
	}
	event->pchContactName = allocAddString( ugcGenesisContactNameRaw( zmapName, missionName, challengeName ));
	event->pchDialogName = StructAllocString( ugcGenesisSpecialDialogBlockNameTemp( dialogName, blockName ));

	return event;
}

/// Create a GameEvent that will trigger when finishing a special
/// dialog using CONTACT-NAME and DIALOG-NAME.
GameEvent* ugcGenesisExternalPromptEvent( char* dialogName, char* contactName, bool isComplete )
{
	GameEvent* event = StructCreate( parse_GameEvent );
	if( isComplete ) {
		event->type = EventType_ContactDialogComplete;
	} else {
		event->type = EventType_ContactDialogStart;
	}
	event->pchContactName = allocAddString( contactName );
	event->pchDialogName = StructAllocString( dialogName );

	return event;
}

/// Version of ugcGenesisMissionName that takes each parameter
/// seperately.
const char* ugcGenesisMissionNameRaw( const char* zmapName, const char* genesisMissionName, bool isOpenMission )
{
	char missionName[256];
	char* fix = NULL;

	sprintf( missionName, "%s_Mission_%s%s",
			 zmapName, genesisMissionName,
			 (isOpenMission ? "_OpenMission" : "") );
	if( resFixName( missionName, &fix )) {
		strcpy( missionName, fix );
		estrDestroy( &fix );
	}
	return allocAddString( missionName );
}

/// Version of ugcGenesisContactName that takes each parameter
/// seperately.
const char* ugcGenesisContactNameRaw( const char* zmapName, const char* missionName, const char* challengeName )
{
	char contactName[256];
	char* fix = NULL;

	if( zmapName ) {
		if (challengeName != NULL)
			sprintf( contactName, "%s_Contact_Shared_%s",
					 zmapName, challengeName );
		else
			sprintf( contactName, "%s_Contact_%s",
					 zmapName, missionName );
	} else {
		strcpy( contactName, missionName );
	}
	
	if( resFixName( contactName, &fix )) {
		strcpy( contactName, fix );
		estrDestroy( &fix );
	}
	return allocAddString( contactName );
}

/// Convert from GENESIS-COSTUME to CONTACT-COSTUME.
void ugcGenesisMissionCostumeToContactCostume( UGCGenesisMissionCostume* genesisCostume, ContactCostume* contactCostume )
{
	switch( genesisCostume->eCostumeType ) {
		case UGCGenesisMissionCostumeType_Specified:
			contactCostume->eCostumeType = ContactCostumeType_Specified;
		xcase UGCGenesisMissionCostumeType_PetCostume:
			contactCostume->eCostumeType = ContactCostumeType_PetContactList;
		xcase UGCGenesisMissionCostumeType_CritterGroup:
			contactCostume->eCostumeType = ContactCostumeType_CritterGroup;
		xcase UGCGenesisMissionCostumeType_Player:
			contactCostume->eCostumeType = ContactCostumeType_Player;
		xdefault:
			contactCostume->eCostumeType = ContactCostumeType_Default;
	}
	
	COPY_HANDLE( contactCostume->costumeOverride, genesisCostume->hCostume );
	COPY_HANDLE( contactCostume->hPetOverride, genesisCostume->hPetCostume );
	contactCostume->eCostumeCritterGroupType = genesisCostume->eCostumeCritterGroupType;
	COPY_HANDLE( contactCostume->hCostumeCritterGroup, genesisCostume->hCostumeCritterGroup );
	contactCostume->pchCostumeMapVar = allocAddString( genesisCostume->pchCostumeMapVar );
	contactCostume->pchCostumeIdentifier = allocAddString( genesisCostume->pchCostumeIdentifier );
}


/// Convert from CONTACT-COSTUME to GENESIS-COSTUME.
void ugcGenesisMissionCostumeFromContactCostume( UGCGenesisMissionCostume* genesisCostume, ContactCostume* contactCostume )
{
	switch( contactCostume->eCostumeType ) {
		case ContactCostumeType_Specified:
			genesisCostume->eCostumeType = UGCGenesisMissionCostumeType_Specified;
		case ContactCostumeType_PetContactList:
			genesisCostume->eCostumeType = UGCGenesisMissionCostumeType_PetCostume;
		case ContactCostumeType_CritterGroup:
			genesisCostume->eCostumeType = UGCGenesisMissionCostumeType_CritterGroup;
		case ContactCostumeType_Player:
			genesisCostume->eCostumeType = UGCGenesisMissionCostumeType_Player;
		default:
			genesisCostume->eCostumeType = UGCGenesisMissionCostumeType_Specified;
	}
	
	COPY_HANDLE( genesisCostume->hCostume, contactCostume->costumeOverride );
	COPY_HANDLE( genesisCostume->hPetCostume, contactCostume->hPetOverride );
	genesisCostume->eCostumeCritterGroupType = contactCostume->eCostumeCritterGroupType;
	COPY_HANDLE( genesisCostume->hCostumeCritterGroup, contactCostume->hCostumeCritterGroup );
	genesisCostume->pchCostumeMapVar = StructAllocString( contactCostume->pchCostumeMapVar );
	genesisCostume->pchCostumeIdentifier = StructAllocString( contactCostume->pchCostumeIdentifier );
}

/// Find the challenge for the challenge named CHALLENGE-NAME.
UGCGenesisMissionChallenge* ugcGenesisFindChallenge(UGCGenesisMapDescription* map_desc, UGCGenesisMissionDescription* mission_desc, const char* challenge_name, bool* outIsShared)
{
	int it;

	if( nullStr( challenge_name )) {
		SAFE_ASSIGN( outIsShared, false );
		return NULL;
	}

	if( mission_desc ) {
		for( it = 0; it != eaSize( &mission_desc->eaChallenges ); ++it ) {
			UGCGenesisMissionChallenge* challenge = mission_desc->eaChallenges[ it ];
			if( stricmp( challenge->pcName, challenge_name ) == 0 ) {
				SAFE_ASSIGN( outIsShared, false );
				return challenge;
			}
		}
	}
	for( it = 0; it != eaSize( &map_desc->shared_challenges ); ++it ) {
		UGCGenesisMissionChallenge* challenge = map_desc->shared_challenges[ it ];
		if( stricmp( challenge->pcName, challenge_name ) == 0 ) {
			SAFE_ASSIGN( outIsShared, true );
			return challenge;
		}
	}

	SAFE_ASSIGN( outIsShared, false );
	return NULL;
}


#include "gslNNOUGCGenesisMissions_h_ast.c"
#include "gslNNOUGCGenesisMissions_c_ast.c"

// MJF Aug/28/2012 - This would make sense to move to its own file, eventually.
#include "gslNNOUGCGenesisStructs_h_ast.c"
