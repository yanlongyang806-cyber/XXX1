//// UGC user-facing resources
////
//// UGC has a collection of resources that it uses to create
//// underlying missions, ZoneMaps, costumes, etc.  This is where you
//// can place all the UGC-specific dictionaries. These structures should
//// only be used in the UGC editor and to generate the underlying Genesis
//// structures; they are never used in generation or loaded directly during
//// the game.

#pragma once
GCC_SYSTEM

#include "ReferenceSystem.h"
#include "WorldLibEnums.h"
#include "wlGroupPropertyStructs.h"
#include "UGCProjectCommon.h"

typedef struct AIAnimList AIAnimList;
typedef struct AllegianceDef AllegianceDef;
typedef struct FSM FSM;
typedef struct InteractionDef InteractionDef;
typedef struct PlayerCostume PlayerCostume;
typedef struct UGCCheckedAttrib UGCCheckedAttrib;
typedef struct UGCComponent UGCComponent;
typedef struct UGCDialogTreeBlock UGCDialogTreeBlock;
typedef struct UGCFSM UGCFSM;
typedef struct UGCGenesisInterior UGCGenesisInterior;
typedef struct UGCGenesisPrefab UGCGenesisPrefab;
typedef struct UGCGenesisSpace UGCGenesisSpace;
typedef struct UGCMissionObjective UGCMissionObjective;
typedef struct UGCUnitializedMap UGCUnitializedMap;
typedef struct UGCWhen UGCWhen;
typedef struct WorldUGCProperties WorldUGCProperties;
typedef struct WorldVariableDef WorldVariableDef;

typedef U32 ContainerID;
typedef enum CharClassTypes CharClassTypes;

AUTO_ENUM;
typedef enum UGCInteractDuration
{
	UGCDURATION_INSTANT,	ENAMES("UGC.DurationInstant")
	UGCDURATION_SHORT,		ENAMES("UGC.DurationShort")
	UGCDURATION_MEDIUM,		ENAMES("UGC.DurationMedium")
	UGCDURATION_LONG,		ENAMES("UGC.DurationLong")
} UGCInteractDuration;
extern StaticDefineInt UGCInteractDurationEnum[];

AUTO_ENUM;
typedef enum UGCZOrderSort
{
	UGCZORDERSORT_LOWEST = -2,	ENAMES("UGC.ZOrderSortLowest")
	UGCZORDERSORT_LOW = -1,		ENAMES("UGC.ZOrderSortLow")
	UGCZORDERSORT_NORMAL = 0,	ENAMES("UGC.ZOrderSortNormal")
	UGCZORDERSORT_HIGH = 1,		ENAMES("UGC.ZOrderSortHigh")
	UGCZORDERSORT_HIGHEST = 2,	ENAMES("UGC.ZOrderSortHighest")
} UGCZOrderSort;
extern StaticDefineInt UGCZOrderSortEnum[];

AUTO_ENUM;
typedef enum UGCComponentType
{
	// Layout - Common
	UGC_COMPONENT_TYPE_SPAWN,
	UGC_COMPONENT_TYPE_RESPAWN,
	UGC_COMPONENT_TYPE_WHOLE_MAP,
	UGC_COMPONENT_TYPE_CONTACT,
	UGC_COMPONENT_TYPE_KILL,
	UGC_COMPONENT_TYPE_ACTOR,			// Child component of KILL
	UGC_COMPONENT_TYPE_COMBAT_JOB,
	UGC_COMPONENT_TYPE_DESTRUCTIBLE,
	UGC_COMPONENT_TYPE_PATROL_POINT,
	UGC_COMPONENT_TYPE_TRAP,
	UGC_COMPONENT_TYPE_TRAP_TARGET,		// Child component of TRAP
	UGC_COMPONENT_TYPE_TRAP_TRIGGER,	// Child component of TRAP
	UGC_COMPONENT_TYPE_TRAP_EMITTER,	// Child component of TRAP
	UGC_COMPONENT_TYPE_TELEPORTER,
	UGC_COMPONENT_TYPE_TELEPORTER_PART, // Child component of TELEPORTER
	UGC_COMPONENT_TYPE_CLUSTER,
	UGC_COMPONENT_TYPE_CLUSTER_PART,	// Child component of CLUSTER
	UGC_COMPONENT_TYPE_REWARD_BOX,		// Treasure chest
	UGC_COMPONENT_TYPE_SOUND,
	
	// Layout - Interior
	UGC_COMPONENT_TYPE_ROOM,
	UGC_COMPONENT_TYPE_ROOM_DOOR,			ENAMES( ROOM_DOOR ROOM_OPEN_DOOR )
	UGC_COMPONENT_TYPE_FAKE_DOOR,			ENAMES( FAKE_DOOR ROOM_CLOSED_DOOR )
	
	// Layout - Ground
	UGC_COMPONENT_TYPE_BUILDING_DEPRECATED,
	
	// Layout - Space
	UGC_COMPONENT_TYPE_PLANET,
	
	// External only
	UGC_COMPONENT_TYPE_EXTERNAL_DOOR,
	
	// Mission
	UGC_COMPONENT_TYPE_ROOM_MARKER,
	UGC_COMPONENT_TYPE_DIALOG_TREE,

	// Common (sorts to bottom)
	UGC_COMPONENT_TYPE_OBJECT,				ENAMES( OBJECT BUILDING )

	UGC_COMPONENT_TYPE_COUNT,				EIGNORE
} UGCComponentType;
extern StaticDefineInt UGCComponentTypeEnum[];

AUTO_STRUCT;
typedef struct UGCCheckedAttrib
{
	bool bNot;								AST(NAME("Not"))
	const char* astrSkillName;				AST(NAME("SkillName") NAME("Name") POOL_STRING)
	const char* astrItemName;				AST(NAME("ItemName") POOL_STRING)
} UGCCheckedAttrib;
extern ParseTable parse_UGCCheckedAttrib[];
#define TYPE_parse_UGCCheckedAttrib UGCCheckedAttrib

AUTO_STRUCT;
typedef struct UGCInteractProperties
{
	char *pcInteractText;					AST(NAME("InteractText"))
	char *pcInteractFailureText;			AST(NAME("InteractFailureText"))
	REF_TO(AIAnimList) hInteractAnim;		AST(NAME("InteractAnim"))
	UGCInteractDuration eInteractDuration;	AST(NAME("InteractDuration") DEFAULT(UGCDURATION_MEDIUM))

	UGCCheckedAttrib* succeedCheckedAttrib;	AST(NAME("SucceedCheckedAttrib"))
	bool bTakesItem;						AST(NAME("InteractTakesItem"))

	char *pcDropItemName;					AST(NAME("DropItem"))
} UGCInteractProperties;
extern ParseTable parse_UGCInteractProperties[];
#define TYPE_parse_UGCInteractProperties UGCInteractProperties

AUTO_STRUCT;
typedef struct UGCGenesisBackdropSkyOverride
{
	REF_TO(SkyInfo) hSkyOverride;					AST(NAME("SkyOverride"))
} UGCGenesisBackdropSkyOverride;
extern ParseTable parse_UGCGenesisBackdropSkyOverride[];
#define TYPE_parse_UGCGenesisBackdropSkyOverride UGCGenesisBackdropSkyOverride

AUTO_STRUCT;
typedef struct UGCGenesisBackdrop
{
	REF_TO(SkyInfo) hSkyBase;						AST(NAME("SkyBase") NAME("Backdrop"))
	UGCGenesisBackdropSkyOverride** eaSkyOverrides;	AST(NAME("SkyOverride"))
	char *strAmbientSoundOverride;					AST(NAME("AmbSoundOverride"))
} UGCGenesisBackdrop;
extern ParseTable parse_UGCGenesisBackdrop[];
#define TYPE_parse_UGCGenesisBackdrop UGCGenesisBackdrop

AUTO_STRUCT AST_IGNORE_STRUCT("UGCBackdrop");
typedef struct UGCGenesisSpace
{
	UGCGenesisBackdrop backdrop;					AST(EMBEDDED_FLAT NAME("Backdrop"))
} UGCGenesisSpace;
extern ParseTable parse_UGCGenesisSpace[];
#define TYPE_parse_UGCGenesisSpace UGCGenesisSpace

AUTO_STRUCT AST_IGNORE_STRUCT("UGCBackdrop");
typedef struct UGCGenesisPrefab
{
	UGCGenesisBackdrop backdrop;					AST(EMBEDDED_FLAT NAME("Backdrop"))

	const char *map_name;							AST(NAME("MapName") POOL_STRING)
	bool customizable;								AST(NAME("Customizable"))
} UGCGenesisPrefab;
extern ParseTable parse_UGCGenesisPrefab[];
#define TYPE_parse_UGCGenesisPrefab UGCGenesisPrefab

AUTO_STRUCT;
typedef struct UGCUnitializedMap
{
	UGCMapType eType;								AST(NAME("Type"))
} UGCUnitializedMap;
extern ParseTable parse_UGCUnitializedMap[];
#define TYPE_parse_UGCUnitializedMap UGCUnitializedMap

/// Map resource (can be Interior or Space)

// A map is a resource which ultimately generates a functional ZoneMap
// that a player can navigate.

AUTO_STRUCT AST_IGNORE_STRUCT(Created) AST_IGNORE_STRUCT(Saved);
typedef struct UGCMap
{
	const char *pcName;								AST(NAME("Name") POOL_STRING KEY)
	const char *pcFilename;							AST(CURRENTFILE NO_NETSEND)
	char *pcDisplayName;							AST(NAME("DisplayName"))
	char *strNotes;									AST(NAME("Notes"))

	UGCGenesisSpace *pSpace;						AST(NAME("Space"))
	UGCGenesisPrefab *pPrefab;						AST(NAME("Prefab"))
	UGCUnitializedMap *pUnitializedMap;				AST(NAME("UnitializedMap"))

	// Budget values calculated last validate
	int cacheComponentCount[UGC_COMPONENT_TYPE_COUNT]; NO_AST
} UGCMap;
extern ParseTable parse_UGCMap[];
#define TYPE_parse_UGCMap UGCMap

/// Project mission
///
/// Contains the entire mission for this project, expressed in a
/// series of objectives. These are broken up into player mission
/// objectives that are persisted on your character, and open
/// mission objectives that exist only on the map.

AUTO_ENUM;
typedef enum UGCDialogTreePromptActionStyle
{
	//see "Dialog Implementation Guidelines" on the wiki.
	//UGC_DIALOG_INT_COLORS in UGCDialogTreeEditor relies on this order; check there if changing.
	UGCDIALOG_STYLE_NORMAL,				ENAMES("UGC.DialogStyleNormal" "Normal")//normal, white
	UGCDIALOG_STYLE_MISSION_INFO,		ENAMES("UGC.DialogStyleMissionInfo" "MissionInfo")//(Yellow Text) - Indicates text which progresses a player towards a commitment.
	UGCDIALOG_STYLE_MISSION_OBJECTIVE,	ENAMES("UGC.DialogStyleMissionObjective" "AcceptMission")//(Orange Text) - Indicates text which commits a player to an action.
	//"visited" and "unavailable skill" should be handled automatically.
} UGCDialogTreePromptActionStyle;
extern StaticDefineInt UGCDialogTreePromptActionStyleEnum[];

AUTO_STRUCT;
typedef struct UGCDialogTreePromptAction {
	char *pcText;									AST(STRUCTPARAM NAME("Text"))
	
	U32 nextPromptID;								AST(NAME("NextPromptID"))
	bool bDismissAction;							AST(NAME("DismissAction"))
	UGCDialogTreePromptActionStyle style;			AST(NAME("Style"))

	// When this button becomes visible
	UGCWhen* pShowWhen;								AST(NAME("ShowWhen"))
	// When this button becomes hidden
	UGCWhen* pHideWhen;								AST(NAME("HideWhen"))
	// An attribute any player on the team must have for this path to
	// be avaliable.
	UGCCheckedAttrib* enabledCheckedAttrib;			AST(NAME("EnabledCheckedAttrib"))
} UGCDialogTreePromptAction;
extern ParseTable parse_UGCDialogTreePromptAction[];
#define TYPE_parse_UGCDialogTreePromptAction UGCDialogTreePromptAction

AUTO_ENUM;
typedef enum UGCDialogTreePromptType {
	UGCPROMPT_MISC,									ENAMES("UGC.PromptTypeMisc")
} UGCDialogTreePromptType;
extern StaticDefineInt UGCDialogTreePromptTypeEnum[];
AUTO_STRUCT AST_IGNORE("CameraPos") AST_IGNORE("PromptType");
typedef struct UGCDialogTreePrompt {
	U32 uid;										AST(STRUCTPARAM)

	char *pcPromptTitle;							AST(NAME("PromptTitle"))
	char *pcPromptBody;								AST(NAME("PromptBody"))
	UGCDialogTreePromptAction **eaActions;			AST(NAME("Action") ADDNAMES("PromptButton"))

	// One of these should be set
	char *pcPromptCostume;							AST(NAME("PromptCostume"))
	REF_TO(PetContactList) hPromptPetCostume;		AST(NAME("PromptPetCostume"))
	
	char *pcPromptStyle;							AST(NAME("PromptStyle"))

	Vec2 dialogEditorPos;							NO_AST
} UGCDialogTreePrompt;
extern ParseTable parse_UGCDialogTreePrompt[];
#define TYPE_parse_UGCDialogTreePrompt UGCDialogTreePrompt

// This data structure only exists for legacy purposes.  There should
// be exactly one pre UGCComponent of type DIALOG_TREE.
AUTO_STRUCT;
typedef struct UGCDialogTreeBlock {
	int blockIndex;									AST(NAME("BlockIndex"))
	
	UGCDialogTreePrompt initialPrompt;				AST(EMBEDDED_FLAT)
	UGCDialogTreePrompt** prompts;					AST(NAME("Prompt"))
	
	F32 editorX;									NO_AST
	F32 editorY;									NO_AST
} UGCDialogTreeBlock;
extern ParseTable parse_UGCDialogTreeBlock[]; 
#define TYPE_parse_UGCDialogTreeBlock UGCDialogTreeBlock

AUTO_STRUCT;
typedef struct UGCMissionMapLink
{
	// Source Map
	U32 uDoorComponentID;						AST(NAME("DoorComponent"))
	bool bDoorUsesMapLocation;					AST(NAME("DoorUsesMapLocation"))
	UGCMapLocation* pDoorMapLocation;			AST(NAME("DoorMapLocation"))

	// Destination Map
	U32 uSpawnComponentID;						AST(NAME("SpawnComponent"))
	char* strSpawnInternalMapName;				AST(NAME("SpawnInternalMapName"))

	// Dialog prompt (if apropriate)
	UGCDialogTreePrompt* pDialogPrompt;			AST(NAME("DialogBlock"))

    // Interact text (if pDialogPrompt and the default pDialogPrompt are both null)
	char* strInteractText;						AST(NAME("InteractText"))

	// Text to appear when the link would be active (only valid for project maps)
	char* strReturnText;						AST(NAME("ReturnText"))

	// Open mission name (only valid for project maps)
	char* strOpenMissionName;					AST(NAME("OpenMissionName"))
} UGCMissionMapLink;
extern ParseTable parse_UGCMissionMapLink[];
#define TYPE_parse_UGCMissionMapLink UGCMissionMapLink

AUTO_STRUCT AST_IGNORE(DisplayName) AST_IGNORE(JournalText);
typedef struct UGCMission
{
	const char* filename;					AST( NAME(FN) CURRENTFILE NO_NETSEND )
	const char* name;						AST( NAME(Name) KEY POOL_STRING )

	// Granting
	UGCDialogTreePrompt sGrantPrompt;		AST( NAME("GrantBlock") )
	char* strInitialMapName;				AST( NAME(InitialMapName) )
	char* strInitialSpawnPoint;				AST( NAME(InitialSpawnPoint) )

	UGCMissionObjective** objectives;		AST( NAME(Objective) )
	UGCMissionMapLink** map_links;			AST( NAME(MapLink) )
	UGCMissionMapLink* return_map_link;		AST( NAME(ReturnMapLink) )
} UGCMission;
extern ParseTable parse_UGCMission[];
#define TYPE_parse_UGCMission UGCMission

AUTO_ENUM;
typedef enum UGCMissionObjectiveType
{
	UGCOBJ_COMPLETE_COMPONENT,
	UGCOBJ_UNLOCK_DOOR,

	// Branching types
	UGCOBJ_ALL_OF,
	UGCOBJ_IN_ORDER,

	// Processed objectives
	UGCOBJ_TMOG_MAP_MISSION,		EIGNORE
	UGCOBJ_TMOG_REACH_INTERNAL_MAP,	EIGNORE
	UGCOBJ_TMOG_REACH_CRYPTIC_MAP,	EIGNORE
} UGCMissionObjectiveType;

AUTO_ENUM;
typedef enum UGCWaypointMode
{
	UGC_WAYPOINT_NONE,							ENAMES("UGC.WaypointNone" "None")
	UGC_WAYPOINT_AREA,							ENAMES("UGC.WaypointArea" "Area" "Automatic")
	UGC_WAYPOINT_POINTS,						ENAMES("UGC.WaypointPoints" "Points")
	//UGC_WAYPOINT_CUSTOM;						ENAMES("UGC.WaypointCustom" "Custom")
} UGCWaypointMode;
extern StaticDefineInt UGCWaypointModeEnum[];

/// An objective for a persistent mission
///
/// TODO: integrate in GenesisWhen?
AUTO_STRUCT AST_IGNORE(ParentID);
typedef struct UGCMissionObjective
{
	U32 id;										AST( NAME(ID) STRUCTPARAM )
	UGCMissionObjectiveType type;				AST( NAME(Type) )
	char* uiString;								AST( NAME(UIString) )
	UGCWaypointMode waypointMode;				AST( NAME(WaypointMode) )

	char* strComponentInternalMapName;			AST( NAME(ComponentMapName) )
	bool bComponentIsExternal;					AST( NAME(ComponentIsExternal) )
	
	U32 componentID;							AST( NAME(ComponentID) )
	U32 *extraComponentIDs;						AST( NAME(ExtraComponentID) )
	UGCMissionObjective** eaChildren;			AST( NAME(Children) )
	char* successFloaterText;					AST( NAME(SuccessFloaterText) )

	// ****
	UGCInteractProperties sInteractProps;		AST( EMBEDDED_FLAT )

	// Tmog-only data
	const char* astrMapName;					NO_AST

	// Editor infrastructure
	float editorX;								NO_AST
	float editorY;								NO_AST
} UGCMissionObjective;
extern ParseTable parse_UGCMissionObjective[];
#define TYPE_parse_UGCMissionObjective UGCMissionObjective

//// Component resources

// A component is any placeable object such as a spawn point, encounter,
// clickable, room marker, etc. The actual resource is the component list,
// and there is only one per project.

AUTO_STRUCT AST_FIXUPFUNC( fixupUGCComponentList );
typedef struct UGCComponentList
{
	const char *pcName;								AST(NAME("Name") POOL_STRING KEY)
	const char *pcFilename;							AST(CURRENTFILE NO_NETSEND)

	// DO NOT modify this earray outside of the ugcComponentOp API!
	//
	// You probably want to call one of ugcComponentOpCreate,
	// ugcComponentOpDelete, etc.
	//
	// (Constness prevents external modification.)
	UGCComponent *const* const eaComponents;		AST(NAME("Component"))

	// A stash table version of the above.  This is kept in sync via the API.
	StashTable stComponentsById;					NO_AST
} UGCComponentList;
extern ParseTable parse_UGCComponentList[];
#define TYPE_parse_UGCComponentList UGCComponentList

///////////////////////////////////
// There's a little bit of mayhem with the height snap enum. DEFAULT has various behaviours depending on the type
//  of map. In particular, though, for prefab maps, we used to raycast for terrain but only from a height of 5000
//  to a height of zero. This fails for any maps that go below zero. There was a 'fix' made that extended this
//  to cast down to -5000.0, but of course Star Trek has projects which are dependent on the old behaviour.
//  SNAP_TERRAIN and SNAP_WORLDGEO wre added to deal with this. SNAP_DEFAULT will continue to cast only to 0.0. Objects with
//  SNAP_TERRAIN or SNAP_WORLDGEO will cast to -5000.0. Any old projects will continue their old behaviour. New objects will
//  default to SNAP_TERRAIN in their selector so they will 'behave'. 

AUTO_ENUM;
typedef enum UGCComponentHeightSnap
{
	COMPONENT_HEIGHT_SNAP_LEGACY,		// This is essntially a legacy mode that had different behaviours depending on the map type. We now explicitly
		ENAMES(LEGACY DEFAULT)			//			use the other types.
										// For prefab interior and exterior maps, snap to platform parent, then grid platforms,
										//			then terrain (at legacy distance), then use InternalSpawnPoint Y. Replaced with SNAP_TERRAIN
										// For interior maps, snap to room floor which should use the TERRAIN collision bit. Essentially follow the same
										//			rules as above. This should never actually have been used since STO didn't have custom interiors.
										//			Neverwinter made DEFAULT require a parent object and used it for snapping to 'levels' of a room.
										//			This functionality has been replaced with ROOM_PARENTED
										// For space maps, snap to a specified platform parent, then to the InternalSpawnPoint Y. Now use SNAP_ABSOLUTE directly
	COMPONENT_HEIGHT_SNAP_ABSOLUTE,		// Always snap to the InternalSpawnPoint Y.
	COMPONENT_HEIGHT_SNAP_GEOMETRY,		// Ignored for custom and space (Uses InternalSpawnPointY). Snap to highest geometry using legacy distance. Replaced with
										//			SNAP_WORLDGEO
	COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE, // Snap to the height of the room's pivot. (Actually, add the room height to the offset position and use ABSOLUTE,
										//		not sure if it's the same thing) 
	COMPONENT_HEIGHT_SNAP_ROOM_PARENTED, // Snap to a specified room platform. If there's no specified platform, use InternalSpawnPoint Y
	COMPONENT_HEIGHT_SNAP_TERRAIN,		// Snap to prefab grid platforms, then terrain and not world geometry. Then use InternalSpawnPoint Y
										//		Replacement for Default on exterior maps with increased raycast distance
	COMPONENT_HEIGHT_SNAP_WORLDGEO,		// Snap to prefab grid platforms, then world geometry and not terrain. Then use InternalSpawnPoint Y
										//		Replacement for GEOMETRY on exterior maps with increased raycast distance

	COMPONENT_HEIGHT_SNAP_UNSPECIFIED,	// Snap is not specified. Such as new or unplaced components.
	
} UGCComponentHeightSnap;
extern StaticDefineInt UGCComponentHeightSnapEnum[];

///////////////////////////////////


AUTO_STRUCT;
typedef struct UGCComponentPlacement
{
	bool bIsExternalPlacement;						AST(NAME("ExternalPlacement"))

	// Internal placement
	char *pcMapName;								AST(NAME("MapName"))
	U32 uRoomID;									AST(NAME("RoomID"))
	Vec3 vPos;										AST(NAME("Position"))
	union
	{
		Vec3 vRotPYR;								AST(NAME("RotationPYR"))
		float fRotLegacy[3];						AST(REDUNDANTNAME INDEX(0, Pitch) INDEX(1, Rotation) INDEX(2, Roll))
	};

	UGCComponentHeightSnap eSnap;					AST(NAME("Snap"))
	// Only valid if Snap is COMPONENT_HEIGHT_SNAP_TERRAIN or COMPONENT_HEIGHT_SNAP_WORLDGEO.
	// If Terrain is not actually hit, such as when a platform is hit, then the normal defaults to pointing up.
	// This feature must be enabled via per-project defaults (ExteriorsAllowNormalSnapping)
	bool bSnapNormal;								AST(NAME("SnapNormal"))

	int iRoomLevel;									AST(NAME("RoomLevel")) // Which level of the room the component is in (if it is in a room)
	UGCZOrderSort eZOrderSort;						AST(NAME("ZOrderSort"))

	// External placement
	char* pcExternalMapName;						AST(NAME("ExternalMap"))
	char* pcExternalObjectName;						AST(NAME("ExternalObject"))
} UGCComponentPlacement;
extern ParseTable parse_UGCComponentPlacement[];
#define TYPE_parse_UGCComponentPlacement UGCComponentPlacement

AUTO_ENUM;
typedef enum UGCWhenType
{
	UGCWHEN_OBJECTIVE_IN_PROGRESS,		ENAMES(OBJECTIVE_IN_PROGRESS IN_PROGRESS)
	UGCWHEN_OBJECTIVE_COMPLETE,			ENAMES(OBJECTIVE_COMPLETE COMPLETE)
	UGCWHEN_OBJECTIVE_START,			ENAMES(OBJECTIVE_START START)
	UGCWHEN_MISSION_START,				ENAMES(MISSION_START)
	UGCWHEN_MAP_START,					ENAMES(MAP_START)
	UGCWHEN_COMPONENT_COMPLETE,			ENAMES(COMPONENT_COMPLETE)
	UGCWHEN_COMPONENT_REACHED,			ENAMES(COMPONENT_REACHED)
	UGCWHEN_CURRENT_COMPONENT_COMPLETE,	ENAMES(CURRENT_COMPONENT_COMPLETE)
	UGCWHEN_DIALOG_PROMPT_REACHED,		ENAMES(DIALOG_PROMPT_REACHED)
	UGCWHEN_PLAYER_HAS_ITEM,			ENAMES(PLAYER_HAS_ITEM)
	UGCWHEN_MANUAL,						ENAMES(MANUAL) // Basically means "Never"
} UGCWhenType;
extern StaticDefineInt UGCWhenTypeEnum[];

AUTO_STRUCT;
typedef struct UGCWhenDialogPrompt
{
	U32 uDialogID;							AST(NAME("DialogID") DEFAULT(-1))
	int iPromptID;							AST(NAME("PromptID") DEFAULT(-1))
} UGCWhenDialogPrompt;
extern ParseTable parse_UGCWhenDialogPrompt[];
#define TYPE_parse_UGCWhenDialogPrompt UGCWhenDialogPrompt

AUTO_STRUCT;
typedef struct UGCWhen
{
	UGCWhenType eType;						AST(NAME("Type") STRUCTPARAM)
	U32 *eauComponentIDs;					AST(NAME("ComponentID"))
	U32 *eauObjectiveIDs;					AST(NAME("ObjectiveID"))
	char *strItemName;						AST(NAME("ItemName"))
	UGCWhenDialogPrompt** eaDialogPrompts;	AST(NAME("DialogPrompt"))
} UGCWhen;
extern ParseTable parse_UGCWhen[];
#define TYPE_parse_UGCWhen UGCWhen

AUTO_STRUCT;
typedef struct UGCFSMVar
{
	const char* astrName;							AST(NAME("Name") POOL_STRING)
	float floatVal;									AST(NAME("FloatVal"))
	char* strStringVal;								AST(NAME("StringVal"))
	UGCWhen* pWhenVal;								AST(NAME("WhenVal"))
} UGCFSMVar;
extern ParseTable parse_UGCFSMVar[];
#define TYPE_parse_UGCFSMVar UGCFSMVar

AUTO_STRUCT;
typedef struct UGCFSMProperties
{
	char *pcFSMNameRef;								AST(NAME("FSMRef"))
	UGCFSMVar** eaExternVarsV1;						AST(NAME("ExternVarV1"))
	WorldVariableDef **eaExternVarsV0;				AST(NAME("ExternVar") NO_INDEX)
} UGCFSMProperties;
extern ParseTable parse_UGCFSMProperties[];
#define TYPE_parse_UGCFSMProperties UGCFSMProperties

AUTO_STRUCT;
typedef struct UGCRoomDetailData
{
	int iIndex;							AST(NAME("Index"))
	int iChoice;						AST(NAME("Choice"))
} UGCRoomDetailData;
extern ParseTable parse_UGCRoomDetailData[];
#define TYPE_parse_UGCRoomDetailData UGCRoomDetailData

AUTO_STRUCT AST_IGNORE("Count") AST_IGNORE("InteractDef") AST_IGNORE("PromptTitle") AST_IGNORE("PromptInteractionText");
typedef struct UGCComponent
{
	U32 uID;										AST(NAME("ID") STRUCTPARAM KEY)
	U32 uParentID;									AST(NAME("ParentID"))
	INT_EARRAY uChildIDs;							AST(NAME("ChildIDs"))
	char *pcVisibleName;							AST(NAME("VisibleName"))
	UGCComponentType eType;							AST(NAME("Type"))
	UGCComponentPlacement sPlacement;				AST(NAME("Placement"))
	UGCMapType eMapType;							AST(NAME("MapType"))

	char *pcOldObjectLibraryName;					AST(NAME("ObjectName"))
	int iObjectLibraryId;							AST(NAME("ObjectID"))

	// When this object becomes visible
	UGCWhen *pStartWhen;							AST(NAME("When") NAME("PromptWhen") NAME("PromptIsPopup"))
	// When this object becomes hidden
	UGCWhen *pHideWhen;								AST(NAME("HideWhen"))
	// A player must have this checked attribute to see this object (done per-player)
	UGCCheckedAttrib visibleCheckedAttrib;			AST(NAME("VisibleCheckedAttrib"))

	// For "Room" and "Sound" types
		const char *strSoundEvent;					AST(NAME("SoundEvent") POOL_STRING)

	// For "Marker" type
		bool bIsRoomVolume;							AST(NAME("IsRoomVolume"))
		F32 fVolumeRadius;							AST(NAME("VolumeRadius"))

	// For "Planet" type
		char *pcOldPlanetRingName;					AST(NAME("Ring"))
		int iPlanetRingId;							AST(NAME("RingID"))

	// For "Interact" type
		char *pcOldInteractText;					AST(NAME("InteractText"))
		REF_TO(AIAnimList) hOldInteractAnim;		AST(NAME("InteractAnim"))
		UGCInteractDuration eOldInteractDuration;	AST(NAME("InteractDuration") DEFAULT(UGCDURATION_MEDIUM))

		// If InteractIsMissionReturn is set, then eaTriggerGroups
		// should be ignored.  IsMissionReturn means that this object
		// does a MissionReturn.
		UGCInteractProperties **eaTriggerGroups;	AST(NAME("InteractTriggerGroup"))
		bool bInteractIsMissionReturn;				AST(NAME("InteractIsMissionReturn"))
		bool bInteractForce;						AST(NAME("Interact"))

	// For "Contact" type
		char *pcPromptCostumeName;					AST(NAME("PromptCostumeName")) // DEPRECATED; Now uses the costume on the Dialog Tree component

	// For "Dialog Tree" type
		// What is said
		UGCDialogTreeBlock dialogBlock;				AST(EMBEDDED_FLAT)

		// legacy format, one component, multiple blocks
		UGCDialogTreeBlock** blocksV1;				AST(NAME("Block") ADDNAMES("Prompt"))

	// For both "FSM Chain" and "Dialog Tree" type (any timeline object)
		U32 uActorID;								AST(NAME("ActorID") ADDNAMES("PromptContactID"))
		U32 *eaObjectiveIDs;						AST(NAME("ObjectiveID") ADDNAMES("PromptWhenObjectiveID"))
		bool bIsDefault;							AST(NAME("IsDefault"))

	// For "Actor" and "Contact" types
		bool bDisplayNameWasFixed;					AST(NAME(DisplayNameWasFixed))	// Says whether the DisplayName_DEPRECATED field has properly been copied to VisibleName
		char *pcDisplayName_DEPRECATED;				AST(NAME("DisplayName") ADDNAMES("ContactName"))
		char *pcCostumeName;						AST(NAME("CostumeName"))

	// For "FSM Chain" and "Kill" types
		UGCFSMProperties fsmProperties;				AST(EMBEDDED_FLAT)
		U32 *eaPatrolPoints;						AST(NAME("PatrolPoint")) // Actually a list of component IDs
		char *pcDropItemName;						AST(NAME("DropItem"))

		// Legacy behavior was for patrols to start at the kill
		// component's position.  Fixup was added to support legacy projects.
		//
		// This flag will be false if the fixup is necessary.
		bool bPatrolPointsFixed_FromComponentPosition;	AST(NAME(bPatrolPointsFixed_FromComponentPosition))

	// For "Room" type
		UGCRoomDetailData **eaRoomDetails;			AST(NAME("Detail"))
		const char *strSoundDSP;					AST(NAME("SoundDSP"))

	// For "Room Door" type
		int iRoomDoorID;							AST(NAME("RoomDoor"))
		bool bIsDoorExplicitDefault;				AST(NAME("IsDoorExplicitDefault"))

	// For "Trap" type
		char *pcTrapPower;							AST(NAME("TrapPower"))

	// For "Trap Target" type
		int iTrapEmitterIndex;						AST(NAME("EmitterIndex"))

	// For "Patrol Point" type
		U32 uPatrolParentID;						AST(NAME("PatrolParentID"))

	// For "Actor" type
		char *pcActorCritterGroupName;				AST(NAME("ActorCritterGroupName"))
} UGCComponent;
extern ParseTable parse_UGCComponent[];
#define TYPE_parse_UGCComponent UGCComponent

#define UGC_FOR_EACH_COMPONENT_OF_TYPE(pUGCComponentList, iUGCComponentType, component) \
	FOR_EACH_IN_EARRAY_FORWARDS(pUGCComponentList->eaComponents, UGCComponent, component) { \
		if(component->eType == iUGCComponentType)
#define UGC_FOR_EACH_COMPONENT_ON_MAP(pUGCComponentList, pcMapName, component) \
	FOR_EACH_IN_EARRAY_FORWARDS(pUGCComponentList->eaComponents, UGCComponent, component) { \
		if(ugcComponentIsOnMap(component, pcMapName, false))
#define UGC_FOR_EACH_COMPONENT_OF_TYPE_ON_MAP(pUGCComponentList, iUGCComponentType, pcMapName, component) \
	FOR_EACH_IN_EARRAY_FORWARDS(pUGCComponentList->eaComponents, UGCComponent, component) { \
		if(component->eType == iUGCComponentType && ugcComponentIsOnMap(component, pcMapName, false))
#define UGC_FOR_EACH_COMPONENT_END } FOR_EACH_END

/// Costumes

// Player-created costumes.

AUTO_STRUCT;
typedef struct UGCCostumeSlot
{
	const char* astrSlot;					AST(NAME("Slot") POOL_STRING)
	const char* astrCostume;				AST(NAME("Costume") POOL_STRING)
	int* eaColors;							AST(NAME("Color"))
} UGCCostumeSlot;
extern ParseTable parse_UGCCostumeSlot[];
#define TYPE_parse_UGCCostumeSlot UGCCostumeSlot

AUTO_STRUCT;
typedef struct UGCCostumeScale {
	const char* astrName;					AST(NAME("Name") POOL_STRING)
	float value;							AST(NAME("Value"))
} UGCCostumeScale;
extern ParseTable parse_UGCCostumeScale[];
#define TYPE_parse_UGCCostumeScale UGCCostumeScale

AUTO_STRUCT;
typedef struct UGCCostumePart {
	const char* astrBoneName;				AST(NAME("Bone") POOL_STRING)
	const char* astrGeometryName;			AST(NAME("Geometry") POOL_STRING)
	const char* astrMaterialName;			AST(NAME("Material") POOL_STRING)
	const char* astrTexture0Name;			AST(NAME("Texture0") POOL_STRING)
	const char* astrTexture1Name;			AST(NAME("Texture1") POOL_STRING)
	const char* astrTexture2Name;			AST(NAME("Texture2") POOL_STRING)
	const char* astrTexture3Name;			AST(NAME("Texture3") POOL_STRING)
	int colors[4];							AST(NAME("Color"))
} UGCCostumePart;
extern ParseTable parse_UGCCostumePart[];
#define TYPE_parse_UGCCostumePart UGCCostumePart

AUTO_STRUCT;
typedef struct UGCCostumeData
{
	const char* astrPresetCostumeName;		AST(NAME("PresetCostume") POOL_STRING)
	UGCCostumeSlot** eaSlots;				AST(NAME("Slot"))
	UGCCostumeScale** eaBodyScales;			AST(NAME("BodyScale"))
	UGCCostumeScale** eaScales;				AST(NAME("Scale"))
	UGCCostumePart** eaParts;				AST(NAME("Part"))
	const char *astrStance;					AST(NAME("Stance") POOL_STRING)
	
	F32 fHeight;							AST(NAME("Height"))
	int skinColor;							AST(NAME("SkinColor"))
	bool isAdvanced;						AST(NAME("IsAdvanced"))
} UGCCostumeData;
extern ParseTable parse_UGCCostumeData[];
#define TYPE_parse_UGCCostumeData UGCCostumeData

AUTO_STRUCT;
typedef struct UGCCostume
{
	const char *astrName;					AST(NAME("Name") POOL_STRING KEY)
	const char *fstrFilename;				AST(CURRENTFILE NO_NETSEND)

	// Add costume fields here
	char *pcDisplayName;					AST(NAME("DisplayName"))
	char *pcDescription;					AST(NAME("Description"))

	// "Neverwinter" mode
	UGCCostumeData data;					AST(NAME("Data"))
	PlayerCostume* pCachedPlayerCostume;	AST(NO_NETSEND NO_WRITE)

	// (legacy) "CharCreator" mode
	PlayerCostume *pPlayerCostume;			AST(NAME("PlayerCostume"))
	U32 eRegion;							AST(NAME("Region"))	// CharClassTypes enum cannot be saved properly so using U32
	REF_TO(AllegianceDef) hAllegiance;		AST(NAME("Allegiance"))
} UGCCostume;
extern ParseTable parse_UGCCostume[];
#define TYPE_parse_UGCCostume UGCCostume

/// Items

// Player-created items.

AUTO_STRUCT;
typedef struct UGCItem
{
	const char *astrName;					AST(NAME("Name") POOL_STRING KEY)
	const char *fstrFilename;				AST(CURRENTFILE NO_NETSEND)

	char *strDisplayName;					AST(NAME("DisplayName"))
	char *strDescription;					AST(NAME("Description"))
	char *strIcon;							AST(NAME("Icon"))
} UGCItem;
extern ParseTable parse_UGCItem[];
#define TYPE_parse_UGCItem UGCItem

/// Sounds (static dictionary)

#define UGC_DICTIONARY_SOUND "UGCSound"

AUTO_STRUCT;
typedef struct UGCSound
{
	const char *astrName;					AST(NAME("Name") POOL_STRING STRUCTPARAM KEY)
	char *strTags;							AST(NAME("Tags"))
	char *strSoundName;						AST(NAME("SoundName") ADDNAMES("AmbSound"))
} UGCSound;
extern ParseTable parse_UGCSound[];
#define TYPE_parse_UGCSound UGCSound

#define UGC_DICTIONARY_SOUND_DSP "UGCSoundDSP"

AUTO_STRUCT;
typedef struct UGCSoundDSP
{
	const char *astrName;					AST(NAME("Name") POOL_STRING STRUCTPARAM KEY)
	char *strSoundDSPName;					AST(NAME("SoundDSPName"))
} UGCSoundDSP;
extern ParseTable parse_UGCSoundDSP[];
#define TYPE_parse_UGCSoundDSP UGCSoundDSP

/// Trap Power Groups

#define UGC_DICTIONARY_TRAP_POWER_GROUP "UGCTrapPowerGroup"

AUTO_STRUCT;
typedef struct UGCTrapPowerGroup
{
	const char *astrName;					AST(NAME("Name") POOL_STRING STRUCTPARAM KEY)
	char **eaPowerNames;					AST(NAME("PowerDef"))
} UGCTrapPowerGroup;
extern ParseTable parse_UGCTrapPowerGroup[];
#define TYPE_parse_UGCTrapPowerGroup UGCTrapPowerGroup

// Utility structure for bundling all the resources in a project
// namespace into a single structure that can be serialized to
// file or across a wire, diffed, etc.
AUTO_STRUCT
AST_IGNORE_STRUCT(DeleteRes);
typedef struct UGCProjectData
{
	char *ns_name;							AST(NAME("Namespace"))
	char *project_prefix;					AST(NAME("ProjectPrefix")) // For commit-to-layers
	UGCProjectInfo *project;				AST(NAME("Project"))
	// If you change the fields above (their order, number, or types) you must also edit UGCProjectDataHeader)

	UGCMap **maps;							AST(NAME("Map"))
	UGCMission *mission;					AST(NAME("Mission"))
	UGCComponentList *components;			AST(NAME("Components"))
	UGCCostume **costumes;					AST(NAME("Costume"))
	UGCItem **items;						AST(NAME("Item"))
} UGCProjectData;
extern ParseTable parse_UGCProjectData[];
#define TYPE_parse_UGCProjectData UGCProjectData

/// Resource functions

void ugcResourceLoadLibrary( void );
