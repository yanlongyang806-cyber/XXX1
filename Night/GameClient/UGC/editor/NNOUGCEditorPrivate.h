/***************************************************************************



*
* This is the private UGC editor header file to include from inside the
* UGC editor system. Declarations go in here that are needed only by the
* UGC editor system itself and should not be included elsewhere.
*
* If you think you need to include this elsewhere, consider moving the
* declarations you need from here to UGCEditorMain.h and make sure the
* implementation of those functions are in UGCEditorMain.c (they probably
* already are).
***************************************************************************/

#pragma once
GCC_SYSTEM

#include "CombatEnums.h"
#include "EditLib.h"
#include "EditLibUIUtil.h"
#include "MultiEditField.h"
#include "ResourceDBUtils.h"
#include "StdTypes.h"
#include "UICore.h"

typedef enum UGCComponentType UGCComponentType;
typedef enum UGCInteriorSelectionType UGCInteriorSelectionType;
typedef enum UGCZeniPickerFilterType UGCZeniPickerFilterType;
typedef struct AllegianceDef AllegianceDef;
typedef struct MEField MEField;
typedef struct MEFieldContextEntry MEFieldContextEntry;
typedef struct NOCONST(UGCProjectReviews) NOCONST(UGCProjectReviews);
typedef struct PossibleUGCProjectInfo PossibleUGCProjectInfo;
typedef struct PossibleUGCProjectInfoReviews PossibleUGCProjectInfoReviews;
typedef struct UGCAssetLibraryRow UGCAssetLibraryRow;
typedef struct UGCBacklinkTable UGCBacklinkTable;
typedef struct UGCCheckedAttrib UGCCheckedAttrib;
typedef struct UGCCheckedAttribCombo UGCCheckedAttribCombo;
typedef struct UGCCheckedAttribDef UGCCheckedAttribDef;
typedef struct UGCComponent UGCComponent;
typedef struct UGCComponentList UGCComponentList;
typedef struct UGCCostume UGCCostume;
typedef struct UGCCostumeEditor UGCCostumeEditor;
typedef struct UGCDialogTreeHeadshotGroup UGCDialogTreeHeadshotGroup;
typedef struct UGCDialogTreePrompt UGCDialogTreePrompt;
typedef struct UGCEditorMenus UGCEditorMenus;
typedef struct UGCInteractProperties UGCInteractProperties;
typedef struct UGCItem UGCItem;
typedef struct UGCItemEditorDoc UGCItemEditorDoc;
typedef struct UGCMap UGCMap;
typedef struct UGCMapEditorDoc UGCMapEditorDoc;
typedef struct UGCMission UGCMission;
typedef struct UGCMissionDoc UGCMissionDoc;
typedef struct UGCMissionLibraryModel UGCMissionLibraryModel;
typedef struct UGCMissionMapLink UGCMissionMapLink;
typedef struct UGCMissionNodeGroup UGCMissionNodeGroup;
typedef struct UGCMissionObjective UGCMissionObjective;
typedef struct UGCProjectAutosaveData UGCProjectAutosaveData;
typedef struct UGCProjectData UGCProjectData;
typedef struct UGCProjectInfo UGCProjectInfo;
typedef struct UGCProjectPermission UGCProjectPermission;
typedef struct UGCProjectReviews UGCProjectReviews;
typedef struct UGCProjectStatusQueryInfo UGCProjectStatusQueryInfo;
typedef struct UGCRuntimeError UGCRuntimeError;
typedef struct UGCRuntimeErrorContext UGCRuntimeErrorContext;
typedef struct UGCRuntimeStatus UGCRuntimeStatus;
typedef struct UGCWhen UGCWhen;
typedef struct UIButton UIButton;
typedef struct UIComboBox UIComboBox;
typedef struct UIMenu UIMenu;
typedef struct UIMenuItem UIMenuItem;
typedef struct UISprite UISprite;
typedef struct UITextShortcutTab UITextShortcutTab;
typedef struct UITree UITree;
typedef struct WorldUGCRestrictionProperties WorldUGCRestrictionProperties;
typedef struct ZoneMapEncounterInfo ZoneMapEncounterInfo;
typedef struct ZoneMapEncounterObjectInfo ZoneMapEncounterObjectInfo;

typedef void (*UICellDrawFunc)(UIList *pList, UIListColumn *pColumn, UI_MY_ARGS, F32 z, CBox *pLogicalBox, S32 iRow, UserData pDrawData);
typedef bool (*UGCAssetLibraryCustomFilterFn)(UserData data, UGCAssetLibraryRow *row);
										  
// Enums
typedef enum UGCInteractPropertiesFlags
{
	UGCINPR_BASIC					 = (1 << 0),
	UGCINPR_CUSTOM_ANIM_AND_DURATION = (1 << 1),
	UGCINPR_CHECKED_ATTRIB_SKILLS	 = (1 << 2),
	UGCINPR_CHECKED_ATTRIB_ITEMS	 = (1 << 3),
} UGCInteractPropertiesFlags;

typedef enum UGCEditorType {
	UGCEDITOR_NONE,
	
	UGCEDITOR_PROJECT,
	UGCEDITOR_MISSION,
	UGCEDITOR_MAP,
	UGCEDITOR_DIALOG_TREE,
	UGCEDITOR_COSTUME,
	UGCEDITOR_ITEM,
} UGCEditorType;

AUTO_ENUM;
typedef enum UGCEditorViewMode
{
	UGC_VIEW_PROJECT_INFO,
	UGC_VIEW_NO_MAPS,
	UGC_VIEW_MAP_EDITOR,
	UGC_VIEW_MISSION,
	UGC_VIEW_NO_COSTUMES,
	UGC_VIEW_COSTUME,
	UGC_VIEW_ITEM,
	UGC_VIEW_NO_DIALOG_TREES,
	UGC_VIEW_DIALOG_TREE,

	// used only by the asset browser, not actually views
	UGC_AB_MAP_NODE,
} UGCEditorViewMode;
extern StaticDefineInt UGCEditorViewModeEnum[];

AUTO_STRUCT;
typedef struct UGCEditorView
{
	UGCEditorViewMode mode;			AST( NAME("Mode") )
	const char* name;				AST( NAME("Name") UNOWNED )

	// MJF TODO: Consider rolling this into the acitveMode/activeDoc
	// data.
	const char* playingEditorMap;	AST( NAME("PlayingEditorMap") POOL_STRING )
} UGCEditorView;
extern ParseTable parse_UGCEditorView[];
#define TYPE_parse_UGCEditorView UGCEditorView

typedef enum UGCEditorCloseOption
{
	UGC_EDITOR_CLOSE_NONE = 0,

	UGC_EDITOR_CLOSE_CANCEL = 1,	// canceling the operation
	UGC_EDITOR_CLOSE_PROJECT,		// closing the project and returning to the project chooser
	UGC_EDITOR_CLOSE_EDITOR,		// closing the editor and returning to character chooser
	UGC_EDITOR_QUIT,				// closing the entire program

	// Save bit indicates that a save should happen, then the close option should be queued
	UGC_EDITOR_SAVE = BIT(31)
} UGCEditorCloseOption;

AUTO_ENUM;
typedef enum UGCActionID
{
	UGC_ACTION_SAVE = 1,
	UGC_ACTION_PUBLISH,
	UGC_ACTION_WITHDRAW,
	UGC_ACTION_FREEZE,
	UGC_ACTION_CLOSE,
	UGC_ACTION_LOGOUT,
	UGC_ACTION_UNDO,
	UGC_ACTION_REDO,
	UGC_ACTION_CUT,
	UGC_ACTION_COPY,
	UGC_ACTION_PASTE,
	UGC_ACTION_DELETE,
	UGC_ACTION_DUPLICATE,
	UGC_ACTION_REPORT_BUG,
	UGC_ACTION_VIEW_EULA,
	UGC_ACTION_PLAY_MISSION,
	UGC_ACTION_DESELECT_ALL,
	UGC_ACTION_MOVE_UP,
	UGC_ACTION_MOVE_DOWN,
	UGC_ACTION_MOVE_LEFT,
	UGC_ACTION_MOVE_RIGHT,

	UGC_ACTION_MAP_CREATE,
	UGC_ACTION_MAP_IMPORT,
	UGC_ACTION_MAP_DELETE,
	UGC_ACTION_MAP_DUPLICATE,
	UGC_ACTION_MAP_EDIT_NAME,
	UGC_ACTION_MAP_EDIT_BACKDROP,
	UGC_ACTION_MAP_SEARCH_COMPONENT,
	UGC_ACTION_MAP_SET_LAYOUT_MODE,
	UGC_ACTION_MAP_SET_DETAIL_MODE,

	UGC_ACTION_COSTUME_CREATE,
	UGC_ACTION_COSTUME_DELETE,
	UGC_ACTION_COSTUME_DUPLICATE,
	UGC_ACTION_COSTUME_EDIT_NAME,
	UGC_ACTION_COSTUME_RANDOMIZE_ALL,
	UGC_ACTION_COSTUME_RANDOMIZE_REGION0,
	UGC_ACTION_COSTUME_RANDOMIZE_REGION1,
	UGC_ACTION_COSTUME_RANDOMIZE_REGION2,
	UGC_ACTION_COSTUME_RANDOMIZE_REGION3,
	UGC_ACTION_COSTUME_RANDOMIZE_REGION4,
	UGC_ACTION_COSTUME_RANDOMIZE_REGION5,
	UGC_ACTION_COSTUME_RANDOMIZE_REGION6,
	UGC_ACTION_COSTUME_RANDOMIZE_REGION7,
	UGC_ACTION_COSTUME_RANDOMIZE_REGION8,
	UGC_ACTION_COSTUME_RANDOMIZE_REGION9,

	UGC_ACTION_ITEM_CREATE,
	UGC_ACTION_ITEM_SORT_BY_NAME,
	UGC_ACTION_ITEM_SORT_BY_ICON,

	UGC_ACTION_DIALOG_PLAY,
	UGC_ACTION_DIALOG_DELETE,
	UGC_ACTION_DIALOG_PLAY_SELECTION,

	UGC_ACTION_SET_BACKDROP,
	UGC_ACTION_CREATE_MARKER,
	UGC_ACTION_CREATE_RESPAWN,
	UGC_ACTION_PLAY_MAP,
	UGC_ACTION_PLAY_MAP_FROM_LOCATION,
	UGC_ACTION_ROOM_CLEAR,
	UGC_ACTION_ROOM_POPULATE,

	UGC_ACTION_MISSION_PLAY_SELECTION,
	UGC_ACTION_MISSION_DELETE,
	UGC_ACTION_MISSION_CREATE_CLICKIE_OBJECTIVE,
	UGC_ACTION_MISSION_CREATE_KILL_OBJECTIVE,
	UGC_ACTION_MISSION_CREATE_DIALOG_OBJECTIVE,
	UGC_ACTION_MISSION_CREATE_DIALOG,
	UGC_ACTION_MISSION_CREATE_MARKER_OBJECTIVE,
	UGC_ACTION_MISSION_CREATE_UNLOCK_OBJECTIVE,

	UGC_ACTION_PLAYING_RESET_MAP,
	UGC_ACTION_PLAYING_KILL_TARGET,
	UGC_ACTION_PLAYING_FULL_HEAL,
	UGC_ACTION_PLAYING_TOGGLE_EDIT_MODE,
	UGC_ACTION_PLAYING_TRANSLATE_MODE,
	UGC_ACTION_PLAYING_ROTATE_MODE,
	UGC_ACTION_PLAYING_SLIDE_MODE,
	UGC_ACTION_PLAYING_DELETE_SELECTION,
	UGC_ACTION_PLAYING_UNDO,
	UGC_ACTION_PLAYING_REDO,
} UGCActionID;
extern StaticDefineInt UGCActionIDEnum[];


AUTO_ENUM;
typedef enum UGCMissionLibraryModelType {
	UGCMIMO_NEW_OBJECTIVE,
	UGCMIMO_NEW_UNLOCK_DOOR_OBJECTIVE,
	UGCMIMO_NEW_TALK_TO_OBJECTIVE,
	UGCMIMO_PROJECT_MAP,
	UGCMIMO_NEW_PROJECT_MAP,
	UGCMIMO_CRYPTIC_MAP,
} UGCMissionLibraryModelType;
extern StaticDefineInt UGCMissionLibraryModelTypeEnum[];

typedef struct UGCActionDescription
{
	UGCActionID eID;
	const char *strNameMessage;
	const char *strIcon;
	const char* strIconDisabled;
	bool bOpensDialog;
	const char *strDescriptionMessage;

	bool bIsCheckBox;
} UGCActionDescription;

typedef struct UGCEditorMenus
{
	UIPane* root_pane;

	// Child widgets
	UISprite* foundry_sprite;
	UIMenuBar *menu_bar;
	UISeparator* separator_after_menu_bar;
	UIButton* save_button;
	UIButton* publish_button;
	UIButton* errors_button;

	UISeparator* separator_before_window_buttons;
	UIButton* minimize_button;
	UIButton* restore_button;
	UIButton* close_button;

	// Menus that are dynamic:
	UIMenu *map_editor_menu;
	UIMenu *mission_editor_menu;

	// After the dynamic menu, so it needs to get removed too
	UIMenu *help_menu;

	// List of allegiance-specific menus
	UIMenu** allegiance_menus;

	UIMenuItem *item_adv_mode;

	UIMenuItem **refresh_list;
} UGCEditorMenus;

AUTO_STRUCT;
typedef struct UGCProjectInfoList
{
	UGCProjectInfo **defs;		AST(NAME(UGCProjectInfo))
} UGCProjectInfoList;
extern ParseTable parse_UGCProjectInfoList[];
#define TYPE_parse_UGCProjectInfoList UGCProjectInfoList

typedef struct UGCInteractPropertiesGroup {
	UGCInteractProperties *pStruct;
	UGCMapType eMapType;
	MEFieldChangeCallback pCallback;
	UserData pCallbackData;
} UGCInteractPropertiesGroup;

AUTO_ENUM;
typedef enum UGCEditorCopyType {
	UGC_COPY_NONE = 0,
	UGC_COPY_COMPONENT,
	UGC_CUT_COMPONENT,
} UGCEditorCopyType;

AUTO_STRUCT;
typedef struct UGCEditorCopyBuffer {
	UGCEditorCopyType eType;

	// For UGC_COPY_COMPONENT and UGC_CUT_COMPONENT:
	UGCMapType eSourceMapType;

	// For UGC_COPY_COMPONENT:
	UGCComponent **eaComponents;
	UGCComponent **eaChildComponents;
	UGCComponent **eaTimelineComponents;

	// For UGC_CUT_COMPONENT:
	U32 *eauComponentIDs;
} UGCEditorCopyBuffer;
extern ParseTable parse_UGCEditorCopyBuffer[];
#define TYPE_parse_UGCEditorCopyBuffer UGCEditorCopyBuffer

AUTO_STRUCT;
typedef struct UGCEditorDefaults
{
	// Costume Editor skies
	char* strCostumeEditorSky;				AST(NAME("CostumeEditorSky"))
	char* strSpaceCostumeEditorSky;			AST(NAME("SpaceCostumeEditorSky"))
	
	// UI shortcuts
	UITextShortcutTab** eaTextShortcutTabs;			AST(NAME("TextShortcutTab") LATEBIND)
} UGCEditorDefaults;
extern ParseTable parse_UGCEditorDefaults[];
#define TYPE_parse_UGCEditorDefaults UGCEditorDefaults

#define UGC_EDITOR_PREF_ADV_MODE "UGC.Editor.State.AdvancedMode"

#define UGC_PANE_TOP_BORDER 38
#define UGC_LIBRARY_PANE_WIDTH_SMALL 240
#define UGC_LIBRARY_PANE_WIDTH 365
#define UGC_UNPLACED_PANE_HEIGHT_MIN 100
#define UGC_UNPLACED_PANE_HEIGHT_MAX 260
#define UGC_PANE_BUTTON_HEIGHT 32
#define UGC_ROW_HEIGHT 25
#define UGC_ROW_ICON_SIZE 24
#define PROJECT_INFO_DATA_COLUMN 120

/// Control what mode the UGC editor's UI is in.
extern int g_ugcEditorMode;

extern StaticDefineInt UGCWhenTypeNormalEnum[];
extern StaticDefineInt UGCWhenTypeHideEnum[];
extern StaticDefineInt UGCWhenTypeContactHideEnum[];
extern StaticDefineInt UGCWhenTypeRoomDoorHideEnum[];
extern StaticDefineInt UGCWhenTypeDialogShowEnum[];
extern StaticDefineInt UGCWhenTypeObjectiveShowEnum[];
extern StaticDefineInt UGCWhenTypeObjectiveHideEnum[];
extern StaticDefineInt UGCWhenTypeObjectiveContactHideEnum[];
extern StaticDefineInt UGCWhenTypeObjectiveRoomDoorHideEnum[];
extern StaticDefineInt UGCWhenTypeNoMapEnum[];

UGCEditorDefaults *ugcEditorGetDefaults(void);

UGCMissionObjective* ugcEditorObjectiveCreate( UGCComponentType type, UGCMissionLibraryModelType modelType, U32 id, const char* mapName );
UGCMap* ugcEditorUninitializedMapCreate( UGCMapType type );

void ugcEditorMEFieldChangedCB(MEField *pField, bool bFinished, UserData unused);
void ugcEditorWidgetChangedCB(UIWidget* ignored, UserData ignored2);
void ugcEditorSetErrorPrefix(const char *pcPrefixFormat, ...);
bool ugcEditorMEFieldErrorCB(UGCRuntimeErrorContext*pErrorContext, const char *pchFieldName, int iFieldIndex, char **estrToolTip_out);
bool ugcEditorErrorPrint(const UGCRuntimeStatus* runtimeStatus, const UGCRuntimeErrorContext* pErrorContext, const char *pchFieldName, int iFieldIndex, char **estrToolTip_out);
int ugcErrorCount( const UGCRuntimeStatus* runtimeStatus, const UGCRuntimeErrorContext* ctx );
const UGCRuntimeError **ugcErrorList( const UGCRuntimeStatus* runtimeStatus, const UGCRuntimeErrorContext* ctx );
void ugcErrorButtonRefresh( UISprite** icon, const UGCRuntimeStatus* runtimeStatus, const UGCRuntimeErrorContext* ctx, const char* fieldName, F32 y, UIWidget* parent );
void ugcErrorButtonRefreshForMapComponent( UISprite** icon, const UGCRuntimeStatus* runtimeStatus, const char* strComponentName );

// Call ugcEditorApplyUpdate generally.  This is for when no data changed, but the view might.
#define ugcEditorUpdateUI() ugcEditorUpdateUIEx()
#define ugcEditorApplyUpdate() ugcEditorApplyUpdateEx()

// Pickers
MEFieldContextEntry* ugcMEContextAddResourcePicker(const char* strObjectType, const char* strDefaultText, const char* strPickerTitle, bool allowTrashButton, const char* strFieldName, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddResourcePickerEx(const char* strObjectType, UGCAssetLibraryCustomFilterFn filterFn, UserData filterData, const char* strDefaultText, const char* strPickerTitle, bool allowTrashButton, const char* strPickerNote, const char* strFieldName, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddResourcePickerMsg(const char* strObjectType, const char* strDefaultText, const char* strPickerTitle, bool allowTrashButton, const char* strFieldName, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddResourcePickerExMsg(const char* strObjectType, UGCAssetLibraryCustomFilterFn filterFn, UserData filterData, const char* strDefaultText, const char* strPickerTitle, bool allowTrashButton, const char* strPickerNote, const char* strFieldName, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddCostumePicker( bool allowProjectCostumes, const char* strDefaultText, const char* strPickerTitle, const char* strCostumeFieldName, const char* strPetCostumeFieldName, const char* strDisplayName, const char* strTooltip );
MEFieldContextEntry* ugcMEContextAddCheckedAttribPicker( UGCInteractPropertiesFlags flags, const char* strDefaultText, const char* strPickerTitle, const char* strFieldName, const char* strDisplayName, const char* strTooltip );
MEFieldContextEntry* ugcMEContextAddCheckedAttribPickerMsg( UGCInteractPropertiesFlags flags, const char* strDefaultText, const char* strPickerTitle, const char* strFieldName, const char* strDisplayName, const char* strTooltip );
MEFieldContextEntry* ugcMEContextAddComponentPicker(const char* strDefaultText, const char* strFieldName, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddComponentPickerIndex(const char* strDefaultText, const char* strFieldName, int index, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddComponentPickerEx(const char* strDefaultText, const char* strDefaultMapName, UGCZeniPickerFilterType forceFilterType, const char* strFieldName, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddComponentPickerIndexEx(const char* strDefaultText, const char* strDefaultMapName, UGCZeniPickerFilterType forceFilterType, const char* strFieldName, int index, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddComponentPickerMsg(const char* strDefaultText, const char* strFieldName, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddComponentPickerIndexMsg(const char* strDefaultText, const char* strFieldName, int index, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddComponentPickerExMsg(const char* strDefaultText, const char* strDefaultMapName, UGCZeniPickerFilterType forceFilterType, const char* strFieldName, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddComponentPickerIndexExMsg(const char* strDefaultText, const char* strDefaultMapName, UGCZeniPickerFilterType forceFilterType, const char* strFieldName, int index, const char* strDisplayName, const char* strTooltip);
MEFieldContextEntry* ugcMEContextAddLocationPickerMsg(const char* strFieldName, const char* strDisplayName, const char* strTooltip);
void ugcMEContextAddWhenPickerMsg( const char* mapName, UGCWhen* when, const char* ctxName, int whenIdx, bool hasObjective,
								   StaticDefineInt* enumModel, const char* label, const char* tooltip );

MEFieldContextEntry* ugcMEContextAddBooleanMsg( const char* strFieldName, const char* strDisplayName, const char* strTooltip );
MEFieldContextEntry* ugcMEContextAddMultilineText( const char* strFieldName, const char* strDisplayName, const char* strTooltip );
MEFieldContextEntry* ugcMEContextAddMultilineTextMsg( const char* strFieldName, const char* strDisplayName, const char* strTooltip );
MEFieldContextEntry* ugcMEContextAddProjectEditorMultilineTextMsg( const char* strFieldName, const char* strDisplayName, const char* strTooltip );
MEFieldContextEntry* ugcMEContextAddStarRating( float rating, const char* strFieldName );
MEFieldContextEntry* ugcMEContextAddStarRatingIndex( float rating, const char* strFieldName, int index );
MEFieldContextEntry* ugcMEContextAddBarGraphMsg( int value, int max, const char* strFieldName, const char* strDisplayName, const char* strTooltip );
MEFieldContextEntry* ugcMEContextAddList( UIModel peaModel, int rowHeight, UICellDrawFunc drawFn, UserData drawData, const char* strName );
MEFieldContextEntry* ugcMEContextAddErrorSpriteForEditor( UGCEditorType editor, const char* strName );
MEFieldContextEntry* ugcMEContextAddErrorSpriteForMissionMapTransition( const char* strNextObjectiveName );
MEFieldContextEntry* ugcMEContextAddErrorSpriteForMissionMap( const char* strNextMapName, const char* strNextObjectiveName );
MEFieldContextEntry* ugcMEContextAddErrorSpriteForMapComponent( const char* componentName );
MEFieldContextEntry* ugcMEContextAddErrorSpriteForMissionObjective( const char* strObjectiveName );
MEFieldContextEntry* ugcMEContextAddErrorSpriteForDialogTreePrompt( const char* strDialogTreeName, U32 promptID );

UIPane* ugcMEContextPushPaneParentWithHeader( const char* pchUID, const char* pchHeaderUID, const char* headerName, bool bWhiteBackground );
UIPane* ugcMEContextPushPaneParentWithHeaderMsg( const char* pchUID, const char* pchHeaderUID, const char* headerName, bool bWhiteBackground );
UIPane* ugcMEContextPushPaneParentWithBooleanCheckMsg( const char* pchUID, const char* pchHeaderUID, const char* headerName, bool bState, bool bActive, UIActivationFunc toggledCB, UserData toggledData );

UIPane* ugcMEContextPushPane( const char* strName ); // Use this w/ MEContextPop when you're done 

void ugcRefreshColorCombo(UIColorCombo **ppCombo, int x, int y, UIColorSet *pColorSet, U8 vColor[4], UserData pUserData, UIWidget *pParent, UIActivationFunc changeCallback, UIColorHoverFunc hoverCallback);
void ugcRefreshInteractProperties(UGCInteractProperties *properties, UGCMapType map_type, UGCInteractPropertiesFlags flags);
void ugcEditorPlacePropertiesWidgetsForBoxes( UIPane* propertiesPane, UISprite* propertiesSprite, CBox* screenBox, CBox* selectedBox, bool paneIsDocked, bool forAnimateToCenter );

// Misc Utils
const char* ugcResourceGetDisplayName(const char *object_type, const char *object_name, const char *default_label);
const char* ugcTrapGetDisplayName( int objlibID, const char* trapPower, const char* defaultName );
void ugcEditorSetCursorForRotation( float rot );

void ugcEditorErrorHandled(const UGCRuntimeError *error);
void ugcEditorUpdateUIEx( void );
void ugcEditorApplyUpdateEx( void );
bool ugcEditorIsIgnoringChanges( void );


UGCMapEditorDoc *ugcEditorEditMapComponent( const char* map_name, U32 component_id, bool select_unplaced, bool refresh_ui );
void ugcEditorEditMissionObjective( const char* objective_name );
void ugcEditorEditMissionDialogTreeBlock( U32 dialog_id );
void ugcEditorEditDialogTreeBlock( U32 dialog_id, int promptID, int promptActionIndex );
void ugcEditorEditMissionMapTransitionByMapLink( UGCMissionMapLink* mapLink );
void ugcEditorEditMissionNode( UGCMissionNodeGroup* group );
void ugcEditorEditMission( void );
void ugcEditorEditCostume( UIWidget* ignored, const char *costume_name );
void ugcEditorErrorsWindowShow( UIWidget* ignored, UserData ignored2 );
void ugcEditorErrorsWidgetRefresh( UITree** ppTree, bool navigateToErrorOnDoubleClick );
void ugcEditorSetActivePlayingEditorMap( const char* mapName );
const char* ugcEditorGetActivePlayingEditorMap( void );

void ugcEditorQueueApplyUpdate(void);
void ugcEditorQueueUIUpdate(void);

UGCProjectData *ugcEditorGetProjectData(void);
UGCProjectData *ugcEditorGetLastSaveData(void);
UGCBacklinkTable* ugcEditorGetBacklinkTable(void);

UGCEditorCopyBuffer *ugcEditorStartCopy(UGCEditorCopyType type);
UGCEditorCopyBuffer *ugcEditorCurrentCopy(void);
void ugcEditorAbortCopy(void);

const char *ugcEditorGetNamespace(void);
UGCMapEditorDoc *ugcEditorGetActiveMapDoc(void);
UGCMapEditorDoc *ugcEditorGetMapDoc(const char *map_name);
UGCMapEditorDoc *ugcEditorGetComponentMapDoc(const UGCComponent *component);
UGCMap **ugcEditorGetMapsList(void);
UGCMap* ugcEditorGetMapByName(const char* name);
UGCMap* ugcEditorGetComponentMap(const UGCComponent *component);
UGCComponentList *ugcEditorGetComponentList(void);
UGCMission *ugcEditorGetMission(void);
UGCComponent* ugcEditorFindComponentByID(U32 id);
UGCItemEditorDoc *ugcEditorGetActiveItemDoc();
UGCItem* ugcEditorGetItemByName(const char* name);
UGCRuntimeStatus *ugcEditorGetRuntimeStatus(void);
UGCRuntimeError** ugcEditorGetRuntimeErrors(void);
bool ugcEditorMissionIsLoaded(void);
void ugcEditorGetCachedProjectData(UGCProjectReviews** out_pReviews, int* out_pPageNumber, UGCProjectStatusQueryInfo** out_pStatus);

// Useful UI callbacks
void ugcEditorPopupChooserMapsCB( UIButton* ignored, UserData ignored2 );
void ugcEditorPopupChooserDialogsCB( UIButton* ignored, UserData ignored2 );
void ugcEditorPopupChooserCostumesCB( UIButton* ignored, UserData ignored2 );
void ugcEditorPopupChooserItemsCB( UIButton* ignored, UserData ignored2 );

UGCMap *ugcEditorGetLastEditMap(const char *map_name);

void ugcEditorRefreshGlobalUI(void);

StaticDefineInt* ugcEditorGetPickerEnum( void );
void ugcEditorFinishCreateMap( UGCMap* map, bool bNewMapInMissionEditor );
void ugcEditorPlay(const char* map_name, U32 objective_id, bool is_mission, Vec3 spawn_pos, Vec3 spawn_rot);
void ugcEditorPlayFromStart(void);
void ugcEditorPlayDialogTree(U32 componentID, int promptID);

void ugcEditorStartObjectFlashing(void *object);
F32 ugcEditorGetObjectFlashingTime(void *object);

void ugcEditorCreateProject(UIMenu *menu, void *unused);
void ugcEditorLoadProject(UIMenu *menu, void *unused);
void ugcEditorSetEditable(UIMenu *menu, void *userdata);

void ugcEditorExecuteCommandByID(UGCActionID command_id);
bool ugcEditorQueryCommandByID(UGCActionID command_id, char** out_estr);

void ugcEditorResetMap( void );
void ugcEditorSetDocPane( UIPane* pane );

void ugcEditorUserResourceChanged(void);
void ugcEditorProjectAllegianceChanged(void);

bool ugcEditorObjectRestrictionSetIsValid(WorldUGCRestrictionProperties* props);
bool ugcEditorEncObjFilter( const char* zmName, ZoneMapEncounterObjectInfo* object, UserData ignored );

void ugcEditorSaveAndPublishProjectCmd(void);
void ugcEditorShowSetAuthorAllowsFeaturedDialog(void);
void UGCEditorQueryJobStatus(void);

const char** ugcComponentAnimationDefaultVals( UGCMapType map_type );

void ugcWidgetTreeSetFocusCallback( UIWidget* root, UIActivationFunc fn, UserData data );

CharClassTypes ugcGetRegionTypeFromName(const char *pcName);
const char *ugcGetRegionNameFromType(CharClassTypes eType);

void ugcEditorGetMoreReviews(UIButton *button, UserData ignored);
void UGCEditorUpdatePublishStatus(bool succeeded, const char *pDisplayString);

void* ugcEditorGetObject( const char* dictName, const char* objName );
UGCCostume* ugcEditorCreateCostume(const char* displayName, const char* presetCostumeName, U32 region);

// Create/destroy stuff
void ugcEditorCreateNewMap(bool bNewMapInMissionEditor);
void ugcEditorDeleteMap(UGCMapEditorDoc* doc);
void ugcEditorDuplicateMap(UGCMapEditorDoc* doc);
void ugcEditorCreateNewCostume(UIWidget* ignored, UserData ignored2);
void ugcEditorCreateNewItem(UIWidget* ignored, UserData ignored2 );

// UGCEditorFixup.c

bool ugcEditorFixupPostEdit(bool query_delete);

// UGCEditorActions.c

UGCEditorMenus *ugcEditorCreateMenus();
void ugcEditorShowContextMenu(UGCActionID *actions);
bool ugcEditorGetContextMenuPosition(F32 *out_x, F32 *out_y);
bool ugcEditorHasContextMenuPosition();
void ugcEditorMenusRefresh(UGCEditorMenus *menus, bool visible, UGCEditorViewMode mode);
F32 ugcEditorGetMenuHeight(UGCEditorMenus *menus);
void ugcEditorMenuOncePerFrame(UGCEditorMenus *menus);

UGCActionDescription *ugcEditorGetAction(UGCActionID id);
void ugcEditorExecuteAction(UIButton *button, UserData data);
UIMenuItem* ugcEditorMenuItemCreate(UGCActionID id);
UIButton *ugcEditorButtonCreate(UGCActionID id, bool show_icon, bool icon_only);
MEFieldContextEntry* ugcMEContextAddEditorButton(UGCActionID id, bool show_icon, bool icon_only);

// Animated Headshot management
bool ugcAnimatedHeadshotThisFrame( void );
void ugcSetAnimatedHeadshotThisFrame( void );
