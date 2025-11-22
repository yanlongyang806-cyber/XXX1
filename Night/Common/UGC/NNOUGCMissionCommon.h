//// UGC mission common routines
////
//// Code used to validate & generate UGC missions on both client & server.

#pragma once
GCC_SYSTEM

typedef enum UGCComponentType UGCComponentType;
typedef struct InteractableOverride InteractableOverride;
typedef struct StashTableImp* StashTable;
typedef struct UGCBacklinkTable UGCBacklinkTable;
typedef struct UGCComponent UGCComponent;
typedef struct UGCComponentList UGCComponentList;
typedef struct UGCDialogTreeBlock UGCDialogTreeBlock;
typedef struct UGCDialogTreePrompt UGCDialogTreePrompt;
typedef struct UGCGenesisMissionChallenge UGCGenesisMissionChallenge;
typedef struct UGCGenesisMissionDescription UGCGenesisMissionDescription;
typedef struct UGCGenesisMissionObjective UGCGenesisMissionObjective;
typedef struct UGCGenesisMissionPrompt UGCGenesisMissionPrompt;
typedef struct UGCGenesisMissionPromptBlock UGCGenesisMissionPromptBlock;
typedef struct UGCInteractProperties UGCInteractProperties;
typedef struct UGCMap UGCMap;
typedef struct UGCMission UGCMission;
typedef struct UGCMissionMapLink UGCMissionMapLink;
typedef struct UGCMissionObjective UGCMissionObjective;
typedef struct UGCPortalClickableInfo UGCPortalClickableInfo;
typedef struct UGCProjectData UGCProjectData;

#define UGC_TEXT_SINGLE_LINE_MAX_LENGTH 100
#define UGC_TEXT_MULTI_LINE_MAX_LENGTH 1000
#define UGC_OBJECTIVE_MAX_DEPTH 2

AUTO_STRUCT;
typedef struct UGCMapTransitionInfo
{
	U32 objectiveID;			AST(NAME("ObjectiveID"))
	char* prevMapName;			AST(NAME("PrevMapName"))
	bool prevIsInternal;		AST(NAME("PrevIsInternal"))
	U32 *mapObjectiveIDs;			AST(NAME("MapObjectiveIDs"))
} UGCMapTransitionInfo;
extern ParseTable parse_UGCMapTransitionInfo[];
#define TYPE_parse_UGCMapTransitionInfo UGCMapTransitionInfo

AUTO_STRUCT;
typedef struct UGCMapInfo
{
	char* mapName;				AST(NAME("MapName"))
	bool isInternal;			AST(NAME("IsInternal"))
} UGCMapInfo;
extern ParseTable parse_UGCMapInfo[];
#define TYPE_parse_UGCMapInfo UGCMapInfo

/// Mission generation
void ugcMissionObjectivesCount( UGCProjectData* ugcProj, int* out_maxDepth, int* out_numObjectives );
void ugcMissionTransmogrifyObjectives( UGCProjectData* ugcProj, UGCMissionObjective** objectives, const char* astrTargetMapName, bool createTmogCrypticMap, UGCMissionObjective*** out_peaTmogObjectives );

InteractableOverride* ugcComponentGenerateInteractOverride( UGCProjectData* proj_data, UGCComponent* component, UGCMissionObjective** objectives );
UGCGenesisMissionPrompt* ugcComponentGeneratePromptMaybe( UGCComponent* prompt, UGCProjectData* ugcProj, UGCMissionObjective** objectives, const char* mapName, UGCGenesisMissionObjective*** out_fsmObjectives );
void ugcGeneratePromptBlock( UGCGenesisMissionPromptBlock* out_block, UGCProjectData* ugcProj, UGCComponent* contact, const char* promptName, const char* name, bool initialBlockShouldBeWholePrompt,
							 U32 promptID, const char* nextBlockName, UGCDialogTreePrompt* block, UGCMissionObjective** objectives, UGCComponentList* components,
							 UGCGenesisMissionObjective*** out_extraObjectives );

void ugcMissionGetComponentPortalProperties( UGCProjectData *ugcProj, UGCComponent *pComponent, const char* mission_name, bool is_start,
										   const char **out_map_name, UGCPortalClickableInfo **out_clickable,
										   const char **out_spawn, char **out_warp_text);
const char *ugcMissionGetPortalClickableLogicalNameTemp(UGCPortalClickableInfo *clickable);
bool ugcMissionGetPortalShouldAutoExecute(UGCPortalClickableInfo* clickable);
#define ugcMissionGetMapTransitions(ugcProj,missionObjectives) ugcMissionGetMapTransitions_dbg(ugcProj,missionObjectives MEM_DBG_PARMS_INIT)
UGCMapTransitionInfo** ugcMissionGetMapTransitions_dbg( const UGCProjectData* ugcProj, const UGCMissionObjective** missionObjectives MEM_DBG_PARMS );
UGCMapTransitionInfo* ugcMissionFindTransitionForObjective( UGCMapTransitionInfo** transitions, U32 objectiveID );

/// Objective querying
UGCMissionObjective* ugcObjectiveFind( UGCMissionObjective** objectives, U32 id );
UGCMissionObjective* ugcObjectiveFindByLogicalName( UGCMissionObjective** objectives, const char* logicalName );
UGCMissionObjective* ugcObjectiveFindPrevious( UGCMissionObjective** objectives, U32 id );
UGCMissionObjective** ugcObjectiveFindPath( UGCMissionObjective** objectives, U32 id );
UGCMissionObjective*** ugcObjectiveFindParentEA( UGCMissionObjective*** peaObjectives, U32 id, int* out_index );
UGCMissionObjective* ugcObjectiveFindMapObjective( UGCMissionObjective** objectives, const char* name );
UGCMissionObjective* ugcObjectiveFindComponent( UGCMissionObjective** objectives, U32 componentID );
UGCMissionObjective* ugcObjectiveFindComponentRelated( UGCMissionObjective** objectives, UGCComponentList *componentList, U32 componentID);
const UGCMissionObjective* ugcObjectiveFindComponentRelatedUsingTableConst( const UGCProjectData* ugcProj, const UGCBacklinkTable* pBacklinkTable, U32 componentID);
UGCMissionObjective* ugcObjectiveFindOnMap( UGCMissionObjective** objectives, UGCComponentList* componentList, const char* map );
UGCMissionObjective* ugcObjectiveFindOnCrypticMap( UGCMissionObjective** objectives, UGCComponentList* componentList, const char* map );
const char* ugcObjectiveInternalMapName( UGCProjectData* ugcProj, UGCMissionObjective* objective );
const char* ugcObjectiveMapName( const UGCProjectData* ugcProj, const UGCMissionObjective* objective, bool* out_isInternal );
UGCMapInfo** ugcObjectiveFinishMapNames( UGCProjectData* ugcProj, UGCMissionObjective* objective );
bool ugcObjectiveIsCompleteMapOnMap( UGCMissionObjective* objective, char* mapName );
bool ugcObjectiveHasID( UGCMissionObjective* objective, U32 id );
bool ugcObjectiveHasIDRaw( UGCMissionObjective* objective, UserData rawID );
void ugcObjectiveGenerateIDs( UGCProjectData* ugcProj, U32* out_ids, int numIds );

/// Support for default text in mission -- as a special case, passing
/// NULL returns the default text
const char* ugcMissionObjectiveUIString( const UGCMissionObjective* objective );
const char* ugcMissionObjectiveLogicalNameTemp( const UGCMissionObjective* objective );
const char* ugcMissionObjectiveIDLogicalNameTemp( U32 id );
const char* ugcDialogTreePromptButtonText( const UGCDialogTreePrompt* prompt, int index );
const char* ugcDialogTreePromptStyle( const UGCDialogTreePrompt* prompt );
const char* ugcMapDisplayName( const UGCMap* map );

const char* ugcInteractPropsInteractText( const UGCInteractProperties* props );
const char* ugcInteractPropsInteractFailureText( const UGCInteractProperties* props );

// Finds s MissionMapLink that matches the given map transition
UGCMissionMapLink *ugcMissionFindLink( UGCMission* mission, UGCComponentList *com_list, const char* nextMap, const char* prevMap );
UGCMissionMapLink *ugcMissionFindLinkByObjectiveID( UGCProjectData* projData, U32 objectiveID, bool allMapObjectives );
UGCMissionMapLink *ugcMissionFindLinkByExitComponent( UGCProjectData* ugcProj, U32 componentID );
UGCMissionMapLink *ugcMissionFindLinkByMap( UGCProjectData* ugcProj, const char* map );
UGCMissionMapLink *ugcMissionFindLinkByExitMap( UGCProjectData* ugcProj, const char* map );
const char* ugcLinkButtonText( UGCMissionMapLink* link );

// For a link, keep walking backward to find the "Cryptic"->Project link to that map.
UGCMissionMapLink *ugcMissionFindCrypticSourceLink( UGCProjectData* ugcProj, UGCMissionMapLink* link );

// Returns the first component with the given type on the map
UGCComponent *ugcMissionGetDefaultComponentForMap(UGCProjectData* ugcProj, UGCComponentType type, const char *map_name);
