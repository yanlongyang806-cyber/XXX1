//// UGC common routines
////
//// Structures & functions internally used by the UGC system by both client
//// and server.
#pragma once
GCC_SYSTEM

#include "ReferenceSystem.h"
#include "WorldLibStructs.h"
#include "globaltypes.h"
#include "progression_common.h"
#include "utils.h"
#include "wlGenesisMissionsGameStructs.h"
#include "wlUGC.h"
#include "ugcprojectcommon.h"

typedef enum MultiValType MultiValType;
typedef enum UGCComponentType UGCComponentType;
typedef enum UGCMapType UGCMapType;
typedef enum UGCWhenType UGCWhenType; 
typedef enum WorldPatrolRouteType WorldPatrolRouteType;
typedef struct CharacterPath CharacterPath;
typedef struct CostumeEditLine CostumeEditLine;
typedef struct ExclusionVolumeGroup ExclusionVolumeGroup;
typedef struct FSMExternVar FSMExternVar;
typedef struct GlobalGAELayerDef GlobalGAELayerDef;
typedef struct GroupDef GroupDef;
typedef struct GroupVolumeProperties GroupVolumeProperties;
typedef struct HeightMapExcludeGrid HeightMapExcludeGrid;
typedef struct InteractionDef InteractionDef;
typedef struct LibFileLoad LibFileLoad;
typedef struct NOCONST(UGCProject) NOCONST(UGCProject);
typedef struct NOCONST(UGCProjectReviews) NOCONST(UGCProjectReviews);
typedef struct NOCONST(UGCProjectVersion) NOCONST(UGCProjectVersion);
typedef struct NOCONST(UGCProjectVersionRestrictionProperties) NOCONST(UGCProjectVersionRestrictionProperties);
typedef struct NOCONST(UGCTimeStamp) NOCONST(UGCTimeStamp);
typedef struct PCPart PCPart;
typedef struct PetContactList PetContactList;
typedef struct PlayerCostume PlayerCostume;
typedef struct ResourceInfo ResourceInfo;
typedef struct ResourceSearchRequest ResourceSearchRequest;
typedef struct ResourceSearchResult ResourceSearchResult;
typedef struct RewardTable RewardTable;
typedef struct SecondaryZoneMap SecondaryZoneMap;
typedef struct SkyInfo SkyInfo;
typedef struct SkyInfoGroup SkyInfoGroup;
typedef struct UGCAssetTagCategory UGCAssetTagCategory;
typedef struct UGCBacklinkTable UGCBacklinkTable;
typedef struct UGCCheckedAttrib UGCCheckedAttrib;
typedef struct UGCComponent UGCComponent;
typedef struct UGCComponentList UGCComponentList;
typedef struct UGCComponentPlacement UGCComponen8tPlacement;
typedef struct UGCComponentPlacement UGCComponentPlacement;
typedef struct UGCComponentPlacementInfo UGCComponentPlacementInfo;
typedef struct UGCCostume UGCCostume;
typedef struct UGCCostumeMetadata UGCCostumeMetadata;
typedef struct UGCCostumePart UGCCostumePart;
typedef struct UGCCostumeScale UGCCostumeScale;
typedef struct UGCCostumeSlot UGCCostumeSlot;
typedef struct UGCDefChildMetadata UGCDefChildMetadata;
typedef struct UGCDialogTreeBlock UGCDialogTreeBlock;
typedef struct UGCDialogTreePrompt UGCDialogTreePrompt;
typedef struct UGCFSM UGCFSM;
typedef struct UGCFSMExternVar UGCFSMExternVar;
typedef struct UGCFSMVar UGCFSMVar;
typedef struct UGCItem UGCItem;
typedef struct UGCItemList UGCItemList;
typedef struct UGCKillCreditLimit UGCKillCreditLimit;
typedef struct UGCKillCreditLimit2 UGCKillCreditLimit2;
typedef struct UGCMap UGCMap;
typedef struct UGCMapLocation UGCMapLocation;
typedef struct UGCMission UGCMission;
typedef struct UGCMissionMapLink UGCMissionMapLink;
typedef struct UGCMissionObjective UGCMissionObjective;
typedef struct UGCPerAllegianceDefaults UGCPerAllegianceDefaults;
typedef struct UGCProject UGCProject;
typedef struct UGCProjectData UGCProjectData; 
typedef struct UGCProjectInfo UGCProjectInfo;
typedef struct UGCProjectReviews UGCProjectReviews;
typedef struct UGCProjectSearchInfo UGCProjectSearchInfo;
typedef struct UGCProjectSeries UGCProjectSeries;
typedef struct UGCProjectSeries UGCProjectSeries;
typedef struct UGCProjectSeriesVersion UGCProjectSeriesVersion;
typedef struct UGCProjectVersion UGCProjectVersion;
typedef struct UGCProjectVersionRestrictionProperties UGCProjectVersionRestrictionProperties;
typedef struct UGCRuntimeStatus UGCRuntimeStatus;
typedef struct UGCSingleReview UGCSingleReview;
typedef struct UGCSpaceBackdropLight UGCSpaceBackdropLight;
typedef struct UGCTimeStampPlusShardName UGCTimeStampPlusShardName;
typedef struct WorldFXVolumeProperties WorldFXVolumeProperties;
typedef struct WorldPowerVolumeProperties WorldPowerVolumeProperties;
typedef struct WorldRegion WorldRegion;
typedef struct WorldSkyVolumeProperties WorldSkyVolumeProperties;
typedef struct WorldUGCProperties WorldUGCProperties;
typedef struct WorldUGCRestrictionProperties WorldUGCRestrictionProperties;
typedef struct WorldVariableDef WorldVariableDef;
typedef struct ZoneMapEncounterRegionInfo ZoneMapEncounterRegionInfo;
typedef struct ZoneMapEncounterRoomInfo ZoneMapEncounterRoomInfo;
typedef struct ZoneMapInfo ZoneMapInfo;
typedef struct ZoneMapMetadataPathNode ZoneMapMetadataPathNode;
									 
#define GENESIS_UGC_LAYOUT_NAME "UGCLayout"
#define GENESIS_UNPLACED_ID 0xFFFFFFFF
#define UGC_TOPLEVEL_ROOM_ID 0xFFFFEEEE
#define UGC_ANY_ROOM_ID 0xFFFFDDDD

AUTO_STRUCT;
typedef struct UGCFSMMetadata
{
	UGCFSMExternVar** eaExternVars;				AST(NAME("ExternVar"))
} UGCFSMMetadata;
extern ParseTable parse_UGCFSMMetadata[];
#define TYPE_parse_UGCFSMMetadata UGCFSMMetadata

AUTO_STRUCT;
typedef struct UGCFSMExternVarDef
{
	const char* name;							AST(STRUCTPARAM)
	REF_TO(Message) hDisplayName;				AST(NAME("DisplayName"))
	REF_TO(Message) hTooltip;					AST(NAME("Tooltip"))	

	// Properties only valid if the type is int or float
	float minValue;								AST(NAME("MinValue"))
	float maxValue;								AST(NAME("MaxValue"))
	float defaultValue;							AST(NAME("DefaultValue"))
} UGCFSMExternVarDef;
extern ParseTable parse_UGCFSMExternVarDef[];
#define TYPE_parse_UGCFSMExternVarDef UGCFSMExternVarDef

AUTO_STRUCT;
typedef struct UGCFSMExternVar
{
	const char* astrName;						AST(NAME("Name") POOL_STRING)
	MultiValType type;							AST(NAME("Type") INT)
	const char* scType;							AST(NAME("scType"))

	UGCFSMExternVarDef defProps;				AST(NAME("DefProps"))
} UGCFSMExternVar;
extern ParseTable parse_UGCFSMExternVar[];
#define TYPE_parse_UGCFSMExternVar UGCFSMExternVar

AUTO_STRUCT;
typedef struct UGCCostumeMetadata
{
	// The full costume -- only set if this is a costume you can use as a prefab
	PlayerCostume* pFullCostume;				AST(NAME("FullCostume"))

	// The parts in this costume -- only set if this is an item costume
	PCPart** eaItemParts;						AST(NAME("ItemPart"))
} UGCCostumeMetadata;
extern ParseTable parse_UGCCostumeMetadata[];
#define TYPE_parse_UGCCostumeMetadata UGCCostumeMetadata

AUTO_STRUCT;
typedef struct UGCGroupDefMetadata
{
	// Information about child positions / orientations.  Only set for
	// COMPONENT_TYPE_CLUSTER.
	UGCDefChildMetadata** eaClusterChildren;	AST(NAME("UGCClusterChild"))

	// List of all the PathNodes, their connections, and their
	// positions in the def.
	ZoneMapMetadataPathNode** eaPathNodes;		AST(NAME("UGCPathNode"))
} UGCGroupDefMetadata;
extern ParseTable parse_UGCGroupDefMetadata[];
#define TYPE_parse_UGCGroupDefMetadata UGCGroupDefMetadata

AUTO_STRUCT;
typedef struct UGCDefChildMetadata
{
	int defUID;									AST(NAME("DefUID"))
	const char* astrDefDebugName;				AST(NAME("DefDebugName"))
	Vec3 pos;									AST(NAME("Pos"))
	float rot;									AST(NAME("Rot"))
} UGCDefChildMetadata;
extern ParseTable parse_UGCDefChildMetadata[];
#define TYPE_parse_UGCDefChildMetadata UGCDefChildMetadata

/// Editor defines

/// Per-project defaults structure

// This is where all project-specific defaults are stored.
// The structure is loaded from this file:
//   data\genesis\ugc_defaults.txt

AUTO_ENUM;
typedef enum UGCBudgetType
{
	UGC_BUDGET_TYPE_MAP = 1,
	UGC_BUDGET_TYPE_COMPONENT,
	UGC_BUDGET_TYPE_COSTUME,
	UGC_BUDGET_TYPE_OBJECTIVE,
	UGC_BUDGET_TYPE_LIGHT,
	UGC_BUDGET_TYPE_ITEM,
	UGC_BUDGET_TYPE_DIALOG_TREE_PROMPT,
} UGCBudgetType;

AUTO_STRUCT;
typedef struct UGCProjectBudget
{
	UGCBudgetType eType;
	UGCComponentType eComponentType; // For TYPE_COMPONENT
	int iSoftLimit;
	int iHardLimit;
} UGCProjectBudget;
extern ParseTable parse_UGCProjectBudget[];
#define TYPE_parse_UGCProjectBudget UGCProjectBudget

typedef enum UGCDialogStyle
{
	UGC_DIALOG_STYLE_WINDOW,
	UGC_DIALOG_STYLE_IN_WORLD,	
} UGCDialogStyle;

typedef enum UGCCostumeEditorStyle
{
	UGC_COSTUME_EDITOR_STYLE_CHAR_CREATOR,
	UGC_COSTUME_EDITOR_STYLE_NEVERWINTER,
} UGCCostumeEditorStyle;

AUTO_STRUCT;
typedef struct UGCPerProjectEncounterRank
{
	const char *pcRankName;						AST(POOL_STRING STRUCTPARAM)
	char *pcMarkerTex;							AST(NAME("MarkerImage"))
} UGCPerProjectEncounterRank;

AUTO_STRUCT;
typedef struct UGCCostumeNWPartDef
{
	const char* astrName;						AST(NAME("Name") POOL_STRING)
	bool enableGeometry;						AST(NAME("EnableGeometry"))
	bool enableMaterial;						AST(NAME("EnableMaterial"))
	bool enableTextures;						AST(NAME("EnableTextures"))
	bool enableColors;							AST(NAME("EnableColors"))
} UGCCostumeNWPartDef;
extern ParseTable parse_UGCCostumeNWPartDef[];
#define TYPE_parse_UGCCostumeNWPartDef UGCCostumeNWPartDef

AUTO_STRUCT;
typedef struct UGCCostumeNWRegionModeDef
{
	UGCCostumeNWPartDef** eaParts;				AST(NAME("Part"))
	const char** eaBodyScales;					AST(POOL_STRING NAME("BodyScale"))
	const char** eaScaleInfos;					AST(POOL_STRING NAME("ScaleInfo"))
} UGCCostumeNWRegionModeDef;
extern ParseTable parse_UGCCostumeNWRegionModeDef[];
#define TYPE_parse_UGCCostumeNWRegionModeDef UGCCostumeNWRegionModeDef

AUTO_STRUCT;
typedef struct UGCCostumeRegionDef
{
	const char* astrName;						AST(POOL_STRING STRUCTPARAM)

	// Used in CHAR_CREATOR mode
	const char** eaBonesInclude;				AST(POOL_STRING NAME("BoneInclude"))
	const char** eaBonesExclude;				AST(POOL_STRING NAME("BoneExclude"))
	const char** eaBodyScalesInclude;			AST(POOL_STRING NAME("BodyScaleInclude"))
	const char** eaBodyScalesExclude;			AST(POOL_STRING NAME("BodyScaleExclude"))
	const char** eaScaleGroups;					AST(POOL_STRING NAME("ScaleGroup"))

	// Used in NEVERWINTER mode
	UGCCostumeNWRegionModeDef nwBasic;			AST(NAME("NWBasic"))
	UGCCostumeNWRegionModeDef nwAdvanced;		AST(NAME("NWAdvanced"))
} UGCCostumeRegionDef;

AUTO_STRUCT;
typedef struct UGCCostumeSlotDef
{
	const char* astrName;						AST(POOL_STRING NAME("Name"))
	REF_TO(Message) hDisplayName;				AST(POOL_STRING NAME("DisplayName"))
	REF_TO(Message) hTooltip;					AST(POOL_STRING NAME("Tooltip"))
	const char** eaBones;						AST(POOL_STRING NAME("Bone"))
} UGCCostumeSlotDef;
extern ParseTable parse_UGCCostumeSlotDef[];
#define TYPE_parse_UGCCostumeSlotDef UGCCostumeSlotDef

AUTO_STRUCT;
typedef struct UGCCostumeSkeletonSlotDef
{
	const char* astrName;						AST(POOL_STRING NAME("Name"))
	UGCCostumeSlotDef** eaSlotDef;				AST(NAME("SlotDef"))
} UGCCostumeSkeletonSlotDef;
extern ParseTable parse_UGCCostumeSkeletonSlotDef[];
#define TYPE_parse_UGCCostumeSkeletonSlotDef UGCCostumeSkeletonSlotDef

AUTO_STRUCT;
typedef struct UGCSpecialComponentDef
{
	char* pcLabel;								AST(STRUCTPARAM)
	const char* astrMessageKey;					AST(NAME(MessageKey) POOL_STRING)
	const char* astrDescriptionMessageKey;		AST(NAME(DescriptionMessageKey) POOL_STRING)
	UGCComponentType eType;						AST(NAME(Type))
	const char *astrObjectName;					AST(NAME(ObjectName) POOL_STRING)
	const char* astrTextureOverride;			AST(NAME(TextureOverride))

	UGCMapType eRestrictToMapType;				AST(NAME(RestrictToMapType))
	bool bSpaceOnly;							AST(NAME(SpaceOnly))
	bool bGroundOnly;							AST(NAME(GroundOnly))
} UGCSpecialComponentDef;

AUTO_STRUCT;
typedef struct UGCCostumeNamingConventionField
{
	char *pcFieldName;							AST(STRUCTPARAM)
	bool bOptional;
	char *pcTooltip;
} UGCCostumeNamingConventionField;

AUTO_STRUCT;
typedef struct UGCRect
{
	float x;								AST(STRUCTPARAM)
	float y;								AST(STRUCTPARAM)
	float w;								AST(STRUCTPARAM)
	float h;								AST(STRUCTPARAM)
} UGCRect;
extern ParseTable parse_UGCRect[];
#define TYPE_parse_UGCRect UGCRect

//an axis-aligned rectangular region in a UGCMapRegions.
AUTO_STRUCT;
typedef struct UGCOverworldMapRegion
{
	const char* astrMapName;				AST(NAME(MapName) POOL_STRING)
	UGCMapLocation* pMapLocation;			AST(NAME(MapLocation))
	UGCRect** eaRects;						AST(NAME(Rect))
} UGCOverworldMapRegion;
extern ParseTable parse_UGCOverworldMapRegion[];
#define TYPE_parse_UGCOverworldMapRegion UGCOverworldMapRegion

AUTO_STRUCT;
typedef struct UGCCheckedAttribDef
{
	const char* name;						AST(NAME(Name) POOL_STRING)
	const char* displayName;				AST(NAME(DisplayName))
	char* playerExprText;					AST(NAME(PlayerExprText))
	char* teamExprText;						AST(NAME(TeamExprText))
} UGCCheckedAttribDef;
extern ParseTable parse_UGCCheckedAttribDef[];
#define TYPE_parse_UGCCheckedAttribDef UGCCheckedAttribDef

AUTO_STRUCT AST_IGNORE("DefaultDoorMap");
typedef struct UGCPerProjectDefaults
{
	// Allegiance
	const char *pcAllegianceRestriction;		AST(NAME("AllegianceRestriction") POOL_STRING)

	// Links
	UGCDialogTreePrompt* pDefaultTransitionPrompt; AST(NAME("DefaultTransitionPrompt"))

	// Missions
	MissionPlayType nonCombatType;				AST(NAME("NonCombatType"))
	MissionPlayType combatType;					AST(NAME("CombatType"))

	// Mission Override
	char *pcOverrideCategoryName;				AST(NAME("OverrideCategoryName"))

	// Pets
	UGCPerAllegianceDefaults** allegiance;		AST(NAME("Allegiance"))

	// Encounter actor ranks
	UGCPerProjectEncounterRank **ranks; 		AST(NAME("Rank"))

	// Encounter limits
	S32 iMaxActorsInAggroDist;					AST(NAME("MaxActorsInAggroDist"))
	S32 iMaxEnemyActorsInAggroDist;				AST(NAME("MaxEnemyActorsInAggroDist"))
	S32 iMaxFriendlyActorsInAggroDist;			AST(NAME("MaxFriendlyActorsInAggroDist"))

	// Genesis Misc
	WorldVariableDef** variableDefs;			AST(NAME("VariableDef"))

	// Behaviors
	char *pcBehavior;							AST(NAME("Behavior"))
	char *pcNoCombatBehavior;					AST(NAME("NoCombatBehavior"))
	
	// Traps
	char *pcTrapObject;

	// Interiors
	char *pcCustomInteriorMap;					AST(NAME("CustomInteriorMap"))
	REF_TO(SkyInfo) hInteriorSky;				AST(NAME("InteriorSky"))
	REF_TO(InteractionDef) hInteriorClickyInteractionDef;
	char *pcInteriorKillObject;
	char *pcInteriorDetailObject;
	char *pcInteriorDestructibleObject;
	char *pcInteriorDoorObject;

	S32 iMaxRoomSize;

	// Space
	REF_TO(SkyInfo) hSpaceSky;					AST(NAME("SpaceSky"))
	REF_TO(InteractionDef) hSpaceClickyInteractionDef;
	char *pcSpaceKillObject;
	char *pcSpaceDetailObject;
	char *pcSpaceDestructibleObject;
	char *pcSpacePlanetObject;
	char *pcSpaceRingObject;
	char *pcSpaceSunObject;
	F32 fSpaceAggroDistance;
	F32 fSpaceMaxActorDistance;

	// Ground
	REF_TO(SkyInfo) hGroundSky;					AST(NAME("GroundSky"))
	char *pcGroundBuildingObject;
	F32 fGroundAggroDistance;
	F32 fGroundMaxActorDistance;

	// Costumes
	// stf_head, stf_upperbody, stf_lowerbody
	UGCCostumeSkeletonSlotDef** eaCostumeSkeletonDefs;	AST(NAME("CostumeSkeletonDef"))
	UGCCostumeRegionDef** eaCostumeRegionDefs;			AST(NAME("CostumeRegionDef") NAME("CostumeRegionScale"))

	// Series
	GameProgressionNodeType eProgressionNodeTypeProject; AST(NAME("ProgressionNodeTypeProject"))
	GameProgressionNodeType eProgressionNodeType1;	AST(NAME("ProgressionNodeType1"))
	GameProgressionNodeType eProgressionNodeType2;	AST(NAME("ProgressionNodeType2"))

	// Special Component Types
	UGCSpecialComponentDef **eaSpecialComponents;	AST(NAME("SpecialComponent"))

	// Budgets
	UGCProjectBudget **ppDefaultBudgets;			AST(NAME("Budget"))


	// For costumes to be frozen to data directory
	UGCCostumeNamingConventionField **eaCostumeNamingConventionFields;	AST(NAME("CostumeNameField"))

	// For finding the region a location on a map is in.
	UGCOverworldMapRegion** eaMapRegions;			AST(NAME("MapRegion"))

	const char *pcDialogTreePromptCameraPosition;	AST(NAME("Cutscene"))

	// Tethering is when related components on a map are kept near each other (e.g. spawn point and abort exit)
	bool bTetheringAllowed;							AST(NAME("TetheringAllowed"))

	// Normal snapping allowed for exteriors
	bool bExteriorsAllowNormalSnapping;		AST(NAME("ExteriorsAllowNormalSnapping"))

	// Whens
	UGCCheckedAttribDef** checkedAttribs; AST(NAME("CheckedAttrib"))

	// Exterior config
	WorldSkyVolumeProperties *boundary;		AST(NAME("BoundsSkyFade"))

	// Prompts
	const char* strFallbackPromptText;		AST(NAME("FallbackPromptText"))

	// Tags whitelist
	UGCTag *eaiUGCTags;						AST(NAME("UGCTag"))
} UGCPerProjectDefaults;
extern ParseTable parse_UGCPerProjectDefaults[];
#define TYPE_parse_UGCPerProjectDefaults UGCPerProjectDefaults

AUTO_STRUCT;
typedef struct UGCPetContactListMap
{
	const char* srcContactList;					AST(STRUCTPARAM POOL_STRING)
	const char* destContactList;				AST(STRUCTPARAM POOL_STRING)
} UGCPetContactListMap;
extern ParseTable parse_UGCPetContactListMap[];
#define TYPE_parse_UGCPetContactListMap UGCPetContactListMap

AUTO_STRUCT;
typedef struct UGCAllegianceRelation
{
	char* pcFilterValue;						AST(STRUCTPARAM)
	char* pcTagName;						AST(STRUCTPARAM NAME("AllegianceTagName"))
} UGCAllegianceRelation;

AUTO_STRUCT;
typedef struct UGCRespecClassLevel
{
	int iLevel;									AST(STRUCTPARAM)
	REF_TO(RewardTable) hRewardTable;			AST(NAME("RewardTable"))
} UGCRespecClassLevel;

AUTO_STRUCT;
typedef struct UGCRespecClass
{
	REF_TO(CharacterPath) hRespecClassName;		AST(STRUCTPARAM)
	UGCRespecClassLevel** eaLevels;				AST(NAME("Level"))
} UGCRespecClass;

AUTO_STRUCT;
typedef struct UGCPerAllegianceDefaults
{
	const char* allegianceName;					AST(STRUCTPARAM NAME("AllegianceName") POOL_STRING)
	char *pcDefaultCrypticMap;					AST(NAME("DefaultDoorMap"))
	
	UGCPetContactListMap** maps;				AST(NAME("Map"))
	UGCAllegianceRelation** relations;			AST(NAME("Relation"))

	UGCRespecClass** respecClasses;				AST(NAME("RespecClass"))
} UGCPerAllegianceDefaults;
extern ParseTable parse_UGCPerAllegianceDefaults[];
#define TYPE_parse_UGCPerAllegianceDefaults UGCPerAllegianceDefaults
	
/// Useful structs

// Specifies a clickable Genesis object (Room, Challenge, or Challenge Volume)
AUTO_STRUCT;
typedef struct UGCPortalClickableInfo
{
	char *mission_name;
	char *layout_name;
	char *room_name;
	char *challenge_name;
	bool is_volume;
	bool is_whole_map;
} UGCPortalClickableInfo;
extern ParseTable parse_UGCPortalClickableInfo[];
#define TYPE_parse_UGCPortalClickableInfo UGCPortalClickableInfo

AUTO_STRUCT;
typedef struct UGCComponentPatrolPoint
{
	U32 componentID;					AST(NAME("ComponentID"))
	Vec3 pos;							AST(NAME("Position"))
	U32 roomID;							AST(NAME("RoomID"))

	// For validation
	bool prevConnectionInvalid;
	bool nextConnectionInvalid;
} UGCComponentPatrolPoint;
extern ParseTable parse_UGCComponentPatrolPoint[];
#define TYPE_parse_UGCComponentPatrolPoint UGCComponentPatrolPoint

AUTO_STRUCT;
typedef struct UGCComponentPatrolPath
{
	WorldPatrolRouteType patrolType;				AST(NAME(PatrolPath))
	UGCComponentPatrolPoint **points;				AST(NAME(PatrolPoint))
} UGCComponentPatrolPath;
extern ParseTable parse_UGCComponentPatrolPath[];
#define TYPE_parse_UGCComponentPatrolPath UGCComponentPatrolPath

// Structures to get data about the Trap object library piece

AUTO_STRUCT;
typedef struct UGCTrapPointData
{
	int id;
	Vec3 pos;
} UGCTrapPointData;
extern ParseTable parse_UGCTrapPointData[];
#define TYPE_parse_UGCTrapPointData UGCTrapPointData

AUTO_STRUCT;
typedef struct UGCTrapSelfContained
{
	GroupVolumeProperties *pVolume;
} UGCTrapSelfContained;
extern ParseTable parse_UGCTrapSelfContained[];
#define TYPE_parse_UGCTrapSelfContained UGCTrapSelfContained

AUTO_STRUCT;
typedef struct UGCTrapProperties
{
	UGCTrapSelfContained *pSelfContained;
	UGCTrapPointData **eaEmitters;
} UGCTrapProperties;
extern ParseTable parse_UGCTrapProperties[];
#define TYPE_parse_UGCTrapProperties UGCTrapProperties

AUTO_STRUCT;
typedef struct UGCFreezeProjectMapInfo
{
	const char *astrInternalMapName;	AST(NAME("MapName") POOL_STRING)
	char *pcOutMapName;					AST(NAME("OutMapName"))

	// For UI
	char *pcDisplayName;				AST(NAME("DisplayName"))
	char *pcMapDirectory;				AST(NAME("MapDirectory"))
	char *pcMapFile;					AST(NAME("MapFile"))
	char *pcMapLayerFile;				AST(NAME("MapLayerFile"))
	char *pcMapMissionFile1;			AST(NAME("MapMissionFile1"))
	char *pcMapMissionFile2;			AST(NAME("MapMissionFile2"))
} UGCFreezeProjectMapInfo;
extern ParseTable parse_UGCFreezeProjectMapInfo[];
#define TYPE_parse_UGCFreezeProjectMapInfo UGCFreezeProjectMapInfo

AUTO_STRUCT;
typedef struct UGCFreezeProjectCostumeInfo
{
	const char *astrInternalCostumeName;AST(NAME("CostumeName") POOL_STRING)
	char **eaCostumeNameParts;			AST(NAME("CostumeNamePart"))

	// For UI
	char *pcDisplayName;				AST(NAME("DisplayName"))
	char *pcCostumeFile;				AST(NAME("CostumeFile"))
	char *pcOutCostumeName;				AST(NAME("OutCostumeName"))
} UGCFreezeProjectCostumeInfo;
extern ParseTable parse_UGCFreezeProjectCostumeInfo[];
#define TYPE_parse_UGCFreezeProjectCostumeInfo UGCFreezeProjectCostumeInfo

AUTO_STRUCT;
typedef struct UGCFreezeProjectInfo
{
	char *pcProjectPrefix;				AST(NAME("ProjectPrefix"))
	UGCFreezeProjectMapInfo **eaMaps;	AST(NAME("Map"))
	UGCFreezeProjectCostumeInfo **eaCostumes; AST(NAME("Costume"))

	// For UI
	char *pcProjectDirectory;			AST(NAME("ProjectDirectory"))
	char *pcMissionFile;				AST(NAME("MissionFile"))
} UGCFreezeProjectInfo;
extern ParseTable parse_UGCFreezeProjectInfo[];
#define TYPE_parse_UGCFreezeProjectInfo UGCFreezeProjectInfo

AUTO_STRUCT;
typedef struct UGCComponentValidPosition
{
	Vec3 position;
	float rotation;
	U32 room_id;
	int room_door;
	int room_level;

	F32 platform_height;
} UGCComponentValidPosition;

typedef enum UGCFixupFlags
{
	UGC_FIXUP_MOVED_DIALOG_BLOCKS = (1<<0),
	UGC_FIXUP_CLEARED_WHENS = (1<<1),
} UGCFixupFlags;

typedef enum UGCBudgetValidateState {
	UGC_BUDGET_NORMAL,
	UGC_BUDGET_SOFT_LIMIT,
	UGC_BUDGET_HARD_LIMIT,
} UGCBudgetValidateState;

AUTO_ENUM;
typedef enum UGCCostumeOverrideType {
	UGC_COSTUME_OVERRIDE_NONE,
	UGC_COSTUME_OVERRIDE_SKIN_COLOR,
	UGC_COSTUME_OVERRIDE_SLOT,
	UGC_COSTUME_OVERRIDE_SLOT_COLOR,
	UGC_COSTUME_OVERRIDE_PART_GEOMETRY,
	UGC_COSTUME_OVERRIDE_PART_MATERIAL,
	UGC_COSTUME_OVERRIDE_PART_TEXTURE0,
	UGC_COSTUME_OVERRIDE_PART_TEXTURE1,
	UGC_COSTUME_OVERRIDE_PART_TEXTURE2,
	UGC_COSTUME_OVERRIDE_PART_TEXTURE3,
	UGC_COSTUME_OVERRIDE_PART_COLOR,

	UGC_COSTUME_OVERRIDE_ENTIRE_COSTUME,
} UGCCostumeOverrideType;

/// A single override
AUTO_STRUCT;
typedef struct UGCCostumeOverride {
	UGCCostumeOverrideType type;		AST(NAME("Type"))
	const char* astrName;				AST(NAME("Name") POOL_STRING)
	int colorIndex;						AST(NAME("ColorIndex"))
	
	const char* strValue;				AST(NAME("StrValue"))
	int iValue;							AST(NAME("IntValue"))
	PlayerCostume* entireCostume;		AST(NAME("EntireCostume"))
} UGCCostumeOverride;
extern ParseTable parse_UGCCostumeOverride[];
#define TYPE_parse_UGCCostumeOverride UGCCostumeOverride

typedef struct UGCBoundingVolume {
	Vec3 center;
	Vec3 extents[2];
	float rot;
} UGCBoundingVolume;

//////////////////////////////////////////////////////////////////////
/// Backdrops

AUTO_STRUCT;
typedef struct UGCInteriorLightingProps
{
	GroupDefRef child_light;					AST(EMBEDDED_FLAT)	
	F32 inner_angle;							AST(NAME("InnerAngle"))
	F32 outer_angle;							AST(NAME("OuterAngle"))
	U8 no_lights : 1;							AST(NAME("NoLights"))
} UGCInteriorLightingProps;

extern const char* g_UGCMissionName;
extern const char* g_UGCMissionNameCompleted;

//// Functions

void ugcLoadDictionaries( void );


// Validation (UGCValidateCommon.c)

void ugcValidateProject( UGCProjectData* ugcProj );
void ugcValidateSeries( const UGCProjectSeries* ugcSeries );
bool ugcValidateErrorfIfStatusHasErrors( UGCRuntimeStatus* status );
UGCBudgetValidateState ugcValidateBudgets( UGCProjectData* ugcProj );

// Fixup (UGCFixupCommon.c)

void ugcEditorFixupProjectData(UGCProjectData* ugcProj, int* out_numDialogsDeleted, int* out_numCostumesReset, int* out_numObjectivesReset, int* out_fixupFlags);
void ugcEditorFixupMaps(UGCProjectData* ugcProj);
void ugcEditorFixupObjectivesComponentMapNames(UGCProjectData *ugcProj, UGCMissionObjective** objectives, int* numObjectivesReset);
int ugcProjectFixupDeprecated( UGCProjectData* ugcProj, bool fixup );
int ugcFixupGetIDFromName(const char* pOldName);	// Get a component ID from an old-style name.

// Get the current defaults
UGCPerProjectDefaults *ugcGetDefaults();
int ugcGetAllegianceDefaultsIndex( const UGCProjectData* ugcProj );
UGCPerAllegianceDefaults* ugcGetAllegianceDefaults( const UGCProjectData* data );
const char* ugcGetDefaultMapName( const UGCProjectData* data );
UGCProjectBudget *ugcFindBudget(UGCBudgetType type, UGCComponentType component_type);
UGCDialogStyle ugcDefaultsDialogStyle(void);
bool ugcDefaultsMapTransitionsSpecifyDoor(void);
bool ugcDefaultsSingleMissionEnabled(void);
bool ugcDefaultsIsSeriesEditorEnabled(void);
bool ugcDefaultsIsItemEditorEnabled(void);
bool ugcDefaultsIsMapLinkIncludeTeammatesEnabled(void);
bool ugcDefaultsIsColoredPromptButtonsEnabled(void);
bool ugcDefaultsIsOverworldMapLinkAllowed(void);
bool ugcDefaultsIsPathNodesEnabled( void );
bool ugcDefaultsSearchFiltersByPlayerLevel(void);
bool ugcDefaultsMapLinkSkipCompletedMaps(void);
bool ugcDefaultsPreviewImagesAndOverworldMapEnabled(void);
bool ugcDefaultsMissionReturnEnabled(void);
UGCCostumeEditorStyle ugcDefaultsCostumeEditorStyle(void);
UGCCostumeSkeletonSlotDef* ugcDefaultsCostumeSkeletonDef( const char* skeletonName );
UGCCostumeRegionDef* ugcDefaultsCostumeRegionDef( const char* regionName );
UGCCheckedAttribDef* ugcDefaultsCheckedAttribDef( const char* attribName );
const char* ugcDefaultsFallbackPromptText( void );
const char* ugcDefaultsGetAllegianceRestriction( void );
bool ugcDefaultsAuthorAllowsFeaturedBlocksEditing( void );
WorldVariableDef** ugcGetDefaultVariableDefs(void);
const UGCKillCreditLimit* ugcDefaultsGetKillCreditLimit( void );
MissionPlayType ugcDefaultsGetNonCombatType( void );
UGCSpecialComponentDef* ugcDefaultsSpecialComponentDef( const char* specialName );


bool ugcDefaultsIsFinalRewardBoxSupported(void); // This causes a single reward box (or Super Chest) to be placed on the last map of a UGC project
const char *ugcDefaultsGetRewardBoxObjlib(void); // If a final reward box is supported, get the Object Library piece to use for the internal map reward box
const char *ugcDefaultsGetRewardBoxContact(void); // If a final reward box is supported, get the Contact name to use for the reward box
const char *ugcDefaultsGetRewardBoxReward(void); // If a final reward box is supported, get the Reward Table name to use for the reward box
const char *ugcDefaultsGetRewardBoxDisplay(void); // If a final reward box is supported, get the Display string to use for the reward box objective and waypoint
const char *ugcDefaultsGetRewardBoxLootedDisplay(void); // If a final reward box is supported, get the Display string to use for the floater text once the reward box has been looted

// Are these options enabled in the current project?
bool ugcIsSpaceEnabled(void);
bool ugcIsAllegianceEnabled(void);
bool ugcIsFixedLevelEnabled(void);
bool ugcIsCheckedAttribEnabled(void);
bool ugcIsInteriorEditorEnabled(void);
bool ugcIsDialogWithObjectEnabled(void);
bool ugcIsMissionItemsEnabled(void);
bool ugcIsChatAL0(void);
bool ugcMapCanCustomizeBackdrop(UGCMapType map_type);
bool ugcIsLegacyHeightSnapEnabled();

// Utility function to load a UGC project from disk
UGCProjectData *ugcProjectLoadFromDir( const char *dir );

// Returns a sensible default start spawn name for a given map, using links,
// doors, and component information.

const char *ugcProjectGetMapStartSpawnTemp(UGCProjectData *data, UGCMap *map);
ContainerID ugcProjectContainerID(UGCProjectData *data);
MissionPlayType ugcProjectPlayType( UGCProjectData* ugcProj );
char* ugcAllocSMFString( const char* str, bool allowComplex );

// Map function
UGCMap* ugcMapFindByName( const UGCProjectData* data, const char* mapName );
#define ugcMapGetType(map) ugcMapGetTypeEx(map, false)
UGCMapType ugcMapGetTypeEx(const UGCMap *map, bool noPrefabType);
UGCMapType ugcMapGetPrefabType(const char *map_name, bool noPrefabType);
UGCMapType ugcWorldRegionGetPrefabType(const ZoneMapInfo* zminfo, const WorldRegion* region, bool noPrefabType);
const char* ugcMapGetDisplayName( UGCProjectData* ugcProj, const char* mapName );
const char* ugcMapMissionName( UGCProjectData* data, UGCMissionObjective* objective );
const char* ugcMapMissionLinkName( UGCProjectData* data, UGCMissionMapLink* link );
const char* ugcMapMissionLinkReturnText( const UGCMissionMapLink* link );
bool ugcMapIsGround(UGCMap *map);
bool ugcMapTypeIsGround(UGCMapType type);

// Costume functions
UGCCostume *ugcCostumeFindByName( UGCProjectData* projData, const char *costume_name );
bool ugcCostumeSpecifierExists( UGCProjectData* projData, const char* costume_name );
const char* ugcCostumeSpecifierGetDisplayName( const UGCProjectData* ugcProj, const char* costumeName );
PCPart* ugcCostumeMetadataGetPartByBone( UGCCostumeMetadata* pCostume, const char* astrBoneName );

PlayerCostume* ugcCostumeGeneratePlayerCostume( UGCCostume* costume, UGCCostumeOverride* override, const char* namespace );
void ugcCostumeRevertToPreset( UGCCostume* ugcCostume, const char* presetName );
bool ugcCostumeFindBodyScaleDef( const char* skelName, const char* name, bool allowBasic, bool allowAdvanced );
bool ugcCostumeFindScaleInfoDef( const char* skelName, const char* name, bool allowBasic, bool allowAdvanced );
UGCCostumeNWPartDef* ugcCostumeFindPartDef( const char* skelName, const char* name, bool allowBasic, bool allowAdvanced );
UGCCostumeSlotDef* ugcCostumeFindSlotDef( const char* skelName, const char* astrName );
UGCCostumeScale* ugcCostumeFindBodyScale( UGCCostume* ugcCostume, const char* scaleName );
UGCCostumeScale* ugcCostumeFindScaleInfo( UGCCostume* ugcCostume, const char* scaleName );
UGCCostumePart* ugcCostumeFindPart( UGCCostume* ugcCostume, const char* boneName );
UGCCostumeSlot* ugcCostumeFindSlot( UGCCostume* costume, const char* astrName );

// Item functions
UGCItem *ugcItemFindByName( UGCProjectData* projData, const char *item_name );
const char* ugcItemGetIconName( UGCItem* item );

// Search function
extern bool bUGCResourceValidation;
ResourceSearchResult *ugcProjectSearchRequest( UGCProjectData* projData, ResourceSearchRequest *request, UGCMapType map_type);
ResourceSearchResult* ugcResourceSearchRequest(const ResourceSearchRequest* request);
bool ugcProjectFilterAllegiance(const UGCProjectData *data, const char *value, int object_id);

// Component functions

bool ugcComponentCanReparent(UGCComponentType type);
bool ugcComponentHasParent(const UGCComponentList *list, const UGCComponent *component, U32 parent_id);
const char *ugcComponentGetLogicalNameTemp(const UGCComponent *component);
U32 ugcComponentNameGetID(const char* name);
U32 ugcPromptNameGetID(const char* name);
bool ugcComponentIsOnMap( const UGCComponent* component, const char* mapName, bool includeUnplaced );
UGCMapType ugcComponentMapType( const UGCProjectData* ugcProj, const UGCComponent* component );
UGCComponent *ugcComponentFindMapSpawn( UGCComponentList *list, const char *pcMapName );
const char* ugcCostumeHandleString( UGCProjectData* ugcProj, const char* name );

// Returns the last reward box component in the list. There is assumed to be only one. This is ensured in fixup.
UGCComponent *ugcComponentFindFinalRewardBox( UGCComponentList *list );

bool ugcComponentLayoutCompatible(UGCComponentType component_type, UGCMapType map_type);
bool componentStateCanBecomeHidden(UGCComponentType type);
bool componentStateCanHaveCheckedAttrib(UGCComponentType type);
bool ugcComponentStateCanBeEdited(UGCProjectData* ugcProj, UGCComponent* component);
F32 ugcComponentGetWorldHeight(const UGCComponent *component, const UGCComponentList *components);
bool ugcComponentAllowFreePlacement(UGCComponentType type);
bool ugcComponentAllow3DRotation(UGCComponentType type);
UGCComponentValidPosition **ugcComponentFindValidPositions(const UGCProjectData *ugcProj, UGCBacklinkTable* pBacklinkTable, const UGCComponent *component, const Vec3 world_pos);
bool ugcComponentIsValidPosition(const UGCProjectData *ugcProj, UGCBacklinkTable* pBacklinkTable, const UGCComponent *component, const Vec3 world_pos, U32 *room_levels, bool snapping, F32 snap_xz, F32 snap_y,
								UGCComponentValidPosition *out_position);
void ugcComponentSetValidPosition(UGCProjectData *ugcProj, UGCComponent *component, UGCComponentValidPosition *position);
void ugcComponentGetValidPosition(UGCComponentValidPosition* out_position, UGCProjectData* ugcProj, UGCComponent* component);
const char *ugcComponentTypeGetName(UGCComponentType eType);
const char *ugcComponentTypeGetDisplayName(UGCComponentType eType, bool bShort);
void ugcComponentGetDisplayNameSafe(char* out, int out_size, const UGCProjectData* ugcProj, const UGCComponent* component, bool bForGeneration);
#define ugcComponentGetDisplayName(buffer,ugcProj,component,bForGeneration) ugcComponentGetDisplayNameSafe(SAFESTR(buffer),ugcProj,component,bForGeneration)
void ugcComponentGetDisplayNameDefaultSafe(char* out, int out_size, const UGCProjectData* ugcProj, const UGCComponent* component, bool bForGeneration);
#define ugcComponentGetDisplayNameDefault(buffer,ugcProj,component,bForGeneration) ugcComponentGetDisplayNameDefaultSafe(SAFESTR(buffer),ugcProj,component,bForGeneration)

bool ugcComponentCalcBounds( UGCComponentList* list, UGCComponent* component, Vec3 out_boundsMin, Vec3 out_boundsMax );
bool ugcComponentCalcBoundsForObjLib( int objlibId, Vec3 out_boundsMin, Vec3 out_boundsMax, float *pOut_Radius);
UGCComponent *ugcComponentFindByID(const UGCComponentList *list, U32 id);
UGCComponent *ugcComponentFindByLogicalName(UGCComponentList *list, const char *name);
UGCComponent *ugcComponentFindDefaultPromptForID(UGCComponentList *list, U32 id );
UGCComponent *ugcComponentFindPromptForID( UGCComponentList* list, U32 id );
UGCComponent** ugcComponentFindPopupPromptsForObjectiveStart(UGCComponentList *list, U32 id);
UGCComponent** ugcComponentFindPopupPromptsForObjectiveComplete(UGCComponentList *list, U32 id);
UGCComponent** ugcComponentFindPopupPromptsForMissionStart(UGCComponentList *list);
UGCComponent** ugcComponentFindPopupPromptsForWhenInDialog(UGCComponentList *list, const UGCComponent* component);
UGCComponent *ugcComponentFindPromptByContactAndObjective(UGCComponentList *list, U32 contactID, U32 objectiveID );
bool ugcComponentIsUsedInObjectives( UGCComponentList *list, UGCComponent* component, UGCMissionObjective** objectives );
bool ugcComponentIsUsedInLinks( UGCComponent* component, UGCMission* mission );
UGCWhenType ugcComponentStartWhenType( const UGCComponent *component );
UGCDialogTreePrompt* ugcDialogTreeGetPrompt( UGCDialogTreeBlock* block, int promptID );
bool ugcComponentHasPatrol( const UGCComponent* component, WorldPatrolRouteType* out_routeType );

UGCComponent *ugcComponentOpCreate(UGCProjectData *data, UGCComponentType type, U32 parentID);
void ugcComponentOpSetParent(UGCProjectData *data, UGCComponent *component, U32 parentID);
UGCComponent *ugcComponentOpDuplicate(UGCProjectData *data, UGCComponent *src, U32 parentID);
UGCComponent *ugcComponentOpClone(UGCProjectData *data, UGCComponent* component);
void ugcComponentOpReset(UGCProjectData *data, UGCComponent *component, UGCMapType map_type, bool keep_object);

UGCComponent* ugcComponentOpExternalObjectFind(UGCProjectData* data, const char* zmapName, const char* logicalName);
UGCComponent* ugcComponentOpExternalObjectFindOrCreate(UGCProjectData* data, const char* zmapName, const char* logicalName);

bool ugcComponentOpDelete(UGCProjectData *data, UGCComponent *component, bool force); // If force is not set, only deletes non-mission components
void ugcComponentOpDeleteChildren(UGCProjectData *data, UGCComponent *component, bool force);
void ugcComponentOpSetPlacement(UGCProjectData *data, UGCComponent *component, UGCMap *map, U32 room_id);
UGCComponent **ugcComponentFindPlacements(UGCComponentList *list, const char *map_name, U32 room_id);
const char* ugcComponentGroundCostumeName( UGCProjectData* ugcProj, UGCComponent* component );
U32 ugcComponentMakeUniqueID(UGCComponentList *list);

bool ugcComponentCreateClusterChildren( UGCProjectData* ugcProj, UGCComponent* component );
bool ugcComponentCreateTrapChildren( UGCProjectData* ugcProj, UGCComponent* component );
bool ugcComponentCreateActorChildren( UGCProjectData* ugcProj, UGCComponent* component );

/// Backlink Table.  Allows us to quiclky get deduced data for optimized codepaths.
///
/// Make sure you call destroy after calling refresh.

// Create and update the BacklinkTable
void ugcBacklinkTableRefresh( UGCProjectData* ugcProj, UGCBacklinkTable** ppTable );
// Clean up the BacklinkCache
void ugcBacklinkTableDestroy( UGCBacklinkTable** ppTable );
// Find if there is any triggered components
bool ugcBacklinkTableFindTrigger( UGCBacklinkTable* pTable, U32 triggeringID, U32 triggeringPromptID );
// List out the triggered components
void ugcBacklinkTableFindAllTriggers( UGCProjectData* ugcProj, UGCBacklinkTable* pTable, U32 triggeringID, int triggeringPromptID, UGCComponent ***out_components );
void ugcBacklinkTableFindAllTriggersConst( const UGCProjectData* ugcProj, UGCBacklinkTable* pTable, U32 triggeringID, int triggeringPromptID, const UGCComponent ***out_components );
// Get all the components on a map
void ugcBacklinkTableGetMapComponents( const UGCProjectData* ugcProj, UGCBacklinkTable* pTable, const char* mapName, const UGCComponent*** out_components );
// Get all the dialog trees for this component
void ugcBacklinkTableGetDialogTreesForComponent( const UGCProjectData* ugcProj, const UGCBacklinkTable* pTable, U32 contactID, const UGCComponent*** out_components );;

// Patrol routes
UGCComponentPatrolPath *ugcComponentGetPatrolPath(const UGCProjectData *ugcProj, const UGCComponent *component, const UGCComponentPatrolPoint** positionOverrides);

PlayerCostume* ugcPlayerCostumeFromString( UGCProjectData* data, const char* costumeName );

UGCTrapProperties *ugcTrapGetProperties(GroupDef *def);
UGCComponent* ugcTrapFindEmitter( UGCProjectData* ugcProj, UGCComponent* trap );

UGCAssetTagCategory *ugcSkyGetSlotCategory(UGCMapType map_type);

UGCFSMVar* ugcComponentBehaviorGetFSMVar(UGCComponent *component, const char* name);

extern int ugcPerfDebug;
#define ugcLoadStart_printf(fmt, ...) do { if( ugcPerfDebug ) loadstart_printf(fmt, __VA_ARGS__); } while(0)
#define ugcLoadEnd_printf(fmt, ...) do { if( ugcPerfDebug ) loadend_printf(fmt, __VA_ARGS__); } while(0)

const char* ugcDialogTreePromptCameraPos( const UGCDialogTreePrompt* prompt );

bool ugcTetheringAllowed();

bool ugcComponentSupportsNormalSnapping(UGCComponent* component);
bool ugcComponentPlacementNormalSnappingActive(UGCComponentPlacement* placement);

// Whether or not this project (STO, NW) requires UGC messages to be translated
bool ugcDefaultsRequireTranslatedMessages();


void ugcFormatMessageKey(char **estrResult, const char *pcKey, ...);
void ugcConcatMessageKey(char **estrResult, const char *pcKey, ...);

// Wrapper around FormatMessageKeyDefault that clears the result so it can be reused in one function a lot.
// This also gives UGC a hook to change Errorf behavior when a Message is untranslated. STO will not care, NW will care.
void ugcFormatMessageKeyDefault(char **estrResult, const char *pcKey, const char *pcDefault, ...);

bool ugcGetBoundingVolumeFromPoints(UGCBoundingVolume* out_boundingVolume, F32 *points);


//// Resource Infos -- external resources info for UGC
#define UGC_DICTIONARY_RESOURCE_INFO "UGCResourceInfo"

AUTO_STRUCT;
typedef struct WorldUGCActorCostumeProperties
{
	REF_TO(PlayerCostume)	hCostumeRef;	AST(RESOURCEDICT(PlayerCostume) NAME(Costume))
	char* pcCostumeName;
} WorldUGCActorCostumeProperties;
extern ParseTable parse_WorldUGCActorCostumeProperties[];
#define TYPE_parse_WorldUGCActorCostumeProperties WorldUGCActorCostumeProperties

AUTO_STRUCT;
typedef struct WorldUGCActorProperties
{
	DisplayMessage displayNameMsg;					AST(STRUCT(parse_DisplayMessage))
	DisplayMessage groupDisplayNameMsg;				AST(STRUCT(parse_DisplayMessage))
	const char *pcRankName;							AST(POOL_STRING)
	char* pcClass;
	WorldUGCActorCostumeProperties **eaCostumes;	AST(NAME(Costume))
} WorldUGCActorProperties;
extern ParseTable parse_WorldUGCActorProperties[];
#define TYPE_parse_WorldUGCActorProperties WorldUGCActorProperties

// Properties pertaining to Object Library pieces (GroupDefs)
AUTO_STRUCT;
typedef struct WorldUGCGroupDefProperties
{
	bool bRoomDoorsEverywhere;											AST( NAME(RoomDoorsEverywhere) )
	char* strClickableName;												AST( NAME(ClickableName))
	WorldUGCActorProperties **eaEncounterActors;						AST( NAME(EncounterActor) NAME(ActorProperties) )
	int iCost;															AST( NAME(Cost) )

	// Default setting for Snap Normal when this GroupDef is used as a Component in an exterior prefab and the component is snapped to Terrain or Geometry.
	// Useful for things like fences and grass, which should follow the contours of the terrain, by default.
	bool bDefaultSnapNormal;											AST( NAME(DefaultSnapNormal) )
} WorldUGCGroupDefProperties;
extern ParseTable parse_WorldUGCGroupDefProperties[];
#define TYPE_parse_WorldUGCGroupDefProperties WorldUGCGroupDefProperties

AUTO_STRUCT;
typedef struct WorldUGCFSMProperties
{
	bool bHasPatrol;							AST(NAME("HasPatrol"))
	WorldPatrolRouteType ePatrolType;			AST(NAME("PatrolType"))
	UGCFSMExternVarDef** eaExternVars;			AST(NAME("ExternVar"))
} WorldUGCFSMProperties;
extern ParseTable parse_WorldUGCFSMProperties[];
#define TYPE_parse_WorldUGCFSMProperties WorldUGCFSMProperties

AUTO_STRUCT;
typedef struct WorldUGCProperties
{
	DisplayMessage dVisibleName;										AST( NAME(VisibleName) STRUCT(parse_DisplayMessage) USERFLAG(TOK_USEROPTIONBIT_1) )
	DisplayMessage dDescription;										AST( NAME(Description) STRUCT(parse_DisplayMessage) USERFLAG(TOK_USEROPTIONBIT_1) )
	DisplayMessage dDefaultName;										AST( NAME(DefaultName) STRUCT(parse_DisplayMessage) USERFLAG(TOK_USEROPTIONBIT_1) )
	const char *pchImageOverride;										AST( NAME(ImageOverride) RESOURCEDICT(Texture) POOL_STRING)
	bool bNoDescription;												AST( NAME(NoDescription) BOOLFLAG )

	int iSortPriority;													AST( NAME(SortPriority) )

	WorldUGCRestrictionProperties restrictionProps;						AST( EMBEDDED_FLAT )

	F32 fMapDefaultHeight;												AST( NAME(MapDefaultHeight) NAME(DefaultHeight) )
	bool bMapOnlyPlatformsAreLegal;										AST( NAME(MapOnlyPlatformsAreLegal) )
	
	WorldUGCGroupDefProperties groupDefProps;							AST( EMBEDDED_FLAT )
	WorldUGCFSMProperties fsmProps;										AST( EMBEDDED_FLAT )
} WorldUGCProperties;
extern ParseTable parse_WorldUGCProperties[];
#define TYPE_parse_WorldUGCProperties WorldUGCProperties

AUTO_STRUCT AST_IGNORE_STRUCT(FSMExternVar);
typedef struct UGCResourceInfo
{
	const char* pcName;							AST(KEY POOL_STRING)
	const char* pcFilename;						AST(CURRENTFILE)
	ResourceInfo* pResInfo;						AST(NAME("ResInfo"))
	WorldUGCProperties* pUGCProperties;			AST(NAME("UGCProperties"))

	// Info for the fsms
	UGCFSMMetadata* pFSMMetadata;				AST(NAME("FSMMetadata"))

	// Info for PlayerCostumes
	UGCCostumeMetadata* pCostumeMetadata;		AST(NAME("CostumeMetadata"))

	// Info for ObjectLibrary
	UGCGroupDefMetadata* pDefMetadata;			AST(NAME("DefMetadata"))
} UGCResourceInfo;
extern ParseTable parse_UGCResourceInfo[];
#define TYPE_parse_UGCResourceInfo UGCResourceInfo

extern bool ugcResourceInfosPopulated;

//// TagTypes.  Resources are grouped in the UI using tags.

AUTO_STRUCT;
typedef struct UGCAssetTag
{
	char *pcName;								AST(STRUCTPARAM NAME("Name"))
	REF_TO(Message) hDisplayName;				AST(STRUCTPARAM NAME("DisplayName"))
} UGCAssetTag;
extern ParseTable parse_UGCAssetTag[];
#define TYPE_parse_UGCAssetTag UGCAssetTag

AUTO_STRUCT;
typedef struct UGCAssetTagCategory
{
	char *pcName;								AST(STRUCTPARAM)
	REF_TO(Message) hDisplayName;				AST(NAME("DisplayName"))
	UGCAssetTag **eaTags;						AST(NAME("Tag"))
	bool bIsHidden;								AST(NAME("IsHidden"))
} UGCAssetTagCategory;
extern ParseTable parse_UGCAssetTagCategory[];
#define TYPE_parse_UGCAssetTagCategory UGCAssetTagCategory

AUTO_STRUCT AST_IGNORE("SpaceModePreview");
typedef struct UGCAssetTagType
{
	const char *pcName;							AST(KEY POOL_STRING STRUCTPARAM)
	const char *pcFilename;						AST(CURRENTFILE)
	char *pcDictName;							AST(NAME("Dictionary"))
	bool bFilterType;							AST(NAME("FilterByTypeName"))
	bool bDrawEditorOnly;						AST(NAME("DrawEditorOnly"))
	bool bEnableIconGridView;					AST(NAME("EnableIconGridView"))
	bool bPreferIconGridView;					AST(NAME("PreferIconGridView"))
	bool bIsYTranslate;							AST(NAME("IsYTranslate"))
	UGCAssetTagCategory **eaCategories;			AST(NAME("TagCategory"))
} UGCAssetTagType;
extern ParseTable parse_UGCAssetTagType[];
#define TYPE_parse_UGCAssetTagType UGCAssetTagType

extern DictionaryHandle g_UGCTagTypeDictionary;

void ugcLoadTagTypeLibrary(void);

UGCFSMMetadata* ugcResourceGetFSMMetadata( const char *objName );
UGCCostumeMetadata* ugcResourceGetCostumeMetadata( const char* objName );
UGCGroupDefMetadata* ugcResourceGetGroupDefMetadataInt( int objName );
UGCFSMExternVar* ugcResourceGetFSMExternVar( const char* objName, const char* varName );

//// Resource

//// Utility

bool ugcHasTagType(const char *tags, const char *dict_name, const char *tag_type);
bool ugcHasTag(const char *tags, const char *tag);
bool ugcGetTagValue(const char *tags, const char *category, char *value, size_t value_size);
void ugcMapComponentValidBounds( Vec3 out_min, Vec3 out_max, const UGCProjectData* ugcProj, UGCBacklinkTable* pBacklinkTable, const UGCMap* map, const UGCComponent* component );

//// Platforms

HeightMapExcludeGrid *ugcMapEditorGenerateExclusionGrid(UGCMapPlatformData *platform_data);
void ugcZoneMapRoomGetPlatforms(int room_id, int room_level, ExclusionVolumeGroup ***out_volumes);

// Special name handling for room doors

const char *ugcIntLayoutGetRoomName(U32 id);
const char *ugcIntLayoutDoorGetClickyLogicalName(const char *room_name, const char *door_name, const char *layout_name);
const char *ugcIntLayoutDoorGetSpawnLogicalName(const char *room_name, const char *door_name, const char *layout_name);
// Get the spawn point or region name for a prefab Cryptic map
bool ugcGetZoneMapSpawnPoint(const char *map_name, Vec3 out_spawn_pos, Quat* pOut_spawn_orientation);
ZoneMapEncounterRegionInfo *ugcGetZoneMapDefaultRegion(const char *map_name);
ZoneMapEncounterRoomInfo *ugcGetZoneMapPlayableVolume(const char *map_name);
SecondaryZoneMap **ugcGetZoneMapSecondaryMaps(const char *map_name);
void ugcGetZoneMapPlaceableBounds( Vec3 out_min, Vec3 out_max, const char* mapName, bool restrictToPlayable );

// Platform infos
void ugcPlatformDictionaryLoad(void);
