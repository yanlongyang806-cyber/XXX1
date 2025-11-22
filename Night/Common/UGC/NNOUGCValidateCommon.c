#include "AnimList_Common.h"
#include "CombatEnums.h"
#include "error.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "TextFilter.h"
#include "tokenstore.h"

#include "NNOUGCCommon.h"
#include "NNOUGCInteriorCommon.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCResource.h"
#include "UGCCommon.h"
#include "UGCError.h"
#include "UGCInteriorCommon.h"
#include "WorldGrid.h"
#include "wlEncounter.h"
#include "wlUGC.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

static void ugcValidateComponentUIDs( UGCProjectData* ugcProj );
static void ugcValidateDisplayString( UGCRuntimeErrorContext* context, const char* fieldName, const char* displayStr, bool isRequired, bool allowProfane );
static void ugcValidateMapLocation( UGCRuntimeErrorContext* context, const char* fieldName, const UGCMapLocation* mapLocation );
static void ugcValidateMissionTransitions( UGCProjectData* ugcProj );
static void ugcValidateMissionObjectives( UGCProjectData* ugcProj, UGCMissionObjective** ugcObjectives, bool inCompleteAll );
static void ugcValidateMap( const UGCProjectData* data, UGCBacklinkTable* pBacklinkTable, const UGCMap* map );
static void ugcValidateInteractProperties(const UGCProjectData *ugcProj, UGCRuntimeErrorContext* ctx, UGCInteractProperties *interactProps);
static void ugcValidateUGCWhen( const UGCProjectData* data, UGCWhen *when, const UGCMissionObjective *objective, int componentObjectiveCount, const char* internal_map_name, bool is_show, bool allow_objective_when, UGCRuntimeErrorContext* ctx, int when_idx);
static void ugcValidateComponent( const UGCProjectData* data, UGCBacklinkTable* pBacklinkTable, const UGCComponent* component, UGCMapTransitionInfo** transitions );
static void ugcValidateTagType(const UGCProjectData* data, UGCRuntimeErrorContext *context, const char *field_name, const char *object_name, const char *tag_type_name, bool optional);

static void ugcValidateSeriesNode( const UGCProjectSeries* ugcSeries, const UGCProjectSeriesNode* seriesNode );
static void ugcValidateResource( UGCRuntimeErrorContext* context, const char* fieldName,
								 const char* dict, const char* resName );
	
static bool gUGCDebugFailAllValidation = false;
AUTO_CMD_INT(gUGCDebugFailAllValidation, UGCDebugFailAllValidation) ACMD_CMDLINE;

void ugcValidateProject( UGCProjectData* ugcProj )
{
	ugcLoadStart_printf( "Project validate..." );
	ugcValidateBudgets( ugcProj );
	
	// Validate the global project
	{
		UGCRuntimeErrorContext* ctx = ugcMakeErrorContextDefault();
		char *estrError = NULL;

		estrCreate(&estrError);
		if(!UGCProject_ValidatePotentialName(ugcProj->project->pcPublicName, false, &estrError))
			ugcRaiseErrorInField( UGC_ERROR, ctx, "PublicName", estrError, "Project has invalid name" );
		estrDestroy(&estrError);
		ugcValidateDisplayString( ctx, "Description", ugcProj->project->strDescription, true, false );
		if( !ugcGetIsRepublishing() ) {
			ugcValidateDisplayString( ctx, "Notes", ugcProj->project->strNotes, false, true );
		}

		// Do not validate ugcProj->project->pMapLocation -- it is auto deduced.

		StructDestroySafe( parse_UGCRuntimeErrorContext, &ctx );
	}
	ugcLoadEnd_printf( "done." );

	// Validate the mission
	ugcLoadStart_printf( "Mission validation..." );
	{
		int maxDepth;
		int numObjectives;
		UGCRuntimeErrorContext* ctx = ugcMakeErrorContextMission( ugcProj->mission->name );
		ugcMissionObjectivesCount( ugcProj, &maxDepth, &numObjectives );
		if( numObjectives == 0 ) {
			ugcRaiseErrorInField( UGC_FATAL_ERROR, ctx, "__WHOLE_DOC__", "UGC.Mission_NoObjectives", "Mission has no objectives" );
		}

		if (ugcDefaultsDialogStyle() == UGC_DIALOG_STYLE_WINDOW)
		{
			ugcValidateDisplayString( ctx, "PromptBody", ugcProj->mission->sGrantPrompt.pcPromptBody, true, false );
			if(   ugcDefaultsDialogStyle() == UGC_DIALOG_STYLE_WINDOW
				&& nullStr( ugcProj->mission->sGrantPrompt.pcPromptCostume ) && !IS_HANDLE_ACTIVE( ugcProj->mission->sGrantPrompt.hPromptPetCostume )) {
					ugcRaiseErrorInField( UGC_ERROR, ctx, "PromptCostume", "UGC.Block_NoCostume", "No costume specified for this block." );
			}
		}

		StructDestroySafe( parse_UGCRuntimeErrorContext, &ctx );
	}
	ugcValidateMissionObjectives( ugcProj, ugcProj->mission->objectives, false );
	ugcValidateMissionTransitions( ugcProj );
	ugcLoadEnd_printf( "done." );

	{
		UGCBacklinkTable* backlinkTable = NULL;
		ugcBacklinkTableRefresh( ugcProj, &backlinkTable );

		// Validate maps
		ugcLoadStart_printf( "Map validation..." );
		{
			int it;
			for( it = 0; it != eaSize(&ugcProj->maps); ++it ) {
				ugcValidateMap(ugcProj, backlinkTable, ugcProj->maps[it]);
			}
		}
		ugcLoadEnd_printf( "done." );

		ugcLoadStart_printf( "Component validation..." );
		ugcValidateComponentUIDs(ugcProj);

		// Validate each component
		{
			UGCComponent* unplacedComponent = NULL;

			{
				UGCMapTransitionInfo** transitions = ugcMissionGetMapTransitions( ugcProj, ugcProj->mission->objectives );				
				int it;
				for( it = 0; it != eaSize( &ugcProj->components->eaComponents ); ++it ) {
					UGCComponent* component = ugcProj->components->eaComponents[ it ];
					ugcValidateComponent( ugcProj, backlinkTable, component, transitions );

					if( component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE && !component->sPlacement.bIsExternalPlacement ) {
						if( component->sPlacement.uRoomID == GENESIS_UNPLACED_ID ) {
							unplacedComponent = component;
						}
					}
				}

				eaDestroyStruct( &transitions, parse_UGCMapTransitionInfo );
			}

			if( unplacedComponent ) {
				if( eaSize( &ugcProj->maps ) > 0 ) {
					ugcRaiseError( UGC_FATAL_ERROR, ugcMakeTempErrorContextChallenge( ugcComponentGetLogicalNameTemp( unplacedComponent ), NULL, NULL ), 
								   "UGC.Component_NotPlaced",
								   "Must place all components before proceeding." );
				} else {
					ugcRaiseError( UGC_FATAL_ERROR, ugcMakeTempErrorContextChallenge( ugcComponentGetLogicalNameTemp( unplacedComponent ), NULL, NULL ),
								   "UGC.Component_NoMaps",
								   "Must create maps for the components to be on." );
				}
			}
		}
		ugcLoadEnd_printf( "done." );

		ugcBacklinkTableDestroy( &backlinkTable );
	}

	// Validate items
	ugcLoadStart_printf( "Item validation..." );
	{
		FOR_EACH_IN_EARRAY(ugcProj->items, UGCItem, item)
		{
			UGCRuntimeErrorContext* ctx = ugcMakeTempErrorContextUGCItem( item->astrName );
			ugcValidateDisplayString( ctx, "DisplayName", item->strDisplayName, true, false);
			ugcValidateDisplayString( ctx, "Description", item->strDescription, true, false);
			ugcValidateTagType( ugcProj, ctx, "Icon", item->strIcon, "MissionItemIcon", false);
		}
		FOR_EACH_END;
	}
	ugcLoadEnd_printf( "done." );
}

UGCBudgetValidateState ugcValidateBudgets( UGCProjectData* ugcProj )
{
	bool budgetSoftLimitHit = false;
	bool budgetHardLimitHit = false;

	// Validate the mission
	{
		UGCRuntimeErrorContext* ctx = ugcMakeTempErrorContextMission( ugcProj->mission->name );
		int maxDepth;
		int numObjectives;
		UGCProjectBudget* budget = ugcFindBudget( UGC_BUDGET_TYPE_OBJECTIVE, 0 );
		ugcMissionObjectivesCount( ugcProj, &maxDepth, &numObjectives );
		if( maxDepth > UGC_OBJECTIVE_MAX_DEPTH ) {
			ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC.Mission_TooDeep", "Mission objectives are too deep." );
			budgetSoftLimitHit = true;
		}
		if( numObjectives > SAFE_MEMBER( budget, iSoftLimit )) {
			ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC.Mission_TooManyObjectives", "Too many mission objectives." );
			budgetSoftLimitHit = true;
		}
		if( numObjectives > SAFE_MEMBER( budget, iHardLimit )) {
			budgetHardLimitHit = true;
		}
	}

	// Validate maps
	{
		UGCProjectBudget *budget = ugcFindBudget(UGC_BUDGET_TYPE_MAP, 0);
		UGCRuntimeErrorContext* ctx = ugcMakeTempErrorContextDefault();

		if (eaSize(&ugcProj->maps) > SAFE_MEMBER( budget, iSoftLimit )) {
			ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC.Project_TooManyMaps", "Project has too many maps" );
			budgetSoftLimitHit = true;
		}
		if( eaSize( &ugcProj->maps ) > SAFE_MEMBER( budget, iHardLimit )) {
			budgetHardLimitHit = true;
		}
	}

	FOR_EACH_IN_EARRAY(ugcProj->maps, UGCMap, map)
	{
		UGCRuntimeErrorContext* ctx = ugcMakeTempErrorContextMap(map->pcName);
		
		// Count components by type
		memset( map->cacheComponentCount, 0, sizeof( map->cacheComponentCount ));
		UGC_FOR_EACH_COMPONENT_ON_MAP(ugcProj->components, map->pcName, component) {
			const WorldUGCProperties* ugcProps = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", component->iObjectLibraryId );
			UGCComponentType bucketType = component->eType;

			// A few component types share buckets.
			if( bucketType == UGC_COMPONENT_TYPE_CLUSTER_PART ) {
				bucketType = UGC_COMPONENT_TYPE_OBJECT;
			}

			if( ugcProps ) {
				map->cacheComponentCount[ bucketType ] += MAX( 1, ugcProps->groupDefProps.iCost );
			} else {
				map->cacheComponentCount[ bucketType ] += 1;
			}
		} UGC_FOR_EACH_COMPONENT_END;

		if( !ugcGetIsRepublishing() ) {
			UGCComponentType type;
			for (type = 0; type < UGC_COMPONENT_TYPE_COUNT; type++) {
				UGCProjectBudget *budget = ugcFindBudget(UGC_BUDGET_TYPE_COMPONENT, type);
				if( map->cacheComponentCount[type] > SAFE_MEMBER( budget, iSoftLimit )) {
					switch (type) {
						xcase UGC_COMPONENT_TYPE_SPAWN:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentSpawn", "Map has too many spawn points." );
						xcase UGC_COMPONENT_TYPE_RESPAWN:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentRespawn", "Map has too many respawn points." );
						xcase UGC_COMPONENT_TYPE_COMBAT_JOB:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentCombatJob", "Map has too many combat nodes." );
						xcase UGC_COMPONENT_TYPE_KILL:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentKill", "Map has too many enemies." );
						xcase UGC_COMPONENT_TYPE_CONTACT:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentContact", "Map has too many contacts." );
						xcase UGC_COMPONENT_TYPE_OBJECT:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentObject", "Map has too many detail objects." );
						xcase UGC_COMPONENT_TYPE_SOUND:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentSound", "Map has too many sound objects." );
						xcase UGC_COMPONENT_TYPE_ROOM:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentRoom", "UGC.Map_TooManyComponents.Room", "Map has too many rooms." );
						xcase UGC_COMPONENT_TYPE_ROOM_MARKER:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentRoomMarker", "Map has too many volume markers." );
						xcase UGC_COMPONENT_TYPE_DIALOG_TREE:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentDialogTree", "Map has too many dialogs." );
						xcase UGC_COMPONENT_TYPE_ACTOR:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentActor", "Map has too many actors." );
						xcase UGC_COMPONENT_TYPE_CLUSTER:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentCluster", "Map has too many clusters." );
						xcase UGC_COMPONENT_TYPE_CLUSTER_PART:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentClusterPart", "Map has too many cluster parts." );
						xcase UGC_COMPONENT_TYPE_TELEPORTER:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentTeleporter", "Map has too many teleporters." );
						xcase UGC_COMPONENT_TYPE_TELEPORTER_PART:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentTeleporterPart", "Map has too many teleporter parts." );
						xcase UGC_COMPONENT_TYPE_TRAP:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentTrap", "Map has too many traps." );
						xcase UGC_COMPONENT_TYPE_TRAP_TARGET:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentTrapTarget", "Map has too many trap targets." );
						xcase UGC_COMPONENT_TYPE_TRAP_EMITTER:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentTrapEmitter", "Map has too many trap emitters." );
						xcase UGC_COMPONENT_TYPE_TRAP_TRIGGER:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentTrapTrigger", "Map has too many trap triggers." );

						xdefault:
							ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC_Errors.OverBudget_ComponentUnknown", "Map has too many components of an unknown type." );
					}
					budgetSoftLimitHit = true;
				}
				if( map->cacheComponentCount[type] > SAFE_MEMBER( budget, iHardLimit )) {
					budgetHardLimitHit = true;
				}
			}
		}
	} FOR_EACH_END;

	// Validate costumes
	{
		UGCProjectBudget *budget = ugcFindBudget(UGC_BUDGET_TYPE_COSTUME, 0);
		if( eaSize(&ugcProj->costumes) > SAFE_MEMBER( budget, iSoftLimit ))
		{
			UGCRuntimeErrorContext* ctx = ugcMakeTempErrorContextDefault();
			ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC.Project_TooManyCostumes", "Project has too many costumes" );
			budgetSoftLimitHit = true;
		}
		if( eaSize( &ugcProj->costumes ) > SAFE_MEMBER( budget, iHardLimit ))
		{
			budgetHardLimitHit = true;
		}
	}

	// Validate items
	{
		UGCProjectBudget *budget = ugcFindBudget(UGC_BUDGET_TYPE_ITEM, 0);
		if( eaSize( &ugcProj->items ) > SAFE_MEMBER( budget, iSoftLimit ))
		{
			UGCRuntimeErrorContext* ctx = ugcMakeErrorContextDefault();
			ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC.Project_TooManyItems", "Project has too many items" );
			budgetSoftLimitHit = true;
		}
		if( eaSize( &ugcProj->items ) > SAFE_MEMBER( budget, iHardLimit ))
		{
			budgetHardLimitHit = true;
		}
	}

	// Validate dialog trees
	{
		UGCProjectBudget* budget = ugcFindBudget( UGC_BUDGET_TYPE_DIALOG_TREE_PROMPT, 0 );
		int numDialogTreePrompts = 0;
		int it;

		for( it = 0; it != eaSize( &ugcProj->components->eaComponents ); ++it ) {
			UGCComponent* component = ugcProj->components->eaComponents[ it ];
			if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				numDialogTreePrompts += 1 + eaSize( &component->dialogBlock.prompts );
			}
		}

		if( numDialogTreePrompts > SAFE_MEMBER( budget, iSoftLimit )) {
			if( !ugcGetIsRepublishing() ) {
				UGCRuntimeErrorContext* ctx = ugcMakeErrorContextDefault();
				ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC.Project_TooManyDialogTreePrompts", "Project has too many dialog tree prompts" );
			}
			budgetSoftLimitHit = true;
		}
		if( numDialogTreePrompts > SAFE_MEMBER( budget, iHardLimit )) {
			budgetHardLimitHit = true;
		}
	}

	if( budgetHardLimitHit ) {
		return UGC_BUDGET_HARD_LIMIT;
	} else if( budgetSoftLimitHit ) {
		return UGC_BUDGET_SOFT_LIMIT;
	} else {
		return UGC_BUDGET_NORMAL;
	}
}

static void ugcValidateComponentUIDs( UGCProjectData* ugcProj )
{
	int i;
	U32 *ids = NULL;
	FOR_EACH_IN_EARRAY( ugcProj->components->eaComponents, UGCComponent, item)
	{
		U32 new_id = item->uID;
		for (i = eaiSize(&ids)-1; i >= 0; --i)
		{
			if (ids[i] == new_id)
			{
				ugcRaiseErrorInternalCode(UGC_FATAL_ERROR, "Bad component UID");
				eaiDestroy(&ids);
				return;
			}
		}
		eaiPush(&ids, new_id);
	}
	FOR_EACH_END;
	eaiDestroy(&ids);
}

/// Raise a UGC error if DISPLAY-STR shouldn't be allowed for a
/// message in UGC..
static void ugcValidateDisplayString( UGCRuntimeErrorContext* context, const char* fieldName, const char* displayStr, bool isRequired, bool allowProfane )
{
	assert( fieldName );

	if( !allowProfane ) {
		int* profaneRange = NULL;
		if( IsAnyProfaneList( displayStr, &profaneRange ) || gUGCDebugFailAllValidation ) {
			char* estr = NULL;
			int it;
			for( it = 0; it + 1 < eaiSize( &profaneRange ); it += 2 ) {
				int start = profaneRange[ it + 0 ];
				int end = profaneRange[ it + 1 ];
				char buffer[ 256 ] = { 0 };
				strncpy( buffer, displayStr + start, end - start );
				estrConcatf( &estr, "%s, ", buffer );
			}
			if( estrLength( &estr )) {
				estrSetSize( &estr, estrLength( &estr ) - 2 );
			}
			
			ugcRaiseErrorInFieldExtraText( UGC_ERROR, context, fieldName, "UGC.DisplayString_Profane", estr, "Text has profane characters" );
			eaiDestroy( &profaneRange );
			return;
		}
		eaiDestroy( &profaneRange );
	}

	{
		char* errPos;
		if( displayStr && !UTF8StringIsValid( displayStr, &errPos )) {
			char errValue[ 16 ];
			sprintf( errValue, "%d", *errPos );
			ugcRaiseErrorInFieldExtraText( UGC_ERROR, context, fieldName, "UGC.DisplayString_Invalid", errValue, "Text is not valid UTF8" );
		}
	}

	if( (nullStr( displayStr ) || gUGCDebugFailAllValidation) && isRequired ) {
		ugcRaiseErrorInField( UGC_ERROR, context, fieldName, "UGC.DisplayString_TooShort", "Text is not specified" );
		return;
	}

	if( displayStr && UTF8GetLength( displayStr ) > UGC_TEXT_MULTI_LINE_MAX_LENGTH ) {
		ugcRaiseErrorInField( UGC_ERROR, context, fieldName, "UGC.DisplayString_TooLong", "Text is too long" );
	}
}

/// Raise an error if MAP-LOCATION shouldn't be allowed in UGC.
///
/// NOTE: This assumes that MapLocations are valid in this project.
/// If map locations are not valid, do not call this function!
void ugcValidateMapLocation( UGCRuntimeErrorContext* context, const char* fieldName, const UGCMapLocation* pMapLocation )
{
	if(   !pMapLocation
		  || pMapLocation->positionX < 0 || pMapLocation->positionX > 1
		  || pMapLocation->positionY < 0 || pMapLocation->positionY > 1
		  || nullStr( pMapLocation->astrIcon )) {
		ugcRaiseErrorInField( UGC_ERROR, context, "DoorMapLocation", "UGC.InvalidMapLocation", "Map location is invalid." );
	} else {
		ugcValidateResource( context, "DoorMapLocation", "Texture", pMapLocation->astrIcon );
	}
}

static void ugcValidateMissionTransitions( UGCProjectData* ugcProj )
{
	UGCMapTransitionInfo** transitions = ugcMissionGetMapTransitions( ugcProj, ugcProj->mission->objectives );

	FOR_EACH_IN_EARRAY_FORWARDS(transitions, UGCMapTransitionInfo, info) {
		UGCMissionObjective* objective = ugcObjectiveFind( ugcProj->mission->objectives, info->objectiveID );
		bool nextIsInternal;
		const char* nextMapName = ugcObjectiveMapName( ugcProj, objective, &nextIsInternal );
		UGCRuntimeErrorContext* ctx = ugcMakeTempErrorContextMapTransition( ugcMissionObjectiveLogicalNameTemp( objective ), ugcProj->mission->name );
		UGCMissionMapLink *link = ugcMissionFindLink( ugcProj->mission, ugcProj->components,
													  nextIsInternal ? nextMapName : NULL,
													  info->prevIsInternal ? info->prevMapName : NULL );
		if( nextIsInternal ) {
			if( link ) {
				if( link->pDialogPrompt ) {
					ugcValidateDisplayString( ctx, "PromptTitle", link->pDialogPrompt->pcPromptTitle, false, false );
					ugcValidateDisplayString( ctx, "PromptBody", link->pDialogPrompt->pcPromptBody, false, false );

					{
						int it;
						for( it = 0; it != eaSize( &link->pDialogPrompt->eaActions ); ++it ) {
							ugcValidateDisplayString( ctx, "PromptButton", link->pDialogPrompt->eaActions[ it ]->pcText, false, false );
						}
					}
				}

				if( !ugcDefaultsSingleMissionEnabled() ) {
					if( !nullStr( link->strOpenMissionName )) {
						ugcValidateDisplayString( ctx, "OpenMissionName", link->strOpenMissionName, true, false );
					}
				}
				
				ugcValidateMapLocation( ctx, "MapLocation", link->pDoorMapLocation );
			} else {
				if( !ugcDefaultsSingleMissionEnabled() ) {
					// The following fields need to be filled in!
					ugcValidateDisplayString( ctx, "OpenMissionName", "", true, false );
				}
			}
		}

		if( link ) {
			ugcValidateDisplayString( ctx, "InteractText", link->strInteractText, false, false );
			ugcValidateDisplayString( ctx, "ReturnText", link->strReturnText, false, false );
		}
	} FOR_EACH_END;

	// Validate that all transitions are from legal positions (i.e. the first map transition)
	FOR_EACH_IN_EARRAY_FORWARDS(transitions, UGCMapTransitionInfo, info)
	{
		UGCMissionObjective *objective = ugcObjectiveFind( ugcProj->mission->objectives, info->objectiveID );
		UGCRuntimeErrorContext* ctx = ugcMakeTempErrorContextMapTransition( ugcMissionObjectiveLogicalNameTemp( objective ), ugcProj->mission->name );
		bool nextIsInternal;
		const char* nextMapName = ugcObjectiveMapName( ugcProj, objective, &nextIsInternal );
		bool transitionIsCrypticToProject = (!info->prevIsInternal && nextIsInternal);
		
		UGCMissionMapLink *link = ugcMissionFindLink( ugcProj->mission, ugcProj->components,
													  (nextIsInternal ? nextMapName : NULL),
													  (info->prevIsInternal ? info->prevMapName : NULL) );
		UGCComponent* component = NULL;

		// Cryptic -> Cryptic transitions don't matter.
		if( !info->prevIsInternal && !nextIsInternal ) {
			continue;
		}
		
		if( link ) {
			component = ugcComponentFindByID(ugcProj->components, link->uDoorComponentID);
		}

		if( ugcDefaultsMapTransitionsSpecifyDoor() || transitionIsCrypticToProject ) {
			if( !component || component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP ) {
				if( info->prevIsInternal || !SAFE_MEMBER( link, bDoorUsesMapLocation )) {
					ugcRaiseErrorInField( UGC_ERROR, ctx, "DoorComponent", "UGC.MissionDoorNotSet", "Map link must specify a door" );
				}
			}
		}
	}
	FOR_EACH_END;

	eaDestroyStruct(&transitions, parse_UGCMapTransitionInfo);
}

/// Raise a UGC error if this prompt is not fully formed
static void ugcValidateDialogTreePrompt( const UGCProjectData* data, const UGCComponent* component, const UGCDialogTreePrompt* prompt, const UGCComponent* contact, bool requireCostume, bool* hasSuccess )
{
	UGCRuntimeErrorContext* ctx = ugcMakeErrorContextChallenge( ugcComponentGetLogicalNameTemp(component), NULL, NULL );
	const char* mapName;

	if( contact ) {
		mapName = contact->sPlacement.pcMapName;
	} else {
		mapName = component->sPlacement.pcMapName;
	}

	if( prompt != &component->dialogBlock.initialPrompt ) {
		char promptName[ 256 ];
		sprintf( promptName, "Prompt_%d", prompt->uid );
		ctx->prompt_name = StructAllocString( promptName );
	}

	ugcValidateDisplayString( ctx, "PromptTitle", prompt->pcPromptTitle, false, false );
	if( !ugcGetIsRepublishing() ) {
		ugcValidateDisplayString( ctx, "PromptBody", prompt->pcPromptBody, true, false );
	}
	
	if( requireCostume && nullStr( prompt->pcPromptCostume ) && !IS_HANDLE_ACTIVE( prompt->hPromptPetCostume ) || gUGCDebugFailAllValidation) {
		ugcRaiseErrorInField( UGC_ERROR, ctx, "PromptCostume", "UGC.Block_NoCostume", "No costume specified for this block." );
	}

	{
		int actionIt;
		for( actionIt = 0; actionIt != eaSize( &prompt->eaActions ); ++actionIt ) {
			UGCDialogTreePromptAction* action = prompt->eaActions[ actionIt ];
			ctx->prompt_action_index = actionIt;

			ugcValidateDisplayString( ctx, "Text", action->pcText, false, false );
			if( action->nextPromptID == 0 && !action->bDismissAction ) {
				*hasSuccess = true;
			}
			if( action->pShowWhen ) {
				ugcValidateUGCWhen( data, action->pShowWhen, NULL, 0, mapName, true, true, ctx, 0 );
			}
			if( action->pHideWhen ) {
				ugcValidateUGCWhen( data, action->pHideWhen, NULL, 0, mapName, false, true, ctx, 1 );
			}
		}
		ctx->prompt_action_index = -1;
	}

	StructDestroy( parse_UGCRuntimeErrorContext, ctx );
}

/// Raise a UGC error if MAP-NAME/OBJECT-NAME is not of the right type
/// or is not allowed in UGC.
static void ugcValidateExternalMapObject( UGCRuntimeErrorContext* context, const UGCProjectData* data, WorldEncounterObjectType type, WorldEncounterObjectInteractType interactType,
	const char* mapName, const char* objectName )
{
	ZoneMapEncounterObjectInfo* objectInfo = zeniObjectFind( mapName, objectName );

	if( !objectInfo || !IS_HANDLE_ACTIVE( objectInfo->displayName ) || gUGCDebugFailAllValidation ) {
		ugcRaiseError( UGC_ERROR, context, "UGC.ExternalMapObject_NotFound", "Map %s does not have resource %s.", mapName, objectName );
		return;
	}
	if( (type != -1 && objectInfo->type != type) || (interactType != -1 && objectInfo->interactType != interactType) ) {
		ugcRaiseError( UGC_ERROR, context, "UGC.ExternalMapObject_WrongType", "Resource %s is of wrong type.", objectName );
		return;
	}

	// validate restrictions
	{
		WorldUGCRestrictionProperties restriction = { 0 };
		StructCopyAll( parse_WorldUGCRestrictionProperties, &objectInfo->restrictions, &restriction );
		ugcRestrictionsIntersect( &restriction, data->project->pRestrictionProperties );

		if( !ugcRestrictionsIsValid( &restriction ) || gUGCDebugFailAllValidation ) {
			ugcRaiseError( UGC_ERROR, context, "UGC.Component_Unavailable", "Component does not satisfy project restrictions." );
		}
		StructReset( parse_WorldUGCRestrictionProperties, &restriction );
	}
}

static void ugcValidateTagType(const UGCProjectData* data, UGCRuntimeErrorContext *context, const char *field_name, const char *object_name, const char *tag_type_name, bool optional)
{
	UGCAssetTagType *tag_type = RefSystem_ReferentFromString("TagType", tag_type_name);
	ResourceInfo* resInfo = NULL;
	bool found = false;

	if(!tag_type)
	{
		AssertOrAlert("UGC_UNKNOWN_TAG_TYPE", "Tag type not found: '%s'", NULL_TO_EMPTY(tag_type_name));
		return;
	}

	if (optional && nullStr( object_name ))
		return;

	if (stricmp(tag_type_name, "Costume") == 0)
	{
		FOR_EACH_IN_EARRAY(data->costumes, UGCCostume, costume)
		{
			if ((!ugcIsSpaceEnabled() || costume->eRegion == (U32)StaticDefineIntGetInt(CharClassTypesEnum, "Ground")) &&
				resNamespaceBaseNameEq(costume->astrName, object_name))
			{
				found = true;
				break;
			}
		}
		FOR_EACH_END;
	}
	else if (stricmp(tag_type_name, "SpaceCostume") == 0)
	{
		FOR_EACH_IN_EARRAY(data->costumes, UGCCostume, costume)
		{
			if (costume->eRegion == (U32)StaticDefineIntGetInt(CharClassTypesEnum, "Space") &&
				resNamespaceBaseNameEq(costume->astrName, object_name))
			{
				found = true;
				break;
			}
		}
		FOR_EACH_END;
	}
	if (found)
		return;

	if( nullStr( object_name )) {
		ugcRaiseErrorInField( UGC_ERROR, context, field_name, "UGC.Resource_NotSpecified", "Resource has not been specified" );
		return;
	}

	resInfo = ugcResourceGetInfo(tag_type->pcDictName, object_name);
	if (!resInfo || gUGCDebugFailAllValidation)
	{
		ugcRaiseErrorInField(UGC_ERROR, context, field_name, "UGC.Resource_NotFound", "%s %s - resource does not exist", tag_type->pcDictName, object_name);
		return;
	}

	if (!ugcHasTagType(resInfo->resourceTags, tag_type->pcDictName, tag_type_name))
	{
		ugcRaiseErrorInField(UGC_ERROR, context, field_name, "UGC.Resource_NotFound", "%s %s - resource not tagged as expected", tag_type->pcDictName, object_name);
		return;
	}
}

static void ugcValidateTagTypeInt(const UGCProjectData* data, UGCRuntimeErrorContext *context, const char *field_name, int object_name, const char *tag_type_name, bool optional)
{
	char objectNameAsString[ RESOURCE_NAME_MAX_SIZE ];
	if (object_name)
		sprintf( objectNameAsString, "%d", object_name );
	else
		objectNameAsString[0] = '\0';
	ugcValidateTagType(data, context, field_name, objectNameAsString, tag_type_name, optional);
}

static void ugcValidateMap( const UGCProjectData* data, UGCBacklinkTable* pBacklinkTable, const UGCMap* map )
{
	UGCRuntimeErrorContext* ctx = ugcMakeTempErrorContextMap( map->pcName );
	UGCMapType map_type = ugcMapGetType(map);
	UGCGenesisBackdrop* ugc_backdrop = NULL;
	UGCPerProjectDefaults* defaults = ugcGetDefaults();

	ugcValidateDisplayString( ctx, "DisplayName", map->pcDisplayName, true, false );

	if (!ugcIsSpaceEnabled())
	{
		if (map_type == UGC_MAP_TYPE_SPACE ||
			map_type == UGC_MAP_TYPE_PREFAB_SPACE)
		{
			ugcRaiseErrorInternalCode(UGC_FATAL_ERROR, "Space maps not valid in this project!");
			return;
		}
	}

	// Validate backdrop
	if (map->pSpace)
	{
		ugc_backdrop = &map->pSpace->backdrop;
	}
	if (map->pPrefab)
	{
		ugc_backdrop = &map->pPrefab->backdrop;

		switch (map_type)
		{
		case UGC_MAP_TYPE_PREFAB_INTERIOR:
			ugcValidateTagType( data, ctx, "PrefabName", map->pPrefab->map_name, "PrefabInteriorMap", false);
			break;
		case UGC_MAP_TYPE_PREFAB_GROUND:
			ugcValidateTagType( data, ctx, "PrefabName", map->pPrefab->map_name, "PrefabGroundMap", false);
			break;
		case UGC_MAP_TYPE_PREFAB_SPACE:
			ugcValidateTagType( data, ctx, "PrefabName", map->pPrefab->map_name, "PrefabSpaceMap", false);
			break;

		case UGC_MAP_TYPE_INTERIOR:
			if( stricmp_safe( defaults->pcCustomInteriorMap, map->pPrefab->map_name ) != 0 ) {
				ugcRaiseErrorInternalCode(UGC_FATAL_ERROR, "UGC -- Prefab map is set to invalid map name %s, should be %s",
					map->pPrefab->map_name, defaults->pcCustomInteriorMap );
			}
			break;

		case UGC_MAP_TYPE_GROUND:
		case UGC_MAP_TYPE_SPACE:
			ugcRaiseErrorInternalCode(UGC_FATAL_ERROR, "UGC -- Prefab map is set, it should not be." );
			break;
		}
	}

	// Only one dialog can start during "Map Start"
	{
		int numMapStartDialogsAccum = 0;
		int it;
		for( it = 0; it != eaSize( &data->components->eaComponents ); ++it ) {
			UGCComponent* component = data->components->eaComponents[ it ];

			if( component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE || !ugcComponentIsOnMap( component, map->pcName, false )) {
				continue;
			}

			if( ugcComponentStartWhenType( component ) == UGCWHEN_MAP_START ) {
				++numMapStartDialogsAccum;
			}
		}

		if( numMapStartDialogsAccum > 1 ) {
			for( it = 0; it != eaSize( &data->components->eaComponents ); ++it ) {
				UGCComponent* component = data->components->eaComponents[ it ];
				UGCRuntimeErrorContext* componentCtx = NULL;

				if( component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE || !ugcComponentIsOnMap( component, map->pcName, false )) {
					continue;
				}
				componentCtx = ugcMakeErrorContextChallenge( ugcComponentGetLogicalNameTemp(component), NULL, NULL );

				if( ugcComponentStartWhenType( component ) == UGCWHEN_MAP_START ) {
					ugcRaiseErrorInField( UGC_ERROR, componentCtx, "When.0.Type", "UGC.When.DialogTree_TooManyMapStart", "Only one dialog is allowed to start at map start." );
				}

				StructDestroy( parse_UGCRuntimeErrorContext, componentCtx );
			}
		}
	}

	if (ugc_backdrop)
	{
		if (!ugcMapCanCustomizeBackdrop(map_type))
		{
			// This should not happen
			ugcRaiseErrorInternalCode(UGC_FATAL_ERROR, "Customized backdrop on an invalid map type!");
			return;
		}

		if (map_type == UGC_MAP_TYPE_SPACE || map_type == UGC_MAP_TYPE_GROUND || map_type == UGC_MAP_TYPE_PREFAB_GROUND)
			ugcValidateTagType( data, ctx, "AmbSound", ugc_backdrop->strAmbientSoundOverride, "UGCAmbientSound", /*optional=*/true);

		FOR_EACH_IN_EARRAY(ugc_backdrop->eaSkyOverrides, UGCGenesisBackdropSkyOverride, sky)
		{
			char field_name[32];
			int slot = FOR_EACH_IDX(ugc_backdrop->pSkyGroup->override_list, sky);
			sprintf(field_name, "SkyOverride%d", slot);
			switch (map_type)
			{
				case UGC_MAP_TYPE_INTERIOR:
				case UGC_MAP_TYPE_PREFAB_INTERIOR:
					ugcValidateTagType( data, ctx, field_name, REF_STRING_FROM_HANDLE(sky->hSkyOverride), "InteriorSkyLayerOverride", true);
					break;
				case UGC_MAP_TYPE_GROUND:
				case UGC_MAP_TYPE_PREFAB_GROUND:
					ugcValidateTagType( data, ctx, field_name, REF_STRING_FROM_HANDLE(sky->hSkyOverride), "ExteriorSkyLayerOverride", true);
					break;
				case UGC_MAP_TYPE_SPACE:
				case UGC_MAP_TYPE_PREFAB_SPACE:
					ugcValidateTagType( data, ctx, field_name, REF_STRING_FROM_HANDLE(sky->hSkyOverride), "SpaceSkyLayerOverride", true);
					break;
			}
		}
		FOR_EACH_END;

		switch (map_type)
		{
		case UGC_MAP_TYPE_INTERIOR:
		case UGC_MAP_TYPE_PREFAB_INTERIOR:
			ugcValidateTagType( data, ctx, "SkyBase", REF_STRING_FROM_HANDLE(ugc_backdrop->hSkyBase), "InteriorSkyLayerBase", false);
			break;
		case UGC_MAP_TYPE_GROUND:
		case UGC_MAP_TYPE_PREFAB_GROUND:
			ugcValidateTagType( data, ctx, "SkyBase", REF_STRING_FROM_HANDLE(ugc_backdrop->hSkyBase), "ExteriorSkyLayerBase", false);
			break;
		case UGC_MAP_TYPE_SPACE:
		case UGC_MAP_TYPE_PREFAB_SPACE:
			ugcValidateTagType( data, ctx, "SkyBase", REF_STRING_FROM_HANDLE(ugc_backdrop->hSkyBase), "SpaceSkyLayerBase", false);
			break;
		}
	}

	// Custom Indoor maps need to have at least one room in them
	if( map_type == UGC_MAP_TYPE_INTERIOR ) {
		UGCComponent** eaMapComponents = NULL;
		int it;
		ugcBacklinkTableGetMapComponents( data, pBacklinkTable, map->pcName, &eaMapComponents );
		for( it = eaSize( &eaMapComponents ) - 1; it >= 0; --it ) {
			if( eaMapComponents[ it ]->eType == UGC_COMPONENT_TYPE_ROOM ) {
				break;
			}
		}
		if( it < 0 ) {
			ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC.Map_CustomInteriorHasNoRooms", "Map has no rooms" );
		}
	}
}

static void ugcValidateComponentBehavior( const UGCProjectData* data, UGCRuntimeErrorContext *ctx, const UGCComponent *component)
{
	if (component->eType == UGC_COMPONENT_TYPE_KILL)
	{
		ugcValidateTagType(data, ctx, "FSMRef", component->fsmProperties.pcFSMNameRef, "Behavior", false);

		if(!nullStr(component->pcDropItemName))
		{
			ResourceInfo *resInfo = ugcResourceGetInfo("FSM", component->fsmProperties.pcFSMNameRef);
			if(ugcHasTag(resInfo->resourceTags, "DropItemNotAllowed"))
				ugcRaiseErrorInField(UGC_ERROR, ctx, "FSMRef", "UGC.Behavior_DropItemNotAllowed", "%s %s - does not allow dropped items", "FSM", component->fsmProperties.pcFSMNameRef);
		}
	}
	else if (component->eType == UGC_COMPONENT_TYPE_CONTACT)
		ugcValidateTagType(data, ctx, "FSMRef", component->fsmProperties.pcFSMNameRef, "NonCombatBehavior", false);

	FOR_EACH_IN_EARRAY_FORWARDS(component->fsmProperties.eaExternVarsV1, UGCFSMVar, var)
	{
		char field_name[128];
		sprintf(field_name, "%s.StringVal", var->astrName);
		ugcValidateDisplayString( ctx, field_name, var->strStringVal, false, false );
	}
	FOR_EACH_END;
}

void ugcValidateUGCWhen( const UGCProjectData* data, UGCWhen *when, const UGCMissionObjective *objective, int componentObjectiveCount, const char* internal_map_name, bool is_show, bool allow_objective_when, UGCRuntimeErrorContext* ctx, int when_idx )
{
	int i;
	if (objective)
	{
		if (is_show)
		{
			if (when->eType != UGCWHEN_OBJECTIVE_IN_PROGRESS &&
				when->eType != UGCWHEN_MAP_START)
			{
				ugcRaiseErrorInternalCode(UGC_FATAL_ERROR, "Bad objective component show type: %d", when->eType);
			}
		}
		else
		{
			if (when->eType != UGCWHEN_CURRENT_COMPONENT_COMPLETE &&
				when->eType != UGCWHEN_OBJECTIVE_COMPLETE &&
				when->eType != UGCWHEN_MANUAL)
			{
				ugcRaiseErrorInternalCode(UGC_FATAL_ERROR, "Bad objective component hide type: %d", when->eType);
			}
		}
	}
	switch (when->eType)
	{
	case UGCWHEN_OBJECTIVE_IN_PROGRESS:
	case UGCWHEN_OBJECTIVE_COMPLETE:
	case UGCWHEN_OBJECTIVE_START:
		if( !allow_objective_when ) {
			char field_name[256];
			sprintf(field_name, "When.%d.Type", when_idx);
			ugcRaiseErrorInField( UGC_ERROR, ctx, field_name, "UGC.When.InvalidType", "State transition does not have a valid type specified.");
		} else if( componentObjectiveCount > 1 ) {
			char field_name[256];
			assert( objective );
			sprintf( field_name, "When.%d.Type", when_idx );
			if( !ugcGetIsRepublishing() ) {
				ugcRaiseErrorInField( UGC_ERROR, ctx, field_name, "UGC.When.AmbiguousType_MultipleObjectives", "This component has multiple related objectives, it is ambiguous which one will be used." );
			}
		} else {
			for (i = eaiSize(&when->eauObjectiveIDs)-1; i >= 0; --i)
			{
				assert(when->eauObjectiveIDs);
				if (objective)
				{
					if (objective->id != when->eauObjectiveIDs[i])
					{
						ugcRaiseErrorInternalCode(UGC_FATAL_ERROR, "Bad objective component when objective");
					}
				}
				if (when->eauObjectiveIDs[i] == -1 || gUGCDebugFailAllValidation)
				{
					char field_name[256];
					sprintf(field_name, "When.%d.ObjectiveID.%d", when_idx, i);
					ugcRaiseErrorInField( UGC_ERROR, ctx, field_name, "UGC.When.NoObjectiveSelected", "State transition has no objective selected." );
				}
			}
		}
		break;

	case UGCWHEN_COMPONENT_COMPLETE:
	case UGCWHEN_COMPONENT_REACHED:
		for (i = eaiSize(&when->eauComponentIDs)-1; i >= 0; --i)
		{
			char field_name[256];
			sprintf(field_name, "When.%d.ComponentID.%d", when_idx, i);
				
			assert(when->eauComponentIDs);
			if (when->eauComponentIDs[i] == -1 || gUGCDebugFailAllValidation) {
				ugcRaiseErrorInField( UGC_ERROR, ctx, field_name, "UGC.When.NoComponentSelected", "State transition has no component selected." );
			} else {
				UGCComponent* otherComponent = ugcComponentFindByID( data->components, when->eauComponentIDs[ i ]);

				if( otherComponent && otherComponent->eType == UGC_COMPONENT_TYPE_DIALOG_TREE && otherComponent->uActorID ) {
					otherComponent = ugcComponentFindByID( data->components, otherComponent->uActorID );
				}

				if( !otherComponent ) {
					ugcRaiseErrorInField( UGC_ERROR, ctx, field_name, "UGC.When.NoComponentSelected", "State transition has no component selected." );
				} else if( !internal_map_name || stricmp( otherComponent->sPlacement.pcMapName, internal_map_name ) != 0 ) {
					ugcRaiseErrorInField( UGC_ERROR, ctx, field_name, "UGC.When.ComponentNotOnMap", "State transition has a component on a different map selected." );
				}
			}
		}
		break;
	case UGCWHEN_CURRENT_COMPONENT_COMPLETE:
	case UGCWHEN_MISSION_START:
	case UGCWHEN_MAP_START:
	case UGCWHEN_MANUAL:
		break;
	}
}

static void ugcValidateKillActorDensity(const UGCProjectData* data, UGCRuntimeErrorContext *ctx, const UGCComponent *component)
{
	int child_idx, child_idx_other;
	UGCMapType map_type = ugcMapGetType(ugcMapFindByName(data, component->sPlacement.pcMapName));
	UGCPerProjectDefaults *defaults = ugcGetDefaults();
	F32 radius = (map_type == UGC_MAP_TYPE_SPACE || map_type == UGC_MAP_TYPE_PREFAB_SPACE) ? defaults->fSpaceAggroDistance : defaults->fGroundAggroDistance;
	F32 actor_distance = (map_type == UGC_MAP_TYPE_SPACE || map_type == UGC_MAP_TYPE_PREFAB_SPACE) ? defaults->fSpaceMaxActorDistance : defaults->fGroundMaxActorDistance;

	for (child_idx = 0; child_idx < eaiSize(&component->uChildIDs); child_idx++)
	{
		const UGCComponent *actor = ugcComponentFindByID(data->components, component->uChildIDs[child_idx]);
		int all_actor_count = eaiSize(&component->uChildIDs); // Count the current encounter
		int enemy_actor_count = 0;
		int friendly_actor_count = 0;

		if (ugcProjectFilterAllegiance(data, "Enemy", component->iObjectLibraryId))
			enemy_actor_count += eaiSize(&component->uChildIDs);
		else
			friendly_actor_count += eaiSize(&component->uChildIDs);

		if (actor_distance > 0 && distance3(actor->sPlacement.vPos, component->sPlacement.vPos) > actor_distance || gUGCDebugFailAllValidation)
		{
			ugcRaiseErrorInField( UGC_FATAL_ERROR, ctx, "Position", "UGC.ActorTooFarFromEncounter", "Actor is too far from encounter center point." );
		}

		UGC_FOR_EACH_COMPONENT_ON_MAP(data->components, component->sPlacement.pcMapName, other_component)
		{
			if (other_component != component && other_component->eType == UGC_COMPONENT_TYPE_KILL)
			{
				int actor_count = 0;

				for (child_idx_other = 0; child_idx_other < eaiSize(&other_component->uChildIDs); child_idx_other++)
				{
					const UGCComponent *other_actor = ugcComponentFindByID(data->components, other_component->uChildIDs[child_idx_other]);
					if (distance3(other_actor->sPlacement.vPos, actor->sPlacement.vPos) < radius)
					{
						actor_count += eaiSize(&other_component->uChildIDs);
						break;
					}
				}

				all_actor_count += actor_count;
				if (ugcProjectFilterAllegiance(data, "Enemy", other_component->iObjectLibraryId))
					enemy_actor_count += actor_count;
				else
					friendly_actor_count += actor_count;
			}
		}
		UGC_FOR_EACH_COMPONENT_END;

		if (defaults->iMaxActorsInAggroDist > 0 && all_actor_count > defaults->iMaxActorsInAggroDist || gUGCDebugFailAllValidation)
		{
			ugcRaiseErrorInField( UGC_FATAL_ERROR, ctx, "Position", "UGC.EncountersTooCloseTogether", "Too many encounter actors in close proximity." );
			break;
		}
		if (defaults->iMaxEnemyActorsInAggroDist > 0 && enemy_actor_count > defaults->iMaxEnemyActorsInAggroDist)
		{
			ugcRaiseErrorInField( UGC_FATAL_ERROR, ctx, "Position", "UGC.EncountersTooCloseTogether", "Too many encounter actors in close proximity." );
			break;
		}
		if (defaults->iMaxFriendlyActorsInAggroDist > 0 && friendly_actor_count > defaults->iMaxFriendlyActorsInAggroDist)
		{
			ugcRaiseErrorInField( UGC_FATAL_ERROR, ctx, "Position", "UGC.EncountersTooCloseTogether", "Too many encounter actors in close proximity." );
			break;
		}
	}
}
 
static int ugcValidateComponentCountRelatedObjectives1( const UGCProjectData* ugcProj, U32 componentID, UGCMissionObjective** objectives, bool includeDialogTreeActors )
{
	int accum = 0;
	
	int it;
	int extraIt;
	for( it = 0; it != eaSize( &objectives ); ++it ) {
		UGCMissionObjective* objective = objectives[ it ];
		if( objective->type == UGCOBJ_COMPLETE_COMPONENT || objective->type == UGCOBJ_UNLOCK_DOOR ) {
			const UGCComponent* objectiveComponent = ugcComponentFindByID( ugcProj->components, objective->componentID );
			if( objectiveComponent ) {
				if( objectiveComponent->uID == componentID ) {
					++accum;
				} else if( includeDialogTreeActors
						   && objectiveComponent->eType == UGC_COMPONENT_TYPE_DIALOG_TREE
						   && objectiveComponent->uActorID == componentID ) {
					++accum;
				}
			}

			for( extraIt = 0; extraIt != eaiSize( &objective->extraComponentIDs ); ++extraIt ) {
				UGCComponent* objectiveExtraComponent = ugcComponentFindByID( ugcProj->components, objective->extraComponentIDs[ extraIt ]);
				if( objectiveExtraComponent ) {
					if( objectiveExtraComponent->uID == componentID ) {
						++accum;
					} else if( includeDialogTreeActors
							   && objectiveExtraComponent->eType == UGC_COMPONENT_TYPE_DIALOG_TREE
							   && objectiveExtraComponent->uActorID == componentID ) {
						++accum;
					}
				}
			}
		}

		accum += ugcValidateComponentCountRelatedObjectives1( ugcProj, componentID, objective->eaChildren, includeDialogTreeActors );
	}
	
	return accum;
}

static int ugcValidateComponentCountRelatedObjectives( const UGCProjectData* ugcProj, U32 componentID, UGCMission* mission, bool includeDialogTreeActors )
{
	int accum = 0;
	
	int it;
	for( it = 0; it != eaSize( &mission->map_links ); ++it ) {
		UGCMissionMapLink* mapLink = mission->map_links[ it ];

		if( mapLink->uDoorComponentID == componentID ) {
			++accum;
			
			// We don't care if a player exits a map in the same way
			// multiple times -- but it can't be used in an objective
			// if it's being used to exit!
			break;
		}
	}
	if( mission->return_map_link ) {
		if( mission->return_map_link->uDoorComponentID == componentID ) {
			++accum;
		}
	}

	accum += ugcValidateComponentCountRelatedObjectives1( ugcProj, componentID, mission->objectives, includeDialogTreeActors );

	return accum;
}

static void ugcValidateComponent( const UGCProjectData* data, UGCBacklinkTable* pBacklinkTable, const UGCComponent* component, UGCMapTransitionInfo** transitions )
{
	UGCRuntimeErrorContext* ctx = ugcMakeErrorContextChallenge( ugcComponentGetLogicalNameTemp(component), NULL, NULL );
	const UGCMissionObjective* rel_objective = ugcObjectiveFindComponentRelatedUsingTableConst( data, pBacklinkTable, component->uID);

	// MJF Aug/7/2013 -- There shouldn't be a distinction between
	// objectives this component appears in and objectives this
	// component appears in including as a dialog tree actor, but
	// there was a bug in validation earlier where dialog tree actors
	// weren't counted.  65% of important projects all took advantage
	// of this gap in validation.
	//
	// So instead we plug the holes that are formed with this gap --
	// "This Objective In Progress" and "This Objective Complete" are
	// ambiguous and not if a component is used in mulitple objectives
	// includig as a dialog tree actor.  This was only taken
	// adavantage of by 1% of important projects.
	//
	// Long term, we should get rid of the "You can't use a component
	// in more than one objective" limitation.
	int componentObjectiveCount = ugcValidateComponentCountRelatedObjectives( data, component->uID, data->mission, false );
	int componentObjectiveCountIncludingDialogTreeActors = ugcValidateComponentCountRelatedObjectives( data, component->uID, data->mission, true );
	UGCMapType map_type = UGC_MAP_TYPE_ANY;
	bool map_is_space = false, map_is_ground = false;

	FOR_EACH_IN_EARRAY(component->eaTriggerGroups, UGCInteractProperties, interact_properties)
	{
		ugcValidateInteractProperties(data, ctx, interact_properties);
	}
	FOR_EACH_END;

	if (component->sPlacement.uRoomID != GENESIS_UNPLACED_ID &&
		!component->sPlacement.bIsExternalPlacement)
	{
		UGCMap *map = ugcMapFindByName(data, component->sPlacement.pcMapName);
		Vec3 placeable_min, placeable_max;
		map_type = ugcMapGetType(map);
		if (map_type == UGC_MAP_TYPE_SPACE || map_type == UGC_MAP_TYPE_PREFAB_SPACE)
			map_is_space = true;
		else
			map_is_ground = true;

		if (component->eType != UGC_COMPONENT_TYPE_WHOLE_MAP)
		{
			F32 spawn_height = 0;
			F32 component_height = 0;

			if (map->pPrefab)
			{
				Vec3 out_spawn_pos;
				if (ugcGetZoneMapSpawnPoint(map->pPrefab->map_name, out_spawn_pos, NULL))
					spawn_height = out_spawn_pos[1];
			}
			ugcMapComponentValidBounds( placeable_min, placeable_max, data, pBacklinkTable, map, component );
			if (component->sPlacement.vPos[0] < placeable_min[0] ||
				component->sPlacement.vPos[0] > placeable_max[0] ||
				(component->sPlacement.vPos[1]+spawn_height) < placeable_min[1] ||
				(component->sPlacement.vPos[1]+spawn_height) > placeable_max[1] ||
				component->sPlacement.vPos[2] < placeable_min[2] ||
				component->sPlacement.vPos[2] > placeable_max[2] ||
				gUGCDebugFailAllValidation)
			{
				ugcRaiseErrorInField( UGC_FATAL_ERROR, ctx, "Position", "UGC.ComponentOutOfBounds", "Component is out of playable bounds." );
			}

			component_height = ugcComponentGetWorldHeight(component, data->components);
			// Room doors and fake door positions are validated by the room they are in.
			if (  component->eType != UGC_COMPONENT_TYPE_ROOM_DOOR && component->eType != UGC_COMPONENT_TYPE_FAKE_DOOR
				  && !ugcComponentIsValidPosition(data, pBacklinkTable, component, component->sPlacement.vPos, NULL, false, 0, 0, NULL))
			{
				ugcRaiseErrorInField( UGC_FATAL_ERROR, ctx, "Position", "UGC.ComponentPlacementNotValid", "Component is not in a valid location." );
			}
		}
	}

	ugcValidateDisplayString( ctx, "VisibleName", component->pcVisibleName, false, false );
	ugcValidateDisplayString( ctx, "ActorCritterGroupName", component->pcActorCritterGroupName, false, false );

	if (component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE)
	{
		if( ugcComponentIsOnMap( component, NULL, false ) ) {
			const char* externalMapName = component->sPlacement.pcExternalMapName;
			const char* externalObjectName = component->sPlacement.pcExternalObjectName;
			switch( component->eType ) {
			case UGC_COMPONENT_TYPE_KILL:
				ugcValidateExternalMapObject( ctx, data, WL_ENC_ENCOUNTER, -1, externalMapName, externalObjectName );
			xcase UGC_COMPONENT_TYPE_CONTACT:
				ugcValidateExternalMapObject( ctx, data, -1, WL_ENC_CONTACT, externalMapName, externalObjectName );
			xcase UGC_COMPONENT_TYPE_OBJECT:
			case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
				ugcValidateExternalMapObject( ctx, data, WL_ENC_INTERACTABLE, WL_ENC_CLICKIE, externalMapName, externalObjectName );
			xcase UGC_COMPONENT_TYPE_DESTRUCTIBLE:
				ugcValidateExternalMapObject( ctx, data, WL_ENC_INTERACTABLE, WL_ENC_DESTRUCTIBLE, externalMapName, externalObjectName );
			xcase UGC_COMPONENT_TYPE_EXTERNAL_DOOR:
				ugcValidateExternalMapObject( ctx, data, WL_ENC_INTERACTABLE, WL_ENC_DOOR, externalMapName, externalObjectName );
			xcase UGC_COMPONENT_TYPE_ROOM_MARKER:
			case UGC_COMPONENT_TYPE_PLANET:
				ugcValidateExternalMapObject( ctx, data, WL_ENC_NAMED_VOLUME, -1, externalMapName, externalObjectName );
			xcase UGC_COMPONENT_TYPE_REWARD_BOX:
				ugcValidateExternalMapObject( ctx, data, WL_ENC_INTERACTABLE, WL_ENC_REWARD_BOX, externalMapName, externalObjectName );
			xcase UGC_COMPONENT_TYPE_WHOLE_MAP: case UGC_COMPONENT_TYPE_SPAWN:
					// No validation to do here
			xdefault:
				ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC.InternalError", "Unexpected component type." );
			}
		}
	}

	switch( component->eType ) {
	xcase UGC_COMPONENT_TYPE_OBJECT:
		if (map_is_ground)
			ugcValidateTagTypeInt(data, ctx, "ObjectID", component->iObjectLibraryId, "Detail", false);
		else if (map_is_space)
			ugcValidateTagTypeInt(data, ctx, "ObjectID", component->iObjectLibraryId, "SpaceDetail", false);

	xcase UGC_COMPONENT_TYPE_SOUND:
		ugcValidateTagType(data, ctx, "SoundEvent", component->strSoundEvent, "UGCSound", false);

	xcase UGC_COMPONENT_TYPE_COMBAT_JOB:
		ugcValidateTagTypeInt(data, ctx, "ObjectID", component->iObjectLibraryId, "CombatJob", false);

	xcase UGC_COMPONENT_TYPE_ROOM:
		ugcValidateTagTypeInt(data, ctx, "ObjectID", component->iObjectLibraryId, "Room", false);

		ugcValidateTagType(data, ctx, "SoundEvent", component->strSoundEvent, "UGCAmbientSound", true);

		// Check for collisions
		if (ugcRoomCheckCollision(data->components, component, component->sPlacement.vPos, component->sPlacement.vRotPYR, true) || gUGCDebugFailAllValidation)
		{
			ugcRaiseError( UGC_FATAL_ERROR, ctx, "UGC.Room_Colliding", "Room is colliding with another room." );
		}

		// Check for invalid doors
		{
			int door_id;
			UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);
			if (room_info && eaSize(&room_info->doors))
			{
				for (door_id = 0; door_id < eaSize(&room_info->doors); door_id++)
				{
					UGCComponent* door_component = NULL;
					int* eaDoorTypeID = NULL;
					UGCDoorSlotState state = ugcRoomGetDoorSlotState(data->components, component, door_id, &door_component, &eaDoorTypeID, NULL, NULL);
					UGCRuntimeErrorContext* doorCtx = NULL;
					if( door_component ) {
						doorCtx = ugcMakeErrorContextChallenge( ugcComponentGetLogicalNameTemp( door_component ), NULL, NULL  );
					}
					
					if (state == UGC_DOOR_SLOT_OCCUPIED_MULTIPLE || gUGCDebugFailAllValidation)
					{
						ugcRaiseError( UGC_FATAL_ERROR, doorCtx, "UGC.RoomDoor_Multiple", "Multiple doors in same door location." );
					}
					else if( state == UGC_DOOR_SLOT_OCCUPIED && door_component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR )
					{
						bool valid = false;
						UGC_FOR_EACH_COMPONENT_OF_TYPE(data->components, UGC_COMPONENT_TYPE_ROOM, other_room_component)
						{
							if (other_room_component != component &&
								resNamespaceBaseNameEq(other_room_component->sPlacement.pcMapName, component->sPlacement.pcMapName))
							{
								int other_door_idx;
								if (ugcRoomIsDoorConnected(other_room_component, door_component, &other_door_idx))
								{
									valid = true;
									break;
								}
							}
						}
						UGC_FOR_EACH_COMPONENT_END;
						if (!valid)
						{
							ugcRaiseError( UGC_FATAL_ERROR, doorCtx, "UGC.RoomDoor_InvalidOpen", "Room door placed in a fake door location." );
						}
					}

					if( state == UGC_DOOR_SLOT_OCCUPIED && door_component->iObjectLibraryId ) {
						int doorID = ugcRoomDoorGetTypeIDForResourceInfo( ugcResourceGetInfoInt( "ObjectLibrary", door_component->iObjectLibraryId ));
						if( eaiFind( &eaDoorTypeID, doorID ) == -1 ) {
							ugcRaiseErrorInField( UGC_ERROR, doorCtx, "ObjectID", "UGC.RoomDoor_InvalidObject", "Door Object is not valid for this position." );
						}
					}

					eaiDestroy( &eaDoorTypeID );
					StructDestroy( parse_UGCRuntimeErrorContext, doorCtx );
				}
			}
		}

	xcase UGC_COMPONENT_TYPE_ROOM_DOOR:
		ugcValidateTagTypeInt(data, ctx, "ObjectID", component->iObjectLibraryId, "RoomDoor", !rel_objective);
		
	xcase UGC_COMPONENT_TYPE_FAKE_DOOR:
		ugcValidateTagTypeInt(data, ctx, "ObjectID", component->iObjectLibraryId, "RoomDoor", false);

	xcase UGC_COMPONENT_TYPE_PLANET:
		ugcValidateTagTypeInt(data, ctx, "ObjectID", component->iObjectLibraryId, "Planet", false);
		ugcValidateTagTypeInt(data, ctx, "RingObjectName", component->iPlanetRingId, "PlanetRing", true);

	xcase UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
		ugcValidateTagTypeInt(data, ctx, "ObjectID", component->iObjectLibraryId, "Building", false);

	xcase UGC_COMPONENT_TYPE_CONTACT:
		if(component->fsmProperties.pcFSMNameRef)
			ugcValidateComponentBehavior(data, ctx, component);

		if (map_is_ground)
		{
			ugcValidateTagType(data, ctx, "CostumeName", component->pcCostumeName, "Costume", false);
		}
		else if (map_is_space)
		{
			ugcValidateTagType(data, ctx, "CostumeName", component->pcCostumeName, "SpaceCostume", false);
		}

	xcase UGC_COMPONENT_TYPE_DIALOG_TREE: {
			UGCComponent* contact = ugcComponentFindByID( data->components, component->uActorID );
			UGCMissionObjective* objective = ugcObjectiveFindComponent( data->mission->objectives, component->uID );

			{
				int promptIt;
				bool hasSuccess = false;
				ugcValidateDialogTreePrompt( data, component, &component->dialogBlock.initialPrompt, contact, contact == NULL, &hasSuccess );

				for( promptIt = 0; promptIt != eaSize( &component->dialogBlock.prompts ); ++promptIt ) {
					ugcValidateDialogTreePrompt( data, component, component->dialogBlock.prompts[ promptIt ], contact, false, &hasSuccess );
				}

				if( !hasSuccess || gUGCDebugFailAllValidation ) {
					ugcRaiseError( UGC_ERROR, ctx, "UGC.DialogTree_NoSuccess", "Dialog Tree needs one successful exit." );
				}
			}

			if( (component->bIsDefault || objective || ugcComponentStartWhenType(component) == UGCWHEN_OBJECTIVE_IN_PROGRESS || gUGCDebugFailAllValidation) && !contact ) {
				ugcRaiseError( UGC_ERROR, ctx, "UGC.Component_NotFound", "Could not find contact." );
			}
			if( objective && eaiFind( &component->eaObjectiveIDs, objective->id ) < 0 ) {
				ugcRaiseError( UGC_ERROR, ctx, "UGC.DialogTree_HasWrongDuration", "DialogTree has wrong duration." );
			}

			if(   ugcComponentStartWhenType( component ) == UGCWHEN_OBJECTIVE_START
				&& ea32Size( &component->eaObjectiveIDs ) && !ugcMissionFindTransitionForObjective( transitions, component->eaObjectiveIDs[ 0 ])) {
					ugcRaiseErrorInField( UGC_ERROR, ctx, "When.0.Type", "UGC.DialogTree_NotAtMapStart", "Map Start dialogs must be right after a map transition." );
			}
		}
	xcase UGC_COMPONENT_TYPE_KILL: {
			if(component->fsmProperties.pcFSMNameRef)
				ugcValidateComponentBehavior(data, ctx, component);

			if (map_is_ground)
				ugcValidateTagTypeInt(data, ctx, "ObjectID", component->iObjectLibraryId, "Encounter", false);
			else if (map_is_space)
				ugcValidateTagTypeInt(data, ctx, "ObjectID", component->iObjectLibraryId, "SpaceEncounter", false);

			ugcValidateKillActorDensity(data, ctx, component);
		}

	xcase UGC_COMPONENT_TYPE_ACTOR:
		if (map_is_ground)
			ugcValidateTagType(data, ctx, "CostumeName", component->pcCostumeName, "Costume", true);
		else if (map_is_space)
			ugcValidateTagType(data, ctx, "CostumeName", component->pcCostumeName, "SpaceCostume", true);
			
	xcase UGC_COMPONENT_TYPE_TRAP: {
			GroupDef *def = objectLibraryGetGroupDef(component->iObjectLibraryId, false);
			char power_group_name[256];
			if (def && ugcGetTagValue(def->tags, "type", SAFESTR(power_group_name)))
			{
				UGCTrapPowerGroup *power_group = RefSystem_ReferentFromString(UGC_DICTIONARY_TRAP_POWER_GROUP, power_group_name);
				if (power_group)
				{
					if (!component->pcTrapPower || eaFindString(&power_group->eaPowerNames, component->pcTrapPower) == -1 || gUGCDebugFailAllValidation)
					{
						ugcRaiseErrorInField(UGC_ERROR, ctx, "TrapPower", "UGC.Trap_InvalidPower", "Power %s does not exist in power group %s.", component->pcTrapPower ? component->pcTrapPower : "<NULL>", power_group_name);
					}
				}
				else
				{
					ugcRaiseErrorInternalCode(UGC_FATAL_ERROR, "Trap power group missing: %s", power_group_name);
				}
			}
			else
			{
				ugcRaiseErrorInternalCode(UGC_FATAL_ERROR, "Def missing or not tagged: %d", component->iObjectLibraryId);
			}
		}
	}

	// Validate restrictions
	if(   component->eType != UGC_COMPONENT_TYPE_TELEPORTER_PART
		  && component->eType != UGC_COMPONENT_TYPE_RESPAWN
		  && component->eType != UGC_COMPONENT_TYPE_REWARD_BOX
		  && component->iObjectLibraryId ) {
		const WorldUGCProperties* ugcProps = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", component->iObjectLibraryId );
		if (ugcProps) {
			WorldUGCRestrictionProperties restriction = { 0 };
			StructCopyAll( parse_WorldUGCRestrictionProperties, &ugcProps->restrictionProps, &restriction );
			ugcRestrictionsIntersect( &restriction, data->project->pRestrictionProperties );

			if( !ugcRestrictionsIsValid( &restriction ) || gUGCDebugFailAllValidation) {
				ugcRaiseError( UGC_ERROR, ctx, "UGC.Component_Unavailable", "Component does not satisfy project restrictions." );
			}
			StructReset( parse_WorldUGCRestrictionProperties, &restriction );
		}

		if( !ugcProps || gUGCDebugFailAllValidation) {
			char buffer[ 256 ];
			ugcComponentGetDisplayName( buffer, data, component, false );
			ugcRaiseErrorInField( UGC_ERROR, ctx, "ObjectID", "UGC.Component_RemovedFromUGC", "Component is no longer available in the Foundry. [%s]", buffer );
		}
	}

	// Validate a component is not used in two places
	if( component->eType != UGC_COMPONENT_TYPE_WHOLE_MAP && componentObjectiveCount > 1 ) {
		ugcRaiseError( UGC_ERROR, ctx, "UGC.Component_UsedMultipleTimes", "Component is used in multiple objectives and/or transitions." );
	}
	
	if( ugcDefaultsMissionReturnEnabled() && component->bInteractIsMissionReturn ) {
		// Validate MissionReturn components aren't used in any other way.
		if( componentObjectiveCountIncludingDialogTreeActors > 0 ) {
			ugcRaiseErrorInField( UGC_ERROR, ctx, "InteractIsMissionReturn", "UGC.Component_MissionReturn_UsedInMission", "MissionReturn Component is used in the mission as well." );
		}
		if( eaSize( &component->eaTriggerGroups )) {
			ugcRaiseErrorInField( UGC_ERROR, ctx, "InteractIsMissionReturn", "UGC.Component_MissionReturn_UsedInTriggers", "MissionReturn Component triggers other components" );
		}

		// Validate MissionReturn components are of the right type
		if(   component->eType != UGC_COMPONENT_TYPE_OBJECT && component->eType != UGC_COMPONENT_TYPE_FAKE_DOOR
			  && component->eType != UGC_COMPONENT_TYPE_ROOM_DOOR ) {
			ugcRaiseErrorInField( UGC_ERROR, ctx, "InteractIsMissionReturn", "UGC.Component_MissionReturn_OnWrongType", "INTERNAL ERROR: MissionReturn is on a component of the wrong type" );
		}
		if(  component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR ) {
			ugcRaiseErrorInField( UGC_ERROR, ctx, "InteractIsMissionReturn", "UGC.Component_MissionReturn_OnOpenableDoor", "MissionReturn Component is on an openable door.  This leads to ambiguous decisions." );
		}
	}

	// Validate whens
	{
		bool isDialogTree = (component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE);
		bool isRewardBox = (component->eType == UGC_COMPONENT_TYPE_REWARD_BOX);
		if( !isRewardBox && (!isDialogTree || component->sPlacement.pcMapName) ) {
			if (!component->pStartWhen)
				ugcRaiseErrorInternalCode(UGC_FATAL_ERROR, "Bad component start when");
			else
			{
				ugcValidateUGCWhen(data, component->pStartWhen, rel_objective, componentObjectiveCountIncludingDialogTreeActors, component->sPlacement.pcMapName, true, !isDialogTree, ctx, 0);
			}
			if (component->pHideWhen)
			{
				ugcValidateUGCWhen(data, component->pHideWhen, rel_objective, componentObjectiveCountIncludingDialogTreeActors, component->sPlacement.pcMapName, false, !isDialogTree, ctx, 1);
			}
		}
	}

	// Validate patrols
	if (eaiSize(&component->eaPatrolPoints) > 0)
	{
		int point_idx;
		UGCComponentPatrolPath *path = ugcComponentGetPatrolPath(data, component, NULL);

		for (point_idx = 0; point_idx < eaSize(&path->points); point_idx++)
		{
			if (path->points[point_idx]->prevConnectionInvalid
				|| path->points[point_idx]->nextConnectionInvalid
				|| gUGCDebugFailAllValidation)
			{
				ugcRaiseError( UGC_ERROR, ctx, "UGC.Component_PatrolIsInvalid", "Invalid patrol: No door found between rooms." );
				break;
			}
		}
		StructDestroy(parse_UGCComponentPatrolPath, path);
	}

	StructDestroy( parse_UGCRuntimeErrorContext, ctx );
}


/// Raise a UGC error if the resource RES-NAME in DICT is not valid
/// for UGC.
void ugcValidateResource( UGCRuntimeErrorContext* context, const char* fieldName,
	const char* dict, const char* resName )
{
	ResourceInfo* resInfo = ugcResourceGetInfo( dict, resName );

	if( !resInfo || gUGCDebugFailAllValidation ) {
		ugcRaiseErrorInField( UGC_ERROR, context, fieldName, "UGC.Resource_NotFound", "%s %s does not exist", dict, resName );
		return;
	}
}

void ugcValidateItem( UGCRuntimeErrorContext* context, const char* fieldName, const UGCProjectData* ugcProj, const char* itemName)
{
	FOR_EACH_IN_EARRAY(ugcProj->items, UGCItem, item)
	{
		if (resNamespaceBaseNameEq(item->astrName, itemName) && !gUGCDebugFailAllValidation)
			return;
	}
	FOR_EACH_END;

	ugcRaiseErrorInField( UGC_ERROR, context, fieldName, "UGC.Resource_NotFound", "Item %s does not exist", itemName );
}

static void ugcValidateInteractProperties(const UGCProjectData *ugcProj, UGCRuntimeErrorContext* ctx, UGCInteractProperties *interactProps)
{
	ugcValidateDisplayString( ctx, "InteractText", interactProps->pcInteractText, false, false );
	ugcValidateDisplayString( ctx, "InteractFailureText", interactProps->pcInteractFailureText, false, false );
	if( IS_HANDLE_ACTIVE( interactProps->hInteractAnim )) {
		ugcValidateResource( ctx, "InteractAnim", "AIAnimList", REF_STRING_FROM_HANDLE( interactProps->hInteractAnim ));
	}

	if(interactProps->succeedCheckedAttrib)
	{
		if( !nullStr( interactProps->succeedCheckedAttrib->astrItemName )) {
			ugcValidateItem( ctx, "RequiredItem", ugcProj, interactProps->succeedCheckedAttrib->astrItemName );
		}
		if( !nullStr( interactProps->succeedCheckedAttrib->astrSkillName )) {
			ugcRaiseError(UGC_ERROR, ctx, "UGC.Objective_CheckedAttribNotAllowed", "Interacts can not use checked attribs.");
		}
	}

	if (interactProps->pcDropItemName)
	{
		ugcValidateItem(ctx, "DropItem", ugcProj, interactProps->pcDropItemName);
	}
}

static bool ugcAnyObjectiveInternalMap( UGCMissionObjective** ugcObjectives )
{
	int it;
	for( it = 0; it != eaSize( &ugcObjectives ); ++it ) {
		UGCMissionObjective* objective = ugcObjectives[ it ];

		if( objective->strComponentInternalMapName ) {
			return true;
		}
		if( ugcAnyObjectiveInternalMap( objective->eaChildren )) {
			return true;
		}
	}

	return false;
}

static void ugcValidateMissionObjectives( UGCProjectData* ugcProj, UGCMissionObjective** ugcObjectives, bool inCompleteAll )
{
	int it;
	for( it = 0; it != eaSize( &ugcObjectives ); ++it ) {
		UGCMissionObjective* ugcObjective = ugcObjectives[ it ];
		UGCRuntimeErrorContext* ctx = ugcMakeTempErrorContextObjective( ugcMissionObjectiveLogicalNameTemp( ugcObjective ), ugcProj->mission->name );
		UGCComponent* component;

		if( ugcObjective->type == UGCOBJ_COMPLETE_COMPONENT || ugcObjective->type == UGCOBJ_UNLOCK_DOOR ) {
			component = ugcComponentFindByID( ugcProj->components, ugcObjective->componentID );
		} else {
			component = NULL;
		}

		if(   ugcObjective->type != UGCOBJ_ALL_OF && ugcObjective->type != UGCOBJ_IN_ORDER
			  && (!component || component->eType != UGC_COMPONENT_TYPE_WHOLE_MAP) ) {
			ugcValidateDisplayString( ctx, "UIString", ugcObjective->uiString, true, false );
		}

		switch( ugcObjective->type ) {
			case UGCOBJ_COMPLETE_COMPONENT: case UGCOBJ_UNLOCK_DOOR: {
				if( nullStr( ugcObjective->strComponentInternalMapName ) && ugcObjective->waypointMode == UGC_WAYPOINT_AREA ) {
					ugcRaiseErrorInField( UGC_ERROR, ctx, "WaypointMode", "UGC.InternalError", "Internal Error -- Waypoint mode can only be \"Points\" or \"None\" for Cryptic maps." );
				}

				if(   !component || (component->sPlacement.bIsExternalPlacement
									 && component->sPlacement.uRoomID == GENESIS_UNPLACED_ID) ) {
					ugcRaiseErrorInField( UGC_ERROR, ctx, "ComponentID", "UGC.Component_NotFound", "No associated component" );
				} else {
					int componentIt;
					for( componentIt = 0; componentIt != ea32Size( &ugcObjective->extraComponentIDs ); ++componentIt ) {
						UGCComponent* extraComponent = ugcComponentFindByID( ugcProj->components, ugcObjective->extraComponentIDs[ componentIt ] );
						UGCRuntimeErrorContext* componentCtx = ugcMakeErrorContextChallenge( ugcComponentGetLogicalNameTemp( extraComponent ), NULL, NULL );

						if(   !extraComponent || gUGCDebugFailAllValidation
							  || (extraComponent->sPlacement.bIsExternalPlacement && extraComponent->sPlacement.uRoomID == GENESIS_UNPLACED_ID) ) {
							ugcRaiseErrorInField( UGC_ERROR, ctx, "ExtraComponentID", "UGC.Component_NotFound", "No associated component" );
						} else {
							if( component->sPlacement.bIsExternalPlacement ) {
								if(   !extraComponent->sPlacement.bIsExternalPlacement
									  || stricmp_safe( component->sPlacement.pcExternalMapName,
													   extraComponent->sPlacement.pcExternalMapName ) != 0 ) {
									ugcRaiseErrorInField( UGC_ERROR, ctx, "ExtraComponentID", "UGC.Component_NotOnSameMap", "All objective components must be on the same map." );
								}
							} else {
								if(  extraComponent->sPlacement.bIsExternalPlacement
									 || stricmp_safe( component->sPlacement.pcMapName,
													  extraComponent->sPlacement.pcMapName ) != 0 ) {
									ugcRaiseErrorInField( UGC_ERROR, ctx, "ExtraComponentID", "UGC.Component_NotOnSameMap", "All objective components must be on the same map." );
								}
							}

							if(   extraComponent->eType == UGC_COMPONENT_TYPE_KILL && extraComponent->iObjectLibraryId &&
								  !ugcProjectFilterAllegiance( ugcProj, "Enemy", extraComponent->iObjectLibraryId )) {
								ugcRaiseErrorInField( UGC_ERROR, componentCtx, "ObjectID", "UGC.Component_NotEnemy", "Component selected for a KILL task is not of enemy allegiance" );
							}
						}

						StructDestroy( parse_UGCRuntimeErrorContext, componentCtx );
					}

					
					if (component->eType == UGC_COMPONENT_TYPE_ROOM_MARKER || component->eType == UGC_COMPONENT_TYPE_PLANET) {
						ugcValidateDisplayString( ctx, "SuccessFloaterText", ugcObjective->successFloaterText, false, false );
					}
					if(   component->eType == UGC_COMPONENT_TYPE_OBJECT || component->eType == UGC_COMPONENT_TYPE_BUILDING_DEPRECATED
						  || component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR || component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR
						  || component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
						ugcValidateInteractProperties(ugcProj, ctx, &ugcObjective->sInteractProps);
					}

					if( component->eType == UGC_COMPONENT_TYPE_ROOM_MARKER && ugcObjective->waypointMode == UGC_WAYPOINT_POINTS ) {
						ugcRaiseErrorInField( UGC_ERROR, ctx, "WaypointMode", "UGC.InternalError", "Internal Error -- Waypoint mode can only be \"Volume\" or \"None\" for reach location objectives." );
					}

					if (component->eType == UGC_COMPONENT_TYPE_KILL)
						// Check that component type is actually an enemy
					{
						if( component->iObjectLibraryId &&
							!ugcProjectFilterAllegiance( ugcProj, "Enemy", component->iObjectLibraryId ))
						{
							UGCRuntimeErrorContext* componentCtx = ugcMakeErrorContextChallenge( ugcComponentGetLogicalNameTemp( component ), NULL, NULL );
							ugcRaiseErrorInField( UGC_ERROR, componentCtx, "ObjectID", "UGC.Component_NotEnemy", "Component selected for a KILL task is not of enemy allegiance" );
							StructDestroy( parse_UGCRuntimeErrorContext, componentCtx );
						}
					}

					// Validate animation region
					if( component->eType == UGC_COMPONENT_TYPE_OBJECT && ugcIsSpaceEnabled() ) {
						ResourceInfo* animListInfo = ugcResourceGetInfo( "AIAnimList", REF_STRING_FROM_HANDLE( ugcObjective->sInteractProps.hInteractAnim ));
						if( animListInfo ) {
							switch( ugcComponentMapType( ugcProj, component )) {
								case UGC_MAP_TYPE_INTERIOR: case UGC_MAP_TYPE_PREFAB_INTERIOR:
								case UGC_MAP_TYPE_GROUND: case UGC_MAP_TYPE_PREFAB_GROUND:
									if( !ugcHasTag( animListInfo->resourceTags, "Region_Ground" ) || gUGCDebugFailAllValidation) {
										ugcRaiseErrorInField( UGC_ERROR, ctx, "InteractAnim", "UGC.Anim_WrongRegionType", "Animation is not valid for this region." );
									}

								xcase UGC_MAP_TYPE_SPACE: case UGC_MAP_TYPE_PREFAB_SPACE:
									if( !ugcHasTag( animListInfo->resourceTags, "Region_Space" ) || gUGCDebugFailAllValidation) {
										ugcRaiseErrorInField( UGC_ERROR, ctx, "InteractAnim", "UGC.Anim_WrongRegionType", "Animation is not valid for this region." );
									}
							}
						}
					}
				}
			}
			xcase UGCOBJ_ALL_OF: case UGCOBJ_IN_ORDER: {
				if( ugcObjective->type == UGCOBJ_ALL_OF && ugcAnyObjectiveInternalMap( ugcObjective->eaChildren ) && !ugcObjectiveInternalMapName( ugcProj, ugcObjective )) {
					ugcRaiseError( UGC_ERROR, ctx, "UGC.Maps_Under_AllOf", "Maps not allowed inside Complete All" );
				}

				if( ugcObjective->waypointMode ) {
					ugcRaiseErrorInField( UGC_ERROR, ctx, "WaypointMode", "UGC.InternalError", "Internal Error -- Waypoint mode is set for an ALL_OF / IN_ORDER objective" );
				}
				
				if( !eaSize( &ugcObjective->eaChildren ) || eaFind( &ugcObjective->eaChildren, NULL ) >= 0 || gUGCDebugFailAllValidation ) {
					ugcRaiseError( UGC_ERROR, ctx, "UGC.InternalError", "Internal Error -- No children" );
				} else {
					ugcValidateMissionObjectives( ugcProj, ugcObjective->eaChildren, true );
				}
			}
		}
	}
}

void ugcValidateSeries( const UGCProjectSeries* ugcSeries )
{
	UGCProjectSeriesVersion* version = eaTail( &ugcSeries->eaVersions );
	UGCRuntimeErrorContext* ctx = ugcMakeErrorContextDefault();
	char* estrError = NULL;
	
	ugcLoadStart_printf( "Series validate..." );

	estrCreate( &estrError );
	if( !UGCProject_ValidatePotentialName( version->strName, true, &estrError )) {
		ugcRaiseErrorInField( UGC_ERROR, ctx, "strName", estrError, "Project has invalid name" );
	}
	estrDestroy( &estrError );
	ugcValidateDisplayString( ctx, "strDescription", version->strDescription, true, false );

	ugcValidateResource( ctx, "strImage", "Texture", version->strImage );

	{
		int it;
		for( it = 0; it != eaSize( &version->eaChildNodes ); ++it ) {
			ugcValidateSeriesNode( ugcSeries, version->eaChildNodes[ it ]);
		}
	}
	
	ugcLoadEnd_printf( "done" );
}

void ugcValidateSeriesNode( const UGCProjectSeries* ugcSeries, const UGCProjectSeriesNode* seriesNode )
{
	char buffer[ 256 ];
	UGCRuntimeErrorContext* ctx;

	sprintf( buffer, "SeriesNode_%d", seriesNode->iNodeID );
	ctx = ugcMakeErrorContextEpisodePart( buffer );
	
	if( !seriesNode->iProjectID ) {
		ugcValidateDisplayString( ctx, "strName", seriesNode->strName, true, false );
		ugcValidateDisplayString( ctx, "strDescription", seriesNode->strDescription, true, false );

		{
			int it;
			for( it = 0; it != eaSize( &seriesNode->eaChildNodes ); ++it ) {
				ugcValidateSeriesNode( ugcSeries, seriesNode->eaChildNodes[ it ]);
			}
		}
	}

	StructDestroySafe( parse_UGCRuntimeErrorContext, &ctx );
}

bool ugcValidateErrorfIfStatusHasErrors( UGCRuntimeStatus* status )
{
	UGCRuntimeError* error = ugcStatusMostImportantError( status );
	
	if( error ) {
		char* estr = NULL;
		ParserWriteText( &estr, parse_UGCRuntimeError, (UGCRuntimeStatus*)error, 0, 0, 0 );
		Errorf( "UGC Error: %s", estr );
		estrDestroy( &estr );

		return true;
	} else {
		return false;
	}
}
