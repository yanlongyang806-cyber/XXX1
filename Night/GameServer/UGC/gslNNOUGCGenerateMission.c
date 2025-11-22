#include "gslNNOUGCGenesisMissions.h"

#include "Expression.h"
#include "RegionRules.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "NNOUGCCommon.h"
#include "NNOUGCInteriorCommon.h"
#include "UGCInteriorCommon.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCResource.h"
#include "contact_common.h"
#include "gslNNOUGCGenesisStructs.h"
#include "mission_common.h"
#include "rewardCommon.h"
#include "wlUGC.h"
#include "WorldLib.h"
#include "WorldGrid.h"
#include "WorldLibEnums.h"
#include "gslNNOUGCGenesisStructs.h"
#include "UGCError.h"
#include "error.h"

#include "contact_common.h"

bool ugcVisibleFSMObjectives = false;
AUTO_CMD_INT(ugcVisibleFSMObjectives, ugcVisibleFSMObjectives);

static void ugcComponentSetClickyAndCheckedAttrib( UGCGenesisMissionChallengeClickie** out_ppClickie, UGCCheckedAttrib** out_ppCheckedAttrib, UGCProjectData* ugcProj, const UGCComponent* component, const UGCInteractProperties* props );

////////////////////////////////////////////////////////////
// Utility Functions
////////////////////////////////////////////////////////////

static void ugcMissionFSMObjectiveFixup( UGCGenesisMissionObjective* objective )
{
	if( ugcVisibleFSMObjectives ) {
		int it;
		StructCopyString( &objective->pcShortText, objective->pcName );

		for( it = 0; it != eaSize( &objective->eaChildren ); ++it ) {
			ugcMissionFSMObjectiveFixup( objective->eaChildren[ it ]);
		}
	}
}

static const char* ugcComponentInteractionDef( UGCProjectData* proj, const UGCComponent* component, UGCInteractDuration duration, bool hideAfterInteract )
{
	bool isSpaceMap;
	
	if( component->sPlacement.bIsExternalPlacement ) {
		ZoneMapInfo* info = NULL;
		WorldRegion* region = NULL;
		RegionRules* rr = NULL;

		info = worldGetZoneMapByPublicName( component->sPlacement.pcExternalMapName );
		if( info ) {
			WorldRegion** regions = zmapInfoGetWorldRegions( info );
			region = eaGet( &regions, 0 );
		}

		if( region ) {
			rr = getRegionRulesFromRegion( region );
		}

		if( rr ) {
			isSpaceMap = rr->bSpaceFlight;
		} else {
			isSpaceMap = false;
		}
	} else {
		UGCMap* map = ugcMapFindByName( proj, component->sPlacement.pcMapName );
		UGCMapType mapType = ugcMapGetType( map );

		isSpaceMap = (mapType == UGC_MAP_TYPE_SPACE || mapType == UGC_MAP_TYPE_PREFAB_SPACE);
	}
	
	switch( duration ) {
		case UGCDURATION_INSTANT:
			if( isSpaceMap ) {
				if( component->sPlacement.bIsExternalPlacement ) {
					return "Space_Node_Instant_NoCooldown";
				} else if( hideAfterInteract ) {
					return "Space_Node_Instant_NoRespawn";
				} else {
					return "Space_Node_Instant";
				}
			} else {
				if( component->sPlacement.bIsExternalPlacement ) {
					return "Ground_Node_Instant_NoCooldown";
				} else if( hideAfterInteract ) {
					return "Ground_Node_Instant_NoRespawn";
				} else {
					return "Ground_Node_Instant";
				}
			}
		
		case UGCDURATION_SHORT:
			if( isSpaceMap ) {
				if( component->sPlacement.bIsExternalPlacement ) {
					return "Space_Node_Interrupt_Short_NoCooldown";
				} else if( hideAfterInteract ) {
					return "Space_Node_Interrupt_Short_NoRespawn";
				} else {
					return "Space_Node_Interrupt_Short";
				}
			} else {
				if( component->sPlacement.bIsExternalPlacement ) {
					return "Ground_Node_Interrupt_Short_NoCooldown";
				} else if( hideAfterInteract ) {
					return "Ground_Node_Interrupt_Short_NoRespawn";
				} else {
					return "Ground_Node_Interrupt_Short";
				}
			}
			
		xcase UGCDURATION_LONG:
			if( isSpaceMap ) {
				if( component->sPlacement.bIsExternalPlacement ) {
					return "Space_Node_Interrupt_Long_NoCooldown";
				} else if( hideAfterInteract ) {
					return "Space_Node_Interrupt_Long_NoRespawn";
				} else {
					return "Space_Node_Interrupt_Long";
				}
			} else {
				if( component->sPlacement.bIsExternalPlacement ) {
					return "Ground_Node_Interrupt_Long_NoCooldown";
				} else if( hideAfterInteract ) {
					return "Ground_Node_Interrupt_Long_NoRespawn";
				} else {
					return "Ground_Node_Interrupt_Long";
				}
			}

		xcase UGCDURATION_MEDIUM: default:
			if( isSpaceMap ) {
				if( component->sPlacement.bIsExternalPlacement ) {
					return "Space_Node_Interrupt_Med_NoCooldown";
				} else if( hideAfterInteract ) {
					return "Space_Node_Interrupt_Med_NoRespawn";
				} else {
					return "Space_Node_Interrupt_Med";
				}
			} else {
				if(  component->sPlacement.bIsExternalPlacement ) {
					return "Ground_Node_Interrupt_Med_NoCooldown";
				} else if( hideAfterInteract ) {
					return "Ground_Node_Interrupt_Med_NoRespawn";
				} else {
					return "Ground_Node_Interrupt_Med";
				}
			}
	}
}

static void ugcDialogCompleteExternalPromptNames( UGCGenesisWhenExternalPrompt*** out_eaExternalPrompts, UGCProjectData* ugcProj, const UGCComponent* externalContact, const UGCComponent* dialog )
{
	const char* externalMapName = externalContact->sPlacement.pcExternalMapName;
	const char* externalObjectName = externalContact->sPlacement.pcExternalObjectName;
	char nsPrefix[RESOURCE_NAME_MAX_SIZE] = { 0 };
	ZoneMapEncounterObjectInfo* contactZeni = zeniObjectFind( externalMapName, externalObjectName );
	assert( externalContact->sPlacement.bIsExternalPlacement );
	assert( dialog->eType == UGC_COMPONENT_TYPE_DIALOG_TREE );

	if (ugcProj->ns_name)
		sprintf(nsPrefix, "%s:", ugcProj->ns_name);

	if( !contactZeni || nullStr( contactZeni->ugcContactName )) {
		ugcRaiseErrorInternalCode( UGC_ERROR, "Internal Error - Contact %s on map %s does not have a ugcContactName set.",
								   externalObjectName, externalMapName );
	} else {
		UGCComponent** dialogTrees = ugcComponentFindPopupPromptsForWhenInDialog( ugcProj->components, dialog );
		int blockIt;
		int promptIt;

		for( blockIt = 0; blockIt != eaSize( &dialogTrees ); ++blockIt ) {
			for( promptIt = -1; promptIt != eaSize( &dialogTrees[ blockIt ]->dialogBlock.prompts ); ++promptIt ) {
				UGCDialogTreePrompt* prompt = (promptIt == -1 ? &dialogTrees[ blockIt ]->dialogBlock.initialPrompt : dialogTrees[ blockIt ]->dialogBlock.prompts[ promptIt ]);
				bool isCompletePrompt = false;
				int actionIt;
				for( actionIt = 0; actionIt != eaSize( &prompt->eaActions ); ++actionIt ) {
					if( !prompt->eaActions[ actionIt ]->nextPromptID && !prompt->eaActions[ actionIt ]->bDismissAction ) {
						isCompletePrompt = true;
						break;
					}
				}

				if( isCompletePrompt ) {
					UGCGenesisWhenExternalPrompt* externalPrompt = StructCreate( parse_UGCGenesisWhenExternalPrompt );
					char promptNameBuffer[ 512 ];

					if( blockIt == 0 && promptIt == -1 ) {
						sprintf( promptNameBuffer, "%s%s/Prompt_%d", nsPrefix, ugcProj->project_prefix, dialog->uID );
					} else if( promptIt >= 0 ) {
						sprintf( promptNameBuffer, "%s%s/Prompt_%d_Block%d_%d", nsPrefix, ugcProj->project_prefix, dialog->uID, blockIt, prompt->uid );
					} else {
						sprintf( promptNameBuffer, "%s%s/Prompt_%d_Block%d", nsPrefix, ugcProj->project_prefix, dialog->uID, blockIt );
					}

					externalPrompt->pcContactName = StructAllocString( contactZeni->ugcContactName );
					externalPrompt->pcPromptName = StructAllocString( promptNameBuffer );
					externalPrompt->pcEncounterName = StructAllocString( externalObjectName );
					externalPrompt->pcEncounterMapName = StructAllocString( externalMapName );

					eaPush( out_eaExternalPrompts, externalPrompt );
				}
			}
		}

		eaDestroy( &dialogTrees );
	}
}

////////////////////////////////////////////////////////////
// Mission Objectives
////////////////////////////////////////////////////////////

static bool ugcMissionObjectiveWhenAccum( UGCProjectData* ugcProj, UGCGenesisWhen* accum, const UGCComponent* component, const UGCMissionObjective* objective, const char* astrTargetMapName )
{
	UGCGenesisWhenType prevType = accum->type;

	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_OBJECT: case UGC_COMPONENT_TYPE_EXTERNAL_DOOR: case UGC_COMPONENT_TYPE_CLUSTER_PART: {
			if( !astrTargetMapName ) {
				UGCGenesisWhenExternalChallenge* externalChallenge = StructCreate( parse_UGCGenesisWhenExternalChallenge );

				accum->type = UGCGenesisWhen_ExternalChallengeComplete;
				externalChallenge->pcMapName = StructAllocString( component->sPlacement.pcExternalMapName );
				externalChallenge->pcName = StructAllocString( component->sPlacement.pcExternalObjectName );
				externalChallenge->eType = GenesisChallenge_Clickie;

				ugcComponentSetClickyAndCheckedAttrib( &externalChallenge->pClickie, &externalChallenge->succeedCheckedAttrib, ugcProj, component, &objective->sInteractProps );
				eaPush( &accum->eaExternalChallenges, externalChallenge );
			} else {
				accum->type = UGCGenesisWhen_ChallengeComplete;
				eaPush( &accum->eaChallengeNames, StructAllocString( ugcComponentGetLogicalNameTemp( component ) ) );
			}
		}

		xcase UGC_COMPONENT_TYPE_DESTRUCTIBLE: {
			if( !astrTargetMapName ) {
				// Not implemented
			} else {
				accum->type = UGCGenesisWhen_ChallengeComplete;
				eaPush( &accum->eaChallengeNames, StructAllocString( ugcComponentGetLogicalNameTemp( component ) ) );
			}
		}

		xcase UGC_COMPONENT_TYPE_BUILDING_DEPRECATED: {
			if( !astrTargetMapName ) {
				// Do nothing - should never get here
			} else {
				accum->type = UGCGenesisWhen_ChallengeComplete;
				eaPush( &accum->eaChallengeNames, StructAllocString( ugcComponentGetLogicalNameTemp( component ) ) );
			}
		}

		xcase UGC_COMPONENT_TYPE_ROOM_DOOR:
		acase UGC_COMPONENT_TYPE_FAKE_DOOR: {
			// geometry must be specified for this door to work!
			if( !component->iObjectLibraryId ) {
				return false;
			}
			
			if( !astrTargetMapName ) {
				// Do nothing - should never get here
			} else {
				accum->type = UGCGenesisWhen_ChallengeComplete;
				eaPush( &accum->eaChallengeNames, StructAllocString( ugcComponentGetLogicalNameTemp( component ) ) );
			}
		}

		xcase UGC_COMPONENT_TYPE_DIALOG_TREE: {
			UGCComponent* contact = ugcComponentFindByID( ugcProj->components, component->uActorID ); 
			char promptName[ 256 ];
			sprintf( promptName, "Prompt_%d", component->uID );

			if( !contact ) {
				// ugcGenesisRaiseError(); //< MJF TODO fix this
				break;
			}
			if( !ugcComponentIsOnMap( contact, astrTargetMapName, false )) {
				break;
			}

			if( !astrTargetMapName ) {
				if( contact->eType == UGC_COMPONENT_TYPE_CONTACT ) {
					accum->type = UGCGenesisWhen_ExternalPromptComplete;
					ugcDialogCompleteExternalPromptNames( &accum->eaExternalPrompts, ugcProj, contact, component );
					if( !accum->eaExternalPrompts ) {
						accum->type = UGCGenesisWhen_Manual;
					}
				} else {
					accum->type = UGCGenesisWhen_PromptComplete;
					eaPush( &accum->eaPromptNames, StructAllocString( promptName ));
					accum->pcPromptChallengeName = StructAllocString( contact->sPlacement.pcExternalObjectName );
					accum->pcPromptMapName = StructAllocString( contact->sPlacement.pcExternalMapName );
				}
			} else {
				accum->type = UGCGenesisWhen_PromptComplete;
				eaPush( &accum->eaPromptNames, StructAllocString( promptName ));
				accum->pcPromptChallengeName = StructAllocString( ugcComponentGetLogicalNameTemp( contact ));
			}
		}

		xcase UGC_COMPONENT_TYPE_ROOM_MARKER: {
			if( !astrTargetMapName ) {
				UGCGenesisWhenExternalRoom* externalWhenRoom = StructCreate( parse_UGCGenesisWhenExternalRoom );
				accum->type = UGCGenesisWhen_ExternalRoomEntry;
				externalWhenRoom->pcMapName = StructAllocString( component->sPlacement.pcExternalMapName );
				externalWhenRoom->pcName = StructAllocString( component->sPlacement.pcExternalObjectName );
				eaPush( &accum->eaExternalRooms, externalWhenRoom );
			} else {
				if (component->bIsRoomVolume)
				{
					UGCGenesisWhenRoom* whenRoom = StructCreate( parse_UGCGenesisWhenRoom );
					accum->type = UGCGenesisWhen_RoomEntry;
					whenRoom->roomName = StructAllocString( ugcIntLayoutGetRoomName(component->sPlacement.uRoomID) );
					whenRoom->layoutName = StructAllocString( GENESIS_UGC_LAYOUT_NAME );
					eaPush( &accum->eaRooms, whenRoom );
				}
				else
				{
					accum->type = UGCGenesisWhen_ReachChallenge;
					eaPush( &accum->eaChallengeNames, StructAllocString( ugcComponentGetLogicalNameTemp(component) ) );
				}
			}
		}

		xcase UGC_COMPONENT_TYPE_PLANET: {
			if( !astrTargetMapName ) {
				// Do nothing - should never get here
			} else {
				accum->type = UGCGenesisWhen_ReachChallenge;
				eaPush( &accum->eaChallengeNames, StructAllocString( ugcComponentGetLogicalNameTemp(component) ) );
			}
		}

		xcase UGC_COMPONENT_TYPE_KILL: {
			if( !astrTargetMapName ) {
				UGCGenesisWhenExternalChallenge* externalChallenge = StructCreate( parse_UGCGenesisWhenExternalChallenge );
				accum->type = UGCGenesisWhen_ExternalChallengeComplete;
				externalChallenge->pcMapName = StructAllocString( component->sPlacement.pcExternalMapName );
				externalChallenge->pcName = StructAllocString( component->sPlacement.pcExternalObjectName );
				externalChallenge->eType = GenesisChallenge_Encounter2;
				eaPush( &accum->eaExternalChallenges, externalChallenge );
			} else {
				accum->type = UGCGenesisWhen_ChallengeComplete;
				eaPush( &accum->eaChallengeNames, StructAllocString( ugcComponentGetLogicalNameTemp(component) ) );
			}
		}

		xcase UGC_COMPONENT_TYPE_WHOLE_MAP: {
			if( !astrTargetMapName ) {
				accum->type = UGCGenesisWhen_ExternalMapStart;
				accum->bAnyCrypticMap = true;
			} else {
				accum->type = UGCGenesisWhen_MapStart;
			}
		}
	}

	devassert( accum->type == prevType || prevType == 0 );
	if( accum->type == UGCGenesisWhen_MapStart && component->eType != UGC_COMPONENT_TYPE_WHOLE_MAP )
		return false;
	else
		return true;
}

static void ugcMissionGenerateExtraObjectiveWaypoint(UGCGenesisMissionObjective* objective, const char *mapName, const char *objectName)
{
	MissionWaypoint *waypoint = StructCreate(parse_MissionWaypoint);
	waypoint->type = MissionWaypointType_Clicky;
	waypoint->mapName = allocAddString(mapName);
	waypoint->name = StructAllocString(objectName);
	eaPush(&objective->eaExtraWaypoints, waypoint);
}

static void ugcMissionGenerateObjectives( UGCProjectData* ugcProj, const UGCMissionObjective** objectives, const char* astrTargetMapName, UGCGenesisMissionObjective*** out_peaGenObjectives, UGCGenesisMissionPrompt*** out_peaGenPrompts )
{
	UGCPerProjectDefaults *defaults = ugcGetDefaults();
	int it;
	char nsPrefix[ RESOURCE_NAME_MAX_SIZE ] = { 0 };

	if( !nullStr(ugcProj->ns_name) ) {
		sprintf(nsPrefix, "%s:", ugcProj->ns_name);
	}

	for( it = 0; it != eaSize( &objectives ); ++it ) {
		const UGCMissionObjective* objective = objectives[ it ];
		const UGCMissionObjective* prev_objective = eaGet(&objectives, it-1);
		
		UGCGenesisMissionObjective* objectiveAccum = StructCreate( parse_UGCGenesisMissionObjective );
		char objectiveName[ 256 ];
		bool isError = false;

		strcpy( objectiveName, ugcMissionObjectiveLogicalNameTemp( objective ));
		objectiveAccum->pcName = StructAllocString( objectiveName );
		objectiveAccum->pcShortText = ugcAllocSMFString( ugcMissionObjectiveUIString( objective ), false);
		objectiveAccum->pcSuccessFloaterText = ugcAllocSMFString( objective->successFloaterText, false );

		switch( objective->type ) {
			xcase UGCOBJ_TMOG_MAP_MISSION: {
				char mapName[ RESOURCE_NAME_MAX_SIZE ];
				UGCMissionMapLink* mapLink;
				sprintf( mapName, "%s%s", nsPrefix, objective->astrMapName );
				mapLink = ugcMissionFindLinkByExitMap( ugcProj, mapName );

				objectiveAccum->succeedWhen.type = UGCGenesisWhen_ExternalOpenMissionComplete;
				objectiveAccum->eWaypointMode = UGCGenesisMissionWaypointMode_Points;
				eaPush( &objectiveAccum->succeedWhen.eaExternalMissionNames,
						ugcGenesisMissionNameRaw( mapName, g_UGCMissionName, true ));
				eaPush( &objectiveAccum->succeedWhen.eaExternalMapNames, StructAllocString( mapName ));

				// [NNO-19925] If we don't go through the overworld
				// map, alternate map must be filled in with the
				// target Cryptic map.  Otherwise, waypoints won't
				// always show up.
				if( mapLink && !mapLink->bDoorUsesMapLocation ) {
					UGCMissionMapLink* crypticMapLink = ugcMissionFindCrypticSourceLink( ugcProj, mapLink );
					UGCComponent* crypticDoor = ugcComponentFindByID( ugcProj->components, crypticMapLink->uDoorComponentID );
					if( !crypticDoor || !crypticDoor->sPlacement.bIsExternalPlacement ) {
						ugcRaiseErrorInternalCode(UGC_ERROR, "Internal Error - Map does not have a Cryptic source door.");
					} else {
						objectiveAccum->succeedWhen.astrExternalAlternateMapForWaypoint = allocAddString( crypticDoor->sPlacement.pcExternalMapName );
					}
				}
			}

			xcase UGCOBJ_TMOG_REACH_INTERNAL_MAP: {
				char mapName[ RESOURCE_NAME_MAX_SIZE ];
				UGCMissionMapLink* mapLink;
				sprintf( mapName, "%s%s", nsPrefix, objective->astrMapName );
				mapLink = ugcMissionFindLinkByExitMap( ugcProj, mapName );

				objectiveAccum->succeedWhen.type = UGCGenesisWhen_ExternalMapStart;
				objectiveAccum->eWaypointMode = UGCGenesisMissionWaypointMode_Points;
				eaPush( &objectiveAccum->succeedWhen.eaExternalMapNames, StructAllocString( mapName ));

				// [NNO-19925] If we don't go through the overworld
				// map, alternate map must be filled in with the
				// target Cryptic map.  Otherwise, waypoints won't
				// always show up.
				if( mapLink && !mapLink->bDoorUsesMapLocation ) {
					UGCMissionMapLink* crypticMapLink = ugcMissionFindCrypticSourceLink( ugcProj, mapLink );
					UGCComponent* crypticDoor = ugcComponentFindByID( ugcProj->components, crypticMapLink->uDoorComponentID );
					if( !crypticDoor || !crypticDoor->sPlacement.bIsExternalPlacement ) {
						ugcRaiseErrorInternalCode(UGC_ERROR, "Internal Error - Map does not have a Cryptic source door.");
					} else {
						objectiveAccum->succeedWhen.astrExternalAlternateMapForWaypoint = allocAddString( crypticDoor->sPlacement.pcExternalMapName );
					}
				}
			}

			xcase UGCOBJ_COMPLETE_COMPONENT: case UGCOBJ_UNLOCK_DOOR: {
				UGCComponent* component = ugcComponentFindByID( ugcProj->components, objective->componentID );

				if( !component ) {
					break;
				}
				if( component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE && !ugcComponentIsOnMap( component, astrTargetMapName, false )) {
					break;
				}

				// If this is a "whole_map" component, then it is okay for its objective to be
				// invisible.  (Unlike other objectives which need some default text.)
				if( component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP && nullStr( objective->uiString )) {
					StructFreeStringSafe( &objectiveAccum->pcShortText );
				}

				if( component->eType != UGC_COMPONENT_TYPE_WHOLE_MAP ) {
					switch( objective->waypointMode ) {
						xcase UGC_WAYPOINT_NONE:
							objectiveAccum->eWaypointMode = UGCGenesisMissionWaypointMode_None;
						xcase UGC_WAYPOINT_AREA:
							if( nullStr( objective->strComponentInternalMapName )) {
								objectiveAccum->eWaypointMode = UGCGenesisMissionWaypointMode_None;
							} else {
								objectiveAccum->eWaypointMode = UGCGenesisMissionWaypointMode_AutogeneratedVolume;
							}
						xcase UGC_WAYPOINT_POINTS:
							objectiveAccum->eWaypointMode = UGCGenesisMissionWaypointMode_Points;
						xdefault:
							FatalErrorf( "ecase" );
					}
				}
				
				isError = !ugcMissionObjectiveWhenAccum( ugcProj, &objectiveAccum->succeedWhen, component, objective, astrTargetMapName );
				{
					int extraIt;
					for( extraIt = 0; extraIt != ea32Size( &objective->extraComponentIDs ); ++extraIt ) {
						UGCComponent* extraComponent = ugcComponentFindByID( ugcProj->components, objective->extraComponentIDs[ extraIt ] );

						if( !extraComponent || extraComponent->eType != component->eType ) {
							break;
						}
						if( extraComponent->eType != UGC_COMPONENT_TYPE_DIALOG_TREE && !ugcComponentIsOnMap( extraComponent, astrTargetMapName, false )) {
							break;
						}

						ugcMissionObjectiveWhenAccum( ugcProj, &objectiveAccum->succeedWhen, extraComponent, objective, astrTargetMapName );
					}
				}
			}
			
			xcase UGCOBJ_ALL_OF:
				objectiveAccum->succeedWhen.type = UGCGenesisWhen_AllOf;
				ugcMissionGenerateObjectives( ugcProj, objective->eaChildren, astrTargetMapName, &objectiveAccum->eaChildren, out_peaGenPrompts );

			xcase UGCOBJ_IN_ORDER:
				objectiveAccum->succeedWhen.type = UGCGenesisWhen_InOrder;
				ugcMissionGenerateObjectives( ugcProj, objective->eaChildren, astrTargetMapName, &objectiveAccum->eaChildren, out_peaGenPrompts );

			xdefault:
				devassertmsg( false, "Encountered an unexpected Persistent Mission type." );
		}

		// Handle hasn't been filled in yet, to prevent from generating a bad mission
		if( isError ) {
			UGCGenesisMissionPrompt* errPromptAccum = StructCreate( parse_UGCGenesisMissionPrompt );
			UGCGenesisMissionPromptAction* action = StructCreate( parse_UGCGenesisMissionPromptAction );
			WorldGameActionProperties* showPromptAction = StructCreate( parse_WorldGameActionProperties );
			char buffer[ 1024 ];
			

			sprintf( buffer, "ObjErr_%s", objectiveName );
			errPromptAccum->pcName = StructAllocString( buffer );
			errPromptAccum->showWhen.type = UGCGenesisWhen_Manual;

			sprintf( buffer, "<font style=OOC>Task Error</font><br><br>"
					 "This task (%s) could not be generated.  Click OK to skip this task.",
					 objectiveAccum->pcShortText );
			eaPush( &errPromptAccum->sPrimaryBlock.eaBodyText, StructAllocString( buffer ));
			eaPush( &errPromptAccum->sPrimaryBlock.eaActions, action );
			action->pcText = StructAllocString( "OK" );
			eaPush( out_peaGenPrompts, errPromptAccum );

			objectiveAccum->succeedWhen.type = UGCGenesisWhen_PromptComplete;
			eaPush( &objectiveAccum->succeedWhen.eaPromptNames, StructAllocString( errPromptAccum->pcName ));

			showPromptAction->eActionType = WorldGameActionType_Contact;
			showPromptAction->pContactProperties = StructCreate( parse_WorldContactActionProperties );
			{
				if( astrTargetMapName ) {
					char mapNameWithNS[ RESOURCE_NAME_MAX_SIZE ];
					sprintf( mapNameWithNS, "%s%s", nsPrefix, astrTargetMapName );
					SET_HANDLE_FROM_STRING( "ContactDef", ugcGenesisContactNameRaw( mapNameWithNS, g_UGCMissionName, NULL ),
										showPromptAction->pContactProperties->hContactDef );
				} else {
					SET_HANDLE_FROM_STRING( "ContactDef", ugcGenesisContactNameRaw( NULL, ugcProj->project_prefix, NULL ),
										showPromptAction->pContactProperties->hContactDef );
				}
			}
			showPromptAction->pContactProperties->pcDialogName = StructAllocString( errPromptAccum->pcName );
			
			eaPush( &objectiveAccum->eaOnStartActions, showPromptAction );
		}

		eaPush( out_peaGenObjectives, objectiveAccum );
	}
}

////////////////////////////////////////////////////////////
// Portal Generation
////////////////////////////////////////////////////////////

static char* ugcMissionPortalInteractExpressionTemp( UGCProjectData* ugcProj, const char* objectiveName )
{
	static char buffer[ 1024 ];

	if( !nullStr( objectiveName )) {
		sprintf( buffer, "TeamMissionStateInProgress(\"%s\")", objectiveName );
		if( !ugcDefaultsMapLinkSkipCompletedMaps() ) {
			strcatf( buffer, " or TeamMissionStateSucceeded(\"%s\")", objectiveName );
		}
	} else {
		// Null string means "Whole Mission Completed".
		//
		// This can't be properly checked on UGC missions (since they
		// do not go into the completedMissions list).  Instead
		// check if the mission is not in progress.
		char nsPrefix[ RESOURCE_NAME_MAX_SIZE ] = "";

		if( ugcProj->ns_name ) {
			sprintf( nsPrefix, "%s:", ugcProj->ns_name );
		}
		
		sprintf( buffer, "NOT TeamMissionStateInProgress(\"%s%s\")", nsPrefix, ugcProj->project_prefix );
	}
	
	return buffer;
}

static InteractableOverride* ugcMissionPortalInteractOverride( UGCProjectData* ugcProj, SA_PARAM_OP_STR const char* objectiveName, int missionNum,
	const char* fromMap, const char* fromDoor, 
	const char* text )
{
	if( !nullStr( fromMap ) ) {
		InteractableOverride* portalAccum = StructCreate( parse_InteractableOverride );
		WorldInteractionPropertyEntry* entry = StructCreate( parse_WorldInteractionPropertyEntry );
		char* baseInteractExpr = ugcMissionPortalInteractExpressionTemp( ugcProj, objectiveName );
		char* interactExpr = NULL;
		portalAccum->pPropertyEntry = entry;

		portalAccum->pcMapName = allocAddString( fromMap );
		portalAccum->pcInteractableName = allocAddString( fromDoor );

		entry->pTextProperties = StructCreate( parse_WorldTextInteractionProperties );
		{
			char* textSanitized = ugcAllocSMFString( text, false );
			entry->pTextProperties->interactOptionText.pEditorCopy = langCreateMessage( NULL, NULL, NULL, textSanitized );
			StructFreeStringSafe( &textSanitized );
		}
		entry->pTextProperties->interactOptionText.bEditorCopyIsServer = true;

		if( !nullStr( baseInteractExpr )) {
			estrConcatf( &interactExpr, "%s", baseInteractExpr );
		}

		if( missionNum >= 0 && resNamespaceIsUGC( fromMap )) {
			if( interactExpr ) {
				estrInsert( &interactExpr, 0, "(", 1 );
				estrConcatf( &interactExpr, ") and " );
			}
			estrConcatf( &interactExpr, "GetMapVariableInt(\"Mission_Num\") = %d", missionNum );
		}

		if( interactExpr ) {
			entry->bOverrideInteract = true;
			entry->pInteractCond = exprCreateFromString( interactExpr, NULL );
			estrDestroy( &interactExpr );
		}

		return portalAccum;
	}

	return NULL;
}

static WorldGameActionProperties* ugcMissionPortalDoorGameAction( SA_PARAM_OP_STR const char* objectiveName,
	const char* toMap, const char* toSpawn )
{
	WorldGameActionProperties* accum = StructCreate( parse_WorldGameActionProperties );
	accum->eActionType = WorldGameActionType_Warp;
	accum->pWarpProperties = StructCreate( parse_WorldWarpActionProperties );
	if( ugcDefaultsIsMapLinkIncludeTeammatesEnabled() ) {
		accum->pWarpProperties->bIncludeTeammates = true;
	}
	accum->pWarpProperties->warpDest.eType = WVAR_MAP_POINT;
	accum->pWarpProperties->warpDest.eDefaultType = WVARDEF_SPECIFY_DEFAULT;
	accum->pWarpProperties->warpDest.pSpecificValue = StructCreate( parse_WorldVariable );
	accum->pWarpProperties->warpDest.pSpecificValue->eType = WVAR_MAP_POINT;
	if( !nullStr( toMap ) && stricmp( toSpawn, "MISSION_RETURN" ) != 0 ) {
		accum->pWarpProperties->warpDest.pSpecificValue->pcZoneMap = StructAllocString( toMap );
		accum->pWarpProperties->warpDest.pSpecificValue->pcStringVal = StructAllocString( toSpawn );

		if( !nullStr( objectiveName ) ) {
			char expr[ 256 ];
			WorldVariableDef* missionNumDef = StructCreate( parse_WorldVariableDef );

			eaPush( &accum->pWarpProperties->eaVariableDefs, missionNumDef );
			missionNumDef->pcName = allocAddString( "Mission_Num" );
			missionNumDef->eType = WVAR_INT;
			missionNumDef->eDefaultType = WVARDEF_EXPRESSION;

			sprintf( expr, "not TeamMissionStateSucceeded(\"%s\")", objectiveName );
			missionNumDef->pExpression = exprCreateFromString( expr, NULL );
			eaPush( &accum->pWarpProperties->eaVariableDefs, missionNumDef );
		} else {
			// Null means "Whole Mission completed".
			//
			// We don't care about that for setting mission num
		}
	} else {
		accum->pWarpProperties->warpDest.pSpecificValue->pcStringVal = StructAllocString( "MissionReturn" );
	}

	return accum;
}

static void ugcMissionGenerateOverride( InteractableOverride*** out_peaOverrides, SA_PARAM_OP_VALID UGCProjectData* ugcProj, const char* objectiveName, int missionNum,
										const char* fromMap, UGCPortalClickableInfo* fromDoor, const char* text,
										const char* fromCrypticMap, UGCPortalClickableInfo* fromCrypticDoor, const char* fromCrypticText,
										bool autoExecute, WorldGameActionProperties* block )
{
	InteractableOverride* accum = NULL;

	if( missionNum == 0 && ugcDefaultsMapLinkSkipCompletedMaps() ) {
		return;
	}

	if( fromDoor->is_whole_map && !resNamespaceIsUGC( fromMap )) {
		ZoneMapEncounterInfo* zeniInfo = RefSystem_ReferentFromString( "ZoneMapEncounterInfo", fromMap );
		if( zeniInfo && eaSize( &zeniInfo->volume_logical_name ) > 0 ) {
			FOR_EACH_IN_EARRAY_FORWARDS( zeniInfo->volume_logical_name, char, interact_name ) {
				accum = ugcMissionPortalInteractOverride( ugcProj, objectiveName, missionNum, fromMap, interact_name, text );
				if( accum ) {
					accum->pPropertyEntry->pcInteractionClass = allocAddString( "CLICKABLE" );
					accum->pPropertyEntry->pActionProperties = StructCreate( parse_WorldActionInteractionProperties );
					accum->pPropertyEntry->pcCategoryName = allocAddString(	ugcGetDefaults()->pcOverrideCategoryName );
					if( autoExecute ) {
						accum->pPropertyEntry->bAutoExecute = true;
						accum->pPropertyEntry->iPriority = WorldOptionalActionPriority_Low;
					}
					eaPush( &accum->pPropertyEntry->pActionProperties->successActions.eaActions, StructClone( parse_WorldGameActionProperties, block ));

					eaPush( out_peaOverrides, accum );
				}
			} FOR_EACH_END;
		} else {
			// MJF TODO:
			// bFailure = true;
		}
	} else {
		accum = ugcMissionPortalInteractOverride( ugcProj, objectiveName, missionNum, fromMap, ugcMissionGetPortalClickableLogicalNameTemp( fromDoor ), text );
		if( accum ) {
			accum->pPropertyEntry->pcInteractionClass = allocAddString( "CLICKABLE" );
			accum->pPropertyEntry->pActionProperties = StructCreate( parse_WorldActionInteractionProperties );
			accum->pPropertyEntry->pcCategoryName = allocAddString(	ugcGetDefaults()->pcOverrideCategoryName );
			if( autoExecute ) {
				accum->pPropertyEntry->bAutoExecute = true;
				accum->pPropertyEntry->iPriority = WorldOptionalActionPriority_Low;
			}
			eaPush( &accum->pPropertyEntry->pActionProperties->successActions.eaActions, StructClone( parse_WorldGameActionProperties, block ));

			eaPush( out_peaOverrides, accum );
		}
	}
	
	if( accum && ugcDefaultsMapLinkSkipCompletedMaps() && !nullStr( fromCrypticMap )) {
		if( fromCrypticDoor->is_whole_map ) {
			ZoneMapEncounterInfo* zeniInfo = RefSystem_ReferentFromString( "ZoneMapEncounterInfo", fromCrypticMap );
			if( zeniInfo && eaSize( &zeniInfo->volume_logical_name ) > 0 ) {
				FOR_EACH_IN_EARRAY_FORWARDS( zeniInfo->volume_logical_name, char, interact_name ) {
					InteractableOverride* fromCrypticAccum = ugcMissionPortalInteractOverride( ugcProj, objectiveName, missionNum, fromCrypticMap, interact_name, fromCrypticText );
					if( fromCrypticAccum ) {
						fromCrypticAccum->pPropertyEntry->pcInteractionClass = allocAddString( "CLICKABLE" );
						fromCrypticAccum->pPropertyEntry->pActionProperties = StructCreate( parse_WorldActionInteractionProperties );
						if( autoExecute ) {
							fromCrypticAccum->pPropertyEntry->bAutoExecute = true;
							fromCrypticAccum->pPropertyEntry->iPriority = WorldOptionalActionPriority_Low;
						}
						eaPush( &fromCrypticAccum->pPropertyEntry->pActionProperties->successActions.eaActions, StructClone( parse_WorldGameActionProperties, block ));
						eaPush( out_peaOverrides, fromCrypticAccum );
					}
				} FOR_EACH_END;
			} else {
				// MJF TODO:
				// bFailure = true;
			}
		} else {
			InteractableOverride* fromCrypticAccum = ugcMissionPortalInteractOverride( ugcProj, objectiveName, missionNum, fromCrypticMap, ugcMissionGetPortalClickableLogicalNameTemp( fromCrypticDoor ), fromCrypticText );
			if( fromCrypticAccum ) {
				fromCrypticAccum->pPropertyEntry->pcInteractionClass = allocAddString( "CLICKABLE" );
				fromCrypticAccum->pPropertyEntry->pActionProperties = StructCreate( parse_WorldActionInteractionProperties );
				if( autoExecute ) {
					fromCrypticAccum->pPropertyEntry->bAutoExecute = true;
					fromCrypticAccum->pPropertyEntry->iPriority = WorldOptionalActionPriority_Low;
				}
				eaPush( &fromCrypticAccum->pPropertyEntry->pActionProperties->successActions.eaActions, StructClone( parse_WorldGameActionProperties, block ));

				eaPush( out_peaOverrides, fromCrypticAccum );
			}
		}
	}
}

//builds an image menu item override to warp to a map and adds it to contact OverworldMap.
static ImageMenuItemOverride* ugcMissionAddOverworldMapOverride( UGCProjectData *ugcProj, UGCMapLocation* pDoorMapLocation, const char* map_objective_name, const char* end_map_name, const char* spawn_name )
{
	ImageMenuItemOverride* override = StructCreate( parse_ImageMenuItemOverride );
	
	override->pcContactName = allocAddString("Overworld_Map");
	override->pImageMenuItem = StructCreate(parse_ContactImageMenuItem);
	override->pImageMenuItem->name.pEditorCopy = langCreateMessageDefaultOnly( ugcMapGetDisplayName( ugcProj, end_map_name ));
	override->pImageMenuItem->iconImage = StructAllocString( pDoorMapLocation->astrIcon );
	override->pImageMenuItem->x = pDoorMapLocation->positionX;
	override->pImageMenuItem->y = pDoorMapLocation->positionY;
	override->pImageMenuItem->action = StructCreate( parse_WorldGameActionBlock );
	eaPush( &override->pImageMenuItem->action->eaActions, ugcMissionPortalDoorGameAction( map_objective_name, end_map_name, spawn_name ));
	
	//condition so it is only visible when appropriate:
	override->pImageMenuItem->visibleCondition = exprCreateFromString( ugcMissionPortalInteractExpressionTemp( ugcProj, map_objective_name ), NULL );
	
	return override;
}

static void ugcMissionGeneratePortalBetweenComponents(
		UGCProjectData* ugcProj, UGCMissionMapLink* existingLink, UGCComponent* pStartComponent, UGCComponent* pEndComponent, UGCMissionMapLink* fromCrypticLink, const char* mapObjectiveName, const char* mapLinkName,
		UGCGenesisZoneMission* out_mission, InteractableOverride*** out_peaAdditionalProps, ImageMenuItemOverride*** out_imageMenuOverrides)
{
	if (pEndComponent)
	{
		char *start_map_name = NULL;
		char *end_map_name = NULL;
		char *spawn_name = NULL;
		UGCPortalClickableInfo *pClickable = NULL;
		char *from_cryptic_start_map_name = NULL;
		UGCPortalClickableInfo *pFromCrypticClickable = NULL;
		UGCDialogTreePrompt* transitionPrompt = NULL;
		char nsPrefix[RESOURCE_NAME_MAX_SIZE] = { 0 };

		// This is what is actually generated
		WorldGameActionProperties* portalGameAction = NULL;

		if (ugcProj->ns_name)
			sprintf(nsPrefix, "%s:", ugcProj->ns_name);

		ugcMissionGetComponentPortalProperties(ugcProj, pStartComponent, g_UGCMissionName, true, &start_map_name, &pClickable,
											   NULL, NULL);
		ugcMissionGetComponentPortalProperties(ugcProj, pEndComponent, g_UGCMissionName, false, &end_map_name, NULL,
											   &spawn_name, NULL);
		if( fromCrypticLink ) {
			UGCComponent* fromCrypticComponent = ugcComponentFindByID( ugcProj->components, fromCrypticLink->uDoorComponentID );
			ugcMissionGetComponentPortalProperties( ugcProj, fromCrypticComponent, g_UGCMissionName, true, &from_cryptic_start_map_name, &pFromCrypticClickable,
													NULL, NULL );
		}

		if( existingLink && existingLink->pDialogPrompt ) {
			transitionPrompt = existingLink->pDialogPrompt;
		} else {
			transitionPrompt = ugcGetDefaults()->pDefaultTransitionPrompt;
		}

		/// At this point all local variables should be set, and we
		/// start the generation of the portal and portalGameAction.
		 
		// If this uses a world map, then the override should just go
		// to the contact "Overworld_Map".  On Cryptic maps, this does
		// not need to generate an override at all.
		if( existingLink && existingLink->bDoorUsesMapLocation && out_imageMenuOverrides ) {
			// If the spawn is a mission return, then no additional
			// overworld map override is needed.  The player can just
			// choose to go back to the right static map.
			if( stricmp( spawn_name, "MISSION_RETURN" ) != 0 ) {
				if( existingLink->pDoorMapLocation ) {
					eaPush( out_imageMenuOverrides, ugcMissionAddOverworldMapOverride( ugcProj, existingLink->pDoorMapLocation, mapObjectiveName, end_map_name, spawn_name ));
				}
				else
				{
					UGCMapLocation ugcMapLocation;
					ugcMapLocation.positionX = 0.5;
					ugcMapLocation.positionY = 0.5;
					ugcMapLocation.astrIcon = "MapIcon_QuestionMark_01";
					eaPush( out_imageMenuOverrides, ugcMissionAddOverworldMapOverride( ugcProj, &ugcMapLocation, mapObjectiveName, end_map_name, spawn_name ));
				}
			}

			if( !ugcComponentIsOnMap( pStartComponent, NULL, true )) {
				portalGameAction = StructCreate( parse_WorldGameActionProperties );
				portalGameAction->eActionType = WorldGameActionType_Contact;
				portalGameAction->pContactProperties = StructCreate( parse_WorldContactActionProperties );
				SET_HANDLE_FROM_STRING( "ContactDef", "Overworld_Map", portalGameAction->pContactProperties->hContactDef );
			}
		}
		// Otherwise, if we have a transitionPrompt, then the override
		// should go to the transitionPrompt, which actually does the
		// portal warp.
		else if( transitionPrompt ) {
			UGCDialogTreePrompt* defaultPrompt = ugcGetDefaults()->pDefaultTransitionPrompt;
			UGCDialogTreePrompt* prompt = StructClone( parse_UGCDialogTreePrompt, transitionPrompt );
			UGCGenesisMissionPrompt* promptAccum = StructCreate( parse_UGCGenesisMissionPrompt );
			eaPush( &out_mission->desc.eaPrompts, promptAccum );

			// Fill out the UGC Prompt (prompt)
			if( defaultPrompt ) {
				if( nullStr( prompt->pcPromptBody )) {
					StructCopyString( &prompt->pcPromptBody, defaultPrompt->pcPromptBody );
				}
				if( eaSize( &prompt->eaActions ) == 0 ) {
					eaCopyStructs( &defaultPrompt->eaActions, &prompt->eaActions, parse_UGCDialogTreePromptAction );
				}
				if( nullStr( prompt->eaActions[ 0 ]->pcText ) && eaSize( &defaultPrompt->eaActions )) {
					StructCopyString( &prompt->eaActions[ 0 ]->pcText, defaultPrompt->eaActions[ 0 ]->pcText );
				}
				if( !prompt->pcPromptCostume && !IS_HANDLE_ACTIVE( prompt->hPromptPetCostume )) {
					StructCopyString( &prompt->pcPromptCostume, defaultPrompt->pcPromptCostume );
					COPY_HANDLE( prompt->hPromptPetCostume, defaultPrompt->hPromptPetCostume );
				}
				if( nullStr( prompt->pcPromptStyle )) {
					StructCopyString( &prompt->pcPromptStyle, defaultPrompt->pcPromptStyle );
				}
			}

			// Generate the Genesis Prompt (promptAccum)
			promptAccum->pcName = StructAllocString( mapLinkName );
			promptAccum->showWhen.type = UGCGenesisWhen_Manual;
			ugcGeneratePromptBlock( &promptAccum->sPrimaryBlock, ugcProj, NULL, NULL, NULL, false, 0, NULL, prompt, NULL, NULL, NULL );

			// Fill out the Genesis Prompt's buttons.  The first one
			// is to do the warp, the second should do nothing.
			eaSetSizeStruct( &promptAccum->sPrimaryBlock.eaActions, parse_UGCGenesisMissionPromptAction, 2 );
			eaPush( &promptAccum->sPrimaryBlock.eaActions[ 0 ]->actionBlock.eaActions,
					ugcMissionPortalDoorGameAction( mapObjectiveName, end_map_name, spawn_name ));
			promptAccum->sPrimaryBlock.eaActions[ 1 ]->pcText = StructAllocString( "Not Now" );
			promptAccum->sPrimaryBlock.eaActions[ 1 ]->bDismissAction = true;

			// Redirect the override's action to the Genesis Prompt.
			portalGameAction = StructCreate( parse_WorldGameActionProperties );
			portalGameAction->eActionType = WorldGameActionType_Contact;
			portalGameAction->pContactProperties = StructCreate( parse_WorldContactActionProperties );
			SET_HANDLE_FROM_STRING( "ContactDef", ugcGenesisContactNameRaw( NULL, out_mission->desc.pcName, NULL ),
									portalGameAction->pContactProperties->hContactDef );
			portalGameAction->pContactProperties->pcDialogName = StructAllocString( promptAccum->pcName );

			StructDestroy( parse_UGCDialogTreePrompt, prompt );
		}
		// Otherwise, the override does the portal warp directly.
		else {
			portalGameAction = ugcMissionPortalDoorGameAction( mapObjectiveName, end_map_name, spawn_name );
		}

		// If fromCrypticLink uses the world map, similarly it should
		// just create an override on the contact "Overworld_Map".
		if( fromCrypticLink && fromCrypticLink->bDoorUsesMapLocation && out_imageMenuOverrides ) {
			if( stricmp( spawn_name, "MISSION_RETURN" ) != 0 ) {
				if( existingLink && existingLink->pDoorMapLocation ) {
					eaPush( out_imageMenuOverrides, ugcMissionAddOverworldMapOverride( ugcProj, existingLink->pDoorMapLocation, mapObjectiveName, end_map_name, spawn_name ));
				}
				else
				{
					UGCMapLocation ugcMapLocation;
					ugcMapLocation.positionX = 0.5;
					ugcMapLocation.positionY = 0.5;
					ugcMapLocation.astrIcon = "MapIcon_QuestionMark_01";
					eaPush( out_imageMenuOverrides, ugcMissionAddOverworldMapOverride( ugcProj, &ugcMapLocation, mapObjectiveName, end_map_name, spawn_name ));
				}
			}

			// Make sure no other overrides get generated
			from_cryptic_start_map_name = NULL;
			pFromCrypticClickable = NULL;
		}

		if( portalGameAction ) {
			ugcMissionGenerateOverride( out_peaAdditionalProps, ugcProj, mapObjectiveName, 1,
										start_map_name, pClickable, ugcLinkButtonText( existingLink ),
										from_cryptic_start_map_name, pFromCrypticClickable, ugcLinkButtonText( fromCrypticLink ),
										ugcMissionGetPortalShouldAutoExecute(pClickable), portalGameAction );
		}
			
		StructDestroySafe(parse_UGCPortalClickableInfo, &pClickable);
		StructFreeStringSafe(&start_map_name);
		StructFreeStringSafe(&end_map_name);
		StructFreeStringSafe(&spawn_name);

		// and do the completed mission
		ugcMissionGetComponentPortalProperties(ugcProj, pStartComponent, g_UGCMissionNameCompleted, true, &start_map_name, &pClickable,
			NULL, NULL);
		ugcMissionGetComponentPortalProperties(ugcProj, pEndComponent, g_UGCMissionNameCompleted, false, &end_map_name, NULL,
			&spawn_name, NULL);

		if( resNamespaceIsUGC( start_map_name )) {
			ugcMissionGenerateOverride( out_peaAdditionalProps, ugcProj, mapObjectiveName, 0,
										start_map_name, pClickable, ugcLinkButtonText( existingLink ),
										NULL, NULL, NULL,
										ugcMissionGetPortalShouldAutoExecute(pClickable), portalGameAction );	
			StructDestroySafe(parse_UGCPortalClickableInfo, &pClickable);
			StructFreeStringSafe(&start_map_name);
			StructFreeStringSafe(&end_map_name);
			StructFreeStringSafe(&spawn_name);
		}

		StructDestroySafe(parse_WorldGameActionProperties, &portalGameAction);
	}
	else
	{
		// bFailure = true; TomY TODO - do something
	}
}

static void ugcMissionGeneratePortal( UGCProjectData *ugcProj, UGCMissionObjective** missionObjectives, UGCMapTransitionInfo* transition, UGCMapTransitionInfo* fromCrypticTransition, UGCGenesisZoneMission* mission, InteractableOverride*** peaAdditionalProps, ImageMenuItemOverride*** imageMenuOverrides)
{
	UGCMissionObjective* mapObjective = ugcObjectiveFind( missionObjectives, transition->objectiveID );
	char mapObjectiveFullName[ 128 ];
	char mapLinkName[ 256 ];
	const char* prevMap = (transition->prevIsInternal ? transition->prevMapName : NULL);
	const char* nextMap = ugcObjectiveInternalMapName( ugcProj, mapObjective );	
	UGCComponent *pStartComponent = NULL;
	UGCComponent *pEndComponent = NULL;
	UGCMissionMapLink *existingLink = ugcMissionFindLink( ugcProj->mission, ugcProj->components, nextMap, prevMap );

	// Cryptic map needs a subset of the above
	UGCMissionObjective* fromCrypticMapObjective = NULL;
	// fromCrypticPrevMap would always be NULL!
	const char* fromCrypticNextMap = NULL;
	UGCComponent *pFromCrypticStartComponent = NULL;
	UGCMissionMapLink *fromCrypticLink = NULL;

	if (existingLink)
	{
		pStartComponent = ugcComponentFindByID(ugcProj->components, existingLink->uDoorComponentID);
		pEndComponent = ugcComponentFindByID(ugcProj->components, existingLink->uSpawnComponentID);
	}
	else
	{
		// Find Whole Map component on prevMap
		if (prevMap)
		{
			char nameSpacedName[RESOURCE_NAME_MAX_SIZE] = { 0 };
			sprintf(nameSpacedName,"%s:%s",ugcProj->ns_name,prevMap);
			pStartComponent = ugcMissionGetDefaultComponentForMap(ugcProj, UGC_COMPONENT_TYPE_WHOLE_MAP, nameSpacedName);
		}
	}

	if( fromCrypticTransition ) {
		fromCrypticMapObjective = ugcObjectiveFind( missionObjectives, fromCrypticTransition->objectiveID );
		fromCrypticNextMap = ugcObjectiveInternalMapName( ugcProj, fromCrypticMapObjective );
		fromCrypticLink = ugcMissionFindLink( ugcProj->mission, ugcProj->components, fromCrypticNextMap, fromCrypticTransition->prevMapName );
		assert( (!fromCrypticTransition->prevIsInternal || SAFE_MEMBER( fromCrypticLink, bDoorUsesMapLocation )) && fromCrypticNextMap );

		if( fromCrypticLink ) {
			pFromCrypticStartComponent = ugcComponentFindByID( ugcProj->components, fromCrypticLink->uDoorComponentID );
		} 
	}

	if (!pEndComponent)
	{
		char pcMapName[256] = { 0 };
		if (nextMap)
			sprintf(pcMapName, "%s:%s", ugcProj->ns_name, nextMap);
		pEndComponent = ugcMissionGetDefaultComponentForMap(ugcProj, UGC_COMPONENT_TYPE_SPAWN, pcMapName[0] ? pcMapName : NULL);
	}

	{
		char nsPrefix[ RESOURCE_NAME_MAX_SIZE ] = "";
		if (ugcProj->ns_name)
			sprintf(nsPrefix, "%s:", ugcProj->ns_name);
		
		sprintf( mapObjectiveFullName, "%s%s::%s", nsPrefix, ugcProj->project_prefix, ugcMissionObjectiveLogicalNameTemp( mapObjective ));
	}
	sprintf( mapLinkName, "MapLink%d", mapObjective->id );

	ugcMissionGeneratePortalBetweenComponents( ugcProj, existingLink, pStartComponent, pEndComponent, fromCrypticLink,
											   mapObjectiveFullName, mapLinkName,
											   mission, peaAdditionalProps, imageMenuOverrides );
}

static void ugcMissionGenerateReturnPortal( UGCProjectData* ugcProj, UGCGenesisZoneMission* out_mission, InteractableOverride*** out_peaAdditionalProps )
{	
	if( ugcProj->mission->return_map_link ) {
		UGCComponent *pStartComponent = ugcComponentFindByID(ugcProj->components, ugcProj->mission->return_map_link->uDoorComponentID);

		UGCComponent *pEndComponent = ugcMissionGetDefaultComponentForMap( ugcProj, UGC_COMPONENT_TYPE_SPAWN, NULL );
		devassert(pEndComponent); // guaranteed by ugcMissionGetDefaultComponentForMap with NULL map

		if(!pStartComponent)
			ugcRaiseErrorInternalCode(UGC_ERROR, "Internal Error - Return map link specifies door component %u, but it does not exist.", ugcProj->mission->return_map_link->uDoorComponentID);

		// we never want an ImageMenuOverride on the return link because the return link allows the player to choose any static map (if the overworld map
		// is being used for the return)
		ugcMissionGeneratePortalBetweenComponents( ugcProj, ugcProj->mission->return_map_link, pStartComponent, pEndComponent, NULL, NULL, NULL, out_mission, out_peaAdditionalProps, /*out_imageMenuOverrides=*/NULL );
	} 
}

static void ugcMissionGenerateFinalRewardBoxObjective( UGCProjectData *ugcProj, UGCGenesisZoneMission* mission, InteractableOverride*** peaAdditionalProps, MissionOfferOverride*** peaAdditionalOffers )
{
	if(ugcDefaultsIsFinalRewardBoxSupported())
	{
		UGCComponent *reward_box = ugcComponentFindFinalRewardBox(ugcProj->components);
		if(reward_box)
		{
			// Arbitrary, but by convention
			#define REWARD_BOX_SUBMISSION_NAME "Get_Reward"

			char mapName[ RESOURCE_NAME_MAX_SIZE ] = { 0 };
			char objectName[ RESOURCE_NAME_MAX_SIZE ] = { 0 };

			UGCGenesisMissionObjective* objective = StructCreate( parse_UGCGenesisMissionObjective );

			UGCGenesisWhenExternalChallenge *challenge = StructCreate( parse_UGCGenesisWhenExternalChallenge );

			// The map name and object name of the reward box depend on external or internal placement.
			// Other than this, the setup for a reward box is the same for either case because the reward box must be on the personal mission.
			if(reward_box->sPlacement.bIsExternalPlacement)
			{
				sprintf( mapName, "%s", reward_box->sPlacement.pcExternalMapName );
				sprintf( objectName, "%s", reward_box->sPlacement.pcExternalObjectName );
			}
			else
			{
				char nsPrefix[RESOURCE_NAME_MAX_SIZE] = { 0 };

				if (ugcProj->ns_name)
					sprintf(nsPrefix, "%s:", ugcProj->ns_name);

				sprintf( mapName, "%s%s", nsPrefix, reward_box->sPlacement.pcMapName );
				sprintf( objectName, "%s_%s", g_UGCMissionName, ugcComponentGetLogicalNameTemp(reward_box) );
			}

			challenge->eType = GenesisChallenge_Clickie;
			challenge->pcMapName = StructAllocString(mapName);
			challenge->pcName = StructAllocString(objectName);
			objective->pcName = StructAllocString(REWARD_BOX_SUBMISSION_NAME);
			objective->pcShortText = ugcAllocSMFString(ugcDefaultsGetRewardBoxDisplay(), false);
			objective->pcSuccessFloaterText = ugcAllocSMFString(ugcDefaultsGetRewardBoxLootedDisplay(), false);
			objective->succeedWhen.type = reward_box->sPlacement.bIsExternalPlacement ? UGCGenesisWhen_ExternalRewardBoxLooted : UGCGenesisWhen_RewardBoxLooted;
			objective->eWaypointMode = UGCGenesisMissionWaypointMode_Points;
			eaPush(&objective->succeedWhen.eaExternalChallenges, challenge);

			SET_HANDLE_FROM_REFDATA(g_hRewardTableDict, ugcDefaultsGetRewardBoxReward(), objective->hReward);

			eaPush(&mission->desc.eaObjectives, objective);

			// interactable override for reward box
			{
				char objective_name[ 256 ];
				InteractableOverride* interactable = StructCreate( parse_InteractableOverride );
				WorldInteractionPropertyEntry* entry = StructCreate( parse_WorldInteractionPropertyEntry );
				char* interactExpr = NULL;
				interactable->pPropertyEntry = entry;

				interactable->pcMapName = allocAddString( mapName );
				interactable->pcInteractableName = allocAddString( objectName );

				snprintf_s( objective_name, sizeof(objective_name), "%s::" REWARD_BOX_SUBMISSION_NAME, mission->desc.pcName );

				estrConcatf( &interactExpr, "MissionStateInProgress(\"%s\")", objective_name );
				entry->pInteractCond = exprCreateFromString( interactExpr, NULL );
				entry->pcInteractionClass = allocAddString("Contact");
				entry->bUseExclusionFlag = true;
				entry->pContactProperties = StructCreate(parse_WorldContactInteractionProperties);
				SET_HANDLE_FROM_STRING(g_ContactDictionary, ugcDefaultsGetRewardBoxContact(), entry->pContactProperties->hContactDef);
				estrDestroy( &interactExpr );

				eaPush(peaAdditionalProps, interactable);
			}

			// mission offer override for reward box
			{
				MissionOfferOverride* offer = StructCreate( parse_MissionOfferOverride );
				char dialog_name[ 512 ];

				snprintf_s( dialog_name, sizeof(dialog_name), "%s/%s", mission->desc.pcName, mission->desc.pcName );

				offer->pcContactName = allocAddString(ugcDefaultsGetRewardBoxContact());
				offer->pMissionOffer = StructCreate( parse_ContactMissionOffer );
				SET_HANDLE_FROM_STRING(g_MissionDictionary, mission->desc.pcName, offer->pMissionOffer->missionDef);
				offer->pMissionOffer->allowGrantOrReturn = ContactMissionAllow_SubMissionComplete;
				offer->pMissionOffer->eUIType = ContactMissionUIType_FauxTreasureChest;
				offer->pMissionOffer->pchSpecialDialogName = allocAddString(dialog_name);
				offer->pMissionOffer->pchSubMissionName = StructAllocString(REWARD_BOX_SUBMISSION_NAME);

				eaPush(peaAdditionalOffers, offer);
			}
		}
	}
}

static void ugcMissionGenerateReturnMapLinkObjective( UGCGenesisZoneMission* missionAccum, const UGCProjectData* ugcProj )
{
	const UGCMissionMapLink* mapLink = ugcProj->mission->return_map_link;
	UGCComponent* returnComponent = ugcComponentFindByID( ugcProj->components, mapLink->uDoorComponentID );

	UGCGenesisMissionObjective* objective = StructCreate( parse_UGCGenesisMissionObjective );
	char lastMapNameBuffer[ RESOURCE_NAME_MAX_SIZE ];
	char returnObjectNameBuffer[ RESOURCE_NAME_MAX_SIZE ];

	if( !returnComponent ) {
		ugcRaiseErrorInternalCode( UGC_ERROR, "Internal Error - Return MapLink component doesn't exist." );
		return;
	}
	sprintf( lastMapNameBuffer, "%s:%s", ugcProj->ns_name, returnComponent->sPlacement.pcMapName );
	sprintf( returnObjectNameBuffer, "%s_%s", g_UGCMissionName, ugcComponentGetLogicalNameTemp( returnComponent ));

	objective->pcName = StructAllocString( "Leave_Map" );
	objective->pcShortText = StructAllocString( ugcMapMissionLinkReturnText( mapLink ));
	objective->succeedWhen.type = UGCGenesisWhen_ExternalMapStart;
	objective->succeedWhen.bNot = true;

	// Place a waypoint directly on the link object
	if( returnComponent->eType != UGC_COMPONENT_TYPE_WHOLE_MAP )
		ugcMissionGenerateExtraObjectiveWaypoint(objective, lastMapNameBuffer, returnObjectNameBuffer);

	eaPush( &objective->succeedWhen.eaExternalMapNames, StructAllocString( lastMapNameBuffer ));

	eaPush( &missionAccum->desc.eaObjectives, objective );	
}

static void ugcMissionGenerateMissionPortals( UGCProjectData *ugcProj, UGCMissionObjective** objectives, UGCGenesisZoneMission* mission, InteractableOverride*** peaAdditionalProps, ImageMenuItemOverride*** imageMenuOverrides )
{
	UGCMapTransitionInfo** mapTransitions = ugcMissionGetMapTransitions( ugcProj, objectives );
	UGCMapTransitionInfo* fromCrypticTransition = NULL;
	FOR_EACH_IN_EARRAY_FORWARDS(mapTransitions, UGCMapTransitionInfo, transition)
	{
		UGCMissionObjective* objective = ugcObjectiveFind( objectives, transition->objectiveID );
		bool isInternal;
		const char* mapName = ugcObjectiveMapName( ugcProj, objective, &isInternal );
		UGCMissionMapLink* transitionLink = ugcMissionFindLink( ugcProj->mission, ugcProj->components, mapName, transition->prevMapName );

		if( !transition->prevIsInternal || SAFE_MEMBER( transitionLink, bDoorUsesMapLocation )) {
			fromCrypticTransition = transition;
		}
		if( isInternal || transition->prevIsInternal ) {
			ugcMissionGeneratePortal( ugcProj, objectives, transition,
									  // We only want to have the Extra Cryptic -> Internal
									  // transition when there are second and third internal
									  // maps linked off that Cryptic door.
									  fromCrypticTransition != transition ? fromCrypticTransition : NULL,
									  mission, peaAdditionalProps, imageMenuOverrides );
		}

		// The first map transition is guaranteed to be for the first
		// objective.  This is where you start.
		if( FOR_EACH_IDX( mapTransitions, transition ) == 0 ) {
			UGCComponent* endComponent = NULL;

			if( transitionLink ) {
				endComponent = ugcComponentFindByID( ugcProj->components, transitionLink->uSpawnComponentID );
			} else {
				endComponent = ugcMissionGetDefaultComponentForMap( ugcProj, UGC_COMPONENT_TYPE_SPAWN, mapName );
			}

			if( endComponent && isInternal ) {
				char* endMapName = NULL;
				char* spawnName = NULL;
				ugcMissionGetComponentPortalProperties( ugcProj, endComponent, g_UGCMissionName, false, &endMapName, NULL, &spawnName, NULL );

				StructCopyString( &ugcProj->mission->strInitialMapName, endMapName );
				StructCopyString( &ugcProj->mission->strInitialSpawnPoint, spawnName );
			} else {
				//if it's a UGC map, make sure it includes the namespace:
				if (isInternal && !strchr_fast(mapName,':')){
					char buffer[256];
					sprintf(buffer, "%s:%s", ugcProj->ns_name, mapName);
					StructCopyString( &ugcProj->mission->strInitialMapName, buffer );
				}else{
					StructCopyString( &ugcProj->mission->strInitialMapName, mapName );
				}
				StructCopyString( &ugcProj->mission->strInitialSpawnPoint, NULL );
			}
		}
	}
	FOR_EACH_END;

	// Return map transition is only available when all map
	// transitions specify a door.
	if( ugcDefaultsMapTransitionsSpecifyDoor() ) {
		ugcMissionGenerateReturnPortal( ugcProj, mission, peaAdditionalProps );
	}

	eaDestroyStruct( &mapTransitions, parse_UGCMapTransitionInfo );
}

UGCGenesisMissionDrop *ugcComponentGenerateMissionDropMaybe(UGCComponent* kill, UGCProjectData* ugcProj)
{
	if(!nullStr(kill->pcDropItemName))
	{
		UGCGenesisMissionDrop *dropAccum = StructCreate(parse_UGCGenesisMissionDrop);
		char nsPrefix[RESOURCE_NAME_MAX_SIZE] = { 0 };
		char item_drop_name[RESOURCE_NAME_MAX_SIZE] = { 0 };
		char encounterName[RESOURCE_NAME_MAX_SIZE] = { 0 };
		char mapName[RESOURCE_NAME_MAX_SIZE] = { 0 };

		if(ugcProj->ns_name)
			sprintf(nsPrefix, "%s:", ugcProj->ns_name);
		sprintf(item_drop_name, "%s%s", nsPrefix, kill->pcDropItemName);
		SET_HANDLE_FROM_STRING("RewardTable", item_drop_name, dropAccum->hReward);

		sprintf(encounterName, "LogGrp_%s_%s", g_UGCMissionName, ugcComponentGetLogicalNameTemp(kill));
		dropAccum->astrEncounterLogicalName = allocAddString(encounterName);

		sprintf(mapName, "%s%s", nsPrefix, kill->sPlacement.pcMapName );
		dropAccum->astrMapName = allocAddString(mapName);

		return dropAccum;
	}
	return NULL;
}

////////////////////////////////////////////////////////////
// Main Mission Generation
////////////////////////////////////////////////////////////

bool ugcReturnMissionRequired( UGCProjectData* ugcProj )
{
	return ugcDefaultsIsFinalRewardBoxSupported() && ugcProj->mission->return_map_link;
}

void ugcMissionGenerate( UGCProjectData* ugcProj )
{
	// generate the mission
	UGCGenesisZoneMission* genesisMissionAccum = NULL;
	UGCGenesisMissionAdditionalParams additionalParams = { 0 };
	UGCMissionObjective** missionObjectives = NULL;
	char missionName[ RESOURCE_NAME_MAX_SIZE ];

	ugcMissionTransmogrifyObjectives( ugcProj, SAFE_MEMBER( ugcProj, mission->objectives ), NULL, false, &missionObjectives );
	if( !ugcProj ) {
		eaDestroyStruct( &missionObjectives, parse_UGCMissionObjective );
		return;
	}

	genesisMissionAccum = StructCreate( parse_UGCGenesisZoneMission );

	if (!nullStr(ugcProj->ns_name))
		sprintf(missionName, "%s:%s", ugcProj->ns_name, ugcProj->project_prefix);
	else
		strcpy(missionName, ugcProj->project_prefix);
	genesisMissionAccum->desc.pcName = StructAllocString( missionName );
	genesisMissionAccum->desc.pcDisplayName = ugcAllocSMFString( ugcProj->project->pcPublicName, false );
	genesisMissionAccum->desc.pcDescriptionText = ugcAllocSMFString( ugcProj->project->strDescription, true );
	genesisMissionAccum->desc.pcSummaryText = ugcAllocSMFString( ugcProj->project->pcPublicName, false );
	SET_HANDLE_FROM_STRING( "MissionCategory", "UGC", genesisMissionAccum->desc.hCategory );
	
	genesisMissionAccum->desc.generationType = UGCGenesisMissionGenerationType_PlayerMission;
	genesisMissionAccum->desc.grantDescription.eGrantType = UGCGenesisMissionGrantType_Manual;
	genesisMissionAccum->desc.grantDescription.eTurnInType = UGCGenesisMissionTurnInType_Automatic;

	genesisMissionAccum->desc.ePlayType = ugcProjectPlayType( ugcProj );
	genesisMissionAccum->desc.eAuthorSource = ContentAuthor_User;
	genesisMissionAccum->desc.ugcProjectID = namespaceIsUGC(ugcProj->ns_name) ? ugcProjectContainerID( ugcProj ) : 0;

	ugcMissionGenerateObjectives( ugcProj, missionObjectives, NULL,
								  &genesisMissionAccum->desc.eaObjectives, &genesisMissionAccum->desc.eaPrompts );

	ugcMissionGenerateFinalRewardBoxObjective( ugcProj, genesisMissionAccum, &additionalParams.eaInteractableOverrides, &additionalParams.eaMissionOfferOverrides );

	{
		UGCGenesisMissionObjective** fsmObjectives = NULL;		
		int it;
		for( it = 0; it != eaSize( &ugcProj->components->eaComponents ); ++it ) {
			UGCComponent* component = ugcProj->components->eaComponents[ it ];
			
			switch( component->eType )
			{
				case UGC_COMPONENT_TYPE_DIALOG_TREE:
				{
					UGCGenesisMissionPrompt* promptAccum = ugcComponentGeneratePromptMaybe( component, ugcProj, missionObjectives, NULL, &fsmObjectives );
					if( promptAccum ) {
						eaPush( &genesisMissionAccum->desc.eaPrompts, promptAccum );
					}
				}
				case UGC_COMPONENT_TYPE_KILL:
				{
					UGCGenesisMissionDrop* dropAccum = ugcComponentGenerateMissionDropMaybe(component, ugcProj);
					if(dropAccum)
						eaPush(&genesisMissionAccum->desc.eaMissionDrops, dropAccum);
				}
			}
		}

		if( fsmObjectives ) {
			UGCGenesisMissionObjective* fsmObjective = StructCreate( parse_UGCGenesisMissionObjective );
			fsmObjective->pcName = StructAllocString( "FSM" );
			fsmObjective->succeedWhen.type = UGCGenesisWhen_AllOf;
			fsmObjective->eaChildren = fsmObjectives;
			fsmObjective->bOptional = true;
			ugcMissionFSMObjectiveFixup( fsmObjective );
			eaInsert( &genesisMissionAccum->desc.eaObjectives, fsmObjective, 0 );
			
			fsmObjectives = NULL;
		}
	}

	ugcMissionGenerateMissionPortals( ugcProj, missionObjectives, genesisMissionAccum, &additionalParams.eaInteractableOverrides, &additionalParams.eaImageMenuItemOverrides );
	// If you have a return_map_link set, we need to put a waypoint on
	// your map showing how to leave after you completed the mission.
	//
	// 
	if( ugcDefaultsMapTransitionsSpecifyDoor() && ugcProj->mission->return_map_link ) {
		UGCGenesisZoneMission* returnDoorMissionAccum = StructCreate( parse_UGCGenesisZoneMission );
		char returnDoorMissionName[ RESOURCE_NAME_MAX_SIZE ];
		if ( !nullStr( ugcProj->ns_name )) {
			sprintf( returnDoorMissionName, "%s:%s_ReturnLink", ugcProj->ns_name, ugcProj->project_prefix );
		} else {
			sprintf( returnDoorMissionName, "%s_ReturnLink", ugcProj->project_prefix );
		}

		returnDoorMissionAccum->desc.pcName = StructAllocString( returnDoorMissionName );
		returnDoorMissionAccum->desc.pcDisplayName = ugcAllocSMFString( ugcProj->project->pcPublicName, false );
		returnDoorMissionAccum->desc.grantDescription.eGrantType = UGCGenesisMissionGrantType_Manual;
		ugcMissionGenerateReturnMapLinkObjective( returnDoorMissionAccum, ugcProj );

		{
			UGCGenesisMissionRequirements* req = ugcGenesisGenerateMission( NULL, NULL, -1, returnDoorMissionAccum, NULL, true, ugcProj->project_prefix, true );
			StructDestroy( parse_UGCGenesisMissionRequirements, req );
			StructDestroySafe( parse_UGCGenesisZoneMission, &returnDoorMissionAccum );
		}

		// Grant this mission when the main mission completes
		{
			WorldGameActionProperties* grantReturnLink = StructCreate( parse_WorldGameActionProperties );
			grantReturnLink->eActionType = WorldGameActionType_GrantMission;
			grantReturnLink->pGrantMissionProperties = StructCreate( parse_WorldGrantMissionActionProperties );
			grantReturnLink->pGrantMissionProperties->eType = WorldMissionActionType_Named;
			SET_HANDLE_FROM_STRING( "Mission", returnDoorMissionName, grantReturnLink->pGrantMissionProperties->hMissionDef );

			eaPush( &additionalParams.eaSuccessActions, grantReturnLink );
		}
	}

	{
		UGCGenesisMissionRequirements* req = ugcGenesisGenerateMission( NULL, NULL, -1, genesisMissionAccum, &additionalParams, true, ugcProj->project_prefix, true );
		StructDestroy( parse_UGCGenesisMissionRequirements, req );
		StructReset( parse_UGCGenesisMissionAdditionalParams, &additionalParams );
		StructDestroySafe( parse_UGCGenesisZoneMission, &genesisMissionAccum );
	}
	
	eaDestroyStruct( &missionObjectives, parse_UGCMissionObjective );
}

////////////////////////////////////////////////////////////
// Component/Challenge Generation
////////////////////////////////////////////////////////////

static void ugcComponentSetClickyAndCheckedAttrib( UGCGenesisMissionChallengeClickie** out_ppClickie, UGCCheckedAttrib** out_ppCheckedAttrib, UGCProjectData* ugcProj, const UGCComponent* component, const UGCInteractProperties* props )
{
	char nsPrefix[ RESOURCE_NAME_MAX_SIZE ] = { 0 };
	if( ugcProj->ns_name ) {
		sprintf( nsPrefix, "%s:", ugcProj->ns_name );
	}

	if( !*out_ppClickie ) {
		*out_ppClickie = StructCreate( parse_UGCGenesisMissionChallengeClickie );
	}

	COPY_HANDLE( (*out_ppClickie)->hInteractAnim, props->hInteractAnim );
	SET_HANDLE_FROM_STRING( "InteractionDef", ugcComponentInteractionDef( ugcProj, component, props->eInteractDuration, false ), (*out_ppClickie)->hInteractionDef );

	(*out_ppClickie)->pcInteractText = ugcAllocSMFString( ugcInteractPropsInteractText( props ), false );
	(*out_ppClickie)->pcFailureText = ugcAllocSMFString( ugcInteractPropsInteractFailureText( props ), false );
	
	if( props->succeedCheckedAttrib ) {
		if( !*out_ppCheckedAttrib ) {
			*out_ppCheckedAttrib = StructCreate( parse_UGCCheckedAttrib );
		}
		StructCopyAll( parse_UGCCheckedAttrib, props->succeedCheckedAttrib, *out_ppCheckedAttrib );

		if( props->succeedCheckedAttrib->astrItemName ) {
			(*out_ppCheckedAttrib)->astrItemName = allocAddString( props->succeedCheckedAttrib->astrItemName );
			(*out_ppClickie)->bConsumeSuccessItem = props->bTakesItem;
		}
	}

	if( props->pcDropItemName ) {
		char item_drop_name[RESOURCE_NAME_MAX_SIZE] = { 0 };
		sprintf( item_drop_name, "%s%s", nsPrefix, props->pcDropItemName );
		SET_HANDLE_FROM_STRING( "RewardTable", item_drop_name, (*out_ppClickie)->hRewardTable );
	}
}

static bool ugcComponentSetChallengeClickyParams( UGCProjectData *data, UGCBacklinkTable* pBacklinkTable, UGCComponent *component, UGCGenesisMissionChallenge *challengeAccum)
{
	UGCMissionObjective* objective = ugcObjectiveFindComponent( data->mission->objectives, component->uID );
	bool non_shared = false;

	challengeAccum->eType = GenesisChallenge_Clickie;
	challengeAccum->pClickie = StructCreate( parse_UGCGenesisMissionChallengeClickie );
	{
		char componentName[ 256 ];
		ugcComponentGetDisplayName( componentName, data, component, true );
		challengeAccum->pClickie->strVisibleName = StructAllocString( componentName );
	}
	if( objective ) {
		ugcComponentSetClickyAndCheckedAttrib( &challengeAccum->pClickie, &challengeAccum->succeedCheckedAttrib, data, component, &objective->sInteractProps );
		non_shared = true;
	} else if( ugcBacklinkTableFindTrigger( pBacklinkTable, component->uID, 0 ) || component->bInteractForce ) {
		if (eaSize(&component->eaTriggerGroups) > 0)
		{
			ugcComponentSetClickyAndCheckedAttrib( &challengeAccum->pClickie, &challengeAccum->succeedCheckedAttrib, data, component, component->eaTriggerGroups[ 0 ]);
		}
		else
		{
			SET_HANDLE_FROM_STRING( "InteractionDef", ugcComponentInteractionDef( data, component, UGCDURATION_MEDIUM, false ), challengeAccum->pClickie->hInteractionDef );
			challengeAccum->pClickie->pcInteractText = ugcAllocSMFString( ugcInteractPropsInteractText( NULL ), false );
		}

		non_shared = true;
	}
	else
	{
		StructDestroySafe(parse_UGCGenesisMissionChallengeClickie, &challengeAccum->pClickie);
		challengeAccum->eType = GenesisChallenge_None;
	}

	return non_shared;
}

static void ugcWhenMakeGenesisWhen( const UGCWhen* ugcWhen, U32 componentID, UGCGenesisWhen* when, UGCComponentList* components, UGCMissionObjective** objectives )
{
	if( !ugcWhen ) {
		when->type = UGCGenesisWhen_MapStart;
	} else {
	    bool failed = false;
		
		switch( ugcWhen->eType ) {
			case UGCWHEN_MISSION_START:
			case UGCWHEN_MAP_START:
				when->type = UGCGenesisWhen_MapStart;
				break;
			case UGCWHEN_MANUAL:
				when->type = UGCGenesisWhen_Manual;
				break;
			case UGCWHEN_OBJECTIVE_IN_PROGRESS:
			case UGCWHEN_OBJECTIVE_COMPLETE:
			case UGCWHEN_OBJECTIVE_START:
				{
					int i;
					if (ugcWhen->eType == UGCWHEN_OBJECTIVE_COMPLETE)
						when->type = UGCGenesisWhen_ObjectiveCompleteAll;
					else
						when->type = UGCGenesisWhen_ObjectiveInProgress;
					for (i = 0; i < eaiSize(&ugcWhen->eauObjectiveIDs); i++)
					{
						if (!ugcObjectiveFind(objectives, ugcWhen->eauObjectiveIDs[i]))
						{
							// Tomy TODO - Error
							failed = true;
							break;
						}
						eaPush(&when->eaObjectiveNames, StructAllocString(ugcMissionObjectiveIDLogicalNameTemp(ugcWhen->eauObjectiveIDs[i])));
					}

					if( eaSize( &when->eaObjectiveNames ) == 0 ) {
						when->type = UGCGenesisWhen_Manual;
					}
				}
				break;
			case UGCWHEN_CURRENT_COMPONENT_COMPLETE:
			case UGCWHEN_COMPONENT_COMPLETE:
			case UGCWHEN_COMPONENT_REACHED:
				{
					U32* componentIDs = NULL;
					int i;
					when->type = UGCGenesisWhen_MapStart;

					if( ugcWhen->eType == UGCWHEN_CURRENT_COMPONENT_COMPLETE ) {
						ea32Push( &componentIDs, componentID );
					} else {
						componentIDs = ugcWhen->eauComponentIDs;
					}
					
					for (i = 0; i < ea32Size(&componentIDs); i++)
					{
						UGCComponent *ref_component = ugcComponentFindByID(components, componentIDs[i]);
						if (ref_component)
						{
							if (ref_component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE)
							{
								char promptName[ 256 ];
								if (when->type == UGCGenesisWhen_ChallengeComplete ||
									when->type == UGCGenesisWhen_ReachChallenge)
								{
									// Tomy TODO - Error
									failed = true;
									break;
								}
								sprintf( promptName, "Prompt_%d", ref_component->uID );
								when->type = UGCGenesisWhen_PromptComplete;
								eaPush(&when->eaPromptNames, StructAllocString(promptName));
							}
							else
							{
								if (when->type == UGCGenesisWhen_PromptComplete)
								{
									// Tomy TODO - Error
									failed = true;
									break;
								}
								if (ugcWhen->eType == UGCWHEN_COMPONENT_COMPLETE || ugcWhen->eType == UGCWHEN_CURRENT_COMPONENT_COMPLETE)
									when->type = UGCGenesisWhen_ChallengeComplete;
								else
									when->type = UGCGenesisWhen_ReachChallenge;
								eaPush(&when->eaChallengeNames, StructAllocString(ugcComponentGetLogicalNameTemp(ref_component)));
							}
						}
						else
						{
							// TomY TODO - Error
							failed = true;
							break;
						}
					}

					if( ugcWhen->eType == UGCWHEN_CURRENT_COMPONENT_COMPLETE ) {
						ea32Destroy( &componentIDs );
					}

					if( eaSize( &when->eaChallengeNames ) == 0 && eaSize( &when->eaPromptNames ) == 0 ) {
						when->type = UGCGenesisWhen_Manual;
					}
				}

			xcase UGCWHEN_DIALOG_PROMPT_REACHED:
				{
					int i;
					when->type = UGCGenesisWhen_PromptStart;
					
					for (i = 0; i < eaSize(&ugcWhen->eaDialogPrompts); i++)
					{
						UGCWhenDialogPrompt* whenDialogPrompt = ugcWhen->eaDialogPrompts[ i ];
						UGCComponent* dialogTree = ugcComponentFindByID(components, whenDialogPrompt->uDialogID);
						UGCDialogTreePrompt* prompt = dialogTree ? ugcDialogTreeGetPrompt( &dialogTree->dialogBlock, whenDialogPrompt->iPromptID ) : NULL;

						if( dialogTree && dialogTree->eType == UGC_COMPONENT_TYPE_DIALOG_TREE && prompt )
						{
							char buffer[ 256 ];
							UGCGenesisWhenPromptBlock* promptBlock = StructCreate( parse_UGCGenesisWhenPromptBlock );
							
							sprintf( buffer, "Prompt_%d", whenDialogPrompt->uDialogID );
							promptBlock->promptName = StructAllocString( buffer );

							if( dialogTree->dialogBlock.blockIndex == 0 && whenDialogPrompt->iPromptID == -1 ) {
								promptBlock->blockName = NULL;
							} else if( whenDialogPrompt->iPromptID >= 0 ) {
								sprintf( buffer, "Block%d_%d", dialogTree->dialogBlock.blockIndex, whenDialogPrompt->iPromptID );
								promptBlock->blockName = StructAllocString( buffer );
							} else {
								sprintf( buffer, "Block%d", dialogTree->dialogBlock.blockIndex );
								promptBlock->blockName = StructAllocString( buffer );
							}
							eaPush( &when->eaPromptBlocks, promptBlock );
						}
						else
						{
							// TomY TODO - Error
							failed = true;
							break;
						}
					}
					
					if( eaSize( &when->eaPromptBlocks ) == 0 ) {
						when->type = UGCGenesisWhen_Manual;
					}
				}

			xcase UGCWHEN_PLAYER_HAS_ITEM:
				{
					when->type = UGCGenesisWhen_ItemCount;
					eaPush(&when->eaItemDefNames, StructAllocString(ugcWhen->strItemName));
					when->iItemCount = 1;
				}
		}
	}
}

static void ugcComponentGenerateObjectives(UGCComponent *component, UGCMissionObjective** objectives, UGCComponentList* components, UGCGenesisMissionObjective*** out_extraObjectives,
											UGCGenesisMissionObjective **out_beforeObjective, UGCGenesisMissionObjective **out_duringObjective)
{
	// MJF TODO: Combine this with the logic for prompt actions.
	UGCGenesisWhen startWhen = { UGCGenesisWhen_MapStart };
	UGCGenesisWhen hideWhen = { UGCGenesisWhen_MapStart };
	UGCGenesisMissionObjective* beforeObjective = NULL;
	UGCGenesisMissionObjective* duringObjective = NULL;

	ugcWhenMakeGenesisWhen( component->pStartWhen, component->uID, &startWhen, components, objectives );
	ugcWhenMakeGenesisWhen( component->pHideWhen, component->uID, &hideWhen, components, objectives );

	if (startWhen.type == UGCGenesisWhen_MapStart && hideWhen.type == UGCGenesisWhen_Manual) {
		*out_beforeObjective = NULL;
		*out_duringObjective = NULL;
		return;
	}

	if( startWhen.type != UGCGenesisWhen_MapStart ) {
		char buffer[ 256 ];
		beforeObjective = StructCreate( parse_UGCGenesisMissionObjective );
		sprintf( buffer, "BeforeVisible_Component_%d", component->uID );
		beforeObjective->pcName = StructAllocString( buffer );
		StructCopyAll( parse_UGCGenesisWhen, &startWhen, &beforeObjective->succeedWhen );

		duringObjective = StructCreate( parse_UGCGenesisMissionObjective );
		sprintf( buffer, "Visible_Component_%d", component->uID );
		duringObjective->pcName = StructAllocString( buffer );
		duringObjective->succeedWhen.type = UGCGenesisWhen_Manual;
		duringObjective->bOptional = true;
	}
	if( hideWhen.type != UGCGenesisWhen_MapStart ) {
		if( !duringObjective ) {
			char buffer[ 256 ];
			duringObjective = StructCreate( parse_UGCGenesisMissionObjective );
			sprintf( buffer, "Visible_Component_%d", component->uID );
			duringObjective->pcName = StructAllocString( buffer );
		}
		StructCopyAll( parse_UGCGenesisWhen, &hideWhen, &duringObjective->succeedWhen );
	}

	if( beforeObjective && duringObjective ) {
		UGCGenesisMissionObjective* parentObjective = StructCreate( parse_UGCGenesisMissionObjective );
		char buffer[ 256 ];
		sprintf( buffer, "Parent_Component_%d", component->uID );
		parentObjective->pcName = StructAllocString( buffer );
		parentObjective->succeedWhen.type = UGCGenesisWhen_InOrder;
		eaPush( &parentObjective->eaChildren, beforeObjective );
		eaPush( &parentObjective->eaChildren, duringObjective );
		
		eaPush( out_extraObjectives, parentObjective );
	} else if( duringObjective ) {
		eaPush( out_extraObjectives, duringObjective );
	}

	*out_beforeObjective = beforeObjective;
	*out_duringObjective = duringObjective;
}

static void ugcComponentApplyChallengeWhenSequence(UGCWhen **whenList, UGCGenesisMissionChallenge **challengeList, U32 currentComponentID,
											UGCMissionObjective **objectives, UGCComponentList *components,
											UGCGenesisMissionObjective ***out_extraObjectives)
{
	int when_idx, challenge_idx;
	char buffer[ 256 ];
	UGCGenesisMissionObjective **objectiveList = NULL;
	UGCGenesisMissionObjective* parentObjective = StructCreate( parse_UGCGenesisMissionObjective );

	assert(eaSize(&challengeList) == eaSize(&whenList)+1);

	sprintf( buffer, "Component_%d_State_Parent", currentComponentID );
	parentObjective->pcName = StructAllocString( buffer );
	parentObjective->succeedWhen.type = UGCGenesisWhen_InOrder;

	for (when_idx = 0; when_idx < eaSize(&whenList); when_idx++)
	{
		UGCGenesisMissionObjective *newObjective = StructCreate(parse_UGCGenesisMissionObjective);
		sprintf(buffer, "Component_%d_State_%d", currentComponentID, when_idx);
		newObjective->pcName = StructAllocString( buffer );
		ugcWhenMakeGenesisWhen(whenList[when_idx], currentComponentID, &newObjective->succeedWhen, components, objectives);
		eaPush(&objectiveList, newObjective);
		eaPush(&parentObjective->eaChildren, newObjective);
	}

	if (parentObjective->eaChildren)
	{
		eaPush( out_extraObjectives, parentObjective );
	}
	else
	{
		StructDestroy(parse_UGCGenesisMissionObjective, parentObjective);
	}

	for (challenge_idx = 0; challenge_idx < eaSize(&challengeList); challenge_idx++)
	{
		if (challengeList[challenge_idx])
		{
			UGCGenesisWhen *when = &challengeList[challenge_idx]->clickieVisibleWhen;
			if (challenge_idx < eaSize(&objectiveList))
			{
				when->type = UGCGenesisWhen_ObjectiveInProgress;
				eaPush( &when->eaObjectiveNames, StructAllocString(objectiveList[challenge_idx]->pcName));
			}
			else if (challenge_idx == eaSize(&objectiveList))
			{
				when->type = UGCGenesisWhen_ObjectiveComplete;
				eaPush( &when->eaObjectiveNames, StructAllocString(objectiveList[challenge_idx-1]->pcName));
			}
		}
	}
}

static void ugcComponentApplyVisibleWhen( UGCGenesisWhen* visibleWhen, UGCComponent* component, UGCMissionObjective** objectives, UGCComponentList* components, UGCGenesisMissionObjective*** out_extraObjectives )
{
	if( visibleWhen ) {
		UGCGenesisMissionObjective* beforeObjective = NULL;
		UGCGenesisMissionObjective* duringObjective = NULL;
	
		ugcComponentGenerateObjectives( component, objectives, components, out_extraObjectives, &beforeObjective, &duringObjective);
				
		if( duringObjective ) {
			visibleWhen->type = UGCGenesisWhen_ObjectiveInProgress;
			eaPush( &visibleWhen->eaObjectiveNames, StructAllocString( duringObjective->pcName ));
		}
		if( component->visibleCheckedAttrib.astrItemName || component->visibleCheckedAttrib.astrSkillName ) {
			visibleWhen->checkedAttrib = StructClone( parse_UGCCheckedAttrib, &component->visibleCheckedAttrib );
		}
	}
}

static bool ugcComponentApplyWhen( UGCGenesisWhen* clickableWhen, UGCGenesisWhen* visibleWhen, UGCComponent* component, UGCMissionObjective** objectives, UGCComponentList* components, UGCGenesisMissionObjective*** out_extraObjectives )
{
	UGCMissionObjective* componentObjective = ugcObjectiveFindComponent( objectives, component->uID );
			
	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_OBJECT: case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED: case UGC_COMPONENT_TYPE_PLANET:
		case UGC_COMPONENT_TYPE_ROOM_DOOR: case UGC_COMPONENT_TYPE_CLUSTER_PART: case UGC_COMPONENT_TYPE_FAKE_DOOR: {

			assert( component->eType == UGC_COMPONENT_TYPE_PLANET || clickableWhen );
			if( clickableWhen ) {
				if( componentObjective ) {
					char objectiveName[ 256 ];
					sprintf( objectiveName, "Objective_%d", componentObjective->id );
					clickableWhen->type = UGCGenesisWhen_ObjectiveInProgress;
					eaPush( &clickableWhen->eaObjectiveNames, StructAllocString( objectiveName ));
				} else {
					clickableWhen->type = UGCGenesisWhen_MapStart;
				}
			}

			ugcComponentApplyVisibleWhen( visibleWhen, component, objectives, components, out_extraObjectives );
			
			return (componentObjective != NULL
					|| (visibleWhen && (visibleWhen->type != UGCGenesisWhen_MapStart || visibleWhen->checkedAttrib)));
		}
			
		xcase UGC_COMPONENT_TYPE_KILL: {
			if( componentObjective ) {
				char objectiveName[ 256 ];
				sprintf( objectiveName, "Objective_%d", componentObjective->id );
				visibleWhen->type = UGCGenesisWhen_ObjectiveInProgress;
				eaPush( &visibleWhen->eaObjectiveNames, StructAllocString( objectiveName ));
			} else {
				ugcComponentApplyVisibleWhen( visibleWhen, component, objectives, components, out_extraObjectives );
			}
		}
			
		xcase UGC_COMPONENT_TYPE_CONTACT: case UGC_COMPONENT_TYPE_SOUND: {
			ugcComponentApplyVisibleWhen( visibleWhen, component, objectives, components, out_extraObjectives );
		}
	}
	return true;
}

static void ugcComponentApplyTrapWhens( UGCGenesisWhen* visibleWhen_Unarmed, UGCGenesisWhen* visibleWhen_Armed, UGCGenesisWhen* visibleWhen_Disarmed, UGCComponent* component, UGCMissionObjective** objectives, UGCComponentList* components, UGCGenesisMissionObjective*** out_extraObjectives )
{
	UGCGenesisMissionObjective* beforeObjective = NULL;
	UGCGenesisMissionObjective* duringObjective = NULL;

	ugcComponentGenerateObjectives(component, objectives, components, out_extraObjectives, &beforeObjective, &duringObjective);

	if (visibleWhen_Unarmed)
	{
		if (beforeObjective)
		{
			visibleWhen_Unarmed->type = UGCGenesisWhen_ObjectiveInProgress;
			eaPush( &visibleWhen_Unarmed->eaObjectiveNames, StructAllocString( beforeObjective->pcName ));
		}
		else
		{
			visibleWhen_Unarmed->type = UGCGenesisWhen_Manual;
		}
	}

	if (visibleWhen_Armed)
	{
		if (duringObjective)
		{
			visibleWhen_Armed->type = UGCGenesisWhen_ObjectiveInProgress;
			eaPush( &visibleWhen_Armed->eaObjectiveNames, StructAllocString( duringObjective->pcName ));
		}
		else
		{
			visibleWhen_Armed->type = UGCGenesisWhen_MapStart;
		}
	}

	if (visibleWhen_Disarmed)
	{
		if (duringObjective)
		{
			visibleWhen_Disarmed->type = UGCGenesisWhen_ObjectiveComplete;
			eaPush( &visibleWhen_Disarmed->eaObjectiveNames, StructAllocString( duringObjective->pcName ));
		}
		else
		{
			visibleWhen_Disarmed->type = UGCGenesisWhen_Manual;
		}
	}
}

static void ugcComponentSetDetails(UGCComponent *component, UGCGenesisMissionChallenge *challengeAccum)
{
	UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);
	if (room_info)
	{
		FOR_EACH_IN_EARRAY(room_info->details, UGCRoomDetailDef, detail_def)
		{
			int idx = FOR_EACH_IDX(room_info->details, detail_def);
			UGCGenesisGroupDefChildParam *child_param = StructCreate(parse_UGCGenesisGroupDefChildParam);
			child_param->iValue = 0;
			FOR_EACH_IN_EARRAY(component->eaRoomDetails, UGCRoomDetailData, room_data)
			{
				if (room_data->iIndex == idx)
				{
					child_param->iValue = room_data->iChoice;
					break;
				}
			}
			FOR_EACH_END;
			child_param->astrParameter = detail_def->astrParameter;
			eaPush(&challengeAccum->eaChildParams, child_param);
		}
		FOR_EACH_END;
	}
}

static void ugcSetSnapSettings(UGCGenesisMissionChallenge* challengeAccum, UGCComponent *pComponent, UGCMapType mapType, UGCComponent *pRoomParent)
{
	switch (pComponent->sPlacement.eSnap)
	{
		case COMPONENT_HEIGHT_SNAP_LEGACY:
			challengeAccum->bSnapRayCast = (mapType == UGC_MAP_TYPE_PREFAB_GROUND || mapType == UGC_MAP_TYPE_PREFAB_INTERIOR);
			challengeAccum->bLegacyHeightCheck = true;
			break;
			
		case COMPONENT_HEIGHT_SNAP_GEOMETRY:
			challengeAccum->bSnapRayCast = (mapType == UGC_MAP_TYPE_PREFAB_GROUND || mapType == UGC_MAP_TYPE_PREFAB_INTERIOR);
			challengeAccum->bSnapToGeo = true;
			challengeAccum->bLegacyHeightCheck = true;
			break;
			
		case COMPONENT_HEIGHT_SNAP_ABSOLUTE:
			challengeAccum->bAbsolutePos = true;
			break;
			
		case COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE:
			challengeAccum->bAbsolutePos = true;
			if (pRoomParent)
				challengeAccum->vPosition[1] += pRoomParent->sPlacement.vPos[1];
			break;

		case COMPONENT_HEIGHT_SNAP_ROOM_PARENTED:
			// Theoretically we have a parent group specified. So no raycasting or absolute positioning
			break;
			
		case COMPONENT_HEIGHT_SNAP_TERRAIN:
			challengeAccum->bSnapRayCast = true;
			challengeAccum->bSnapNormal = pComponent->sPlacement.bSnapNormal;
			break;

		case COMPONENT_HEIGHT_SNAP_WORLDGEO:
			challengeAccum->bSnapRayCast = true;
			challengeAccum->bSnapToGeo = true;
			challengeAccum->bSnapNormal = pComponent->sPlacement.bSnapNormal;
			break;
	}
}

static void ugcComponentGeneratePatrol( UGCGenesisMissionChallenge* challengeAccum, UGCProjectData* ugcProj, UGCComponent* component )
{
	if (eaiSize(&component->eaPatrolPoints) > 0)
	{
		int point_idx;
		UGCComponentPatrolPath *path = ugcComponentGetPatrolPath( ugcProj, component, NULL );
		challengeAccum->pPatrol = StructCreate(parse_WorldPatrolProperties);
		challengeAccum->pPatrol->route_type = path->patrolType;

		for (point_idx = 0; point_idx < eaSize(&path->points); point_idx++)
		{
			WorldPatrolPointProperties *point;
			if( path->points[point_idx]->prevConnectionInvalid || path->points[point_idx]->nextConnectionInvalid ) {
				challengeAccum->pPatrol->route_type = PATROL_ONEWAY;
				break;
			}

			point = StructCreate(parse_WorldPatrolPointProperties);
			copyVec3(path->points[point_idx]->pos, point->pos);
			eaPush(&challengeAccum->pPatrol->patrol_points, point);
		}

		StructDestroy(parse_UGCComponentPatrolPath, path);
	}
}

static void ugcMapTypeDefaultDetailObject( UGCMapType mapType, char* buffer, int buffer_size )
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	
	if( mapType == UGC_MAP_TYPE_SPACE || mapType == UGC_MAP_TYPE_PREFAB_SPACE ) {
		sprintf_s( SAFESTR2( buffer ), "%s", defaults->pcSpaceDetailObject );
	} else {
		sprintf_s( SAFESTR2( buffer ), "%s", defaults->pcInteriorDetailObject );
	}
}

static void ugcMapTypeDefaultKillObject( UGCMapType mapType, char* buffer, int buffer_size )
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	
	if( mapType == UGC_MAP_TYPE_SPACE || mapType == UGC_MAP_TYPE_PREFAB_SPACE ) {
		sprintf_s( SAFESTR2( buffer ), "%s", defaults->pcSpaceKillObject );
	} else {
		sprintf_s( SAFESTR2( buffer ), "%s", defaults->pcInteriorKillObject );
	}
}

static void ugcMapTypeDefaultDoorObject( UGCMapType mapType, char* buffer, int buffer_size )
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	sprintf_s( SAFESTR2( buffer ), "%s", defaults->pcInteriorDoorObject );
}

static bool ugcComponentGenerateChallengeMaybe( UGCGenesisMissionDescription *missionAccum, UGCGenesisMissionDescription *completedMissionAccum, UGCGenesisMissionChallenge ***sharedChallengeAccum, UGCProjectData* data, UGCBacklinkTable* pBacklinkTable, UGCComponent* component, UGCMissionObjective** objectives, UGCMap *map, UGCGenesisMissionObjective*** out_fsmObjectives )
{
	UGCGenesisMissionChallenge* challengeAccum;
	UGCMapType mapType = ugcMapGetType(map);
	bool challengeIsShared = false;
	UGCMissionMapLink* mapLink = ugcMissionFindLinkByExitComponent( data, component->uID );
	UGCComponent *roomParent = ugcComponentGetRoomParent( data->components, component);

	if(   component->eType == UGC_COMPONENT_TYPE_ACTOR
		  || component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP
		  || component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE
		  || component->eType == UGC_COMPONENT_TYPE_PATROL_POINT
		  || component->eType == UGC_COMPONENT_TYPE_TRAP_TARGET
		  || component->eType == UGC_COMPONENT_TYPE_TRAP_TRIGGER
		  || component->eType == UGC_COMPONENT_TYPE_TRAP_EMITTER
		  || component->eType == UGC_COMPONENT_TYPE_TELEPORTER_PART
		  || component->eType == UGC_COMPONENT_TYPE_CLUSTER) {
		return false;
	}
	
	challengeAccum = StructCreate( parse_UGCGenesisMissionChallenge );
	challengeAccum->pcLayoutName = StructAllocString( GENESIS_UGC_LAYOUT_NAME );
	challengeAccum->pcName = StructAllocString( ugcComponentGetLogicalNameTemp(component) );
	challengeAccum->iCount = 1;
	challengeAccum->bForceNamedObject = (ugcMissionFindLinkByExitComponent( data, component->uID ) != NULL);

	copyVec3( component->sPlacement.vPos, challengeAccum->vPosition );
	copyVec3( component->sPlacement.vRotPYR, challengeAccum->vRotation );

	ugcSetSnapSettings(challengeAccum, component, mapType, roomParent);

	// Handle MissionReturn doors
	if( ugcDefaultsMissionReturnEnabled() && component->bInteractIsMissionReturn ) {
		missionAccum->zoneDesc.startDescription.eExitFrom = UGCGenesisMissionExitFrom_Challenge;
		eaPush( &missionAccum->zoneDesc.startDescription.eaExitChallenges, StructAllocString( challengeAccum->pcName ));
	}

	switch( component->eType ) {
		case UGC_COMPONENT_TYPE_SPAWN:
			challengeAccum->eType = GenesisChallenge_None;
			challengeAccum->pcChallengeName = StructAllocString( "Goto Spawn Point" );
			break;

		case UGC_COMPONENT_TYPE_RESPAWN:
			challengeAccum->eType = GenesisChallenge_None;
			challengeAccum->pcChallengeName = StructAllocString( "UGC_Respawn_Point_With_Campfire" );
			break;

		case UGC_COMPONENT_TYPE_COMBAT_JOB:
			challengeAccum->eType = GenesisChallenge_None;
			{
				char buffer[ RESOURCE_NAME_MAX_SIZE ];
				if( component->iObjectLibraryId ) {
					sprintf( buffer, "%d", component->iObjectLibraryId );
				} else {
					ugcMapTypeDefaultDetailObject( mapType, SAFESTR( buffer ));
				}
				challengeAccum->pcChallengeName = StructAllocString( buffer );
			}
			break;
		case UGC_COMPONENT_TYPE_PLANET:
			challengeAccum->eType = GenesisChallenge_Clickie;
			{
				char buffer[ RESOURCE_NAME_MAX_SIZE ];
				if( component->iObjectLibraryId ) {
					sprintf( buffer, "%d", component->iObjectLibraryId );
				} else {
					ugcMapTypeDefaultDetailObject( mapType, SAFESTR( buffer ));
				}
				challengeAccum->pcChallengeName = StructAllocString( buffer );
			}
			ugcComponentSetDetails(component, challengeAccum);
			challengeAccum->pVolume = StructCreate(parse_UGCGenesisObjectVolume);
			challengeAccum->pVolume->is_relative = true;
			challengeAccum->pVolume->size = 0.8f;

			ugcComponentApplyWhen( NULL, &challengeAccum->clickieVisibleWhen, component, objectives, data->components, out_fsmObjectives );

			if (component->iPlanetRingId)
			{
				char name_buf[256];
				UGCGenesisMissionChallenge* challengeRingAccum = StructClone(parse_UGCGenesisMissionChallenge, challengeAccum);
				assert(challengeRingAccum);
				sprintf(name_buf, "%s_RING", challengeAccum->pcName);
				challengeRingAccum->pcName = StructAllocString( name_buf );
				{
					char buffer[ RESOURCE_NAME_MAX_SIZE ];
					sprintf( buffer, "%d", component->iPlanetRingId );
					challengeAccum->pcChallengeName = StructAllocString( buffer );
				}
				eaPush( &missionAccum->eaChallenges, challengeRingAccum );

				if( !component->pHideWhen || component->pHideWhen->eType == UGCWHEN_MANUAL ) {
					challengeRingAccum = StructClone(parse_UGCGenesisMissionChallenge, challengeRingAccum);
					StructReset( parse_UGCGenesisWhen, &challengeRingAccum->spawnWhen );
					StructReset( parse_UGCGenesisWhen, &challengeRingAccum->clickieVisibleWhen );
					eaPush( &completedMissionAccum->eaChallenges, challengeRingAccum );
				}
			}
			break;
		case UGC_COMPONENT_TYPE_FAKE_DOOR:
			challengeAccum->eType = GenesisChallenge_Clickie;
			{
				GroupDef *door_group = objectLibraryGetGroupDef(component->iObjectLibraryId, false);
				char buffer[ RESOURCE_NAME_MAX_SIZE ];

				// Prevent crashing on bad master doors
				if (!door_group || eaSize(&door_group->children) != UGC_OBJLIB_ROOMDOOR_NUM_CHILDREN)
					ugcMapTypeDefaultDoorObject( mapType, SAFESTR( buffer ));
				else
					sprintf( buffer, "%d", door_group->children[UGC_OBJLIB_ROOMDOOR_FAKE_CHILD]->name_uid );
				
				challengeAccum->pcChallengeName = StructAllocString( buffer );
			}

			ugcComponentSetChallengeClickyParams(data, pBacklinkTable, component, challengeAccum);
			ugcComponentApplyWhen( &challengeAccum->spawnWhen, NULL, component, objectives, data->components, out_fsmObjectives );
			break;

		case UGC_COMPONENT_TYPE_ROOM_DOOR: {
			char locked_obj_name[ RESOURCE_NAME_MAX_SIZE ];
			char unlocked_obj_name[ RESOURCE_NAME_MAX_SIZE ];
			UGCMissionObjective* objective = ugcObjectiveFindComponent( data->mission->objectives, component->uID );

			challengeAccum->ePathNodesFrom = UGCGenesisPathNodesFrom_ChallengePos;
			FOR_EACH_IN_EARRAY( data->components->eaComponents, UGCComponent, otherComponent ) {
				if( ugcRoomIsDoorConnected( otherComponent, component, NULL )) {
					eaPush( &challengeAccum->eastrPathNodesAutoconnectChallenge,
							StructAllocString( ugcComponentGetLogicalNameTemp( otherComponent )));
				}
			} FOR_EACH_END;

			if( component->iObjectLibraryId ) {
				GroupDef *door_group = objectLibraryGetGroupDef(component->iObjectLibraryId, false);
				
				// Prevent crashing on bad master doors
				if (!door_group || eaSize(&door_group->children) != UGC_OBJLIB_ROOMDOOR_NUM_CHILDREN)
				{
					sprintf( locked_obj_name, "%d", component->iObjectLibraryId );
					sprintf( unlocked_obj_name, "%d", component->iObjectLibraryId );
				}
				else
				{
					sprintf( locked_obj_name, "%d", door_group->children[UGC_OBJLIB_ROOMDOOR_LOCKED_CHILD]->name_uid );
					sprintf( unlocked_obj_name, "%d", door_group->children[UGC_OBJLIB_ROOMDOOR_UNLOCKED_CHILD]->name_uid );
				}
			} else {
				sprintf( locked_obj_name, "" );
				sprintf( unlocked_obj_name, "" );
			}

			if (objective)
			{
				// Locked -> unlocked, using Interact When
				challengeAccum->pcChallengeName = StructAllocString( unlocked_obj_name );

				ugcComponentSetChallengeClickyParams(data, pBacklinkTable, component, challengeAccum);
				challengeAccum->pClickie->bIsUGCDoor = true;
				ugcComponentApplyWhen( &challengeAccum->spawnWhen, NULL, component, objectives, data->components, out_fsmObjectives );
			}
			else
			{
				// Locked -> unlocked -> locked using When Sequence
				UGCWhen **whenList = NULL;
				UGCGenesisMissionChallenge **challengeList = NULL;
				UGCGenesisMissionChallenge *challengeUnlockedDoor, *challengeRelockedDoor;
				char name_buf[256];

				challengeUnlockedDoor = StructClone(parse_UGCGenesisMissionChallenge, challengeAccum);

				challengeAccum->pcChallengeName = StructAllocString( locked_obj_name );

				challengeUnlockedDoor->pcChallengeName = StructAllocString( unlocked_obj_name );
				sprintf(name_buf, "%s_UNLOCKED", challengeAccum->pcName);
				StructCopyString(&challengeUnlockedDoor->pcName, name_buf );
				eaPush(&missionAccum->eaChallenges, challengeUnlockedDoor);

				challengeRelockedDoor = StructClone(parse_UGCGenesisMissionChallenge, challengeAccum);
				sprintf(name_buf, "%s_RELOCKED", challengeAccum->pcName);
				StructCopyString(&challengeRelockedDoor->pcName, name_buf );
				eaPush(&missionAccum->eaChallenges, challengeRelockedDoor);

				eaStackCreate(&whenList, 2);
				eaStackCreate(&challengeList, 3);

				// Locked, then unlocked, then locked
				eaPush(&challengeList, challengeAccum);
				eaPush(&challengeList, challengeUnlockedDoor);
				eaPush(&challengeList, challengeRelockedDoor);

				ugcComponentSetChallengeClickyParams(data, pBacklinkTable, component, challengeUnlockedDoor);
				if (challengeUnlockedDoor->pClickie)
					challengeUnlockedDoor->pClickie->bIsUGCDoor = true;

				eaPush(&whenList, component->pStartWhen);
				eaPush(&whenList, component->pHideWhen);
				ugcComponentApplyChallengeWhenSequence(whenList, challengeList, component->uID, objectives, data->components, out_fsmObjectives);
			}
		}
			break;

		case UGC_COMPONENT_TYPE_OBJECT: case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED: case UGC_COMPONENT_TYPE_CLUSTER_PART: {
			bool force_non_shared = false;

			{
				char buffer[ RESOURCE_NAME_MAX_SIZE ];
				if( component->iObjectLibraryId ) {
					sprintf( buffer, "%d", component->iObjectLibraryId );
				} else {
					ugcMapTypeDefaultDetailObject( mapType, SAFESTR( buffer ));
				}
				challengeAccum->pcChallengeName = StructAllocString( buffer );
			}
			challengeAccum->iPlatformGroup = component->uID;
			ugcComponentSetDetails(component, challengeAccum);

			if( ugcComponentSetChallengeClickyParams(data, pBacklinkTable, component, challengeAccum)) {
				force_non_shared = true;
			}
			if( ugcComponentFindPromptForID( data->components, component->uID )) {
				force_non_shared = true;
			}

			if (  !ugcComponentApplyWhen( &challengeAccum->spawnWhen, &challengeAccum->clickieVisibleWhen, component, objectives, data->components, out_fsmObjectives )
				  && !mapLink && !force_non_shared
				  && !component->bInteractIsMissionReturn) {
				challengeIsShared = true;
			}
		}
		break;

		case UGC_COMPONENT_TYPE_SOUND: {
			UGCSound *sound = RefSystem_ReferentFromString("UGCSound", component->strSoundEvent);
			if(sound)
				challengeAccum->astrObjectSoundEvent = allocAddString(sound->strSoundName);

			challengeAccum->iPlatformGroup = component->uID;

			if(!ugcComponentApplyWhen( &challengeAccum->spawnWhen, &challengeAccum->clickieVisibleWhen, component, objectives, data->components, out_fsmObjectives ))
				challengeIsShared = true;
		}
		break;

		case UGC_COMPONENT_TYPE_TELEPORTER: {
			{
				char buffer[ RESOURCE_NAME_MAX_SIZE ];
				sprintf( buffer, "%d", component->iObjectLibraryId );
				challengeAccum->pcChallengeName = StructAllocString( buffer );
			}
			challengeAccum->iPlatformGroup = component->uID;
			ugcComponentSetDetails(component, challengeAccum);

			challengeAccum->ePathNodesFrom = UGCGenesisPathNodesFrom_GroupDef;
			challengeAccum->bPathNodesAutoconnectNearest = true;

			// figure out the child placements
			challengeAccum->bChildrenAreGroupDefs = true;
			{
				int it;
				for( it = 0; it != eaiSize( &component->uChildIDs ); ++it ) {
					UGCComponent* child = ugcComponentFindByID( data->components, component->uChildIDs[ it ]);
					UGCGenesisPlacementChildParams* posData = StructCreate( parse_UGCGenesisPlacementChildParams );

					StructCopyString( &posData->pcLogicalName, ugcComponentGetLogicalNameTemp( child ));
					subVec3( child->sPlacement.vPos, component->sPlacement.vPos, posData->vOffset );
					copyVec3( child->sPlacement.vRotPYR, posData->vPyr );
					eaPush( &challengeAccum->eaChildren, posData );
				}
			}
		}
		break;

		case UGC_COMPONENT_TYPE_REWARD_BOX:
		{
			challengeAccum->eType = GenesisChallenge_None;
			challengeAccum->pcChallengeName = StructAllocString( ugcDefaultsGetRewardBoxObjlib() );
		}
		break;

		case UGC_COMPONENT_TYPE_TRAP: {
			GroupDef *def = objectLibraryGetGroupDef(component->iObjectLibraryId, false);
			UGCTrapProperties *properties = def ? ugcTrapGetProperties(def) : NULL;

			challengeAccum->eType = GenesisChallenge_None;
			{
				char buffer[ RESOURCE_NAME_MAX_SIZE ];
				sprintf( buffer, "%d", component->iObjectLibraryId );
				challengeAccum->pcChallengeName = StructAllocString( buffer );
			}

			if (properties)
			{
				int emitter_idx, child_idx;
				UGCGenesisMissionChallenge *challengeTrapArmed = NULL;

				// Create a trigger for the trap
				if (properties->pSelfContained)
				{
					challengeTrapArmed = challengeAccum;
					ugcComponentApplyTrapWhens( NULL, &challengeTrapArmed->clickieVisibleWhen, NULL, component, objectives, data->components, out_fsmObjectives );

					// Add the volume that triggers the actual trap
					challengeTrapArmed->pVolume = StructCreate(parse_UGCGenesisObjectVolume);
					challengeTrapArmed->pVolume->pVolumeProperties = StructClone(parse_GroupVolumeProperties, properties->pSelfContained->pVolume);
					challengeTrapArmed->pcTrapObjective = challengeTrapArmed->clickieVisibleWhen.eaObjectiveNames ? StructAllocString(challengeTrapArmed->clickieVisibleWhen.eaObjectiveNames[0]) : NULL;
				}
				else
				{
					UGCComponent *trigger_component = NULL;
					UGCComponent *emitter_component = NULL;

					// Find trigger component, if any
					for (child_idx = 0; child_idx < eaiSize(&component->uChildIDs); child_idx++)
					{
						UGCComponent *child_component = ugcComponentFindByID(data->components, component->uChildIDs[child_idx]);
						if (child_component && child_component->eType == UGC_COMPONENT_TYPE_TRAP_TRIGGER)
						{
							trigger_component = child_component;
						}
						if (child_component && child_component->eType == UGC_COMPONENT_TYPE_TRAP_EMITTER)
						{
							emitter_component = child_component;
						}
					}

					if (emitter_component)
					{
						// Move generated challenge to emitter's location
						copyVec3( emitter_component->sPlacement.vPos, challengeAccum->vPosition );
						copyVec3( emitter_component->sPlacement.vRotPYR, challengeAccum->vRotation );

						ugcSetSnapSettings(challengeAccum, emitter_component, mapType, roomParent);
					}
					if (trigger_component)
					{
						char name_buf[256];
						UGCGenesisMissionChallenge *challengeTrapUnarmed;
						UGCGenesisMissionChallenge *challengeTrapDisarmed;

						// Place "Unarmed" trap piece
						challengeTrapUnarmed = StructClone(parse_UGCGenesisMissionChallenge, challengeAccum);
						challengeTrapUnarmed->pcChallengeName = StructAllocString("UGC_Trap_Unarmed");
						sprintf(name_buf, "%s_UNARMED", challengeAccum->pcName);
						StructCopyString(&challengeTrapUnarmed->pcName, name_buf );
						copyVec3(trigger_component->sPlacement.vPos, challengeTrapUnarmed->vPosition);
						copyVec3(trigger_component->sPlacement.vRotPYR, challengeTrapUnarmed->vRotation);
						eaPush(&missionAccum->eaChallenges, challengeTrapUnarmed);

						// Place "Armed" trap piece
						challengeTrapArmed = StructClone(parse_UGCGenesisMissionChallenge, challengeAccum);
						challengeTrapArmed->pcChallengeName = StructAllocString("UGC_Trap_Core");
						sprintf(name_buf, "%s_ARMED", challengeAccum->pcName);
						StructCopyString(&challengeTrapArmed->pcName, name_buf );
						copyVec3(trigger_component->sPlacement.vPos, challengeTrapArmed->vPosition);
						copyVec3(trigger_component->sPlacement.vRotPYR, challengeTrapArmed->vRotation);
						eaPush(&missionAccum->eaChallenges, challengeTrapArmed);

						// Place "Disarmed" trap piece
						challengeTrapDisarmed = StructClone(parse_UGCGenesisMissionChallenge, challengeAccum);
						challengeTrapDisarmed->pcChallengeName = StructAllocString("UGC_Trap_Disarmed");
						sprintf(name_buf, "%s_DISARMED", challengeAccum->pcName);
						StructCopyString(&challengeTrapDisarmed->pcName, name_buf );
						copyVec3(trigger_component->sPlacement.vPos, challengeTrapDisarmed->vPosition);
						copyVec3(trigger_component->sPlacement.vRotPYR, challengeTrapDisarmed->vRotation);
						eaPush(&missionAccum->eaChallenges, challengeTrapDisarmed);


						ugcComponentApplyTrapWhens( &challengeTrapUnarmed->clickieVisibleWhen, &challengeTrapArmed->clickieVisibleWhen, &challengeTrapDisarmed->clickieVisibleWhen, component, objectives, data->components, out_fsmObjectives );

						// Add the volume that triggers the actual trap
						challengeTrapArmed->pVolume = StructCreate(parse_UGCGenesisObjectVolume);
						challengeTrapArmed->pVolume->is_square = true;
						challengeTrapArmed->pVolume->is_relative = true;
						challengeTrapArmed->pVolume->size = 1.0f;
						challengeTrapArmed->pcTrapObjective = challengeTrapArmed->clickieVisibleWhen.eaObjectiveNames ? StructAllocString(challengeTrapArmed->clickieVisibleWhen.eaObjectiveNames[0]) : 0;
					}
				}

				// Create emitter/target pairs
				for (emitter_idx = 0; emitter_idx < eaSize(&properties->eaEmitters); emitter_idx++)
				{
					if (properties->eaEmitters[emitter_idx])
					{
						UGCComponent *target_component = NULL;
						for (child_idx = 0; child_idx < eaiSize(&component->uChildIDs); child_idx++)
						{
							UGCComponent *child_component = ugcComponentFindByID(data->components, component->uChildIDs[child_idx]);
							if (child_component && child_component->eType == UGC_COMPONENT_TYPE_TRAP_TARGET && child_component->iTrapEmitterIndex == emitter_idx)
							{
								target_component = child_component;
								break;
							}
						}
						if (target_component)
						{
							char name_buf[128];
							UGCGenesisMissionChallenge *challengeTrapEmitterPoint, *challengeTrapTargetPoint;
							
							// Create "Emitter" named point
							challengeTrapEmitterPoint = StructClone( parse_UGCGenesisMissionChallenge, challengeAccum );
							assert(challengeTrapEmitterPoint);
							StructFreeString(challengeTrapEmitterPoint->pcName);
							sprintf(name_buf, "%s_EMITTER_%d", challengeAccum->pcName, emitter_idx);
							challengeTrapEmitterPoint->pcName = StructAllocString(name_buf);
							addVec3(properties->eaEmitters[emitter_idx]->pos, challengeTrapEmitterPoint->vPosition, challengeTrapEmitterPoint->vPosition);
							StructReset(parse_UGCGenesisWhen, &challengeTrapEmitterPoint->spawnWhen);
							StructReset(parse_UGCGenesisWhen, &challengeTrapEmitterPoint->clickieVisibleWhen);
							challengeTrapEmitterPoint->pcChallengeName = StructAllocString("Named Point");
							eaPush(&missionAccum->eaChallenges, challengeTrapEmitterPoint);
							eaPush(&completedMissionAccum->eaChallenges, StructClone(parse_UGCGenesisMissionChallenge, challengeTrapEmitterPoint));

							// Create "Target" named point
							challengeTrapTargetPoint = StructClone( parse_UGCGenesisMissionChallenge, challengeAccum );
							assert(challengeTrapTargetPoint);
							StructFreeString(challengeTrapTargetPoint->pcName);
							sprintf(name_buf, "%s_TARGET_%d", challengeAccum->pcName, emitter_idx);
							challengeTrapTargetPoint->pcName = StructAllocString(name_buf);
							copyVec3(target_component->sPlacement.vPos, challengeTrapTargetPoint->vPosition);
							challengeTrapTargetPoint->vPosition[1] += properties->eaEmitters[emitter_idx]->pos[1]; // For components, set height same as emitter
							StructReset(parse_UGCGenesisWhen, &challengeTrapTargetPoint->spawnWhen);
							StructReset(parse_UGCGenesisWhen, &challengeTrapTargetPoint->clickieVisibleWhen);
							challengeTrapTargetPoint->pcChallengeName = StructAllocString("Named Point");
							eaPush(&missionAccum->eaChallenges, challengeTrapTargetPoint);
							eaPush(&completedMissionAccum->eaChallenges, StructClone(parse_UGCGenesisMissionChallenge, challengeTrapTargetPoint));

							if (challengeTrapArmed)
							{
								// Add a trap trigger for the emitter/target pair
								UGCGenesisMissionTrap *trap = StructCreate(parse_UGCGenesisMissionTrap);
								trap->bOnVolumeEntered = true;
								trap->pcPowerName = StructAllocString(component->pcTrapPower);
								trap->pcEmitterChallenge = StructAllocString(challengeTrapEmitterPoint->pcName);
								trap->pcTargetChallenge = StructAllocString(challengeTrapTargetPoint->pcName);
								eaPush(&challengeTrapArmed->eaTraps, trap);
							}
						}
					}
				}

				StructDestroy(parse_UGCTrapProperties, properties);
			}
		}
		break;

		case UGC_COMPONENT_TYPE_ROOM:
		{
			UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);
			UGCSound *sound = RefSystem_ReferentFromString("UGCSound", component->strSoundEvent);
			UGCSoundDSP *dsp = RefSystem_ReferentFromString("UGCSoundDSP", component->strSoundDSP);
			int detail_idx = 0;
			challengeAccum->eType = GenesisChallenge_None;
			{
				char buffer[ RESOURCE_NAME_MAX_SIZE ];
				sprintf( buffer, "%d", component->iObjectLibraryId );
				challengeAccum->pcChallengeName = StructAllocString( buffer );
			}
			challengeAccum->iPlatformGroup = component->uID;
			challengeAccum->ePathNodesFrom = UGCGenesisPathNodesFrom_GroupDef;

			if(sound)
				challengeAccum->astrRoomToneSoundEvent = allocAddString(sound->strSoundName);
			if(dsp)
				challengeAccum->astrRoomSoundDSP = allocAddString(dsp->strSoundDSPName);

			ugcComponentSetDetails(component, challengeAccum);
			if (room_info)
			{
				int door_id;
				for (door_id = 0; door_id < eaSize(&room_info->doors); door_id++)
				{
					UGCComponent *door_component = NULL;
					int *door_types = NULL;
					int door_type_id = -1;
					UGCRoomDoorInfo *door = room_info->doors[door_id];
					UGCGenesisRoomDoorSwitch *detail = StructCreate(parse_UGCGenesisRoomDoorSwitch);
					UGCDoorSlotState state = ugcRoomGetDoorSlotState(data->components, component, door_id, &door_component, &door_types, NULL, NULL);

					if( state == UGC_DOOR_SLOT_OCCUPIED ) {
						assert(door_component);
						door_type_id = ugcRoomDoorGetTypeID( door_component, door_types );
					}

					detail->iIndex = detail_idx++;
					if (door_type_id < 0)
					{
						detail->iSelected = 0; // "Closed"
					}
					else
					{
						// Look for the door tag on the room door
						int type_idx;
						for (type_idx = 0; type_idx < eaiSize(&door->eaiDoorTypeIDs); type_idx++)
						{
							if (door->eaiDoorTypeIDs[type_idx] == door_type_id)
							{
								detail->iSelected = type_idx + 1;
								break;
							}
						}
					}
					detail->astrScopePath = door->astrScopePath;
					eaPush(&challengeAccum->eaRoomDoors, detail);

					eaiDestroy(&door_types);
				}
			}

			challengeIsShared = true;
		}
		break;

		case UGC_COMPONENT_TYPE_DESTRUCTIBLE:
			challengeAccum->eType = GenesisChallenge_Destructible;
			{
				char buffer[ RESOURCE_NAME_MAX_SIZE ];
				sprintf( buffer, "%d", component->iObjectLibraryId );
				challengeAccum->pcChallengeName = StructAllocString( buffer );
			}
			challengeAccum->iPlatformGroup = component->uID;
			ugcComponentSetDetails(component, challengeAccum);
			break;

		case UGC_COMPONENT_TYPE_KILL:
			challengeAccum->eType = GenesisChallenge_Encounter2;
			{
				char buffer[ RESOURCE_NAME_MAX_SIZE ];
				if( component->iObjectLibraryId ) {
					sprintf( buffer, "%d", component->iObjectLibraryId );
				} else {
					ugcMapTypeDefaultKillObject( mapType, SAFESTR( buffer ));
				}
				challengeAccum->pcChallengeName = StructAllocString( buffer );
			}
			ugcComponentApplyWhen( NULL, &challengeAccum->spawnWhen, component, objectives, data->components, out_fsmObjectives );

			if(component->uChildIDs)
			{
				int i;
				for(i=0; i<ea32Size(&component->uChildIDs); i++)
				{
					Vec3 offset;
					UGCComponent *actor = ugcComponentFindByID(data->components, component->uChildIDs[i]);
					UGCGenesisPlacementChildParams *actorData = StructCreate(parse_UGCGenesisPlacementChildParams);
					F32 dist;
					F32 angle;

					actorData->vPyr[1] = subAngle(RAD(actor->sPlacement.vRotPYR[1]), RAD(component->sPlacement.vRotPYR[1]));
					subVec3(actor->sPlacement.vPos, component->sPlacement.vPos, offset);

					dist = normalVec3(offset);
					angle = getVec3Yaw(offset);
					angle = subAngle(angle, RAD(component->sPlacement.vRotPYR[1]));  // find the difference

					actorData->vOffset[0] = sinf(angle) * dist;
					actorData->vOffset[2] = cosf(angle) * dist;

					{
						char actorName[ 256 ];
						ugcComponentGetDisplayName( actorName, data, actor, true );
						actorData->actor_params.pcActorName = ugcAllocSMFString( actorName, false );
						
						actorData->actor_params.pcActorCritterGroupName = ugcAllocSMFString( actor->pcActorCritterGroupName, false );
					}

					if(!nullStr(actor->pcCostumeName))
					{
						actorData->actor_params.costume = StructCreate(parse_WorldActorCostumeProperties);
						SET_HANDLE_FROM_STRING( "PlayerCostume", ugcCostumeHandleString(data, actor->pcCostumeName), actorData->actor_params.costume->hCostume );
					}
					
					actorData->is_actor = true;

					eaPush(&challengeAccum->eaChildren, actorData);
				}
			}

			ugcComponentGeneratePatrol( challengeAccum, data, component );
			break;

		case UGC_COMPONENT_TYPE_ROOM_MARKER:
			challengeAccum->eType = GenesisChallenge_None;
			challengeAccum->pcChallengeName = StructAllocString( "Goto Spawn Point" );
			challengeAccum->pVolume = StructCreate(parse_UGCGenesisObjectVolume);
			challengeAccum->pVolume->size = component->fVolumeRadius;
			{
				UGCMissionObjective* objective = ugcObjectiveFindComponent( data->mission->objectives, component->uID );
				if( !objective && ugcBacklinkTableFindTrigger( pBacklinkTable, component->uID, 0 )) {
					if (eaSize(&component->eaTriggerGroups) > 0)
					{
						challengeAccum->succeedCheckedAttrib = StructClone( parse_UGCCheckedAttrib, component->eaTriggerGroups[0]->succeedCheckedAttrib );
					}
				}
			}
			break;

		case UGC_COMPONENT_TYPE_CONTACT: 
			challengeAccum->eType = GenesisChallenge_Contact;
			if (mapType == UGC_MAP_TYPE_PREFAB_SPACE || mapType == UGC_MAP_TYPE_SPACE)
				challengeAccum->pcChallengeName = StructAllocString( "Ugc_Contact_Space_Objlib" );
			else
				challengeAccum->pcChallengeName = StructAllocString( "Ugc_Contact_Objlib" );
			ugcComponentApplyWhen( NULL, &challengeAccum->spawnWhen, component, objectives, data->components, out_fsmObjectives );
			
			challengeAccum->pContact = StructCreate( parse_UGCGenesisContactParams );

			if(!nullStr(component->pcCostumeName) || !nullStr(component->pcVisibleName))
			{
				UGCGenesisPlacementChildParams *actorData = StructCreate(parse_UGCGenesisPlacementChildParams);

				{
					char actorName[256];
					ugcComponentGetDisplayName( actorName, data, component, true );
					actorData->actor_params.pcActorName = ugcAllocSMFString( actorName, false );
				}

				if(!nullStr(component->pcCostumeName))
				{
					actorData->actor_params.costume = StructCreate(parse_WorldActorCostumeProperties);
					SET_HANDLE_FROM_STRING( "PlayerCostume", ugcCostumeHandleString(data, component->pcCostumeName), actorData->actor_params.costume->hCostume );
				}

				actorData->is_actor = true;

				eaPush(&challengeAccum->eaChildren, actorData);
			}

			ugcComponentGeneratePatrol( challengeAccum, data, component );
			break;

		default:
			devassertmsg( false, "Unhandled component type" );
	}

	{
		UGCComponent *parent = ugcComponentFindByID(data->components, component->uParentID);
		if (parent)
		{
			challengeAccum->iPlatformParentGroup = parent->uID;
			challengeAccum->iPlatformParentLevel = component->sPlacement.iRoomLevel;
		}
	}

	if (challengeIsShared)
	{
		eaPush( sharedChallengeAccum, challengeAccum );
	}
	else
	{
		eaPush( &missionAccum->eaChallenges, challengeAccum );

		if( component->eType != UGC_COMPONENT_TYPE_KILL && (!component->pHideWhen || component->pHideWhen->eType == UGCWHEN_MANUAL) && !component->visibleCheckedAttrib.astrItemName && !component->visibleCheckedAttrib.astrSkillName ) {
			UGCGenesisMissionChallenge *challengeWhenCompleted = StructClone( parse_UGCGenesisMissionChallenge, challengeAccum );
			assert(challengeWhenCompleted);
			StructReset( parse_UGCGenesisWhen, &challengeWhenCompleted->spawnWhen );
			StructReset( parse_UGCGenesisWhen, &challengeWhenCompleted->clickieVisibleWhen );
			StructDestroySafe( parse_UGCGenesisMissionChallengeClickie, &challengeWhenCompleted->pClickie );
			challengeWhenCompleted->succeedCheckedAttrib = NULL;
			StructFreeStringSafe(&challengeWhenCompleted->pcTrapObjective);
			eaDestroyStruct(&challengeWhenCompleted->eaTraps, parse_UGCGenesisMissionTrap);
			eaPush( &completedMissionAccum->eaChallenges, challengeWhenCompleted );
		}
	}

	return true;
}

////////////////////////////////////////////////////////////
// Prompt Generation
////////////////////////////////////////////////////////////

static void ugcComponentGeneratePromptPushUsedObjectives( UGCGenesisMissionPrompt* promptAccum, UGCProjectData* ugcProj, UGCComponent* contact, UGCMissionObjective** missionObjectives )
{
	int it;
	for( it = 0; it != eaSize( &missionObjectives ); ++it ) {
		UGCMissionObjective* objective = missionObjectives[ it ];

		if( objective->type == UGCOBJ_ALL_OF || objective->type == UGCOBJ_IN_ORDER ) {
			ugcComponentGeneratePromptPushUsedObjectives( promptAccum, ugcProj, contact, objective->eaChildren );
		} else if( ugcComponentFindPromptByContactAndObjective( ugcProj->components, contact->uID, objective->id )) {
			eaPush( &promptAccum->showWhen.eaObjectiveNames,
					StructAllocString( ugcMissionObjectiveIDLogicalNameTemp( objective->id )));
		}
	}
}

UGCGenesisMissionPrompt* ugcComponentGeneratePromptMaybe( UGCComponent* prompt, UGCProjectData* ugcProj, UGCMissionObjective** objectives, const char* mapName, UGCGenesisMissionObjective*** out_fsmObjectives )
{
	UGCComponent* contact = ugcComponentFindByID( ugcProj->components, prompt->uActorID );
	UGCGenesisMissionPrompt* promptAccum = NULL;
	bool isInStoryView = (prompt->sPlacement.pcMapName == NULL);

	// Generation is all based off of the first block, don't generate if this isn't the first block. 
	if( prompt->dialogBlock.blockIndex > 0 ) {
		return NULL;
	}

	if( !isInStoryView ) {
		// PLACED IN MAP EDITOR
		if( ugcComponentIsOnMap( prompt, mapName, false )) {
			promptAccum = StructCreate( parse_UGCGenesisMissionPrompt );
			ugcWhenMakeGenesisWhen( prompt->pStartWhen, prompt->uID, &promptAccum->showWhen, ugcProj->components, objectives );
		}
	} else {
		// PLACED IN STORY EDITOR
		if( !contact && ugcComponentStartWhenType(prompt) == UGCWHEN_OBJECTIVE_IN_PROGRESS ) {
			return NULL;
		}

		// Create prompt accum, fill out its WHEN
		if( contact && ugcComponentIsOnMap( contact, mapName, false )) {
			promptAccum = StructCreate( parse_UGCGenesisMissionPrompt );

			if( !mapName ) {
				const char* externalMapName = contact->sPlacement.pcExternalMapName;
				const char* externalObjectName = contact->sPlacement.pcExternalObjectName;
				ZoneMapEncounterObjectInfo* contactInfo = zeniObjectFind( externalMapName, externalObjectName );

				if( contactInfo && contactInfo->type == WL_ENC_ENCOUNTER ) {
					promptAccum->pcExternalContactName = contactInfo->ugcContactName;
				} else {
					eaPush( &promptAccum->eaExternalMapNames, StructAllocString( externalMapName ));
					promptAccum->pcChallengeName = StructAllocString( externalObjectName );
				}
			} else {
				promptAccum->pcChallengeName = StructAllocString( ugcComponentGetLogicalNameTemp( contact ));
			}

			if( prompt->bIsDefault ) {
				promptAccum->showWhen.bNot = true;
				promptAccum->showWhen.type = UGCGenesisWhen_ObjectiveInProgress;
				ugcComponentGeneratePromptPushUsedObjectives( promptAccum, ugcProj, contact, objectives );

				if( eaSize( &promptAccum->showWhen.eaObjectiveNames ) == 0 ) {
					eaDestroyEx( &promptAccum->showWhen.eaObjectiveNames, NULL );
					promptAccum->showWhen.bNot = false;
					promptAccum->showWhen.type = UGCGenesisWhen_MapStart;
				}
			} else if( eaiSize( &prompt->eaObjectiveIDs )) {
				int it;

				promptAccum->showWhen.type = UGCGenesisWhen_ObjectiveInProgress;
				for( it = 0; it != eaiSize( &prompt->eaObjectiveIDs ); ++it ) {
					eaPush( &promptAccum->showWhen.eaObjectiveNames,
							StructAllocString( ugcMissionObjectiveIDLogicalNameTemp( prompt->eaObjectiveIDs[ it ])));
				}
			} else {
				StructDestroySafe( parse_UGCGenesisMissionPrompt, &promptAccum );
			}
		} else {
			if( ugcComponentStartWhenType(prompt) == UGCWHEN_MISSION_START && !mapName ) {
				promptAccum = StructCreate( parse_UGCGenesisMissionPrompt );
			} else if( ugcComponentStartWhenType(prompt) == UGCWHEN_COMPONENT_COMPLETE ) {
				int i;

				if( !ugcComponentIsOnMap( prompt, mapName, false ))
					return NULL;

				promptAccum = StructCreate( parse_UGCGenesisMissionPrompt );
				promptAccum->showWhen.type = UGCGenesisWhen_ChallengeComplete;
				for (i = 0; i < eaiSize(&prompt->pStartWhen->eauComponentIDs); i++)
				{
					UGCComponent *component = ugcComponentFindByID( ugcProj->components, prompt->pStartWhen->eauComponentIDs[i]);
					if (component)
					{
						eaPush( &promptAccum->showWhen.eaChallengeNames, StructAllocString( ugcComponentGetLogicalNameTemp( component ) ) );
					}
				}
			} else if( ugcComponentStartWhenType(prompt) == UGCWHEN_COMPONENT_REACHED ) {
				int i;
				UGCGenesisWhenRoom *room;

				if( !ugcComponentIsOnMap( prompt, mapName, false ))
					return NULL;

				room = StructCreate(parse_UGCGenesisWhenRoom);
				promptAccum = StructCreate( parse_UGCGenesisMissionPrompt );
				promptAccum->showWhen.type = UGCGenesisWhen_ReachChallenge;
				for (i = 0; i < eaiSize(&prompt->pStartWhen->eauComponentIDs); i++)
				{
					UGCComponent *component = ugcComponentFindByID( ugcProj->components, prompt->pStartWhen->eauComponentIDs[i]);
					if (component)
					{
						eaPush( &promptAccum->showWhen.eaChallengeNames, StructAllocString( ugcComponentGetLogicalNameTemp( component ) ) );
					}
				}
			} else if( ugcComponentStartWhenType(prompt) == UGCWHEN_MAP_START ) {

				if( !ugcComponentIsOnMap( prompt, mapName, false ))
					return NULL;

				promptAccum = StructCreate( parse_UGCGenesisMissionPrompt );
				promptAccum->showWhen.type = UGCGenesisWhen_MapStart;
		
			} else if( ugcComponentStartWhenType(prompt) != UGCWHEN_OBJECTIVE_IN_PROGRESS ) {
				U32 objectiveID = eaiGet( &prompt->eaObjectiveIDs, 0 );
				UGCMissionObjective* objective = ugcObjectiveFind( objectives, objectiveID );

				if( objective ) {
					promptAccum = StructCreate( parse_UGCGenesisMissionPrompt );

					if( ugcComponentStartWhenType(prompt) == UGCWHEN_OBJECTIVE_COMPLETE ) {
						promptAccum->showWhen.type = UGCGenesisWhen_ObjectiveComplete;
					} else {
						promptAccum->showWhen.type = UGCGenesisWhen_ObjectiveInProgress;
					}
					eaPush( &promptAccum->showWhen.eaObjectiveNames,
							StructAllocString( ugcMissionObjectiveIDLogicalNameTemp( objectiveID )));
			
					if( !mapName ) {
						UGCMapInfo** mapInfos = ugcObjectiveFinishMapNames( ugcProj, objective );
						int it;

						for( it = 0; it != eaSize( &mapInfos ); ++it ) {
							if( !mapInfos[ it ]->isInternal ) {
								eaPush( &promptAccum->eaExternalMapNames, StructAllocString( mapInfos[ it ]->mapName ));
							}
						}

						eaDestroyStruct( &mapInfos, parse_UGCMapInfo );
						if( eaSize( &promptAccum->eaExternalMapNames ) == 0 ) {
							 StructDestroySafe( parse_UGCGenesisMissionPrompt, &promptAccum );
						}
					}
				}
			}
		}
	}

	// If a prompt is created here, then we should actually fill in the blocks
	if( promptAccum ) {
		UGCComponent** dialogTrees = ugcComponentFindPopupPromptsForWhenInDialog( ugcProj->components, prompt );
		char name[ 256 ];
		int blockIt;
		int promptIt;
		
		sprintf( name, "Prompt_%d", prompt->uID );
		promptAccum->pcName = StructAllocString( name );

		for( blockIt = 0; blockIt != eaSize( &dialogTrees ); ++blockIt ) {
			U32 dialogID = dialogTrees[ blockIt ]->uID;
			UGCDialogTreeBlock* block = &dialogTrees[ blockIt ]->dialogBlock;
			UGCGenesisMissionPromptBlock initialBlock = { 0 };
			char nextBlockName[ 256 ];
			
			if( blockIt + 1 < eaSize( &dialogTrees )) {
				sprintf( nextBlockName, "Block%d", blockIt + 1 );
			} else {
				sprintf( nextBlockName, "" );
			}
		
			sprintf( name, "Block%d", blockIt );
			ugcGeneratePromptBlock( &initialBlock, ugcProj, contact, promptAccum->pcName, name, blockIt == 0,
									dialogID, nextBlockName, &block->initialPrompt, objectives, ugcProj->components,
									out_fsmObjectives );

			if( blockIt == 0 ) {
				StructFreeStringSafe( &initialBlock.name );
				StructCopyAll( parse_UGCGenesisMissionPromptBlock, &initialBlock, &promptAccum->sPrimaryBlock );
			} else {
				eaPush( &promptAccum->namedBlocks, StructClone( parse_UGCGenesisMissionPromptBlock, &initialBlock ));
			}
			
			for( promptIt = 0; promptIt != eaSize( &block->prompts ); ++promptIt ) {
				UGCGenesisMissionPromptBlock genesisBlock = { 0 };
				
				ugcGeneratePromptBlock( &genesisBlock, ugcProj, contact, promptAccum->pcName, name, blockIt == 0,
										dialogID, nextBlockName, block->prompts[ promptIt ], objectives, ugcProj->components,
										out_fsmObjectives );
				if(   !IS_HANDLE_ACTIVE( genesisBlock.costume.hCostume )
					  && !IS_HANDLE_ACTIVE( genesisBlock.costume.hPetCostume )) {
					StructCopyAll( parse_UGCGenesisMissionCostume, &initialBlock.costume, &genesisBlock.costume );
				}
				eaPush( &promptAccum->namedBlocks, StructClone( parse_UGCGenesisMissionPromptBlock, &genesisBlock ));
				StructReset( parse_UGCGenesisMissionPromptBlock, &genesisBlock );
			}

			StructReset( parse_UGCGenesisMissionPromptBlock, &initialBlock );
		}

		eaDestroy( &dialogTrees );
	}

	return promptAccum;
}

static const char* ugcPetContactListHandleString( UGCProjectData* ugcProj, const char* name )
{
	UGCPerAllegianceDefaults* config = ugcGetAllegianceDefaults( ugcProj );
	int petListIt;

	// Can only do the allegiance mapping if there is a single valid allegiance to use
	if( config ) {		
		for( petListIt = 0; petListIt != eaSize( &config->maps ); ++petListIt ) {
			if( config->maps[ petListIt ]->srcContactList == name ) {
				return allocAddString( config->maps[ petListIt ]->destContactList );
			}
		}
	}

	return allocAddString( name );
}

bool ugcPromptActionIsTrigger( UGCProjectData* ugcProj, U32 componentID, UGCDialogTreePromptAction* action )
{
	return (action->style == UGCDIALOG_STYLE_MISSION_OBJECTIVE || !action->nextPromptID && !action->bDismissAction);

	//Might want to try 
	//if( ugcObjectiveFindComponent( ugcProj->mission->objectives, componentID ) && !action->nextPromptID ) {
	//	return true;
	//}
	//if( ugcComponentFindTrigger( ugcProj->components, componentID, action->nextPromptID )) {
	//	return true;
	//}

}

bool ugcPromptActionIsMissionInfo( UGCProjectData* ugcProj, U32 componentID, UGCDialogTreePromptAction* action )
{
	return (action && action->style == UGCDIALOG_STYLE_MISSION_INFO);
}

void ugcGeneratePromptBlock( UGCGenesisMissionPromptBlock* out_block, UGCProjectData* ugcProj, UGCComponent* contact, const char* promptName, const char* name, bool initialBlockShouldBeWholePrompt,
							 U32 promptID, const char* nextBlockName, UGCDialogTreePrompt* block, UGCMissionObjective** objectives, UGCComponentList* components, UGCGenesisMissionObjective*** out_extraObjectives )
{
	UGCDialogStyle dialogStyle = ugcDefaultsDialogStyle();
	
	if( name ) {
		char nameBuffer[ 256 ];
		if( (S32)block->uid > 0 ) {
			sprintf( nameBuffer, "%s_%d", name, block->uid );
		} else {
			sprintf( nameBuffer, "%s", name );
		}
		out_block->name = StructAllocString( nameBuffer );
	}

	if( dialogStyle == UGC_DIALOG_STYLE_WINDOW ) {
		if( block->pcPromptCostume ) {
			out_block->costume.eCostumeType = UGCGenesisMissionCostumeType_Specified;
			SET_HANDLE_FROM_STRING( "PlayerCostume",
									ugcCostumeHandleString( ugcProj, block->pcPromptCostume ),
									out_block->costume.hCostume );
		} else if( IS_HANDLE_ACTIVE( block->hPromptPetCostume )) {
			out_block->costume.eCostumeType = UGCGenesisMissionCostumeType_PetCostume;
			SET_HANDLE_FROM_STRING( "PetContactList",
									ugcPetContactListHandleString( ugcProj, REF_STRING_FROM_HANDLE( block->hPromptPetCostume )),
									out_block->costume.hPetCostume );
		} else if( contact ) {
			const char* contactCostume = ugcComponentGroundCostumeName( ugcProj, contact );
			if( contactCostume ) {
				out_block->costume.eCostumeType = UGCGenesisMissionCostumeType_Specified;
				SET_HANDLE_FROM_STRING( "PlayerCostume",
										ugcCostumeHandleString( ugcProj, contactCostume ),
										out_block->costume.hCostume );
			}
		}

		out_block->pchHeadshotStyle = StructAllocString( ugcDialogTreePromptStyle( block ));
	} else if( dialogStyle == UGC_DIALOG_STYLE_IN_WORLD ) {
		SET_HANDLE_FROM_STRING( "Cutscene", ugcDialogTreePromptCameraPos( block ), out_block->hCutsceneDef );
		SET_HANDLE_FROM_STRING( "AIAnimList", ugcDialogTreePromptStyle( block ), out_block->hAnimList );
	}

	if( !nullStr( block->pcPromptTitle )) {
		StructCopyString( &out_block->pcTitleText, ugcAllocSMFString( block->pcPromptTitle, false ));
	} else {
		StructCopyString( &out_block->pcTitleText, " " );
	}
	eaPush( &out_block->eaBodyText, ugcAllocSMFString( block->pcPromptBody, true ));

	if( eaSize( &block->eaActions )) {
		int actionIt;
		for( actionIt = 0; actionIt != eaSize( &block->eaActions ); ++actionIt ) {
			UGCDialogTreePromptAction* action = block->eaActions[ actionIt ];
			UGCGenesisMissionPromptAction* accum = StructCreate( parse_UGCGenesisMissionPromptAction );

			if( !nullStr( action->pcText )) {
				accum->pcText = ugcAllocSMFString( action->pcText, false );
			}
			
			if( ugcPromptActionIsTrigger( ugcProj, promptID, action )) {
				accum->astrStyleName = allocAddString( "Mission_Objective" );
			} else if( ugcPromptActionIsMissionInfo( ugcProj, promptID, action )) {
				accum->astrStyleName = allocAddString( "Mission_Info" );
			}
				
			if( nextBlockName ) {
				if( action->nextPromptID ) {
					if( action->nextPromptID == (U32)-1 ) {
						if( initialBlockShouldBeWholePrompt ) {
							accum->pcNextPromptName = StructAllocString( promptName );
							accum->bDismissAction = true;
						} else {
							accum->pcNextBlockName = StructAllocString( name );
						}
					} else {
						char buffer[ 256 ];
						sprintf( buffer, "%s_%d", name, action->nextPromptID );
						accum->pcNextBlockName = StructAllocString( buffer );
					}
				} else if( !action->bDismissAction ) {
					if( nextBlockName[ 0 ]) {
						accum->pcNextBlockName = StructAllocString( nextBlockName );
					}
				} else {
					accum->bDismissAction = true;
				}
			}

			accum->enabledCheckedAttrib = StructClone( parse_UGCCheckedAttrib, action->enabledCheckedAttrib );

			// Grant item on actions that complete the dialog.
			if( !action->nextPromptID && !action->bDismissAction ) {
				UGCMissionObjective* objective = ugcObjectiveFindComponent( ugcProj->mission->objectives, promptID );
				if( objective && !nullStr( objective->sInteractProps.pcDropItemName )) {
					WorldGameActionProperties* giveItemAccum = StructCreate( parse_WorldGameActionProperties );
					char buffer[ RESOURCE_NAME_MAX_SIZE ];
					giveItemAccum->eActionType = WorldGameActionType_GiveItem;
					giveItemAccum->pGiveItemProperties = StructCreate( parse_WorldGiveItemActionProperties );

					sprintf( buffer, "%s:%s", ugcProj->ns_name, objective->sInteractProps.pcDropItemName );
					SET_HANDLE_FROM_STRING( "ItemDef", buffer, giveItemAccum->pGiveItemProperties->hItemDef );
					giveItemAccum->pGiveItemProperties->iCount = 1;
					
					eaPush( &accum->actionBlock.eaActions, giveItemAccum );
				}
			}

			// Not possible for GrantMission and MapLink prompts.
			if( components && objectives && out_extraObjectives ) {
				// MJF TODO: Combine this with the logic for clickie components.
				UGCGenesisWhen startWhen = { UGCGenesisWhen_MapStart };
				UGCGenesisWhen hideWhen = { UGCGenesisWhen_MapStart };
				UGCGenesisMissionObjective* beforeObjective = NULL;
				UGCGenesisMissionObjective* duringObjective = NULL;

				ugcWhenMakeGenesisWhen( action->pShowWhen, 0, &startWhen, components, objectives );
				ugcWhenMakeGenesisWhen( action->pHideWhen, 0, &hideWhen, components, objectives );

				if (startWhen.type == UGCGenesisWhen_MapStart && hideWhen.type == UGCGenesisWhen_Manual)
				{
					// We are the defaults. We don't need any extra objectives.
				}
				else
				{
					if( startWhen.type != UGCGenesisWhen_MapStart ) {
						char buffer[ 256 ];
						beforeObjective = StructCreate( parse_UGCGenesisMissionObjective );
						sprintf( buffer, "BeforeVisible_%s_%s_%d", promptName, out_block->name, actionIt );
						beforeObjective->pcName = StructAllocString( buffer );
						StructCopyAll( parse_UGCGenesisWhen, &startWhen, &beforeObjective->succeedWhen );
	
						duringObjective = StructCreate( parse_UGCGenesisMissionObjective );
						sprintf( buffer, "Visible_%s_%s_%d", promptName, out_block->name, actionIt );
						duringObjective->pcName = StructAllocString( buffer );
						duringObjective->succeedWhen.type = UGCGenesisWhen_Manual;
						duringObjective->bOptional = true;
					}
					if( hideWhen.type != UGCGenesisWhen_MapStart ) {
						if( !duringObjective ) {
							char buffer[ 256 ];
							duringObjective = StructCreate( parse_UGCGenesisMissionObjective );
							sprintf( buffer, "Visible_%s_%s_%d", promptName, out_block->name, actionIt );
							duringObjective->pcName = StructAllocString( buffer );
						}
						StructCopyAll( parse_UGCGenesisWhen, &hideWhen, &duringObjective->succeedWhen );
					}
	
					if( beforeObjective && duringObjective ) {
						UGCGenesisMissionObjective* parentObjective = StructCreate( parse_UGCGenesisMissionObjective );
						char buffer[ 256 ];
						sprintf( buffer, "Parent_%s_%s_%d", promptName, out_block->name, actionIt );
						parentObjective->pcName = StructAllocString( buffer );
						parentObjective->succeedWhen.type = UGCGenesisWhen_InOrder;
						eaPush( &parentObjective->eaChildren, beforeObjective );
						eaPush( &parentObjective->eaChildren, duringObjective );
					
						eaPush( out_extraObjectives, parentObjective );
					} else if( duringObjective ) {
						eaPush( out_extraObjectives, duringObjective );
					}
	
					if( duringObjective ) {
						accum->when.type = UGCGenesisWhen_ObjectiveInProgress;
						eaPush( &accum->when.eaObjectiveNames, StructAllocString( duringObjective->pcName ));
					}
				}
			}

			eaPush( &out_block->eaActions, accum );
		}
	} else {
		if( !nullStr( nextBlockName )) {
			UGCGenesisMissionPromptAction* accum = StructCreate( parse_UGCGenesisMissionPromptAction );
			accum->pcNextBlockName = StructAllocString( nextBlockName );
			
			eaPush( &out_block->eaActions, accum );
		}
	}
}

////////////////////////////////////////////////////////////
// FSM Generation
////////////////////////////////////////////////////////////

static const char* ugcComponentFSMGetUniqueName(void)
{
	static char name[256];
	static int id = 0;
	sprintf(name, "FSM_%d", id++);

	return name;
}

////////////////////////////////////////////////////////////
// Map Open Mission Generation
////////////////////////////////////////////////////////////

void ugcMissionGenerateForMap(UGCProjectData* ugcProj, const char *raw_map_name, UGCGenesisMapDescription* out_mapDesc, Vec3 override_spawn_pos, Vec3 override_spawn_rot)
{
	UGCComponentList* components = ugcProj->components;
	UGCGenesisMissionDescription* missionAccum = NULL;
	UGCGenesisMissionDescription* completedMissionAccum = NULL;
	UGCMissionObjective** mapMissionObjectives = NULL;
	int portal_id = 1;
	char map_name[ RESOURCE_NAME_MAX_SIZE ];
	UGCMap *map = ugcMapFindByName( ugcProj, raw_map_name );
	UGCBacklinkTable* pBacklinkTable = NULL;
	ugcBacklinkTableRefresh( ugcProj, &pBacklinkTable );

	resExtractNameSpace_s( raw_map_name, NULL, 0, SAFESTR(map_name) );
	
	// Generate mission
	ugcMissionTransmogrifyObjectives( ugcProj, ugcProj->mission->objectives, allocAddString( map_name ), false, &mapMissionObjectives );

	if( eaSize( &mapMissionObjectives ) > 0 ) {
		completedMissionAccum = StructCreate( parse_UGCGenesisMissionDescription );
		missionAccum = StructCreate( parse_UGCGenesisMissionDescription );

		completedMissionAccum->zoneDesc.pcName = StructAllocString( g_UGCMissionNameCompleted );
		completedMissionAccum->zoneDesc.pcDisplayName = StructAllocString( "Completed" );
		completedMissionAccum->zoneDesc.generationType = UGCGenesisMissionGenerationType_OpenMission_NoPlayerMission;
		SET_HANDLE_FROM_STRING( "MissionCategory", "UGC", completedMissionAccum->zoneDesc.hCategory );

		missionAccum->zoneDesc.pcName = StructAllocString( g_UGCMissionName );
		if( ugcDefaultsSingleMissionEnabled() ) {
			missionAccum->zoneDesc.pcDisplayName = ugcAllocSMFString( ugcProj->project->pcPublicName, false );
		} else {
			missionAccum->zoneDesc.pcDisplayName = ugcAllocSMFString( ugcMapMissionName( ugcProj, mapMissionObjectives[ 0 ]), false );
		}
		missionAccum->zoneDesc.generationType = UGCGenesisMissionGenerationType_OpenMission_NoPlayerMission;
		SET_HANDLE_FROM_STRING( "MissionCategory", "UGC", missionAccum->zoneDesc.hCategory );

		// generate the objectives
		ugcMissionGenerateObjectives( ugcProj, mapMissionObjectives, allocAddString( map_name ),
									  &missionAccum->zoneDesc.eaObjectives, &missionAccum->zoneDesc.eaPrompts );
	} else {
		completedMissionAccum = StructCreate( parse_UGCGenesisMissionDescription );
		missionAccum = StructCreate( parse_UGCGenesisMissionDescription );

		completedMissionAccum->zoneDesc.pcName = StructAllocString( g_UGCMissionNameCompleted );
		completedMissionAccum->zoneDesc.pcDisplayName = NULL;
		completedMissionAccum->zoneDesc.generationType = UGCGenesisMissionGenerationType_OpenMission_NoPlayerMission;
		SET_HANDLE_FROM_STRING( "MissionCategory", "UGC", completedMissionAccum->zoneDesc.hCategory );
	
		missionAccum->zoneDesc.pcName = StructAllocString( g_UGCMissionName );
		missionAccum->zoneDesc.pcDisplayName = NULL;
		missionAccum->zoneDesc.generationType = UGCGenesisMissionGenerationType_OpenMission_NoPlayerMission;
		SET_HANDLE_FROM_STRING( "MissionCategory", "UGC", missionAccum->zoneDesc.hCategory );
	}

	missionAccum->zoneDesc.ePlayType = ugcProjectPlayType( ugcProj );
	missionAccum->zoneDesc.eAuthorSource = ContentAuthor_User;
	missionAccum->zoneDesc.ugcProjectID = namespaceIsUGC(ugcProj->ns_name) ? ugcProjectContainerID( ugcProj ) : 0;
	completedMissionAccum->zoneDesc.ePlayType = ugcProjectPlayType( ugcProj );
	completedMissionAccum->zoneDesc.eAuthorSource = ContentAuthor_User;
	completedMissionAccum->zoneDesc.ugcProjectID = missionAccum->zoneDesc.ugcProjectID;

	if( ugcDefaultsSingleMissionEnabled() ) {
		UGCMissionMapLink* exitMapLink = ugcMissionFindLinkByExitMap( ugcProj, raw_map_name );

		// if return mission is not required or the map link is not the last one, we copy the return text over. Otherwise, the return mission will handle the last door text.
		if(!ugcReturnMissionRequired(ugcProj) || ugcProj->mission->return_map_link != exitMapLink)
		{
			StructCopyString( &missionAccum->zoneDesc.strReturnText, ugcMapMissionLinkReturnText( exitMapLink )); 
			StructCopyString( &completedMissionAccum->zoneDesc.strReturnText, ugcMapMissionLinkReturnText( exitMapLink ));
		}
	}
	
	// order is important!  mission_num = 0 is assumed to be the map completed!
	eaPush( &out_mapDesc->missions, completedMissionAccum );
	eaPush( &out_mapDesc->missions, missionAccum );

	// Generate challenges/prompts
	{
		bool mapStartPromptExistsIt = false;
		UGCGenesisMissionObjective** fsmObjectives = NULL;
		int it;
		for( it = 0; it != eaSize( &components->eaComponents ); ++it ) {
			UGCComponent* component = components->eaComponents[ it ];
			
			if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE )
			{
				UGCGenesisMissionPrompt* promptAccum = ugcComponentGeneratePromptMaybe( component, ugcProj, mapMissionObjectives, map_name, &fsmObjectives );
				if( promptAccum ) {
					if(   promptAccum->showWhen.type == UGCGenesisWhen_MapStart && nullStr( promptAccum->pcExternalContactName )
						  && nullStr( promptAccum->pcChallengeName )) {
						if( mapStartPromptExistsIt ) {
							promptAccum->showWhen.type = UGCGenesisWhen_Manual;
						}
						mapStartPromptExistsIt = true;
					}
					
					eaPush( &missionAccum->zoneDesc.eaPrompts, promptAccum );
					//TODO: figure out how to represent these prompts -- eaPush( &completedMissionAccum->zoneDesc.eaPrompts, StructClone( parse_UGCGenesisMissionPrompt, promptAccum ));
				}
			}
			else if( ugcComponentIsOnMap( component, map_name, false ))
			{
				bool challengeGenerated = ugcComponentGenerateChallengeMaybe( missionAccum, completedMissionAccum, &out_mapDesc->shared_challenges, ugcProj, pBacklinkTable, component, mapMissionObjectives, map, &fsmObjectives ); 
				if(   challengeGenerated
					  && (component->eType == UGC_COMPONENT_TYPE_KILL || component->eType == UGC_COMPONENT_TYPE_CONTACT) )
				{
					UGCFSMMetadata* fsmMetadata = ugcResourceGetFSMMetadata( component->fsmProperties.pcFSMNameRef );
					if( fsmMetadata ) {
						UGCGenesisFSM *fsm = StructCreate( parse_UGCGenesisFSM );
						fsm->pcName = allocAddString( ugcComponentFSMGetUniqueName() );
						fsm->pcFSMName = StructAllocString( component->fsmProperties.pcFSMNameRef );
						fsm->pcChallengeLogicalName = StructAllocString(ugcComponentGetLogicalNameTemp(component));
						fsm->activeWhen.type = UGCGenesisWhen_MapStart;

						FOR_EACH_IN_EARRAY_FORWARDS( fsmMetadata->eaExternVars, UGCFSMExternVar, externVar ) {
							UGCFSMVar* fsmVar = ugcComponentBehaviorGetFSMVar( component, externVar->astrName );
							if( fsmVar ) {
								WorldVariableDef* varDef = StructCreate( parse_WorldVariableDef );
								varDef->pcName = externVar->astrName;
								varDef->pSpecificValue = StructCreate( parse_WorldVariable );
									
								// Patrol_Speed is weird.  Even though it is a float, it needs
								// to be a string!
								if( stricmp( externVar->astrName, "Patrol_Speed" ) == 0 ) {
									char buffer[ 256 ];
									sprintf( buffer, "%f", fsmVar->floatVal );

									varDef->eType = varDef->pSpecificValue->eType = WVAR_STRING;
									varDef->pSpecificValue->pcStringVal = StructAllocString( buffer );
								} else if( stricmp( externVar->scType, "AIAnimList" ) == 0 ) {
									varDef->eType = varDef->pSpecificValue->eType = WVAR_ANIMATION;
									varDef->pSpecificValue->pcStringVal = StructAllocString( fsmVar->strStringVal );
								} else if( stricmp( externVar->scType, "AllMissionsIndex" ) == 0 ) {
									UGCGenesisMissionObjective* objective = StructCreate( parse_UGCGenesisMissionObjective );
									UGCGenesisWhen when = { UGCGenesisWhen_Manual };
									char buffer[ 256 ];

									if( fsmVar->pWhenVal ) {
										ugcWhenMakeGenesisWhen( fsmVar->pWhenVal, component->uID, &when, ugcProj->components, mapMissionObjectives );
									}
									sprintf( buffer, "FSM_Component_%d_%s", component->uID, externVar->astrName );
									objective->pcName = StructAllocString( buffer );
									StructCopyAll( parse_UGCGenesisWhen, &when, &objective->succeedWhen );
									eaPush( &fsmObjectives, objective );

									varDef->eType = varDef->pSpecificValue->eType = WVAR_STRING;
									sprintf( buffer, "%s:%s::%s",
											 ugcProj->ns_name,
											 ugcGenesisMissionNameRaw( map_name, missionAccum->zoneDesc.pcName, true ),
											 objective->pcName );
									varDef->pSpecificValue->pcStringVal = StructAllocString( buffer );
								} else if( stricmp( externVar->scType, "message" ) == 0 ) {
									varDef->eType = varDef->pSpecificValue->eType = WVAR_MESSAGE;
									varDef->pSpecificValue->pcStringVal = StructAllocString( fsmVar->strStringVal );
								} else if( externVar->type == MULTI_INT ) {
									varDef->eType = varDef->pSpecificValue->eType = WVAR_INT;
									varDef->pSpecificValue->iIntVal = fsmVar->floatVal;
								} else if( externVar->type == MULTI_FLOAT ) {
									varDef->eType = varDef->pSpecificValue->eType = WVAR_FLOAT;
									varDef->pSpecificValue->fFloatVal = fsmVar->floatVal;
								} else if( externVar->type == MULTI_STRING ) {
									varDef->eType = varDef->pSpecificValue->eType = WVAR_STRING;
									varDef->pSpecificValue->pcStringVal = StructAllocString( fsmVar->strStringVal );
								} else {
									ugcRaiseErrorInternalCode( UGC_ERROR, "Internal Error - FSMVar %s %s is of unsupported type.",
															   component->fsmProperties.pcFSMNameRef, externVar->astrName );
									varDef->eType = varDef->pSpecificValue->eType = WVAR_INT;
									varDef->pSpecificValue->iIntVal = 0;
								}

								eaPush( &fsm->eaVarDefs, varDef );
							}
						} FOR_EACH_END;

						eaPush( &missionAccum->zoneDesc.eaFSMs, fsm );
					}
				}
			}
		}

		if(override_spawn_pos)
		{
			UGCComponent *override_spawn_component = StructCreate(parse_UGCComponent);
			override_spawn_component->eType = UGC_COMPONENT_TYPE_SPAWN;

			ugcComponentOpSetPlacement(ugcProj, override_spawn_component, map, 0);

			copyVec3(override_spawn_pos, override_spawn_component->sPlacement.vPos);
			if( override_spawn_rot ) {
				copyVec3(override_spawn_rot, override_spawn_component->sPlacement.vRotPYR);
			}
			override_spawn_component->sPlacement.iRoomLevel = -1;
			override_spawn_component->eMapType = ugcMapGetType(map);
			if(ugcComponentGenerateChallengeMaybe( missionAccum, completedMissionAccum, &out_mapDesc->shared_challenges, ugcProj, pBacklinkTable, override_spawn_component, mapMissionObjectives, map, &fsmObjectives ))
			{
				UGCGenesisMissionChallenge *challenge = missionAccum->eaChallenges[eaSize(&missionAccum->eaChallenges) - 1];
				StructCopyString(&challenge->pcName, "SPAWN_OVERRIDE");
			}
			StructDestroy(parse_UGCComponent, override_spawn_component);
		}

		if( eaSize( &fsmObjectives )) {
			UGCGenesisMissionObjective* fsmObjective = StructCreate( parse_UGCGenesisMissionObjective );
			fsmObjective->pcName = StructAllocString( "FSM" );
			fsmObjective->succeedWhen.type = UGCGenesisWhen_AllOf;
			fsmObjective->eaChildren = fsmObjectives;
			fsmObjective->bOptional = true;
			ugcMissionFSMObjectiveFixup( fsmObjective );
			eaInsert( &missionAccum->zoneDesc.eaObjectives, fsmObjective, 0 );

			fsmObjectives = NULL;
		}
	}
	
	eaDestroyStruct( &mapMissionObjectives, parse_UGCMissionObjective );
	ugcBacklinkTableDestroy( &pBacklinkTable );
}
