
#pragma once

typedef struct CBox CBox;
typedef struct GroupDef GroupDef;
typedef struct HeightMapExcludeGrid HeightMapExcludeGrid;
typedef struct ResourceInfo ResourceInfo;
typedef struct UGCComponent UGCComponent;
typedef struct UGCComponentList UGCComponentList;
typedef struct UGCProjectData UGCProjectData;
typedef struct UGCRoomInfo UGCRoomInfo;

// The minimum space understood by the room system.  All room objects
// (rooms, doors, etc.) have to be placed on this grid.
#define UGC_ROOM_GRID 10.f

// The minimum space per kit.  Doors-anywhere rooms can only be placed
// in increments of this.  This is different from the room grid,
// because the room grid has to have doors on the kit.
//
// Likely, this will always need to be 2*UGC_ROOM_GRID.
#define UGC_KIT_GRID 20.f

#define ROT_TO_QUADRANT(rot) (floor(fmodf((rot+2*PI)/(0.5f*PI)+0.001f, 4.0)))

typedef enum UGCDoorSlotState
{
	// No door here 
	UGC_DOOR_SLOT_EMPTY,

	// Door here, if it is a FAKE_DOOR, then not connected to another
	// room, if it is a ROOM_DOOR then this connects two rooms.
	UGC_DOOR_SLOT_OCCUPIED,

	// Multiple doors here, an error state
	UGC_DOOR_SLOT_OCCUPIED_MULTIPLE,
} UGCDoorSlotState;

// The total number of children a RoomDoor ObjectLibrary piece must
// have.
#define UGC_OBJLIB_ROOMDOOR_NUM_CHILDREN 3

// The child at this index will be used for
// UGC_COMPONENT_TYPE_FAKE_DOOR.
#define UGC_OBJLIB_ROOMDOOR_FAKE_CHILD 0

// The child at this index will be used for locked
// UGC_COMPONENT_TYPE_ROOM_DOOR.
#define UGC_OBJLIB_ROOMDOOR_LOCKED_CHILD 1

// The child at this index will be used for unlocked
// UGC_COMPONENT_TYPE_ROOM_DOOR.
#define UGC_OBJLIB_ROOMDOOR_UNLOCKED_CHILD 2

UGCRoomInfo* ugcRoomGetRoomInfo( int objectLibraryID );

// Collision checking

void ugcRoomGetColliderList(const UGCComponentList *components, U32 *drag_ids, F32 *drag_positions,
							const UGCComponent *component, const Vec3 component_pos, const Vec3 component_rot, UGCRoomInfo *room_info,
							bool one_way_check, UGCRoomInfo ***out_infos, S32 **out_offsets);

bool ugcRoomCheckCollisionPoint(IVec2 rotated_pt, int irot, UGCRoomInfo **coll_infos, S32 *coll_offsets);

bool ugcRoomCheckCollision(const UGCComponentList *components, const UGCComponent *component, const Vec3 component_pos, const Vec3 component_rot, bool one_way_check);

// Room doors

int ugcRoomDoorGetTypeIDForResourceInfo(ResourceInfo* info);
int ugcRoomDoorGetTypeID(UGCComponent *door_component, int* eaValidTypes);
void ugcRoomGetDoorLocalPos(UGCRoomInfo *room_info, int door_idx, Vec3 out_local_pos);
F32 ugcRoomGetDoorLocalRot(UGCRoomInfo *room_info, int door_idx);
F32 ugcRoomGetNearestDoor(const UGCComponent *component, UGCRoomInfo *room_info, Vec2 local_pos, int *out_door_idx);
F32 ugcRoomGetNearestValidDoor(UGCProjectData *ugcProj, UGCComponent *room_component, UGCComponent *door_component, UGCRoomInfo *room_buffer, Vec2 local_pos, int *out_door_idx);
bool ugcRoomIsDoorConnected(const UGCComponent *room_component, UGCComponent *door_component, int *out_door_idx);
UGCDoorSlotState ugcRoomGetDoorSlotState(UGCComponentList *components, const UGCComponent *room_component, int door_id, UGCComponent **out_door_component, int **out_door_types, UGCComponent **out_room_component, int *out_door_id);

// Utilities

UGCComponent *ugcComponentGetRoomParent(const UGCComponentList *list, const UGCComponent *component);
void ugcRoomRotateAndFlipPoint(IVec2 in, int irot, IVec2 out);
void ugcRoomReverseRotateAndFlipPoint(IVec2 in, int irot, IVec2 out);
void ugcRoomRotateBounds(IVec2 min, IVec2 max, int irot, IVec2 out_min, IVec2 out_max);
F32 ugcLayoutGetRoomLevelDelta(UGCComponent *component, S32 editing_level);
void ugcRoomConvertWorldToLocal(const UGCComponent *room_component, const Vec3 world_pos, Vec3 out_local_pos);
void ugcRoomConvertLocalToWorld(const UGCComponent *room_component, Vec3 local_pos, Vec3 out_world_pos);

void ugcRoomGetWorldBoundingBox( UGCComponent* room, CBox* out_roomBox );
