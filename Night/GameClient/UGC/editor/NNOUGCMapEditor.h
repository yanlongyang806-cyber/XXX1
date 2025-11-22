#pragma once
GCC_SYSTEM

#include"inputLibEnums.h"
#include"CBox.h"

typedef enum UGCActionID UGCActionID;
typedef enum UGCComponentType UGCComponentType;
typedef enum UGCWaypointMode UGCWaypointMode;
typedef enum UGCZeniPickerFilterType UGCZeniPickerFilterType;
typedef struct EditUndoStructOp EditUndoStructOp;
typedef struct ExclusionVolumeGroup ExclusionVolumeGroup;
typedef struct GenesisUGCPath GenesisUGCPath;
typedef struct GenesisUGCRoom GenesisUGCRoom;
typedef struct HeightMapExcludeGrid HeightMapExcludeGrid;
typedef struct MEField MEField;
typedef struct ResourceSearchResultRow ResourceSearchResultRow;
typedef struct UGCAssetLibraryPane UGCAssetLibraryPane;
typedef struct UGCAssetLibraryRow UGCAssetLibraryRow;
typedef struct UGCAssetTagType UGCAssetTagType;
typedef struct UGCCheckedAttribCombo UGCCheckedAttribCombo;
typedef struct UGCComponent UGCComponent;
typedef struct UGCComponentList UGCComponentList;
typedef struct UGCComponentValidPosition UGCComponentValidPosition;
typedef struct UGCEditorCopyBuffer UGCEditorCopyBuffer;
typedef struct UGCInteractPropertiesGroup UGCInteractPropertiesGroup;
typedef struct UGCMap UGCMap;
typedef struct UGCMapEditorDoc UGCMapEditorDoc;
typedef struct UGCMapUIDragState UGCMapUIDragState;
typedef struct UGCMapUIMouseEvent UGCMapUIMouseEvent;
typedef struct UGCProjectData UGCProjectData;
typedef struct UGCProjectInfo UGCProjectInfo;
typedef struct UGCRuntimeErrorContext UGCRuntimeErrorContext;
typedef struct UGCRuntimeStatus UGCRuntimeStatus;
typedef struct UGCSpaceRoom UGCSpaceRoom;
typedef struct UGCSpecialComponentDef UGCSpecialComponentDef;
typedef struct UGCStateWhenGroup UGCStateWhenGroup;
typedef struct UGCUIMapEditorBackdrop UGCUIMapEditorBackdrop;
typedef struct UGCUIMapEditorComponent UGCUIMapEditorComponent;
typedef struct UGCUIMapEditorObjectiveWaypoint UGCUIMapEditorObjectiveWaypoint;
typedef struct UGCUIMapEditorWidgetContainer UGCUIMapEditorWidgetContainer;
typedef struct UGCUnplacedList UGCUnplacedList;
typedef struct UGCWhen UGCWhen;
typedef struct UIButton UIButton;
typedef struct UIComboBox UIComboBox;
typedef struct UILabel UILabel;
typedef struct UIMinimap UIMinimap;
typedef struct UIPane UIPane;
typedef struct UISMFView UISMFView;
typedef struct UIScrollArea UIScrollArea;
typedef struct UISlider UISlider;
typedef struct UISpinner UISpinner;
typedef struct UISprite UISprite;
typedef struct UITextEntry UITextEntry;
typedef struct UITreeNode UITreeNode;
typedef struct UITreechart UITreechart;
typedef struct UIWidget UIWidget;
typedef struct UIWindow UIWindow;
typedef struct ZoneMapEncounterInfo ZoneMapEncounterInfo;

#define UGC_NONE 0xFFFFFFFF

typedef enum UGCMapUIMouseEventPriority
{
	// From highest priority to lowest
	UGC_MOUSE_EVENT_HIGH = 0,

	UGC_MOUSE_EVENT_BACKDROP_HIGH,
	UGC_MOUSE_EVENT_COMPONENT_MOVE,
	UGC_MOUSE_EVENT_COMPONENT_ROTATE,
	UGC_MOUSE_EVENT_ROOM_ROTATE,
	UGC_MOUSE_EVENT_ROOM_MOVE, 
	UGC_MOUSE_EVENT_ROOM_CONTEXT,
	UGC_MOUSE_EVENT_BACKDROP,
	
	UGC_MOUSE_EVENT_OTHER
} UGCMapUIMouseEventPriority;

typedef enum UGCMapUIMouseEventType
{
	UGC_MOUSE_EVENT_NONE = 0,
	UGC_MOUSE_EVENT_HOVER,
	UGC_MOUSE_EVENT_CLICK,
	UGC_MOUSE_EVENT_DOUBLE_CLICK,
	UGC_MOUSE_EVENT_DRAG,
} UGCMapUIMouseEventType;

typedef enum UGCMapUIDragEvent
{
	UGC_DRAG_DRAGGING,			// Drag in progress
	UGC_DRAG_DROPPED,			// Drag complete
	UGC_DRAG_CANCELLED,			// Operation cancelled by user
	UGC_DRAG_DELETED,			// Dragged to trash can
} UGCMapUIDragEvent;

typedef enum UGCMapUIDragType
{
	UGC_DRAG_MOVE_COMPONENT,
	UGC_DRAG_ROTATE_COMPONENT,
	UGC_DRAG_MMB_PAN,
	UGC_DRAG_MARQUEE_SELECT,
	UGC_CREATE_DOOR,
} UGCMapUIDragType;

// Callback type definitions

typedef bool (*UGCMapUIMouseEventFn)(UGCMapEditorDoc *doc, UGCMapUIMouseEvent* event);
typedef void (*UGCMapUIDragFn)(UGCMapEditorDoc* doc, UGCMapUIDragEvent event);

// Structs

// This represents any mouse event that the map layout view cares
// about.  Events are prioritized and only the first one is actually
// executed, by calling the callback function.
typedef struct UGCMapUIMouseEvent {
	char* debug_label;
	UGCUIMapEditorComponent* component_widget;
	UGCMapUIMouseEventFn callback;

	// Mouse state
	MouseButton button;
	int position[2];
	UGCMapUIMouseEventType type;

	// All these (in addition to the type of event) affect priorities.
	UGCMapUIMouseEventPriority priority;
	bool is_selected;	// True if object is the selected one
	F32 distance;

	// Extra tracking info
	int door_id;
	bool door_is_open;
	int *door_types;
	UGCMapUIDragType drag_type; // Click type; May or may not be an actual drag
} UGCMapUIMouseEvent;

// This represents an (in progress) drag that is happening.  Drags
// should always get initiated in the callback of a
// UGCMapUIMouseEvent.
typedef struct UGCMapUIDragState
{
	UGCMapUIDragType type;
	UGCUIMapEditorComponent* primary_widget;
	UGCUIMapEditorComponent** secondary_widgets;
	UGCUIMapEditorComponent** tethered_widgets;
	UGCMapUIDragFn callback;

	// Initiating click info
	Vec2 click_world_xz;
	MouseButton click_button;

	// If the trash button should be available.
	bool show_trash;

	// Amount to offset the mouse position by, for when the user
	// clicks off the center of a component.
	int mouse_offset[2];
	bool door_snap_offset; // If set, rotate snapped to 90/270, otherwise 0/180

	// Stateful, these are updated every frame the drag is active
	bool trash_highlighted;
	Vec3 world_pos;
	bool valid_pos;
} UGCMapUIDragState;

typedef struct UGCTriggerGroupUI
{
	UILabel *label;
	UILabel *objective_label;
	UIButton **trigger_buttons;
	UGCInteractPropertiesGroup *interact_group;
} UGCTriggerGroupUI;

typedef enum UGCMapEditorMode
{
	UGC_MAP_EDITOR_DETAIL,
	UGC_MAP_EDITOR_LAYOUT,
} UGCMapEditorMode;

typedef struct UGCMapEditorDoc
{
	const char *doc_name;
	UGCMap *map_data;
	UGCMapEditorMode mode;

	// Unplaced UI
	UILabel *unplaced_description;
	UIButton *unplaced_create_button;

	// Global UI
	UILabel *map_label;
	UIButton* map_name_button;
	UIButton* edit_map_name_button;
	UISprite* edit_map_name_error_sprite;
	S32 view_y;
	UIPane *toolbar_window;
	UIPane *toolbar_noedit_window;

	// Selection
	U32* selected_components;

	// Highlight
	U32* highlight_components;

	// The list of components selected last time
	// ugcMapEditorClearSelection() was called, so the patrol routes
	// can have a sensible visibility.
	U32* prev_selected_components;

	// Layout UI

	// Pixel position = (world position / layout_kit_spacing) * layout_grid_size * layout_scale
	F32 layout_grid_size;				// Pixels on screen per grid unit (at scale 1.0)
	F32 layout_scale;					// Current UI scale factor
	F32 layout_kit_spacing;				// Feet in the world per grid unit

	Vec2 layout_min_pos;				// Bounds of the editable region
	Vec2 layout_max_pos;				// Bounds of the editable region

	F32 spawn_radius;					// Radius of the spawn area for this map's region type

	F32 objects_fade;					// 0 = Editable objects, 1 = Invisible objects (Custom interiors only)
	F32 rooms_fade;						// 0 = Editable rooms, 1 = Static rooms (Custom interiors only)

	F32 frame_z;
	
	UGCMapUIDragState *drag_state;
	UGCMapUIMouseEvent **events;
	
	// Root UI
	UIPane* pRootPane;

	// Header pane
	UIPane* header_pane;
	UIButton *delete_button;
	UIButton *duplicate_button;
	UIButton *play_button;
	UIButton *snap_buttons[2];
	UIButton* viewWaypointButton;
	UIButton* searchComponentsButton;
	UIButton* editBackdropButton;
	UISprite* editBackdropErrorSprite;

	// Content pane
	UIPane* layout_pane;
	CBox layout_last_box;
	UGCUIMapEditorBackdrop *backdrop_widget;
	CBox backdrop_last_box;
	UIScrollArea *map_widget;
	UGCUIMapEditorWidgetContainer* map_widget_container;
	UIButton *trash_button;

	UGCUIMapEditorComponent **component_widgets;
	UGCUIMapEditorObjectiveWaypoint** objectiveWaypointWidgets;

	// Library pane
	UIPane* library_pane;
	int activeLibraryTabIndex;
	UGCAssetLibraryPane* libraryEmbeddedPicker;

	// Unplaced pane
	UIPane* unplaced_pane;
	UILabel* unplaced_header;
	UGCUnplacedList* unplaced_list;

	// Properties pane
	UIPane* properties_pane;
	UISprite* properties_sprite;

	// Component search window
	UIWindow* search_window;

	// Properties windows
	UIWindow* global_properties_window;
	UIWindow* backdrop_properties_window;

	/// Caches:

	// A BLOCK EARRAY, not a regular earray.
	StaticDefineInt* beaObjectivesModel;
} UGCMapEditorDoc;

AUTO_STRUCT;
typedef struct UGCComponentPlacementData
{
	UGCComponent *pPrimaryComponent;	NO_AST
	Vec3	vPrimaryOldPos;				NO_AST
	Vec3	vPrimaryOldRot;				NO_AST
	
	float	fXPos;		AST(NAME("XPos"))
	float	fYPos;		AST(NAME("YPos"))
	float	fZPos;		AST(NAME("ZPos"))

	float	fRotPitch;	AST(NAME("RotPitch"))
	float	fRotYaw;	AST(NAME("RotYaw"))
	float	fRotRoll;	AST(NAME("RotRoll"))

	bool isPlayingEditor;
} UGCComponentPlacementData;
extern ParseTable parse_UGCComponentPlacementData[];
#define TYPE_parse_UGCComponentPlacementData UGCComponentPlacementData

AUTO_STRUCT;
typedef struct UGCMarkerVolumeRadiusData
{
	UGCComponent *pComponent;	NO_AST
	
	float	fVolumeRadius;		AST(NAME("VolumeRadius"))
	
} UGCMarkerVolumeRadiusData;
extern ParseTable parse_UGCMarkerVolumeRadiusData[];
#define TYPE_parse_UGCMarkerVolumeRadiusData UGCMarkerVolumeRadiusData

AUTO_STRUCT;
typedef struct UGCComponentValidPositionUI
{
	UGCComponent *component;						NO_AST
	int selected;									AST(NAME("Selected"))
	UGCComponentValidPosition **results;			AST(NAME("Results"))
	char **labels;									AST(NAME("Labels"))
} UGCComponentValidPositionUI;
extern ParseTable parse_UGCComponentValidPositionUI[];
#define TYPE_parse_UGCComponentValidPositionUI UGCComponentValidPositionUI

typedef struct UGCPotentialRoomDoor
{
	// Creation information:
	U32 roomID;
	int doorIdx;

	// Extra data used for sorting:
	U32 connectedRoomID;
	F32 distanceFromIdeal;
} UGCPotentialRoomDoor;

UGCMapEditorDoc *ugcMapEditorLoadDoc(const char* map_name);
void ugcMapEditorFreeDoc(UGCMapEditorDoc *doc);
const char *ugcMapEditorGetName(UGCMapEditorDoc *doc);
void ugcMapEditorHandleAction(UGCMapEditorDoc *doc, UGCActionID action);
bool ugcMapEditorQueryAction(UGCMapEditorDoc *doc, UGCActionID action, char** out_estr);
void ugcMapEditorDocRefresh(UGCMapEditorDoc *doc);
void ugcMapEditorSetVisible(UGCMapEditorDoc *editor);
void ugcMapEditorSwitchToPlayMode(UGCMapEditorDoc* doc);
void ugcMapEditorSwitchToEditMode(UGCMapEditorDoc* doc);
void ugcMapEditorOncePerFrame(UGCMapEditorDoc *gen_doc, bool isActive);
void ugcMapEditorCopyChildrenRecurse(UGCEditorCopyBuffer *buffer, UGCComponent *component);

void ugcMapEditorPaste(UGCMapEditorDoc *doc, UGCEditorCopyBuffer *buffer, bool is_duplicate, bool offset);

ZoneMapEncounterInfo **ugcMapEditorBuildEncounterInfos( const char* mapName, UGCZeniPickerFilterType filterType );

bool ugcMapEditorGetTranslateSnap(UGCComponent* component, F32 *snap_xz, F32 *snap_y);
bool ugcMapEditorGetTranslateSnapEnabled(void);
F32 ugcMapEditorGetObjectTranslateSnap(void);
void ugcMapEditorToggleTranslateSnap(void);
bool ugcMapEditorGetRotateSnap(UGCComponentType type, F32 *snap);
void ugcMapEditorToggleRotateSnap(void);
bool ugcMapEditorGetViewWaypoints( void );
void ugcMapEditorToggleViewWaypoints( void );
bool ugcMapEditorGetPropertiesPaneIsDocked( void );
void ugcMapEditorTogglePropertiesPaneIsDocked( void );

void ugcMapEditorAddSelectedComponent(UGCMapEditorDoc *doc, U32 component_id, bool scroll_to_selected, bool zoom_to_selected);
void ugcMapEditorRemoveSelectedComponent(UGCMapEditorDoc *doc, U32 component_id);
void ugcMapEditorSetSelectedComponent(UGCMapEditorDoc *doc, U32 id, int placement, bool scroll_to_selected, bool zoom_to_selected);
void ugcMapEditorClearSelection(UGCMapEditorDoc *doc);
void ugcMapEditorClearSelectionWidgetCB( UIWidget* ignored, UGCMapEditorDoc* doc );
void ugcMapEditorClearObjectSelection(UGCMapEditorDoc *doc, U32 component_id, U32 component_id_to_select);
void ugcMapEditorSelectUnplacedTab(UGCMapEditorDoc *doc);
void ugcMapEditorMoveSelection(UGCMapEditorDoc* doc, int dx, int dy);

bool ugcMapEditorIsComponentSelected(UGCMapEditorDoc *doc, int component_id);
bool ugcMapEditorIsAnyComponentSelected(UGCMapEditorDoc *doc, int* eaComponents);
bool ugcMapEditorIsComponentPrevSelected(UGCMapEditorDoc *doc, int component_id);
bool ugcMapEditorIsAnyComponentPrevSelected(UGCMapEditorDoc *doc, int* eaComponents);

void ugcMapEditorSetHighlightedComponent(UGCMapEditorDoc *doc, U32 id);
void ugcMapEditorClearHighlight(UGCMapEditorDoc *doc);
bool ugcMapEditorIsComponentHighlighted(UGCMapEditorDoc *doc, int component_id);
bool ugcMapEditorIsAnyComponentHighlighted(UGCMapEditorDoc *doc, int* eaComponents);

void ugcMapEditorComponentPlace(UGCComponent *component, UGCMap *map, bool do_not_reset);
void ugcMapEditorRoomCreateDoors( UGCComponent* room, UGCPotentialRoomDoor*** inout_eaDoors );

void ugcMapEditorAbortCutForComponent(U32 component_id);

void ugcMapEditorUseRoomSoundForAllRooms(UGCMapEditorDoc *doc, U32 *component_ids);
void ugcMapEditorClearRoom(UGCMapEditorDoc *doc, U32 *component_ids);
void ugcMapEditorPopulateRoom(UGCMapEditorDoc *doc, U32 *component_ids);
void ugcMapEditorUpdateFocusForSelection( UGCMapEditorDoc* doc );

// This doc is shown if there are no maps.
//
// Yeah, the name is hideous.  The me from now wants to hit the me
// from three years ago, when the naming convention for docs was
// created.
typedef struct UGCNoMapsEditorDoc
{
	UIPane* pRootPane;
} UGCNoMapsEditorDoc;

UGCNoMapsEditorDoc *ugcNoMapsEditorLoadDoc( void );
void ugcNoMapsEditorDocRefresh(UGCNoMapsEditorDoc *pDoc);
void ugcNoMapsEditorSetVisible(UGCNoMapsEditorDoc *pDoc);
void ugcNoMapsEditorOncePerFrame(UGCNoMapsEditorDoc *pDoc, bool isActive);
void ugcNoMapsEditorFreeDoc(UGCNoMapsEditorDoc **ppDoc);
void ugcNoMapsEditorHandleAction(UGCNoMapsEditorDoc *pDoc, UGCActionID action);
bool ugcNoMapsEditorQueryAction(UGCNoMapsEditorDoc *pDoc, UGCActionID action, char** out_estr);
