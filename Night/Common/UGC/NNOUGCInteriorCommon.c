//// UGC custom interior specific routines
////
#include "NNOUGCInteriorCommon.h"
#include "UGCInteriorCommon.h"

#include "CBox.h"
#include "NNOUGCCommon.h"
#include "NNOUGCResource.h"
#include "StringCache.h"
#include "UGCCommon.h"
#include "WorldGrid.h"
#include "wlExclusionGrid.h"
#include "wlUGC.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

////////////////////////////////////////////////////////////////
// Room Buffers (footprint & platforms)
////////////////////////////////////////////////////////////////

static StashTable g_pRoomInfos = NULL;
static char **g_eaDoorTypeList = NULL; // 0 is always "GENERIC_DOOR"

bool g_UGCRoomInfosDebug = false;
AUTO_CMD_INT( g_UGCRoomInfosDebug, ugc_RoomInfosDebug );

UGCComponent *ugcComponentGetRoomParent(const UGCComponentList *list, const UGCComponent *component)
{
	UGCComponent *parent = ugcComponentFindByID(list, component->uParentID);
	if (component->eType == UGC_COMPONENT_TYPE_ROOM)
		return (UGCComponent*)component;
	if (!parent)
		return NULL;
	return ugcComponentGetRoomParent(list, parent);
}

static int ugcRoomGetDoorTypeID(const char *door_type_str)
{
	int idx = eaFindString(&g_eaDoorTypeList, door_type_str);
	if (idx < 0)
	{
		if (eaSize(&g_eaDoorTypeList) == 0)
			eaPush(&g_eaDoorTypeList, strdup("DoorType_GENERIC_DOOR"));
		idx = eaSize(&g_eaDoorTypeList);
		eaPush(&g_eaDoorTypeList, strdup(door_type_str));
	}
	return idx;
}

static bool ugcRoomDetailsCalcHelper( UGCRoomInfo *list, GroupDef *def, GroupInfo *info, GroupInheritedInfo *inherited_info, bool needs_entry)
{
	if (SAFE_MEMBER( def->property_structs.ugc_room_object_properties, eType ) == UGC_ROOM_OBJECT_DETAIL_SET)
	{
		const char *visible_name = NULL;
		UGCRoomDetailDef *detail_def = StructCreate(parse_UGCRoomDetailDef);
		detail_def->iChildCount = eaSize(&def->children);
		detail_def->astrParameter = def->property_structs.physical_properties.pcChildSelectParam;
		
		if (!def->property_structs.ugc_room_object_properties)
		{
			eaPush(&detail_def->eaNames, "!!NO DETAIL NAME!!");
		}
		else
		{
			eaPush(&detail_def->eaNames, REF_STRING_FROM_HANDLE(def->property_structs.ugc_room_object_properties->dVisibleName.hMessage));
		}
		FOR_EACH_IN_EARRAY_FORWARDS(def->children, GroupChild, child)
		{
			GroupDef *child_def = groupChildGetDef(def, child, true);
			if( !child_def || !child_def->property_structs.ugc_room_object_properties )
			{
				eaPush(&detail_def->eaNames, "!!NO OPTION NAME!!" );
			}
			else
			{
				eaPush(&detail_def->eaNames, REF_STRING_FROM_HANDLE(child_def->property_structs.ugc_room_object_properties->dVisibleName.hMessage));
			}
		}
		FOR_EACH_END;
		eaPush(&list->details, detail_def);
	}
	if (SAFE_MEMBER( def->property_structs.ugc_room_object_properties, eType ) == UGC_ROOM_OBJECT_PREPOP_SET)
	{
		const char *visible_name = NULL;
		UGCRoomPopulateDef *populate_def = StructCreate(parse_UGCRoomPopulateDef);

		FOR_EACH_IN_EARRAY(def->children, GroupChild, child)
		{
			UGCRoomPopulateObject *object = StructCreate(parse_UGCRoomPopulateObject);
			Mat3 orientMat;
			F32 pyr[3];
			
			mulVecMat4(child->pos, info->world_matrix, object->vPos);

			mulMat3( info->world_matrix, child->mat, orientMat );
			getMat3YPR( orientMat, pyr );
			object->fRot = DEG(pyr[1]);
			
			object->iGroupUID = child->name_uid;
			object->astrGroupDebugName = allocAddString( child->debug_name );
			eaPush(&populate_def->eaObjects, object);
		}
		FOR_EACH_END;

		COPY_HANDLE( populate_def->hDisplayName, def->property_structs.ugc_room_object_properties->dVisibleName.hMessage );
		eaPush(&list->populates, populate_def);
		return false;
	}
	return true;
}

static void ugcRoomBufferMergeFootprint( UGCRoomInfo* roomBuffer, Vec3 min, Vec3 max )
{
	IVec2 footprintMin;
	IVec2 footprintMax;

	footprintMin[0] = floor( min[0] / UGC_ROOM_GRID + 0.5 );
	footprintMin[1] = floor( min[2] / UGC_ROOM_GRID + 0.5 );
	footprintMax[0] = floor( max[0] / UGC_ROOM_GRID + 0.5 );
	footprintMax[1] = floor( max[2] / UGC_ROOM_GRID + 0.5 );
	
	roomBuffer->footprint_min[0] = MIN( roomBuffer->footprint_min[0], MIN( footprintMin[0], footprintMax[0] ));
	roomBuffer->footprint_min[1] = MIN( roomBuffer->footprint_min[1], MIN( footprintMin[1], footprintMax[1] ));
	roomBuffer->footprint_max[0] = MAX( roomBuffer->footprint_max[0], MAX( footprintMin[0], footprintMax[0] ) - 1 );
	roomBuffer->footprint_max[1] = MAX( roomBuffer->footprint_max[1], MAX( footprintMin[1], footprintMax[1] ) - 1 );
}

static bool ugcRoomFootprintCalcHelper(UGCRoomInfo *room_buffer, GroupDef *def, GroupInfo *info, GroupInheritedInfo *inherited_info, bool needs_entry)
{
	if( groupIsVolumeType(def, "UGCRoomFootprint") && def->property_structs.volume ) {
		Vec3 volume_min, volume_max;
		mulVecMat4( def->property_structs.volume->vBoxMin, info->world_matrix, volume_min );
		mulVecMat4( def->property_structs.volume->vBoxMax, info->world_matrix, volume_max );

		ugcRoomBufferMergeFootprint( room_buffer, volume_min, volume_max );
	}
	if ( SAFE_MEMBER( def->property_structs.ugc_room_object_properties, eType ) == UGC_ROOM_OBJECT_DOOR )
	{
		int i;
		int *child_path = NULL;
		char *scope_name, *scope_path;
		for (i = eaiSize(&inherited_info->idxs_in_parent)-1; i > 0; --i)
		{
			eaiInsert(&child_path, inherited_info->parent_defs[i-1]->children[inherited_info->idxs_in_parent[i]]->uid_in_parent, 0);
		}

		if (groupDefFindScopeNameByFullPath(inherited_info->parent_defs[0], child_path, eaiSize(&child_path), &scope_name) &&
			stashFindPointer(inherited_info->parent_defs[0]->name_to_path, scope_name, &scope_path))
		{
			UGCRoomDoorInfo *door = calloc(1, sizeof(UGCRoomDoorInfo));
			door->pos[0] = floor(info->world_matrix[3][0]/UGC_ROOM_GRID+0.1f);
			door->pos[1] = 0;
			door->pos[2] = floor(info->world_matrix[3][2]/UGC_ROOM_GRID+0.1f);
			door->rot = getVec3Yaw(info->world_matrix[0]);
			door->door_id = def->name_uid;
			door->astrScopeName = allocAddString(scope_name);
			door->astrScopePath = allocAddString(scope_path);

			// Index 0 is reserved for "closed"; door types start at 1
			for (i = 1; i < eaSize(&def->children); i++)
			{
				GroupDef *child_def = groupChildGetDef(def, def->children[i], true);
				if (child_def)
				{
					eaiPush(&door->eaiDoorTypeIDs, ugcRoomGetDoorTypeID(child_def->tags));
				}
				else
				{
					eaiPush(&door->eaiDoorTypeIDs, -1);
				}
			}

			eaPush(&room_buffer->doors, door);
		}
		eaiDestroy(&child_path);
	}

	return true;
}

static bool ugcRoomFootprintCalcFallbackHelper(UGCRoomInfo *room_buffer, GroupDef *def, GroupInfo *info, GroupInheritedInfo *inherited_info, bool needs_entry)
{
	if( def->property_structs.volume && def->property_structs.room_properties ) {
		WorldRoomType roomType = def->property_structs.room_properties->eRoomType;
		if( roomType == WorldRoomType_Room || roomType == WorldRoomType_Partition ) {
			Vec3 volume_min, volume_max;
			mulVecMat4( def->property_structs.volume->vBoxMin, info->world_matrix, volume_min );
			mulVecMat4( def->property_structs.volume->vBoxMax, info->world_matrix, volume_max );

			ugcRoomBufferMergeFootprint( room_buffer, volume_min, volume_max );
		}
	}

	return true;
}

static void ugcRoomBufferCalcFootprint(UGCRoomInfo *room_buffer, GroupDef *def, GroupInfo *info, bool is_fallback)
{
	bool isFootprint;

	if( !is_fallback ) {
		isFootprint = (def->property_structs.volume != NULL
					   && groupIsVolumeType( def, "UGCRoomFootprint" ));
	} else {
		if( def->property_structs.volume && def->property_structs.room_properties ) {
			WorldRoomType roomType = def->property_structs.room_properties->eRoomType;
			isFootprint = (roomType == WorldRoomType_Room || roomType == WorldRoomType_Partition);
		} else {
			isFootprint = false;
		}
	}
	
	if( isFootprint ) {
		IVec2 footprint_min, footprint_max;
		int x, z;
		int width = room_buffer->footprint_max[0]+1-room_buffer->footprint_min[0];
		int height = room_buffer->footprint_max[1]+1-room_buffer->footprint_min[1];
		Vec3 volume_min, volume_max;
		mulVecMat4(def->property_structs.volume->vBoxMin, info->world_matrix, volume_min);
		mulVecMat4(def->property_structs.volume->vBoxMax, info->world_matrix, volume_max);

		footprint_min[0] = floor( volume_min[0] / UGC_ROOM_GRID + 0.5 ) - room_buffer->footprint_min[0];
		footprint_min[1] = floor( volume_min[2] / UGC_ROOM_GRID + 0.5 ) - room_buffer->footprint_min[1];
		footprint_max[0] = floor( volume_max[0] / UGC_ROOM_GRID + 0.5 ) - room_buffer->footprint_min[0];
		footprint_max[1] = floor( volume_max[2] / UGC_ROOM_GRID + 0.5 ) - room_buffer->footprint_min[1];

		assert(0 <= footprint_min[0] && footprint_min[0] <= width);
		assert(0 <= footprint_max[0] && footprint_max[0] <= width);
		assert(0 <= footprint_min[1] && footprint_min[1] <= height);
		assert(0 <= footprint_max[1] && footprint_max[1] <= height);

		for (z = MIN(footprint_min[1], footprint_max[1]); z < MAX(footprint_min[1], footprint_max[1]); z++)
			for (x = MIN(footprint_min[0], footprint_max[0]); x < MAX(footprint_min[0], footprint_max[0]); x++)
				room_buffer->footprint_buf[x+z*width] = 1;
	}
}

static bool ugcRoomFootprintDrawHelper(UGCRoomInfo *room_buffer, GroupDef *def, GroupInfo *info, GroupInheritedInfo *inherited_info, bool needs_entry)
{
	ugcRoomBufferCalcFootprint( room_buffer, def, info, false );
	return true;
}

static bool ugcRoomFootprintDrawFallbackHelper(UGCRoomInfo *room_buffer, GroupDef *def, GroupInfo *info, GroupInheritedInfo *inherited_info, bool needs_entry)
{
	ugcRoomBufferCalcFootprint( room_buffer, def, info, true );
	return true;
}


UGCRoomInfo* ugcRoomGetRoomInfo( int objectLibraryID )
{
	if( !objectLibraryID ) {
		return NULL;
	}
	
	if( g_UGCRoomInfosDebug ) {
		stashTableDestroy( g_pRoomInfos );
		g_pRoomInfos = NULL;
	}
	if( !g_pRoomInfos ) {
		g_pRoomInfos = stashTableCreateInt( 16 );
	}
	
	{
		UGCRoomInfo* roomInfo = NULL;
		if( stashIntFindPointer( g_pRoomInfos, objectLibraryID, &roomInfo )) {
			return roomInfo;
		} else {
			const GroupDef* def = objectLibraryGetGroupDef( objectLibraryID, false );
			if( !def ) {
				return NULL;
			}
			
			roomInfo = ugcRoomAllocRoomInfo( def );
			stashIntAddPointer( g_pRoomInfos, objectLibraryID, roomInfo, true );
			return roomInfo;
		}
	}
}

UGCRoomInfo* ugcRoomAllocRoomInfo( const GroupDef* room_group )
{
	GroupDef* def = (GroupDef*)room_group; // It's a shame I need to cast away const here.
	UGCRoomInfo* roomInfo = calloc( 1, sizeof( *roomInfo ));
	bool footprint_is_fallback = false;
	bool found_platforms = false;
	int level;

	roomInfo->iNumLevels = 1;
	groupTreeTraverse( NULL, def, NULL, NULL, ugcRoomDetailsCalcHelper, roomInfo, true, true );

	if( eaiSize( &roomInfo->uLevelGroupIDs ) == 0 ) {
		eaiPush( &roomInfo->uLevelGroupIDs, room_group->name_uid );
	}

	setVec2(roomInfo->footprint_min, 1e8, 1e8);
	setVec2(roomInfo->footprint_max, -1e8, -1e8);
	groupTreeTraverse(NULL, def, NULL, NULL, ugcRoomFootprintCalcHelper, roomInfo, true, true);

	if(	  roomInfo->footprint_min[0] > roomInfo->footprint_max[0]
		  || roomInfo->footprint_min[1] > roomInfo->footprint_max[1] ) {
		groupTreeTraverse(NULL, def, NULL, NULL, ugcRoomFootprintCalcFallbackHelper, roomInfo, true, true );
		footprint_is_fallback = true;
	}
	
	if(	  roomInfo->footprint_min[0] <= roomInfo->footprint_max[0]
		  && roomInfo->footprint_min[1] <= roomInfo->footprint_max[1]) {
		int w = (roomInfo->footprint_max[0]+1-roomInfo->footprint_min[0]);
		int h = (roomInfo->footprint_max[1]+1-roomInfo->footprint_min[1]);
		roomInfo->footprint_buf = calloc(1, w * h);
		if( footprint_is_fallback ) {
			groupTreeTraverse(NULL, def, NULL, NULL, ugcRoomFootprintDrawFallbackHelper, roomInfo, true, true);
		} else {
			groupTreeTraverse(NULL, def, NULL, NULL, ugcRoomFootprintDrawHelper, roomInfo, true, true);
		}
	}
	else
	{
		return NULL;
	}

	for (level = 0; level < eaiSize(&roomInfo->uLevelGroupIDs); level++)
	{
		ExclusionVolumeGroup **platforms = NULL;
		HeightMapExcludeGrid *grid = NULL;

		ugcZoneMapRoomGetPlatforms(room_group->name_uid, level, &platforms);

		if (eaSize(&platforms) > 0)
		{
			grid = exclusionGridCreate(0, 0, 1, 1);
			FOR_EACH_IN_EARRAY(platforms, ExclusionVolumeGroup, volume_group)
			{
				ExclusionObject *object = calloc(1, sizeof(ExclusionObject));
				identityMat4(object->mat);
				object->volume_group = volume_group;
				object->max_radius = 1e8;
				object->volume_group_owned = true;
				exclusionGridAddObject(grid, object, 1e8, false);
			}
			FOR_EACH_END;
			eaDestroy(&platforms);

			found_platforms = true;
		}

		eaPush(&roomInfo->platform_grids, grid);
	}

	if (!found_platforms && !roomInfo->footprint_buf)
	{
		stashIntAddPointer(g_pRoomInfos, room_group->name_uid, NULL, true);
		SAFE_FREE(roomInfo);
		return NULL;
	}

	return roomInfo;
}

void ugcRoomFreeRoomInfo( UGCRoomInfo* roomInfo )
{
	if( !roomInfo ) {
		return;
	}
	
	free( roomInfo->footprint_buf );
	eaDestroyStruct( &roomInfo->doors, parse_UGCRoomDoorInfo );
	eaDestroyEx( &roomInfo->platform_grids, exclusionGridFree );
	eaDestroyStruct( &roomInfo->details, parse_UGCRoomDetailDef );
	eaDestroyStruct( &roomInfo->populates, parse_UGCRoomPopulateDef );
	eaiDestroy( &roomInfo->uLevelGroupIDs );
	
	free( roomInfo );
}


////////////////////////////////////////////////////////////////
// Room Doors
////////////////////////////////////////////////////////////////

int ugcRoomDoorGetTypeIDForResourceInfo(ResourceInfo* info)
{
	char **tag_list = NULL;
	if (!info)
		return -1;

	DivideString(info->resourceTags, ",", &tag_list, DIVIDESTRING_POSTPROCESS_STRIP_WHITESPACE | DIVIDESTRING_POSTPROCESS_DONT_PUSH_EMPTY_STRINGS);

	FOR_EACH_IN_EARRAY(tag_list, char, tag)
	{
		if (strStartsWith(tag, "doortype_"))
		{
			int id = ugcRoomGetDoorTypeID(tag);
			eaDestroyEx( &tag_list, NULL );
			return id;
		}
	}
	FOR_EACH_END;

	eaDestroyEx(&tag_list, NULL);
	return -1;
}

int ugcRoomDoorGetTypeID(UGCComponent *door_component, int* eaValidTypes)
{
	if( door_component->iObjectLibraryId ) {
		return ugcRoomDoorGetTypeIDForResourceInfo( ugcResourceGetInfoInt( "ObjectLibrary", door_component->iObjectLibraryId ));
	} else if( door_component->bIsDoorExplicitDefault ) {
		return -1;
	} else {
		if( eaiSize( &eaValidTypes ) > 0 ) {
			int idAccum = eaValidTypes[ 0 ];
			int it;
			for( it = 1; it != eaiSize( &eaValidTypes ); ++it ) {
				idAccum = MAX( idAccum, eaValidTypes[ it ]);
			}
			return idAccum;
		} else {
			return -1;
		}
	}
}

void ugcRoomGetDoorLocalPos(UGCRoomInfo *room_buffer, int door_idx, Vec3 out_local_pos)
{
	UGCRoomDoorInfo *door = room_buffer->doors[door_idx];
	out_local_pos[0] = door->pos[0] * UGC_ROOM_GRID;
	out_local_pos[1] = 0;
	out_local_pos[2] = door->pos[2] * UGC_ROOM_GRID;
}

F32 ugcRoomGetDoorLocalRot(UGCRoomInfo *room_buffer, int door_idx)
{
	UGCRoomDoorInfo *door = room_buffer->doors[door_idx];
	return DEG(door->rot) - 90;
}

// Returns distance to nearest door (local_pos in the component's relative coordinates)
F32 ugcRoomGetNearestDoor(const UGCComponent *component, UGCRoomInfo *room_buffer, Vec2 local_pos, int *out_door_idx)
{
	int irot = ROT_TO_QUADRANT(RAD(component->sPlacement.vRotPYR[1]));
	F32 min_dist = 1e8;
	*out_door_idx = -1;
	// Check for clicks on doors
	FOR_EACH_IN_EARRAY(room_buffer->doors, UGCRoomDoorInfo, door)
	{
		F32 out_x, out_y, dist;

		out_x = door->pos[0] * UGC_ROOM_GRID;
		out_y = door->pos[2] * UGC_ROOM_GRID;

		dist = sqrtf(SQR(out_x-local_pos[0]) + SQR(out_y-local_pos[1]));

		if (dist < min_dist)
		{
			min_dist = dist;
			*out_door_idx = FOR_EACH_IDX(room_buffer->doors, door);
		}
	}
	FOR_EACH_END;
	return min_dist;
}

// Returns distance to nearest valid door (local_pos in the component's relative coordinates)
F32 ugcRoomGetNearestValidDoor(UGCProjectData *ugcProj, UGCComponent *room_component, UGCComponent *door_component, UGCRoomInfo *room_buffer, Vec2 local_pos, int *out_door_idx)
{
	int irot = ROT_TO_QUADRANT(RAD(door_component->sPlacement.vRotPYR[1]));
	F32 min_dist = 1e8;
	*out_door_idx = -1;
	// Check for clicks on doors
	FOR_EACH_IN_EARRAY(room_buffer->doors, UGCRoomDoorInfo, door)
	{
		F32 out_x, out_y, dist;

		out_x = door->pos[0] * UGC_ROOM_GRID;
		out_y = door->pos[2] * UGC_ROOM_GRID;

		dist = sqrtf(SQR(out_x-local_pos[0]) + SQR(out_y-local_pos[1]));

		if (dist < min_dist)
		{
			UGCComponent *other_door_component = NULL;
			int door_idx = FOR_EACH_IDX(room_buffer->doors, door);
			UGCDoorSlotState door_state = ugcRoomGetDoorSlotState(ugcProj->components, room_component, door_idx, &other_door_component, NULL, NULL, NULL);
			if (door_state == UGC_DOOR_SLOT_EMPTY
				|| (door_state == UGC_DOOR_SLOT_OCCUPIED && other_door_component == door_component))
			{
				min_dist = dist;
				*out_door_idx = door_idx;
			}
		}
	}
	FOR_EACH_END;
	return min_dist;
}

bool ugcRoomIsDoorConnected(const UGCComponent *room_component, UGCComponent *door_component, int *out_door_idx)
{
	Vec3 local_pos, door_pos;
	UGCRoomInfo *room_buffer;

	if (door_component->eType != UGC_COMPONENT_TYPE_ROOM_DOOR &&
		door_component->eType != UGC_COMPONENT_TYPE_FAKE_DOOR)
	{
		return false;
	}

	if (door_component->sPlacement.uRoomID != UGC_TOPLEVEL_ROOM_ID)
	{
		return false; // Unplaced door
	}
	if( stricmp( door_component->sPlacement.pcMapName, room_component->sPlacement.pcMapName ) != 0 ) {
		return false;
	}

	if (door_component->uParentID == room_component->uID)
	{
		if (out_door_idx)
			*out_door_idx = door_component->iRoomDoorID;
		return true;
	}

	room_buffer = ugcRoomGetRoomInfo(room_component->iObjectLibraryId);
	if (!room_buffer || !room_buffer->doors)
		return false;

	// Calculate local position in room
	{
		Vec3 rel_pos;
		Mat3 rot_inv_matrix;
		createMat3DegYPR(rot_inv_matrix, room_component->sPlacement.vRotPYR);
		transposeMat3(rot_inv_matrix);

		subVec3(door_component->sPlacement.vPos, room_component->sPlacement.vPos, rel_pos);
		mulVecMat3(rel_pos, rot_inv_matrix, local_pos);
	}

	// Does one of our door positions match?
	FOR_EACH_IN_EARRAY(room_buffer->doors, UGCRoomDoorInfo, door)
	{
		setVec3(door_pos, door->pos[0] * UGC_ROOM_GRID, 0, door->pos[2] * UGC_ROOM_GRID);
		if (distance3(door_pos, local_pos) < 1.f)
		{
			if (out_door_idx)
				*out_door_idx = FOR_EACH_IDX(room_buffer->doors, door);
			return true; // TomY TODO - check rotation as well!
		}
	}
	FOR_EACH_END;

	return false;
}

UGCDoorSlotState ugcRoomGetDoorSlotState(
		UGCComponentList *components, const UGCComponent *room_component, int door_id, 
		UGCComponent **out_door_component, int **out_door_types,
		UGCComponent **out_room_component, int *out_door_id)
{
	UGCRoomInfo *room_buffer;
	Vec3 door_world_pos;
	UGCComponent *valid_door = NULL;
	UGCDoorSlotState state = UGC_DOOR_SLOT_EMPTY;

	FOR_EACH_IN_EARRAY(components->eaComponents, UGCComponent, door_component)
	{
		int door_idx;
		if (ugcRoomIsDoorConnected(room_component, door_component, &door_idx) &&
			door_idx == door_id)
		{
			if (valid_door && door_component != valid_door)
			{
				// More than one door at this location!
				state = UGC_DOOR_SLOT_OCCUPIED_MULTIPLE;
				break;
			}
			if (door_component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR
				|| door_component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR)
			{
				state = UGC_DOOR_SLOT_OCCUPIED;
				valid_door = door_component;
			}
		}
	}
	FOR_EACH_END;

	if(   (state == UGC_DOOR_SLOT_OCCUPIED || state == UGC_DOOR_SLOT_OCCUPIED_MULTIPLE)
		  && out_door_component ) {
		*out_door_component = valid_door;
	}

	room_buffer = ugcRoomGetRoomInfo(room_component->iObjectLibraryId);

	{
		Vec3 door_local_pos;
		ugcRoomGetDoorLocalPos(room_buffer, door_id, door_local_pos);
		ugcRoomConvertLocalToWorld(room_component, door_local_pos, door_world_pos);
	}

	UGC_FOR_EACH_COMPONENT_OF_TYPE(components, UGC_COMPONENT_TYPE_ROOM, other_room_component)
	{
		if (other_room_component != room_component &&
			resNamespaceBaseNameEq(other_room_component->sPlacement.pcMapName, room_component->sPlacement.pcMapName))
		{
			int out_door_idx;
			F32 dist;
			Vec3 other_local_pos;
			UGCRoomInfo *other_room_buffer = ugcRoomGetRoomInfo(other_room_component->iObjectLibraryId);
			if( !other_room_buffer ) {
				continue;
			}

			ugcRoomConvertWorldToLocal(other_room_component, door_world_pos, other_local_pos);
			other_local_pos[1] = other_local_pos[2];
			dist = ugcRoomGetNearestDoor(other_room_component, other_room_buffer, other_local_pos, &out_door_idx);
			if (dist < 1.f)
			{
				if (out_door_types)
				{
					int type_idx;
					bool room_has_generic = false;
					bool other_room_has_generic = false;
					for (type_idx = 0; type_idx < eaiSize(&other_room_buffer->doors[out_door_idx]->eaiDoorTypeIDs); type_idx++)
					{
						int door_type = other_room_buffer->doors[out_door_idx]->eaiDoorTypeIDs[type_idx];
						if (door_type == room_buffer->doors[door_id]->eaiDoorTypeIDs[0]) // Compare to "primary" door type
						{
							eaiPush(out_door_types, door_type);
						}
						if (door_type == 0)
							other_room_has_generic = true;
					}
					for (type_idx = 0; type_idx < eaiSize(&room_buffer->doors[door_id]->eaiDoorTypeIDs); type_idx++)
					{
						int door_type = room_buffer->doors[door_id]->eaiDoorTypeIDs[type_idx];
						if (door_type == other_room_buffer->doors[out_door_idx]->eaiDoorTypeIDs[0]) // Compare to "primary" door type
						{
							eaiPush(out_door_types, door_type);
						}
						if (door_type == 0)
							room_has_generic = true;
					}
					if (other_room_has_generic && room_has_generic)
						eaiPush(out_door_types, 0);
				}
				if (out_room_component)
					*out_room_component = other_room_component;
				if (out_door_id)
					*out_door_id = out_door_idx;
				return state;
			}
		}
	}
	UGC_FOR_EACH_COMPONENT_END;

	if (out_door_types)
	{
		int type_idx_1;
		for (type_idx_1 = 0; type_idx_1 < eaiSize(&room_buffer->doors[door_id]->eaiDoorTypeIDs); type_idx_1++)
			eaiPush(out_door_types, room_buffer->doors[door_id]->eaiDoorTypeIDs[type_idx_1]);
	}

	if (out_room_component)
		*out_room_component = NULL;
	if (out_door_id)
		*out_door_id = 0;
	return state;
}

////////////////////////////////////////////////////////////////
// Utility Functions
////////////////////////////////////////////////////////////////

void ugcRoomRotateAndFlipPoint(IVec2 in, int irot, IVec2 out)
{
	if (irot == 0 || irot == 2)
	{
		out[0] = in[0];
		out[1] = -in[1];
	}
	else
	{
		out[0] = in[1];
		out[1] = 1+in[0];
	}
	if (irot > 1)
	{
		out[0] = -out[0]-1;
		out[1] = -out[1];
	}
	else
	{
		out[1] -= 1;
	}
}

void ugcRoomReverseRotateAndFlipPoint(IVec2 in, int irot, IVec2 out)
{
	IVec2 tmp;
	if (irot > 1)
	{
		tmp[0] = -in[0]-1;
		tmp[1] = -in[1];
	}
	else
	{
		tmp[0] = in[0];
		tmp[1] = in[1] + 1;
	}
	if (irot == 0 || irot == 2)
	{
		out[0] = tmp[0];
		out[1] = -tmp[1];
	}
	else
	{
		out[0] = tmp[1]-1;
		out[1] = tmp[0];
	}
}

void ugcRoomRotateBounds(IVec2 min, IVec2 max, int irot, IVec2 out_min, IVec2 out_max)
{
	int i;
	IVec2 corner, out_pt;
	
	setVec2(corner, min[0], min[1]);
	ugcRoomRotateAndFlipPoint(corner, irot, out_pt);
	copyVec2(out_pt, out_min);
	copyVec2(out_pt, out_max);

	for (i = 1; i < 4;  i++)
	{
		setVec2(corner, (i % 2) ? max[0] : min[0], (i >> 1) ? max[1] : min[1]);
		ugcRoomRotateAndFlipPoint(corner, irot, out_pt);
		out_min[0] = MIN(out_min[0], out_pt[0]);
		out_max[0] = MAX(out_max[0], out_pt[0]);
		out_min[1] = MIN(out_min[1], out_pt[1]);
		out_max[1] = MAX(out_max[1], out_pt[1]);
	}
}

void ugcRoomConvertWorldToLocal(const UGCComponent *room_component, const Vec3 world_pos, Vec3 out_local_pos)
{
	Mat3 rot_inv_matrix;
	Vec3 rel_pos;

	subVec3(world_pos, room_component->sPlacement.vPos, rel_pos);
	createMat3DegYPR(rot_inv_matrix, room_component->sPlacement.vRotPYR);
	transposeMat3(rot_inv_matrix);
	mulVecMat3(rel_pos, rot_inv_matrix, out_local_pos);
}

void ugcRoomConvertLocalToWorld(const UGCComponent *room_component, Vec3 local_pos, Vec3 out_world_pos)
{
	Mat3 rot_matrix;
	Vec3 rel_pos;

	createMat3DegYPR(rot_matrix, room_component->sPlacement.vRotPYR);
	mulVecMat3(local_pos, rot_matrix, rel_pos);

	addVec3(rel_pos, room_component->sPlacement.vPos, out_world_pos);
}

////////////////////////////////////////////////////////////////
// Room-room collision
////////////////////////////////////////////////////////////////

void ugcRoomGetColliderList(const UGCComponentList *components, U32 *drag_ids, F32 *drag_positions,
							const UGCComponent *component, const Vec3 component_pos, const Vec3 component_rot, UGCRoomInfo *room_buffer,
							bool one_way_check, UGCRoomInfo ***out_buffers, S32 **out_offsets)
{
	IVec2 component_min, component_max;
	int irot = ROT_TO_QUADRANT(RAD(component_rot[1]));
	ugcRoomRotateBounds(room_buffer->footprint_min, room_buffer->footprint_max, irot, component_min, component_max);

	UGC_FOR_EACH_COMPONENT_OF_TYPE(components, UGC_COMPONENT_TYPE_ROOM, coll_component)
	{
		int idx;
		S32 x_offset, z_offset;
		IVec2 coll_min, coll_max;
		Vec3 coll_pos;
		Vec3 coll_rot;
		UGCRoomInfo *coll_buffer;
		int irot2;

		if (coll_component == component)
			continue;
		if (!ugcComponentIsOnMap(coll_component, component->sPlacement.pcMapName, false))
			continue;
		if (one_way_check && coll_component->uID < component->uID)
			continue;

		coll_buffer = ugcRoomGetRoomInfo(coll_component->iObjectLibraryId);
		if( coll_buffer ) {
			if ((idx = eaiFind(&drag_ids, coll_component->uID)) > -1)
			{
				copyVec3(&drag_positions[idx*4], coll_pos);
				copyVec3(&drag_positions[idx*4+3], coll_rot);
			}
			else
			{
				copyVec3(coll_component->sPlacement.vPos, coll_pos);
				copyVec3(coll_component->sPlacement.vRotPYR, coll_rot);
			}
			irot2 = ROT_TO_QUADRANT(RAD(coll_rot[1]));

			x_offset = (coll_pos[0]-component_pos[0]) / UGC_ROOM_GRID;
			z_offset = (coll_pos[2]-component_pos[2]) / UGC_ROOM_GRID;
			ugcRoomRotateBounds(coll_buffer->footprint_min, coll_buffer->footprint_max, irot2, coll_min, coll_max);

			if ((coll_min[0]+x_offset) <= component_max[0] &&
				(coll_max[0]+x_offset) >= component_min[0] &&
				(coll_min[1]-z_offset) <= component_max[1] &&
				(coll_max[1]-z_offset) >= component_min[1])
			{
				eaPush(out_buffers, coll_buffer);
				eaiPush(out_offsets, x_offset);
				eaiPush(out_offsets, 0);
				eaiPush(out_offsets, z_offset);
				eaiPush(out_offsets, irot2);
			}
		}
	}
	UGC_FOR_EACH_COMPONENT_END;
}

bool ugcRoomCheckCollisionPoint(IVec2 rotated_pt, int irot, UGCRoomInfo **coll_buffers, S32 *coll_offsets)
{
	int i;
	for (i = 0; i < eaSize(&coll_buffers); i++)
	{
		UGCRoomInfo *coll_buffer = coll_buffers[i];
		IVec2 local_pt;
		IVec2 orig_pt;
		local_pt[0] = rotated_pt[0] - coll_offsets[i*4];
		local_pt[1] = rotated_pt[1] + coll_offsets[i*4+2];
		ugcRoomReverseRotateAndFlipPoint(local_pt, coll_offsets[i*4+3], orig_pt);
		if (orig_pt[0] >= coll_buffer->footprint_min[0] &&
				orig_pt[0] <= coll_buffer->footprint_max[0] &&
				orig_pt[1] >= coll_buffer->footprint_min[1] &&
				orig_pt[1] <= coll_buffer->footprint_max[1])
		{
			S32 coll_width = coll_buffer->footprint_max[0]+1-coll_buffer->footprint_min[0];
			orig_pt[0] -= coll_buffer->footprint_min[0];
			orig_pt[1] -= coll_buffer->footprint_min[1];
			if (coll_buffer->footprint_buf[orig_pt[0]+orig_pt[1]*coll_width] != 0)
			{
				return true;
			}
		}
	}
	return false;
}

bool ugcRoomCheckCollision(const UGCComponentList *components, const UGCComponent *component, const Vec3 component_pos, const Vec3 component_rot, bool one_way_check)
{
	UGCRoomInfo *room_buffer = ugcRoomGetRoomInfo(component->iObjectLibraryId);
	UGCRoomInfo **coll_buffers = NULL;
	int irot = ROT_TO_QUADRANT(RAD(component_rot[1]));
	S32 *coll_offsets = NULL;

	if (!room_buffer)
		return false;

	ugcRoomGetColliderList(components, NULL, NULL,
						   component, component_pos, component_rot, room_buffer,
						   one_way_check, &coll_buffers, &coll_offsets);

	if (eaSize(&coll_buffers) > 0)
	{
		int px, py;
		int width = room_buffer->footprint_max[0]+1-room_buffer->footprint_min[0];
		int height = room_buffer->footprint_max[1]+1-room_buffer->footprint_min[1];

		for (py = 0; py < height; py++)
			for (px = 0; px < width; px++)
				if (room_buffer->footprint_buf[px+py*width] != 0)
				{
					IVec2 in_pt = { px+room_buffer->footprint_min[0], py+room_buffer->footprint_min[1] };
					IVec2 rotated_pt;
					ugcRoomRotateAndFlipPoint(in_pt, irot, rotated_pt);
					if (ugcRoomCheckCollisionPoint(rotated_pt, irot, coll_buffers, coll_offsets))
					{
						eaDestroy(&coll_buffers);
						eaiDestroy(&coll_offsets);
						return true;
					}
				}
	}

	eaDestroy(&coll_buffers);
	eaiDestroy(&coll_offsets);
	return false;
}

void ugcRoomGetWorldBoundingBox( UGCComponent* room, CBox* out_roomBox )
{
	UGCRoomInfo* roomInfo = ugcRoomGetRoomInfo( room->iObjectLibraryId );
	Vec3 min;
	Vec3 max;

	setVec3( min, roomInfo->footprint_min[ 0 ] * UGC_ROOM_GRID, 0, roomInfo->footprint_min[ 1 ] * UGC_ROOM_GRID );
	setVec3( max, roomInfo->footprint_max[ 0 ] * UGC_ROOM_GRID, 0, roomInfo->footprint_max[ 1 ] * UGC_ROOM_GRID );
	ugcRoomConvertLocalToWorld( room, min, min );
	ugcRoomConvertLocalToWorld( room, max, max );

	out_roomBox->lx = MIN( min[ 0 ] , max[ 0 ]);
	out_roomBox->ly = MIN( min[ 2 ] , max[ 2 ]);
	out_roomBox->hx = MAX( min[ 0 ] , max[ 0 ]);
	out_roomBox->hy = MAX( min[ 2 ] , max[ 2 ]);
}

