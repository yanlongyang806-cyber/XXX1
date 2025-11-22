#include "NNOUGCCommon.h"

#include "../WorldLib/StaticWorld/WorldGridPrivate.h"
#include "CostumeCommon.h"
#include "CostumeCommonTailor.h"
#include "ObjectLibrary.h"
#include "ResourceInfo.h"
#include "StateMachine.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "NNOUGCInteriorCommon.h"
#include "UGCInteriorCommon.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCResource.h"
#include "WorldGrid.h"
#include "error.h"
#include "quat.h"
#include "wlUGC.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

void ugcEditorFixupMaps(UGCProjectData* ugcProj);
void ugcEditorFixupComponents(UGCProjectData* ugcProj, int* numDialogsDeleted, int* numCostumesReset, int* fixupFlags);
void ugcEditorFixupMission(UGCProjectData* ugcProj, int* numCostumesReset, int* numObjectivesReset);
void ugcEditorFixupObjectivesComponentMapNames(UGCProjectData *ugcProj, UGCMissionObjective** objectives, int* numObjectivesReset);
void ugcEditorFixupCostumes( UGCProjectData* ugcProj );
void ugcEditorFixupProject(UGCProjectData* ugcProj);
bool ugcEditorFixupCheckedAttrib( UGCProjectData* ugcProj, UGCCheckedAttrib* checkedAttrib );

//////////////////////////////////////////////
// Map Fixup
//////////////////////////////////////////////

static bool ugcMapGetDefaultSpawnPosition( UGCProjectData* ugcProj, UGCMap* map, Vec3 out_pos, float* out_rot, int* out_parentComponent )
{
	if( map->pUnitializedMap ) {
		return false;
	} else if( ugcMapGetType( map ) == UGC_MAP_TYPE_INTERIOR ) {
		FOR_EACH_IN_EARRAY_FORWARDS( ugcProj->components->eaComponents, UGCComponent, room_component ) { // forwards
			if(   room_component->eType == UGC_COMPONENT_TYPE_ROOM
				  && ugcComponentIsOnMap( room_component, map->pcName, false )) {
				UGCRoomInfo *room_info = ugcRoomGetRoomInfo(room_component->iObjectLibraryId);
				if( room_info && eaSize( &room_info->doors ) > 0 ) {
					// Place the spawn point near a door
					Vec3 door_local_pos;
					F32 door_rot;
					int door_rot_quant;
					ugcRoomGetDoorLocalPos(room_info, 0, door_local_pos); // first door
					ugcRoomConvertLocalToWorld(room_component, door_local_pos, out_pos);

					door_rot = ugcRoomGetDoorLocalRot(room_info, 0)+room_component->sPlacement.vRotPYR[1]; // first door
					door_rot_quant = (int)floor(door_rot/90.f + 0.01) % 4;
					if (door_rot_quant < 0)
						door_rot_quant += 4;
					switch (door_rot_quant) {
						xcase 0:
							out_pos[2] -= 10.f;
						xcase 1:
							out_pos[0] -= 10.f;
						xcase 2:
							out_pos[2] += 10.f;
						xcase 3:
							out_pos[0] += 10.f;
					}

					*out_rot = 0;
					*out_parentComponent = room_component->uID;
					return true;
				}
			}
		} FOR_EACH_END;
	} else {
		Quat qOrientation;
		if( !map->pPrefab || !ugcGetZoneMapSpawnPoint( map->pPrefab->map_name, out_pos, &qOrientation )) {
			setVec3(out_pos, 0, 0, 0);
			*out_rot = 0;
		} else {
			Vec3 vPYR;
			quatToPYR(qOrientation, vPYR);
			*out_rot = vPYR[1];
		}
		out_pos[1] = 0;
		*out_parentComponent = 0;
		return true;
	}

	return false;
}

static bool ugcMapGetDefaultRewardBoxPosition( UGCProjectData* ugcProj, UGCMap* map, Vec3 out_pos, float* out_rot, int* out_parentComponent )
{
	if( map->pUnitializedMap ) {
		return false;
	} else if( ugcMapGetType( map ) == UGC_MAP_TYPE_INTERIOR ) {
		FOR_EACH_IN_EARRAY( ugcProj->components->eaComponents, UGCComponent, room_component ) { // backwards
			if(   room_component->eType == UGC_COMPONENT_TYPE_ROOM
				&& ugcComponentIsOnMap( room_component, map->pcName, false )) {
					UGCRoomInfo *room_info = ugcRoomGetRoomInfo(room_component->iObjectLibraryId);
					if( room_info && eaSize( &room_info->doors ) > 0 ) {
						// Place the reward box near a door
						Vec3 door_local_pos;
						F32 door_rot;
						int door_rot_quant;
						ugcRoomGetDoorLocalPos(room_info, eaSize( &room_info->doors ) - 1, door_local_pos); // last door
						ugcRoomConvertLocalToWorld(room_component, door_local_pos, out_pos);

						door_rot = ugcRoomGetDoorLocalRot(room_info, eaSize( &room_info->doors ) - 1)+room_component->sPlacement.vRotPYR[1]; // last door
						door_rot_quant = (int)floor(door_rot/90.f + 0.01) % 4;
						if (door_rot_quant < 0)
							door_rot_quant += 4;
						switch (door_rot_quant) {
							xcase 0:
						out_pos[2] -= 10.f;
						xcase 1:
						out_pos[0] -= 10.f;
						xcase 2:
						out_pos[2] += 10.f;
						xcase 3:
						out_pos[0] += 10.f;
						}

						*out_rot = 0;
						*out_parentComponent = room_component->uID;
						return true;
					}
			}
		} FOR_EACH_END;
	} else {
		Quat qOrientation;
		if( !map->pPrefab || !ugcGetZoneMapSpawnPoint( map->pPrefab->map_name, out_pos, &qOrientation )) {
			setVec3(out_pos, 0, 0, 0);
			*out_rot = 0;
		} else {
			Vec3 vPYR;
			quatToPYR(qOrientation, vPYR);
			*out_rot = vPYR[1];
		}
		out_pos[1] = 0;
		*out_parentComponent = 0;
		return true;
	}

	return false;
}

void ugcEditorFixupMaps(UGCProjectData* ugcProj)
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	
	// Make sure each map has a spawn point
	FOR_EACH_IN_EARRAY(ugcProj->maps, UGCMap, map)
	{
		UGCMapType mapType = ugcMapGetType( map );
		UGCComponent* spawn_component = NULL;
		UGCComponent* mission_return_component = NULL;
		bool has_whole_map = false;
		UGCAssetTagCategory *slot_category = ugcSkyGetSlotCategory(ugcMapGetType(map));
		UGCGenesisBackdrop *ugc_backdrop;
		if( map->pPrefab ) {
			ugc_backdrop = &map->pPrefab->backdrop;
		} else if( map->pSpace ) {
			ugc_backdrop = &map->pSpace->backdrop;
		} else {
			ugc_backdrop = NULL;
		}

		// if the map is not used (and not completed), it should be deleted
		if( map->pUnitializedMap ) {
			UGCMissionObjective* objective = ugcObjectiveFindOnMap( ugcProj->mission->objectives, ugcProj->components, map->pcName );
			if( !objective ) {
				StructDestroySafe( parse_UGCMap, &map );
				eaRemove( &ugcProj->maps, FOR_EACH_IDX( ugcProj->maps, map ));
				continue;
			}
		}

		FOR_EACH_IN_EARRAY(ugcProj->components->eaComponents, UGCComponent, component)
		{
			if (component->eType == UGC_COMPONENT_TYPE_SPAWN &&
				resNamespaceBaseNameEq(component->sPlacement.pcMapName, map->pcName))
			{
				spawn_component = component;
			}
			if (component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP &&
				resNamespaceBaseNameEq(component->sPlacement.pcMapName, map->pcName))
			{
				has_whole_map = true;
			}
			if (component->bInteractIsMissionReturn &&
				resNamespaceBaseNameEq(component->sPlacement.pcMapName, map->pcName))
			{
				mission_return_component = component;
			}
		}
		FOR_EACH_END;

		if (!spawn_component)
		{
			Vec3 new_spawn_pos;
			float fNewSpawnRot = 0;
			int new_spawn_room = 0;

			if( ugcMapGetDefaultSpawnPosition(ugcProj, map, new_spawn_pos, &fNewSpawnRot, &new_spawn_room ))
			{
				UGCComponent *new_spawn = ugcComponentOpCreate(ugcProj, UGC_COMPONENT_TYPE_SPAWN, new_spawn_room);
				assert(new_spawn);
				ugcComponentOpSetPlacement(ugcProj, new_spawn, map, UGC_TOPLEVEL_ROOM_ID);
				copyVec3(new_spawn_pos, new_spawn->sPlacement.vPos);
				setVec3(new_spawn->sPlacement.vRotPYR, 0, fNewSpawnRot, 0);

				spawn_component = new_spawn;
			}
		}
		if (!has_whole_map)
		{
			UGCComponent *new_com = ugcComponentOpCreate(ugcProj, UGC_COMPONENT_TYPE_WHOLE_MAP, 0);
			assert(new_com);
			ugcComponentOpSetPlacement(ugcProj, new_com, map, UGC_TOPLEVEL_ROOM_ID);
			setVec3(new_com->sPlacement.vPos, 0, 0, 0);
		}

		// Fixup backdrop
		if( ugc_backdrop ) {
			if( !IS_HANDLE_ACTIVE( ugc_backdrop->hSkyBase )) {
				switch( mapType ) {
					xcase UGC_MAP_TYPE_INTERIOR: case UGC_MAP_TYPE_PREFAB_INTERIOR:
						COPY_HANDLE( ugc_backdrop->hSkyBase, defaults->hInteriorSky );
					xcase UGC_MAP_TYPE_GROUND: case UGC_MAP_TYPE_PREFAB_GROUND:
						COPY_HANDLE( ugc_backdrop->hSkyBase, defaults->hGroundSky );
					xcase UGC_MAP_TYPE_SPACE: case UGC_MAP_TYPE_PREFAB_SPACE:
						COPY_HANDLE( ugc_backdrop->hSkyBase, defaults->hSpaceSky );
				}
			}
			
			eaSetSizeStruct( &ugc_backdrop->eaSkyOverrides, parse_UGCGenesisBackdropSkyOverride, 5 );
			if(   mapType != UGC_MAP_TYPE_SPACE && mapType != UGC_MAP_TYPE_PREFAB_SPACE
				  && mapType != UGC_MAP_TYPE_GROUND && mapType != UGC_MAP_TYPE_PREFAB_GROUND ) {
				StructFreeStringSafe( &ugc_backdrop->strAmbientSoundOverride );
			}
		}
	}
	FOR_EACH_END;
}

//////////////////////////////////////////////
// Component Fixup
//////////////////////////////////////////////

static void ugcEditorFixupSetUGCWhen(UGCWhen *when, UGCWhenType type, int *flags)
{
	if (flags && type != when->eType)
		*flags |= UGC_FIXUP_CLEARED_WHENS;
	when->eType = type;	
}

static void ugcEditorFixupUGCWhen(UGCProjectData *ugcProj, UGCComponentType component_type, UGCWhen *when, UGCMissionObjective *objective, bool is_start, const char* internalMapName, int *fixupFlags)
{
	int i;
	if (objective)
	{
		if (is_start)
		{
			if (when->eType != UGCWHEN_OBJECTIVE_IN_PROGRESS &&
				when->eType != UGCWHEN_MAP_START)
			{
				ugcEditorFixupSetUGCWhen(when, UGCWHEN_MAP_START, fixupFlags);
			}
		}
		else
		{
			if (when->eType != UGCWHEN_CURRENT_COMPONENT_COMPLETE &&
				when->eType != UGCWHEN_OBJECTIVE_COMPLETE &&
				when->eType != UGCWHEN_MANUAL)
			{
				ugcEditorFixupSetUGCWhen(when, UGCWHEN_MANUAL, fixupFlags);
			}
		}
	}

	if (  (component_type == UGC_COMPONENT_TYPE_CONTACT || component_type == UGC_COMPONENT_TYPE_TRAP
		   || component_type == UGC_COMPONENT_TYPE_SOUND || component_type == UGC_COMPONENT_TYPE_ROOM_DOOR)
		  && when->eType == UGCWHEN_CURRENT_COMPONENT_COMPLETE)
	{
		ugcEditorFixupSetUGCWhen(when, UGCWHEN_MANUAL, fixupFlags);
	}

	switch (when->eType)
	{
	case UGCWHEN_OBJECTIVE_IN_PROGRESS:
	case UGCWHEN_OBJECTIVE_COMPLETE:
	case UGCWHEN_OBJECTIVE_START:
		for (i = eaiSize(&when->eauObjectiveIDs)-1; i >= 0; --i)
		{
			assert(when->eauObjectiveIDs);
			if (when->eauObjectiveIDs[i] != -1 && !ugcObjectiveFind(ugcProj->mission->objectives, when->eauObjectiveIDs[i]))
			{
				eaiRemove(&when->eauObjectiveIDs, i);
			}
		}
		if (objective)
		{
			eaiClear(&when->eauObjectiveIDs);
			eaiPush(&when->eauObjectiveIDs, objective->id);
		}
		else
		{
			if (eaiSize(&when->eauObjectiveIDs) == 0)
			{
				eaiPush(&when->eauObjectiveIDs, -1);
			}
		}
		eaiClear(&when->eauComponentIDs);
		eaDestroyStruct( &when->eaDialogPrompts, parse_UGCWhenDialogPrompt );
		break;

	case UGCWHEN_COMPONENT_COMPLETE:
	case UGCWHEN_COMPONENT_REACHED:
		for (i = eaiSize(&when->eauComponentIDs)-1; i >= 0; --i)
		{
			assert(when->eauComponentIDs);
			if (when->eauComponentIDs[i] != -1)
			{
				UGCComponent *component = ugcComponentFindByID(ugcProj->components, when->eauComponentIDs[i]);
				if( !component || component->sPlacement.uRoomID == GENESIS_UNPLACED_ID ) {
					eaiRemove(&when->eauComponentIDs, i);
				} else if( when->eType == UGCWHEN_COMPONENT_COMPLETE
						   && component->eType != UGC_COMPONENT_TYPE_KILL
						   && component->eType != UGC_COMPONENT_TYPE_OBJECT
						   && component->eType != UGC_COMPONENT_TYPE_SOUND
						   && component->eType != UGC_COMPONENT_TYPE_ROOM_DOOR
						   && component->eType != UGC_COMPONENT_TYPE_FAKE_DOOR
						   && component->eType != UGC_COMPONENT_TYPE_BUILDING_DEPRECATED
						   && component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE
						   && component->eType != UGC_COMPONENT_TYPE_CLUSTER_PART ) {
					eaiRemove( &when->eauComponentIDs, i );
				} else if( when->eType == UGCWHEN_COMPONENT_REACHED
						   && component->eType != UGC_COMPONENT_TYPE_ROOM_MARKER
						   && component->eType != UGC_COMPONENT_TYPE_PLANET ) {
					eaiRemove( &when->eauComponentIDs, i );
				} else if( internalMapName && !resNamespaceBaseNameEq( component->sPlacement.pcMapName, internalMapName )) {
					eaiRemove(&when->eauComponentIDs, i);
				}
			}
		}
		if (eaiSize(&when->eauComponentIDs) == 0)
		{
			eaiPush(&when->eauComponentIDs, -1);
		}
		eaiClear(&when->eauObjectiveIDs);
		eaDestroyStruct( &when->eaDialogPrompts, parse_UGCWhenDialogPrompt );
		break;

	case UGCWHEN_DIALOG_PROMPT_REACHED:
		for (i = eaSize(&when->eaDialogPrompts)-1; i >= 0; --i)
		{
			UGCWhenDialogPrompt* whenPrompt = when->eaDialogPrompts[i];
			if (whenPrompt->uDialogID != -1)
			{
				if(whenPrompt->uDialogID != -1 && whenPrompt->iPromptID != 0)
				{
					UGCComponent *dialogTree = ugcComponentFindByID(ugcProj->components, whenPrompt->uDialogID);
					UGCDialogTreePrompt* prompt = dialogTree ? ugcDialogTreeGetPrompt( &dialogTree->dialogBlock, whenPrompt->iPromptID) : NULL;
					if (!dialogTree || dialogTree->eType != UGC_COMPONENT_TYPE_DIALOG_TREE || !prompt)
					{
						StructDestroy(parse_UGCWhenDialogPrompt, whenPrompt);
						eaRemove(&when->eaDialogPrompts, i);
					}
					else if( internalMapName )
					{
						UGCComponent* dialogActor = ugcComponentFindByID( ugcProj->components, dialogTree->uActorID );
						if(   !dialogActor || dialogActor->sPlacement.uRoomID == GENESIS_UNPLACED_ID
							  || !resNamespaceBaseNameEq( dialogActor->sPlacement.pcMapName, internalMapName )) {
							StructDestroy(parse_UGCWhenDialogPrompt, whenPrompt);
							eaRemove(&when->eaDialogPrompts, i);
						}
					}
				}
			}
		}
		if (eaSize(&when->eaDialogPrompts) == 0)
		{
			UGCWhenDialogPrompt* whenDialogPrompt = StructCreate(parse_UGCWhenDialogPrompt);
			whenDialogPrompt->uDialogID = -1;
			eaPush(&when->eaDialogPrompts, whenDialogPrompt);
		}
		eaiClear(&when->eauComponentIDs);
		eaiClear(&when->eauObjectiveIDs);
		break;

	case UGCWHEN_CURRENT_COMPONENT_COMPLETE:
	case UGCWHEN_MISSION_START:
	case UGCWHEN_MAP_START:
	case UGCWHEN_MANUAL:
		eaiClear(&when->eauComponentIDs);
		eaiClear(&when->eauObjectiveIDs);
		eaDestroyStruct( &when->eaDialogPrompts, parse_UGCWhenDialogPrompt );
		break;
	}
}

static bool ugcEditorFixupComponentDialogTree(UGCProjectData* ugcProj, UGCComponent *component, UGCMissionObjective* objective, int* fixupFlags)
{
	UGCComponent* contactComponent = ugcComponentFindByID(ugcProj->components, component->uActorID );
	int promptIt;
	int actionIt;

	// Fixup dialogs to always have at least one action
	if( eaSize( &component->dialogBlock.initialPrompt.eaActions ) == 0 ) {
		eaPush( &component->dialogBlock.initialPrompt.eaActions, StructCreate( parse_UGCDialogTreePromptAction ));
	}
	component->dialogBlock.initialPrompt.uid = -1;

	for( promptIt = 0; promptIt != eaSize( &component->dialogBlock.prompts ); ++promptIt ) {
		UGCDialogTreePrompt* prompt = component->dialogBlock.prompts[ promptIt ];
		if( eaSize( &prompt->eaActions ) == 0 ) {
			eaPush( &prompt->eaActions, StructCreate( parse_UGCDialogTreePromptAction ));
		}
	}

	// Fixup dialog actions to always have whens, also fixup checked attribs
	for( actionIt = 0; actionIt != eaSize( &component->dialogBlock.initialPrompt.eaActions ); ++actionIt ) {
		UGCDialogTreePromptAction* action = component->dialogBlock.initialPrompt.eaActions[ actionIt ];

		if( !action->pShowWhen ) {
			action->pShowWhen = StructCreate( parse_UGCWhen );
			action->pShowWhen->eType = UGCWHEN_MAP_START;
		}
		ugcEditorFixupUGCWhen(ugcProj, UGC_COMPONENT_TYPE_DIALOG_TREE, action->pShowWhen, NULL, true, SAFE_MEMBER( contactComponent, sPlacement.pcMapName ), fixupFlags);

		if( !action->pHideWhen ) {
			action->pHideWhen = StructCreate( parse_UGCWhen );
			action->pHideWhen->eType = UGCWHEN_MANUAL;
		}
		ugcEditorFixupUGCWhen(ugcProj, UGC_COMPONENT_TYPE_DIALOG_TREE, action->pHideWhen, NULL, true, SAFE_MEMBER( contactComponent, sPlacement.pcMapName ), fixupFlags);

		if( action->enabledCheckedAttrib ) {
			if( !ugcEditorFixupCheckedAttrib( ugcProj, action->enabledCheckedAttrib )) {
				StructDestroySafe( parse_UGCCheckedAttrib, &action->enabledCheckedAttrib );
			}
		}
	}

	for( promptIt = 0; promptIt != eaSize( &component->dialogBlock.prompts ); ++promptIt ) {
		for( actionIt = 0; actionIt != eaSize( &component->dialogBlock.prompts[ promptIt ]->eaActions ); ++actionIt ) {
			UGCDialogTreePromptAction* action = component->dialogBlock.prompts[ promptIt ]->eaActions[ actionIt ];

			if( !action->pShowWhen ) {
				action->pShowWhen = StructCreate( parse_UGCWhen );
				action->pShowWhen->eType = UGCWHEN_MAP_START;
			}
			ugcEditorFixupUGCWhen(ugcProj, UGC_COMPONENT_TYPE_DIALOG_TREE, action->pShowWhen, NULL, true, SAFE_MEMBER( contactComponent, sPlacement.pcMapName ), fixupFlags);

			if( !action->pHideWhen ) {
				action->pHideWhen = StructCreate( parse_UGCWhen );
				action->pHideWhen->eType = UGCWHEN_MANUAL;
			}
			ugcEditorFixupUGCWhen(ugcProj, UGC_COMPONENT_TYPE_DIALOG_TREE, action->pHideWhen, NULL, true, SAFE_MEMBER( contactComponent, sPlacement.pcMapName ), fixupFlags);

			if( action->enabledCheckedAttrib ) {
				if( !ugcEditorFixupCheckedAttrib( ugcProj, action->enabledCheckedAttrib )) {
					StructDestroySafe( parse_UGCCheckedAttrib, &action->enabledCheckedAttrib );
				}
			}
		}
	}

	// Dialog trees must always have a valid actor.
	if(   !contactComponent
		  || (contactComponent->eType != UGC_COMPONENT_TYPE_CONTACT
			  && contactComponent->eType != UGC_COMPONENT_TYPE_OBJECT) ) {
		if( objective ) {
			// Create a component for it
			contactComponent = ugcComponentOpCreate( ugcProj, UGC_COMPONENT_TYPE_CONTACT, 0 );
			component->uActorID = contactComponent->uID;
			return false;
		} else {
			// Delete the dialog
			ugcComponentOpDelete( ugcProj, component, false );
			return false;
		}
	}

	// If there are unreachable prompts, they should be deleted
	{
		StashTable promptReachableTable = stashTableCreateInt( eaSize( &component->dialogBlock.prompts ));

		for( actionIt = 0; actionIt != eaSize( &component->dialogBlock.initialPrompt.eaActions ); ++actionIt ) {
			UGCDialogTreePromptAction* action = component->dialogBlock.initialPrompt.eaActions[ actionIt ];
			if( action->nextPromptID ) {
				stashIntAddInt( promptReachableTable, action->nextPromptID, 1, false );
			}
		}
		for( promptIt = 0; promptIt != eaSize( &component->dialogBlock.prompts ); ++promptIt ) {
			UGCDialogTreePrompt* prompt = component->dialogBlock.prompts[ promptIt ];
			for( actionIt = 0; actionIt != eaSize( &prompt->eaActions ); ++actionIt ) {
				UGCDialogTreePromptAction* action = prompt->eaActions[ actionIt ];
				if( action->nextPromptID ) {
					stashIntAddInt( promptReachableTable, action->nextPromptID, 1, false );
				}
			}
		}

		for( promptIt = eaSize( &component->dialogBlock.prompts ) - 1; promptIt >= 0; --promptIt ) {
			UGCDialogTreePrompt* prompt = component->dialogBlock.prompts[ promptIt ];
			if( !stashIntFindInt( promptReachableTable, prompt->uid, NULL )) {
				StructDestroy( parse_UGCDialogTreePrompt, prompt );
				eaRemove( &component->dialogBlock.prompts, promptIt );
			}
		}

		stashTableDestroy( promptReachableTable );
	}

	// In Neverwinter, the dialog title is always the name of the contact doing the talking
	{
		char contactComponentName[ 256 ];
		if( contactComponent ) {
			ugcComponentGetDisplayName( contactComponentName, ugcProj, contactComponent, true );
		} else {
			strcpy( contactComponentName, "" );
		}

		StructCopyString( &component->dialogBlock.initialPrompt.pcPromptTitle, contactComponentName );
		for( promptIt = 0; promptIt != eaSize( &component->dialogBlock.prompts ); ++promptIt ) {
			StructCopyString( &component->dialogBlock.prompts[ promptIt ]->pcPromptTitle, contactComponentName );
		}
	}

	return true;
}

static void ugcEditorFixupComponentDetails(UGCComponent *component)
{
	UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);
	if (room_info)
	{
		FOR_EACH_IN_EARRAY(room_info->details, UGCRoomDetailDef, detail_def)
		{
			int idx = FOR_EACH_IDX(room_info->details, detail_def);
			UGCRoomDetailData *data = NULL;
			FOR_EACH_IN_EARRAY(component->eaRoomDetails, UGCRoomDetailData, detail_data)
			{
				if (detail_data->iIndex == idx)
				{
					data = detail_data;
					break;
				}
			}
			FOR_EACH_END;
			if (!data)
			{
				data = StructCreate(parse_UGCRoomDetailData);
				data->iIndex = idx;
				eaPush(&component->eaRoomDetails, data);
			}
			else
			{
				if (data->iChoice < 0 || data->iChoice >= detail_def->iChildCount)
				{
					data->iChoice = 0;
				}
			}
		}
		FOR_EACH_END;
	}
	else
	{
		eaDestroyStruct(&component->eaRoomDetails, parse_UGCRoomDetailData);
	}
}

static void ugcEditorFixupTriggers(UGCProjectData *ugcProj, UGCBacklinkTable* pBacklinkTable, UGCComponent *component, UGCMissionObjective* objective, int *fixupFlags)
{
	if( !ugcComponentStateCanBeEdited( ugcProj, component )) {
		if( component->eType != UGC_COMPONENT_TYPE_REWARD_BOX ) {
			if( !component->pStartWhen ) {
				component->pStartWhen = StructCreate( parse_UGCWhen );
			}
			if( !component->pHideWhen ) {
				component->pHideWhen = StructCreate( parse_UGCWhen );
			}
		}
		
		if( component->pStartWhen && component->pStartWhen->eType != UGCWHEN_MAP_START ) {
			component->pStartWhen->eType = UGCWHEN_MAP_START;
		}
		if( component->pHideWhen && component->pHideWhen->eType != UGCWHEN_MANUAL ) {
			component->pHideWhen->eType = UGCWHEN_MANUAL;
		}
		return;
	}
	
	if (!component->pStartWhen && component->eType != UGC_COMPONENT_TYPE_REWARD_BOX)
	{
		if (component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE || component->sPlacement.uRoomID != GENESIS_UNPLACED_ID)
		{
			component->pStartWhen = StructCreate(parse_UGCWhen);
			component->pStartWhen->eType = UGCWHEN_MAP_START;
		}
		else
		{
			component->pStartWhen = StructCreate(parse_UGCWhen);
			component->pStartWhen->eType = UGCWHEN_OBJECTIVE_IN_PROGRESS;
		}
	}
	if(component->pStartWhen)
		ugcEditorFixupUGCWhen(ugcProj, component->eType, component->pStartWhen, objective, true, component->sPlacement.pcMapName, fixupFlags);

	if (!component->pHideWhen && componentStateCanBecomeHidden(component->eType))
	{
		component->pHideWhen = StructCreate(parse_UGCWhen);
		component->pHideWhen->eType = UGCWHEN_MANUAL;
	}
	if (component->pHideWhen)
		ugcEditorFixupUGCWhen(ugcProj, component->eType, component->pHideWhen, objective, false, component->sPlacement.pcMapName, fixupFlags);

	if (objective)
	{
		if (eaSize(&component->eaTriggerGroups) > 0)
			eaDestroyStruct(&component->eaTriggerGroups, parse_UGCInteractProperties);
		StructReset( parse_UGCCheckedAttrib, &component->visibleCheckedAttrib );
	}
	else if (component->bInteractForce || ugcBacklinkTableFindTrigger(pBacklinkTable, component->uID, 0))
	{
		if( eaSize( &component->eaTriggerGroups ) == 0 ) {
			UGCInteractProperties *trigger_properties = StructCreate(parse_UGCInteractProperties);
			trigger_properties->eInteractDuration = UGCDURATION_MEDIUM;
			eaPush(&component->eaTriggerGroups, trigger_properties);
		}
	}
	else
	{
		eaClearStruct( &component->eaTriggerGroups, parse_UGCInteractProperties );
	}

	FOR_EACH_IN_EARRAY_FORWARDS( component->eaTriggerGroups, UGCInteractProperties, triggerGroup ) {
		if( triggerGroup->succeedCheckedAttrib ) {
			if( !ugcEditorFixupCheckedAttrib( ugcProj, triggerGroup->succeedCheckedAttrib )) {
				StructDestroySafe( parse_UGCCheckedAttrib, &triggerGroup->succeedCheckedAttrib );
			}
		}
	} FOR_EACH_END;
	ugcEditorFixupCheckedAttrib( ugcProj, &component->visibleCheckedAttrib );
}

static bool ugcEditorFixupDialogTreePrompt( UGCProjectData* ugcProj, UGCDialogTreePrompt* prompt, const char* defaultCostume, const char* defaultPetCostume )
{
	if( !nullStr( prompt->pcPromptCostume ) && !ugcCostumeSpecifierExists( ugcProj, prompt->pcPromptCostume )) {
		StructFreeStringSafe( &prompt->pcPromptCostume );
		prompt->pcPromptCostume = StructAllocString( defaultCostume );
		SET_HANDLE_FROM_STRING( "PetContactList", defaultPetCostume, prompt->hPromptPetCostume );

		return true;
	}

	return false;
}

static int ugcEditorFixupDialogTreeBlock( UGCProjectData* ugcProj, UGCDialogTreeBlock* block, const char* defaultCostume, const char* defaultPetCostume )
{
	int accum = 0;
	int it;

	accum += ugcEditorFixupDialogTreePrompt( ugcProj, &block->initialPrompt, defaultCostume, defaultPetCostume );
	for( it = 0; it != eaSize( &block->prompts ); ++it ) {
		accum += ugcEditorFixupDialogTreePrompt( ugcProj, block->prompts[ it ], defaultCostume, defaultPetCostume );
	}

	return accum;
}

static void ugcEditorFixupUnreferencedCostumes(UGCProjectData *ugcProj, UGCComponent *component, int *numCostumesReset)
{
	if( component->eType == UGC_COMPONENT_TYPE_CONTACT ) {
		if( !nullStr( component->pcPromptCostumeName ) ) {
			UGCComponent *prompt_component = ugcComponentFindDefaultPromptForID(ugcProj->components, component->uID);
			if (prompt_component && !prompt_component->pcCostumeName)
			{
				UGCDialogTreePrompt *prompt = &component->dialogBlock.initialPrompt;
				if (!IS_HANDLE_ACTIVE(prompt->hPromptPetCostume) && nullStr(prompt->pcPromptCostume))
				{
					StructCopyString(&prompt->pcPromptCostume, component->pcPromptCostumeName);
				}
			}
			StructFreeStringSafe( &component->pcPromptCostumeName );
		}
		if( !nullStr( component->pcCostumeName ) && !ugcCostumeSpecifierExists( ugcProj, component->pcCostumeName )) {
			StructFreeStringSafe( &component->pcCostumeName );
			++*numCostumesReset;
		}
	}
	if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
		*numCostumesReset += ugcEditorFixupDialogTreeBlock( ugcProj, &component->dialogBlock, NULL, NULL );
	}
	if( component->eType == UGC_COMPONENT_TYPE_ACTOR ) {
		if( !nullStr( component->pcCostumeName ) && !ugcCostumeSpecifierExists( ugcProj, component->pcCostumeName )) {
			StructFreeStringSafe( &component->pcCostumeName );
			++*numCostumesReset;
		}
	}
}

static bool ugcComponentPlacementIsImplicit( const UGCComponent* component )
{
	return component->sPlacement.bIsExternalPlacement || component->sPlacement.uRoomID == GENESIS_UNPLACED_ID;
}

bool ugcComponentWhenConditionIsValid( UGCProjectData *ugcProj, const UGCComponent *component )
{
	if (!component->pStartWhen)
		return true;

	if (component->pStartWhen->eType == UGCWHEN_COMPONENT_REACHED ||
		component->pStartWhen->eType == UGCWHEN_COMPONENT_COMPLETE)
	{
		int i;
		for (i = 0; i < eaiSize(&component->pStartWhen->eauComponentIDs); i++)
		{
			UGCComponent *ref_component = ugcComponentFindByID( ugcProj->components, component->pStartWhen->eauComponentIDs[i] );
			if (ref_component)
				return true;
		}
	}

	return false;
}

static bool ugcComponentTimelineIsUnreachable( UGCProjectData* ugcProj, const UGCComponent* component )
{
	if (ugcComponentWhenConditionIsValid( ugcProj, component )) {
		return false;
	}

	if (!component->pStartWhen)
		return true;

	if( component->bIsDefault || component->pStartWhen->eType == UGCWHEN_OBJECTIVE_IN_PROGRESS ) {
		if( component->uActorID == 0 ) {
			return true;
		}
	}

	if( !component->bIsDefault && component->pStartWhen->eType != UGCWHEN_MISSION_START ) {
		bool referencedInObjectives = false;
		int it;
		for( it = 0; it != eaiSize( &component->eaObjectiveIDs ); ++it ) {
			UGCMissionObjective* objective = ugcObjectiveFind( ugcProj->mission->objectives, component->eaObjectiveIDs[ it ]);

			if( objective ) {
				referencedInObjectives = true;
			}
		}

		if( !referencedInObjectives ) {
			return true;
		}
	}

	return false;
}

static bool ugcEditorFixupComponentHasValidPlacement(const UGCProjectData *ugcProj, const UGCComponent* component)
{
	if( component->sPlacement.bIsExternalPlacement ) {
		// Cryptic map
		return true;
	} else {
		// Not a Cryptic Map
		const char *map_name = component->sPlacement.pcMapName;					
	
		if (!map_name)
			return false;

		FOR_EACH_IN_EARRAY(ugcProj->maps, UGCMap, map)
		{
			if (resNamespaceBaseNameEq(map->pcName, map_name))
			{
				return (component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP
						|| !map->pUnitializedMap);
			}
		}
		FOR_EACH_END;
		return false;
	}
}

static void ugcEditorFixupComponentTrap(UGCProjectData *ugcProj, UGCComponent *component)
{
	GroupDef *def = objectLibraryGetGroupDef(component->iObjectLibraryId, false);
	char power_group_name[256];

	if (!component->pcTrapPower && def && ugcGetTagValue(def->tags, "type", SAFESTR(power_group_name)))
	{
		UGCTrapPowerGroup *power_group = RefSystem_ReferentFromString(UGC_DICTIONARY_TRAP_POWER_GROUP, power_group_name);
		if (power_group)
		{
			component->pcTrapPower = StructAllocString(power_group->eaPowerNames[0]);
		}
	}
}

static void ugcEditorFixupFinalRewardBox(UGCProjectData *ugcProj)
{
	UGCComponent *reward_box_component = NULL;
	UGCComponent **copy_components = NULL;

	eaCopy(&copy_components, &ugcProj->components->eaComponents);

	// Ensure exactly 1 reward box exists, if supported. Delete all that are after that first 1. If not supported, delete them all.
	// This ensures that if we duplicate the last map or do anything else that adds another reward chest, it gets removed.
	FOR_EACH_IN_EARRAY_FORWARDS(copy_components, UGCComponent, component)
	{
		if( component->eType == UGC_COMPONENT_TYPE_REWARD_BOX )
		{
			if(ugcDefaultsIsFinalRewardBoxSupported()) // if supported, ensure only 1 exists
			{
				if(reward_box_component) // if already exists, delete
					ugcComponentOpDelete(ugcProj, component, true);
				else
					reward_box_component = component; // store the only 1 that should exist
			}
			else // not supported, so delete them all
				ugcComponentOpDelete(ugcProj, component, true);
		}
	}
	FOR_EACH_END;

	eaDestroy(&copy_components);

	// Ensure that the 1 reward chest is actually placed on the correct last map...
	if(ugcDefaultsIsFinalRewardBoxSupported()) // only if supported
	{
		if(eaSize(&ugcProj->mission->objectives)) // wait until we have objectives so we can know the last map
		{
			UGCMissionObjective *lastObjective = ugcProj->mission->objectives[eaSize(&ugcProj->mission->objectives) - 1];
			bool lastIsInternal;
			const char *lastMapName = ugcObjectiveMapName(ugcProj, lastObjective, &lastIsInternal);

			if(lastIsInternal) // last objective is on an UGC project internal map
			{
				UGCMap *map = ugcMapFindByName(ugcProj, lastMapName);
				char ns[RESOURCE_NAME_MAX_SIZE], base[RESOURCE_NAME_MAX_SIZE];
				resExtractNameSpace(map->pcName, ns, base);

				// If final reward box exists and is on last map (which is internal), return.
				if(reward_box_component && !reward_box_component->sPlacement.bIsExternalPlacement && (0 == stricmp(base, reward_box_component->sPlacement.pcMapName)))
					return;

				{
					Vec3 new_reward_pos;
					int new_reward_room = 0;
					bool valid_reward_pos = false;
					float fNewRewardRot=0.0f;

					// if we cannot get a default position, we will delete any pre-existing reward box at the bottom of this fixup function.
					if( ugcMapGetDefaultRewardBoxPosition(ugcProj, map, new_reward_pos, &fNewRewardRot, &new_reward_room ) )
					{
						// create or move the reward box
						if(reward_box_component)
						{
							ugcComponentOpSetParent(ugcProj, reward_box_component, new_reward_room);

							StructReset(parse_UGCComponentPlacement, &reward_box_component->sPlacement);
							reward_box_component->sPlacement.bIsExternalPlacement = false;
						}
						else
						{
							reward_box_component = ugcComponentOpCreate(ugcProj, UGC_COMPONENT_TYPE_REWARD_BOX, new_reward_room);
							assert(reward_box_component);
						}
						ugcComponentOpSetPlacement(ugcProj, reward_box_component, map, UGC_TOPLEVEL_ROOM_ID);
						copyVec3(new_reward_pos, reward_box_component->sPlacement.vPos);
						setVec3(reward_box_component->sPlacement.vRotPYR, 0, fNewRewardRot, 0);

						return; // we have fixed up final reward box to be on last (internal) map
					}
				}
			}
			else // last objective is on a Cryptic external map
			{
				// If final reward box exists and is specified on a Cryptic map, return.
				if(reward_box_component && reward_box_component->sPlacement.bIsExternalPlacement && !nullStr(reward_box_component->sPlacement.pcExternalMapName)
						&& !nullStr(reward_box_component->sPlacement.pcExternalObjectName))
					return;

				{
					ZoneMapEncounterInfo *chosenZeni = NULL;
					ZoneMapEncounterObjectInfo *chosenZeniObj = NULL;
					FOR_EACH_IN_REFDICT("ZoneMapEncounterInfo", ZoneMapEncounterInfo, zeni)
					{
						FOR_EACH_IN_EARRAY(zeni->objects, ZoneMapEncounterObjectInfo, zeniObj)
						{
							if(zeniObjIsUGC( zeniObj ) && zeniObj->interactType == WL_ENC_REWARD_BOX)
							{
								bool preferred = (stricmp(zeni->map_name, lastMapName) == 0);
								if(!chosenZeni)
								{
									chosenZeni = zeni;
									chosenZeniObj = zeniObj;
								}
								else
								{
									if(stricmp(chosenZeni->map_name, lastMapName) != 0 && stricmp(zeni->map_name, lastMapName) == 0) // preferred
									{
										chosenZeni = zeni;
										chosenZeniObj = zeniObj;
										break;
									}
								}
							}
						}
						FOR_EACH_END;
					}
					FOR_EACH_END;

					// if we cannot choose a default zeni object, we will delete any pre-existing reward box at the bottom of this fixup function.
					if(chosenZeni && chosenZeniObj)
					{
						if(reward_box_component)
						{
							ugcComponentOpSetParent(ugcProj, reward_box_component, 0);

							StructReset(parse_UGCComponentPlacement, &reward_box_component->sPlacement);
							reward_box_component->sPlacement.bIsExternalPlacement = true;
							StructCopyString(&reward_box_component->sPlacement.pcExternalMapName, chosenZeni->map_name);
							StructCopyString(&reward_box_component->sPlacement.pcExternalObjectName, chosenZeniObj->logicalName);
						}
						else
							reward_box_component = ugcComponentOpExternalObjectFindOrCreate(ugcProj, chosenZeni->map_name, chosenZeniObj->logicalName);

						return; // we have fixed up final reward box to be on last (Cryptic) map
					}
				}
			}
		}
	}

	// Here's all the reasons we may get down here and delete the reward box:
	//   The last map cannot have objects placed, yet, because it is not fully created or has no rooms.
	//   There are no objectives, so delete any Reward box that had been placed.
	//   In case we get rid of all tagged reward chests in cryptic maps.
	//   Maybe we used to support the final reward box, but then turned off its support?
	if(reward_box_component)
		ugcComponentOpDelete(ugcProj, reward_box_component, true);
}

static void ugcEditorFixupComponentDoor(UGCComponentList *components, UGCComponent *component)
{
	int num_rooms = 0;

	FOR_EACH_IN_EARRAY(components->eaComponents, UGCComponent, room_component)
	{
		if (ugcRoomIsDoorConnected(room_component, component, NULL))
			num_rooms++;
	}
	FOR_EACH_END;

	if (num_rooms < 2) {
		component->eType = UGC_COMPONENT_TYPE_FAKE_DOOR;
	} else {
		component->eType = UGC_COMPONENT_TYPE_ROOM_DOOR;
	}
}

static bool ugcEditorFixupComponentPatrols(UGCProjectData *ugcProj, UGCComponent *component)
{
	if (component->eType == UGC_COMPONENT_TYPE_KILL || component->eType == UGC_COMPONENT_TYPE_CONTACT)
	{
		int i;
		
		if( !ugcComponentHasPatrol( component, NULL ))
		{
			// FSM selected has no patrol; clear points if they exist
			if( eaiSize( &component->eaPatrolPoints )) {
				eaiDestroy(&component->eaPatrolPoints);
				return false;
			} else {
				return true;
			}
		}

		for (i = eaiSize(&component->eaPatrolPoints)-1; i >= 0; --i)
		{
			UGCComponent *point_component = ugcComponentFindByID(ugcProj->components, component->eaPatrolPoints[i]);
			if (!point_component)
				eaiRemove(&component->eaPatrolPoints, i);
		}

		if( eaiSize( &component->eaPatrolPoints ) == 0 && component->sPlacement.uRoomID != GENESIS_UNPLACED_ID ) {
			UGCComponent* newPoint = ugcComponentOpCreate(ugcProj, UGC_COMPONENT_TYPE_PATROL_POINT, component->uParentID);
			StructCopyAll( parse_UGCComponentPlacement, &component->sPlacement, &newPoint->sPlacement );
			newPoint->sPlacement.vPos[0] += 10;
			newPoint->uPatrolParentID = component->uID;
			eaiPush( &component->eaPatrolPoints, newPoint->uID );
		}
	}
	else
	{
		eaiDestroy(&component->eaPatrolPoints);
	}

	return true;
}

static void ugcEditorFixupComponentBehavior(UGCProjectData* ugcProj, UGCComponent *component, int* fixupFlags)
{
	UGCFSMMetadata* pFSMMetadata = ugcResourceGetFSMMetadata(component->fsmProperties.pcFSMNameRef);

	// Legacy support.  Vars used to be stored in WorldVariableDefs.
	// Now they are store in UGCFSMVar, a much simpler structure.
	if( eaSize( &component->fsmProperties.eaExternVarsV0 )) {
		eaDestroyStruct( &component->fsmProperties.eaExternVarsV1, parse_UGCFSMVar );
		FOR_EACH_IN_EARRAY_FORWARDS( component->fsmProperties.eaExternVarsV0, WorldVariableDef, varDef ) {
			if( varDef->pSpecificValue ) {
				UGCFSMVar* fsmVar = StructCreate( parse_UGCFSMVar );
				fsmVar->astrName = varDef->pcName;

				if( stricmp( varDef->pcName, "Patrol_Speed" ) == 0 ) {
					sscanf( varDef->pSpecificValue->pcStringVal, "%f", &fsmVar->floatVal );
				} else if( varDef->eType == WVAR_STRING || varDef->eType == WVAR_ANIMATION || varDef->eType == WVAR_MESSAGE ) {
					StructCopyString( &fsmVar->strStringVal, varDef->pSpecificValue->pcStringVal );
				} else if( varDef->eType == WVAR_INT ) {
					fsmVar->floatVal = varDef->pSpecificValue->iIntVal;
				} else if( varDef->eType == WVAR_FLOAT ) {
					fsmVar->floatVal = varDef->pSpecificValue->fFloatVal;
				}

				eaPush( &component->fsmProperties.eaExternVarsV1, fsmVar );
			}
		} FOR_EACH_END;
		eaDestroyStruct( &component->fsmProperties.eaExternVarsV0, parse_WorldVariableDef );
	}

	if( pFSMMetadata ) {	
		FOR_EACH_IN_EARRAY( component->fsmProperties.eaExternVarsV1, UGCFSMVar, fsmVar ) {
			if( !ugcResourceGetFSMExternVar( component->fsmProperties.pcFSMNameRef, fsmVar->astrName )) {
				StructDestroySafe( parse_UGCFSMVar, &fsmVar );
				eaRemove( &component->fsmProperties.eaExternVarsV1, FOR_EACH_IDX( component->fsmProperties.eaExternVarsV1, fsmVar ));
			}
		} FOR_EACH_END;

		FOR_EACH_IN_EARRAY_FORWARDS( pFSMMetadata->eaExternVars, UGCFSMExternVar, externVar ) {
			UGCFSMVar* fsmVar = ugcComponentBehaviorGetFSMVar( component, externVar->astrName );
			if( !fsmVar ) {
				fsmVar = StructCreate( parse_UGCFSMVar );
				fsmVar->astrName = externVar->astrName;
				eaPush( &component->fsmProperties.eaExternVarsV1, fsmVar );
				if( externVar->defProps.defaultValue ) {
					fsmVar->floatVal = externVar->defProps.defaultValue;
				}
			}

			if( stricmp( externVar->scType, "AllMissionsIndex" ) == 0 ) {
				if( !fsmVar->pWhenVal ) {
					fsmVar->pWhenVal = StructCreate( parse_UGCWhen );
					fsmVar->pWhenVal->eType = UGCWHEN_MANUAL;
				}
				ugcEditorFixupUGCWhen( ugcProj, component->eType, fsmVar->pWhenVal, NULL, false, component->sPlacement.pcMapName, fixupFlags );
			} else {
				StructDestroySafe( parse_UGCWhen, &fsmVar->pWhenVal );
			}
			if( externVar->defProps.minValue || externVar->defProps.maxValue ) {
				fsmVar->floatVal = CLAMP( fsmVar->floatVal, externVar->defProps.minValue, externVar->defProps.maxValue );
			}
		} FOR_EACH_END;
	} else {
		eaDestroyStruct( &component->fsmProperties.eaExternVarsV1, parse_UGCFSMVar );
	}
}

bool ugcEditorFixupComponentImpliedChildren(UGCProjectData *data, UGCComponent *component)
{
	bool needsRestart = false;

	// Unplaced components need to have no children so when they get
	// placed their children appear relative to them.
	if( component->sPlacement.uRoomID == GENESIS_UNPLACED_ID ) {
		if( ea32Size( &component->uChildIDs )) {
			needsRestart = true;
		}
		ugcComponentOpDeleteChildren( data, component, true );
		return needsRestart;
	}
	
	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_ROOM: case UGC_COMPONENT_TYPE_OBJECT: case UGC_COMPONENT_TYPE_SOUND:
		case UGC_COMPONENT_TYPE_CLUSTER:{
			// rooms, details, and clusters are allowed to have arbitrary children 
		}
		
		xcase UGC_COMPONENT_TYPE_KILL:
			if( ugcComponentCreateActorChildren( data, component )) {
				needsRestart = true;
			}

		xcase UGC_COMPONENT_TYPE_TRAP: {
			UGCTrapProperties* pTrapData = ugcTrapGetProperties( objectLibraryGetGroupDef( component->iObjectLibraryId, false ));
			if( !pTrapData || (eaiSize( &component->uChildIDs ) != eaSize( &pTrapData->eaEmitters ) + 2 //< need to add trigger, emitter
							   && !pTrapData->pSelfContained) ) {
				ugcComponentOpDelete( data, component, false );
				needsRestart = true;
			}
			StructDestroySafe( parse_UGCTrapProperties, &pTrapData );
		}

		xcase UGC_COMPONENT_TYPE_TELEPORTER: {
			UGCGroupDefMetadata* pDefData = ugcResourceGetGroupDefMetadataInt( component->iObjectLibraryId );

			if( !pDefData || eaiSize( &component->uChildIDs ) != eaSize( &pDefData->eaClusterChildren )) {
				ugcComponentOpDelete( data, component, false );
				needsRestart = true;
			}
		}
		
		xdefault:
			if( ea32Size( &component->uChildIDs )) {
				needsRestart = true;
			}
			ugcComponentOpDeleteChildren(data, component, true);
	}

	return needsRestart;
}

int ugcFixupGetIDFromName(const char* pOldName)
{
	GroupDef *pDef=NULL;
	
	// These are some hardcoded conversions. There were bugs that caused some private child groups to be usable in UGC projects.
	//  Some of these existed before the name -> id conversion, so we cannot rely on the ID to fix up the fact that we had to
	//  create new non-childed versions of these objects. These new objects are named the same as the original child,
	//  but without the "&". WOLF[15May12]
	if (stricmp(pOldName,"Ugc_Stf_Station_01_Holodisplay_02_2&")==0)
	{
		pDef = objectLibraryGetGroupDefByName( "Ugc_Stf_Station_01_Holodisplay_02_2", false );
	}
	else if (stricmp(pOldName,"Ugc_Stf_Pattern_Enhancer_02_1&")==0)
	{
		pDef = objectLibraryGetGroupDefByName( "Ugc_Stf_Pattern_Enhancer_02_1", false );
	}
	else if (stricmp(pOldName,"Ugc_Stf_Pattern_Enhancer_02_2&")==0)
	{
		pDef = objectLibraryGetGroupDefByName( "Ugc_Stf_Pattern_Enhancer_02_2", false );
	}
	else if (stricmp(pOldName,"Ugc_Stf_Station_Auctionlight_01_1&")==0)
	{
		pDef = objectLibraryGetGroupDefByName( "Ugc_Stf_Station_Auctionlight_01_Glow", false );
	}
	else if (stricmp(pOldName,"Ugc_Stf_Station_Auctionlight_01_2&")==0)
	{
		pDef = objectLibraryGetGroupDefByName( "Ugc_Stf_Station_Auctionlight_01_Fixture", false );
	}
	else if (stricmp(pOldName,"Ugc_Gen_Caves_Glowingplant_01#1")==0)
	{
		pDef = objectLibraryGetGroupDefByName( "Ugc_Gen_Caves_Glowingplant_01", false );
	}
	else if (stricmp(pOldName,"Ugc_Gen_Caves_Glowingplant_02#1")==0)
	{
		pDef = objectLibraryGetGroupDefByName( "Ugc_Gen_Caves_Glowingplant_02", false );
	}
	else if (stricmp(pOldName,"Ugc_Gen_Caves_Glowingplant_03#1")==0)
	{
		pDef = objectLibraryGetGroupDefByName( "Ugc_Gen_Caves_Glowingplant_03", false );
	}
	else if (stricmp(pOldName,"Ugc_Gen_Caves_Glowingplant_04#1")==0)
	{
		pDef = objectLibraryGetGroupDefByName( "Ugc_Gen_Caves_Glowingplant_04", false );
	}
		

	///  The regular conversion which takes the name and looks up the group def with that name
	else
	{
		pDef = objectLibraryGetGroupDefByName( pOldName, false );
	}

	if( pDef!=NULL )
	{
		return(pDef->name_uid);
	}
	return(0);
}


int ugcDataCorruptionDoesNotAssert;
AUTO_CMD_INT( ugcDataCorruptionDoesNotAssert, ugcDataCorruptionDoesNotAssert );

void ugcEditorFixupComponents(UGCProjectData* ugcProj, int* numDialogsDeleted, int* numCostumesReset, int* fixupFlags)
{
	int it;
	bool deletedComponents = false;
	bool needsRestart = false;

	if(!ugcProj->components)
		return;

	{
		UGCComponent **old_components = NULL;

		eaCopy( &old_components, &ugcProj->components->eaComponents );

		FOR_EACH_IN_EARRAY(old_components, UGCComponent, component) { // backwards
			if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				UGCDialogTreeBlock **blocksV1 = component->blocksV1;
				component->blocksV1 = NULL;
				// fixup dialog tree
				if( eaSize( &blocksV1 ) > 0 ) {
					StructCopyAll( parse_UGCDialogTreeBlock, blocksV1[ 0 ], &component->dialogBlock );
					component->dialogBlock.blockIndex = 0;

					for( it = 1; it < eaSize( &blocksV1 ); ++it ) {
						UGCComponent* newComponent = ugcComponentOpClone( ugcProj, component );
						StructCopyAll( parse_UGCDialogTreeBlock, blocksV1[ it ], &newComponent->dialogBlock );
						newComponent->dialogBlock.blockIndex = it;
					}

					if (fixupFlags)
						*fixupFlags |= UGC_FIXUP_MOVED_DIALOG_BLOCKS;
				}
				eaDestroyStruct( &blocksV1, parse_UGCDialogTreeBlock );
			}
		}
		FOR_EACH_END;
	
		eaDestroy(&old_components);
	}

	// Make sure we have the default components for the WHOLE_MAP and MISSION_RETURN
	{
		UGCComponent *pTheComponent;
		pTheComponent = ugcComponentOpExternalObjectFindOrCreate( ugcProj, ugcGetDefaultMapName( ugcProj ), "WHOLE_MAP" );
		pTheComponent = ugcComponentOpExternalObjectFindOrCreate( ugcProj, ugcGetDefaultMapName( ugcProj ), "MISSION_RETURN" );
	}

	do {
		UGCBacklinkTable* pBacklinkTable = NULL;
		ugcBacklinkTableRefresh( ugcProj, &pBacklinkTable );
		
		needsRestart = false;
		for( it = eaSize( &ugcProj->components->eaComponents )-1; it >= 0 ; it = MIN( it - 1, eaSize( &ugcProj->components->eaComponents ) - 1 )) {
			UGCComponent* component = ugcProj->components->eaComponents[ it ];
			UGCMissionObjective* objective = ugcObjectiveFindComponentRelated( ugcProj->mission->objectives, ugcProj->components, component->uID);

			if( ugcComponentAllow3DRotation( component->eType )) {
				component->sPlacement.vRotPYR[0] = fixAngleDeg(component->sPlacement.vRotPYR[0]);
				component->sPlacement.vRotPYR[1] = fixAngleDeg(component->sPlacement.vRotPYR[1]);
				component->sPlacement.vRotPYR[2] = fixAngleDeg(component->sPlacement.vRotPYR[2]);
			} else {
				component->sPlacement.vRotPYR[0] = 0;
				component->sPlacement.vRotPYR[1] = fixAngleDeg(component->sPlacement.vRotPYR[1]);
				component->sPlacement.vRotPYR[2] = 0;
			}

			// Fixup whole maps that are marked external even though
			// they shouldn't be.  This was caused by a shard
			// configuration bug causing the mission editor to create whole maps in the wrong state.
			if(   component->sPlacement.bIsExternalPlacement
				  && component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP
				  && namespaceIsUGCAnyShard( component->sPlacement.pcExternalMapName )) {
				// Delete any bogus objectives created
				while( objective ) {
					int index;
					UGCMissionObjective*** peaParentEArray = ugcObjectiveFindParentEA( &ugcProj->mission->objectives, objective->id, &index );
					assert( peaParentEArray );
					StructDestroy( parse_UGCMissionObjective, objective );
					eaRemove( peaParentEArray, index );
					objective = ugcObjectiveFindComponentRelated( ugcProj->mission->objectives, ugcProj->components, component->uID );
				}

				// Delete the component
				ugcComponentOpDelete( ugcProj, component, false );
				needsRestart = true;
				continue;
			}

			// Fixup fixed component types to have the right object library ID
			if( component->eType == UGC_COMPONENT_TYPE_REWARD_BOX ) {
				GroupDef* def = objectLibraryGetGroupDefByName( ugcDefaultsGetRewardBoxObjlib(), false );
				component->iObjectLibraryId = SAFE_MEMBER( def, name_uid );
			} else if( component->eType == UGC_COMPONENT_TYPE_RESPAWN ) {
				GroupDef* def = objectLibraryGetGroupDefByName( "UGC_Respawn_Point_With_Campfire", false );
				component->iObjectLibraryId = SAFE_MEMBER( def, name_uid );
			}

			// Fixup object library name -> id
			if( !component->iObjectLibraryId ) {
				component->iObjectLibraryId = ugcFixupGetIDFromName(component->pcOldObjectLibraryName);
				if (component->iObjectLibraryId!=0)
				{
					// We found an ID, we can delete the old name
					StructFreeStringSafe( &component->pcOldObjectLibraryName );
				}
			}
			if( !component->iPlanetRingId ) {
				component->iPlanetRingId=ugcFixupGetIDFromName(component->pcOldPlanetRingName);
				if(component->iPlanetRingId!=0)
				{
					// We found an ID, we can delete the old name
					StructFreeStringSafe( &component->pcOldPlanetRingName );
				}
			}

			// Fixup map type
			if (component->eMapType == UGC_MAP_TYPE_ANY && component->sPlacement.uRoomID != GENESIS_UNPLACED_ID &&
				component->sPlacement.pcMapName)
			{
				UGCMap *map = ugcMapFindByName(ugcProj, component->sPlacement.pcMapName);
				component->eMapType = ugcMapGetType(map);
			}

			// Fix up trigger conditions and properties
			ugcEditorFixupTriggers(ugcProj, pBacklinkTable, component, objective, fixupFlags);

			// Fix up contact/actor display names
			if (!component->bDisplayNameWasFixed)
			{
				component->bDisplayNameWasFixed=true;
				if (component->eType == UGC_COMPONENT_TYPE_ACTOR || component->eType == UGC_COMPONENT_TYPE_CONTACT)
				{
					// NOTE: The DisplayName may be NULL and we do need to delete anything in the Visible name in that case. It's likely
					//   one of the "ACTOR #38" style names which is of no use anymore.
					StructFreeString(component->pcVisibleName);
					component->pcVisibleName = component->pcDisplayName_DEPRECATED;
					component->pcDisplayName_DEPRECATED = NULL;
				}
			}

			if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				if (!ugcEditorFixupComponentDialogTree(ugcProj, component, objective, fixupFlags))
				{
					needsRestart = true;
					continue;
				}
			}

			// Dialogs and FSMs get deleted if they reference nothing
			if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE &&
				component->sPlacement.uRoomID == GENESIS_UNPLACED_ID && 
				ugcComponentTimelineIsUnreachable( ugcProj, component ) ) {
					if( ugcComponentOpDelete( ugcProj, component, false )) {
						++*numDialogsDeleted;
						needsRestart = true;
						continue;
					}
			}

			if( component->eType == UGC_COMPONENT_TYPE_PATROL_POINT ) {
				UGCComponent* patrolParent = ugcComponentFindByID( ugcProj->components, component->uPatrolParentID );
				if( !patrolParent || eaiFind( &patrolParent->eaPatrolPoints, component->uID ) == -1 ) {
					if( ugcComponentOpDelete( ugcProj, component, false )) {
						needsRestart = true;
						continue;
					}
				}
			}
			if (!ugcEditorFixupComponentPatrols(ugcProj, component))
			{
				needsRestart = true;
				continue;
			}

			if (component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR ||
				component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR)
			{
				ugcEditorFixupComponentDoor(ugcProj->components, component);

				if(   component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR && !component->iObjectLibraryId
					  && !component->bIsDoorExplicitDefault ) {
					if( ugcComponentOpDelete( ugcProj, component, false )) {
						needsRestart = true;
						continue;
					}
				}
				if( component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR || component->iObjectLibraryId ) {
					component->bIsDoorExplicitDefault = false;
				}
			}

			if (component->eType == UGC_COMPONENT_TYPE_TRAP)
			{
				ugcEditorFixupComponentTrap(ugcProj, component);
			}

			if (component->eType == UGC_COMPONENT_TYPE_KILL || component->eType == UGC_COMPONENT_TYPE_CONTACT)
			{
				ugcEditorFixupComponentBehavior(ugcProj, component, fixupFlags);
			}
			
			if( ugcEditorFixupComponentImpliedChildren(ugcProj, component)) {
				needsRestart = true;
				continue;
			}

			// This fixup has to be before the fixup of parent component positions.
			if( !component->bPatrolPointsFixed_FromComponentPosition ) {
				if( ugcComponentHasPatrol( component, NULL )) {
					UGCComponent* newPoint = ugcComponentOpCreate( ugcProj, UGC_COMPONENT_TYPE_PATROL_POINT, component->uParentID );
					StructCopyAll( parse_UGCComponentPlacement, &component->sPlacement, &newPoint->sPlacement );
					newPoint->uPatrolParentID = component->uID;
					eaiInsert( &component->eaPatrolPoints, newPoint->uID, 0 );
					needsRestart = true;
				}

				component->bPatrolPointsFixed_FromComponentPosition = true;
			}

			// Fixup some parent component positions to be the bounding box center of their children.
			if(   component->eType == UGC_COMPONENT_TYPE_TELEPORTER || component->eType == UGC_COMPONENT_TYPE_CLUSTER
				  || component->eType == UGC_COMPONENT_TYPE_KILL ) {
				if( eaiSize( &component->uChildIDs )) {
					UGCComponent* firstChild = ugcComponentFindByID( ugcProj->components, component->uChildIDs[ 0 ]);
					Vec3 minPosAccum;
					Vec3 maxPosAccum;
					int childIt;
					copyVec3( firstChild->sPlacement.vPos, minPosAccum );
					copyVec3( firstChild->sPlacement.vPos, maxPosAccum );

					for( childIt = 1; childIt != eaiSize( &component->uChildIDs ); ++childIt ) {
						UGCComponent* child = ugcComponentFindByID( ugcProj->components, component->uChildIDs[ childIt ]);
						MINVEC3( minPosAccum, child->sPlacement.vPos, minPosAccum );
						MAXVEC3( maxPosAccum, child->sPlacement.vPos, maxPosAccum );
					}
					setVec3( component->sPlacement.vPos,
							 (minPosAccum[ 0 ] + maxPosAccum[ 0 ]) / 2,
							 (minPosAccum[ 1 ] + maxPosAccum[ 1 ]) / 2,
							 (minPosAccum[ 2 ] + maxPosAccum[ 2 ]) / 2 );
				} else {
					setVec3( component->sPlacement.vPos, 0, 0, 0 );
				}
			}

			// Remove unreferenced costumes from components
			ugcEditorFixupUnreferencedCostumes(ugcProj, component, numCostumesReset);

			ugcEditorFixupComponentDetails(component);

			if(   ugcComponentPlacementIsImplicit( component ) && !ugcComponentIsUsedInObjectives( ugcProj->components, component, ugcProj->mission->objectives ) &&
				!ugcComponentIsUsedInLinks( component, ugcProj->mission )) {
					if(   component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE
						  && component->eType != UGC_COMPONENT_TYPE_CONTACT
						  && component->eType != UGC_COMPONENT_TYPE_OBJECT
						  && component->eType != UGC_COMPONENT_TYPE_SOUND
						  && component->eType != UGC_COMPONENT_TYPE_SPAWN
						  && component->eType != UGC_COMPONENT_TYPE_REWARD_BOX
						  && component->eType != UGC_COMPONENT_TYPE_WHOLE_MAP) {
						if( ugcComponentOpDelete(ugcProj, component, false)) {
							needsRestart = true;
							continue;
						}
					}
					if( component->eType == UGC_COMPONENT_TYPE_CONTACT || component->eType == UGC_COMPONENT_TYPE_OBJECT || component->eType == UGC_COMPONENT_TYPE_SOUND ) {
						int otherIt;
						for( otherIt = 0; otherIt != eaSize( &ugcProj->components->eaComponents ); ++otherIt ) {
							UGCComponent* otherComponent = ugcProj->components->eaComponents[ otherIt ];
							if( otherComponent == component ) {
								continue;
							}

							if( otherComponent->uActorID == component->uID ) {
								break;
							}
						}

						if( otherIt >= eaSize( &ugcProj->components->eaComponents )) {
							if( ugcComponentOpDelete( ugcProj, component, false )) {
								needsRestart = true;
								continue;
							}
						}
					}
					if( component->eType == UGC_COMPONENT_TYPE_SPAWN )
					{
						// Never delete the component with the default name. It is required even if not referenced
						if (stricmp(component->sPlacement.pcExternalMapName,ugcGetDefaultMapName( ugcProj ))!=0)
						{
							if( ugcComponentOpDelete( ugcProj, component, false )) {
								needsRestart = true;
								continue;
							}
						}
					}
					if( component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP )
					{	
						// Never delete the component with the default name. It is required even if not referenced
						if (stricmp(component->sPlacement.pcExternalMapName,ugcGetDefaultMapName( ugcProj ))!=0)
						{
							if( component->sPlacement.bIsExternalPlacement ) {
								if( ugcComponentOpDelete( ugcProj, component, false )) {
									needsRestart = true;
									continue;
								}
							} else if( !ugcMapFindByName( ugcProj, component->sPlacement.pcMapName )) {
								if( ugcComponentOpDelete( ugcProj, component, false )) {
									needsRestart = true;
									continue;
								}
							}
						}
					}
			}
			else
			{
				// Find components on deleted rooms or maps and delete them or move them to 'Unplaced'
				if (!ugcEditorFixupComponentHasValidPlacement(ugcProj, component))
				{
					// Implicitly created components should never be deleted automatically
					if(   component->eType != UGC_COMPONENT_TYPE_ACTOR
						  && component->eType != UGC_COMPONENT_TYPE_TRAP_EMITTER
						  && component->eType != UGC_COMPONENT_TYPE_TRAP_TRIGGER
						  && component->eType != UGC_COMPONENT_TYPE_TRAP_TARGET ) {
						U32 componentID = component->uID;
						char componentName[ 256 ];

						ugcComponentGetDisplayName( componentName, ugcProj, component, false );
						
						if( ugcComponentOpDelete(ugcProj, component, false)) {
							printf("Component %s abandoned -- deleting.\n", componentName);
							needsRestart = true;
							continue;
						} else {
							// Many dialog trees are always unplaced,
							// and would trigger this message after
							// every change.
							if( component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE ) {
								printf("Component %s abandoned -- moving to unplaced.\n", componentName);
							}
						}
					}
				}
			}

			// Check that all the component's children exist and have
			// this component as a parent
			{
				int childIt;
				for( childIt = ea32Size( &component->uChildIDs ) - 1; childIt >= 0; --childIt ) {
					UGCComponent* childComponent = ugcComponentFindByID( ugcProj->components, component->uChildIDs[ childIt ]);
					if( ugcDataCorruptionDoesNotAssert || ugcGetIsRepublishing() ) {
						if( !childComponent ) {
							Alertf( "Data corrpution found: Component %d's child, Component %d, does not exist!  Removing.",
									component->uID, component->uChildIDs[ childIt ]);
							ea32Remove( &component->uChildIDs, childIt );
						} else if( childComponent->uParentID != component->uID ) {
							Alertf( "Data corruption found: Component %d's child, Component %d, lists a different component as its parent!  Removing.",
									component->uID, component->uChildIDs[ childIt ]);
							ea32Remove( &component->uChildIDs, childIt );
						}
					} else {
						assert( childComponent );
						assert( childComponent->uParentID == component->uID );
					}
				}
			}

			// Check that no child is listed twice
			{
				int childIt1;
				int childIt2;
				for( childIt1 = 0; childIt1 != ea32Size( &component->uChildIDs ); ++childIt1 ) {
					for( childIt2 = ea32Size( &component->uChildIDs ) - 1; childIt2 > childIt1; --childIt2 ) {
						if( ugcDataCorruptionDoesNotAssert || ugcGetIsRepublishing() ) {
							if( component->uChildIDs[ childIt1 ] == component->uChildIDs[ childIt2 ]) {
								Alertf( "Data corrpution found: Component %d's child, Component %d appears more than once!",
										component->uID, component->uChildIDs[ childIt1 ]);
								ea32Remove( &component->uChildIDs, childIt2 );
							}
						} else {
							assert( component->uChildIDs[ childIt1 ] != component->uChildIDs[ childIt2 ] );
						}
					}
				}
			}

			// Check that every component's parent exists
			if( component->uParentID ) {
				UGCComponent* parentComponent = ugcComponentFindByID( ugcProj->components, component->uParentID );
				if( ugcDataCorruptionDoesNotAssert || ugcGetIsRepublishing() ) {
					if( !parentComponent ) {
						Alertf( "Data corruption found: Component %d's parent, Component %d, does not exist!  Removing parent ref.",
								component->uID, component->uParentID );
						component->uParentID = 0;
					}
				} else {
					assert( parentComponent );
				}
			}

			// Some components are not allowed to be adjusted in the Y position
			if(   component->eType == UGC_COMPONENT_TYPE_ROOM || component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR
				  || component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR ) {
				component->sPlacement.vPos[ 1 ] = 0;
			}
		}

		ugcBacklinkTableDestroy( &pBacklinkTable );
	} while( needsRestart );
}

//////////////////////////////////////////////
// Mission Fixup
//////////////////////////////////////////////

void ugcEditorFixupMissionObjectiveHelper(UGCProjectData *ugcProj, UGCMission *mission, UGCMissionObjective *obj)
{
	FOR_EACH_IN_EARRAY(obj->eaChildren, UGCMissionObjective, child)
	{
		ugcEditorFixupMissionObjectiveHelper(ugcProj, mission, child);
	}
	FOR_EACH_END;

	// fixup components holding data
	if( obj->type == UGCOBJ_COMPLETE_COMPONENT || obj->type == UGCOBJ_UNLOCK_DOOR ) {
		UGCComponent* component = ugcComponentFindByID(ugcProj->components, obj->componentID );
		if( component ) {
			if( component->eType == UGC_COMPONENT_TYPE_OBJECT || component->eType == UGC_COMPONENT_TYPE_BUILDING_DEPRECATED ) {
				if( !nullStr( component->pcOldInteractText )) {
					StructCopyString( &obj->sInteractProps.pcInteractText, component->pcOldInteractText );
					StructFreeStringSafe( &component->pcOldInteractText );
				}
				if( IS_HANDLE_ACTIVE( component->hOldInteractAnim )) {
					COPY_HANDLE( obj->sInteractProps.hInteractAnim, component->hOldInteractAnim );
					REMOVE_HANDLE( component->hOldInteractAnim );
				}
				if( component->eOldInteractDuration != UGCDURATION_MEDIUM ) {
					obj->sInteractProps.eInteractDuration = component->eOldInteractDuration;
					component->eOldInteractDuration = UGCDURATION_MEDIUM;
				}
			}

			if( component->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				UGCComponent* contact = ugcComponentFindByID( ugcProj->components, component->uActorID );
				StructCopyString( &obj->strComponentInternalMapName, SAFE_MEMBER( contact, sPlacement.pcMapName ));
			} else {
				StructCopyString( &obj->strComponentInternalMapName, component->sPlacement.pcMapName );
			}
		}
	}

	// Fixup Invalid waypoint values -- Cryptic maps can't have AREA waypoints
	if( nullStr( obj->strComponentInternalMapName ) && obj->waypointMode == UGC_WAYPOINT_AREA ) {
		obj->waypointMode = UGC_WAYPOINT_POINTS;
	}

	if( obj->sInteractProps.succeedCheckedAttrib ) {
		if( !ugcEditorFixupCheckedAttrib( ugcProj, obj->sInteractProps.succeedCheckedAttrib )) {
			StructDestroySafe( parse_UGCCheckedAttrib, &obj->sInteractProps.succeedCheckedAttrib );
		}
	}
}

void ugcEditorFixupObjectivesComponentMapNames(UGCProjectData *ugcProj, UGCMissionObjective** objectives, int* numObjectivesReset)
{
	FOR_EACH_IN_EARRAY(objectives, UGCMissionObjective, objective) {
		if( objective->componentID ) {
			const char* mapName = ugcObjectiveInternalMapName( ugcProj, objective );
			UGCComponent* oldComponent = ugcComponentFindByID( ugcProj->components, objective->componentID );
			UGCComponent* dialogTree = NULL;
					
			if( oldComponent->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				dialogTree = oldComponent;
				oldComponent = ugcComponentFindByID( ugcProj->components, oldComponent->uActorID );
			}

			if( !mapName ) {
				if( objective->strComponentInternalMapName ) {
					UGCComponent* newComponent = ugcComponentOpCreate( ugcProj, oldComponent->eType, 0 );

					if( dialogTree ) {
						dialogTree->uActorID = newComponent->uID;
					} else {
						objective->componentID = newComponent->uID;
					}
					ugcComponentOpSetPlacement(ugcProj, newComponent, ugcMapFindByName(ugcProj, objective->strComponentInternalMapName), GENESIS_UNPLACED_ID);
					
					if( numObjectivesReset ) {
						*numObjectivesReset += 1;
					}
				} else {
					if( !oldComponent->sPlacement.bIsExternalPlacement ) {
						ugcComponentOpSetPlacement(ugcProj, oldComponent, NULL, GENESIS_UNPLACED_ID);
						oldComponent->sPlacement.bIsExternalPlacement = true;
						oldComponent->sPlacement.pcExternalMapName = StructAllocString( ugcGetDefaultMapName( ugcProj ));
					}
				}
			} else {
				if( stricmp( mapName, objective->strComponentInternalMapName ) != 0 ) {
					UGCComponent* newComponent;

					if( oldComponent->sPlacement.uRoomID == GENESIS_UNPLACED_ID ) {
						ugcComponentOpSetPlacement(ugcProj, oldComponent, NULL, 0);
						oldComponent->sPlacement.bIsExternalPlacement = false;
						StructFreeStringSafe( &oldComponent->sPlacement.pcExternalMapName );
						
						newComponent = oldComponent;
					} else {
						newComponent = ugcComponentOpCreate( ugcProj, oldComponent->eType, 0 );
					}

					if( dialogTree ) {
						dialogTree->uActorID = newComponent->uID;
					} else {
						objective->componentID = newComponent->uID;
					}

					if( objective->strComponentInternalMapName ) {
						ugcComponentOpSetPlacement(ugcProj, newComponent, ugcMapFindByName(ugcProj, objective->strComponentInternalMapName), GENESIS_UNPLACED_ID);
					} else {
						ugcComponentOpSetPlacement(ugcProj, newComponent, NULL, GENESIS_UNPLACED_ID);
						newComponent->sPlacement.bIsExternalPlacement = true;
						newComponent->sPlacement.pcExternalMapName = StructAllocString( ugcGetDefaultMapName( ugcProj ));
					}
					
					if( numObjectivesReset ) {
						*numObjectivesReset += 1;
					}
				}
			}
		}

		ugcEditorFixupObjectivesComponentMapNames( ugcProj, objective->eaChildren, numObjectivesReset );
	} FOR_EACH_END;
}

static void ugcEditorMapLinkSearchHelper(UGCProjectData *ugcProj, UGCMissionObjective **objectives, UGCMissionMapLink ***links_array)
{
	FOR_EACH_IN_EARRAY(objectives, UGCMissionObjective, objective)
	{
		UGCMissionMapLink* link = ugcMissionFindLinkByObjectiveID(ugcProj, objective->id, false);
		if (link && eaFind(links_array, link) == -1)
			eaPush(links_array, link);

		ugcEditorMapLinkSearchHelper(ugcProj, objective->eaChildren, links_array);
	}
	FOR_EACH_END;
}

void ugcEditorFixupMission(UGCProjectData *ugcProj, int* numCostumesReset, int* numObjectivesReset)
{
	UGCMission *mission = ugcProj->mission;
	UGCMissionMapLink **map_link_list = NULL;

	if(!mission)
		return;

	FOR_EACH_IN_EARRAY(mission->objectives, UGCMissionObjective, obj)
	{
		ugcEditorFixupMissionObjectiveHelper(ugcProj, mission, obj);
	}
	FOR_EACH_END;

	FOR_EACH_IN_EARRAY(mission->map_links, UGCMissionMapLink, link)
	{
		UGCComponent *door_component = ugcComponentFindByID(ugcProj->components, link->uDoorComponentID);
		UGCComponent *spawn_component = ugcComponentFindByID(ugcProj->components, link->uSpawnComponentID);

		if (!door_component)
		{
			link->uDoorComponentID = 0;
		}
		if (!spawn_component)
		{
			link->uSpawnComponentID = 0;
		}
		if ((!door_component || door_component->sPlacement.bIsExternalPlacement)
			&& !link->uSpawnComponentID && !ugcMapFindByName(ugcProj, link->strSpawnInternalMapName))
		{
			// We cannot infer the original map; must delete link.
			StructDestroy(parse_UGCMissionMapLink, link);
			eaRemove(&mission->map_links, FOR_EACH_IDX(mission->map_links, link));
			continue;
		}

		if( link->pDialogPrompt ) {
			UGCDialogTreePrompt* defaultTransitionPrompt = ugcGetDefaults()->pDefaultTransitionPrompt;

			if( !defaultTransitionPrompt ) {
				*numCostumesReset += ugcEditorFixupDialogTreePrompt( ugcProj, link->pDialogPrompt, NULL, NULL );
			} else {
				*numCostumesReset += ugcEditorFixupDialogTreePrompt( ugcProj, link->pDialogPrompt, defaultTransitionPrompt->pcPromptCostume, REF_STRING_FROM_HANDLE( defaultTransitionPrompt->hPromptPetCostume ));
			}
		}

		if( spawn_component ) {
			StructCopyString( &link->strSpawnInternalMapName, spawn_component->sPlacement.pcMapName );
		}
	}
	FOR_EACH_END;

	// Prune unused map links
	ugcEditorMapLinkSearchHelper(ugcProj, mission->objectives, &map_link_list);
	FOR_EACH_IN_EARRAY(mission->map_links, UGCMissionMapLink, link)
	{
		if (eaFind(&map_link_list, link) == -1)
		{
			// No references to this link; delete
			StructDestroy(parse_UGCMissionMapLink, link);
			eaRemove(&mission->map_links, FOR_EACH_IDX(mission->map_links, link));
			continue;
		}
	}
	FOR_EACH_END;
	eaDestroy(&map_link_list);

	// Create the final map link, only if necessary
	if( ugcDefaultsMapTransitionsSpecifyDoor() && eaSize( &mission->objectives )) {
		UGCMissionObjective* lastObjective = mission->objectives[ eaSize( &mission->objectives ) - 1 ];
		bool lastIsInternal;
		const char* lastMapName = ugcObjectiveMapName( ugcProj, lastObjective, &lastIsInternal );

		if( lastIsInternal ) {
			UGCComponent* returnDoorComponent;
			if( !mission->return_map_link ) {
				mission->return_map_link = StructCreate( parse_UGCMissionMapLink );
			}

			returnDoorComponent = ugcComponentFindByID( ugcProj->components, mission->return_map_link->uDoorComponentID );
			if( !returnDoorComponent || !ugcComponentIsOnMap( returnDoorComponent, lastMapName, true )) {
				char mapNameWithNs[ RESOURCE_NAME_MAX_SIZE ];
				sprintf( mapNameWithNs, "%s:%s", ugcProj->ns_name, lastMapName );
				returnDoorComponent = ugcMissionGetDefaultComponentForMap( ugcProj, UGC_COMPONENT_TYPE_WHOLE_MAP, mapNameWithNs );
				mission->return_map_link->uDoorComponentID = SAFE_MEMBER( returnDoorComponent, uID );
			}
		} else {
			StructDestroySafe( parse_UGCMissionMapLink, &mission->return_map_link );
		}
	} else {
		StructDestroySafe( parse_UGCMissionMapLink, &mission->return_map_link );
	}

	ugcEditorFixupObjectivesComponentMapNames(ugcProj, ugcProj->mission->objectives, numObjectivesReset);
}

void ugcEditorFixupCostumes( UGCProjectData* ugcProj )
{
	FOR_EACH_IN_EARRAY( ugcProj->costumes, UGCCostume, ugcCostume ) {
		bool isAdvanced = ugcCostume->data.isAdvanced;
		
		switch( ugcDefaultsCostumeEditorStyle() ) {
			xcase UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR:
				if( IsClient() ) {
					StructDestroySafe( parse_PlayerCostume, &ugcCostume->pCachedPlayerCostume );
					ugcCostume->pCachedPlayerCostume = ugcCostumeGeneratePlayerCostume( ugcCostume, NULL, NULL );
				}
				
			xcase UGC_COSTUME_EDITOR_STYLE_NEVERWINTER:
			{
				UGCCostumeMetadata* pCostumeMetaData = ugcResourceGetCostumeMetadata( ugcCostume->data.astrPresetCostumeName );
				if (pCostumeMetaData)
				{
					PlayerCostume* preset = pCostumeMetaData->pFullCostume;
					PCSkeletonDef* skel = GET_REF( preset->hSkeleton );
					UGCCostumeSkeletonSlotDef* skeletonDef = ugcDefaultsCostumeSkeletonDef( skel->pcName );

					// delete any unused body scales
					FOR_EACH_IN_EARRAY( ugcCostume->data.eaBodyScales, UGCCostumeScale, scale ) {
						if( !ugcCostumeFindBodyScaleDef( skel->pcName, scale->astrName, true, isAdvanced )) {
							StructDestroy( parse_UGCCostumeScale, scale );
							eaRemove( &ugcCostume->data.eaBodyScales, FOR_EACH_IDX( ugcCostume->data.eaBodyScales, scale ));
						}
					} FOR_EACH_END;

					// delete any unused scale infos
					FOR_EACH_IN_EARRAY( ugcCostume->data.eaScales, UGCCostumeScale, scale ) {
						const PCScaleInfo* scaleDef = costumeTailor_FindScaleInfoByName( skel, scale->astrName );
						if(   !scaleDef // || scaleDef->fPlayerMin >= scaleDef->fPlayerMax
							  || !ugcCostumeFindScaleInfoDef( skel->pcName, scale->astrName, true, isAdvanced )) {
							StructDestroy( parse_UGCCostumeScale, scale );
							eaRemove( &ugcCostume->data.eaScales, FOR_EACH_IDX( ugcCostume->data.eaScaleInfos, scale ));
						}
					} FOR_EACH_END;

					// delete any unused parts
					FOR_EACH_IN_EARRAY( ugcCostume->data.eaParts, UGCCostumePart, part ) {
						if( !ugcCostumeFindPartDef( skel->pcName, part->astrBoneName, true, isAdvanced )) {
							StructDestroy( parse_UGCCostumePart, part );
							eaRemove( &ugcCostume->data.eaParts, FOR_EACH_IDX( ugcCostume->data.eaParts, part ));
						}
					} FOR_EACH_END;

					// delete any unused slots
					FOR_EACH_IN_EARRAY( ugcCostume->data.eaSlots, UGCCostumeSlot, slot ) {
						if( !ugcCostumeFindSlotDef( skel->pcName, slot->astrSlot )) {
							StructDestroy( parse_UGCCostumeSlot, slot );
							eaRemove( &ugcCostume->data.eaSlots, FOR_EACH_IDX( ugcCostume->data.eaSlots, slot ));
						}
					} FOR_EACH_END;

					{
						PCRegion** eaCostumeRegions = NULL;
						int it;

						costumeTailor_GetValidRegions( CONTAINER_NOCONST( PlayerCostume, preset ), GET_REF( preset->hSpecies ), NULL, NULL, NULL, &eaCostumeRegions, CGVF_UNLOCK_ALL );

						for( it = 0; it != eaSize( &eaCostumeRegions ); ++it ) {
							UGCCostumeRegionDef* regionDef = ugcDefaultsCostumeRegionDef( eaCostumeRegions[ it ]->pcName );

							if( !regionDef ) {
								continue;
							}

							// create any needed body scales
							FOR_EACH_IN_EARRAY( regionDef->nwBasic.eaBodyScales, const char, name ) {
								if( !ugcCostumeFindBodyScale( ugcCostume, name )) {
									UGCCostumeScale* scale = StructCreate( parse_UGCCostumeScale );
									int scaleIndex = costumeTailor_FindBodyScaleInfoIndexByName( preset, name );
									scale->astrName = allocAddString( name );
									scale->value = eafGet( &preset->eafBodyScales, scaleIndex );
									eaPush( &ugcCostume->data.eaBodyScales, scale );
								}
							} FOR_EACH_END;
							FOR_EACH_IN_EARRAY( regionDef->nwAdvanced.eaBodyScales, const char, name ) {
								if( !ugcCostumeFindBodyScale( ugcCostume, name )) {
									UGCCostumeScale* scale = StructCreate( parse_UGCCostumeScale );
									int scaleIndex = costumeTailor_FindBodyScaleInfoIndexByName( preset, name );
									scale->astrName = allocAddString( name );
									scale->value = eafGet( &preset->eafBodyScales, scaleIndex );
									eaPush( &ugcCostume->data.eaBodyScales, scale );
								}
							} FOR_EACH_END;

							// create any needed scale infos
							FOR_EACH_IN_EARRAY( regionDef->nwBasic.eaScaleInfos, const char, name ) {
								const PCScaleInfo* scaleDef = costumeTailor_FindScaleInfoByName( skel, name );
								if(   scaleDef // && scaleDef->fPlayerMin < scaleDef->fPlayerMax
									  && !ugcCostumeFindScaleInfo( ugcCostume, name )) {
									UGCCostumeScale* scale = StructCreate( parse_UGCCostumeScale );
									const PCScaleValue* value = costumeTailor_FindScaleValueByName( preset, name );
									scale->astrName = allocAddString( name );
									scale->value = SAFE_MEMBER( value, fValue );
									eaPush( &ugcCostume->data.eaScales, scale );
								}
							} FOR_EACH_END;
							FOR_EACH_IN_EARRAY( regionDef->nwAdvanced.eaScaleInfos, const char, name ) {
								const PCScaleInfo* scaleDef = costumeTailor_FindScaleInfoByName( skel, name );
								if(   scaleDef // && scaleDef->fPlayerMin < scaleDef->fPlayerMax
									  && !ugcCostumeFindScaleInfo( ugcCostume, name )) {
									UGCCostumeScale* scale = StructCreate( parse_UGCCostumeScale );
									const PCScaleValue* value = costumeTailor_FindScaleValueByName( preset, name );
									scale->astrName = allocAddString( name );
									scale->value = SAFE_MEMBER( value, fValue );
									eaPush( &ugcCostume->data.eaScales, scale );
								}
							} FOR_EACH_END;

							// create any needed parts (leave everything as null so we can say it is "default")
							FOR_EACH_IN_EARRAY( regionDef->nwBasic.eaParts, UGCCostumeNWPartDef, partDef ) {
								if( !ugcCostumeFindPart( ugcCostume, partDef->astrName )) {
									UGCCostumePart* part = StructCreate( parse_UGCCostumePart );
									part->astrBoneName = allocAddString( partDef->astrName );
									eaPush( &ugcCostume->data.eaParts, part );
								}
							} FOR_EACH_END;
							FOR_EACH_IN_EARRAY( regionDef->nwAdvanced.eaParts, UGCCostumeNWPartDef, partDef ) {
								if( !ugcCostumeFindPart( ugcCostume, partDef->astrName )) {
									UGCCostumePart* part = StructCreate( parse_UGCCostumePart );
									part->astrBoneName = allocAddString( partDef->astrName );
									eaPush( &ugcCostume->data.eaParts, part );
								}
							} FOR_EACH_END;
						}

						eaDestroy( &eaCostumeRegions );
					}


					if( skeletonDef ) {
						// create any needed slots
						FOR_EACH_IN_EARRAY( skeletonDef->eaSlotDef, UGCCostumeSlotDef, def ) {
							UGCCostumeSlot* slot = ugcCostumeFindSlot( ugcCostume, def->astrName );
							if( !slot ) {
								slot = StructCreate( parse_UGCCostumeSlot );
								slot->astrSlot = allocAddString( def->astrName );
								eaPush( &ugcCostume->data.eaSlots, slot );
							}
						} FOR_EACH_END;
					}

					if( IsClient() ) {
						StructDestroySafe( parse_PlayerCostume, &ugcCostume->pCachedPlayerCostume );
						ugcCostume->pCachedPlayerCostume = ugcCostumeGeneratePlayerCostume( ugcCostume, NULL, NULL );
					}
				}
			}
		}
	} FOR_EACH_END;
}


bool ugcEditorFixupCheckedAttrib( UGCProjectData* ugcProj, UGCCheckedAttrib* checkedAttrib )
{
	// Fixup attribs that have legacy "PlayerHasItem" skill.
	if( checkedAttrib->astrSkillName == allocAddString( "PlayerHasItem" )) {
		checkedAttrib->astrSkillName = NULL;
	}

	if( !nullStr( checkedAttrib->astrItemName )) {
		UGCItem* item = ugcItemFindByName( ugcProj, checkedAttrib->astrItemName );
		if( !item ) {
			checkedAttrib->astrItemName = NULL;
		}
	}

	// Return if this checked attrib is set
	return !nullStr( checkedAttrib->astrSkillName ) || !nullStr( checkedAttrib->astrItemName );
}

//////////////////////////////////////////////
// Restrictions Fixup
//////////////////////////////////////////////

static void ugcMapLocationFromMapName( UGCMapLocation* out_mapLocation, const char* mapName )
{
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	
	mapName = allocAddString( mapName );
	if( !mapName ) {
		return;
	}

	FOR_EACH_IN_EARRAY_FORWARDS( defaults->eaMapRegions, UGCOverworldMapRegion, mapRegion ) {
		if( mapName == mapRegion->astrMapName ) {
			StructCopy( parse_UGCMapLocation, mapRegion->pMapLocation, out_mapLocation, 0, 0, 0 );
		}		
	} FOR_EACH_END;
}

static const char* ugcMapLocationSearchLocation( UGCProjectData* ugcProj )
{
	const UGCMapLocation* pMapLocation = ugcProj->project->pMapLocation;
	UGCPerProjectDefaults* defaults = ugcGetDefaults();
	
	// Pick the first matching region so first in data is best.
	//
	// Note: If this gets a better editor than text, using order may
	// make less sense.  Right now it allows finer grain control than
	// something like choosing the closest center.
	FOR_EACH_IN_EARRAY_FORWARDS( defaults->eaMapRegions, UGCOverworldMapRegion, mapRegion ) {
		FOR_EACH_IN_EARRAY( mapRegion->eaRects, UGCRect, rect ) {
			if(   rect->x <= pMapLocation->positionX && pMapLocation->positionX <= rect->x + rect->w
				  && rect->y <= pMapLocation->positionY && pMapLocation->positionY <= rect->y + rect->h ) {
				return mapRegion->astrMapName;
			}
		} FOR_EACH_END;
	} FOR_EACH_END;

	if( ugcGetAllegianceDefaults( ugcProj )) {
		return ugcGetAllegianceDefaults( ugcProj )->pcDefaultCrypticMap;
	}
	
	return NULL;
}

void ugcEditorFixupProject(UGCProjectData* ugcProj)
{
	ugcEditorFixupFinalRewardBox(ugcProj);

	// Fixup the location
	if( ugcDefaultsPreviewImagesAndOverworldMapEnabled() ) {
		UGCMissionObjective* firstObjective = eaGet( &ugcProj->mission->objectives, 0 );
		
		if( !ugcProj->project->pMapLocation ) {
			ugcProj->project->pMapLocation = StructCreate( parse_UGCMapLocation );
		}

		ugcProj->project->pMapLocation->positionX = -1;
		ugcProj->project->pMapLocation->positionY = -1;
		ugcProj->project->pMapLocation->astrIcon = NULL;
		StructFreeStringSafe( &ugcProj->project->strSearchLocation );

		if( firstObjective ) {
			if( !firstObjective->strComponentInternalMapName ) {
				// Objective is on a Cryptic map
				UGCComponent* component = ugcComponentFindByID( ugcProj->components, firstObjective->componentID );
				const char* astrCrypticMapName = NULL;
				if( !component ) {
					// No component yet, default to the default
					astrCrypticMapName = ugcGetAllegianceDefaults( ugcProj )->pcDefaultCrypticMap;
				} else {
					astrCrypticMapName = component->sPlacement.pcExternalMapName;
				}

				ugcMapLocationFromMapName( ugcProj->project->pMapLocation, astrCrypticMapName );
				StructCopyString( &ugcProj->project->strSearchLocation, astrCrypticMapName );
			} else {
				UGCMissionMapLink* mapLink = ugcMissionFindLinkByMap( ugcProj, firstObjective->strComponentInternalMapName );

				if( SAFE_MEMBER( mapLink, pDoorMapLocation )) {
					StructCopy( parse_UGCMapLocation, mapLink->pDoorMapLocation, ugcProj->project->pMapLocation, 0, 0, 0 );
				}
				StructCopyString( &ugcProj->project->strSearchLocation, ugcMapLocationSearchLocation( ugcProj ));
			}
		}
	}

	// Fixup the languages
	if( !langIsSupportedThisShard( ugcProj->project->eLanguage )) {
		ugcProj->project->eLanguage = locGetLanguage( getCurrentLocale() );
	}

	// Fixup the restrictions
	{
		WorldUGCRestrictionProperties effectiveRestrictions = { 0 };
		WorldUGCRestrictionProperties* projRestrictions;

		if( !ugcProj->project->pRestrictionProperties ) {
			ugcProj->project->pRestrictionProperties = StructCreate( parse_WorldUGCRestrictionProperties );
		}
		projRestrictions = ugcProj->project->pRestrictionProperties;

		// Restrict based on components
		{
			int it;
			for( it = 0; it != eaSize( &ugcProj->components->eaComponents ); ++it ) {
				UGCComponent* component = ugcProj->components->eaComponents[ it ];

				if( component->iObjectLibraryId ) {
					const WorldUGCProperties* ugcProps = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", component->iObjectLibraryId );

					if( ugcProps ) {
						ugcRestrictionsIntersect( &effectiveRestrictions, &ugcProps->restrictionProps );
					}
				}
			}
		}

		if (ugcIsAllegianceEnabled())
		{
			int it;
			eaDestroyStruct( &effectiveRestrictions.eaFactions, parse_WorldUGCFactionRestrictionProperties );
			for( it = 0; it != eaSize( &projRestrictions->eaFactions ); ++it ) {
				WorldUGCFactionRestrictionProperties* pFaction = StructCreate( parse_WorldUGCFactionRestrictionProperties );
				pFaction->pcFaction = allocAddString( projRestrictions->eaFactions[ it ]->pcFaction );
				eaPush( &effectiveRestrictions.eaFactions, pFaction );
			}
		}
		if (ugcIsFixedLevelEnabled())
		{
			effectiveRestrictions.iMinLevel = projRestrictions->iMinLevel;
			effectiveRestrictions.iMaxLevel = projRestrictions->iMaxLevel;
		}

		StructCopyAll( parse_WorldUGCRestrictionProperties, &effectiveRestrictions, ugcProj->project->pRestrictionProperties );
		StructReset( parse_WorldUGCRestrictionProperties, &effectiveRestrictions );
	}
}

void ugcEditorFixupProjectData(UGCProjectData* ugcProj, int* out_numDialogsDeleted, int* out_numCostumesReset, int* out_numObjectivesReset, int* out_fixupFlags)
{
	ugcEditorFixupMaps(ugcProj);
	ugcEditorFixupComponents( ugcProj, out_numDialogsDeleted, out_numCostumesReset, out_fixupFlags );
	ugcEditorFixupMission( ugcProj, out_numCostumesReset, out_numObjectivesReset );
	ugcEditorFixupProject(ugcProj);
	ugcEditorFixupCostumes(ugcProj);
}

//////////////////////////////////////////////
// Deprecated Object Fixup
//////////////////////////////////////////////

static int ugcProjectFixupDeprecatedMaps(UGCProjectData* ugcProj, bool fixupMap)
{
	int accum = 0;

	int it;
	for( it = 0; it != eaSize( &ugcProj->components->eaComponents ); ++it ) {
		UGCComponent* component = ugcProj->components->eaComponents[ it ];
		ZoneMapEncounterInfo* zeni;

		if( !component->sPlacement.bIsExternalPlacement || !component->sPlacement.pcExternalMapName ) {
			continue;
		}
		zeni = RefSystem_ReferentFromString( "ZoneMapEncounterInfo", component->sPlacement.pcExternalMapName );
		if( !zeni ) {
			continue;
		}

		if( zeni->deprecated_map_new_map_name ) {
			++accum;
			if( fixupMap ) {
				component->sPlacement.pcExternalMapName = StructAllocString( zeni->deprecated_map_new_map_name );
			}
		}
	}

	return accum;
}

static int ugcProjectFixupDeprecatedSounds(UGCProjectData* ugcProj, bool fixupSounds)
{
	static const char *sTwisted_Fane_Ambient = NULL;
	static const char *sChasm_Ext_Twisted_Fane_Amb = NULL;
	static const char *sChasm_Ambient = NULL;
	static const char *sChasm_Ext_Chasm_Amb = NULL;
	static const char *sChasm_Ambient_Neighborhood_2 = NULL;
	static const char *sChasm_Ext_Chasm_Amb_Neighborhood_2 = NULL;
	static const char *sCastle_Never_Ambience_3 = NULL;
	static const char *sCastle_CastleNever_Ambience_3 = NULL;
	static const char *sWooden_Room_Ambient = NULL;
	static const char *sCastle_CastleNever_Ambience_4_Creaky = NULL;
	static const char *sWooden_Room_Ambient_Creaky_1 = NULL;
	static const char *sMonastery_Wolf = NULL;
	static const char *sHelmsHold_Ext_HelmsHold_Forest_Amb_Evil = NULL;
	static const char *sCastle_Never_Ambience_1 = NULL;
	static const char *sCastle_Never_Ambience_2 = NULL;

	int accum = 0;

	int it;

	if(!sTwisted_Fane_Ambient) sTwisted_Fane_Ambient = allocAddString("Twisted_Fane_Ambient");
	if(!sChasm_Ext_Twisted_Fane_Amb) sChasm_Ext_Twisted_Fane_Amb = allocAddString("Chasm_Ext_Twisted_Fane_Amb");
	if(!sChasm_Ambient) sChasm_Ambient = allocAddString("Chasm_Ambient");
	if(!sChasm_Ext_Chasm_Amb) sChasm_Ext_Chasm_Amb = allocAddString("Chasm_Ext_Chasm_Amb");
	if(!sChasm_Ambient_Neighborhood_2) sChasm_Ambient_Neighborhood_2 = allocAddString("Chasm_Ambient_Neighborhood_2");
	if(!sChasm_Ext_Chasm_Amb_Neighborhood_2) sChasm_Ext_Chasm_Amb_Neighborhood_2 = allocAddString("Chasm_Ext_Chasm_Amb_Neighborhood_2");
	if(!sCastle_Never_Ambience_3) sCastle_Never_Ambience_3 = allocAddString("Castle_Never_Ambience_3");
	if(!sCastle_CastleNever_Ambience_3) sCastle_CastleNever_Ambience_3 = allocAddString("Castle_CastleNever_Ambience_3");
	if(!sWooden_Room_Ambient) sWooden_Room_Ambient = allocAddString("Wooden_Room_Ambient");
	if(!sCastle_CastleNever_Ambience_4_Creaky) sCastle_CastleNever_Ambience_4_Creaky = allocAddString("Castle_CastleNever_Ambience_4_Creaky");
	if(!sWooden_Room_Ambient_Creaky_1) sWooden_Room_Ambient_Creaky_1 = allocAddString("Wooden_Room_Ambient_Creaky_1");
	if(!sMonastery_Wolf) sMonastery_Wolf = allocAddString("Monastery_Wolf");
	if(!sHelmsHold_Ext_HelmsHold_Forest_Amb_Evil) sHelmsHold_Ext_HelmsHold_Forest_Amb_Evil = allocAddString("HelmsHold_Ext_HelmsHold_Forest_Amb_Evil");
	if(!sCastle_Never_Ambience_1) sCastle_Never_Ambience_1 = allocAddString("Castle_Never_Ambience_1");
	if(!sCastle_Never_Ambience_2) sCastle_Never_Ambience_2 = allocAddString("Castle_Never_Ambience_2");

	for( it = 0; it != eaSize( &ugcProj->components->eaComponents ); ++it ) {
		UGCComponent* component = ugcProj->components->eaComponents[ it ];

		if(nullStr(component->strSoundEvent))
			continue;

		if(component->strSoundEvent == sTwisted_Fane_Ambient)
			component->strSoundEvent = sChasm_Ext_Twisted_Fane_Amb;
		else if(component->strSoundEvent == sChasm_Ambient)
			component->strSoundEvent = sChasm_Ext_Chasm_Amb;
		else if(component->strSoundEvent == sChasm_Ambient_Neighborhood_2)
			component->strSoundEvent = sChasm_Ext_Chasm_Amb_Neighborhood_2;
		else if(component->strSoundEvent == sCastle_Never_Ambience_3)
			component->strSoundEvent = sCastle_CastleNever_Ambience_3;
		else if(component->strSoundEvent == sWooden_Room_Ambient)
			component->strSoundEvent = sCastle_CastleNever_Ambience_4_Creaky;
		else if(component->strSoundEvent == sWooden_Room_Ambient_Creaky_1)
			component->strSoundEvent = sCastle_CastleNever_Ambience_4_Creaky;
		else if(component->strSoundEvent == sMonastery_Wolf)
			component->strSoundEvent = sHelmsHold_Ext_HelmsHold_Forest_Amb_Evil;
		else if(component->strSoundEvent == sCastle_Never_Ambience_1)
			component->strSoundEvent = sCastle_CastleNever_Ambience_3;
		else if(component->strSoundEvent == sCastle_Never_Ambience_2)
			component->strSoundEvent = sCastle_CastleNever_Ambience_3;
	}

	return accum;
}

int ugcProjectFixupDeprecated(UGCProjectData* ugcProj, bool fixup)
{
	int accum = ugcProjectFixupDeprecatedMaps(ugcProj, fixup);
	accum += ugcProjectFixupDeprecatedSounds(ugcProj, fixup);
	return accum;
}
