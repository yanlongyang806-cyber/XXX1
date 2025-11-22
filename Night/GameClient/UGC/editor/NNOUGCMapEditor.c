#include "NNOUGCMapEditor.h"

#include "BlockEarray.h"
#include "GfxSprite.h"
#include "GfxSpriteText.h"
#include "GfxTexAtlas.h"
#include "MultiEditFieldContext.h"
#include "NNOUGCAssetLibrary.h"
#include "NNOUGCCommon.h"
#include "NNOUGCDialogPromptPicker.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCInteriorCommon.h"
#include "NNOUGCMapEditorProperties.h"
#include "NNOUGCMapEditorWidgets.h"
#include "NNOUGCMapSearch.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCModalDialog.h"
#include "NNOUGCResource.h"
#include "NNOUGCUnplacedList.h"
#include "NNOUGCZeniPicker.h"
#include "RegionRules.h"
#include "ResourceSearch.h"
#include "StringCache.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "UGCError.h"
#include "UGCInteriorCommon.h"
#include "UIMinimap.h"
#include "WorldGrid.h"
#include "inputLib.h"
#include "smf_render.h"
#include "wlExclusionGrid.h"
#include "wlUGC.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););
AUTO_RUN_ANON(memBudgetAddMapping("NNOUGCMapEditorPrivate.h", BUDGET_Editors););

static bool ugcMapEditorCheckPlaceOnMap(UGCMapType source_type, UGCMapType dest_type, bool *clear_properties);
static void ugcMapEditorClearAssetLibrarySelections( UITabGroup* ignored, UGCMapEditorDoc* doc );
static void ugcMapEditorPropertiesRefreshObjectiveModel( UGCMapEditorDoc* doc );
static void ugcMapEditorPropertiesRefreshObjectiveModelRecurse( UGCMapEditorDoc* doc, UGCMissionObjective** objectives );

/// Snapping logic

typedef struct UGCEditorParams
{
	bool bTranslateSnapEnabled;
	F32 fTranslateSnap;
	bool bRotateSnapEnabled;
	F32 fRotateSnap;

	bool bViewWaypointsEnabled;
	bool bPropertiesPaneIsDocked;
} UGCEditorParams;

static UGCEditorParams sMapEditorParams = { true, 2, true, 15, true, false };

bool ugcMapEditorGetTranslateSnap(UGCComponent* component, F32 *snap_xz, F32 *snap_y)
{
	F32 dummy;

	if( !snap_xz ) {
		snap_xz = &dummy;
	}
	if( !snap_y ) {
		snap_y = &dummy;
	}
	
	if (component->eType == UGC_COMPONENT_TYPE_ROOM)
	{
		const WorldUGCProperties* props = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", component->iObjectLibraryId );
		
		if( props && props->groupDefProps.bRoomDoorsEverywhere ) {
			*snap_xz = UGC_KIT_GRID;
		} else {
			*snap_xz = UGC_ROOM_GRID;
		}
		*snap_y = 10;
		return true;
	}
	if (sMapEditorParams.bTranslateSnapEnabled)
	{
		*snap_xz = sMapEditorParams.fTranslateSnap;
		*snap_y = sMapEditorParams.fTranslateSnap;
		return true;
	}
	return false;
}

bool ugcMapEditorGetTranslateSnapEnabled(void)
{
	return sMapEditorParams.bTranslateSnapEnabled;
}

F32 ugcMapEditorGetObjectTranslateSnap(void)
{
	if( sMapEditorParams.bTranslateSnapEnabled ) {
		return sMapEditorParams.fTranslateSnap;
	} else {
		return 0;
	}
}

void ugcMapEditorToggleTranslateSnap(void)
{
	sMapEditorParams.bTranslateSnapEnabled = !sMapEditorParams.bTranslateSnapEnabled;
}

bool ugcMapEditorGetRotateSnap(UGCComponentType type, F32 *snap)
{
	if (type == UGC_COMPONENT_TYPE_ROOM)
	{
		*snap = 90;
		return true;
	}
	if (type == UGC_COMPONENT_TYPE_ROOM_DOOR ||
		type == UGC_COMPONENT_TYPE_FAKE_DOOR)
	{
		*snap = 180;
		return true;
	}
	if (sMapEditorParams.bRotateSnapEnabled)
	{
		*snap = sMapEditorParams.fRotateSnap;
		return true;
	}
	return false;
}

void ugcMapEditorToggleRotateSnap(void)
{
	sMapEditorParams.bRotateSnapEnabled = !sMapEditorParams.bRotateSnapEnabled;
}

bool ugcMapEditorGetViewWaypoints( void )
{
	return sMapEditorParams.bViewWaypointsEnabled;
}

void ugcMapEditorToggleViewWaypoints( void )
{
	sMapEditorParams.bViewWaypointsEnabled = !sMapEditorParams.bViewWaypointsEnabled;
}

bool ugcMapEditorGetPropertiesPaneIsDocked( void )
{
	return sMapEditorParams.bPropertiesPaneIsDocked;
}

void ugcMapEditorTogglePropertiesPaneIsDocked( void )
{
	sMapEditorParams.bPropertiesPaneIsDocked = !sMapEditorParams.bPropertiesPaneIsDocked;
}

/// Editor callbacks

const char *ugcMapEditorGetName(UGCMapEditorDoc *doc)
{
	return doc->doc_name;
}

void ugcMapEditorSetVisible(UGCMapEditorDoc *doc)
{
	ugcEditorSetDocPane( doc->pRootPane );

	if (doc->backdrop_widget->minimap)
	{
		ui_MinimapSetMap( doc->backdrop_widget->minimap, doc->map_data->pPrefab->map_name );
	}
}

void ugcMapEditorSwitchToPlayMode(UGCMapEditorDoc* doc)
{
	// To address [NNO-4470], all the UIMinimaps need to be cleared.
	// This causes the hoggFile handle to get closed.
	if( doc->backdrop_widget->minimap ) {
		ui_MinimapSetMap( doc->backdrop_widget->minimap, NULL );
	}
}

void ugcMapEditorSwitchToEditMode(UGCMapEditorDoc* doc)
{
	// To address [NNO-4470], all the UIMinimaps need to be restored.
	// This will reopen the hoggFile handle.
	if( doc->backdrop_widget->minimap ) {
		ui_MinimapSetMap( doc->backdrop_widget->minimap, doc->map_data->pPrefab->map_name );
	}
}


static void ugcMapEditorUpdateUIForType( UGCMapEditorDoc* doc )
{
	UGCMapType type = ugcMapGetType(doc->map_data);
	RegionRules *rules = NULL;

	if (doc->map_data->pUnitializedMap)
	{
		// Nothing to do yet!
		rules = NULL;
	}
	else if (type == UGC_MAP_TYPE_INTERIOR)
	{
		doc->layout_grid_size = 10.f;
		doc->layout_kit_spacing = 10.f;

		rules = getRegionRulesFromRegionType(StaticDefineIntGetInt(WorldRegionTypeEnum, "Ground"));
	}
	else if (type == UGC_MAP_TYPE_PREFAB_GROUND || type == UGC_MAP_TYPE_PREFAB_INTERIOR)
	{
		doc->layout_grid_size = 32.f;
		doc->layout_kit_spacing = 40.f;
		rules = getRegionRulesFromRegionType(StaticDefineIntGetInt(WorldRegionTypeEnum, "Ground"));
	}
	else if (type == UGC_MAP_TYPE_SPACE || type == UGC_MAP_TYPE_PREFAB_SPACE)
	{
		doc->layout_grid_size = 32.f;
		doc->layout_kit_spacing = 100.f;
		rules = getRegionRulesFromRegionType(StaticDefineIntGetInt(WorldRegionTypeEnum, "Space"));
	}
	else
	{
		assert(0);
	}

	{
		int idx;
		F32 radius = 0;
		Vec3 spawn_pos;
		Quat qRot = {0,0,0,-1};
		for (idx = 0; idx < 10; idx++)
		{
			setVec3(spawn_pos, 0, 0, 0);
			Entity_GetPositionOffset(worldGetAnyCollPartitionIdx(), rules, qRot, idx, spawn_pos, NULL);
			if (lengthVec3(spawn_pos) > radius)
				radius = lengthVec3(spawn_pos);
		}
		doc->spawn_radius = radius;
	}

	{
		Vec3 min_pos, max_pos;
		ugcMapComponentValidBounds( min_pos, max_pos, ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(), doc->map_data, NULL );
		setVec2(doc->layout_min_pos, min_pos[0], min_pos[2]);
		setVec2(doc->layout_max_pos, max_pos[0], max_pos[2]);
	}
}

UGCMapEditorDoc *ugcMapEditorLoadDoc(const char* map_name)
{
	UGCMap* map = ugcEditorGetMapByName( map_name );
	UGCMapEditorDoc *new_doc = calloc(1, sizeof(UGCMapEditorDoc));

	assert( map );
	new_doc->doc_name = map->pcName;

	new_doc->objects_fade = 0.f;
	new_doc->rooms_fade = 0.f;
	new_doc->pRootPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );

	return new_doc;
}

void ugcMapEditorFreeDoc(UGCMapEditorDoc *doc)
{
	char buffer[ 256 ];
	
	eaDestroyEx( &doc->component_widgets, ugcMapEditorWidgetQueueFree );
	eaDestroyEx( &doc->objectiveWaypointWidgets, ugcMapEditorWidgetQueueFree );
	SAFE_FREE(doc->drag_state);
	if( doc->backdrop_widget ) {
		doc->backdrop_widget->doc = NULL;
	}
	if( doc->map_widget ) {
		UI_WIDGET( doc->map_widget )->u64 = 0;
	}

	sprintf( buffer, "UGCMapEditor_%s_Properties", doc->doc_name );
	MEContextDestroyByName( buffer );
	ui_WidgetQueueFreeAndNull( &doc->properties_pane );
	ui_WidgetQueueFreeAndNull( &doc->properties_sprite );

	// Free the library pane
	if( doc->libraryEmbeddedPicker ) {
		ugcAssetLibraryPaneDestroy( doc->libraryEmbeddedPicker );
		doc->libraryEmbeddedPicker = NULL;
	}
	sprintf( buffer, "UGCMapEditor_%s_AssetLibrary", doc->doc_name );
	MEContextDestroyByName( buffer );
	sprintf( buffer, "UGCMapEditor_%s_GlobalProperties", doc->doc_name );
	MEContextDestroyByName( buffer );
	sprintf( buffer, "UGCMapEditor_%s_BackdropProperties", doc->doc_name );
	MEContextDestroyByName( buffer );
	sprintf( buffer, "UGCMapEditor_%s_InBackdrop", doc->doc_name );
	MEContextDestroyByName( buffer );

	ui_WidgetQueueFreeAndNull( &doc->trash_button );
	ui_WidgetQueueFreeAndNull( &doc->search_window );
	ui_WidgetQueueFreeAndNull( &doc->global_properties_window );
	ui_WidgetQueueFreeAndNull( &doc->backdrop_properties_window );
	
	ui_WidgetQueueFreeAndNull( &doc->pRootPane );

	{
		int it;
		for( it = 0; it != beaSize( &doc->beaObjectivesModel ); ++it ) {
			if(   doc->beaObjectivesModel[ it ].key != U32_TO_PTR( DM_INT )
				  && doc->beaObjectivesModel[ it ].key != U32_TO_PTR( DM_END )) {
				free( (char*)doc->beaObjectivesModel[ it ].key );
			}
		}
	}
	beaDestroy( &doc->beaObjectivesModel );

	SAFE_FREE(doc);
}

void ugcMapEditorOncePerFrame(UGCMapEditorDoc *gen_doc, bool isActive)
{
	if (!gen_doc)
		return;
	
	if( isActive && gen_doc->map_data ) {
		ugcLayoutOncePerFrame(gen_doc);
	} else {
		ui_WidgetQueueFreeAndNull( &gen_doc->search_window );
		if( gen_doc->global_properties_window ) {
			ui_WindowHide( gen_doc->global_properties_window );
		}
		if( gen_doc->backdrop_properties_window ) {
			ui_WindowHide( gen_doc->backdrop_properties_window );
		}
		ugcMapEditorClearAssetLibrarySelections( NULL, gen_doc );
	}

	ugcMapEditorPropertiesOncePerFrame( gen_doc );
	
	// Focus and selection should be in sync.  If this is not the
	// case, then the user has specifically focused on a widget.
	// Therefore, clear the selection.
	if( eaiSize( &gen_doc->selected_components ) == 1 ) {
		UGCUIMapEditorComponent* component_ui = ugcLayoutUIGetComponentByID( gen_doc, gen_doc->selected_components[ 0 ]);
		if( !isActive ) {
			ugcMapEditorClearSelection( gen_doc );
			ugcEditorQueueUIUpdate();
		} else if(   component_ui && gen_doc->properties_pane && g_ui_State.focused
			  && !ugcAssetLibraryPickerWindowOpen() && !ugcZeniPickerWindowOpen()
			  && !ugcDialogPromptPickerWindowOpen()
			  && !ui_IsFocused( &g_UGCMapEditorComponentWidgetPlaceholder ) && !ui_IsFocusedOrChildren( gen_doc->properties_pane )) {
			ugcMapEditorClearSelection( gen_doc );
			ugcEditorQueueUIUpdate();
		}
	}
}

/// Operations

static void ugcMapEditorAddComponentPlace(UIButton *button, void *component_type)
{
	UGCMapEditorDoc *doc = ugcEditorGetActiveMapDoc();
	UGCComponentType type = (UGCComponentType)(intptr_t)component_type;
	UGCComponent *component;
	U32 room_id = 0;
	Vec3 pos;

	if (!doc)
		return;

	ugcLayoutGetDefaultPlacement(doc, &room_id, pos);

	ugcMapUICancelAction(doc);
	component = ugcComponentOpCreate(ugcEditorGetProjectData(), type, 0);
	if (component)
	{
		ugcComponentOpReset(ugcEditorGetProjectData(), component, ugcMapGetType(doc->map_data), false);
		ugcComponentOpSetPlacement(ugcEditorGetProjectData(), component, doc->map_data, room_id);
		copyVec3(pos, component->sPlacement.vPos);
		ugcEditorStartObjectFlashing(component);
		ugcMapEditorSetSelectedComponent(doc, component->uID, 0, false, true);
		ugcEditorApplyUpdate();
	}
}

void ugcMapEditorComponentPlace(UGCComponent *component, UGCMap *map, bool keep_object)
{
	UGCMapType map_type = ugcMapGetType(map);
	bool reset_component = false;
	if (!ugcMapEditorCheckPlaceOnMap(component->eMapType, map_type, &reset_component))
	{
		return;
	}
	if (reset_component)
		ugcComponentOpReset(ugcEditorGetProjectData(), component, map_type, keep_object);
	ugcComponentOpSetPlacement(ugcEditorGetProjectData(), component, map, UGC_TOPLEVEL_ROOM_ID);
}

void ugcMapEditorRoomCreateDoors( UGCComponent* room, UGCPotentialRoomDoor*** inout_eaDoors )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCRoomInfo* roomInfo = ugcRoomGetRoomInfo( room->iObjectLibraryId );
	CBox roomBox;
	if( !roomInfo ) {
		return;
	}

	ugcRoomGetWorldBoundingBox( room, &roomBox );

	FOR_EACH_IN_EARRAY( roomInfo->doors, UGCRoomDoorInfo, door ) {
		UGCComponent* connectedRoom = NULL;
		UGCDoorSlotState state = ugcRoomGetDoorSlotState( ugcProj->components, room, FOR_EACH_IDX( roomInfo->doors, door ), NULL, NULL, &connectedRoom, NULL );

		if( state == UGC_DOOR_SLOT_EMPTY && connectedRoom ) {

			UGCPotentialRoomDoor* newDoor = calloc( 1, sizeof( *newDoor ));
			newDoor->roomID = room->uID;
			newDoor->connectedRoomID = connectedRoom->uID;
			newDoor->doorIdx = FOR_EACH_IDX( roomInfo->doors, door );

			// Calculate the ideal position for a door
			{
				CBox connectedRoomBox;
				CBox intersectionBox = { 0 };
				Vec3 idealPos = { 0 };
				Vec3 localPos;
				Vec3 worldPos;
				
				ugcRoomGetDoorLocalPos( roomInfo, FOR_EACH_IDX( roomInfo->doors, door ), localPos );
				ugcRoomConvertLocalToWorld( room, localPos, worldPos );
				ugcRoomGetWorldBoundingBox( room, &connectedRoomBox );
				CBoxIntersectClip( &roomBox, &connectedRoomBox, &intersectionBox );
				idealPos[ 0 ] = (intersectionBox.lx + intersectionBox.hx) / 2;
				idealPos[ 2 ] = (intersectionBox.ly + intersectionBox.hy) / 2;
				newDoor->distanceFromIdeal = distance3SquaredXZ( idealPos, worldPos );
			}
				
			eaPush( inout_eaDoors, newDoor );
		} else if( state == UGC_DOOR_SLOT_OCCUPIED && connectedRoom ) {
			UGCPotentialRoomDoor* newDoor = calloc( 1, sizeof( *newDoor ));
			newDoor->roomID = room->uID;
			newDoor->connectedRoomID = connectedRoom->uID;
			newDoor->doorIdx = -1;
			newDoor->distanceFromIdeal = 0;
			eaPush( inout_eaDoors, newDoor );
		}
	} FOR_EACH_END;
}

/// Actions

void ugcMapEditorDeleteSelected(UGCMapEditorDoc *doc)
{
	int idx;
	bool anyIsActor = false;

	if (eaiSize( &doc->selected_components) == 0)
		return;

	for (idx = 0; idx < eaiSize(&doc->selected_components); idx++)
	{
		UGCComponent *component = ugcEditorFindComponentByID(doc->selected_components[idx]);
		if (!component || !ugcLayoutCanDeleteComponent(component))
			return;

		if( component->eType == UGC_COMPONENT_TYPE_ACTOR ) {
			anyIsActor = true;
		}
	}

	if( anyIsActor ) {
		if( ugcModalDialogMsg( "UGC_MapEditor.DeleteActorEncounter_Title", "UGC_MapEditor.DeleteActorEncounter_Body", UIYes | UINo ) != UIYes ) {
			return;
		}
	}

	for (idx = 0; idx < eaiSize(&doc->selected_components); idx++)
	{
		UGCComponent *component = ugcEditorFindComponentByID(doc->selected_components[idx]);
		if( component ) {
			ugcLayoutDeleteComponent( component );
		}
	}
	ugcEditorApplyUpdate();
}

void ugcMapEditorCutSelected(UGCMapEditorDoc *doc, UGCEditorCopyBuffer *buffer)
{
	int idx, idx2;

	if (eaiSize( &doc->selected_components) == 0)
		return;

	buffer->eSourceMapType = ugcMapGetType(doc->map_data);
	for (idx = 0; idx < eaiSize(&doc->selected_components); idx++)
	{
		UGCComponent *component = ugcEditorFindComponentByID(doc->selected_components[idx]);
		bool found = false;

		if( component->eType == UGC_COMPONENT_TYPE_PATROL_POINT ) {
			continue;
		}

		// Skip any components whose parents are already in the tree
		for (idx2 = 0; idx2 < eaiSize(&doc->selected_components); idx2++)
		{
			if (idx2 != idx && ugcComponentHasParent(ugcEditorGetComponentList(), component, doc->selected_components[idx2]))
			{
				found = true;
				break;
			}
		}
		if (found)
			continue;
		
		eaiPush(&buffer->eauComponentIDs, doc->selected_components[idx]);
	}
}

void ugcMapEditorAbortCutForComponent(U32 component_id)
{
	UGCEditorCopyBuffer *buffer = ugcEditorCurrentCopy();
	if (buffer && buffer->eType == UGC_CUT_COMPONENT)
	{
		if (eaiFind(&buffer->eauComponentIDs, component_id) >= 0)
		{
			ugcEditorAbortCopy();
		}
	}
}

void ugcMapEditorCopyChildrenRecurse(UGCEditorCopyBuffer *buffer, UGCComponent *component)
{
	int i;
	for (i = 0; i < eaiSize(&component->uChildIDs); i++)
	{
		UGCComponent *child_component = ugcEditorFindComponentByID(component->uChildIDs[i]);
		if (child_component)
		{
			eaPush(&buffer->eaChildComponents, StructClone(parse_UGCComponent, child_component));
			ugcMapEditorCopyChildrenRecurse(buffer, child_component);
		}
	}

	FOR_EACH_IN_EARRAY(ugcEditorGetComponentList()->eaComponents, UGCComponent, other_component)
	{
		if (other_component->uActorID == component->uID)
		{
			if( other_component->eType != UGC_COMPONENT_TYPE_DIALOG_TREE || ea32Size( &other_component->eaObjectiveIDs ) == 0 ) {
				eaPush(&buffer->eaTimelineComponents, StructClone(parse_UGCComponent, other_component));
			}
		}
	}
	FOR_EACH_END;
}

void ugcMapEditorCopySelected(UGCMapEditorDoc *doc, UGCEditorCopyBuffer *buffer)
{
	int idx, idx2;

	if (eaiSize( &doc->selected_components) == 0)
		return;

	for (idx = 0; idx < eaiSize(&doc->selected_components); idx++)
	{
		UGCComponent *component = ugcEditorFindComponentByID(doc->selected_components[idx]);
		if(   !component || !ugcLayoutCanDeleteComponent(component)
			  || component->eType == UGC_COMPONENT_TYPE_PATROL_POINT
			  || component->eType == UGC_COMPONENT_TYPE_CLUSTER_PART ) {
			return;
		}
	}

	buffer->eSourceMapType = ugcMapGetType(doc->map_data);

	for (idx = 0; idx < eaiSize(&doc->selected_components); idx++)
	{
		UGCComponent *component = ugcEditorFindComponentByID(doc->selected_components[idx]);
		bool found = false;

		// Skip any components whose parents are already in the tree
		for (idx2 = 0; idx2 < eaiSize(&doc->selected_components); idx2++)
		{
			if (idx2 != idx && ugcComponentHasParent(ugcEditorGetComponentList(), component, doc->selected_components[idx2]))
			{
				found = true;
				break;
			}
		}
		if (found)
			continue;

		eaPush(&buffer->eaComponents, StructClone(parse_UGCComponent, component));
		ugcMapEditorCopyChildrenRecurse(buffer, component);
	}
}

static bool ugcMapEditorCheckPlaceOnMap(UGCMapType source_type, UGCMapType dest_type, bool *clear_properties)
{
	bool region_mismatch = false;

	if (source_type == UGC_MAP_TYPE_ANY)
	{
		*clear_properties = true;
		return true;
	}

	if ((source_type == UGC_MAP_TYPE_INTERIOR || source_type == UGC_MAP_TYPE_GROUND ||
		source_type == UGC_MAP_TYPE_PREFAB_INTERIOR || source_type == UGC_MAP_TYPE_PREFAB_GROUND)
		&& (dest_type == UGC_MAP_TYPE_SPACE || dest_type == UGC_MAP_TYPE_PREFAB_SPACE))
	{
		region_mismatch = true;
	}
	else if ((dest_type == UGC_MAP_TYPE_INTERIOR || dest_type == UGC_MAP_TYPE_GROUND ||
		dest_type == UGC_MAP_TYPE_PREFAB_INTERIOR || dest_type == UGC_MAP_TYPE_PREFAB_GROUND)
		&& (source_type == UGC_MAP_TYPE_SPACE || source_type == UGC_MAP_TYPE_PREFAB_SPACE))
	{
		region_mismatch = true;
	}

	if (region_mismatch)
	{
		UIDialogButtons result;
		result = ugcModalDialogMsg( "UGC_MapEditor.ResetProperties", "UGC_MapEditor.ResetPropertiesDetails", UIYes | UINo );
		if (result == UIYes)
		{
			*clear_properties = true;
			return true;
		}
		else
		{
			return false;
		}
	}

	return true;

}

static void ugcMapEditorFixupPastedWhen(const char *map_name, U32 *id_list, UGCComponent **component_list, UGCWhen *when, bool default_always)
{
	if (!when)
		return;

	if (when->eType == UGCWHEN_COMPONENT_COMPLETE ||
		when->eType == UGCWHEN_COMPONENT_REACHED)
	{
		int i;
		for (i = eaiSize(&when->eauComponentIDs)-1; i >= 0; --i)
		{
			UGCComponent *existing_component;
			int idx = eaiFind(&id_list, when->eauComponentIDs[i]);
			if (idx > -1)
			{
				UGCComponent *new_component = component_list[idx];
				when->eauComponentIDs[i] = new_component->uID;
				continue;
			}
			// Component was not found in paste list. Are we still on the same map?
			existing_component = ugcEditorFindComponentByID(when->eauComponentIDs[i]);
			if (existing_component && ugcComponentIsOnMap(existing_component, map_name, false))
			{
				continue;
			}

			eaiRemove(&when->eauComponentIDs, i);
		}

		if (eaiSize(&when->eauComponentIDs) == 0)
		{
			when->eType = default_always ? UGCWHEN_MAP_START : UGCWHEN_MANUAL;
		}
	}
}

static void ugcMapEditorFixupPastedWhens(const char *map_name, U32 *id_list, UGCComponent **component_list)
{
	FOR_EACH_IN_EARRAY(component_list, UGCComponent, component)
	{
		ugcMapEditorFixupPastedWhen(map_name, id_list, component_list, component->pStartWhen, true);
		ugcMapEditorFixupPastedWhen(map_name, id_list, component_list, component->pHideWhen, false);
	}
	FOR_EACH_END;
}

static void ugcMapEditorPasteDuplicateChildrenRecursive(UGCMapEditorDoc *doc, UGCEditorCopyBuffer *buffer, UGCComponent *root_component, UGCComponent *component, UGCComponent *new_component, bool reset_component, U32 room_id, Vec3 pos, U32 **new_component_ids, UGCComponent ***new_component_list)
{
	if (reset_component && component->eType == UGC_COMPONENT_TYPE_KILL)
		return;

	FOR_EACH_IN_EARRAY(buffer->eaChildComponents, UGCComponent, child_component)
	{
		if (child_component->uParentID == component->uID)
		{
			UGCComponent *new_child_component = ugcComponentOpDuplicate(ugcEditorGetProjectData(), child_component, new_component->uID);
			if (new_child_component)
			{
				eaiPush(new_component_ids, child_component->uID);
				eaPush(new_component_list, new_child_component);
				ugcComponentOpSetPlacement(ugcEditorGetProjectData(), new_child_component, doc->map_data, room_id);
				new_child_component->sPlacement.vPos[0] = pos[0] + (child_component->sPlacement.vPos[0] - root_component->sPlacement.vPos[0]);
				new_child_component->sPlacement.vPos[2] = pos[2] + (child_component->sPlacement.vPos[2] - root_component->sPlacement.vPos[2]);

				ugcMapEditorPasteDuplicateChildrenRecursive(doc, buffer, root_component, child_component, new_child_component, reset_component, room_id, pos, new_component_ids, new_component_list);
			}
		}
	}
	FOR_EACH_END;
}

static void ugcMapEditorPasteMoveChildrenRecursive(UGCMapEditorDoc *doc, UGCEditorCopyBuffer *buffer, UGCComponent *new_component, U32 room_id, Vec3 pos, Vec3 old_position, U32 **new_component_ids, UGCComponent ***new_component_list)
{
	int j;
	for (j = 0; j < eaiSize(&new_component->uChildIDs); j++)
	{
		UGCComponent *child_component = ugcEditorFindComponentByID(new_component->uChildIDs[j]);
		if (child_component)
		{
			eaiPush(new_component_ids, child_component->uID);
			eaPush(new_component_list, child_component);
			ugcComponentOpSetPlacement(ugcEditorGetProjectData(), child_component, doc->map_data, room_id);
			child_component->sPlacement.vPos[0] = pos[0] + (child_component->sPlacement.vPos[0] - old_position[0]);
			child_component->sPlacement.vPos[2] = pos[2] + (child_component->sPlacement.vPos[2] - old_position[2]);

			// Add child to new copy operation
			eaPush(&buffer->eaChildComponents, StructClone(parse_UGCComponent, child_component));

			ugcMapEditorPasteMoveChildrenRecursive(doc, buffer, child_component, room_id, pos, old_position, new_component_ids, new_component_list);
		}
	}
}

void ugcMapEditorPaste(UGCMapEditorDoc *doc, UGCEditorCopyBuffer *buffer, bool is_duplicate, bool offset)
{
	UGCComponent *new_component = NULL;
	UGCMap* map = ugcEditorGetMapByName( doc->doc_name );
	UGCMapType map_type = ugcMapGetType(map);
	F32 offset_dist = offset ? (20.f * doc->layout_kit_spacing / (doc->layout_scale * doc->layout_grid_size)) : 0;
	bool reset_component = false;
	UGCComponent **new_component_list = NULL;
	U32 *new_component_ids = NULL;
	bool snapping;
	F32 snap, snap_y;

	ugcMapUICancelAction(doc);

	if (!buffer)
		return;

	// Check for buildings in interiors, etc.
	FOR_EACH_IN_EARRAY(buffer->eaComponents, UGCComponent, component)
	{
		if (!ugcComponentLayoutCompatible(component->eType, map_type))
		{
			ugcModalDialogMsg( "UGC_MapEditor.Error_WrongMapTypeForComponent", "UGC_MapEditor.Error_WrongMapTypeForComponentDetails", UIOk );
			return;
		}
	}
	FOR_EACH_END;

	if (!ugcMapEditorCheckPlaceOnMap(buffer->eSourceMapType, map_type, &reset_component))
	{
		return;
	}

	if (buffer->eType == UGC_COPY_COMPONENT)
	{
		U32 room_id = 0;
		Vec3 default_pos, old_center_pos, new_pos;

		if (!is_duplicate)
		{
			ugcLayoutGetDefaultPlacement(doc, &room_id, default_pos);

			setVec3(old_center_pos, 0, 0, 0);
			FOR_EACH_IN_EARRAY(buffer->eaComponents, UGCComponent, component)
			{
				addVec3(old_center_pos, component->sPlacement.vPos, old_center_pos);
			}
			FOR_EACH_END;
			scaleVec3(old_center_pos, 1.f/eaSize(&buffer->eaComponents), old_center_pos);
		}
		else
			room_id = UGC_TOPLEVEL_ROOM_ID;

		FOR_EACH_IN_EARRAY(buffer->eaComponents, UGCComponent, component)
		{
			if (is_duplicate)
			{
				UGCRoomInfo *room_buffer = ugcRoomGetRoomInfo(component->iObjectLibraryId);

				new_pos[0] = component->sPlacement.vPos[0] + offset_dist;
				new_pos[1] = component->sPlacement.vPos[1];
				new_pos[2] = component->sPlacement.vPos[2];

				if (room_buffer && offset)
					new_pos[0] += (room_buffer->footprint_max[0]-room_buffer->footprint_min[0]) * UGC_ROOM_GRID;
			}
			else
			{
				new_pos[0] = component->sPlacement.vPos[0] - old_center_pos[0] + default_pos[0];
				new_pos[1] = component->sPlacement.vPos[1] - old_center_pos[1] + default_pos[1];
				new_pos[2] = component->sPlacement.vPos[2] - old_center_pos[2] + default_pos[2];
			}

			new_component = ugcComponentOpDuplicate(ugcEditorGetProjectData(), component, 0);
			if (new_component)
			{
				eaiPush(&new_component_ids, component->uID);
				eaPush(&new_component_list, new_component);

				if (reset_component)
					ugcComponentOpReset(ugcEditorGetProjectData(), new_component, map_type, false);

				{
					// Get valid position if possible, and detect correct parenting
					UGCComponentValidPosition valid_pos;

					snapping = ugcMapEditorGetTranslateSnap(new_component, &snap, &snap_y);

					// This is guaranteed to return a position whether it's valid or not
					ugcComponentIsValidPosition(ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(), new_component, new_pos, NULL, snapping, snap, snap_y, &valid_pos);

					// Move component to correct map, room, and position
					ugcComponentOpSetPlacement(ugcEditorGetProjectData(), new_component, map, room_id);
					ugcComponentSetValidPosition(ugcEditorGetProjectData(), new_component, &valid_pos);
					ugcMapEditorPasteDuplicateChildrenRecursive(doc, buffer, component, component, new_component, reset_component, room_id, valid_pos.position, &new_component_ids, &new_component_list);

					ugcEditorStartObjectFlashing(new_component);
				}

				// Copy over FSMs, dialog trees, etc.
				FOR_EACH_IN_EARRAY(buffer->eaTimelineComponents, UGCComponent, timeline_component)
				{
					UGCComponent *new_child_component = ugcComponentOpDuplicate(ugcEditorGetProjectData(), timeline_component, 0);
					if (new_child_component)
					{
						eaiPush(&new_component_ids, timeline_component->uID);
						eaPush(&new_component_list, new_child_component);
						new_child_component->uActorID = new_component->uID;
					}
				}
				FOR_EACH_END;
			}
		}
		FOR_EACH_END;

		// Fixup patrol points
		FOR_EACH_IN_EARRAY(new_component_list, UGCComponent, new_enc_component)
		{
			int point_idx;
			for (point_idx = 0; point_idx < eaiSize(&new_enc_component->eaPatrolPoints); point_idx++)
			{
				int component_idx = eaiFind(&new_component_ids, new_enc_component->eaPatrolPoints[point_idx]);
				if (component_idx != -1)
				{
					new_enc_component->eaPatrolPoints[point_idx] = new_component_list[component_idx]->uID;
				}
			}
		}
		FOR_EACH_END;
	}
	else if (buffer->eType == UGC_CUT_COMPONENT)
	{
		int i;
		U32 room_id = 0;
		Vec3 default_pos, old_center_pos, new_pos;

		ugcLayoutGetDefaultPlacement(doc, &room_id, default_pos);

		setVec3(old_center_pos, 0, 0, 0);
		for (i = 0; i < eaiSize(&buffer->eauComponentIDs); i++)
		{
			new_component = ugcEditorFindComponentByID(buffer->eauComponentIDs[i]);
			addVec3(old_center_pos, new_component->sPlacement.vPos, old_center_pos);
		}
		scaleVec3(old_center_pos, 1.f/eaiSize(&buffer->eauComponentIDs), old_center_pos);

		for (i = 0; i < eaiSize(&buffer->eauComponentIDs); i++)
		{
			new_component = ugcEditorFindComponentByID(buffer->eauComponentIDs[i]);
			if (new_component)
			{
				Vec3 old_position;
				copyVec3(new_component->sPlacement.vPos, old_position);

				eaiPush(&new_component_ids, new_component->uID);
				eaPush(&new_component_list, new_component);

				if (reset_component)
					ugcComponentOpReset(ugcEditorGetProjectData(), new_component, map_type, false);

				{
					// Get valid position if possible, and detect correct parenting
					UGCComponentValidPosition valid_pos;

					snapping = ugcMapEditorGetTranslateSnap(new_component, &snap, &snap_y);

					new_pos[0] = new_component->sPlacement.vPos[0] - old_center_pos[0] + default_pos[0];
					new_pos[1] = new_component->sPlacement.vPos[1] - old_center_pos[1] + default_pos[1];
					new_pos[2] = new_component->sPlacement.vPos[2] - old_center_pos[2] + default_pos[2];

					// This is guaranteed to return a position whether it's valid or not
					ugcComponentIsValidPosition(ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(), new_component, new_pos, NULL, snapping, snap, snap_y, &valid_pos);

					// Move component to correct map, room, and position
					ugcComponentOpSetPlacement(ugcEditorGetProjectData(), new_component, map, room_id);
					ugcComponentSetValidPosition(ugcEditorGetProjectData(), new_component, &valid_pos);
					ugcMapEditorPasteMoveChildrenRecursive(doc, buffer, new_component, room_id, valid_pos.position, old_position, &new_component_ids, &new_component_list);

					ugcEditorStartObjectFlashing(new_component);
				}

				FOR_EACH_IN_EARRAY(ugcEditorGetComponentList()->eaComponents, UGCComponent, other_component)
				{
					if (other_component->uActorID == new_component->uID)
					{
						eaPush(&buffer->eaTimelineComponents, StructClone(parse_UGCComponent, other_component));
					}
				}
				FOR_EACH_END;

				// Add component to new copy operation
				eaPush(&buffer->eaComponents, StructClone(parse_UGCComponent, new_component));
			}
		}

		// Turn the "cut" buffer into a "copy" buffer for future paste operations
		eaiDestroy(&buffer->eauComponentIDs);
		buffer->eType = UGC_COPY_COMPONENT;
	}
	else if (buffer->eType == UGC_COPY_NONE)
	{
		assertmsg(0, "Copy buffer sent to ugcMapEditorPaste command with no type!");
	}

	ugcMapEditorFixupPastedWhens(doc->doc_name, new_component_ids, new_component_list);

	ugcMapEditorClearSelection(doc);
	if (eaSize(&new_component_list) > 0)
	{
		FOR_EACH_IN_EARRAY(new_component_list, UGCComponent, component)
		{
			// Some subtlety here.  The component may be an implicitly
			// linked component as opposed to one that is draggable on
			// the map (example: default prompts).  These components
			// can not get selected.
			//
			// These components all have no map name.
			if( component->sPlacement.pcMapName == NULL ) {
				continue;
			}
			
			// MJF (7/20/2012): This can NOT be called for DuplicateMap.  Because
			// DuplicateMap does not yet have a fully formed doc, it
			// is not safe to call random funcs on that doc.
			//
			// Currently, disable it for all duplicates.
			ugcMapEditorAddSelectedComponent(doc, component->uID, false, !is_duplicate);
		}
		FOR_EACH_END;
	}

	ugcEditorApplyUpdate();

	eaiDestroy(&new_component_ids);
	eaDestroy(&new_component_list);
}

void ugcMapEditorDuplicateSelected(UGCMapEditorDoc *doc)
{
	UGCEditorCopyBuffer *buffer = StructCreate(parse_UGCEditorCopyBuffer);
	buffer->eType = UGC_COPY_COMPONENT;
	ugcMapEditorCopySelected(doc, buffer);
	ugcMapEditorPaste(doc, buffer, true, true);
	StructDestroy(parse_UGCEditorCopyBuffer, buffer);
}

void ugcMapEditorUseRoomSoundForAllRooms(UGCMapEditorDoc *doc, U32 *component_ids)
{
	int idx;

	ugcMapUICancelAction(doc);

	for (idx = 0; idx < eaiSize(&component_ids); idx++)
	{
		UGCComponent *component = ugcEditorFindComponentByID(component_ids[idx]);

		if(component->eType == UGC_COMPONENT_TYPE_ROOM)
		{
			UGC_FOR_EACH_COMPONENT_OF_TYPE_ON_MAP(ugcEditorGetComponentList(), UGC_COMPONENT_TYPE_ROOM, doc->map_data->pcName, room_component)
			{
				room_component->strSoundEvent = component->strSoundEvent;
			}
			UGC_FOR_EACH_COMPONENT_END;
		}
	}

	ugcEditorQueueApplyUpdate();
}

void ugcMapEditorClearRoom(UGCMapEditorDoc *doc, U32 *component_ids)
{
	UGCComponent* room_component;
	UGCComponent **found_details = NULL;
	if (eaiSize(&component_ids) != 1)
		return;
	room_component = ugcEditorFindComponentByID(component_ids[0]);
	ugcMapUICancelAction(doc);

	UGC_FOR_EACH_COMPONENT_OF_TYPE_ON_MAP(ugcEditorGetComponentList(), UGC_COMPONENT_TYPE_OBJECT, doc->map_data->pcName, detail_component)
	{
		if (detail_component->uParentID == room_component->uID)
		{
			if (!ugcLayoutCanDeleteComponent(detail_component))
				continue;
			eaPush(&found_details, detail_component);
		}
	}
	UGC_FOR_EACH_COMPONENT_END;

	FOR_EACH_IN_EARRAY(found_details, UGCComponent, existing_component)
	{
		ugcComponentOpDelete( ugcEditorGetProjectData(), existing_component, false );
	}
	FOR_EACH_END;
	eaDestroy(&found_details);

	ugcEditorQueueApplyUpdate();
}

typedef struct UGCMapEditorPopulateInfo
{
	UGCMapEditorDoc *doc;
	UGCComponent *room_component;
	UGCRoomInfo* room_info;

	UIWindow *window;
	UIList *list;
} UGCMapEditorPopulateInfo;

void ugcMapEditorPopulateRoomCancel(UIButton *button, UGCMapEditorPopulateInfo *info)
{
	info->list->peaModel = NULL;
	ui_WidgetQueueFree(UI_WIDGET(info->window));
	SAFE_FREE(info);
}

static void ugcMapEditorDoPopulateRoom( UGCMapEditorDoc *doc, UGCComponent* room_component, UGCRoomPopulateDef* populate_def )
{
	{
		U32* eaIDs = NULL;
		eaiPush( &eaIDs, room_component->uID );
		ugcMapEditorClearRoom( doc, eaIDs );
		eaiDestroy( &eaIDs );
	}
	
	if (populate_def)
	{
		FOR_EACH_IN_EARRAY(populate_def->eaObjects, UGCRoomPopulateObject, populate_object)
		{
			if (ugcLayoutCanCreateComponent(doc->map_data->pcName, UGC_COMPONENT_TYPE_OBJECT))
			{
				UGCComponent *new_component = ugcComponentOpCreate(ugcEditorGetProjectData(), UGC_COMPONENT_TYPE_OBJECT, 0);
				Mat3 rotate_mat;
				Vec3 rotated_pos;
				Vec3 vRotPYRRadians;

				ugcComponentOpReset(ugcEditorGetProjectData(), new_component, ugcMapGetType(doc->map_data), false);

				vRotPYRRadians[ 0 ] = RAD( room_component->sPlacement.vRotPYR[ 0 ]);
				vRotPYRRadians[ 1 ] = RAD( room_component->sPlacement.vRotPYR[ 1 ]);
				vRotPYRRadians[ 2 ] = RAD( room_component->sPlacement.vRotPYR[ 2 ]);
				createMat3YPR(rotate_mat, vRotPYRRadians);
				mulVecMat3(populate_object->vPos, rotate_mat, rotated_pos);

				ugcComponentOpSetPlacement(ugcEditorGetProjectData(), new_component, doc->map_data, UGC_TOPLEVEL_ROOM_ID);
				ugcComponentOpSetParent(ugcEditorGetProjectData(), new_component, room_component->uID);
				addVec3(room_component->sPlacement.vPos, rotated_pos, new_component->sPlacement.vPos);
				copyVec3(room_component->sPlacement.vRotPYR, new_component->sPlacement.vRotPYR);
				new_component->sPlacement.vRotPYR[1] += populate_object->fRot;
				new_component->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE;
				new_component->iObjectLibraryId = populate_object->iGroupUID;
				ugcEditorStartObjectFlashing(new_component);
			}
			else
			{
				ugcModalDialogMsg( "UGC_MapEditor.Error_TooManyComponents", "UGC_MapEditor.Error_TooManyComponentsDetails", UIOk );
				break;
			}
		}
		FOR_EACH_END;

		ugcMapEditorZoomToDetailMode(doc);
		ugcEditorQueueApplyUpdate();
	}
}

void ugcMapEditorPopulateRoomOK(UIButton *button, UGCMapEditorPopulateInfo *info)
{
	UGCRoomPopulateDef *populate_def = NULL;
	int selected = ui_ListGetSelectedRow(info->list);

	if (selected >= 0 && selected < eaSize(info->list->peaModel))
	{
		ugcMapEditorDoPopulateRoom( info->doc, info->room_component, (*info->list->peaModel)[selected] );
	}
	ugcMapEditorPopulateRoomCancel(button, info);
}

void ugcMapEditorPopulateRoom(UGCMapEditorDoc *doc, U32 *component_ids)
{
	bool foundAnyDetails = false;
	UGCComponent *room_component;
	UGCRoomInfo *room_info;

	if (eaiSize(&component_ids) != 1)
		return;
	room_component = ugcEditorFindComponentByID(component_ids[0]);

	ugcMapUICancelAction(doc);


	room_info = ugcRoomGetRoomInfo(room_component->iObjectLibraryId);

	if (room_info && eaSize(&room_info->populates) > 0)
	{
		FOR_EACH_IN_EARRAY(ugcEditorGetComponentList()->eaComponents, UGCComponent, detail_component)
		{
			if (detail_component->eType == UGC_COMPONENT_TYPE_OBJECT &&
				ugcComponentIsOnMap(detail_component, doc->map_data->pcName, false) &&
				detail_component->uParentID == room_component->uID)
			{
				foundAnyDetails = true;
				break;
			}
		}
		FOR_EACH_END;
	}

	if (foundAnyDetails)
	{
		UIDialogButtons result = ugcModalDialogMsg( "UGC_MapEditor.PopulateRoomDeletes", "UGC_MapEditor.PopulateRoomDeletesDEtails", UIYes | UINo );
		if (result != UIYes)
		{
			return;
		}
	}

	if( eaSize( &room_info->populates ) == 1 ) {
		ugcMapEditorDoPopulateRoom( doc, room_component, room_info->populates[ 0 ]);
	} else {
		UGCMapEditorPopulateInfo *info;
		UILabel *label;
		UIButton *button;
		UIListColumn *column;
		int x;

		info = calloc(1, sizeof(UGCMapEditorPopulateInfo));
		info->doc = doc;
		info->room_component = room_component;
		info->window = ui_WindowCreate("", 0, 0, 300, 400);
		ui_WidgetSetTextMessage( UI_WIDGET( info->window ), "UGC_MapEditor.PopulateRoom_Title" );
		info->room_info = room_info;

		label = ui_LabelCreate("", 0, 0);
		ui_LabelSetMessage( label, "UGC_MapEditor.PopulateRoom_Label" );
		ui_LabelSetWordWrap( label, true );
		ui_WidgetSetWidthEx( UI_WIDGET( label ), 1, UIUnitPercentage );
		ui_WindowAddChild(info->window, label );

		info->list = ui_ListCreate(parse_UGCRoomPopulateDef, &room_info->populates, 32);
		info->list->fHeaderHeight = 0;
		ui_WidgetSetPosition(UI_WIDGET(info->list), 0, 50 );
		ui_WidgetSetDimensionsEx(UI_WIDGET(info->list), 1, 1, UIUnitPercentage, UIUnitPercentage);
		ui_WidgetSetPaddingEx(UI_WIDGET(info->list), 0, 0, 0, UGC_ROW_HEIGHT);
		ui_WindowAddChild(info->window, info->list);
		ui_ListSetActivatedCallback(info->list, ugcMapEditorPopulateRoomOK, info);
		ui_ListSetSelectedRow( info->list, 0 );

		column = ui_ListColumnCreateParseMessage("Object Set", "DisplayName", NULL);
		ui_ListAppendColumn(info->list, column);

		button = ui_ButtonCreate("", 0, 0, ugcMapEditorPopulateRoomOK, info);
		ui_ButtonSetMessage( button, "UGC.Ok" );
		ui_WidgetSetPositionEx(UI_WIDGET(button), 0, 0, 0, 0, UIBottomRight);
		ui_WidgetSetDimensions( UI_WIDGET( button ), 80, 22 );
		ui_WindowAddChild(info->window, button);
		x = ui_WidgetGetNextX(UI_WIDGET(button)) + 5;

		button = ui_ButtonCreate("", 0, 0, ugcMapEditorPopulateRoomCancel, info);
		ui_ButtonSetMessage( button, "UGC.Cancel" );
		ui_WidgetSetPositionEx(UI_WIDGET(button), x, 0, 0, 0, UIBottomRight);
		ui_WidgetSetDimensions( UI_WIDGET( button ), 80, 22 );
		ui_WindowAddChild(info->window, button);
		x = ui_WidgetGetNextX(UI_WIDGET(button)) + 5;

		ui_WindowSetModal(info->window, true);
		elUICenterWindow(info->window);
		ui_WindowShowEx(info->window, true);
	}
}

static bool ugcMapEditorSelectionContainsRooms( UGCMapEditorDoc* doc )
{
	int it;
	for( it = 0; it != eaiSize(&doc->selected_components); it++ ) {
		UGCComponent *component = ugcEditorFindComponentByID( doc->selected_components[ it ]);

		if( component->eType == UGC_COMPONENT_TYPE_ROOM ) {
			return true;
		}
	}

	return false;
}

static void ugcMapEditorSearchWindowSelectCB( UserData rawDoc, const char* zmName, const char* logicalName )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCMapEditorDoc* doc = rawDoc;

	assert( stricmp( zmName, doc->map_data->pcName ) == 0 );
	if( logicalName ) {
		UGCComponent* component = ugcComponentFindByLogicalName( ugcProj->components, logicalName );
		if( component ) {
			ugcMapEditorSetSelectedComponent( doc, component->uID, 0, true, true );
		}
	}
}

void ugcMapEditorHandleAction(UGCMapEditorDoc *doc, UGCActionID action)
{
	// [NNO-16680] While dragging, prevent actions to prevent crashes.
	if( doc->drag_state ) {
		return;
	}

	switch (action)
	{
		xcase UGC_ACTION_MAP_EDIT_NAME:
			ugcMapEditorGlobalPropertiesWindowShow( doc );

		xcase UGC_ACTION_MAP_EDIT_BACKDROP:
			ugcMapEditorBackdropWindowShow( doc );
		
		xcase UGC_ACTION_PLAY_MAP: case UGC_ACTION_PLAY_MAP_FROM_LOCATION:
			if(ugcEditorHasContextMenuPosition()) {
				Vec3 pos;
				U32 room_id;
				ugcLayoutGetDefaultPlacement(doc, &room_id, pos);
				ugcEditorPlay(doc->doc_name, 0, false, pos, NULL);
			} else {
				ugcEditorPlay(doc->doc_name, 0, false, NULL, NULL);
			}
		xcase UGC_ACTION_MAP_DELETE:
			ugcEditorDeleteMap(doc);
		xcase UGC_ACTION_MAP_DUPLICATE:
			ugcEditorDuplicateMap(doc);
		xcase UGC_ACTION_DELETE:
			ugcMapEditorDeleteSelected(doc);
		xcase UGC_ACTION_CUT:
			{
				UGCEditorCopyBuffer *buffer = ugcEditorStartCopy(UGC_CUT_COMPONENT);
				ugcMapEditorCutSelected(doc, buffer);
			}
		xcase UGC_ACTION_COPY:
			{
				UGCEditorCopyBuffer *buffer = ugcEditorStartCopy(UGC_COPY_COMPONENT);
				ugcMapEditorCopySelected(doc, buffer);
			}
		xcase UGC_ACTION_PASTE:
			{
				UGCEditorCopyBuffer *buffer = ugcEditorCurrentCopy();
				ugcMapEditorPaste(doc, buffer, false, false);
			}
		xcase UGC_ACTION_DUPLICATE:
			ugcMapEditorDuplicateSelected(doc);
		xcase UGC_ACTION_SET_BACKDROP:
			// NO LONGER USED?
		xcase UGC_ACTION_CREATE_MARKER:
			ugcMapEditorAddComponentPlace(NULL, (void*)(intptr_t)UGC_COMPONENT_TYPE_ROOM_MARKER);
		xcase UGC_ACTION_CREATE_RESPAWN:
			ugcMapEditorAddComponentPlace(NULL, (void*)(intptr_t)UGC_COMPONENT_TYPE_RESPAWN);
		xcase UGC_ACTION_ROOM_CLEAR:
			ugcMapEditorClearRoom(doc, doc->selected_components);
		xcase UGC_ACTION_ROOM_POPULATE:
			ugcMapEditorPopulateRoom(doc, doc->selected_components);
		xcase UGC_ACTION_DESELECT_ALL:
			ugcMapEditorClearSelection(doc);
			ugcEditorQueueUIUpdate();

		xcase UGC_ACTION_MOVE_UP: case UGC_ACTION_MOVE_DOWN:
		case UGC_ACTION_MOVE_LEFT: case UGC_ACTION_MOVE_RIGHT: {
			int delta = 1;

			if( inpLevelPeek( INP_SHIFT )) {
				delta *= 5;
			}
			if( ugcMapEditorSelectionContainsRooms( doc )) {
				delta *= UGC_ROOM_GRID;
			}

			switch( action ) {
				xcase UGC_ACTION_MOVE_UP:
					ugcMapEditorMoveSelection( doc, 0, +delta );
				xcase UGC_ACTION_MOVE_DOWN:
					ugcMapEditorMoveSelection( doc, 0, -delta );
				xcase UGC_ACTION_MOVE_LEFT:
					ugcMapEditorMoveSelection( doc, -delta, 0 );
				xcase UGC_ACTION_MOVE_RIGHT:
					ugcMapEditorMoveSelection( doc, +delta, 0 );
			}
		}

		xcase UGC_ACTION_MAP_SEARCH_COMPONENT:
			if( !doc->search_window ) {
				doc->search_window = ugcMapSearchWindowCreate( ugcEditorGetProjectData(), doc->map_data->pcName, ugcMapEditorSearchWindowSelectCB, doc );
			}
			ui_WindowPresentEx( doc->search_window, true );

		xcase UGC_ACTION_MAP_SET_LAYOUT_MODE:
			doc->mode = UGC_MAP_EDITOR_LAYOUT;
			ugcEditorQueueUIUpdate();

		xcase UGC_ACTION_MAP_SET_DETAIL_MODE:
			doc->mode = UGC_MAP_EDITOR_DETAIL;
			ugcEditorQueueUIUpdate();
	}
}

static bool ugcMapEditorIsSelectionValid(UGCMapEditorDoc *doc, bool requires_delete, bool requires_non_actor)
{
	int idx;
	if (eaiSize(&doc->selected_components) == 0)
		return false;

	for (idx = 0; idx < eaiSize(&doc->selected_components); idx++)
	{
		UGCComponent *component = ugcEditorFindComponentByID(doc->selected_components[idx]);
		UGCComponent *parent_component = component ? ugcEditorFindComponentByID(component->uParentID) : NULL;
		if (!component)
			return false;

		if (requires_delete && !ugcLayoutCanDeleteComponent(component))
			return false;

		if (requires_non_actor && parent_component && parent_component->eType == UGC_COMPONENT_TYPE_KILL)
			return false;
	}
	return true;
}

bool ugcMapEditorQueryAction(UGCMapEditorDoc *doc, UGCActionID action, char** out_estr)
{
	// Can't do anything if we have no doc which means we have no maps
	if (doc==NULL || !ugcMapFindByName( ugcEditorGetProjectData(), doc->doc_name ))
	{
		return(false);
	}

	switch (action)
	{
		case UGC_ACTION_MAP_EDIT_NAME:
			return true;
		
		case UGC_ACTION_PLAY_MAP:
			{
				UGCComponent *spawn_point = ugcMissionGetDefaultComponentForMap(ugcEditorGetProjectData(), UGC_COMPONENT_TYPE_SPAWN, doc->map_data->pcName);
				if (!spawn_point)
				{
					estrPrintf(out_estr, "Map has no start spawn. Place a room to create a default spawn.");
					return false;
				}
			}
			return true;
		case UGC_ACTION_PLAY_MAP_FROM_LOCATION:
			return ugcEditorHasContextMenuPosition();

		case UGC_ACTION_MAP_SEARCH_COMPONENT:
			return true;
		
		case UGC_ACTION_MAP_DELETE:
		case UGC_ACTION_MAP_DUPLICATE:
			// Don't allow duplicate or delete of an unfinished/uninitialized map.
			//   Duplicating will crash. Delete will do nothing as the map has
			//   objectives on it already
			if (doc->map_data==NULL  || doc->map_data->pUnitializedMap!=NULL)
			{
				return false;
			}
			return true;
		case UGC_ACTION_PASTE:
			// Don't allow paste onto an unfinished/uninitialized map.
			if (doc->map_data==NULL  || doc->map_data->pUnitializedMap!=NULL)
			{
				return false;
			}

			{
				UGCEditorCopyType type = SAFE_MEMBER(ugcEditorCurrentCopy(), eType);
				if (type == UGC_COPY_COMPONENT || type == UGC_CUT_COMPONENT)
				{
					return true;
				}
			}
			return false;
		case UGC_ACTION_DUPLICATE:
		case UGC_ACTION_COPY:
		case UGC_ACTION_CUT:
			{
				return ugcMapEditorIsSelectionValid(doc, false, true);
			}
		case UGC_ACTION_DELETE:
			{
				return ugcMapEditorIsSelectionValid(doc, true, false);
			}
		case UGC_ACTION_MAP_EDIT_BACKDROP:
			return true;
		case UGC_ACTION_SET_BACKDROP:
		case UGC_ACTION_CREATE_MARKER:
		case UGC_ACTION_CREATE_RESPAWN:
			return true;
		xcase UGC_ACTION_ROOM_CLEAR:
		case UGC_ACTION_ROOM_POPULATE:
			{
				UGCComponent *component = eaiSize(&doc->selected_components) == 1 ? ugcEditorFindComponentByID(doc->selected_components[0]) : NULL;
				return (component != NULL && component->eType == UGC_COMPONENT_TYPE_ROOM);
			}
		case UGC_ACTION_DESELECT_ALL:
			return ugcMapEditorIsSelectionValid(doc, false, false);

		xcase UGC_ACTION_MOVE_UP: case UGC_ACTION_MOVE_DOWN:
		case UGC_ACTION_MOVE_LEFT: case UGC_ACTION_MOVE_RIGHT:
			return ugcMapEditorIsSelectionValid( doc, false, false );

		xcase UGC_ACTION_MAP_SET_LAYOUT_MODE: case UGC_ACTION_MAP_SET_DETAIL_MODE:
			return true;
	}
	return false;
}

/// Selection

static void ugcMapEditorFilterSelection(UGCMapEditorDoc *doc, UGCComponent *new_component)
{
	int idx;
	for (idx = eaiSize(&doc->selected_components)-1; idx >= 0; --idx)
	{
		UGCComponent *component = ugcEditorFindComponentByID(doc->selected_components[idx]);

		if (new_component->eType == UGC_COMPONENT_TYPE_ROOM && component->eType != UGC_COMPONENT_TYPE_ROOM)
		{
			eaiRemove(&doc->selected_components, idx);
		}
		else if (new_component->eType != UGC_COMPONENT_TYPE_ROOM && component->eType == UGC_COMPONENT_TYPE_ROOM)
		{
			eaiRemove(&doc->selected_components, idx);
		}
	}
}

//this is called when selecting on the left pane, ctrl-clicking on things on the map, and ctrl-dragging on the map.
void ugcMapEditorAddSelectedComponent(UGCMapEditorDoc *doc, U32 component_id, bool scroll_to_selected, bool zoom_to_selected)
{
	UGCUIMapEditorComponent *component_widget;
	UGCComponent *component = ugcEditorFindComponentByID(component_id);

	ugcMapEditorFilterSelection(doc, component);
	eaiPush(&doc->selected_components, component_id);

	component_widget = ugcLayoutUIGetComponentByID(doc, component_id);
	if (component_widget && scroll_to_selected)
	{
		UGCUIMapEditorWidget* selected_widget = UGC_UI_MAP_EDITOR_WIDGET( component_widget );

		// Scroll to selected object
		ui_ScrollAreaScrollToPosition( doc->map_widget, selected_widget->x, selected_widget->y );
		doc->map_widget->scrollToTargetWait = 1;
		doc->map_widget->autoScrollCenter = 1;
	}

	if (zoom_to_selected && component && ugcMapGetType(doc->map_data) == UGC_MAP_TYPE_INTERIOR)
	{
		if (component->eType == UGC_COMPONENT_TYPE_ROOM)
			ugcMapEditorZoomToLayoutMode(doc);
		else
			ugcMapEditorZoomToDetailMode(doc);
	}

	if (component && component->sPlacement.uRoomID == GENESIS_UNPLACED_ID)
	{
		ugcUnplacedListSetSelectedComponent( doc->unplaced_list, component );
	}
}

void ugcMapEditorRemoveSelectedComponent(UGCMapEditorDoc *doc, U32 component_id)
{
	if (eaiSize(&doc->selected_components) == 0)
	{
		return;
	}

	eaiFindAndRemoveFast(&doc->selected_components, component_id);
	ugcEditorQueueUIUpdate();
}

// MJF TODO: remove "placement" as a parameter
//this is called when you click on a component on the map. Called twice when unplaced is selected from tasks menu.  
//Not called when selected from the left panel. 
void ugcMapEditorSetSelectedComponent(UGCMapEditorDoc *doc, U32 component_id, int placement, bool scroll_to_selected, bool zoom_to_selected)
{
	UGCUIMapEditorComponent *component_widget;
	UGCComponent *component = ugcEditorFindComponentByID(component_id);

	eaiClear( &doc->prev_selected_components );
	if (eaiSize(&doc->selected_components) != 1 ||
		doc->selected_components[0] != component_id)
	{
		eaiSetSize(&doc->selected_components, 1);
		doc->selected_components[0] = component_id;

		ugcEditorQueueUIUpdate();
	}

	component_widget = ugcLayoutUIGetComponentByID(doc, component_id);
	if (component_widget && scroll_to_selected)
	{
		UGCUIMapEditorWidget *selected_widget = UGC_UI_MAP_EDITOR_WIDGET(component_widget);

		// Scroll to selected object
		ui_ScrollAreaScrollToPosition(doc->map_widget, selected_widget->x, selected_widget->y);
		doc->map_widget->scrollToTargetWait = 1;
		doc->map_widget->autoScrollCenter = 1;
	}

	if (zoom_to_selected && component && ugcMapGetType(doc->map_data) == UGC_MAP_TYPE_INTERIOR)
	{
		//zoom to selected object
		if (component->eType == UGC_COMPONENT_TYPE_ROOM)
			ugcMapEditorZoomToLayoutMode(doc);
		else
			ugcMapEditorZoomToDetailMode(doc);
	}

	if (component && component->sPlacement.uRoomID == GENESIS_UNPLACED_ID)
	{
		ugcUnplacedListSetSelectedComponent(doc->unplaced_list, component);
	}

	ugcMapEditorUpdateFocusForSelection( doc );
}

void ugcMapEditorClearSelection(UGCMapEditorDoc *doc)
{
	eaiCopy(&doc->prev_selected_components, &doc->selected_components);
	eaiClear(&doc->selected_components);
}

void ugcMapEditorClearSelectionWidgetCB(UIWidget* ignored, UGCMapEditorDoc *doc)
{
	ugcMapEditorClearSelection( doc );
	ugcEditorQueueUIUpdate();
}

bool ugcMapEditorIsComponentSelected(UGCMapEditorDoc *doc, int component_id)
{
	if (eaiFind(&doc->selected_components, component_id) == -1)
		return false;
	return true;
}

bool ugcMapEditorIsAnyComponentSelected(UGCMapEditorDoc *doc, int* eaComponents)
{
	int it;
	for( it = 0; it != eaiSize( &eaComponents ); ++it ) {
		if( ugcMapEditorIsComponentSelected( doc, eaComponents[ it ])) {
			return true;
		}
	}

	return false;
}


bool ugcMapEditorIsComponentPrevSelected(UGCMapEditorDoc *doc, int component_id)
{
	if (eaiFind(&doc->prev_selected_components, component_id) == -1)
		return false;
	return true;
}

bool ugcMapEditorIsAnyComponentPrevSelected(UGCMapEditorDoc *doc, int* eaComponents)
{
	int it;
	for( it = 0; it != eaiSize( &eaComponents ); ++it ) {
		if( ugcMapEditorIsComponentPrevSelected( doc, eaComponents[ it ])) {
			return true;
		}
	}

	return false;
}

void ugcMapEditorSetHighlightedComponent(UGCMapEditorDoc *doc, U32 component_id)
{
	eaiSetSize(&doc->highlight_components, 1);
	doc->highlight_components[0] = component_id;
}

void ugcMapEditorClearHighlight(UGCMapEditorDoc *doc)
{
	eaiClear(&doc->highlight_components);
}

bool ugcMapEditorIsComponentHighlighted(UGCMapEditorDoc *doc, int component_id)
{
	if( eaiFind( &doc->highlight_components, component_id ) == -1 ) {
		return false;
	}

	return true;
}

bool ugcMapEditorIsAnyComponentHighlighted(UGCMapEditorDoc *doc, int* eaComponents)
{
	int it;
	for( it = 0; it != eaiSize( &eaComponents ); ++it ) {
		if( ugcMapEditorIsComponentHighlighted( doc, eaComponents[ it ])) {
			return true;
		}
	}

	return false;
}

void ugcMapEditorSelectUnplacedTab(UGCMapEditorDoc *doc)
{
	ugcUnplacedListSetSelectedComponent(doc->unplaced_list, NULL);
}

void ugcMapEditorClearObjectSelection(UGCMapEditorDoc *doc, U32 component_id, U32 component_id_to_select)
{
	if (eaiFind(&doc->selected_components, component_id) != -1) {
		ugcMapEditorClearSelection(doc);

		if( component_id_to_select ) {
			UGCComponent* toSelect = ugcEditorFindComponentByID( component_id_to_select );
			if( toSelect ) {
				ugcMapEditorSetSelectedComponent( doc, component_id_to_select, 0, false, false );
			}
		}
	}
	if (eaiFind(&doc->highlight_components, component_id) != -1) {
		ugcMapEditorClearHighlight(doc);
	}
	
	ugcEditorQueueUIUpdate();
}

void ugcMapEditorMoveSelection(UGCMapEditorDoc* doc, int dx, int dy)
{
	int it;

	for( it = 0; it != eaiSize(&doc->selected_components); it++ ) {
		UGCComponent *component = ugcEditorFindComponentByID( doc->selected_components[ it ]);
		bool found = false;
		int otherIt;

		// Skip any components whose parents are already in the tree
		for( otherIt = 0; otherIt < eaiSize( &doc->selected_components ); otherIt++ ) {
			if( it != otherIt && ugcComponentHasParent( ugcEditorGetComponentList(), component, doc->selected_components[ otherIt ])) {
				found = true;
				break;
			}
		}
		if( found ) {
			continue;
		}

		// Skip unplaced components
		if( component->sPlacement.uRoomID == GENESIS_UNPLACED_ID ) {
			continue;
		}

		{
			UGCProjectData* pUGCProj = ugcEditorGetProjectData();
			Vec3 oldPos;
			Vec2 minBounds = { -1e8, -1e8 };
			Vec2 maxBounds = { 1e8, 1e8 };

			copyVec3( component->sPlacement.vPos, oldPos );
			component->sPlacement.vPos[0] += dx;
			component->sPlacement.vPos[2] += dy;
			ugcComponentOpSetPlacement(pUGCProj, component, ugcMapFindByName(pUGCProj, component->sPlacement.pcMapName), 0);
			ugcLayoutComponentFixupChildLocations( component, oldPos, component->sPlacement.vPos, minBounds, maxBounds );
		}
	}

	ugcEditorApplyUpdate();
}

/// Generation

static ZoneMapEncounterInfo *ugcMapEditorGenerateZeniData(UGCMapEditorDoc *doc, UGCZeniPickerFilterType filterType)
{
	ZoneMapEncounterInfo *info;
	UGCComponentList *com_list = ugcEditorGetComponentList();
	UGCMap *map = doc->map_data;
	ZoneMapEncounterRegionInfo *region = StructCreate(parse_ZoneMapEncounterRegionInfo);
	char ns[RESOURCE_NAME_MAX_SIZE], base[RESOURCE_NAME_MAX_SIZE];
	Vec3 playableMin;
	Vec3 playableMax;
	ugcGetZoneMapPlaceableBounds( playableMin, playableMax, map->pPrefab->map_name, true );

	info = StructCreate(parse_ZoneMapEncounterInfo);
	info->map_name = map->pcName;

	resExtractNameSpace(info->map_name, ns, base);

	FOR_EACH_IN_EARRAY(com_list->eaComponents, UGCComponent, component)
	{
		if (ugcComponentIsOnMap(component, base, false))
		{
			ZoneMapEncounterObjectInfo *obj_info = StructCreate(parse_ZoneMapEncounterObjectInfo);
			UGCComponent *defaultPrompt = NULL;
			switch (component->eType)
			{
				case UGC_COMPONENT_TYPE_SPAWN:
					obj_info->type = WL_ENC_SPAWN_POINT;
					break;
				case UGC_COMPONENT_TYPE_OBJECT:
				case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
				case UGC_COMPONENT_TYPE_CLUSTER_PART:
					obj_info->type = WL_ENC_INTERACTABLE;
					obj_info->interactType = WL_ENC_CLICKIE;
					break;
				case UGC_COMPONENT_TYPE_FAKE_DOOR:
					obj_info->type = WL_ENC_INTERACTABLE;
					obj_info->interactType = WL_ENC_DOOR;
					break;
				case UGC_COMPONENT_TYPE_ROOM_DOOR:
					obj_info->type = WL_ENC_INTERACTABLE;
					obj_info->interactType = WL_ENC_UGC_OPEN_DOOR;
					break;
				case UGC_COMPONENT_TYPE_DESTRUCTIBLE:
					obj_info->type = WL_ENC_INTERACTABLE;
					obj_info->interactType = WL_ENC_DESTRUCTIBLE;
					break;
				case UGC_COMPONENT_TYPE_CONTACT:
					obj_info->type = WL_ENC_INTERACTABLE;
					obj_info->interactType = WL_ENC_CONTACT;

					defaultPrompt = ugcComponentFindDefaultPromptForID(com_list, component->uID );
					break;
				case UGC_COMPONENT_TYPE_KILL:
					obj_info->type = WL_ENC_ENCOUNTER;
					// Make sure this is an actual kill
					{
						if (!ugcProjectFilterAllegiance(ugcEditorGetProjectData(), "Enemy", component->iObjectLibraryId))
						{
							StructDestroySafe(parse_ZoneMapEncounterObjectInfo, &obj_info);
						}
					}
					break;
				case UGC_COMPONENT_TYPE_ROOM_MARKER:
				case UGC_COMPONENT_TYPE_PLANET:
					obj_info->type = WL_ENC_NAMED_VOLUME;
					break;
				case UGC_COMPONENT_TYPE_DIALOG_TREE:
					obj_info->type = WL_ENC_INTERACTABLE;
					obj_info->interactType = WL_ENC_DIALOGTREE;
					break;
				case UGC_COMPONENT_TYPE_WHOLE_MAP:
					obj_info->type = WL_ENC_INTERACTABLE;
					obj_info->interactType = WL_ENC_DOOR;
					
					if( ugcDefaultsMapTransitionsSpecifyDoor() ) {
						StructDestroySafe( parse_ZoneMapEncounterObjectInfo, &obj_info );
					}
					break;
				case UGC_COMPONENT_TYPE_REWARD_BOX:
					obj_info->type = WL_ENC_INTERACTABLE;
					obj_info->interactType = WL_ENC_REWARD_BOX;
					break;
				default:
					StructDestroySafe(parse_ZoneMapEncounterObjectInfo, &obj_info);
					break;
			}

			
						
			if (obj_info)
			{
				char buffer[ 256 ];
					
				obj_info->logicalName = StructAllocString(ugcComponentGetLogicalNameTemp(component));
				ugcComponentGetDisplayName( buffer, ugcEditorGetProjectData(), component, false );
				obj_info->ugcDisplayName = StructAllocString( buffer );
				obj_info->ugcDisplayDetails = StructAllocString( buffer );
				obj_info->ugcComponentID = component->uID;
				copyVec3(component->sPlacement.vPos, obj_info->pos);
				
				if( filterType != UGCZeniPickerType_Usable_As_Warp ) {
					if(   component->bInteractIsMissionReturn
						  || ugcMissionFindLinkByExitComponent( ugcEditorGetProjectData(), component->uID )) {
						obj_info->ugcIsInvalidSelection = true;
					}
				}

				// If a component is outside the playable area, then
				// it can't be picked for any trigger or story
				// behavior.
				if( map->pPrefab ) {
					float* pos = component->sPlacement.vPos;
					if(   playableMin[ 0 ] > pos[ 0 ] || pos[ 0 ] > playableMax[ 0 ]
						  || playableMin[ 2 ] > pos[ 2 ] || pos[ 2 ] > playableMax[ 2 ]) {
						obj_info->ugcIsInvalidSelection = true;
					}
				}
				
				eaPush(&info->objects, obj_info);
			}

			if (defaultPrompt)
			{
				ZoneMapEncounterObjectInfo *prompt_obj_info = StructClone(parse_ZoneMapEncounterObjectInfo, obj_info);
				assert(prompt_obj_info);
				prompt_obj_info->type = WL_ENC_INTERACTABLE;
				prompt_obj_info->interactType = WL_ENC_DIALOGTREE;
				prompt_obj_info->logicalName = StructAllocString(ugcComponentGetLogicalNameTemp(defaultPrompt));
				eaPush(&info->objects, prompt_obj_info);
			}
		}
	}
	FOR_EACH_END;

	info->ugc_display_name = ugcMapGetDisplayName( ugcEditorGetProjectData(), map->pcName );
	info->ugc_picker_widget = &ugcLayoutGenerateStaticUI(doc)->widget;
	info->ugc_map_scale = doc->layout_grid_size / doc->layout_kit_spacing;

	region->min[0] = doc->layout_min_pos[0];
	region->min[1] = -1e8;
	region->min[2] = doc->layout_min_pos[1];
	region->max[0] = doc->layout_max_pos[0];
	region->max[1] =  1e8;
	region->max[2] = doc->layout_max_pos[1];
	eaPush(&info->regions, region);

	return info;
}

ZoneMapEncounterInfo **ugcMapEditorBuildEncounterInfos( const char* mapName, UGCZeniPickerFilterType filterType )
{
	UGCMap* map = ugcEditorGetMapByName( mapName );
	ZoneMapEncounterInfo **ret = NULL;

	if( !map->pUnitializedMap ) {
		UGCMapEditorDoc *map_doc = ugcEditorGetMapDoc(map->pcName);
			
		// This requires the map doc to be open
		if (!map_doc)
			map_doc = ugcMapEditorLoadDoc(map->pcName);
		assert( map_doc );
		eaPush(&ret, ugcMapEditorGenerateZeniData(map_doc, filterType));
	}

	return ret;
}

/// Refresh callback

void ugcMapEditorDocRefresh(UGCMapEditorDoc *doc)
{
	doc->map_data = ugcEditorGetMapByName( doc->doc_name );

	// Refresh cached data first, in case anything else is depending on it!
	ugcMapEditorPropertiesRefreshObjectiveModel( doc );

	// Update the selection.  If there are any components selected
	// that do not exist, remove them.
	{
		int it;
		for( it = eaiSize( &doc->selected_components ) - 1; it >= 0; --it ) {
			UGCComponent* component = ugcEditorFindComponentByID( doc->selected_components[ it ]);
			if( !component ) {
				eaiRemove( &doc->selected_components, it );
			}
		}
	}
	
	// Update widgets
	ugcMapEditorUpdateUIForType(doc);
	ugcLayoutUpdateUI(doc);
	
	if( doc->search_window ) {
		ugcMapSearchWindowRefresh( doc->search_window );
	}
}

void ugcMapEditorPropertiesRefreshObjectiveModel( UGCMapEditorDoc* doc )
{
	UGCMissionObjective** objectives = ugcEditorGetMission()->objectives;
	int it;

	for( it = 0; it != beaSize( &doc->beaObjectivesModel ); ++it ) {
		if(   doc->beaObjectivesModel[ it ].key != U32_TO_PTR( DM_INT )
			  && doc->beaObjectivesModel[ it ].key != U32_TO_PTR( DM_END )) {
			free( (char*)doc->beaObjectivesModel[ it ].key );
		}
	}
	beaSetSize( &doc->beaObjectivesModel, 0 );

	{
		StaticDefineInt* accum;
		accum = beaPushEmpty( &doc->beaObjectivesModel );
		accum->key = U32_TO_PTR( DM_INT );
		ugcMapEditorPropertiesRefreshObjectiveModelRecurse( doc, objectives );
		accum = beaPushEmpty( &doc->beaObjectivesModel );
		accum->key = U32_TO_PTR( DM_END );
	}
}

void ugcMapEditorPropertiesRefreshObjectiveModelRecurse( UGCMapEditorDoc* doc, UGCMissionObjective** objectives )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	
	FOR_EACH_IN_EARRAY_FORWARDS( objectives, UGCMissionObjective, objective ) {
		if( objective->type == UGCOBJ_IN_ORDER || objective->type == UGCOBJ_ALL_OF ) {
			ugcMapEditorPropertiesRefreshObjectiveModelRecurse( doc, objective->eaChildren );
		} else {
			const char* mapName = ugcObjectiveInternalMapName( ugcProj, objective );
			if( mapName && resNamespaceBaseNameEq( mapName, doc->doc_name )) {
				StaticDefineInt* entry = beaPushEmpty( &doc->beaObjectivesModel );
				entry->key = strdup( ugcMissionObjectiveUIString( objective ));
				entry->value = objective->id;
				ugcMapEditorPropertiesRefreshObjectiveModelRecurse( doc, objective->eaChildren );
			}
		}
	} FOR_EACH_END;
}


/// UI

void ugcMapEditorClearAssetLibrarySelections( UITabGroup* ignored, UGCMapEditorDoc* doc )
{
	if( doc->libraryEmbeddedPicker ) {
		ugcAssetLibraryPaneClearSelected( doc->libraryEmbeddedPicker );
	}
}

void ugcMapEditorUpdateFocusForSelection( UGCMapEditorDoc* doc )
{
	UGCComponent* component = NULL;
	UGCUIMapEditorComponent* component_widget = NULL;
	if( eaiSize( &doc->selected_components ) == 1 ) {
		component = ugcEditorFindComponentByID( doc->selected_components[ 0 ]);
		component_widget = ugcLayoutUIGetComponentByID(doc, doc->selected_components[ 0 ]);
	}

	if( component_widget ) {
		ui_SetFocus( &g_UGCMapEditorComponentWidgetPlaceholder );
	} else if( component && component->sPlacement.uRoomID == GENESIS_UNPLACED_ID ) {
		ui_SetFocus( ugcUnplacedListGetUIWidget( doc->unplaced_list ));
	} else {
		// If nothing else is available, use the backdrop
		ui_SetFocus( &g_UGCMapEditorBackdropWidgetPlaceholder );
	}
}

UGCNoMapsEditorDoc *ugcNoMapsEditorLoadDoc( void )
{
	UGCNoMapsEditorDoc* pDoc = calloc( 1, sizeof( *pDoc ));
	pDoc->pRootPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
	return pDoc;
}

static void ugcMapEditorCreateNewMap( UIButton* ignored, UserData ignored2 )
{
	ugcEditorCreateNewMap( false );
}

void ugcNoMapsEditorDocRefresh(UGCNoMapsEditorDoc *pDoc)
{
	UIPane* pane;
	MEFieldContextEntry* entry;
	UIWidget* widget;
	
	MEContextPush( "UGCNoMapsEditor", NULL, NULL, NULL );
	MEContextSetParent( UI_WIDGET( pDoc->pRootPane ));

	pane = MEContextPushPaneParent( "FTUE" );
	{
		entry = MEContextAddLabelMsg( "Text", "UGC_MapEditor.FTUEAddMap", NULL );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		ENTRY_LABEL( entry )->textFrom = UITop;
		ui_WidgetSetFont( widget, "UGC_Important_Alternate" );
		ui_WidgetSetPositionEx( widget, 0, -UGC_ROW_HEIGHT, 0, 0.5, UITop );
		ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );

		entry = MEContextAddButtonMsg( "UGC_MapEditor.AddMap", "UGC_Icons_Labels_New_02", ugcMapEditorCreateNewMap, NULL, "Button", NULL, "UGC_MapEditor.AddMap.Tooltip" );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_WidgetSetPositionEx( widget, 0, 0, 0, 0.5, UITop );
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ENTRY_BUTTON( entry )->bCenterImageAndText = true;
		widget->width = MAX( widget->width, 200 );
		widget->height = MAX( widget->height, 50 );
		ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
	}
	MEContextPop( "FTUE" );
	ui_PaneSetStyle( pane, "UGC_Story_BackgroundArea", true, false );
	ui_WidgetSetPosition( UI_WIDGET( pane ), 0, 0 );
	ui_WidgetSetDimensionsEx( UI_WIDGET( pane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( UI_WIDGET( pane ), 0, 0, UGC_PANE_TOP_BORDER, 0 );

	MEContextPop( "UGCNoMapsEditor" );
}

void ugcNoMapsEditorSetVisible(UGCNoMapsEditorDoc *pDoc)
{
	ugcEditorSetDocPane( pDoc->pRootPane );
}

void ugcNoMapsEditorOncePerFrame(UGCNoMapsEditorDoc *pDoc, bool isActive)
{
	// nothing to do
}

void ugcNoMapsEditorFreeDoc(UGCNoMapsEditorDoc **ppDoc)
{
	if( *ppDoc ) {
		MEContextDestroyByName( "UGCNoMapsEditor" );
		ui_WidgetQueueFreeAndNull( &(*ppDoc)->pRootPane );
		SAFE_FREE( *ppDoc );
	}
}

void ugcNoMapsEditorHandleAction(UGCNoMapsEditorDoc *pDoc, UGCActionID action)
{
	// nothing to do
}

bool ugcNoMapsEditorQueryAction(UGCNoMapsEditorDoc *pDoc, UGCActionID action, char** out_estr)
{
	// nothing to do
	return false;
}


#include "NNOUGCMapEditor_h_ast.c"
