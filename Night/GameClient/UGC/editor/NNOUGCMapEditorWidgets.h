//// All the MapEditorWidgets go here.
////
//// Map Editor widgets are similar, but not exactly light normal
//// UIWidgets.  Here are a few of their differences:
////
//// * A map editor widget always calculates its own width and height.
//// * A map editor widget has no input function (no text input!)
//// * A map editor widget's draw function will get called multiple
////   times, for different layers of drawing (i.e. background,
////   component image, pip are all examples of layers)
//// * A map editor widget can not have any children.
#pragma once

#include"NNOUGCCommon.h"
#include"UICore.h"

typedef enum MouseButton MouseButton;
typedef enum UGCMapUIDragEvent UGCMapUIDragEvent;
typedef enum UGCMapUIDragType UGCMapUIDragType;
typedef enum UGCMapUIMouseEventPriority UGCMapUIMouseEventPriority;
typedef enum UGCUIMapEditorDrawLayer UGCUIMapEditorDrawLayer;
typedef enum UGCWaypointMode UGCWaypointMode;
typedef struct CBox CBox;
typedef struct UGCMapEditorDoc UGCMapEditorDoc;
typedef struct UGCMapUIMouseEvent UGCMapUIMouseEvent;
typedef struct UGCUIMapEditorWidget UGCUIMapEditorWidget;
typedef struct UIMinimap UIMinimap;
typedef struct UIPane UIPane;
typedef struct UISprite UISprite;

typedef bool (*UGCMapUIMouseEventFn)(UGCMapEditorDoc *doc, UGCMapUIMouseEvent* event);
typedef void (*UGCMapUIDragFn)(UGCMapEditorDoc* doc, UGCMapUIDragEvent event);

typedef void (*UGCUIMapEditorWidgetDrawFunction)( void* widget, UGCUIMapEditorDrawLayer layer, UI_PARENT_ARGS );

/// Different layers to draw in the map editor
typedef enum UGCUIMapEditorDrawLayer
{
	UGC_MAP_LAYER_BACKDROP,
	UGC_MAP_LAYER_ROOM,
	UGC_MAP_LAYER_COMPONENT,
	UGC_MAP_LAYER_DOORS,
	UGC_MAP_LAYER_TOP_UI,
	UGC_MAP_LAYER_VOLUME,
	UGC_MAP_LAYER_TEXT,
	UGC_MAP_LAYER_MAX,
} UGCUIMapEditorDrawLayer;

/// Base class for all Map Editor widgets.
///
/// Any widget that appears in the overhead view of the map editor
/// must derive from this.
typedef struct UGCUIMapEditorWidget
{
	// Just enough of widget to work
	UILoopFunction tickF;
	UGCUIMapEditorWidgetDrawFunction drawF;
	UIFreeFunction freeF;

	UGCUIMapEditorWidget*** group;
	UIWidgetModifier state;
	float x;
	float y;
	int priority;
} UGCUIMapEditorWidget;
#define UGC_UI_MAP_EDITOR_WIDGET_TYPE UGCUIMapEditorWidget ugcMapEditorWidget;
#define UGC_UI_MAP_EDITOR_WIDGET(w) (&(w)->ugcMapEditorWidget)

#define UGC_UI_MAP_EDITOR_GET_COORDINATES(widget)						\
	float scale = pScale;												\
	float x = floorf( pX + UGC_UI_MAP_EDITOR_WIDGET(widget)->x * pScale ); \
	float y = floorf( pY + UGC_UI_MAP_EDITOR_WIDGET(widget)->y * pScale ); \
	float z = UI_GET_Z();												\
	CBox pBox = { pX, pY, pX + pW, pY + pH }

/// Container of UGCUIMapEditorWidgets.
typedef struct UGCUIMapEditorWidgetContainer
{
	UI_INHERIT_FROM( UI_WIDGET_TYPE );
	UGCUIMapEditorWidget** eaChildren;

	UGCMapEditorDoc* doc;
} UGCUIMapEditorWidgetContainer;

/// The background minimap
typedef struct UGCUIMapEditorBackdrop
{
	UI_INHERIT_FROM( UGC_UI_MAP_EDITOR_WIDGET_TYPE );
	
	UGCMapEditorDoc* doc;
	UIMinimap* minimap;
	Vec3 region_offset;
	bool is_static;
} UGCUIMapEditorBackdrop;

typedef struct UGCUIMapEditorComponent
{
	UI_INHERIT_FROM(UGC_UI_MAP_EDITOR_WIDGET_TYPE);
	U32 uComponentID;

	// If set, then this will be selected if this component gets
	// deleted.
	U32 uComponentLogicalParent;
	UGCMapEditorDoc *doc;
	bool is_static;
	bool is_deleted;
	int selected_level;

	// A sprite updated to display error state
	UISprite* errorSprite;

	// NOTE: These are only valid if the component is being dragged,
	// i.e., it is in primary_component, secondary_components, or tethered_components
	// of layout_drag.
	bool drag_valid;
	UGCComponentValidPosition drag_position;
	Vec3 drag_relative_position;

	// If set, this is a child of something explicitly dragged.  This
	// requires slightly different behavior, like not being selected
	// after the drag-drop.
	bool drag_is_implicit_child;

	// If set, the parent room is also being dragged at the same time.
	// This means that you shouldn't check for valid positions, since
	// the parent room is moving.
	bool drag_parent_room_is_dragging;

	// If set, the component is in a valid position.  This gets updated every tick
	bool isValidPosition;
} UGCUIMapEditorComponent;

typedef struct UGCUIMapEditorObjectiveWaypoint
{
	UI_INHERIT_FROM( UGC_UI_MAP_EDITOR_WIDGET_TYPE );
	UGCMapEditorDoc* doc;
	U32 objectiveID;
	UGCWaypointMode eMode;

	UGCBoundingVolume volume;
	float* eaPositions;
} UGCUIMapEditorObjectiveWaypoint;

// placeholder "widgets" so that the map editor widgets can track
// focus.
extern UIWidget g_UGCMapEditorComponentWidgetPlaceholder;
extern UIWidget g_UGCMapEditorBackdropWidgetPlaceholder;

void ugcMapEditorWidgetFreeInternal( UGCUIMapEditorWidget* widget );
void ugcMapEditorWidgetContainerAddChild( UGCUIMapEditorWidgetContainer* parent, UGCUIMapEditorWidget* child );
void ugcMapEditorWidgetContainerSort( UGCUIMapEditorWidgetContainer* container );
void ugcMapEditorWidgetQueueFree( UGCUIMapEditorWidget* widget );
void ugcMapEditorWidgetRemoveFromGroup( UGCUIMapEditorWidget* widget );
UGCUIMapEditorWidgetContainer* ugcMapEditorWidgetContainerCreate( void );

// indexed by component type
extern AtlasTex *g_ComponentMarkers[];
extern const char *g_ComponentIcons[];

UGCUIMapEditorWidgetContainer* ugcLayoutGenerateStaticUI(UGCMapEditorDoc *doc);

void ugcLayoutGetUICoords(UGCMapEditorDoc *doc, Vec3 world_pos, Vec2 out_ui_pos);
void ugcLayoutGetUISize(UGCMapEditorDoc *doc, Vec2 scroll_area_size);
void ugcLayoutGetWorldCoords(UGCMapEditorDoc *doc, Vec2 ui_pos, Vec3 out_world_pos);

void ugcLayoutUpdateUI(UGCMapEditorDoc *doc);
void ugcLayoutOncePerFrame(UGCMapEditorDoc *doc);

#define ugcMapUIDoMouseEventTest(doc,button,box,priority,func,fmt,...) ugcMapUIDoMouseEventTestEx(doc,button,box,false,priority,func,fmt,__VA_ARGS__)
UGCMapUIMouseEvent *ugcMapUIDoMouseEventTestEx(UGCMapEditorDoc *doc, MouseButton button, CBox *box, bool circle, UGCMapUIMouseEventPriority priority, UGCMapUIMouseEventFn cb, const char *fmt, ...);

void ugcMapUIStartDrag(UGCMapEditorDoc *doc, MouseButton button, int mouse_offset_x, int mouse_offset_y,
					   UGCMapUIDragType type, bool show_trash, UGCMapUIDragFn cb);
void ugcMapUICancelAction(UGCMapEditorDoc *doc);

bool ugcLayoutCanCreateComponent(const char *map_name, UGCComponentType type);
UGCUIMapEditorComponent *ugcLayoutUIGetComponent(UGCMapEditorDoc *doc, UGCComponent *component);
UGCUIMapEditorComponent *ugcLayoutUIGetComponentByID(UGCMapEditorDoc *doc, U32 component_id);
UGCUIMapEditorComponent *ugcLayoutUIComponentCreate(UGCMapEditorDoc *doc, UGCComponent* component, bool is_static);
void ugcLayoutDeleteComponent(UGCComponent *component);
bool ugcLayoutCanDeleteComponent(UGCComponent *component);
bool ugcLayoutComponentCanRotate(UGCComponentType type);

void ugcMapEditorUpdateZOrder(UGCMapEditorDoc *doc);

void ugcLayoutStartPlaceNewComponent(UGCMapEditorDoc *doc, UGCComponent *component);
void ugcLayoutUIComponentUpdate(UGCUIMapEditorComponent *component_widget);
void ugcLayoutGetDefaultPlacement(UGCMapEditorDoc *doc, U32 *room_id, Vec2 pos);
void ugcLayoutComponentFixupChildLocations(UGCComponent *src_component, Vec3 old_pos, Vec3 new_pos, Vec2 min_bounds, Vec2 max_bounds);
void ugcLayoutComponentFixupChildRotations(UGCComponent *src_component, UGCComponent *root_component, F32 old_rot, F32 new_rot);

void ugcMapEditorZoomToLayoutMode(UGCMapEditorDoc *doc);
void ugcMapEditorZoomToDetailMode(UGCMapEditorDoc *doc);
void ugcComponentCreateChildrenForInitialDrag( UGCProjectData* ugcProj, UGCComponent* component );

// UGCMapEditorWidgetsOpt.c
void ugcLayoutDrawTranslateGrid( UGCMapEditorDoc* doc, F32 x, F32 y );
