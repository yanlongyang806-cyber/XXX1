#include "NNOUGCMapEditorWidgets.h"

#include "EditorManagerUI.h"
#include "Encounter_Common.h"
#include "EntCritter.h"
#include "GfxHeadshot.h"
#include "GfxPrimitive.h"
#include "GfxSprite.h"
#include "GfxSpriteText.h"
#include "GfxTexAtlas.h"
#include "MultiEditField.h"
#include "MultiEditFieldContext.h"
#include "NNOUGCAssetLibrary.h"
#include "NNOUGCDialogPromptPicker.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCInteriorCommon.h"
#include "NNOUGCMapEditor.h"
#include "NNOUGCMapEditorProperties.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCModalDialog.h"
#include "NNOUGCResource.h"
#include "NNOUGCUnplacedList.h"
#include "NNOUGCZeniPicker.h"
#include "ResourceSearch.h"
#include "RoomConnPrivate.h"
#include "StringUtil.h"
#include "UGCCommon.h"
#include "UGCError.h"
#include "UGCInteriorCommon.h"
#include "UICore.h"
#include "UIMinimap.h"
#include "UIPane.h"
#include "UISprite.h"
#include "UITextureAssembly.h"
#include "WorldGrid.h"
#include "gclResourceSnap.h"
#include "inputLib.h"
#include "soundLib.h"
#include "wlExclusionGrid.h"
#include "wlUGC.h"

#define UGC_MAP_OBJECT_FADE_CUTOFF 2.0f
#define UGC_MAP_ROOM_FADE_CUTOFF 2.0f

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

//// A note on coordinates systems used

/// There are four coordinate systems referenced here. To summarize:

// World Coordinates:
//    The final 3-D coordinates of the objects being placed.
//    Remember that the 3-D world is left-handed!

// Grid Coordinates:
//    Coordinates on a grid for things that are grid-based, like interior rooms. This is a straightforward
//    quantization of the World Coordinates:

//    Gx = Wx / layout_kit_spacing
//    Gz = Wz / layout_kit_spacing

// UI Coordinates:
//    Coordinates in the scroll area's view. This is where the widgets go in the whole area covering the
//    map, including border region, in *unscaled* pixels. That is, this is the position before zoom/child scale.

//    Ux = (Wx - layout_min_pos[0]) * layout_grid_size / layout_kit_spacing
//    Uy = (layout_max_pos[1] - Wz) * layout_grid_size / layout_kit_spacing

//    (use ugcLayoutGetUICoords() and ugcLayoutGetWorldCoords() to translate in/out of this space)

// Screen Coordinates:
//    Actual position on screen. This is done by the scroll area widget internally, and consists mainly of
//    scaling and offsetting the UI coordinates.

//    Sx = Ux*layout_scale - sb->xpos
//    Sy = Uy*layout_scale - sb->ypos

static AtlasTex* g_PlanetIcon = NULL;
static AtlasTex* g_RoomGridIcon = NULL;
static AtlasTex* g_RoomDoorIcon = NULL;
static AtlasTex* g_RotateIconBL = NULL;
static AtlasTex* g_RotateIconBR = NULL;
static AtlasTex* g_RotateIconTL = NULL;
static AtlasTex* g_RotateIconTR = NULL;
const char* g_LineMarker = NULL;
const char* g_ArrowMarker = NULL;
AtlasTex *g_ComponentMarkers[UGC_COMPONENT_TYPE_COUNT] = { 0 };
U32 g_ComponentTintColor[UGC_COMPONENT_TYPE_COUNT] = { 0 };
const char *g_ComponentIcons[UGC_COMPONENT_TYPE_COUNT] = { 0 };
static UGCComponent g_TemporaryComponent = { 0 };

#define UGC_DRAG_MIN_TIME 0.2f // less than two tenths of a second -> don't begin drag
#define UGC_DRAG_MIN_DIST 6 // less than size pixels -> don't begin drag
#define UGC_ROTATE_ICON_SCALE 0.4f

static bool g_bUGCTetheringEnabled = true;
AUTO_CMD_INT(g_bUGCTetheringEnabled, ugc_TetheringEnabled);

static int g_iUGCTetheringDragMargin = 20.0f;
AUTO_CMD_INT(g_iUGCTetheringDragMargin, ugc_TetheringDragMargin);

// Budgets display order:
UGCComponentType g_budgetsPriorityDisplay[] = {
	UGC_COMPONENT_TYPE_OBJECT,
	UGC_COMPONENT_TYPE_ACTOR,
	UGC_COMPONENT_TYPE_CONTACT,
	UGC_COMPONENT_TYPE_ROOM,
	UGC_COMPONENT_TYPE_ROOM_DOOR,
	UGC_COMPONENT_TYPE_FAKE_DOOR,
};
UGCComponentType g_budgetsToSortDisplay[] = {
	UGC_COMPONENT_TYPE_SPAWN,
	UGC_COMPONENT_TYPE_RESPAWN,
	UGC_COMPONENT_TYPE_KILL,
	UGC_COMPONENT_TYPE_COMBAT_JOB,
	UGC_COMPONENT_TYPE_PATROL_POINT,
	UGC_COMPONENT_TYPE_TRAP,
	UGC_COMPONENT_TYPE_TRAP_TARGET,
	UGC_COMPONENT_TYPE_TRAP_TRIGGER,
	UGC_COMPONENT_TYPE_TRAP_EMITTER,
	UGC_COMPONENT_TYPE_TELEPORTER,
	UGC_COMPONENT_TYPE_TELEPORTER_PART,
	UGC_COMPONENT_TYPE_CLUSTER,
	UGC_COMPONENT_TYPE_SOUND,
	UGC_COMPONENT_TYPE_ROOM_MARKER,
};


static F32 ugcLayoutComponentRotateHandleScale( const CBox* box );
static void ugcMapEditorUpdateAssetLibrary(UGCMapEditorDoc *doc);
static void ugcMapEditorUpdateUnplaced( UGCMapEditorDoc* doc );
static UGCComponent *ugcLayoutGetComponentByID(U32 id);

static F32 ugcDragGetInitialRotation(UGCMapEditorDoc* doc);
static F32 ugcDragGetCurrentRotation(UGCMapEditorDoc* doc);

// Just enough of a widget to keep the focus
UIWidget g_UGCMapEditorComponentWidgetPlaceholder = { 0 };
UIWidget g_UGCMapEditorBackdropWidgetPlaceholder = { 0 };

static UGCUIMapEditorWidget** g_eaUGCMapEditorWidgetFreeQueue = NULL;

AUTO_RUN;
void ugcSetupPlaceholderWidgets( void )
{
	g_UGCMapEditorComponentWidgetPlaceholder.group = &g_UGCMapEditorComponentWidgetPlaceholder.children;
	g_UGCMapEditorBackdropWidgetPlaceholder.group = &g_UGCMapEditorBackdropWidgetPlaceholder.children;
}

void ugcLayoutGetUICoords(UGCMapEditorDoc *doc, Vec3 world_pos, Vec2 out_ui_pos)
{
	if( doc->layout_kit_spacing == 0 ) {
		setVec2( out_ui_pos, 0, 0 );
	} else {
		out_ui_pos[0] = (world_pos[0]-doc->layout_min_pos[0]) * doc->layout_grid_size / doc->layout_kit_spacing;
		out_ui_pos[1] = (doc->layout_max_pos[1]-world_pos[2]) * doc->layout_grid_size / doc->layout_kit_spacing;
	}
}

void ugcLayoutGetUISize(UGCMapEditorDoc *doc, Vec2 scroll_area_size)
{
	scroll_area_size[0] = (doc->layout_max_pos[0]+2-doc->layout_min_pos[0])*doc->layout_grid_size/doc->layout_kit_spacing;
	scroll_area_size[1] = (doc->layout_max_pos[1]+2-doc->layout_min_pos[1])*doc->layout_grid_size/doc->layout_kit_spacing;
}

void ugcLayoutGetWorldCoords(UGCMapEditorDoc *doc, Vec2 ui_pos, Vec3 out_world_pos)
{
	out_world_pos[0] = doc->layout_min_pos[0] + (ui_pos[0]*doc->layout_kit_spacing/doc->layout_grid_size);
	out_world_pos[2] = doc->layout_max_pos[1] - (ui_pos[1]*doc->layout_kit_spacing/doc->layout_grid_size);
}

void ugcLayoutCommonInitSprites(void)
{
	static bool inited = false;
	if (inited)
		return;

	g_PlanetIcon = atlasLoadTexture("ugc_space_planet");
	g_RoomGridIcon = atlasLoadTexture("white");
	g_RoomDoorIcon = atlasLoadTexture("ugc_room_door_connector");
	g_RotateIconBL = atlasLoadTexture("UGC_Widgets_Rotators_BottomLeft");
	g_RotateIconBR = atlasLoadTexture("UGC_Widgets_Rotators_BottomRight");
	g_RotateIconTL = atlasLoadTexture("UGC_Widgets_Rotators_TopLeft");
	g_RotateIconTR = atlasLoadTexture("UGC_Widgets_Rotators_TopRight");
	g_LineMarker = "UGC_Map_PipConnector_Line";
	g_ArrowMarker = "UGC_Map_PipConnector_Arrow";

	g_ComponentMarkers[UGC_COMPONENT_TYPE_KILL] = atlasLoadTexture( "UGC_Icons_Map_Critter_Group" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_ACTOR] = atlasLoadTexture( "UGC_Icons_Map_Critter" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_CONTACT] = atlasLoadTexture( "UGC_Icons_Map_NPC" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_OBJECT] = atlasLoadTexture( "UGC_Icons_Map_Object" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_SOUND] = atlasLoadTexture( "UGC_Icons_Map_Audio" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_DESTRUCTIBLE] = atlasLoadTexture( "white" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_ROOM_DOOR] = atlasLoadTexture( "UGC_Icons_Map_Door" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_FAKE_DOOR] = atlasLoadTexture( "UGC_Icons_Map_Door" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_BUILDING_DEPRECATED] = atlasLoadTexture( "white" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_ROOM_MARKER] = atlasLoadTexture( "UGC_Icons_Map_Marker" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_PLANET] = atlasLoadTexture( "white" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_SPAWN] = atlasLoadTexture( "UGC_Icons_Map_Spawn" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_RESPAWN] = atlasLoadTexture( "UGC_Icons_Map_Spawn_Respawn" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_COMBAT_JOB] = atlasLoadTexture( "UGC_Icons_Map_Critter_Ambush" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_DIALOG_TREE] = atlasLoadTexture( "white" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_TRAP] = atlasLoadTexture( "UGC_Icons_Map_Trap" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_TRAP_TARGET] = atlasLoadTexture( "UGC_Icons_Map_Trap_Target" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_TRAP_TRIGGER] = atlasLoadTexture( "UGC_Icons_Map_Trap_Tile" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_TRAP_EMITTER] = atlasLoadTexture( "UGC_Icons_Map_Trap_Emitter" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_PATROL_POINT] = atlasLoadTexture( "UGC_Icons_Map_Critter_Patrol" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_TELEPORTER] = atlasLoadTexture( "white" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_TELEPORTER_PART] = atlasLoadTexture( "UGC_Icons_Map_Teleporter" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_CLUSTER] = atlasLoadTexture( "UGC_Icons_Map_Object_Group" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_CLUSTER_PART] = atlasLoadTexture( "UGC_Icons_Map_Object" );
	g_ComponentMarkers[UGC_COMPONENT_TYPE_REWARD_BOX] = atlasLoadTexture( "UGC_Icons_Map_Treasure" );

	g_ComponentTintColor[UGC_COMPONENT_TYPE_KILL] = 0xE14C37FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_ACTOR] = 0xE14C37FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_CONTACT] = 0xEBA600FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_OBJECT] = 0xD9A64CFF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_SOUND] = 0x0095E4FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_DESTRUCTIBLE] = -1;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_ROOM_DOOR] = 0x99A8B7FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_FAKE_DOOR] = 0x99A8B7FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_BUILDING_DEPRECATED] = -1;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_ROOM_MARKER] = 0x0095E4FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_PLANET] = -1;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_SPAWN] = 0x0095E4FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_RESPAWN] = 0x0095E4FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_COMBAT_JOB] = 0xE14C37FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_DIALOG_TREE] = -1;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_TRAP] = 0x8EA5C3FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_TRAP_TARGET] = 0x8EA5C3FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_TRAP_TRIGGER] = 0x8EA5C3FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_TRAP_EMITTER] = 0x8EA5C3FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_PATROL_POINT] = 0xE14C37FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_TELEPORTER] = 0x9A4BD3FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_TELEPORTER_PART] = 0x9A4BD3FF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_CLUSTER] = 0xD9A64CFF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_CLUSTER_PART] = 0xD9A64CFF;
	g_ComponentTintColor[UGC_COMPONENT_TYPE_REWARD_BOX] = 0xC19242FF;

	g_ComponentIcons[UGC_COMPONENT_TYPE_KILL] = "ugc_icon_encounter";
	g_ComponentIcons[UGC_COMPONENT_TYPE_ACTOR] = "ugc_icon_encounter";
	g_ComponentIcons[UGC_COMPONENT_TYPE_CONTACT] = "ugc_icon_contact";
	g_ComponentIcons[UGC_COMPONENT_TYPE_OBJECT] = "ugc_icon_object";
	g_ComponentIcons[UGC_COMPONENT_TYPE_SOUND] = "ugc_icon_sound";
	g_ComponentIcons[UGC_COMPONENT_TYPE_DESTRUCTIBLE] = "ugc_icon_object";
	g_ComponentIcons[UGC_COMPONENT_TYPE_ROOM] = "ugc_icon_room";
	g_ComponentIcons[UGC_COMPONENT_TYPE_ROOM_DOOR] = "ugc_icon_door";
	g_ComponentIcons[UGC_COMPONENT_TYPE_FAKE_DOOR] = "ugc_icon_doorclosed";
	g_ComponentIcons[UGC_COMPONENT_TYPE_BUILDING_DEPRECATED] = "ugc_icon_object";
	g_ComponentIcons[UGC_COMPONENT_TYPE_ROOM_MARKER] = "ugc_icon_marker";
	g_ComponentIcons[UGC_COMPONENT_TYPE_PLANET] = "ugc_icon_object";
	g_ComponentIcons[UGC_COMPONENT_TYPE_SPAWN] = "UGC_Icon_Spawn";
	g_ComponentIcons[UGC_COMPONENT_TYPE_RESPAWN] = "UGC_Icon_Respawn";
	g_ComponentIcons[UGC_COMPONENT_TYPE_COMBAT_JOB] = "UGC_Icon_object";
	g_ComponentIcons[UGC_COMPONENT_TYPE_DIALOG_TREE] = "UGC_Icon_DialogTree";
	g_ComponentIcons[UGC_COMPONENT_TYPE_TRAP] = "UGC_icon_trap";
	g_ComponentIcons[UGC_COMPONENT_TYPE_TRAP_TARGET] = "UGC_icon_trap_target";
	g_ComponentIcons[UGC_COMPONENT_TYPE_TRAP_TRIGGER] = "UGC_icon_object";
	g_ComponentIcons[UGC_COMPONENT_TYPE_TRAP_EMITTER] = "UGC_icon_object";
	g_ComponentIcons[UGC_COMPONENT_TYPE_PATROL_POINT] = "ugc_patrol_point";
	g_ComponentIcons[UGC_COMPONENT_TYPE_TELEPORTER] = "ugc_icon_object";
	g_ComponentIcons[UGC_COMPONENT_TYPE_TELEPORTER_PART] = "ugc_icon_object";
	g_ComponentIcons[UGC_COMPONENT_TYPE_CLUSTER] = "ugc_icon_object";
	g_ComponentIcons[UGC_COMPONENT_TYPE_CLUSTER_PART] = "ugc_icon_object";
	g_ComponentIcons[UGC_COMPONENT_TYPE_REWARD_BOX] = "ugc_icon_object";

	inited = true;
}

/// A light wrapper aronud ugcEditorFindComponentByID that supports
/// the temporary component as ID=0
UGCComponent* ugcLayoutGetComponentByID(U32 id)
{
	if (id == 0)
		return &g_TemporaryComponent;
	return ugcEditorFindComponentByID(id);
}

bool ugcLayoutComponentCanRotate(UGCComponentType type)
{
	if(   type == UGC_COMPONENT_TYPE_ROOM_MARKER
		  || type == UGC_COMPONENT_TYPE_SOUND
		  || type == UGC_COMPONENT_TYPE_DIALOG_TREE
		  || type == UGC_COMPONENT_TYPE_FAKE_DOOR
		  || type == UGC_COMPONENT_TYPE_ROOM_DOOR
		  || type == UGC_COMPONENT_TYPE_PATROL_POINT
		  || type == UGC_COMPONENT_TYPE_TRAP_TARGET
		  || type == UGC_COMPONENT_TYPE_TRAP_TRIGGER
		  || type == UGC_COMPONENT_TYPE_TELEPORTER ) {
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////

static void ugcMapUIDestroyMouseEventInternal( UGCMapUIMouseEvent* event )
{
	eaiDestroy(&event->door_types);
	free( event->debug_label );
	SAFE_FREE(event);
}

static void ugcMapUIDestroyMouseEvent(UGCMapEditorDoc* doc, UGCMapUIMouseEvent** ppEvent)
{
	if( !*ppEvent ) {
		return;
	}
	
	eaFindAndRemove( &doc->events, *ppEvent );	
	ugcMapUIDestroyMouseEventInternal( *ppEvent );	
	*ppEvent = NULL;
}

UGCMapUIMouseEvent *ugcMapUIDoMouseEventTestEx(UGCMapEditorDoc *doc, MouseButton button, CBox *box, bool circle, UGCMapUIMouseEventPriority priority, UGCMapUIMouseEventFn cb, const char *debug_fmt, ...)
{
	UGCMapUIMouseEventType type = UGC_MOUSE_EVENT_NONE;
	char debug_label[256];
	va_list ap;
	int mouseX = 0;
	int mouseY = 0;

	if (mouseDoubleClickHit(button, box)) {
		type = UGC_MOUSE_EVENT_DOUBLE_CLICK;
		mouseDoubleClickCoords(button, &mouseX, &mouseY);
	} else if (mouseClickHit(button, box)) {
		type = UGC_MOUSE_EVENT_CLICK;
		mouseClickCoords(button, &mouseX, &mouseY);
	} else if (mouseDragHit(button, box)) {
		type = UGC_MOUSE_EVENT_DRAG;
		mouseDragCoords(button, &mouseX, &mouseY);
	} else if (button == MS_LEFT && mouseCollision(box)) {
		type = UGC_MOUSE_EVENT_HOVER;
		mousePos(&mouseX, &mouseY);
	}

	if( !type ) {
		return NULL;
	}

	if (circle) {
		int d;
		d = SQR( mouseX - (box->left + box->right) / 2 ) + SQR( mouseY - (box->top + box->bottom) / 2 );
		if( d > SQR( box->right - box->left ) / 4 ) {
			return NULL;
		}
	}

	va_start(ap, debug_fmt);
	vsprintf(debug_label,debug_fmt,ap);
	va_end(ap);

	{
		UGCMapUIMouseEvent* ret = calloc( 1, sizeof( *ret ));
		ret->debug_label = strdup( debug_label );
		ret->position[0] = mouseX;
		ret->position[1] = mouseY;
		ret->type = type;
		ret->distance = 0;
		ret->priority = priority;
		ret->callback = cb;
		ret->button = button;
		eaPush(&doc->events, ret);
		return ret;
	}
}

// Returns -1 if A > B, 0 if A = B, and 1 if A < B
int ugcMapUIMouseEventCompare(const UGCMapUIMouseEvent **lhs, const UGCMapUIMouseEvent **rhs)
{
	const UGCMapUIMouseEvent* eventA = *lhs;
	const UGCMapUIMouseEvent *eventB = *rhs;
	
	// Selected comes before nonselected 
	if(eventA->is_selected && !eventB->is_selected)
		return -1;
	if(eventB->is_selected && !eventA->is_selected)
		return 1;

	// Drags always come before clicks
	if (eventA->type == UGC_MOUSE_EVENT_DRAG && eventB->type != UGC_MOUSE_EVENT_DRAG)
		return -1;
	if (eventB->type == UGC_MOUSE_EVENT_DRAG && eventA->type != UGC_MOUSE_EVENT_DRAG)
		return 1;
	// Clicks always come before hovers
	if (eventA->type == UGC_MOUSE_EVENT_CLICK && eventB->type != UGC_MOUSE_EVENT_CLICK)
		return -1;
	if (eventB->type == UGC_MOUSE_EVENT_CLICK && eventA->type != UGC_MOUSE_EVENT_CLICK)
		return 1;
	// With priority, lower is better
	if (eventA->priority < eventB->priority)
		return -1;
	if (eventB->priority < eventA->priority)
		return 1;
	// With distance, lower is also better
	if (eventA->distance < eventB->distance)
		return -1;
	if (eventB->distance < eventA->distance)
		return 1;
	return 0;
}

////////////////////////////////////////////////////////////////
// Dragging things around
////////////////////////////////////////////////////////////////

void ugcMapUIStartDrag(UGCMapEditorDoc *doc, MouseButton button, int mouse_offset_x, int mouse_offset_y,
					   UGCMapUIDragType type, bool show_trash, UGCMapUIDragFn cb)
{
	if (doc->drag_state) {
		return;
	}

	doc->drag_state = calloc( 1, sizeof( *doc->drag_state ));
	doc->drag_state->type = type;
	doc->drag_state->mouse_offset[0] = mouse_offset_x;
	doc->drag_state->mouse_offset[1] = mouse_offset_y;
	{
		Vec2 uiCoords;
		Vec3 worldCoords;

		uiCoords[0] = (g_ui_State.mouseX - doc->backdrop_last_box.lx) / doc->layout_scale;
		uiCoords[1] = (g_ui_State.mouseY - doc->backdrop_last_box.ly) / doc->layout_scale;
		ugcLayoutGetWorldCoords(doc, uiCoords, worldCoords );

		doc->drag_state->click_world_xz[0] = worldCoords[0];
		doc->drag_state->click_world_xz[1] = worldCoords[2];
	}
	doc->drag_state->click_button = button;
	doc->drag_state->callback = cb;
	doc->drag_state->show_trash = show_trash;

	if( doc->drag_state->show_trash ) {
		// Show "Trash"
		ui_PaneAddChild( doc->layout_pane, doc->trash_button );
	}

	// When dragging, the focus is always on the backdrop
	ui_SetFocus( &g_UGCMapEditorBackdropWidgetPlaceholder );
}

static void ugcMapUIStopDrag(UGCMapEditorDoc *doc, bool cancel)
{
	UGCMapUIDragEvent event;
	if( cancel ) {
		event = UGC_DRAG_CANCELLED;
	} else if ( doc->drag_state->show_trash
				&& (UI_WIDGET( doc->trash_button )->state & kWidgetModifier_Hovering) ) {
		event = UGC_DRAG_DELETED;
	} else {
		event = UGC_DRAG_DROPPED;
	}
	
	doc->drag_state->callback( doc, event );
	SAFE_FREE( doc->drag_state );

	// Hide "Trash"
	ui_PaneRemoveChild( doc->layout_pane, doc->trash_button );

	ugcEditorApplyUpdate();
}

void ugcMapUICancelAction(UGCMapEditorDoc *doc)
{
	if( doc->drag_state ) {
		ugcMapUIStopDrag( doc, true );
		return;
	}
	
	ugcMapEditorClearSelection( doc );
	ugcEditorQueueUIUpdate();
}

bool ugcMapUIIsComponentDragging(UGCMapEditorDoc *doc, U32 component_id)
{
	if( doc->drag_state && doc->drag_state->primary_widget) {
		if( doc->drag_state->primary_widget->uComponentID == component_id ) {
			return true;
		}
		
		FOR_EACH_IN_EARRAY( doc->drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget ) {
			if( secondary_widget->uComponentID == component_id ) {
				return true;
			}
		} FOR_EACH_END;
		FOR_EACH_IN_EARRAY( doc->drag_state->tethered_widgets, UGCUIMapEditorComponent, tethered_widget ) {
			if( tethered_widget->uComponentID == component_id ) {
				return true;
			}
		} FOR_EACH_END;
	}
	return false;
}

void ugcDragOncePerFrame(UGCMapEditorDoc *doc)
{
	Vec2 scale_pos;

	if( doc->drag_state->type == UGC_DRAG_MOVE_COMPONENT ) {
		if( doc->drag_state->valid_pos ) {
			ui_SetCursorByName( "UGC_Cursors_Move" );
		} else {
			ui_SetCursorByName( "UGC_Cursors_Move_Error" );
		}
	} else if( doc->drag_state->type == UGC_DRAG_ROTATE_COMPONENT ) {
		ugcEditorSetCursorForRotation( ugcDragGetCurrentRotation( doc ));
	} else if( doc->drag_state->type == UGC_DRAG_MMB_PAN ) {
		ui_SetCursorByName( "UGC_Cursors_Hand_Closed" );
	} else {
		// use the default cursor
	}
	ui_SoftwareCursorThisFrame();
	ui_CursorLock();

	// Reconstruct world position from screen position
	setVec2( scale_pos,
			 (g_ui_State.mouseX - doc->backdrop_last_box.lx) / doc->layout_scale - doc->drag_state->mouse_offset[ 0 ],
			 (g_ui_State.mouseY - doc->backdrop_last_box.ly) / doc->layout_scale - doc->drag_state->mouse_offset[ 1 ]);
	ugcLayoutGetWorldCoords( doc, scale_pos, doc->drag_state->world_pos );

	doc->drag_state->trash_highlighted = (UI_WIDGET( doc->trash_button )->state == kWidgetModifier_Hovering);
	doc->drag_state->callback( doc, UGC_DRAG_DRAGGING );

	// Detect end of drag
	if( !mouseIsDown( doc->drag_state->click_button )) {
		if ( !unfilteredMouseCollision( &doc->layout_last_box )) {
			ugcMapUIStopDrag( doc, true );
		} else {
			ugcMapUIStopDrag( doc, false );
		}
	}

	if( doc->map_widget ) {
		if( doc->drag_state ) {
			doc->map_widget->forceAutoEdgePan = true;
		} else {
			doc->map_widget->forceAutoEdgePan = false;
		}
	}

	if( doc->trash_button ) {
		if( ui_IsHovering( UI_WIDGET( doc->trash_button ))) {
			ui_ButtonSetImage( doc->trash_button, "UGC_Icons_Map_Trashcan_Over" );
		} else {
			ui_ButtonSetImage( doc->trash_button, "UGC_Icons_Map_Trashcan_Idle" );
		}
	}

	// Free the queued widgets
	{
		int it;
		for( it = 0; it != eaSize( &g_eaUGCMapEditorWidgetFreeQueue ); ++it ) {
			UGCUIMapEditorWidget* widget = g_eaUGCMapEditorWidgetFreeQueue[ it ];
			if( widget ) {
				widget->freeF( widget );
			}
		}
		eaClear( &g_eaUGCMapEditorWidgetFreeQueue );
	}
}

////////////////////////////////////////////////////////////////
// Component widget
////////////////////////////////////////////////////////////////

UGCUIMapEditorComponent *ugcLayoutUIGetComponent(UGCMapEditorDoc *doc, UGCComponent *component)
{
	FOR_EACH_IN_EARRAY(doc->component_widgets, UGCUIMapEditorComponent, component_widget)
	{
		if (component_widget->uComponentID == component->uID)
			return component_widget;
	}
	FOR_EACH_END;
	return NULL;
}

UGCUIMapEditorComponent *ugcLayoutUIGetComponentByID(UGCMapEditorDoc *doc, U32 component_id)
{
	FOR_EACH_IN_EARRAY(doc->component_widgets, UGCUIMapEditorComponent, component_widget)
	{
		if (component_widget->uComponentID == component_id)
			return component_widget;
	}
	FOR_EACH_END;
	return NULL;
}

void ugcLayoutComponentFixupChildLocations(UGCComponent *src_component, Vec3 old_pos, Vec3 new_pos, Vec2 min_bounds, Vec2 max_bounds)
{
	// Fixup child locations
	if(ea32Size(&src_component->uChildIDs))
	{
		int i; 
		for(i=0; i<ea32Size(&src_component->uChildIDs); i++)
		{
			UGCComponent *child = ugcComponentFindByID(ugcEditorGetComponentList(), src_component->uChildIDs[i]);
			if(child)
			{
				Vec3 offset;
				Vec3 new_child_pos;
				subVec3(child->sPlacement.vPos, old_pos, offset);
				addVec3(new_pos, offset, new_child_pos);

				MAX1(new_child_pos[0], min_bounds[0]);
				MAX1(new_child_pos[2], min_bounds[1]);

				MIN1(new_child_pos[0], max_bounds[0]);
				MIN1(new_child_pos[2], max_bounds[1]);

				copyVec3(new_child_pos, child->sPlacement.vPos);

				ugcLayoutComponentFixupChildLocations(child, old_pos, new_pos, min_bounds, max_bounds);
			}
		}
	}
}

void ugcLayoutComponentFixupChildRotations(UGCComponent *src_component, UGCComponent *root_component, F32 old_rot, F32 new_rot)
{
	if(ea32Size(&src_component->uChildIDs))
	{
		int i;
		F32 yawDiff;
		yawDiff = subAngle(RAD(new_rot), RAD(old_rot));
		for(i=0; i<ea32Size(&src_component->uChildIDs); i++)
		{
			UGCComponent *child = ugcComponentFindByID(ugcEditorGetComponentList(), src_component->uChildIDs[i]);
			Vec3 offset;
			F32 yawRel;
			F32 newYaw;
			F32 dist;
			Vec3 new_pos;

			if(!child)
				continue;

			subVec3(child->sPlacement.vPos, root_component->sPlacement.vPos, offset);
			offset[1] = 0;
			dist = normalVec3(offset);
			yawRel = getVec3Yaw(offset);
			newYaw = addAngle(yawRel, yawDiff);
			new_pos[0] = dist * sinf(newYaw) + root_component->sPlacement.vPos[0];
			new_pos[2] = dist * cosf(newYaw) + root_component->sPlacement.vPos[2];

			child->sPlacement.vRotPYR[1] = DEG(subAngle(RAD(child->sPlacement.vRotPYR[1]), -yawDiff));
			child->sPlacement.vPos[0] = new_pos[0];
			child->sPlacement.vPos[2] = new_pos[2];

			ugcLayoutComponentFixupChildRotations(child, root_component, old_rot, new_rot);
		}
	}
}

int *ugcLayoutGetRoomLevels(UGCMapEditorDoc *doc)
{
	int *ret = NULL;
	FOR_EACH_IN_EARRAY(doc->component_widgets, UGCUIMapEditorComponent, component_widget)
	{
		if (component_widget->selected_level > -1)
		{
			eaiPush(&ret, component_widget->uComponentID);
			eaiPush(&ret, component_widget->selected_level);
		}
	}
	FOR_EACH_END;
	return ret;
}

static F32 ugcDragGetInitialRotation(UGCMapEditorDoc* doc)
{
	UGCComponent* primary_component = ugcLayoutGetComponentByID( doc->drag_state->primary_widget->uComponentID );

	return atan2( doc->drag_state->click_world_xz[ 0 ] - primary_component->sPlacement.vPos[ 0 ],
				  doc->drag_state->click_world_xz[ 1 ] - primary_component->sPlacement.vPos[ 2 ]);
}

static F32 ugcDragGetCurrentRotation(UGCMapEditorDoc* doc)
{
	UGCComponent* primary_component = ugcLayoutGetComponentByID( doc->drag_state->primary_widget->uComponentID );

	return atan2( doc->drag_state->world_pos[ 0 ] - primary_component->sPlacement.vPos[ 0 ],
				  doc->drag_state->world_pos[ 2 ] - primary_component->sPlacement.vPos[ 2 ]);
}

static int ugcDoorsSortByRoomsThenDistance( const UGCPotentialRoomDoor** ppDoor1, const UGCPotentialRoomDoor** ppDoor2 )
{
	U32 minRoom1 = MIN( (*ppDoor1)->roomID, (*ppDoor1)->connectedRoomID );
	U32 maxRoom1 = MAX( (*ppDoor1)->roomID, (*ppDoor1)->connectedRoomID );
	U32 minRoom2 = MIN( (*ppDoor2)->roomID, (*ppDoor2)->connectedRoomID );
	U32 maxRoom2 = MAX( (*ppDoor2)->roomID, (*ppDoor2)->connectedRoomID );

	if( minRoom1 != minRoom2 ) {
		return minRoom1 - minRoom2;
	}
	if( maxRoom1 != maxRoom2 ) {
		return maxRoom1 - maxRoom2;
	}
	
	return (*ppDoor1)->distanceFromIdeal - (*ppDoor2)->distanceFromIdeal;
}

// PRIMARY_COMPONENT needs to be passed in so that this can be called
// when dragging out a new component.
//
// SECONDARY_COMPONENT will always be filled with existing components,
// so they don't need to be passed in.
static void ugcMapEditorCreateDoors( UGCMapEditorDoc* doc, UGCComponent* primary_component )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCMapUIDragState* drag_state = doc->drag_state;
	UGCPotentialRoomDoor** eaDoors = NULL;

	if( primary_component->eType == UGC_COMPONENT_TYPE_ROOM ) {
		ugcMapEditorRoomCreateDoors( primary_component, &eaDoors );
	}
	FOR_EACH_IN_EARRAY( drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget ) {
		UGCComponent* secondary_component = ugcLayoutGetComponentByID( secondary_widget->uComponentID );
		if( secondary_component ) {
			ugcMapEditorRoomCreateDoors( secondary_component, &eaDoors );
		}
	} FOR_EACH_END;

	// Only create one door for each room, prioritized by how close
	// they are to the ideal position.
	{
		U32 prevMinRoom = -1;
		U32 prevMaxRoom = -1;
					
		int it;
		eaQSort( eaDoors, ugcDoorsSortByRoomsThenDistance );

		for( it = 0; it != eaSize( &eaDoors ); ++it ) {
			UGCPotentialRoomDoor* door = eaDoors[ it ];
			U32 minRoom = MIN( door->roomID, door->connectedRoomID );
			U32 maxRoom = MAX( door->roomID, door->connectedRoomID );

			if( minRoom == prevMinRoom && maxRoom == prevMaxRoom ) {
				free( door );
				eaRemove( &eaDoors, it );
				--it;
			}

			prevMinRoom = minRoom;
			prevMaxRoom = maxRoom;
		}
	}

	{
		int it;
		for( it = 0; it != eaSize( &eaDoors ); ++it ) {
			UGCPotentialRoomDoor* door = eaDoors[ it ];
			if( door->doorIdx >= 0 ) {
				UGCComponent* room = ugcEditorFindComponentByID( door->roomID );
				UGCMap* map = ugcMapFindByName( ugcProj, room->sPlacement.pcMapName );
				UGCRoomInfo* roomInfo = ugcRoomGetRoomInfo( room->iObjectLibraryId );
						
				UGCComponent* newDoor = ugcComponentOpCreate( ugcProj, UGC_COMPONENT_TYPE_ROOM_DOOR, door->roomID );
				Vec3 localPos;

				ugcComponentOpSetPlacement( ugcProj, newDoor, map, UGC_TOPLEVEL_ROOM_ID );
				ugcComponentOpReset( ugcProj, newDoor, ugcMapGetType( map ), true );

				ugcRoomGetDoorLocalPos( roomInfo, door->doorIdx, localPos );
				ugcRoomConvertLocalToWorld( room, localPos, newDoor->sPlacement.vPos );
				newDoor->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE;
				copyVec3( room->sPlacement.vRotPYR, newDoor->sPlacement.vRotPYR );
				newDoor->sPlacement.vRotPYR[1] = addAngleDeg( newDoor->sPlacement.vRotPYR[1], ugcRoomGetDoorLocalRot( roomInfo, door->doorIdx ));
				newDoor->iRoomDoorID = door->doorIdx;
			}
		}
	}
	eaDestroyEx( &eaDoors, NULL );
}

static void ugcMapEditorTetheredDrag(UGCMapEditorDoc *doc, UGCUIMapEditorComponent *primary_widget, UGCComponent *primary_component,
		UGCUIMapEditorComponent *tethered_widget, UGCComponent *tethered_component, Vec3 tethered_pos)
{
	UGCProjectData *ugcProj = ugcEditorGetProjectData();

	if (ugcMapGetType(doc->map_data) == UGC_MAP_TYPE_INTERIOR)
	{
		UGC_FOR_EACH_COMPONENT_OF_TYPE_ON_MAP(ugcProj->components, UGC_COMPONENT_TYPE_ROOM, primary_component->sPlacement.pcMapName, room_component)
		{
			if ( room_component != primary_component && !ugcComponentHasParent( ugcProj->components, room_component, primary_component->uID ))
			{
				UGCRoomInfo *room_info;

				if((!primary_widget->drag_valid || primary_widget->drag_position.room_id != room_component->uID) && primary_component->sPlacement.uRoomID != room_component->uID)
					continue; // Always going to keep tethered doors in same room

				room_info = ugcRoomGetRoomInfo(room_component->iObjectLibraryId);
				if (room_info)
				{
					if(tethered_component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR && (primary_component->eType == UGC_COMPONENT_TYPE_SPAWN || primary_component->eType == UGC_COMPONENT_TYPE_REWARD_BOX))
					{
						Mat4 item_matrix;
						identityMat4(item_matrix);

						// Calculate relative position
						ugcRoomConvertWorldToLocal(room_component, tethered_pos, item_matrix[3]);

						{
							int valid_door_idx = -1;
							F32 valid_door_dist = 1000;

							int door_idx = -1;
							Vec2 pos = { item_matrix[3][0], item_matrix[3][2] };
							F32 dist = ugcRoomGetNearestValidDoor(ugcProj, room_component, tethered_component, room_info, pos, &door_idx);
							if (door_idx > -1)
							{
								Vec3 door_local_pos, door_world_pos;
								Vec3 dir_new, dir_old;
								F32 dist_new, dist_old;
								ugcRoomGetDoorLocalPos(room_info, door_idx, door_local_pos);
								ugcRoomConvertLocalToWorld(room_component, door_local_pos, door_world_pos);
						
								subVec3(tethered_pos, door_world_pos, dir_new);
								dist_new = lengthVec3(dir_new);
								subVec3(tethered_pos, tethered_component->sPlacement.vPos, dir_old);
								dist_old = lengthVec3(dir_old);

								if(dist_new < dist_old - g_iUGCTetheringDragMargin)
								{
									copyVec3(door_world_pos, tethered_widget->drag_position.position);
									tethered_widget->drag_position.rotation = room_component->sPlacement.vRotPYR[1] + ugcRoomGetDoorLocalRot(room_info, door_idx);
									primary_widget->drag_position.rotation = tethered_widget->drag_position.rotation + 180.0f;
									primary_widget->drag_position.rotation = DEG(fixAngle(RAD(primary_widget->drag_position.rotation)));

									tethered_widget->drag_position.room_id = room_component->uID;
									tethered_widget->drag_position.room_door = door_idx;
									tethered_widget->drag_position.room_level = -1;

									tethered_widget->drag_valid = true;
								}
								else
								{
									copyVec3(tethered_component->sPlacement.vPos, tethered_widget->drag_position.position);
									tethered_widget->drag_position.rotation = tethered_component->sPlacement.vRotPYR[1];
									primary_widget->drag_position.rotation = primary_component->sPlacement.vRotPYR[1];

									tethered_widget->drag_position.room_id = tethered_component->uParentID;
									tethered_widget->drag_position.room_door = tethered_component->iRoomDoorID;
									tethered_widget->drag_position.room_level = -1;

									tethered_widget->drag_valid = true;
								}
							}
						}
					}
					else if((tethered_component->eType == UGC_COMPONENT_TYPE_SPAWN || tethered_component->eType == UGC_COMPONENT_TYPE_REWARD_BOX) && primary_component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR)
					{
						Vec3 spawn_world_pos;
						Vec3 dir_new, dir_old;
						F32 dist_new, dist_old;

						F32 normalized_rotation = primary_widget->drag_position.rotation;
						normalized_rotation = DEG(fixAngle(RAD(normalized_rotation)));

						copyVec3(primary_widget->drag_position.position, spawn_world_pos);
						if(normalized_rotation >= -45.0f && normalized_rotation < 45.0f)
							spawn_world_pos[2] -= UGC_KIT_GRID / 2.0f;
						else if(normalized_rotation >= 45.0f && normalized_rotation < 135.0f)
							spawn_world_pos[0] -=  UGC_KIT_GRID / 2.0f;
						else if(normalized_rotation >= -135.0f && normalized_rotation < -45.0f)
							spawn_world_pos[0] += UGC_KIT_GRID / 2.0f;
						else
							spawn_world_pos[2] += UGC_KIT_GRID / 2.0f;

						subVec3(tethered_pos, spawn_world_pos, dir_new);
						dist_new = lengthVec3(dir_new);
						subVec3(tethered_pos, tethered_component->sPlacement.vPos, dir_old);
						dist_old = lengthVec3(dir_old);

						if(dist_new < dist_old - g_iUGCTetheringDragMargin)
						{
							tethered_widget->drag_valid = ugcComponentIsValidPosition(
									ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(),
									tethered_component, spawn_world_pos, ugcLayoutGetRoomLevels(doc), false, 0, 0,
									&tethered_widget->drag_position);

							tethered_widget->drag_position.rotation = primary_widget->drag_position.rotation + 180.0f;
							tethered_widget->drag_position.rotation = DEG(fixAngle(RAD(tethered_widget->drag_position.rotation)));

							tethered_widget->drag_position.room_id = room_component->uID;
						}
						else
						{
							copyVec3(tethered_component->sPlacement.vPos, tethered_widget->drag_position.position);
							tethered_widget->drag_position.rotation = tethered_component->sPlacement.vRotPYR[1];

							tethered_widget->drag_position.room_id = tethered_component->uParentID;
							tethered_widget->drag_position.room_door = tethered_component->iRoomDoorID;
							tethered_widget->drag_position.room_level = tethered_component->sPlacement.iRoomLevel;

							tethered_widget->drag_valid = true;
						}
					}
				}
			}
		}
		UGC_FOR_EACH_COMPONENT_END;
	}
}

static void ugcMapUIComponentDragCB( UGCMapEditorDoc *doc, UGCMapUIDragEvent event )
{
	UGCMapUIDragState* drag_state = doc->drag_state;
	UGCComponent *primary_component = ugcLayoutGetComponentByID( drag_state->primary_widget->uComponentID);
	F32 snap, snap_y;
	bool snapping;
	UGCMapType map_type = ugcMapGetType(doc->map_data);

	if( !primary_component || doc->map_data->pUnitializedMap ) {
		return;
	}

	drag_state->world_pos[1] = primary_component->sPlacement.vPos[1];

	switch (drag_state->type)
	{
	case UGC_DRAG_MOVE_COMPONENT:

		if (event == UGC_DRAG_DRAGGING)
		{
			UGCMapType type = ugcMapGetType(doc->map_data);
			int *room_levels = ugcLayoutGetRoomLevels(doc);

			snapping = ugcMapEditorGetTranslateSnap(primary_component, &snap, &snap_y);
			drag_state->primary_widget->drag_valid = ugcComponentIsValidPosition(
					ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(),
					primary_component, drag_state->world_pos, room_levels, snapping, snap, snap_y,
					&drag_state->primary_widget->drag_position);
			drag_state->valid_pos = drag_state->primary_widget->drag_valid;

			FOR_EACH_IN_EARRAY(drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget)
			{
				UGCComponent *secondary_component = ugcLayoutGetComponentByID(secondary_widget->uComponentID);
				if (secondary_component)
				{
					Vec3 secondary_pos;
					addVec3(drag_state->primary_widget->drag_position.position, secondary_widget->drag_relative_position, secondary_pos);
					if( secondary_widget->drag_parent_room_is_dragging ) {
						secondary_widget->drag_valid = true;
						ugcComponentGetValidPosition( &secondary_widget->drag_position, ugcEditorGetProjectData(), secondary_component );
						copyVec3( secondary_pos, secondary_widget->drag_position.position );
					} else {
						secondary_widget->drag_valid = ugcComponentIsValidPosition(
								ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(),
								secondary_component, secondary_pos, room_levels, false, snap, snap_y,
								&secondary_widget->drag_position);
					}
					
					// Special exception -- Cluster object children
					// are allowed to be dragged to invalid places.
					//
					// See [NNO-12460]
					if( secondary_component->eType != UGC_COMPONENT_TYPE_CLUSTER_PART ) {
						drag_state->valid_pos = drag_state->valid_pos && secondary_widget->drag_valid;
					}
				}
			}
			FOR_EACH_END;

			FOR_EACH_IN_EARRAY(drag_state->tethered_widgets, UGCUIMapEditorComponent, tethered_widget)
			{
				UGCComponent *tethered_component = ugcLayoutGetComponentByID(tethered_widget->uComponentID);
				if (tethered_component)
				{
					ugcMapEditorTetheredDrag(doc, drag_state->primary_widget, primary_component, tethered_widget, tethered_component, drag_state->primary_widget->drag_position.position);
					drag_state->valid_pos = drag_state->valid_pos && tethered_widget->drag_valid;
				}
			}
			FOR_EACH_END;

			eaiDestroy(&room_levels);
		}
		else if (event == UGC_DRAG_DROPPED)
		{
			UGCComponentPlacement *placement;
			bool isNewComponent = false;
			bool zoom_and_pan_to_object = false;
			F32 new_height = 0;
			if (!drag_state->valid_pos) {
				return;
			}

			if (primary_component->uID == 0)
			{
				// Dragging from Asset Library; create a new component and initialize that instead
				UGCComponent *new_component = ugcComponentOpCreate(ugcEditorGetProjectData(), primary_component->eType, drag_state->primary_widget->drag_position.room_id);
				const WorldUGCProperties* ugcProps = ugcResourceGetUGCPropertiesInt( "ObjectLibrary", primary_component->iObjectLibraryId );
				GroupDef *def;
				U32 id = new_component->uID;
				StructCopy(parse_UGCComponent, primary_component, new_component, 0, 0, 0);
				new_component->uID = id;
				new_component->uParentID = drag_state->primary_widget->drag_position.room_id;
				primary_component = new_component;
				isNewComponent = true;

				if( primary_component->eType != UGC_COMPONENT_TYPE_ROOM_DOOR && primary_component->eType != UGC_COMPONENT_TYPE_FAKE_DOOR ) {
					zoom_and_pan_to_object = true;
				}

				def = objectLibraryGetGroupDef(primary_component->iObjectLibraryId, false);
				if (ugcProps)
				{
					new_height = ugcProps->fMapDefaultHeight;
				}
			}
			placement = &primary_component->sPlacement;

			// Abort a cut if we are moving this component
			ugcMapEditorAbortCutForComponent(primary_component->uID);

			// Move component to final position
			ugcMapEditorComponentPlace(primary_component, doc->map_data, isNewComponent);
			ugcComponentSetValidPosition(ugcEditorGetProjectData(), primary_component, &drag_state->primary_widget->drag_position);
			if(   primary_component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR
				  || primary_component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR ) {
				primary_component->bIsDoorExplicitDefault = true;
			}
			placement->vPos[1] += new_height;
			if( isNewComponent ) {
				ugcComponentCreateChildrenForInitialDrag( ugcEditorGetProjectData(), primary_component );
			}

			// Move secondary components
			FOR_EACH_IN_EARRAY(drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget)
			{
				UGCComponent *secondary_component = ugcLayoutGetComponentByID(secondary_widget->uComponentID);
				if (secondary_component)
				{
					ugcMapEditorComponentPlace(secondary_component, doc->map_data, /*keep_object=*/false);
					ugcComponentSetValidPosition(ugcEditorGetProjectData(), secondary_component, &secondary_widget->drag_position);
					if(   (secondary_component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR
						   || secondary_component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR)
						  && !secondary_widget->drag_is_implicit_child ) {
						secondary_component->bIsDoorExplicitDefault = true;
					}
				}
			}
			FOR_EACH_END;

			// Move tethered components
			FOR_EACH_IN_EARRAY(drag_state->tethered_widgets, UGCUIMapEditorComponent, tethered_widget)
			{
				UGCComponent *tethered_component = ugcLayoutGetComponentByID(tethered_widget->uComponentID);
				if (tethered_component)
				{
					ugcMapEditorComponentPlace(tethered_component, doc->map_data, /*keep_object=*/false);
					ugcComponentSetValidPosition(ugcEditorGetProjectData(), tethered_component, &tethered_widget->drag_position);
				}
			}
			FOR_EACH_END;

			ugcMapEditorCreateDoors( doc, primary_component );

			// Update UI
			ugcEditorStartObjectFlashing(primary_component);
			ugcLayoutUIComponentUpdate(drag_state->primary_widget);
			
			// Create a widget if it doesn't already exist
			if (!ugcLayoutUIGetComponent(doc, primary_component))
			{
				ugcLayoutUIComponentCreate(doc, primary_component, false);
			}

			FOR_EACH_IN_EARRAY(drag_state->tethered_widgets, UGCUIMapEditorComponent, tethered_widget)
			{
				ugcLayoutUIComponentUpdate(tethered_widget);
			}
			FOR_EACH_END;

			ugcEditorQueueUIUpdate();
		}
		else if (event == UGC_DRAG_DELETED)
		{
			bool anyIsActor = false;
			
			if( !ugcLayoutCanDeleteComponent( primary_component )) {
				return;
			}
			if( primary_component->eType == UGC_COMPONENT_TYPE_ACTOR ) {
				anyIsActor = true;
			}

			FOR_EACH_IN_EARRAY(drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget)
			{
				UGCComponent *secondary_component = ugcLayoutGetComponentByID(secondary_widget->uComponentID);
				if (secondary_component && !secondary_widget->drag_is_implicit_child)
				{
					if (!ugcLayoutCanDeleteComponent(secondary_component)) {
						return;
					}
					if( secondary_component->eType == UGC_COMPONENT_TYPE_ACTOR ) {
						anyIsActor = true;
					}
				}
			}
			FOR_EACH_END;

			if( anyIsActor ) {
				if( ugcModalDialogMsg( "UGC_MapEditor.DeleteActorEncounter_Title", "UGC_MapEditor.DeleteActorEncounter_Body", UIYes | UINo ) != UIYes ) {
					return;
				}
			}

			// Abort a cut if we are deleting this component & delete the component
			ugcMapEditorAbortCutForComponent(primary_component->uID);
			ugcLayoutDeleteComponent(primary_component);

			// Do the same for rest of selection
			FOR_EACH_IN_EARRAY(drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget)
			{
				UGCComponent *secondary_component = ugcLayoutGetComponentByID(secondary_widget->uComponentID);
				if (secondary_component)
				{
					ugcMapEditorAbortCutForComponent(secondary_component->uID);
					ugcLayoutDeleteComponent(secondary_component);
				}
			}
			FOR_EACH_END;
		}
		else if (event == UGC_DRAG_CANCELLED)
		{
			if (primary_component->uID == 0) {
				return;
			}

			// DO NOTHING...
		}
		break;
	case UGC_DRAG_ROTATE_COMPONENT:
		{
			UGCComponentPlacement *placement;
			UGCComponentValidPosition *final_position = &drag_state->primary_widget->drag_position;

			copyVec3(primary_component->sPlacement.vPos, final_position->position);

			placement = &primary_component->sPlacement;

			if(event == UGC_DRAG_DRAGGING)
			{
				F32 delta_x, delta_z;
				F32 delta_rotation;
				Mat3 rotation_mat;				
				int *room_levels = ugcLayoutGetRoomLevels(doc);				
				float initial_rotation = ugcDragGetInitialRotation(doc);

				delta_x = drag_state->world_pos[0] - placement->vPos[0];
				delta_z = drag_state->world_pos[2] - placement->vPos[2];

				printf( "%f,%f - %f,%f (offset %d,%d)\n", drag_state->world_pos[0], drag_state->world_pos[2],
						placement->vPos[0], placement->vPos[2],
						doc->drag_state->mouse_offset[0], doc->drag_state->mouse_offset[1]);
				
				final_position->rotation = placement->vRotPYR[1] + DEG(atan2(delta_x, delta_z) - initial_rotation);
				if (ugcMapEditorGetRotateSnap(primary_component->eType, &snap))
				{
					if (drag_state->door_snap_offset)
						final_position->rotation = floor((final_position->rotation-90)/snap+0.5f)*snap + 90;
					else
						final_position->rotation = floor(final_position->rotation/snap+0.5f)*snap;
				}

				delta_rotation = final_position->rotation - placement->vRotPYR[1];
				identityMat3(rotation_mat);
				yawMat3(RAD(delta_rotation), rotation_mat);

				drag_state->primary_widget->drag_valid = true;
				drag_state->valid_pos = true;

				// Rotate secondaries around primary component
				FOR_EACH_IN_EARRAY(drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget)
				{
					UGCComponent *secondary_component = ugcLayoutGetComponentByID(secondary_widget->uComponentID);
					if (secondary_component)
					{
						Vec3 secondary_pos;
						Vec3 rotated_offset;
						F32 old_rotation;

						mulVecMat3(secondary_widget->drag_relative_position, rotation_mat, rotated_offset);
						addVec3(primary_component->sPlacement.vPos, rotated_offset, secondary_pos);

						old_rotation = secondary_component->sPlacement.vRotPYR[1];
						secondary_component->sPlacement.vRotPYR[1] += delta_rotation;

						snapping = ugcMapEditorGetTranslateSnap(secondary_component, &snap, &snap_y);
						if( secondary_widget->drag_parent_room_is_dragging ) {
							secondary_widget->drag_valid = true;
							ugcComponentGetValidPosition( &secondary_widget->drag_position, ugcEditorGetProjectData(), secondary_component );
							copyVec3( secondary_pos, secondary_widget->drag_position.position );
						} else {
							secondary_widget->drag_valid = ugcComponentIsValidPosition(
									ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(),
									secondary_component, secondary_pos, room_levels, false, snap, snap_y,
									&secondary_widget->drag_position);
						}
						drag_state->valid_pos = drag_state->valid_pos && secondary_widget->drag_valid;

						secondary_component->sPlacement.vRotPYR[1] = old_rotation;
					}
				}
				FOR_EACH_END;

				eaiDestroy(&room_levels);
			}
			else if(event == UGC_DRAG_DROPPED)
			{
				if (!drag_state->valid_pos) {
					return;
				}

				placement->vRotPYR[1] = final_position->rotation;
				
				// Move & rotate secondary components
				FOR_EACH_IN_EARRAY(drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget)
				{
					UGCComponent *secondary_component = ugcLayoutGetComponentByID(secondary_widget->uComponentID);
					if (secondary_component)
					{
						ugcComponentSetValidPosition(ugcEditorGetProjectData(), secondary_component, &secondary_widget->drag_position);
					}
				}
				FOR_EACH_END;

				ugcMapEditorCreateDoors( doc, primary_component );
			}
		}
		break;
	}
}

bool ugcLayoutCanDeleteComponent(UGCComponent *component)
{
	// Sadly, Tom coded the patrol points to always use the new
	// component.  But they never are a new component.
	if( component->uID == 0 && component->eType != UGC_COMPONENT_TYPE_PATROL_POINT ) {
		return false;
	}

	if(   component->eType == UGC_COMPONENT_TYPE_WHOLE_MAP
		  || component->eType == UGC_COMPONENT_TYPE_TRAP_TARGET
		  || component->eType == UGC_COMPONENT_TYPE_REWARD_BOX) {
		return false;
	}

	// Is this the last spawn point?
	if (component->eType == UGC_COMPONENT_TYPE_SPAWN)
	{
		UGCMap **maps = ugcEditorGetMapsList();
		UGCComponentList *components = ugcEditorGetComponentList();
		FOR_EACH_IN_EARRAY(maps, UGCMap, map)
		{
			if (resNamespaceBaseNameEq(map->pcName, component->sPlacement.pcMapName))
			{
				int found_count = 0;
				FOR_EACH_IN_EARRAY(components->eaComponents, UGCComponent, it_component)
				{
					if (it_component->eType == UGC_COMPONENT_TYPE_SPAWN &&
						resNamespaceBaseNameEq(map->pcName, it_component->sPlacement.pcMapName))
					{
						found_count++;
					}
				}
				FOR_EACH_END;
				if (found_count == 1)
					return false;
				break;
			}
		}
		FOR_EACH_END;
	}
	
	return true;
}

void ugcLayoutDeleteComponent(UGCComponent *component)
{
	if( component->eType == UGC_COMPONENT_TYPE_ACTOR ) {
		component = ugcEditorFindComponentByID( component->uParentID );
	}

	if( component ) {
		ugcComponentOpDelete( ugcEditorGetProjectData(), component, false );
	}
}

UGCMapEditorMode ugcLayoutComponentDragNewMode( UGCMapEditorDoc* doc, UGCComponent* component )
{
	// RoomDoors are special, they are visible in every mode, but the
	// handles are different.
	if(   component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR
		  || component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR ) {
		return doc->mode;
	}
	
	// This should be kept in sync with ugcLayoutComponentHandleAlpha,
	// so that new components are always visible.
	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_SPAWN: case UGC_COMPONENT_TYPE_PATROL_POINT:
		case UGC_COMPONENT_TYPE_CONTACT:
			return doc->mode;

		xcase UGC_COMPONENT_TYPE_KILL: case UGC_COMPONENT_TYPE_CLUSTER:
		case UGC_COMPONENT_TYPE_ROOM:
			return UGC_MAP_EDITOR_LAYOUT;

		xdefault:
			return UGC_MAP_EDITOR_DETAIL;
	}
}

int ugcLayoutComponentHandleAlpha( UGCMapEditorDoc* doc, UGCComponent* component )
{
	if( !component ) {
		return 0;
	}
	
	// This should be kept in sync with ugcLayoutComponentDragNewMode,
	// so that new components are always visible.
	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_SPAWN: case UGC_COMPONENT_TYPE_PATROL_POINT:
		case UGC_COMPONENT_TYPE_CONTACT:
			return 255;

		xcase UGC_COMPONENT_TYPE_KILL: case UGC_COMPONENT_TYPE_CLUSTER:
		case UGC_COMPONENT_TYPE_ROOM:
			return 255 - doc->rooms_fade * 255;

		xdefault:
			return 255 - doc->objects_fade * 255;
	}
}

static void ugcDragAddChildrenToSecondary(UGCMapEditorDoc* doc, UGCUIMapEditorComponent* componentUI, UGCComponent* primaryComponent)
{
	UGCComponent* component = ugcLayoutGetComponentByID(componentUI->uComponentID);
	int it;
	for( it = 0; it != eaiSize( &component->uChildIDs ); ++it ) {
		UGCComponent* child = ugcLayoutGetComponentByID( component->uChildIDs[ it ]);
		UGCUIMapEditorComponent* childUI = ugcLayoutUIGetComponentByID( doc, component->uChildIDs[ it ]);

		if( childUI && child && child != primaryComponent ) {
			if( eaFind( &doc->drag_state->secondary_widgets, childUI ) == -1 ) {
				subVec3( child->sPlacement.vPos, primaryComponent->sPlacement.vPos, childUI->drag_relative_position );
				childUI->drag_is_implicit_child = true;
				eaPush( &doc->drag_state->secondary_widgets, childUI );
			}
		}
	}
}

static void ugcLayoutGetTetheredComponents(UGCMapEditorDoc* doc, UGCComponent *primary_component, UGCComponent ***eaTetheredComponents)
{
	char baseMapName[RESOURCE_NAME_MAX_SIZE];
	UGCMissionMapLink *mapLink = ugcMissionFindLinkByMap( ugcEditorGetProjectData(), doc->map_data->pcName );
	U32 spawnComponentID = 0;
	U32 exitDoorComponentID = 0;

	resExtractNameSpace_s(doc->map_data->pcName, NULL, 0, SAFESTR(baseMapName));

	if(SAFE_MEMBER(mapLink, uSpawnComponentID))
		spawnComponentID = mapLink->uSpawnComponentID;
	else
	{
		UGCComponent *defaultSpawnComponent = ugcMissionGetDefaultComponentForMap(ugcEditorGetProjectData(), UGC_COMPONENT_TYPE_SPAWN, doc->map_data->pcName);
		if(defaultSpawnComponent)
			spawnComponentID = defaultSpawnComponent->uID;
	}

	{
		UGCMissionMapLink *link = ugcMissionFindLinkByExitMap(ugcEditorGetProjectData(), doc->map_data->pcName);
		if(link && link->uDoorComponentID)
			exitDoorComponentID = link->uDoorComponentID;
	}

	if(spawnComponentID) // if we have a valid start spawn on this map (we should)
	{
		if(primary_component->eType == UGC_COMPONENT_TYPE_SPAWN) // if our primary is a spawn
		{
			if(spawnComponentID == primary_component->uID) // and it is the start spawn
			{
				// find the abort exit and tether it
				UGC_FOR_EACH_COMPONENT_OF_TYPE_ON_MAP(ugcEditorGetComponentList(), UGC_COMPONENT_TYPE_FAKE_DOOR, baseMapName, component)
				{
					if(component->bInteractIsMissionReturn)
						eaPush(eaTetheredComponents, component);
				}
				UGC_FOR_EACH_COMPONENT_END;
			}
		}
		else if(primary_component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR && primary_component->bInteractIsMissionReturn) // if our primary is the abort exit
		{
			// find the start spawn and tether it
			UGC_FOR_EACH_COMPONENT_OF_TYPE_ON_MAP(ugcEditorGetComponentList(), UGC_COMPONENT_TYPE_SPAWN, baseMapName, component)
			{
				if(spawnComponentID == component->uID)
					eaPush(eaTetheredComponents, component);
			}
			UGC_FOR_EACH_COMPONENT_END;
		}
	}

	if(exitDoorComponentID) // if we have a valid exit door on this map
	{
		if(primary_component->uID == exitDoorComponentID) // if our primary is the exit door
		{
			// find the reward box and tether it
			UGC_FOR_EACH_COMPONENT_OF_TYPE_ON_MAP(ugcEditorGetComponentList(), UGC_COMPONENT_TYPE_REWARD_BOX, baseMapName, component)
			{
				eaPush(eaTetheredComponents, component);
			}
			UGC_FOR_EACH_COMPONENT_END;
		}
		else if(primary_component->eType == UGC_COMPONENT_TYPE_REWARD_BOX) // if our primary is the reward box
		{
			// find the exit door and tether it
			UGCComponent *exitDoorComponent = ugcComponentFindByID(ugcEditorGetComponentList(), exitDoorComponentID);
			eaPush(eaTetheredComponents, exitDoorComponent);
		}
	}
}

static void ugcDragAddTethered(UGCMapEditorDoc* doc, UGCUIMapEditorComponent* componentUI, UGCComponent* primaryComponent)
{
	UGCComponent* component = ugcLayoutGetComponentByID(componentUI->uComponentID);
	UGCComponent **eaTetheredComponents = NULL;
	ugcLayoutGetTetheredComponents(doc, component, &eaTetheredComponents);

	FOR_EACH_IN_EARRAY_FORWARDS(eaTetheredComponents, UGCComponent, tethered)
	{
		UGCUIMapEditorComponent* tetheredUI = ugcLayoutUIGetComponentByID( doc, tethered->uID );

		if( tetheredUI && tethered != primaryComponent ) {
			if( eaFind( &doc->drag_state->tethered_widgets, tetheredUI ) == -1 ) {
				subVec3( tethered->sPlacement.vPos, primaryComponent->sPlacement.vPos, tetheredUI->drag_relative_position );
				eaPush( &doc->drag_state->tethered_widgets, tetheredUI );
			}
		}
	}
	FOR_EACH_END;

	eaDestroy(&eaTetheredComponents);
}

static void ugcMapUIStartDragComponent(UGCUIMapEditorComponent *component_widget, int mouse_offset_x, int mouse_offset_y, UGCMapUIDragType type)
{
	UGCMapEditorDoc* doc = component_widget->doc;
	UGCComponent *component = ugcLayoutGetComponentByID(component_widget->uComponentID);
	
	if (component)
	{
		bool show_trash = (type == UGC_DRAG_MOVE_COMPONENT && ugcLayoutCanDeleteComponent(component));
		ugcMapUIStartDrag(doc, MS_LEFT, mouse_offset_x, mouse_offset_y, type, show_trash, ugcMapUIComponentDragCB);
		doc->drag_state->primary_widget = component_widget;
		component_widget->drag_is_implicit_child = false;
		setVec3(component_widget->drag_relative_position, 0, 0, 0);

		// If dragging an existing component, we want to reset the
		// mouse position so that dragging does not cause the object
		// to move.
		if (  component != &g_TemporaryComponent && type == UGC_DRAG_MOVE_COMPONENT
			  && component->sPlacement.uRoomID != GENESIS_UNPLACED_ID
			  && component->eType != UGC_COMPONENT_TYPE_ROOM ) {
			Vec2 componentUIPos;
			Vec2 mousePos;
			ugcLayoutGetUICoords( doc, component->sPlacement.vPos, componentUIPos );

			mousePos[0] = componentUIPos[0] * doc->layout_scale + doc->backdrop_last_box.lx;
			mousePos[1] = componentUIPos[1] * doc->layout_scale + doc->backdrop_last_box.ly;
			mouseSetScreen( mousePos[0], mousePos[1] );
		}

		if (component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR ||
			component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR)
		{
			// Snap door rotation (doesn't support multiselect)
			F32 angle_mod = fmod(fabs(component->sPlacement.vRotPYR[1]),180);
			if (angle_mod > 0.1f && angle_mod < 179.9f)
			{
				doc->drag_state->door_snap_offset = true;
			}
		}
		else if (type == UGC_DRAG_MOVE_COMPONENT || type == UGC_DRAG_ROTATE_COMPONENT)
		{
			// Multiselection support
			if (eaiSize(&doc->selected_components) > 1 && eaiFind(&doc->selected_components, component->uID) != -1)
			{
				int select_idx;
				for (select_idx = 0; select_idx < eaiSize(&doc->selected_components); select_idx++)
				{
					UGCComponent *select_component = ugcLayoutGetComponentByID(doc->selected_components[select_idx]);
					if (select_component && select_component != component)
					{
						UGCUIMapEditorComponent *select_widget = ugcLayoutUIGetComponent(doc, select_component);
						if (!ugcLayoutCanDeleteComponent(select_component) && select_component->eType != UGC_COMPONENT_TYPE_ACTOR)
							show_trash = false;
						subVec3(select_component->sPlacement.vPos, component->sPlacement.vPos, select_widget->drag_relative_position);
						select_widget->drag_is_implicit_child = false;
						eaPush(&doc->drag_state->secondary_widgets, select_widget);
					}
				}
			}

			// Add all children as secondary components as well
			ugcDragAddChildrenToSecondary(doc, component_widget, component);
			FOR_EACH_IN_EARRAY(doc->drag_state->secondary_widgets, UGCUIMapEditorComponent, secondaryComponent) {
				ugcDragAddChildrenToSecondary(doc, secondaryComponent, component);
			} FOR_EACH_END;

			// Cache if the parent room is dragging
			{
				UGCComponent* parentRoom = ugcComponentGetRoomParent(ugcEditorGetComponentList(), component);
				component_widget->drag_parent_room_is_dragging = (parentRoom ? ugcMapUIIsComponentDragging( doc, parentRoom->uID ) : false);
			}
			FOR_EACH_IN_EARRAY(doc->drag_state->secondary_widgets, UGCUIMapEditorComponent, secondaryComponent) {
				UGCComponent* secondary = ugcLayoutGetComponentByID(secondaryComponent->uComponentID);
				UGCComponent* parentRoom = ugcComponentGetRoomParent(ugcEditorGetComponentList(), secondary);
				secondaryComponent->drag_parent_room_is_dragging = (parentRoom ? ugcMapUIIsComponentDragging( doc, parentRoom->uID ) : false);
			} FOR_EACH_END;
		}

		if(ugcTetheringAllowed() && g_bUGCTetheringEnabled)
			// Add all tethered components as well
			ugcDragAddTethered(doc, component_widget, component);
	}
}

void ugcLayoutStartPlaceNewComponent(UGCMapEditorDoc *doc, UGCComponent *component)
{
	UGCUIMapEditorComponent *new_component_widget = ugcLayoutUIGetComponent(doc, component);
	if (!new_component_widget)
	{
		new_component_widget = ugcLayoutUIComponentCreate(doc, component, false);
	}
	zeroVec3( g_TemporaryComponent.sPlacement.vRotPYR );
	g_TemporaryComponent.sPlacement.vRotPYR[1] = new_component_widget->drag_position.rotation = 0;
	doc->mode = ugcLayoutComponentDragNewMode( doc, component );
	
	ugcMapUIStartDragComponent(new_component_widget, 0, 0, UGC_DRAG_MOVE_COMPONENT);
}

//apply the correct click to the component. Assigned as a callback in alled from ugcLayoutBackdropTick(). 
static bool ugcLayoutComponentMouseCB(UGCMapEditorDoc* doc, UGCMapUIMouseEvent* event)
{
	UGCComponent *component = ugcLayoutGetComponentByID(event->component_widget->uComponentID);
	if (!component)
		return true;

	if( event->type != UGC_MOUSE_EVENT_DRAG ) {
		if( event->drag_type == UGC_DRAG_ROTATE_COMPONENT ) {
			Vec2 uiCoords;
			Vec3 worldCoords;
			uiCoords[0] = (g_ui_State.mouseX - doc->backdrop_last_box.lx) / doc->layout_scale;
			uiCoords[1] = (g_ui_State.mouseY - doc->backdrop_last_box.ly) / doc->layout_scale;
			ugcLayoutGetWorldCoords(doc, uiCoords, worldCoords );
			
			ugcEditorSetCursorForRotation( atan2( worldCoords[ 0 ] - component->sPlacement.vPos[ 0 ],
												  worldCoords[ 2 ] - component->sPlacement.vPos[ 2 ]));
		} else {
			ui_SetCursorByName( "UGC_Cursors_Move_Pointer" );
		}
	}

	switch( event->button ) {
		xcase MS_LEFT:
			switch( event->type ) {
				xcase UGC_MOUSE_EVENT_CLICK:
					if (inpLevelPeek( INP_CONTROL )) { //multi-select
						if (ugcMapEditorIsComponentSelected(doc, component->uID)) {
							ugcMapEditorRemoveSelectedComponent(doc, component->uID);
						} else {
							ugcMapEditorAddSelectedComponent(doc, component->uID, false, false);
							ugcEditorQueueUIUpdate();
						}
					} else {
						ugcMapEditorSetSelectedComponent(doc, component->uID, 0, false, false);
					}

				xcase UGC_MOUSE_EVENT_DRAG: {
					Vec2 component_pos;
					F32 offset_x = 0, offset_y = 0;
					ugcLayoutGetUICoords(doc, component->sPlacement.vPos, component_pos);

					if (component->eType == UGC_COMPONENT_TYPE_ROOM && event->drag_type == UGC_DRAG_MOVE_COMPONENT) {
						offset_x = (g_ui_State.mouseX-doc->backdrop_last_box.lx)/doc->layout_scale - component_pos[0];
						offset_y = (g_ui_State.mouseY-doc->backdrop_last_box.ly)/doc->layout_scale - component_pos[1];
					}
					ugcMapUIStartDragComponent(event->component_widget, offset_x, offset_y, event->drag_type);
				}
			}

		xcase MS_RIGHT:
			switch( event->type ) {
				xcase UGC_MOUSE_EVENT_CLICK:
					ugcMapEditorSetSelectedComponent(doc, component->uID, 0, false, false);
					if (component->eType == UGC_COMPONENT_TYPE_ROOM) {
						UGCActionID actions[] = { UGC_ACTION_CUT, UGC_ACTION_COPY, UGC_ACTION_DELETE, UGC_ACTION_ROOM_CLEAR, UGC_ACTION_ROOM_POPULATE, UGC_ACTION_PLAY_MAP_FROM_LOCATION, 0 };
						ugcEditorShowContextMenu(actions);
					} else {
						UGCActionID actions[] = { UGC_ACTION_CUT, UGC_ACTION_COPY, UGC_ACTION_DELETE, UGC_ACTION_DUPLICATE, UGC_ACTION_PLAY_MAP_FROM_LOCATION, 0 };
						ugcEditorShowContextMenu(actions);
					}
			}
	}

	ugcMapEditorSetHighlightedComponent( doc, event->component_widget->uComponentID);

	return true;
}

static struct {
	UGCMapEditorDoc *doc;
	UGCComponent *component;
	int door_id;
	UGCComponentType component_type;
} g_UGCCreateDoorProperties;

static void ugcLayoutCreateRoomDoor(UGCAssetLibraryPane *pane, void *unused, UGCAssetLibraryRow *row)
{
	char *row_name;
	UGCComponent *new_component;
	UGCRoomInfo *room_info = ugcRoomGetRoomInfo(g_UGCCreateDoorProperties.component->iObjectLibraryId);

	if (!room_info || !room_info->footprint_buf)
		return;

	// Cache the row data since it can get deleted in ugcMapUICancelAction()
	row_name = strdup(row->pcName);
	
	ugcMapUICancelAction(g_UGCCreateDoorProperties.doc);
	new_component = ugcComponentOpCreate(ugcEditorGetProjectData(), g_UGCCreateDoorProperties.component_type, g_UGCCreateDoorProperties.component->uID);
	new_component->iObjectLibraryId = atoi( row_name );

	SAFE_FREE(row_name);

	ugcComponentOpSetPlacement(ugcEditorGetProjectData(), new_component, g_UGCCreateDoorProperties.doc->map_data, UGC_TOPLEVEL_ROOM_ID);
	ugcComponentOpReset(ugcEditorGetProjectData(), new_component, ugcMapGetType(g_UGCCreateDoorProperties.doc->map_data), true);

	// Calculate door position & rotation
	{
		Vec3 door_local_pos;
		ugcRoomGetDoorLocalPos(room_info, g_UGCCreateDoorProperties.door_id, door_local_pos);
		ugcRoomConvertLocalToWorld(g_UGCCreateDoorProperties.component, door_local_pos, new_component->sPlacement.vPos);
		new_component->sPlacement.vRotPYR[1] = g_UGCCreateDoorProperties.component->sPlacement.vRotPYR[1] + ugcRoomGetDoorLocalRot(room_info, g_UGCCreateDoorProperties.door_id);
		new_component->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE;
		new_component->iRoomDoorID = g_UGCCreateDoorProperties.door_id;
	}

	ugcEditorStartObjectFlashing(new_component);
	ugcEditorApplyUpdate();
	ugcMapEditorSetSelectedComponent(g_UGCCreateDoorProperties.doc, new_component->uID, 0, false, true);
	ugcEditorApplyUpdate();
}

static int *g_eaiCurrentDoorTypes = NULL;

static bool ugcLayoutRoomDoorFilter(const char *value, UGCAssetLibraryRow *row)
{
	int door_id = ugcRoomDoorGetTypeIDForResourceInfo( ugcResourceGetInfo( "ObjectLibrary", row->pcName ));

	if (door_id == -1)
		return false;

	if (eaiFind(&g_eaiCurrentDoorTypes, door_id) == -1)
		return false;

	return true;
}

static void ugcLayoutGetRoomOptionsPos(UGCComponent *component, IVec2 out_pos, int irot)
{
	int px, py;
	int width, height;
	UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);

	out_pos[0] = out_pos[1] = -100;

	if (!room_info || !room_info->footprint_buf)
		return;

	// Find top-right-most position
	width = room_info->footprint_max[0]+1-room_info->footprint_min[0];
	height = room_info->footprint_max[1]+1-room_info->footprint_min[1];

	for (py = 0; py < height; py++)
		for (px = 0; px < width; px++)
			if (room_info->footprint_buf[px+py*width] != 0)
			{
				IVec2 rotated_pt;
				IVec2 in_pt = { px+room_info->footprint_min[0], py+room_info->footprint_min[1] };
				ugcRoomRotateAndFlipPoint(in_pt, irot, rotated_pt);

				if (rotated_pt[0] > out_pos[0] ||
					(rotated_pt[0] == out_pos[0] && rotated_pt[1] > out_pos[1]))
				{
					out_pos[0] = rotated_pt[0];
					out_pos[1] = rotated_pt[1];
				}
			}
}


#define MOUSE_DIST_SQR(x, y) SQR((x)-g_ui_State.mouseX)+SQR((y)-g_ui_State.mouseY)

static void ugcLayoutComponentTickPatrolPath(UGCMapEditorDoc *doc, UGCUIMapEditorComponent *component_widget, UGCComponent *component, F32 x, F32 y, F32 scale, F32 draw_scale);

float ROTATE_GIZMO_MIN_WIDTH = 45;

static void ugcLayoutComponentTick(UGCUIMapEditorComponent *component_widget, UI_PARENT_ARGS)
{
	UGC_UI_MAP_EDITOR_GET_COORDINATES( component_widget );
	F32 *room_rects = NULL; // List of boxes (x,y,w,h)
	CBox room_bounding_box; // Bounding box of room_rects
	
	UGCMapEditorDoc *doc = component_widget->doc;
	UGCComponent *component = ugcLayoutGetComponentByID(component_widget->uComponentID);
	UGCComponentPlacement *placement;
	F32 draw_scale = doc->layout_grid_size * scale / doc->layout_kit_spacing;
	int rect_idx;
	bool area_is_round = true;
	bool is_room = false;
	bool highlighted = ugcMapEditorIsComponentSelected(doc, component_widget->uComponentID);
	bool ignore_clicks = false;

	if (  !component || component_widget->is_static || component_widget->is_deleted
		  || component->eType == UGC_COMPONENT_TYPE_TELEPORTER ) {
		return;
	}

	placement = &component->sPlacement;

	if( component->eType == UGC_COMPONENT_TYPE_PATROL_POINT ) {
		if(   !ugcMapEditorIsComponentHighlighted( doc, component->uPatrolParentID )
			  && !ugcMapEditorIsComponentSelected( doc, component->uPatrolParentID )
			  && !ugcMapEditorIsComponentPrevSelected( doc, component->uPatrolParentID )) {
			UGCComponent* patrolParent = ugcEditorFindComponentByID( component->uPatrolParentID );

			if( !patrolParent ){
				return;
			}
			if(   !ugcMapEditorIsAnyComponentSelected( doc, patrolParent->eaPatrolPoints )
				  && !ugcMapEditorIsAnyComponentPrevSelected( doc, patrolParent->eaPatrolPoints )) {
				return;
			}
		}
	}

	if (component->eType == UGC_COMPONENT_TYPE_ROOM)
	{
		UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);
		int irot = ROT_TO_QUADRANT(RAD(component->sPlacement.vRotPYR[1]));
		int px, py;
		int width, height;
		Vec3 component_pos;

		if (!room_info || !room_info->footprint_buf)
			return;
		if (doc->rooms_fade > 0.05f)
			ignore_clicks = true; // Ignore everything but right-click

		width = room_info->footprint_max[0]+1-room_info->footprint_min[0];
		height = room_info->footprint_max[1]+1-room_info->footprint_min[1];

		copyVec3(component->sPlacement.vPos, component_pos);

		room_bounding_box.lx = 1e8;
		room_bounding_box.ly = 1e8;
		room_bounding_box.hx = -1e8;
		room_bounding_box.hy = -1e8;
		for (py = 0; py < height; py++) {
			for (px = 0; px < width; px++) {
				if (room_info->footprint_buf[px+py*width] != 0) {
					IVec2 in_pt = { px+room_info->footprint_min[0], py+room_info->footprint_min[1] };
					IVec2 rotated_pt;
					float rectX;
					float rectY;
					float rectW;
					float rectH;

					ugcRoomRotateAndFlipPoint(in_pt, irot, rotated_pt);

					rectX = x+rotated_pt[0]*UGC_ROOM_GRID*draw_scale - 5;
					rectY = y+rotated_pt[1]*UGC_ROOM_GRID*draw_scale - 5;
					rectW = UGC_ROOM_GRID*draw_scale + 10;
					rectH = UGC_ROOM_GRID*draw_scale + 10;
					eafPush(&room_rects, rectX);
					eafPush(&room_rects, rectY);
					eafPush(&room_rects, rectW);
					eafPush(&room_rects, rectH);

					room_bounding_box.lx = MIN( room_bounding_box.lx, rectX );
					room_bounding_box.ly = MIN( room_bounding_box.ly, rectY );
					room_bounding_box.hx = MAX( room_bounding_box.hx, rectX + rectW );
					room_bounding_box.hy = MAX( room_bounding_box.hy, rectY + rectH );
				}
			}
		}

		area_is_round = false;
		is_room = true;
	}
	else if( (component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR || component->eType == UGC_COMPONENT_TYPE_FAKE_DOOR)
			 && ugcLayoutComponentHandleAlpha( doc, component ) < 245 ) {
		AtlasTex* tex;
		F32 icon_scale;

		if( component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR ) {
			tex = atlasLoadTexture("ugc_room_door");
		} else {
			tex = atlasLoadTexture("ugc_room_door_closed");
		}
		icon_scale = UGC_ROOM_GRID * draw_scale * 4 / tex->width;

		eafPush(&room_rects, x - tex->width / 2 * icon_scale );
		eafPush(&room_rects, y - tex->height / 2 * icon_scale );
		eafPush(&room_rects, tex->width * icon_scale );
		eafPush(&room_rects, tex->height * icon_scale );

		BuildCBoxFromCenter(&room_bounding_box, x, y, MAX( tex->width, ROTATE_GIZMO_MIN_WIDTH ), MAX( tex->height, ROTATE_GIZMO_MIN_WIDTH ));
	}
	else
	{
		AtlasTex* tex = g_ComponentMarkers[component->eType];
		F32 icon_scale = 1;

		if( ugcLayoutComponentHandleAlpha( doc, component ) < 245 ) {
			return;
		}

		eafPush(&room_rects, x - tex->width / 2 );
		eafPush(&room_rects, y - tex->height / 2 );
		eafPush(&room_rects, tex->width );
		eafPush(&room_rects, tex->height );

		BuildCBoxFromCenter(&room_bounding_box, x, y, MAX( tex->width, ROTATE_GIZMO_MIN_WIDTH ), MAX( tex->height, ROTATE_GIZMO_MIN_WIDTH ));
	}

	for (rect_idx = 0; rect_idx < eafSize(&room_rects); rect_idx += 4)
	{
		UGCMapUIMouseEvent* event;
		CBox border_box;
		BuildCBox(&border_box, room_rects[rect_idx+0], room_rects[rect_idx+1], room_rects[rect_idx+2], room_rects[rect_idx+3]);
		if (!ignore_clicks &&
			(event = ugcMapUIDoMouseEventTestEx(doc, MS_LEFT,
												&border_box, area_is_round,
												(is_room ? UGC_MOUSE_EVENT_ROOM_MOVE : UGC_MOUSE_EVENT_COMPONENT_MOVE),
												ugcLayoutComponentMouseCB,
												(is_room ? "MOVEROOM %d" : "MOVECOM %d"),
												component->uID)) != NULL)
		{
			event->distance = MOUSE_DIST_SQR(x, y); // Distance from center
			event->drag_type = UGC_DRAG_MOVE_COMPONENT;
			event->component_widget = component_widget;
			event->is_selected = (component->eType != UGC_COMPONENT_TYPE_ROOM
								  && ugcMapEditorIsComponentSelected(doc, component->uID));
		}

		if ((event = ugcMapUIDoMouseEventTestEx(doc, MS_RIGHT,
												&border_box, area_is_round,
												(is_room ? UGC_MOUSE_EVENT_ROOM_CONTEXT : UGC_MOUSE_EVENT_COMPONENT_MOVE),
												ugcLayoutComponentMouseCB,
												(is_room ? "CONTEXTROOM %d" : "CONTEXTCOM %d"),
												component->uID)) != NULL)
		{
			event->distance = MOUSE_DIST_SQR(x, y); // Distance from center
			event->drag_type = 0;
			event->component_widget = component_widget;
			event->is_selected = (component->eType != UGC_COMPONENT_TYPE_ROOM
								  && ugcMapEditorIsComponentSelected(doc, component->uID));
		}
	}

	eafDestroy(&room_rects);

	if (ugcLayoutComponentCanRotate(component->eType) && !ignore_clicks) {
		UGCMapUIMouseEvent* event;
		if( is_room ) {
			F32 iconScale = ugcLayoutComponentRotateHandleScale( &room_bounding_box );
			CBox iconBox;
			
			// top left click
			BuildCBox( &iconBox, room_bounding_box.lx, room_bounding_box.ly, g_RotateIconTL->width * iconScale, g_RotateIconTL->height * iconScale );
			if((event = ugcMapUIDoMouseEventTest(doc, MS_LEFT, &iconBox, UGC_MOUSE_EVENT_ROOM_ROTATE,
												 ugcLayoutComponentMouseCB, "ROTATECOM %d", component->uID)) != NULL) {
				event->distance = MOUSE_DIST_SQR(x, y); // Distance from center
				event->drag_type = UGC_DRAG_ROTATE_COMPONENT;
				event->component_widget = component_widget;
				event->is_selected = (component->eType != UGC_COMPONENT_TYPE_ROOM
									  && ugcMapEditorIsComponentSelected(doc, component->uID));
			}
			
			// top right click
			BuildCBox( &iconBox, room_bounding_box.hx - g_RotateIconTR->width * iconScale, room_bounding_box.ly, g_RotateIconTR->width * iconScale, g_RotateIconTR->height * iconScale );
			if((event = ugcMapUIDoMouseEventTest(doc, MS_LEFT, &iconBox, UGC_MOUSE_EVENT_ROOM_ROTATE,
												 ugcLayoutComponentMouseCB, "ROTATECOM %d", component->uID)) != NULL) {
				event->distance = MOUSE_DIST_SQR(x, y); // Distance from center
				event->drag_type = UGC_DRAG_ROTATE_COMPONENT;
				event->component_widget = component_widget;
				event->is_selected = (component->eType != UGC_COMPONENT_TYPE_ROOM
									  && ugcMapEditorIsComponentSelected(doc, component->uID));
			}
			
			// bottom left click
			BuildCBox( &iconBox, room_bounding_box.lx, room_bounding_box.hy - g_RotateIconBL->height * iconScale, g_RotateIconBL->width * iconScale, g_RotateIconBL->height * iconScale );
			if((event = ugcMapUIDoMouseEventTest(doc, MS_LEFT, &iconBox, UGC_MOUSE_EVENT_ROOM_ROTATE,
												 ugcLayoutComponentMouseCB, "ROTATECOM %d", component->uID)) != NULL) {
				event->distance = MOUSE_DIST_SQR(x, y); // Distance from center
				event->drag_type = UGC_DRAG_ROTATE_COMPONENT;
				event->component_widget = component_widget;
				event->is_selected = (component->eType != UGC_COMPONENT_TYPE_ROOM
									  && ugcMapEditorIsComponentSelected(doc, component->uID));
			}
			
			// bottom right click
			BuildCBox( &iconBox, room_bounding_box.hx - g_RotateIconBR->width * iconScale, room_bounding_box.hy - g_RotateIconBR->height * iconScale, g_RotateIconBR->width * iconScale, g_RotateIconBR->height * iconScale );
			if((event = ugcMapUIDoMouseEventTest(doc, MS_LEFT, &iconBox, UGC_MOUSE_EVENT_ROOM_ROTATE,
												 ugcLayoutComponentMouseCB, "ROTATECOM %d", component->uID)) != NULL) {
				event->distance = MOUSE_DIST_SQR(x, y); // Distance from center
				event->drag_type = UGC_DRAG_ROTATE_COMPONENT;
				event->component_widget = component_widget;
				event->is_selected = (component->eType != UGC_COMPONENT_TYPE_ROOM
									  && ugcMapEditorIsComponentSelected(doc, component->uID));
			}
		} else {
			if((event = ugcMapUIDoMouseEventTest(doc, MS_LEFT, &room_bounding_box,
												 UGC_MOUSE_EVENT_COMPONENT_ROTATE,
												 ugcLayoutComponentMouseCB,
												 "ROTATECOM %d",
												 component->uID)) != NULL)
			{
				event->distance = MOUSE_DIST_SQR(x, y); // Distance from center
				event->drag_type = UGC_DRAG_ROTATE_COMPONENT;
				event->component_widget = component_widget;
				event->is_selected = (component->eType != UGC_COMPONENT_TYPE_ROOM
									  && ugcMapEditorIsComponentSelected(doc, component->uID));
			}
		}
	}
	
	if (component->eType == UGC_COMPONENT_TYPE_KILL || component->eType == UGC_COMPONENT_TYPE_CONTACT)
	{
		ugcLayoutComponentTickPatrolPath(doc, component_widget, component, x, y, scale, draw_scale);
	}
}

static void ugcLayoutComponentTickPatrolPath(UGCMapEditorDoc *doc, UGCUIMapEditorComponent *component_widget, UGCComponent *component, F32 x, F32 y, F32 scale, F32 draw_scale)
{
	if( eaiSize(&component->eaPatrolPoints) == 0 ) {
		return;
	}
	if(   !ugcMapEditorIsComponentSelected(doc, component->uID)
		  && !ugcMapEditorIsComponentPrevSelected( doc, component->uID )
		  && !ugcMapEditorIsAnyComponentSelected( doc, component->eaPatrolPoints )
		  && !ugcMapEditorIsAnyComponentPrevSelected( doc, component->eaPatrolPoints )) {
		return;
	}
}

typedef enum UGCComponentDrawFlags {
	COMPONENT_HIGHLIGHTED		= (1<<0), // Component is currently hovered over
	COMPONENT_SELECTED			= (1<<1), // Component is currently selected
	COMPONENT_DRAGGING			= (1<<2), // Component is currently being dragged
	COMPONENT_MOVING			= (1<<3), // Component is being moved
	COMPONENT_ROTATING			= (1<<4), // Component is being rotated
	COMPONENT_ROTATE_FADE		= (1<<5), // Component is selected but another component is highlighted
	COMPONENT_CUT_BUFFER		= (1<<6), // Component is currently in the cut buffer
	COMPONENT_INVALID_POS		= (1<<7), // Component is in an invalid position
} UGCComponentDrawFlags;

static void ugcLayoutComponentDrawRotateHandle(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, int flags);
static void ugcLayoutComponentDrawTranslateHandle(UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, S32 object_alpha, int flags);
static void ugcLayoutComponentDrawTranslateHandleTrapTarget(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, S32 object_alpha, int flags);
static void ugcLayoutComponentDrawPlanet(UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, int flags);
static void	ugcLayoutComponentDrawPatrolPath(UGCMapEditorDoc *doc, UGCUIMapEditorComponent *component_widget, UGCComponent *component, F32 x, F32 y, F32 z, F32 scale, F32 draw_scale, int flags);
static void	ugcLayoutComponentDrawTeleporterPath(UGCMapEditorDoc *doc, UGCUIMapEditorComponent *component_widget, UGCComponent *component, F32 x, F32 y, F32 z, F32 scale, F32 draw_scale, int flags);
static void ugcLayoutComponentDrawSpawnArea(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, S32 object_alpha, int flags);
static void ugcLayoutComponentDrawAggroArea(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, S32 object_alpha, int flags);
static void ugcLayoutComponentDrawRoomDoor(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, S32 object_alpha, int flags);
static void ugcLayoutComponentDrawRoomMarker(UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, S32 object_alpha, int flags);
static void ugcLayoutComponentDrawRoomFootprint(UGCMapEditorDoc *doc, UGCUIMapEditorComponent *component_widget, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, int flags);
static void ugcLayoutComponentDrawRoomDoorSlots(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, int flags);
static void ugcLayoutComponentDrawObjectPreview(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, int flags);
static void ugcLayoutComponentDrawObjectBounds(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, int flags);
static void ugcLayoutComponentDrawObjectSoundRadii(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, S32 object_alpha, int flags);
static void ugcLayoutComponentDrawLabel(UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, int flags);
static void ugcLayoutComponentDrawError(UGCUIMapEditorComponent* component_widget, F32 x, F32 y, F32 z, F32 draw_scale, S32 alpha, int flags);

// Main draw call for all components that are placed on a map
static void ugcLayoutComponentDraw(UGCUIMapEditorComponent *component_widget, UGCUIMapEditorDrawLayer layer, UI_PARENT_ARGS)
{
	UGC_UI_MAP_EDITOR_GET_COORDINATES( component_widget );
	UGCMapEditorDoc *doc = component_widget->doc;
	UGCComponent *component = ugcLayoutGetComponentByID(component_widget->uComponentID);
	UGCEditorCopyBuffer *buffer;

	S32 handle_alpha = ugcLayoutComponentHandleAlpha( doc, component );
	int flags = 0;
	F32 draw_scale;
	F32 draw_rot = 0;
	F32 component_z;
	bool hidden_room;

	if (!component || component_widget->is_deleted)
		return;

	hidden_room = (component->eType == UGC_COMPONENT_TYPE_ROOM && doc->rooms_fade > 0.05f);

	// [NNO-16215] Avoid growing the z order so darn fast.  UI_GET_Z()
	// doesn't work right with the sheer number of objects in UGC.
	g_ui_State.drawZ += 0.1;
	component_z = g_ui_State.drawZ;
	draw_scale = doc->layout_grid_size * scale / doc->layout_kit_spacing;

	if (!component_widget->is_static && !hidden_room)
	{
		bool isParentDragging = (component->uParentID != 0 && ugcMapUIIsComponentDragging(doc, component->uParentID));
		bool isDragging = ugcMapUIIsComponentDragging(doc, component_widget->uComponentID);
		F32 component_height;

		// Calculate draw flags
		if (isParentDragging || isDragging)
		{
			flags |= COMPONENT_DRAGGING;
			if (doc->drag_state->type == UGC_DRAG_ROTATE_COMPONENT) {
				flags |= COMPONENT_ROTATING;
			} else {
				flags |= COMPONENT_MOVING;
			}

			if( isDragging ) {
				flags |= COMPONENT_SELECTED | COMPONENT_DRAGGING;
			}
		}
		else if (ugcMapEditorIsComponentSelected(doc, component_widget->uComponentID))
		{
			flags |= COMPONENT_SELECTED;
			if (!ugcMapEditorIsComponentHighlighted(doc, component_widget->uComponentID))
			{
				flags |= COMPONENT_ROTATE_FADE;
			}
		}

		if (ugcMapEditorIsComponentHighlighted(doc, component_widget->uComponentID))
		{
			flags |= COMPONENT_HIGHLIGHTED;
		}

		if ((buffer = ugcEditorCurrentCopy()) != NULL &&
			(eaiFind(&buffer->eauComponentIDs, component->uID) != -1 ||
			 (component->uParentID && eaiFind(&buffer->eauComponentIDs, component->uParentID) != -1)))
		{
			flags |= COMPONENT_CUT_BUFFER;
		}

		component_height = ugcComponentGetWorldHeight(component, ugcEditorGetComponentList());
		if (!component_widget->isValidPosition)
		{
			flags |= COMPONENT_INVALID_POS;
		}
	}

	/// BY NOW, FLAGS SHOULD BE FULLY INITIALIZED

	// Don't draw things that are moving
	if (flags & (COMPONENT_MOVING | COMPONENT_ROTATING)) {
			return;
	}

	if (!(flags & (COMPONENT_HIGHLIGHTED | COMPONENT_SELECTED)))
	{
		UGCComponent *room_parent = ugcComponentGetRoomParent(ugcEditorGetComponentList(), component);
		if (room_parent && room_parent != component)
		{
			UGCUIMapEditorComponent *parent_widget = ugcLayoutUIGetComponent(doc, room_parent);
			if (parent_widget && parent_widget->selected_level > -1 &&
				parent_widget->selected_level != component->sPlacement.iRoomLevel)
				return;
		}
	}
	
	draw_rot = component->sPlacement.vRotPYR[1];

	// Rotate handle
	ugcLayoutComponentDrawRotateHandle(doc, component, x, y, component_z, draw_rot, draw_scale, flags);

	// Type-specific draw code
	switch (component->eType)
	{
		xcase UGC_COMPONENT_TYPE_PLANET:
			if( layer == UGC_MAP_LAYER_COMPONENT ) {
				ugcLayoutComponentDrawPlanet(component, x, y, component_z, draw_scale, flags);
			}

		xcase UGC_COMPONENT_TYPE_SPAWN:
			if( layer == UGC_MAP_LAYER_TOP_UI ) {
				ugcLayoutComponentDrawSpawnArea(doc, component, x, y, component_z, draw_scale, handle_alpha, flags);
			}

		xcase UGC_COMPONENT_TYPE_ROOM_DOOR: case UGC_COMPONENT_TYPE_FAKE_DOOR:
			if( layer == UGC_MAP_LAYER_DOORS ) {
				ugcLayoutComponentDrawRoomDoor(doc, component, x, y, component_z, draw_rot, draw_scale, handle_alpha, flags);
			}

		xcase UGC_COMPONENT_TYPE_ROOM_MARKER:
			if( layer == UGC_MAP_LAYER_VOLUME ) {
				ugcLayoutComponentDrawRoomMarker(component, x, y, component_z, draw_scale, handle_alpha, flags);
			}

		xcase UGC_COMPONENT_TYPE_ROOM: {
			const WorldUGCProperties* ugcProps = ugcResourceGetUGCPropertiesInt("ObjectLibrary", component->iObjectLibraryId);
			if( layer == UGC_MAP_LAYER_ROOM ) {
				ugcLayoutComponentDrawRoomFootprint(doc, component_widget, component, x, y, component_z, draw_rot, draw_scale, flags);
				ugcLayoutComponentDrawObjectPreview(doc, component, x, y, component_z, draw_rot, draw_scale, flags);
			}
			if( layer == UGC_MAP_LAYER_DOORS ) {
				if( !ugcProps || !ugcProps->groupDefProps.bRoomDoorsEverywhere ) {
					ugcLayoutComponentDrawRoomDoorSlots(doc, component, x, y, component_z, draw_rot, draw_scale, flags);
				}
			}
			if( layer == UGC_MAP_LAYER_TOP_UI ) {
				ugcLayoutComponentDrawObjectBounds(doc, component, x, y, component_z, draw_rot, draw_scale, flags);
			}
		}

		xcase UGC_COMPONENT_TYPE_OBJECT:
		case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
		case UGC_COMPONENT_TYPE_DESTRUCTIBLE:
		case UGC_COMPONENT_TYPE_TELEPORTER_PART:
		case UGC_COMPONENT_TYPE_CLUSTER_PART:
			if( layer == UGC_MAP_LAYER_COMPONENT ) {
				ugcLayoutComponentDrawObjectPreview(doc, component, x, y, component_z, draw_rot, draw_scale, flags);
			}
			if( layer == UGC_MAP_LAYER_TOP_UI ) {
				ugcLayoutComponentDrawObjectBounds(doc, component, x, y, component_z, draw_rot, draw_scale, flags);
			}

		xcase UGC_COMPONENT_TYPE_SOUND:
			if( layer == UGC_MAP_LAYER_VOLUME ) {
				ugcLayoutComponentDrawObjectSoundRadii(doc, component, x, y, component_z, draw_scale, handle_alpha, flags);
			}

		xcase UGC_COMPONENT_TYPE_TRAP_EMITTER: {
			UGCComponent* parentComponent = ugcLayoutGetComponentByID( component->uParentID );
			if( parentComponent ) {
				if( layer == UGC_MAP_LAYER_COMPONENT ) {
					ugcLayoutComponentDrawObjectPreview(doc, parentComponent, x, y, component_z, draw_rot, draw_scale, flags);
				}
				if( layer == UGC_MAP_LAYER_TOP_UI ) {
					ugcLayoutComponentDrawObjectBounds(doc, parentComponent, x, y, component_z, draw_rot, draw_scale, flags);
				}
			}
		}
			
		xcase UGC_COMPONENT_TYPE_TRAP_TARGET:
			if( layer == UGC_MAP_LAYER_VOLUME ) {
				ugcLayoutComponentDrawTranslateHandleTrapTarget(doc, component, x, y, component_z, draw_scale, handle_alpha, flags);
			}

		xcase UGC_COMPONENT_TYPE_KILL:
			if( layer == UGC_MAP_LAYER_VOLUME ) {
				ugcLayoutComponentDrawAggroArea(doc, component, x, y, component_z, draw_scale, handle_alpha, flags);
			}
			if( layer == UGC_MAP_LAYER_TOP_UI ) {
				ugcLayoutComponentDrawPatrolPath(doc, component_widget, component, x, y, component_z, scale, draw_scale, flags);
			}

		xcase UGC_COMPONENT_TYPE_CONTACT:
			if( layer == UGC_MAP_LAYER_TOP_UI ) {
				ugcLayoutComponentDrawPatrolPath(doc, component_widget, component, x, y, component_z, scale, draw_scale, flags);
			}

		xcase UGC_COMPONENT_TYPE_PATROL_POINT:
			if(   !ugcMapEditorIsComponentHighlighted( doc, component->uPatrolParentID )
				  && !ugcMapEditorIsComponentSelected( doc, component->uPatrolParentID )
				  && !ugcMapEditorIsComponentPrevSelected( doc, component->uPatrolParentID )) {
				UGCComponent* patrolParent = ugcEditorFindComponentByID( component->uPatrolParentID );

				if( !patrolParent ){
					return;
				}
				if(   !ugcMapEditorIsAnyComponentSelected( doc, patrolParent->eaPatrolPoints )
					  && !ugcMapEditorIsAnyComponentPrevSelected( doc, patrolParent->eaPatrolPoints )) {
					return;
				}
			}

		xcase UGC_COMPONENT_TYPE_TELEPORTER:
			if( layer == UGC_MAP_LAYER_TOP_UI ) {
				ugcLayoutComponentDrawTeleporterPath(doc, component_widget, component, x, y, component_z, scale, draw_scale, flags);
			}
	}

	// The node handle / translate gizmo
	if( layer == UGC_MAP_LAYER_TOP_UI ) {
		ugcLayoutComponentDrawTranslateHandle(component, x, y, component_z, draw_rot, draw_scale, handle_alpha, flags);
	}

	// The component visible name label, and error icon
	if( layer == UGC_MAP_LAYER_TEXT ) {
		ugcLayoutComponentDrawError( component_widget, x, y, component_z, draw_scale, handle_alpha, flags );
		ugcLayoutComponentDrawLabel(component, x, y, component_z, draw_scale, flags);
	}
}

// Draw call only used for the component currently being dragged, attached to the cursor
void ugcMapUIComponentDrawDragging(UGCMapEditorDoc *doc, UGCUIMapEditorComponent *component_widget, UGCComponent *component, F32 scale)
{
	int flags;
	F32 component_z = doc->frame_z + 1000.0f;
	F32 draw_scale = doc->layout_grid_size * scale / doc->layout_kit_spacing;
	Vec2 ui_pos;
	F32 draw_rot = component_widget->drag_position.rotation;
	int object_alpha = ugcLayoutComponentHandleAlpha( doc, component );

	// Turn on all the bits!
	flags = COMPONENT_HIGHLIGHTED | COMPONENT_SELECTED | COMPONENT_DRAGGING;
	if( doc->drag_state->type == UGC_DRAG_ROTATE_COMPONENT ) {
		flags |= COMPONENT_ROTATING;
	} else {
		flags |= COMPONENT_MOVING;
	}
		
	if (!component_widget->drag_valid && !doc->drag_state->trash_highlighted) {
		flags |= COMPONENT_INVALID_POS;
	}

	ugcLayoutGetUICoords(doc, component_widget->drag_position.position, ui_pos);
	ui_pos[0] = ui_pos[0]*doc->layout_scale + doc->backdrop_last_box.lx;
	ui_pos[1] = ui_pos[1]*doc->layout_scale + doc->backdrop_last_box.ly;

	clipperPush(NULL);

	// Type-specific draw code
	switch (component->eType)
	{
		xcase UGC_COMPONENT_TYPE_PLANET:
			ugcLayoutComponentDrawPlanet(component, ui_pos[0], ui_pos[1], component_z, draw_scale, flags);

		xcase UGC_COMPONENT_TYPE_SPAWN:
			ugcLayoutComponentDrawSpawnArea(doc, component, ui_pos[0], ui_pos[1], component_z, draw_scale, object_alpha, flags);

		xcase UGC_COMPONENT_TYPE_ROOM_DOOR: case UGC_COMPONENT_TYPE_FAKE_DOOR:
			ugcLayoutComponentDrawRoomDoor(doc, component, ui_pos[0], ui_pos[1], component_z, draw_rot, draw_scale, object_alpha, flags);

		xcase UGC_COMPONENT_TYPE_ROOM_MARKER:
			ugcLayoutComponentDrawRoomMarker(component, ui_pos[0], ui_pos[1], component_z, draw_scale, object_alpha, flags);

		xcase UGC_COMPONENT_TYPE_ROOM: {
			const WorldUGCProperties* ugcProps = ugcResourceGetUGCPropertiesInt("ObjectLibrary", component->iObjectLibraryId);
			ugcLayoutComponentDrawRoomFootprint(doc, component_widget, component, ui_pos[0], ui_pos[1], component_z, draw_rot, draw_scale, flags);
			ugcLayoutComponentDrawObjectPreview(doc, component, ui_pos[0], ui_pos[1], component_z, draw_rot, draw_scale, flags);
			if( !ugcProps || !ugcProps->groupDefProps.bRoomDoorsEverywhere ) {
				ugcLayoutComponentDrawRoomDoorSlots(doc, component, ui_pos[0], ui_pos[1], component_z, draw_rot, draw_scale, flags);
			}
			ugcLayoutComponentDrawObjectBounds(doc, component, ui_pos[0], ui_pos[1], component_z, draw_rot, draw_scale, flags);
		}

		xcase UGC_COMPONENT_TYPE_OBJECT:
		case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
		case UGC_COMPONENT_TYPE_DESTRUCTIBLE:
		case UGC_COMPONENT_TYPE_TELEPORTER_PART:
		case UGC_COMPONENT_TYPE_CLUSTER_PART:
			ugcLayoutComponentDrawObjectPreview(doc, component, ui_pos[0], ui_pos[1], component_z, draw_rot, draw_scale, flags);
			ugcLayoutComponentDrawObjectBounds(doc, component, ui_pos[0], ui_pos[1], component_z, draw_rot, draw_scale, flags);

		xcase UGC_COMPONENT_TYPE_SOUND:
			ugcLayoutComponentDrawObjectSoundRadii(doc, component, ui_pos[0], ui_pos[1], component_z, draw_scale, object_alpha, flags);

		xcase UGC_COMPONENT_TYPE_TRAP_EMITTER: {
			UGCComponent* parentComponent = ugcLayoutGetComponentByID( component->uParentID );
			if( parentComponent ) {
				ugcLayoutComponentDrawObjectBounds(doc, parentComponent, ui_pos[0], ui_pos[1], component_z, draw_rot, draw_scale, flags);
			}
		}

		xcase UGC_COMPONENT_TYPE_TRAP_TARGET:
			ugcLayoutComponentDrawTranslateHandleTrapTarget(doc, component, ui_pos[0], ui_pos[1], component_z, draw_scale, object_alpha, flags);

		
		xcase UGC_COMPONENT_TYPE_KILL:
			ugcLayoutComponentDrawPatrolPath(doc, component_widget, component, ui_pos[0], ui_pos[1], component_z, draw_scale, draw_scale, flags);
	}

	// The node handle / translate gizmo
	ugcLayoutComponentDrawTranslateHandle(component, ui_pos[0], ui_pos[1], component_z, draw_rot, draw_scale, object_alpha, flags);

	clipperPop();
}

static F32 ugcLayoutComponentRotateHandleScale( const CBox* box )
{
	F32 scale = 1;
	scale = MIN( scale, CBoxWidth( box ) / (g_RotateIconTL->width + g_RotateIconTR->width) );
	scale = MIN( scale, CBoxWidth( box ) / (g_RotateIconBL->width + g_RotateIconBR->width) );
	scale = MIN( scale, CBoxHeight( box ) / (g_RotateIconTL->height + g_RotateIconBL->height) );
	scale = MIN( scale, CBoxHeight( box ) / (g_RotateIconTR->height + g_RotateIconBR->height) );

	return scale;
}

static void ugcLayoutComponentDrawRotateHandle(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, int flags)
{
	F32 scale = 1;
	CBox box;

	if( component->eType == UGC_COMPONENT_TYPE_ROOM ) {
		UGCRoomInfo* roomInfo = ugcRoomGetRoomInfo( component->iObjectLibraryId );
		int irot = ROT_TO_QUADRANT( RAD( rot ));
		IVec2 rotatedMin;
		IVec2 rotatedMax;

		
		if( !roomInfo ) {
			return;
		}

		ugcRoomRotateAndFlipPoint( roomInfo->footprint_min, irot, rotatedMin );
		ugcRoomRotateAndFlipPoint( roomInfo->footprint_max, irot, rotatedMax );
		
		CBoxSet( &box, x + MIN(rotatedMin[0], rotatedMax[0]) * UGC_ROOM_GRID * draw_scale, y + MIN(rotatedMin[1], rotatedMax[1]) * UGC_ROOM_GRID * draw_scale,
				 x + (MAX(rotatedMin[0], rotatedMax[0]) + 1) * UGC_ROOM_GRID * draw_scale, y + (MAX(rotatedMin[1], rotatedMax[1]) + 1) * UGC_ROOM_GRID * draw_scale );
	} else {
		AtlasTex* marker_tex = g_ComponentMarkers[ component->eType ];
		BuildCBoxFromCenter( &box, x, y, MAX( marker_tex->width, ROTATE_GIZMO_MIN_WIDTH ), MAX( marker_tex->height, ROTATE_GIZMO_MIN_WIDTH ));
	}

	scale = ugcLayoutComponentRotateHandleScale( &box );
	
	// Only draw the ring if the component is highlighted
	if (!ugcLayoutComponentCanRotate(component->eType) || !(flags & COMPONENT_HIGHLIGHTED)) {
		return;
	}
	if (flags & COMPONENT_DRAGGING) {
		return;
	}

	display_sprite(g_RotateIconTL, box.lx, box.ly, z, scale, scale, -1);
	display_sprite(g_RotateIconTR, box.hx - g_RotateIconTR->width * scale, box.ly, z, scale, scale, -1);
	display_sprite(g_RotateIconBL, box.lx, box.hy - g_RotateIconBL->height * scale, z, scale, scale, -1);
	display_sprite(g_RotateIconBR, box.hx - g_RotateIconBR->width * scale, box.hy - g_RotateIconBR->height * scale, z, scale, scale, -1);
}

static void ugcLayoutComponentDrawTranslateHandle(UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, S32 object_alpha, int flags)
{
	S32 alpha = MIN((flags & COMPONENT_CUT_BUFFER) ? 0x40 : 0xFF, object_alpha);
	U32 rotate_draw_color = COLOR_ALPHA( g_ComponentTintColor[ component->eType ], alpha );
	U32 draw_color = COLOR_ALPHA( 0xFFFFFFFF, alpha );
	AtlasTex *marker_tex;

	if (component->eType == UGC_COMPONENT_TYPE_ROOM)
		return;
	// If the teleporter is a new component, we need to draw
	// something.  But otherwise, the parts of the teleporter are more
	// interesting.
	if ( component->eType == UGC_COMPONENT_TYPE_TELEPORTER && component->uID != 0 )
		return;
	
	if (alpha == 0)
		return;

	marker_tex = g_ComponentMarkers[component->eType];
	if( ugcLayoutComponentCanRotate( component->eType )) {
		display_sprite_rotated( atlasFindTexture( "UGC_Widgets_Rotators_OrientationLight" ), x, y, RAD(rot + 180), z, 1, rotate_draw_color );
	}
	display_sprite_rotated(marker_tex, x, y, 0, z, 1, draw_color);

	if( flags & COMPONENT_SELECTED ) {
		display_sprite_rotated( atlasFindTexture( "UGC_Icons_Map_Selection" ), x, y, 0, z, 1, -1 );
	}
}

static void ugcLayoutComponentDrawTranslateHandleTrapTarget(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, S32 object_alpha, int flags)
{
	UITextureAssembly* texas = RefSystem_ReferentFromString( "UITextureAssembly", g_LineMarker );
	UGCComponent* trap = ugcEditorFindComponentByID( component->uParentID );
	UGCTrapProperties* properties = NULL;
	bool emitterFound = false;
	Vec3 emitterPos;
	F32 emitterRot = 0;

	if( trap ) {
		GroupDef* trapDef = objectLibraryGetGroupDef( trap->iObjectLibraryId, false );
		UGCComponent* emitter = ugcTrapFindEmitter( ugcEditorGetProjectData(), trap );
		properties = trapDef ? ugcTrapGetProperties( trapDef ) : NULL;
		if( emitter ) {
			emitterFound = true;
			if( ugcMapUIIsComponentDragging( doc, emitter->uID ) ) {
				UGCUIMapEditorComponent* emitterUI = ugcLayoutUIGetComponent( doc, emitter );
				copyVec3( emitterUI->drag_position.position, emitterPos );
				emitterRot = emitterUI->drag_position.rotation;
			} else {
				copyVec3( emitter->sPlacement.vPos, emitterPos );
				emitterRot = emitter->sPlacement.vRotPYR[1];
			}
		}
	}

	if( texas && emitterFound && properties && eaGet( &properties->eaEmitters, component->iTrapEmitterIndex )) {
		UGCTrapPointData* trapEmitter = properties->eaEmitters[ component->iTrapEmitterIndex ];
		float trapEmitterDX = cos( RAD( emitterRot )) * trapEmitter->pos[0] - sin( RAD( emitterRot )) * trapEmitter->pos[2];
		float trapEmitterDY = sin( RAD( emitterRot )) * trapEmitter->pos[0] + cos( RAD( emitterRot )) * trapEmitter->pos[2];
		Vec2 toEmitter;
		CBox box = { 0 };
		Vec2 targetPosUI;
		Vec3 targetPosWorld;
		Color4 tint = { g_ComponentTintColor[ UGC_COMPONENT_TYPE_TRAP_TARGET ],
						g_ComponentTintColor[ UGC_COMPONENT_TYPE_TRAP_TARGET ],
						g_ComponentTintColor[ UGC_COMPONENT_TYPE_TRAP_TARGET ],
						g_ComponentTintColor[ UGC_COMPONENT_TYPE_TRAP_TARGET ]};

		targetPosUI[0] = (x - doc->backdrop_last_box.lx) / doc->layout_scale;
		targetPosUI[1] = (y - doc->backdrop_last_box.ly) / doc->layout_scale;
		ugcLayoutGetWorldCoords( doc, targetPosUI, targetPosWorld );
			
		toEmitter[0] = draw_scale * (emitterPos[ 0 ] - targetPosWorld[ 0 ] + trapEmitterDX);
		toEmitter[1] = -draw_scale * (emitterPos[ 2 ] - targetPosWorld[ 2 ] - trapEmitterDY);

		BuildCBox( &box, x, y, 0, lengthVec2( toEmitter ));
		box.lx -= ui_TextureAssemblyLeftSize( texas );
		box.ly -= ui_TextureAssemblyTopSize( texas );
		box.hx += ui_TextureAssemblyRightSize( texas );
		box.hy += ui_TextureAssemblyBottomSize( texas );

		ui_TextureAssemblyDrawRot( texas, &box, x, y, atan2( toEmitter[ 1 ], toEmitter[ 0 ]) - RAD(90), NULL, 1, z, z+0.01, 255, &tint );
	}
	assert( component->eType == UGC_COMPONENT_TYPE_TRAP_TARGET );
	StructDestroySafe( parse_UGCTrapProperties, &properties );
}

static void ugcLayoutComponentDrawLabel(UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, int flags)
{
	char buf[256];
	F32 y_offset = 0;

	if (!(flags & COMPONENT_HIGHLIGHTED))
		return;
	if( component->eType == UGC_COMPONENT_TYPE_TELEPORTER ) {
		return;
	}

	ugcComponentGetDisplayName( buf, ugcEditorGetProjectData(), component, false );

	{
		int irot = ROT_TO_QUADRANT(RAD(component->sPlacement.vRotPYR[1]+45));
		Vec2 bounds_center;
		Vec2 bounds_scale;
		UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);

		if (room_info && room_info->footprint_buf)
		{
			setVec2(bounds_center, (room_info->footprint_min[0]+room_info->footprint_max[0]+1) * UGC_ROOM_GRID * 0.5f * draw_scale,
								   (room_info->footprint_min[1]+room_info->footprint_max[1]+1) * UGC_ROOM_GRID * 0.5f * draw_scale);
			setVec2(bounds_scale, (room_info->footprint_max[0]+1-room_info->footprint_min[0]) * UGC_ROOM_GRID * draw_scale,
						   		  (room_info->footprint_max[1]+1-room_info->footprint_min[1]) * UGC_ROOM_GRID * draw_scale);
		}
		else
		{
			Vec3 boundsMin;
			Vec3 boundsMax;

			ugcComponentCalcBounds( ugcEditorGetComponentList(), component, boundsMin, boundsMax );
			
			setVec2(bounds_center, (boundsMin[0] + boundsMax[0])*0.5f * draw_scale,
								   (boundsMin[2] + boundsMax[2])*0.5f * draw_scale);
			setVec2(bounds_scale, (boundsMax[0] - boundsMin[0]) * draw_scale,
						   		  (boundsMax[2] - boundsMin[2]) * draw_scale);
		}

		switch (irot)
		{
		xcase 0:
			y_offset = -1*bounds_center[1] + bounds_scale[1]*0.5f;
		xcase 2:
			y_offset = bounds_center[1] + bounds_scale[1]*0.5f;
		xcase 1:
			y_offset = bounds_center[0] + bounds_scale[0]*0.5f;
		xcase 3:
			y_offset = -1*bounds_center[0] + bounds_scale[0]*0.5f;
		}

		y_offset += 10;
	}

	ui_StyleFontUse(RefSystem_ReferentFromString("UIStyleFont", "UGC_Important_Alternate"), false, 0);
	gfxfont_Print(x, y + y_offset, z, 1, 1, CENTER_XY, buf);
}

static void ugcLayoutComponentDrawError(UGCUIMapEditorComponent* component_widget, F32 x, F32 y, F32 z, F32 draw_scale, S32 alpha, int flags)
{
	// Kinda a hack to just draw the sprite, but we also want the logic regarding tooltip display

	// Logic that would have been in tick:
	component_widget->errorSprite->tint = colorFromRGBA( COLOR_ALPHA( 0xFFFFFF00, alpha ));
	if( flags & COMPONENT_HIGHLIGHTED ) {
		ui_TooltipsSetActiveText( ui_WidgetGetTooltip( UI_WIDGET( component_widget->errorSprite )),
								  y, y );
	}
	
	ui_SpriteDraw( component_widget->errorSprite,
				   floorf( x - UI_WIDGET( component_widget->errorSprite )->width / 2 ),
				   floorf( y - UI_WIDGET( component_widget->errorSprite )->height / 2 ),
				   UI_WIDGET( component_widget->errorSprite )->width,
				   UI_WIDGET( component_widget->errorSprite )->height,
				   1 );
}

static void ugcLayoutComponentDrawPlanet(UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, int flags)
{
	Vec3 boundsMin;
	Vec3 boundsMax;
	float fRadius;
	if (ugcComponentCalcBounds( ugcEditorGetComponentList(), component, boundsMin, boundsMax )) {
		F32 volume_scale = (boundsMax[0] - boundsMin[0]) * draw_scale / g_PlanetIcon->width;
		display_sprite_rotated(g_PlanetIcon, x, y, 0,
			z, volume_scale, (flags & COMPONENT_CUT_BUFFER) ? 0x80808040 : 0x808080FF);
	}
	if( ugcComponentCalcBoundsForObjLib( component->iPlanetRingId, boundsMin, boundsMax, &fRadius )) {
		F32 volume_scale = (boundsMax[0] - boundsMin[0]) * draw_scale / g_PlanetIcon->width;
		display_sprite_rotated(g_PlanetIcon, x, y, 0,
			z, volume_scale, (flags & COMPONENT_CUT_BUFFER) ? 0x80808020 : 0x80808050);
	}
}

static void	ugcLayoutComponentDrawPatrolPath(UGCMapEditorDoc *doc, UGCUIMapEditorComponent *component_widget, UGCComponent *component, F32 x, F32 y, F32 z, F32 scale, F32 draw_scale, int flags)
{
	UGCComponentPatrolPath *path;
	Vec3 componentPos;

	if( eaiSize( &component->eaPatrolPoints ) == 0 ) {
		return;
	}
	if(   (flags & (COMPONENT_HIGHLIGHTED | COMPONENT_SELECTED)) == 0
		  && !ugcMapEditorIsComponentPrevSelected( doc, component->uID )
		  && !ugcMapEditorIsAnyComponentSelected( doc, component->eaPatrolPoints )
		  && !ugcMapEditorIsAnyComponentPrevSelected( doc, component->eaPatrolPoints )) {
		return;
	}

	if( flags & COMPONENT_DRAGGING ) {
		copyVec3( component_widget->drag_position.position, componentPos );
	} else {
		copyVec3( component->sPlacement.vPos, componentPos );
	}

	{
		UGCComponentPatrolPoint** overrides = NULL;
		if( doc->drag_state && doc->drag_state->type == UGC_DRAG_MOVE_COMPONENT ) {
			int it;

			{
				UGCUIMapEditorComponent* dragComponentUI = doc->drag_state->primary_widget;
				UGCComponent* dragComponent = ugcLayoutGetComponentByID( dragComponentUI->uComponentID );
				if( dragComponent->eType == UGC_COMPONENT_TYPE_PATROL_POINT ) {
					UGCComponentPatrolPoint* override = StructCreate( parse_UGCComponentPatrolPoint );
					override->componentID = dragComponentUI->uComponentID;
					copyVec3( dragComponentUI->drag_position.position, override->pos );
					override->roomID = dragComponentUI->drag_position.room_id;
					eaPush( &overrides, override );
				}
			}
			for( it = 0; it != eaSize( &doc->drag_state->secondary_widgets ); ++it ) {
				UGCUIMapEditorComponent* dragComponentUI = doc->drag_state->secondary_widgets[ it ];
				UGCComponent* dragComponent = ugcEditorFindComponentByID( dragComponentUI->uComponentID );
				if( dragComponent->eType == UGC_COMPONENT_TYPE_PATROL_POINT ) {
					UGCComponentPatrolPoint* override = StructCreate( parse_UGCComponentPatrolPoint );
					override->componentID = dragComponentUI->uComponentID;
					copyVec3( dragComponentUI->drag_position.position, override->pos );
					override->roomID = dragComponentUI->drag_position.room_id;
					eaPush( &overrides, override );
				}
			}
		}

		path = ugcComponentGetPatrolPath(ugcEditorGetProjectData(), component, overrides );
		eaDestroyStruct( &overrides, parse_UGCComponentPatrolPoint );
	}

	{
		UITextureAssembly* texas = RefSystem_ReferentFromString( "UITextureAssembly", g_ArrowMarker );
		Color4 errorTint = { 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF };
		int line_idx;
		int num_points = eaSize(&path->points);
		for (line_idx = 0; line_idx < num_points; line_idx++)
		{
			Vec3 pos1, pos2;
			CBox box;
			Vec2 point1, point2;
			float centerX;
			float centerY;
			float len;
			bool line_error = false;

			if( path->patrolType == PATROL_ONEWAY && line_idx == 0 ) {
				continue;
			}

			if( line_idx == 0 ) {
				copyVec3( path->points[ num_points - 1 ]->pos, pos1 );
				copyVec3( path->points[ line_idx ]->pos, pos2 );
			} else {
				copyVec3( path->points[ line_idx - 1 ]->pos, pos1 );
				copyVec3( path->points[ line_idx ]->pos, pos2 );
			}
			if( path->points[ line_idx ]->prevConnectionInvalid ) {
				line_error = true;
			}

			subVec3(pos1, componentPos, pos1);
			setVec2(point1, x+pos1[0]*draw_scale, y-pos1[2]*draw_scale);
			subVec3(pos2, componentPos, pos2);
			setVec2(point2, x+pos2[0]*draw_scale, y-pos2[2]*draw_scale);
			len = sqrt( distance2Squared( point1, point2 ));

			centerX = (point1[0] + point2[0]) / 2;
			centerY = (point1[1] + point2[1]) / 2;
			BuildCBoxFromCenter( &box, centerX, centerY, 0, len );
			box.lx -= ui_TextureAssemblyLeftSize( texas );
			box.ly -= ui_TextureAssemblyTopSize( texas );
			box.hx += ui_TextureAssemblyRightSize( texas );
			box.hy += ui_TextureAssemblyBottomSize( texas );

			ui_TextureAssemblyDrawRot( texas, &box, centerX, centerY,
									   atan2( point2[1] - point1[1], point2[0] - point1[0] ) + RAD(90),
									   NULL, 1, z, z+0.01, 255,
									   (line_error ? &errorTint : NULL) );
		}
	}

	StructDestroy(parse_UGCComponentPatrolPath, path);
}

static void	ugcLayoutComponentDrawTeleporterPath(UGCMapEditorDoc *doc, UGCUIMapEditorComponent *component_widget, UGCComponent *component, F32 x, F32 y, F32 z, F32 scale, F32 draw_scale, int flags)
{
	UGCComponent* firstChild;
	Vec3 initialPos;
		
	if(   eaiSize( &component->uChildIDs ) < 2
		  || (!ugcMapEditorIsAnyComponentSelected( doc, component->uChildIDs )
			  && !ugcMapEditorIsAnyComponentHighlighted( doc, component->uChildIDs )
			  && !ugcMapEditorIsAnyComponentPrevSelected( doc, component->uChildIDs ))) {
		return;
	}

	firstChild = ugcEditorFindComponentByID( component->uChildIDs[ 0 ]);
	copyVec3( firstChild->sPlacement.vPos, initialPos );
	if( doc->drag_state && doc->drag_state->type == UGC_DRAG_MOVE_COMPONENT ) {
		int widgetIt;
		{
			UGCUIMapEditorComponent* dragComponentUI = doc->drag_state->primary_widget;
			if( dragComponentUI->uComponentID == firstChild->uID ) {
				copyVec3( dragComponentUI->drag_position.position, initialPos );
			}
		}
		for( widgetIt = 0; widgetIt != eaSize( &doc->drag_state->secondary_widgets ); ++widgetIt ) {
			UGCUIMapEditorComponent* dragComponentUI = doc->drag_state->secondary_widgets[ widgetIt ];
			if( dragComponentUI->uComponentID == firstChild->uID ) {
				copyVec3( dragComponentUI->drag_position.position, initialPos );
			}
		}
	}
	
	{
		UITextureAssembly* texas = RefSystem_ReferentFromString( "UITextureAssembly", g_LineMarker );
		Color4 tint = { g_ComponentTintColor[ UGC_COMPONENT_TYPE_TELEPORTER_PART ],
						g_ComponentTintColor[ UGC_COMPONENT_TYPE_TELEPORTER_PART ],
						g_ComponentTintColor[ UGC_COMPONENT_TYPE_TELEPORTER_PART ],
						g_ComponentTintColor[ UGC_COMPONENT_TYPE_TELEPORTER_PART ] };
		int it;
		for( it = 1; it < eaiSize( &component->uChildIDs ); ++it ) {
			UGCComponent* child = ugcEditorFindComponentByID( component->uChildIDs[ it ]);
			Vec2 point1, point2;
			Vec3 childPos;
			float centerX;
			float centerY;
			float len;
			CBox box;

			copyVec3( child->sPlacement.vPos, childPos );
			if( doc->drag_state && doc->drag_state->type == UGC_DRAG_MOVE_COMPONENT ) {
				int widgetIt;
				{
					UGCUIMapEditorComponent* dragComponentUI = doc->drag_state->primary_widget;
					if( dragComponentUI->uComponentID == child->uID ) {
						copyVec3( dragComponentUI->drag_position.position, childPos );
					}
				}
				for( widgetIt = 0; widgetIt != eaSize( &doc->drag_state->secondary_widgets ); ++widgetIt ) {
					UGCUIMapEditorComponent* dragComponentUI = doc->drag_state->secondary_widgets[ widgetIt ];
					if( dragComponentUI->uComponentID == child->uID ) {
						copyVec3( dragComponentUI->drag_position.position, childPos );
					}
				}
			}

			setVec2( point1, x + (initialPos[ 0 ] - component->sPlacement.vPos[ 0 ]) * draw_scale,
					 y + (component->sPlacement.vPos[ 2 ] - initialPos[ 2 ]) * draw_scale );
			setVec2( point2, x + (childPos[ 0 ] - component->sPlacement.vPos[ 0 ]) * draw_scale,
					 y + (component->sPlacement.vPos[ 2 ] - childPos[ 2 ]) * draw_scale);
			len = sqrt( distance2Squared( point1, point2 ));

			centerX = (point1[0] + point2[0]) / 2;
			centerY = (point1[1] + point2[1]) / 2;
			BuildCBoxFromCenter( &box, centerX, centerY, 0, len );
			box.lx -= ui_TextureAssemblyLeftSize( texas );
			box.ly -= ui_TextureAssemblyTopSize( texas );
			box.hx += ui_TextureAssemblyRightSize( texas );
			box.hy += ui_TextureAssemblyBottomSize( texas );

			ui_TextureAssemblyDrawRot( texas, &box, centerX, centerY,
									   atan2( point2[1] - point1[1], point2[0] - point1[0] ) + RAD(90),
									   NULL, 1, z, z+0.01, 255, &tint );
		}
	}
}

static void ugcLayoutComponentDrawSpawnArea(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, S32 object_alpha, int flags)
{
	F32 volume_scale = 2 * doc->spawn_radius * draw_scale / g_PlanetIcon->width;
	S32 alpha = (flags & COMPONENT_CUT_BUFFER) ? 0x20 : object_alpha/2;
	if (alpha > 0)
		display_sprite_rotated(g_PlanetIcon, x, y, 0,
				z, volume_scale, 0x80808000 + alpha);
}

static void ugcLayoutComponentDrawAggroArea(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, S32 object_alpha, int flags)
{
	S32 alpha = (flags & COMPONENT_CUT_BUFFER) ? 0x20 : object_alpha/2;
	if (alpha > 0)
	{
		UGCMapType map_type = ugcMapGetType(doc->map_data);
		UGCPerProjectDefaults *defaults = ugcGetDefaults();
		F32 radius = (map_type == UGC_MAP_TYPE_SPACE || map_type == UGC_MAP_TYPE_PREFAB_SPACE) ? defaults->fSpaceAggroDistance : defaults->fGroundAggroDistance;
		F32 volume_scale = 2 * radius * draw_scale / g_PlanetIcon->width;
		bool has_error = false;

		const UGCRuntimeError **error_list = ugcErrorList( ugcEditorGetRuntimeStatus(), ugcMakeTempErrorContextChallenge( ugcComponentGetLogicalNameTemp( component ), NULL, NULL ) );
		FOR_EACH_IN_EARRAY(error_list, const UGCRuntimeError, error)
		{
			if (stricmp(error->message_key, "UGC.EncountersTooCloseTogether") == 0)
			{
				has_error = true;
				break;
			}
		}
		FOR_EACH_END;
		eaDestroy(&error_list);

		display_sprite_rotated(g_PlanetIcon, x, y, 0,
				z, volume_scale, (has_error ? 0xf0202000 : 0x80808000) + alpha);
	}
}

static void ugcLayoutComponentDrawRoomDoor(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, S32 object_alpha, int flags)
{
	S32 alpha = (flags & COMPONENT_CUT_BUFFER) ? 0x40 : 0xFF;
	U32 draw_color = COLOR_ALPHA( 0x00FF0000, alpha );
	AtlasTex *marker_tex;

	if( component->eType == UGC_COMPONENT_TYPE_ROOM_DOOR ) {
		marker_tex = atlasLoadTexture("ugc_room_door");
	} else {
		marker_tex = atlasLoadTexture("ugc_room_door_closed");
	}

	display_sprite_rotated(marker_tex, x, y, RAD(rot + 90),
						   z, UGC_ROOM_GRID*draw_scale*4 / marker_tex->width, draw_color);
}

static void ugcLayoutComponentDrawRoomMarker(UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, S32 object_alpha, int flags)
{
	F32 volume_scale = 2 * component->fVolumeRadius * draw_scale / g_PlanetIcon->width;
	S32 alpha = (flags & COMPONENT_CUT_BUFFER) ? 0x20 : object_alpha;
	if (alpha > 0)
		display_sprite_rotated(g_PlanetIcon, x, y, 0,
				z, volume_scale, 0x80808000 + alpha);
}

static void ugcLayoutComponentDrawRoomFootprint(UGCMapEditorDoc *doc, UGCUIMapEditorComponent *component_widget, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, int flags)
{
	UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);
	int irot = ROT_TO_QUADRANT(RAD(rot));
	int px, py;
	int width, height;
	UGCRoomInfo **coll_infos = NULL;
	S32 *coll_offsets = NULL;
	Vec3 component_pos;
	U32 alpha = CLAMP(255-doc->rooms_fade*255, 0, 255);

	if (!room_info || !room_info->footprint_buf)
		return;

	width = room_info->footprint_max[0]+1-room_info->footprint_min[0];
	height = room_info->footprint_max[1]+1-room_info->footprint_min[1];

	if (flags & (COMPONENT_CUT_BUFFER | COMPONENT_DRAGGING)) {
		alpha = MIN( 0x40, alpha );
	}
	
	if (flags & COMPONENT_DRAGGING)
	{
		copyVec3(component_widget->drag_position.position, component_pos);
	}
	else 
	{
		copyVec3(component->sPlacement.vPos, component_pos);
	}

	{
		U32 *drag_ids = NULL;
		F32 *drag_positions = NULL;

		if (doc->drag_state)
		{
			if (doc->drag_state->primary_widget)
			{
				eaiPush(&drag_ids, doc->drag_state->primary_widget->uComponentID);
				eafPush(&drag_positions, doc->drag_state->primary_widget->drag_position.position[0]);
				eafPush(&drag_positions, doc->drag_state->primary_widget->drag_position.position[1]);
				eafPush(&drag_positions, doc->drag_state->primary_widget->drag_position.position[2]);
				eafPush(&drag_positions, 0);
				eafPush(&drag_positions, doc->drag_state->primary_widget->drag_position.rotation);
				eafPush(&drag_positions, 0);
			}
			FOR_EACH_IN_EARRAY(doc->drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget)
			{
				eaiPush(&drag_ids, secondary_widget->uComponentID);
				eafPush(&drag_positions, secondary_widget->drag_position.position[0]);
				eafPush(&drag_positions, secondary_widget->drag_position.position[1]);
				eafPush(&drag_positions, secondary_widget->drag_position.position[2]);
				eafPush(&drag_positions, 0);
				eafPush(&drag_positions, secondary_widget->drag_position.rotation);
				eafPush(&drag_positions, 0);
			}
			FOR_EACH_END;
		}

		{
			Vec3 rotVec3;
			setVec3( rotVec3, 0, rot, 0 );
			ugcRoomGetColliderList(ugcEditorGetComponentList(),
								   drag_ids, drag_positions,
								   component, component_pos, rotVec3, room_info,
								   false, &coll_infos, &coll_offsets);
		}

		eaiDestroy(&drag_ids);
		eafDestroy(&drag_positions);
	}

	for (py = 0; py < height; py++)
		for (px = 0; px < width; px++)
			if (room_info->footprint_buf[px+py*width] != 0)
			{
				IVec2 in_pt = { px+room_info->footprint_min[0], py+room_info->footprint_min[1] };
				IVec2 rotated_pt;
				bool valid;
				U32 cell_color = COLOR_ALPHA( 0xFFFFFFFF, alpha );
				F32 cell_z = z;

				ugcRoomRotateAndFlipPoint(in_pt, irot, rotated_pt);
				valid = !ugcRoomCheckCollisionPoint(rotated_pt, irot, coll_infos, coll_offsets);

				if (!valid)
				{
					cell_color &= 0xFF0000FF;
					cell_z += 0.01f;
				}

				if (!valid)
					display_sprite_rotated_ex(NULL, white_tex,
							x+rotated_pt[0]*UGC_ROOM_GRID*draw_scale,
							y+rotated_pt[1]*UGC_ROOM_GRID*draw_scale, cell_z,
							UGC_ROOM_GRID*draw_scale / white_tex->width, UGC_ROOM_GRID*draw_scale / white_tex->height,
							cell_color, 0, false);
				else
					display_sprite_rotated_ex(g_RoomGridIcon, NULL,
							x+rotated_pt[0]*UGC_ROOM_GRID*draw_scale,
							y+rotated_pt[1]*UGC_ROOM_GRID*draw_scale, cell_z,
							UGC_ROOM_GRID*draw_scale / g_RoomGridIcon->width, UGC_ROOM_GRID*draw_scale / g_RoomGridIcon->height,
							cell_color, 0, false);
			}

	eaDestroy(&coll_infos);
	eaiDestroy(&coll_offsets);
}

static void ugcLayoutSwapComponentDragPosition(UGCUIMapEditorComponent *component_widget)
{
	Vec3 pos_swap;
	F32 rot_swap;
	UGCComponent *drag_component = ugcLayoutGetComponentByID(component_widget->uComponentID);
	if (drag_component)
	{
		copyVec3(drag_component->sPlacement.vPos, pos_swap);
		copyVec3(component_widget->drag_position.position, drag_component->sPlacement.vPos);
		copyVec3(pos_swap, component_widget->drag_position.position);

		rot_swap = drag_component->sPlacement.vRotPYR[1];
		drag_component->sPlacement.vRotPYR[1] = component_widget->drag_position.rotation;
		component_widget->drag_position.rotation = rot_swap;
	}
}

static void ugcLayoutComponentDrawRoomDoorSlots(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, int flags)
{
	UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);
	int irot = ROT_TO_QUADRANT(RAD(rot));
	//Vec3 temp_pos;
	S32 alpha = 255-doc->rooms_fade*255;

	if (!room_info || !room_info->footprint_buf || alpha == 0)
		return;

	// It is easier to temporarily set the position & rotation on the components so all the functions below Just Work
	if (doc->drag_state)
	{
		if (doc->drag_state->primary_widget)
		{
			ugcLayoutSwapComponentDragPosition(doc->drag_state->primary_widget);
		}
		FOR_EACH_IN_EARRAY(doc->drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget)
		{
			ugcLayoutSwapComponentDragPosition(secondary_widget);
		}
		FOR_EACH_END;
	}
		
	FOR_EACH_IN_EARRAY(room_info->doors, UGCRoomDoorInfo, door)
	{
		int door_color = 0x8080FF00;
		IVec2 in_pt, out_pt, out_pt_2;
		UGCComponent *other_room = NULL;
		int other_door_id = 0;
		F32 out_x, out_y;
		F32 icon_size = UGC_ROOM_GRID * draw_scale;
		UGCDoorSlotState state = ugcRoomGetDoorSlotState(ugcEditorGetComponentList(), component, FOR_EACH_IDX(room_info->doors, door), NULL, NULL, &other_room, &other_door_id);

		if( state != UGC_DOOR_SLOT_EMPTY ) {
			continue;
		}

		setVec2(in_pt, door->pos[0], door->pos[2]);
		ugcRoomRotateAndFlipPoint(in_pt, irot, out_pt);

		setVec2(in_pt, door->pos[0]-1, door->pos[2]-1);
		ugcRoomRotateAndFlipPoint(in_pt, irot, out_pt_2);

		out_x = (out_pt[0] + out_pt_2[0] + 1) * 0.5f * icon_size + x;
		out_y = (out_pt[1] + out_pt_2[1] + 1) * 0.5f * icon_size + y;
		
		display_sprite_rotated_ex( g_RoomDoorIcon, NULL,
								   out_x - icon_size * 2,
								   out_y - icon_size * 1,
								   z,
								   icon_size * 4 / g_RoomDoorIcon->width,
								   icon_size * 2 / g_RoomDoorIcon->height,
								   door_color | alpha,
								   door->rot + RAD(rot),
								   false );
		// Uncomment this to see door pivot points
		// display_sprite( atlasFindTexture( "white" ), x, y, UI_TOP_Z, .5, .5, -1 );
	}
	FOR_EACH_END;

	// See comment above
	if (doc->drag_state)
	{
		if (doc->drag_state->primary_widget)
		{
			ugcLayoutSwapComponentDragPosition(doc->drag_state->primary_widget);
		}
		FOR_EACH_IN_EARRAY(doc->drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget)
		{
			ugcLayoutSwapComponentDragPosition(secondary_widget);
		}
		FOR_EACH_END;
	}
}

static void ugcLayoutComponentDrawObjectPreview(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, int flags)
{
	S32 alpha = (flags & (COMPONENT_CUT_BUFFER | COMPONENT_DRAGGING)) ? 0x40 : 0xFF;
	BasicTexture *tex = NULL;
	Vec3 boundsMin;
	Vec3 boundsMax;

	{
		ResourceSnapDesc desc = { 0 };
		char buffer[ 256 ];
		desc.astrDictName = allocAddString( "ObjectLibrary" );
		sprintf( buffer, "%d", component->iObjectLibraryId );
		desc.astrResName = allocAddString( buffer );
		desc.objectIsTopDownView = true;
		tex = texFind( gclSnapGetResourceString( &desc, true ), false );
	}

	if (!tex)
		return;

	if( ugcComponentCalcBounds( ugcEditorGetComponentList(), component, boundsMin, boundsMax )) {				
		F32 center_x = (boundsMin[0] + boundsMax[0])*0.5f * draw_scale;
		F32 center_z = (boundsMin[2] + boundsMax[2])*0.5f * draw_scale;
		F32 scale_x = (boundsMax[0] - boundsMin[0]) * draw_scale;
		F32 scale_z = (boundsMax[2] - boundsMin[2]) * draw_scale;
		F32 sin_rot = sin(RAD(rot));
		F32 cos_rot = cos(RAD(rot));
		F32 offset_x = center_x * cos_rot + center_z * sin_rot + x;
		F32 offset_z = center_x * sin_rot - center_z * cos_rot + y;
		Vec2 final_position = { offset_x-scale_x*0.5f, offset_z-scale_z*0.5f };
		F32 bounds_size = MAX(scale_x, scale_z);
		const CBox *clipper_box = clipperGetCurrentCBox();

		if (final_position[0] - bounds_size < clipper_box->right &&
			final_position[0] + bounds_size > clipper_box->left &&
			final_position[1] - bounds_size < clipper_box->bottom &&
			final_position[1] + bounds_size > clipper_box->top)
		{
			display_sprite_rotated_ex(NULL, tex, final_position[0], final_position[1], z,
									  scale_x / tex->width, scale_z / tex->height,
									  COLOR_ALPHA( 0xFFFFFFFF, alpha ),
									  RAD(rot), false);
		}
	}
}

static void ugcLayoutComponentDrawObjectBounds(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 rot, F32 draw_scale, int flags)
{
	Vec3 boundsMin;
	Vec3 boundsMax;
	UGCRoomInfo *room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);
	S32 alpha = 255-doc->objects_fade*255;

	if( (flags & (COMPONENT_HIGHLIGHTED | COMPONENT_SELECTED)) == 0 ) {
		return;
	} 
	
	
	if (component->eType == UGC_COMPONENT_TYPE_ROOM)
		alpha = 255-doc->rooms_fade*255;

	if (alpha != 0 && ugcComponentCalcBounds( ugcEditorGetComponentList(), component, boundsMin, boundsMax )) {
		CBox cbox;
		if( room_info && room_info->footprint_buf ) {
			CBoxSet( &cbox, x + room_info->footprint_min[0] * UGC_ROOM_GRID * draw_scale,
					 y - (room_info->footprint_max[1] + 1) * UGC_ROOM_GRID * draw_scale,
					 x + (room_info->footprint_max[0] + 1) * UGC_ROOM_GRID * draw_scale,
					 y - room_info->footprint_min[1] * UGC_ROOM_GRID * draw_scale );
			ui_StyleBorderDrawMagicRot( ui_StyleBorderGet( "CarbonFibre_BoundingBox" ), &cbox, x, y, RAD(rot), z, 1, alpha );
		} else {
			CBoxSet( &cbox, x + boundsMin[0] * draw_scale, y - boundsMax[2] * draw_scale,
					 x + boundsMax[0] * draw_scale, y - boundsMin[2] * draw_scale );
			ui_StyleBorderDrawMagicRot( ui_StyleBorderGet( "CarbonFibre_BoundingBox" ), &cbox, x, y, RAD(rot), z, 1, alpha );
		}
	}
}

static void ugcLayoutComponentDrawObjectSoundRadii(UGCMapEditorDoc *doc, UGCComponent *component, F32 x, F32 y, F32 z, F32 draw_scale, S32 object_alpha, int flags)
{
	if(component->strSoundEvent)
	{
		UGCSound *sound = RefSystem_ReferentFromString("UGCSound", component->strSoundEvent);
		if(sound && !nullStr(sound->strSoundName))
		{
			S32 alpha = (flags & COMPONENT_CUT_BUFFER) ? 0x10 : object_alpha/4;
			if (alpha > 0)
			{
				UGCMapType map_type = ugcMapGetType(doc->map_data);
				UGCPerProjectDefaults *defaults = ugcGetDefaults();
				F32 radius = sndGetEventRadius(sound->strSoundName);
				F32 volume_scale = 2 * radius * draw_scale / g_PlanetIcon->width;
				display_sprite_rotated(g_PlanetIcon, x, y, 0, z, volume_scale, 0x80FF8000 | alpha);
			}
		}
	}
}

void ugcLayoutUIComponentUpdate(UGCUIMapEditorComponent *component_widget)
{
	UGCMapEditorDoc *doc = component_widget->doc;
	UGCComponent *component = ugcLayoutGetComponentByID(component_widget->uComponentID);
	UGCComponentPlacement *placement;
	F32 spacing = doc->layout_grid_size / doc->layout_kit_spacing;
	Vec2 pos = { 0, 0 };

	if (!component)
		return;

	placement = &component->sPlacement;

	if (placement->uRoomID == UGC_TOPLEVEL_ROOM_ID)
	{
		ugcLayoutGetUICoords(doc, placement->vPos, pos);
	}
	else
	{
		pos[0] = placement->vPos[0] * spacing;
		pos[1] = placement->vPos[2] * spacing;
	}
	UGC_UI_MAP_EDITOR_WIDGET( component_widget )->x = pos[0];
	UGC_UI_MAP_EDITOR_WIDGET( component_widget )->y = pos[1];
	component_widget->isValidPosition = ugcComponentIsValidPosition(ugcEditorGetProjectData(), ugcEditorGetBacklinkTable(), component, component->sPlacement.vPos, NULL, false, 0, 0, NULL);

	{
		UGCRoomInfo *coll_infos = ugcRoomGetRoomInfo(component->iObjectLibraryId);
		if (coll_infos && coll_infos->iNumLevels > 1 && component_widget->selected_level == -1)
		{
			component_widget->selected_level = 0;
		}
	}

	ugcErrorButtonRefreshForMapComponent( &component_widget->errorSprite, ugcEditorGetRuntimeStatus(), ugcComponentGetLogicalNameTemp( component ));
}

static void ugcLayoutUIComponentFreeInternal( UGCUIMapEditorComponent* component )
{
	ui_WidgetForceQueueFree( UI_WIDGET( component->errorSprite ));
	ugcMapEditorWidgetFreeInternal( UGC_UI_MAP_EDITOR_WIDGET( component ));
}

UGCUIMapEditorComponent *ugcLayoutUIComponentCreate(UGCMapEditorDoc *doc, UGCComponent* component, bool is_static)
{
	UGCUIMapEditorComponent *ret = calloc(1, sizeof(UGCUIMapEditorComponent));
	UGC_UI_MAP_EDITOR_WIDGET( ret )->tickF = ugcLayoutComponentTick;
	UGC_UI_MAP_EDITOR_WIDGET( ret )->drawF = ugcLayoutComponentDraw;
	UGC_UI_MAP_EDITOR_WIDGET( ret )->freeF = ugcLayoutUIComponentFreeInternal;
	ret->uComponentID = component->uID;
	if( component->eType == UGC_COMPONENT_TYPE_PATROL_POINT ) {
		ret->uComponentLogicalParent = component->uPatrolParentID;
	}
	ret->doc = doc;
	ret->is_static = is_static;
	ret->selected_level = -1;
	ugcLayoutUIComponentUpdate(ret);
	if (!is_static)
		eaPush(&doc->component_widgets, ret);
	return ret;
}

static void ugcLayoutUIObjectiveWaypointTick( UGCUIMapEditorObjectiveWaypoint* waypoint, UI_PARENT_ARGS )
{
	UGC_UI_MAP_EDITOR_GET_COORDINATES(waypoint);
	UGCMapEditorDoc* doc = waypoint->doc;
	
	// Override hovering logic
	if( mouseCollision( NULL )) {
		int mouseX;
		int mouseY;
		Vec2 mouseDelta;
		Vec2 min;
		Vec2 max;

		mousePos( &mouseX, &mouseY );
		min[ 0 ] = waypoint->volume.extents[ 0 ][ 0 ] * doc->layout_grid_size / doc->layout_kit_spacing * scale;
		min[ 1 ] = waypoint->volume.extents[ 0 ][ 2 ] * doc->layout_grid_size / doc->layout_kit_spacing * scale;
		max[ 0 ] = waypoint->volume.extents[ 1 ][ 0 ] * doc->layout_grid_size / doc->layout_kit_spacing * scale;
		max[ 1 ] = waypoint->volume.extents[ 1 ][ 2 ] * doc->layout_grid_size / doc->layout_kit_spacing * scale;

		setVec2( mouseDelta, mouseX - x, mouseY - y );
		rotateXZ( -waypoint->volume.rot, &mouseDelta[ 0 ], &mouseDelta[ 1 ]);

		if(   min[ 0 ] <= mouseDelta[ 0 ] && mouseDelta[ 0 ] <= max[ 0 ]
			  && min[ 1 ] <= mouseDelta[ 1 ] && mouseDelta[ 1 ] <= max[ 1 ]) {
			UGC_UI_MAP_EDITOR_WIDGET(waypoint)->state |= kWidgetModifier_Hovering;
		} else {
			UGC_UI_MAP_EDITOR_WIDGET(waypoint)->state &= ~kWidgetModifier_Hovering;
		}
	}
}

static void ugcLayoutUIObjectiveWaypointDraw( UGCUIMapEditorObjectiveWaypoint* waypoint, UGCUIMapEditorDrawLayer layer, UI_PARENT_ARGS )
{
	UGC_UI_MAP_EDITOR_GET_COORDINATES( waypoint );
	UGCMapEditorDoc* doc = waypoint->doc;
	AtlasTex* areaWaypointTex = atlasLoadTexture( "MapIcons_AreaWaypoint" );
	AtlasTex* pointWaypointTex = atlasLoadTexture( "Map_Icon_Quest_Waypoint" );


	if( layer == UGC_MAP_LAYER_VOLUME ) {	
		if( waypoint->eMode == UGC_WAYPOINT_AREA ) {
			Vec2 min;
			Vec2 max;
			CBox box;
			min[ 0 ] = waypoint->volume.extents[ 0 ][ 0 ] * doc->layout_grid_size / doc->layout_kit_spacing * scale;
			min[ 1 ] = waypoint->volume.extents[ 0 ][ 2 ] * doc->layout_grid_size / doc->layout_kit_spacing * scale;
			max[ 0 ] = waypoint->volume.extents[ 1 ][ 0 ] * doc->layout_grid_size / doc->layout_kit_spacing * scale;
			max[ 1 ] = waypoint->volume.extents[ 1 ][ 2 ] * doc->layout_grid_size / doc->layout_kit_spacing * scale;

			BuildCBoxFromCenter( &box, x, y, max[ 0 ] - min[ 0 ], max[ 1 ] - min[ 1 ]);
			display_sprite_box_4Color_rot( areaWaypointTex, &box, z, -1, -1, -1, -1, x, y, waypoint->volume.rot );
		} else if( waypoint->eMode == UGC_WAYPOINT_POINTS ) {
			int it;
			for( it = 0; it < eafSize( &waypoint->eaPositions ); it += 3 ) {
				float* pos = &waypoint->eaPositions[ it ];
				Vec2 uiPos;
				ugcLayoutGetUICoords( doc, pos, uiPos );
				display_sprite( pointWaypointTex,
								pX + uiPos[ 0 ] * pScale - pointWaypointTex->width / 2,
								pY + uiPos[ 1 ] * pScale - pointWaypointTex->height / 2,
								z, 1, 1, -1 );
			}
		}
	}

	if( layer == UGC_MAP_LAYER_TEXT ) {
		if( UGC_UI_MAP_EDITOR_WIDGET(waypoint)->state & kWidgetModifier_Hovering ) {
			UGCMissionObjective* objective = ugcObjectiveFind( ugcEditorGetProjectData()->mission->objectives, waypoint->objectiveID );

			if( objective )
			{
				// For cases where the waypoint and an object are in the same place, we'd like the text
				//   to not overlap. Hardcode an offset for the objective text to minimize this as a default.
				F32 fVertOffset = 25.0f;
				ui_StyleFontUse(RefSystem_ReferentFromString("UIStyleFont", "UGC_Important_Alternate"), false, 0);
				gfxfont_Printf( x, y+fVertOffset, z, 1, 1, CENTER_XY, "%s", ugcMissionObjectiveUIString( objective ));
			}
		}
	}
}

static void ugcLayoutUIObjectiveWaypointFreeInternal( UGCUIMapEditorObjectiveWaypoint* waypoint )
{
	eafDestroy( &waypoint->eaPositions );
	ugcMapEditorWidgetFreeInternal( UGC_UI_MAP_EDITOR_WIDGET( waypoint ));
}

static UGCUIMapEditorObjectiveWaypoint* ugcLayoutUIObjectiveWaypointIntern( UGCMapEditorDoc* doc, UGCUIMapEditorWidgetContainer* parent, int* pUIIt )
{
	UGCUIMapEditorObjectiveWaypoint* ui;
	
	if( *pUIIt >= eaSize( &doc->objectiveWaypointWidgets )) {
		ui = calloc( 1, sizeof( *ui ));
		UGC_UI_MAP_EDITOR_WIDGET(ui)->tickF = ugcLayoutUIObjectiveWaypointTick;
		UGC_UI_MAP_EDITOR_WIDGET(ui)->drawF = ugcLayoutUIObjectiveWaypointDraw;
		UGC_UI_MAP_EDITOR_WIDGET(ui)->freeF = ugcLayoutUIObjectiveWaypointFreeInternal;
		ui->doc = doc;
		eaPush( &doc->objectiveWaypointWidgets, ui );
	}

	assert( *pUIIt < eaSize( &doc->objectiveWaypointWidgets ));
	ui = doc->objectiveWaypointWidgets[ *pUIIt ];
	++*pUIIt;

	return ui;
}

void ugcLayoutUIObjectiveWaypointRefresh( UGCMapEditorDoc* doc, UGCUIMapEditorWidgetContainer* parent, int* pUIIt, UGCMissionObjective* objective )
{
	if( objective->waypointMode ) {
		UGCUIMapEditorObjectiveWaypoint* ui = ugcLayoutUIObjectiveWaypointIntern( doc, parent, pUIIt );
		Vec2 uiPos;

		ui->objectiveID = objective->id;
		ui->eMode = objective->waypointMode;
		eafClear( &ui->eaPositions );
	
		if( objective->type == UGCOBJ_COMPLETE_COMPONENT || objective->type == UGCOBJ_UNLOCK_DOOR ) {
			UGCComponent* primaryComponent = ugcEditorFindComponentByID( objective->componentID );
			if( primaryComponent->eType == UGC_COMPONENT_TYPE_DIALOG_TREE ) {
				primaryComponent = ugcEditorFindComponentByID( primaryComponent->uActorID );
			}
		
			if(   primaryComponent->sPlacement.uRoomID != GENESIS_UNPLACED_ID
				  && primaryComponent->eType != UGC_COMPONENT_TYPE_WHOLE_MAP ) {
				eafPush3( &ui->eaPositions, primaryComponent->sPlacement.vPos );
			}
		
			{
				int it;
				for( it = 0; it != eaiSize( &objective->extraComponentIDs ); ++it ) {
					UGCComponent* component = ugcEditorFindComponentByID( objective->extraComponentIDs[ it ]);
					if( component->sPlacement.uRoomID != GENESIS_UNPLACED_ID ) {
						eafPush3( &ui->eaPositions, component->sPlacement.vPos );
					}
				}
			}
		}

		ugcGetBoundingVolumeFromPoints( &ui->volume, ui->eaPositions );
		if( parent ) {
			ugcMapEditorWidgetContainerAddChild( parent, UGC_UI_MAP_EDITOR_WIDGET( ui ));
		}
		ugcLayoutGetUICoords( doc, ui->volume.center, uiPos );
		UGC_UI_MAP_EDITOR_WIDGET( ui )->x = uiPos[ 0 ];
		UGC_UI_MAP_EDITOR_WIDGET( ui )->y = uiPos[ 1 ];
	}

	{
		int it;
			
		for( it = 0; it != eaSize( &objective->eaChildren ); ++it ) {
			ugcLayoutUIObjectiveWaypointRefresh( doc, parent, pUIIt, objective->eaChildren[ it ]);
		}
	}
}

////////////////////////////////////////////////////////////////
// Backdrop widget
////////////////////////////////////////////////////////////////

static void ugcMapUIMarqueeDragCB(UGCMapEditorDoc *doc, UGCMapUIDragEvent event);

static void ugcMapUIBackdropDragCB(UGCMapEditorDoc *doc, UGCMapUIDragEvent event)
{
	if( doc->drag_state->type == UGC_DRAG_MARQUEE_SELECT ) {
		ugcMapUIMarqueeDragCB( doc, event );
	} else {
		// nothing to do... drag is handled by the scrollarea
	}
}

static void ugcMapUIMarqueeDragCB(UGCMapEditorDoc *doc, UGCMapUIDragEvent event)
{
	Vec3 worldMin, worldMax;
	{
		Vec2 uiCoords;
		Vec3 worldCoords;
		setVec2(uiCoords,
				(g_ui_State.mouseX - doc->backdrop_last_box.lx) / doc->layout_scale,
				(g_ui_State.mouseY - doc->backdrop_last_box.ly) / doc->layout_scale);
		ugcLayoutGetWorldCoords(doc, uiCoords, worldCoords);

		setVec3( worldMin,
				 MIN( worldCoords[ 0 ], doc->drag_state->click_world_xz[ 0 ]), 0,
				 MIN( worldCoords[ 2 ], doc->drag_state->click_world_xz[ 1 ]));
		setVec3( worldMax,
				 MAX( worldCoords[ 0 ], doc->drag_state->click_world_xz[ 0 ]), 0,
				 MAX( worldCoords[ 2 ], doc->drag_state->click_world_xz[ 1 ]));
	}

	ugcMapEditorClearSelection(doc);

	FOR_EACH_IN_EARRAY(doc->component_widgets, UGCUIMapEditorComponent, component_widget)
	{
		UGCComponent *component = ugcEditorFindComponentByID(component_widget->uComponentID);
		if( component_widget->uComponentID == 0 ) {
			continue;
		}
			
		// handle these only in the second loop
		if( component->eType == UGC_COMPONENT_TYPE_PATROL_POINT ) {
			continue;
		}
		if (component->eType == UGC_COMPONENT_TYPE_ROOM)
		{
			UGCRoomInfo *room_info;

			if (doc->rooms_fade > 0.05f)
				continue;

			room_info = ugcRoomGetRoomInfo(component->iObjectLibraryId);
			if (!room_info)
				continue;

			if (   component->sPlacement.vPos[0]+room_info->footprint_max[0]*UGC_ROOM_GRID >= worldMin[0]
				&& component->sPlacement.vPos[0]+room_info->footprint_min[0]*UGC_ROOM_GRID <= worldMax[0]
				&& component->sPlacement.vPos[2]+room_info->footprint_max[1]*UGC_ROOM_GRID >= worldMin[2]
				&& component->sPlacement.vPos[2]+room_info->footprint_min[1]*UGC_ROOM_GRID <= worldMax[2])
			{
				ugcMapEditorAddSelectedComponent(doc, component_widget->uComponentID, false, false);
			}
		}
		else
		{
			if (ugcLayoutComponentHandleAlpha( doc, component ) < 240)
				continue;
			if (component->sPlacement.vPos[0] >= worldMin[0] && component->sPlacement.vPos[0] <= worldMax[0]
				&& component->sPlacement.vPos[2] >= worldMin[2] && component->sPlacement.vPos[2] <= worldMax[2])
			{
				ugcMapEditorAddSelectedComponent(doc, component_widget->uComponentID, false, false);
			}
		}
	}
	FOR_EACH_END;

	FOR_EACH_IN_EARRAY(doc->component_widgets, UGCUIMapEditorComponent, component_widget)
	{
		UGCComponent *component = ugcEditorFindComponentByID(component_widget->uComponentID);
		if( component_widget->uComponentID == 0 ) {
			continue;
		}

		if( component->eType == UGC_COMPONENT_TYPE_PATROL_POINT ) {
			if(   !ugcMapEditorIsComponentHighlighted( doc, component->uPatrolParentID )
				  && !ugcMapEditorIsComponentSelected( doc, component->uPatrolParentID )) {
				UGCComponent* patrolParent = ugcEditorFindComponentByID( component->uPatrolParentID );

				if( !patrolParent ){
					continue;
				}
				if(   !ugcMapEditorIsAnyComponentSelected( doc, patrolParent->eaPatrolPoints )
					  && !ugcMapEditorIsAnyComponentPrevSelected( doc, patrolParent->eaPatrolPoints )) {
					continue;
				}
			}
			if( doc->objects_fade > 0.05f ) {
				continue;
			}
			
			if (component->sPlacement.vPos[0] >= worldMin[0] && component->sPlacement.vPos[0] <= worldMax[0]
				&& component->sPlacement.vPos[2] >= worldMin[2] && component->sPlacement.vPos[2] <= worldMax[2])
			{
				ugcMapEditorAddSelectedComponent(doc, component_widget->uComponentID, false, false);
			}
		}
	}
	FOR_EACH_END;

	// If this is the end, we need to update the focused widget
	if( event == UGC_DRAG_DROPPED ) {
		ugcMapEditorUpdateFocusForSelection( doc );
	}
}

static bool ugcLayoutBackdropMouseCB( UGCMapEditorDoc* doc, UGCMapUIMouseEvent* event )
{
	if( event->type == UGC_MOUSE_EVENT_HOVER && inpLevelPeek( INP_SPACE )) {
		ui_SetCursorByName( "UGC_Cursors_Hand_Open" );
	}

	switch( event->button ) {
		xcase MS_LEFT:
			switch( event->type ) {
				xcase UGC_MOUSE_EVENT_CLICK:
					ui_SetFocus( &g_UGCMapEditorBackdropWidgetPlaceholder );
					ugcMapEditorClearSelection(doc);
					ugcEditorQueueUIUpdate();
				xcase UGC_MOUSE_EVENT_DRAG:
					ui_SetFocus( &g_UGCMapEditorBackdropWidgetPlaceholder );
					ugcMapEditorClearSelection( doc );
					ugcEditorQueueUIUpdate();
					if( inpLevelPeek( INP_SPACE )) {
						ugcMapUIStartDrag(doc, event->button, 0, 0, UGC_DRAG_MMB_PAN, false, ugcMapUIBackdropDragCB);
						return false;
					} else {
						ugcMapUIStartDrag(doc, event->button, 0, 0, UGC_DRAG_MARQUEE_SELECT, false, ugcMapUIBackdropDragCB);
						return true;
					}
				xdefault:
					ugcMapEditorClearHighlight(doc);
			}
		xcase MS_RIGHT:
			switch( event->type ) {
				xcase UGC_MOUSE_EVENT_CLICK: {
					UGCActionID actions[] = { UGC_ACTION_PASTE, UGC_ACTION_CREATE_MARKER, UGC_ACTION_CREATE_RESPAWN, UGC_ACTION_PLAY_MAP_FROM_LOCATION, 0 };
					ui_SetFocus( &g_UGCMapEditorBackdropWidgetPlaceholder );
					ugcMapEditorClearSelection(doc);
					ugcEditorShowContextMenu(actions);
					ugcEditorQueueUIUpdate();
				}
			}
	}

	return true;
}

static void ugcLayoutBackdropTick(UGCUIMapEditorBackdrop *backdrop_widget, UI_PARENT_ARGS)
{
	UGC_UI_MAP_EDITOR_GET_COORDINATES(backdrop_widget);
	UGCMapEditorDoc *doc = backdrop_widget->doc;
	static F32 initial_rate = 1.03f;
	static F32 decay_rate = 0.85f;

	if (!doc)
		return;

	if (backdrop_widget->minimap)
	{
		F32 minimap_scale = scale * doc->layout_grid_size / doc->layout_kit_spacing;
		UI_WIDGET(backdrop_widget->minimap)->tickF(backdrop_widget->minimap, pX, pY, pW, pH, minimap_scale);
	}

	BuildCBox( &doc->backdrop_last_box, pX, pY, pW, pH );


	doc->layout_scale = doc->map_widget->childScale;

	{
		static F32 fade_rate = 0.07f;
		if (doc->mode == UGC_MAP_EDITOR_DETAIL) {
			doc->objects_fade = doc->objects_fade*(1-fade_rate);
			doc->rooms_fade = (doc->rooms_fade*(1-fade_rate) + (fade_rate));
		} else if( doc->mode == UGC_MAP_EDITOR_LAYOUT ) {
			doc->objects_fade = (doc->objects_fade*(1-fade_rate) + (fade_rate));
			doc->rooms_fade = doc->rooms_fade*(1-fade_rate);
		}
	}
}

static void ugcLayoutPaneTick(UIPane* pane, UI_PARENT_ARGS)
{
	UGCMapEditorDoc *doc = pane->widget.userinfo;
	UIScrollbar* sb = SAFE_MEMBER( UI_WIDGET( doc->map_widget ), sb );
	UI_GET_COORDINATES(pane);
	if( sb ) {
		BuildCBox( &doc->layout_last_box, x, y, w - ui_ScrollbarWidth( sb ), h - ui_ScrollbarHeight( sb ));
	} else {
		BuildCBox( &doc->layout_last_box, x, y, w, h );
	}
	
	ui_PaneTick( pane, UI_PARENT_VALUES );
}

void ugcMapEditorZoomToLayoutMode(UGCMapEditorDoc *doc)
{
	if (doc->layout_scale > UGC_MAP_ROOM_FADE_CUTOFF)
		ui_ScrollAreaZoomToScale(doc->map_widget, UGC_MAP_ROOM_FADE_CUTOFF * 0.95f, -1);
}

void ugcMapEditorZoomToDetailMode(UGCMapEditorDoc *doc)
{
	if (doc->layout_scale < UGC_MAP_OBJECT_FADE_CUTOFF)
		ui_ScrollAreaZoomToScale(doc->map_widget, UGC_MAP_OBJECT_FADE_CUTOFF * 1.05f, 1);
}

/// This function should put all the children that would be created
/// related to COMPONENT into the UGC-PROJ.
///
/// It exists so when dragging a new component from the library, the
/// children that will get created (either at drop or during fixup)
/// will be seen.
void ugcComponentCreateChildrenForInitialDrag( UGCProjectData* ugcProj, UGCComponent* component )
{
	switch( component->eType ) {
		xcase UGC_COMPONENT_TYPE_KILL:
			ugcComponentCreateActorChildren( ugcProj, component );

		xcase UGC_COMPONENT_TYPE_TRAP:
			ugcComponentCreateTrapChildren( ugcProj, component );

		xcase UGC_COMPONENT_TYPE_TELEPORTER: case UGC_COMPONENT_TYPE_CLUSTER:
			ugcComponentCreateClusterChildren( ugcProj, component );
	}
}


static void ugcLayoutBackdropDraw(UGCUIMapEditorBackdrop *backdrop_widget, UGCUIMapEditorDrawLayer layer, UI_PARENT_ARGS)
{
	UGC_UI_MAP_EDITOR_GET_COORDINATES(backdrop_widget);
	UGCMapEditorDoc *doc = backdrop_widget->doc;
	UIScrollbar *sb;
	UIScrollArea *sa;
	Vec2 scroll_size;
	BasicTexture* gridTex = texFind( "UGC_Kits_Details_Window_Checkerboard", true );
	AtlasTex* whiteTex = atlasFindTexture( "white" );

	if (!doc || layer != UGC_MAP_LAYER_BACKDROP)
		return;

	sa = doc->map_widget;
	sb = UI_WIDGET(sa)->sb;

	ugcLayoutGetUISize(doc, scroll_size);

	if( gridTex )
	{
		int lightness = 255 - doc->rooms_fade*255;
		int color = (lightness<<8) + (lightness<<16) + (lightness<<24) + 0xFF;
		const CBox* clipBox = clipperGetCurrentCBox();
		CBox areaBox;
		BuildCBox( &areaBox, pX, pY, scroll_size[0]*scale, scroll_size[1]*scale );

		display_sprite_tiled_box_scaled( gridTex, clipBox, g_ui_State.drawZ++, color, 1, 1 );

		// Highlight the unusable areas
		if( areaBox.ly > clipBox->ly ) {
			CBox topUnusable = { clipBox->lx, clipBox->ly, clipBox->hx, areaBox.ly };
			display_sprite_box( whiteTex, &topUnusable, g_ui_State.drawZ++, 0xFFFFFF80 );
		}
		if( areaBox.hy < clipBox->hy ) {
			CBox bottomUnusable = { clipBox->lx, areaBox.hy, clipBox->hx, clipBox->hy };
			display_sprite_box( whiteTex, &bottomUnusable, g_ui_State.drawZ++, 0xFFFFFF80 );
		}
		if( areaBox.lx > clipBox->lx ) {
			CBox leftUnusable = { clipBox->lx, areaBox.ly, areaBox.lx, areaBox.hy };
			display_sprite_box( whiteTex, &leftUnusable, g_ui_State.drawZ++, 0xFFFFFF80 );
		}
		if( areaBox.hx < clipBox->hx ) {
			CBox rightUnusable = { areaBox.hx, areaBox.ly, clipBox->hx, areaBox.hy };
			display_sprite_box( whiteTex, &rightUnusable, g_ui_State.drawZ++, 0xFFFFFF80 );
		}
	}

	if (backdrop_widget->minimap)
	{
		F32 minimap_scale = scale * doc->layout_grid_size / doc->layout_kit_spacing;
		UI_WIDGET(backdrop_widget->minimap)->drawF(backdrop_widget->minimap, x, y, scroll_size[0]*scale, scroll_size[1]*scale, minimap_scale);
	}

	doc->frame_z = g_ui_State.drawZ;
	g_ui_State.drawZ += 10;

	if( doc->drag_state && (doc->drag_state->type == UGC_DRAG_MOVE_COMPONENT
							|| doc->drag_state->type == UGC_DRAG_ROTATE_COMPONENT)) {
		UGCComponent *component = ugcLayoutGetComponentByID(doc->drag_state->primary_widget->uComponentID);
		if (component) {
			ugcMapUIComponentDrawDragging(doc, doc->drag_state->primary_widget, component, scale);
			if( component == &g_TemporaryComponent ) {
				UGCUIMapEditorComponent* temporaryUIComponent = doc->drag_state->primary_widget;

				UGCProjectData tempProj = { 0 };

				// Elaborate way to make sure just the children are in the temp project.
				{
					UGCComponent* tempComponent;
					
					// 0. Make sure there's enough of a project to have components
					eaPush( &tempProj.maps, StructClone( parse_UGCMap, doc->map_data ));
					tempProj.mission = StructCreate( parse_UGCMission );
					tempProj.components = StructCreate( parse_UGCComponentList );
					tempProj.components->stComponentsById = stashTableCreateInt( 50 );
					eaIndexedEnable( &tempProj.components->eaComponents, parse_UGCComponent );
					
					// 1. Put the component and its children in the temp project
					tempComponent = ugcComponentOpClone( &tempProj, component );
					ugcComponentCreateChildrenForInitialDrag( &tempProj, tempComponent );

					// 2. Remove the link between the children and the parent
					ea32Clear( &tempComponent->uChildIDs );

					// 3. Remove the component.  Now that there is no
					// link to the children this will not remove the
					// children as well.
					ugcComponentOpDelete( &tempProj, tempComponent, true );
				}

				{
					int it;
					for( it = 0; it != eaSize( &tempProj.components->eaComponents ); ++it ) {
						UGCComponent* child = tempProj.components->eaComponents[ it ];
						Vec3 oldDragPosition;
						float oldDragRotation;

						copyVec3( temporaryUIComponent->drag_position.position, oldDragPosition );
						oldDragRotation = temporaryUIComponent->drag_position.rotation;
						
						addVec3( temporaryUIComponent->drag_position.position, child->sPlacement.vPos, temporaryUIComponent->drag_position.position );
						temporaryUIComponent->drag_position.rotation = child->sPlacement.vRotPYR[1];
						ugcMapUIComponentDrawDragging( doc, temporaryUIComponent, child, scale );
						
						copyVec3( oldDragPosition, temporaryUIComponent->drag_position.position );
						temporaryUIComponent->drag_position.rotation = oldDragRotation;
					}
				}
				StructReset( parse_UGCProjectData, &tempProj );
			}
		}
		FOR_EACH_IN_EARRAY(doc->drag_state->secondary_widgets, UGCUIMapEditorComponent, secondary_widget)
		{
			component = ugcLayoutGetComponentByID(secondary_widget->uComponentID);
			if (component)
				ugcMapUIComponentDrawDragging(doc, secondary_widget, component, scale);
		}
		FOR_EACH_END;
		FOR_EACH_IN_EARRAY(doc->drag_state->tethered_widgets, UGCUIMapEditorComponent, tethered_widget)
		{
			component = ugcLayoutGetComponentByID(tethered_widget->uComponentID);
			if (component)
				ugcMapUIComponentDrawDragging(doc, tethered_widget, component, scale);
		}
		FOR_EACH_END;
	} else if (doc->drag_state && doc->drag_state->type == UGC_DRAG_MARQUEE_SELECT) {
		Vec2 initialPos;
		Vec2 curPos = { g_ui_State.mouseX, g_ui_State.mouseY };
		CBox cbox;
		Vec3 worldPos;
		setVec3( worldPos, doc->drag_state->click_world_xz[0], 0, doc->drag_state->click_world_xz[1] );
		ugcLayoutGetUICoords( doc, worldPos, initialPos );
		initialPos[0] = x + initialPos[0] * scale;
		initialPos[1] = y + initialPos[1] * scale;
		
		CBoxSet( &cbox,
				 MIN( initialPos[0], curPos[0] ), MIN( initialPos[1], curPos[1] ),
				 MAX( initialPos[0], curPos[0] ), MAX( initialPos[1], curPos[1] ));
		ui_TextureAssemblyDraw( RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Selection_Rectangular" ), &cbox, NULL, 1, UI_TOP_Z, UI_TOP_Z + 0.1, 255, NULL );
	}

	if( doc->drag_state && doc->drag_state->type == UGC_DRAG_MOVE_COMPONENT && ugcMapEditorGetTranslateSnapEnabled() ) {
		UGCComponent *component = ugcLayoutGetComponentByID( doc->drag_state->primary_widget->uComponentID );
		if( !component ) {
			ugcMapUIStopDrag( doc, true );
		} else if( component->eType != UGC_COMPONENT_TYPE_ROOM && component->eType != UGC_COMPONENT_TYPE_ROOM_DOOR
				   && component->eType != UGC_COMPONENT_TYPE_FAKE_DOOR ) {
			ugcLayoutDrawTranslateGrid( doc, x, y );
		}
	}
}

static void ui_LayoutBackdropFreeInternal(UGCUIMapEditorBackdrop *backdrop_widget)
{
	if( backdrop_widget->minimap ) {
		backdrop_widget->minimap->widget.freeF(UI_WIDGET(backdrop_widget->minimap));
	}
	ugcMapEditorWidgetFreeInternal(UGC_UI_MAP_EDITOR_WIDGET(backdrop_widget));
}

static UGCUIMapEditorBackdrop* ugcLayoutUIBackdropUpdateUI(UGCMapEditorDoc *doc, bool is_static)
{
	UGCUIMapEditorBackdrop* ret = NULL;
	UGCMapType type = ugcMapGetType(doc->map_data);

	if( is_static ) {
		ret = calloc(1, sizeof(UGCUIMapEditorBackdrop));
		UGC_UI_MAP_EDITOR_WIDGET(ret)->tickF = ugcLayoutBackdropTick;
		UGC_UI_MAP_EDITOR_WIDGET(ret)->drawF = ugcLayoutBackdropDraw;
		UGC_UI_MAP_EDITOR_WIDGET(ret)->freeF = ui_LayoutBackdropFreeInternal;
	} else {
		if( !doc->backdrop_widget ) {
			doc->backdrop_widget = calloc(1, sizeof(UGCUIMapEditorBackdrop));
			UGC_UI_MAP_EDITOR_WIDGET(doc->backdrop_widget)->tickF = ugcLayoutBackdropTick;
			UGC_UI_MAP_EDITOR_WIDGET(doc->backdrop_widget)->drawF = ugcLayoutBackdropDraw;
			UGC_UI_MAP_EDITOR_WIDGET(doc->backdrop_widget)->freeF = ui_LayoutBackdropFreeInternal;
		}
		ret = doc->backdrop_widget;
	}

	if (doc->map_data->pPrefab && !doc->map_data->pPrefab->customizable)
	{
		ZoneMapEncounterRegionInfo *zeni_region = ugcGetZoneMapDefaultRegion(doc->map_data->pPrefab->map_name);

		if (zeni_region)
		{
			UIMinimap *minimap = ret->minimap;
			ZoneMapEncounterRoomInfo* playable_volume = ugcGetZoneMapPlayableVolume(doc->map_data->pPrefab->map_name);
			ZoneMapExternalMapSnap* map_snap = RefSystem_ReferentFromString( "ZoneMapExternalMapSnap", doc->map_data->pPrefab->map_name );
			if( !minimap ) { 
				minimap = ui_MinimapCreate();
				ret->minimap = minimap;
			}
			
			ui_WidgetSetPositionEx( UI_WIDGET( minimap ), 0, 0, 0, 0, UITopLeft );
			minimap->autosize = true;

			{
				Vec3 restrictMin;
				Vec3 restrictMax;
				ugcGetZoneMapPlaceableBounds( restrictMin, restrictMax, doc->map_data->pPrefab->map_name, false );
				ui_MinimapSetMapAndRestrictToRegion( minimap, doc->map_data->pPrefab->map_name, zeni_region->regionName, restrictMin, restrictMax );
			}

			if( playable_volume ) {
				ui_MinimapSetMapHighlightArea( minimap, playable_volume->min, playable_volume->max );
			} else {
				ui_MinimapSetMapHighlightArea( minimap, NULL, NULL );
			}
		}
		else
		{
			ui_WidgetQueueFreeAndNull( &ret->minimap );
		}
	}
	else
	{
		ui_WidgetQueueFreeAndNull( &ret->minimap );
	}
	ret->doc = doc;
	ret->is_static = is_static;
	return ret;
}

////////////////////////////////////////////////////////////////
// Main map view widget
////////////////////////////////////////////////////////////////

static void ugcMapUIMapViewSize(UIScrollArea* map_widget, Vec2 outputSize, UGCMapEditorDoc *doc)
{
	ugcLayoutGetUISize( doc, outputSize );
	outputSize[0] *= map_widget->childScale;
	outputSize[1] *= map_widget->childScale;
}

UIScrollArea *ugcMapUIMapViewCreate(UGCMapEditorDoc *doc)
{
	UIScrollArea* area = ui_ScrollAreaCreate( 0, 0, 0, 0, 0, 0, 1, 1 );
	UI_WIDGET(area)->sb->scrollBoundsX = UIScrollBounds_KeepContentsAtViewCenter;
	UI_WIDGET(area)->sb->scrollBoundsY = UIScrollBounds_KeepContentsAtViewCenter;
	doc->layout_scale = area->childScale;
	area->sizeF = ugcMapUIMapViewSize;
	area->sizeData = doc;

	return area;
}

////////////////////////////////////////////////////////////////
// Main UI
////////////////////////////////////////////////////////////////

bool ugcLayoutCanCreateComponent(const char *map_name, UGCComponentType type)
{
	int component_count = 0;
	UGCComponentList *list = ugcEditorGetComponentList();
	UGCProjectBudget *budget;

	// Can we create another component of this type?
	FOR_EACH_IN_EARRAY(list->eaComponents, UGCComponent, other_component)
	{
		if (ugcComponentIsOnMap(other_component, map_name, false) && other_component->eType == type)
		{
			component_count++;
		}
	}
	FOR_EACH_END;
	budget = ugcFindBudget(UGC_BUDGET_TYPE_COMPONENT, type);
	if (budget && component_count > budget->iHardLimit)
	{
		return false;
	}
	return true;
}

static UGCComponent *ugcLayoutCreateTemporaryComponent(UGCAssetLibraryPane *lib_pane, UGCMapEditorDoc *doc, UGCAssetLibraryRow *row)
{
	UGCComponentType type;
	UGCComponent *component;
	char row_name[ 256 ] = "";

	if( !ugcAssetLibraryRowFillComponentTypeAndName( lib_pane, row, &type, SAFESTR( row_name ))) {
		return NULL;
	}
	if (!ugcLayoutCanCreateComponent(doc->map_data->pcName, type))
		return NULL;
	
	ugcMapUICancelAction(doc);

	component = &g_TemporaryComponent;

	// NOTE: This code is duplicated in ugcPlayingEditorStartDragNewEditComponent().
	// It should get combined in the future.
	StructReset(parse_UGCComponent, component);
	component->eType = type;
	ugcComponentOpReset(ugcEditorGetProjectData(), component, type, false);
	ugcComponentOpSetPlacement(ugcEditorGetProjectData(), component, NULL, GENESIS_UNPLACED_ID);
	switch (type)
	{
		case UGC_COMPONENT_TYPE_PLANET:
		case UGC_COMPONENT_TYPE_KILL:
		case UGC_COMPONENT_TYPE_OBJECT:
		case UGC_COMPONENT_TYPE_DESTRUCTIBLE:
		case UGC_COMPONENT_TYPE_ROOM:
		case UGC_COMPONENT_TYPE_ROOM_DOOR:
		case UGC_COMPONENT_TYPE_FAKE_DOOR:
		case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
		case UGC_COMPONENT_TYPE_TELEPORTER:
		case UGC_COMPONENT_TYPE_CLUSTER:
		case UGC_COMPONENT_TYPE_COMBAT_JOB:
			component->iObjectLibraryId = atoi( row_name );
			break;
		case UGC_COMPONENT_TYPE_TRAP: {
			int objlibID;
			char powerName[ 256 ];
			sscanf_s( row_name, "%d,%s", &objlibID, SAFESTR( powerName ));
			component->iObjectLibraryId = objlibID;
			StructCopyString( &component->pcTrapPower, powerName );
		}
			break;
		case UGC_COMPONENT_TYPE_CONTACT:
			StructCopyString(&component->pcCostumeName, row_name);
			break;
		case UGC_COMPONENT_TYPE_SOUND:
			component->strSoundEvent = allocAddString(row_name);
			break;
	}

	return component;
}

static void ugcMapUIStartComponentDrag(UGCAssetLibraryPane *lib_pane, UGCMapEditorDoc *doc, UGCAssetLibraryRow *row)
{
	UGCComponent *component = ugcLayoutCreateTemporaryComponent(lib_pane, doc, row);
	if (component)
	{
		// ugcLayoutCreateTemporaryComponent may grab the temporaryComponent which already has its room set
		ugcComponentOpSetPlacement(ugcEditorGetProjectData(), component, doc->map_data, component->sPlacement.uRoomID);
		ugcLayoutStartPlaceNewComponent(doc, component);
	}
	else
	{
		ugcModalDialogMsg( "UGC_MapEditor.Error_TooManyComponents", "UGC_MapEditor.Error_TooManyComponentsDetails", UIOk );
	}
}

void ugcLayoutGetDefaultPlacement(UGCMapEditorDoc *doc, U32 *room_id, Vec3 pos)
{
	// Tricky part - figure out placement
	Vec2 screen_pos, ui_pos;
	Vec3 world_pos = { 0, 0, 0 };
	*room_id = UGC_TOPLEVEL_ROOM_ID;

	if (ugcEditorGetContextMenuPosition(&screen_pos[0], &screen_pos[1]))
	{
		screen_pos[0] -= doc->layout_last_box.left;
		screen_pos[1] -= doc->layout_last_box.top+24; // Allowance for title bar
	}
	else
	{
		setVec2(screen_pos, (doc->layout_last_box.right-doc->layout_last_box.left)*0.5f,
				(doc->layout_last_box.bottom-doc->layout_last_box.top)*0.5f);
	}

	setVec2(ui_pos, (screen_pos[0]+UI_WIDGET(doc->map_widget)->sb->xpos) / doc->layout_scale,
			(screen_pos[1]+UI_WIDGET(doc->map_widget)->sb->ypos) / doc->layout_scale);

	ugcLayoutGetWorldCoords(doc, ui_pos, world_pos);
	pos[0] = CLAMP(world_pos[0], doc->layout_min_pos[0], doc->layout_max_pos[0]);
	pos[1] = 0;
	pos[2] = CLAMP(world_pos[2], doc->layout_min_pos[1], doc->layout_max_pos[1]);
}

void ugcLayoutUpdateFlowchart(UGCMapEditorDoc *doc, UGCUIMapEditorWidgetContainer *parent, bool is_static)
{
	UGCUIMapEditorComponent **all_components = NULL;

	{
		UGCComponent **component_array= ugcComponentFindPlacements(ugcEditorGetComponentList(), doc->map_data->pcName, UGC_TOPLEVEL_ROOM_ID);
		FOR_EACH_IN_EARRAY(component_array, UGCComponent, component)
		{
			UGCUIMapEditorComponent *new_component = is_static ? NULL : ugcLayoutUIGetComponent(doc, component);
			if (!new_component)
			{
				new_component = ugcLayoutUIComponentCreate(doc, component, is_static);
				printf("CREATING COMPONENT %d (%X)\n", component->uID, (intptr_t)new_component);
			}
			else
			{
				ugcLayoutUIComponentUpdate(new_component);
				//printf("RETAINING COMPONENT %d (%X)\n", component->uID, (intptr_t)new_component);
			}
			eaPush(&all_components, new_component);
			ugcMapEditorWidgetContainerAddChild(parent, UGC_UI_MAP_EDITOR_WIDGET(new_component));
		}
		FOR_EACH_END;
		eaDestroy(&component_array);
	}

	if (!is_static)
	{
		FOR_EACH_IN_EARRAY(doc->component_widgets, UGCUIMapEditorComponent, component_widget)
		{
			if( !doc->drag_state && eaFind(&all_components, component_widget) == -1) {
				printf("DELETING COMPONENT WIDGET (%X)\n", (intptr_t)component_widget);
				ugcMapEditorClearObjectSelection(doc, component_widget->uComponentID, component_widget->uComponentLogicalParent);
				eaRemove(&doc->component_widgets, FOR_EACH_IDX(doc->component_widgets, component_widget));
				ugcMapEditorWidgetQueueFree(UGC_UI_MAP_EDITOR_WIDGET(component_widget));
			}
		}
		FOR_EACH_END;

		// Refresh objective waypoints
		if( ugcMapEditorGetViewWaypoints() ) {
			UGCProjectData* ugcProj = ugcEditorGetProjectData();
			UGCMissionObjective** mapObjectives = NULL;
			int uiIt = 0;
			int it;
			
			ugcMissionTransmogrifyObjectives( ugcProj, ugcProj->mission->objectives, doc->map_data->pcName, true, &mapObjectives );

			for( it = 0; it != eaSize( &mapObjectives ); ++it ) {
				ugcLayoutUIObjectiveWaypointRefresh( doc, parent, &uiIt, mapObjectives[ it ]);
			}
			while( uiIt < eaSize( &doc->objectiveWaypointWidgets )) {
				ugcMapEditorWidgetQueueFree( UGC_UI_MAP_EDITOR_WIDGET(doc->objectiveWaypointWidgets[ uiIt ]));
				eaRemove( &doc->objectiveWaypointWidgets, uiIt );
			}

			eaDestroyStruct( &mapObjectives, parse_UGCMissionObjective );
		} else {
			eaDestroyEx( &doc->objectiveWaypointWidgets, ugcMapEditorWidgetQueueFree );
		}
	}
	eaDestroy(&all_components);
}

UGCUIMapEditorWidgetContainer* ugcLayoutGenerateStaticUI(UGCMapEditorDoc *doc)
{
	UGCUIMapEditorWidgetContainer* container = ugcMapEditorWidgetContainerCreate();
	ugcMapEditorWidgetContainerAddChild( container, UGC_UI_MAP_EDITOR_WIDGET( ugcLayoutUIBackdropUpdateUI(doc, true)));
	ugcLayoutUpdateFlowchart(doc, container, true);

	ui_WidgetSetDimensions(UI_WIDGET(container),
		(doc->layout_max_pos[0]-doc->layout_min_pos[0])*doc->layout_grid_size/doc->layout_kit_spacing,
		(doc->layout_max_pos[1]-doc->layout_min_pos[1])*doc->layout_grid_size/doc->layout_kit_spacing);

	return container;
}

void ugcMapEditorUpdateZOrder(UGCMapEditorDoc *doc)
{
	FOR_EACH_IN_EARRAY( doc->component_widgets, UGCUIMapEditorComponent, widget ) {
		UGCComponent* component = ugcLayoutGetComponentByID( widget->uComponentID );
		UGC_UI_MAP_EDITOR_WIDGET( widget )->priority = component->sPlacement.eZOrderSort;
	} FOR_EACH_END;

	if( doc->map_widget_container ) {
		ugcMapEditorWidgetContainerSort( doc->map_widget_container );
	}
}

void ugcMapEditorUpdateSnap(UGCMapEditorDoc *doc)
{
	F32 snap;
	if( doc->snap_buttons[ 0 ])
	{
		if (ugcMapEditorGetTranslateSnapEnabled())
		{
			ui_ButtonSetImage(doc->snap_buttons[0], "ugc_trans_snap");
		}
		else
		{
			ui_ButtonSetImage(doc->snap_buttons[0], "ugc_trans_snap_dis");
		}
		ui_ButtonSetImageStretch(doc->snap_buttons[0], true);
	}

	if( doc->snap_buttons[ 1 ])
	{
		if (ugcMapEditorGetRotateSnap(UGC_COMPONENT_TYPE_OBJECT, &snap))
		{
			ui_ButtonSetImage(doc->snap_buttons[1], "ugc_rot_snap");
		}
		else
		{
			ui_ButtonSetImage(doc->snap_buttons[1], "ugc_rot_snap_dis");
		}
		ui_ButtonSetImageStretch(doc->snap_buttons[1], true);
	}
}

static void ugcMapEditorDoToggleTranslateSnap(UIButton *button, UGCMapEditorDoc *doc)
{
	ugcMapEditorToggleTranslateSnap();
	ugcMapEditorUpdateSnap(doc);
}

static void ugcMapEditorDoToggleRotateSnap(UIButton *button, UGCMapEditorDoc *doc)
{
	ugcMapEditorToggleRotateSnap();
	ugcMapEditorUpdateSnap(doc);
}

static void ugcMapEditorDoToggleViewWaypoints( UIButton* ignored, UGCMapEditorDoc* doc )
{
	ugcMapEditorToggleViewWaypoints();
	ugcEditorQueueUIUpdate();
}
 
static void ugcMapEditorFinishCreateMapCB( UIButton* ignored, UGCMap* map )
{
	ugcEditorFinishCreateMap( map, false );
}

static int ugcSortComponentTypeByDisplayName( const UGCComponentType* type1, const UGCComponentType* type2 )
{
	const char* displayName1 = ugcComponentTypeGetDisplayName( *type1, true );
	const char* displayName2 = ugcComponentTypeGetDisplayName( *type2, true );

	return stricmp( displayName1, displayName2 );
}

void ugcLayoutUpdateUISingleBudgetLine( UGCMapEditorDoc* doc, UGCComponentType type )
{
	const char* typeName = StaticDefineIntRevLookup( UGCComponentTypeEnum, type );
	UGCProjectBudget* typeBudget = ugcFindBudget( UGC_BUDGET_TYPE_COMPONENT, type );
	MEFieldContextEntry* entry;
	UIWidget* widget;
	char buffer[ 256 ];
	float labelX;
	float y = MEContextGetCurrent()->iYPos;

	MEContextPush( typeName, NULL, NULL, NULL );

	if( doc->map_data->cacheComponentCount[ type ] > SAFE_MEMBER( typeBudget, iSoftLimit )) {
		entry = MEContextAddSprite( "ugc_icons_labels_alert", "Error", NULL, NULL );
		widget = UI_WIDGET( ENTRY_SPRITE( entry ));
		ui_WidgetSetPositionEx( widget, 0, y, 0, 0, UITopLeft );
		labelX = ui_WidgetGetNextX( widget ) + 2; 
	} else {
		labelX = 0;
	}

	sprintf( buffer, "%s ", ugcComponentTypeGetDisplayName( type, true ));
	entry = MEContextAddLabel( "Label", buffer, NULL );
	widget = UI_WIDGET( ENTRY_LABEL( entry ));
	ui_LabelResize( ENTRY_LABEL( entry ));
	ui_WidgetSetPositionEx( widget, labelX, y + 1, 0, 0, UITopLeft );

	sprintf( buffer, "%d / ", doc->map_data->cacheComponentCount[ type ]);
	entry = MEContextAddLabel( "Count", buffer, NULL );
	widget = UI_WIDGET( ENTRY_LABEL( entry ));
	ui_LabelResize( ENTRY_LABEL( entry ));
	ui_WidgetSetPositionEx( widget, 30, y + 1, 0, 0, UITopRight );

	sprintf( buffer, "%d ", SAFE_MEMBER( typeBudget, iSoftLimit ));
	entry = MEContextAddLabel( "Budget", buffer, NULL );
	widget = UI_WIDGET( ENTRY_LABEL( entry ));
	ui_WidgetSetPositionEx( widget, 0, y + 1, 0, 0, UITopRight );
	ui_WidgetSetWidth( widget, 30 );

	MEContextGetCurrent()->iYPos = ui_WidgetGetNextY( widget );

	MEContextPop( typeName );
}

void ugcLayoutUpdateUI(UGCMapEditorDoc *doc)
{
	UGCMapType mapType = ugcMapGetType( doc->map_data );
	bool noMinimapBefore;

	ugcLayoutCommonInitSprites();

	// Header pane
	{
		if (!doc->header_pane)
		{
			doc->header_pane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
		}
		ui_WidgetSetPosition( UI_WIDGET( doc->header_pane ), 0, 0 );
		ui_WidgetSetDimensionsEx( UI_WIDGET( doc->header_pane ), 1, UGC_PANE_TOP_BORDER, UIUnitPercentage, UIUnitFixed );
		ui_WidgetGroupMove( &UI_WIDGET( doc->pRootPane )->children, UI_WIDGET( doc->header_pane ));

		MEExpanderRefreshButton( &doc->map_name_button, ugcMapGetDisplayName( ugcEditorGetProjectData(), doc->map_data->pcName ), ugcEditorPopupChooserMapsCB, NULL, 0, 0, 0, 1, UIUnitFixed, 0, UI_WIDGET( doc->header_pane ));
		SET_HANDLE_FROM_STRING( "UISkin", "UGCComboButton", UI_WIDGET( doc->map_name_button )->hOverrideSkin );
		ui_WidgetSetPosition( UI_WIDGET( doc->map_name_button ), 10, 6 );
		ui_ButtonResize( doc->map_name_button );
		MIN1( UI_WIDGET( doc->map_name_button )->width, 250 );
		ui_WidgetSetHeight( UI_WIDGET( doc->map_name_button ), UGC_ROW_HEIGHT*1.5-12 );

		if( !doc->edit_map_name_button ) {
			doc->edit_map_name_button = ugcEditorButtonCreate( UGC_ACTION_MAP_EDIT_NAME, false, false );
			ui_PaneAddChild( doc->header_pane, doc->edit_map_name_button );
		}
		ui_WidgetSetPosition( UI_WIDGET( doc->edit_map_name_button ), ui_WidgetGetNextX( UI_WIDGET( doc->map_name_button )), 6 );
		ui_WidgetSetHeight( UI_WIDGET( doc->edit_map_name_button ), UGC_ROW_HEIGHT*1.5-12 );

		ugcMapEditorGlobalPropertiesErrorButtonRefresh( doc, &doc->edit_map_name_error_sprite );
		ui_PaneAddChild( doc->header_pane, doc->edit_map_name_error_sprite );
		ui_WidgetSetPosition( UI_WIDGET( doc->edit_map_name_error_sprite ), ui_WidgetGetNextX( UI_WIDGET( doc->edit_map_name_button )), 11 );

		if( !doc->map_data->pUnitializedMap )
		{
			float x = 4;
			float y = 6;

			if (!doc->delete_button)
			{
				doc->delete_button = ugcEditorButtonCreate(UGC_ACTION_MAP_DELETE, true, true);
				ui_PaneAddChild( doc->header_pane, doc->delete_button );
			}
			ui_WidgetSetPositionEx(UI_WIDGET(doc->delete_button), x, y, 0, 0, UITopRight);
			ui_WidgetSetDimensions(UI_WIDGET(doc->delete_button), UGC_ROW_HEIGHT*1.5-12, UGC_ROW_HEIGHT*1.5-12);
			x = elUINextX( doc->delete_button ) + 5;

			if (!doc->duplicate_button)
			{
				doc->duplicate_button = ugcEditorButtonCreate(UGC_ACTION_MAP_DUPLICATE, true, false );
				ui_PaneAddChild( doc->header_pane, doc->duplicate_button );
			}
			ui_WidgetSetPositionEx(UI_WIDGET(doc->duplicate_button), x, y, 0, 0, UITopRight);
			ui_WidgetSetHeight(UI_WIDGET(doc->duplicate_button), UGC_ROW_HEIGHT*1.5-12);
			x = elUINextX( doc->duplicate_button ) + 5;
		
			if (!doc->play_button)
			{
				doc->play_button = ugcEditorButtonCreate(UGC_ACTION_PLAY_MAP, true, false);
				ui_PaneAddChild( doc->header_pane, doc->play_button );
			}
			ui_WidgetSetPositionEx(UI_WIDGET(doc->play_button), x, y, 0, 0, UITopRight);
			ui_WidgetSetHeight(UI_WIDGET(doc->play_button), UGC_ROW_HEIGHT*1.5-12);
			x = elUINextX( doc->play_button ) + 5;

			if (!doc->snap_buttons[1])
			{
				doc->snap_buttons[1] = ui_ButtonCreateImageOnly("ugc_rot_snap", 0, 0, ugcMapEditorDoToggleRotateSnap, doc);
				ui_PaneAddChild( doc->header_pane, doc->snap_buttons[1] );
			}
			ui_WidgetSetPositionEx(UI_WIDGET(doc->snap_buttons[1]), x, y, 0, 0, UITopRight);
			ui_WidgetSetDimensions(UI_WIDGET(doc->snap_buttons[1]), UGC_ROW_HEIGHT*1.5-12, UGC_ROW_HEIGHT*1.5-12);
			ui_WidgetSetTooltipString(UI_WIDGET(doc->snap_buttons[1]), "Enable rotation snap to angle");
			x = elUINextX( doc->snap_buttons[1] ) + 5;
		
			if (!doc->snap_buttons[0])
			{
				doc->snap_buttons[0] = ui_ButtonCreateImageOnly("ugc_trans_snap", 0, 0, ugcMapEditorDoToggleTranslateSnap, doc);
				ui_PaneAddChild( doc->header_pane, doc->snap_buttons[0] );
			}
			ui_WidgetSetPositionEx(UI_WIDGET(doc->snap_buttons[0]), x, y, 0, 0, UITopRight);
			ui_WidgetSetDimensions(UI_WIDGET(doc->snap_buttons[0]), UGC_ROW_HEIGHT*1.5-12, UGC_ROW_HEIGHT*1.5-12);
			ui_WidgetSetTooltipString(UI_WIDGET(doc->snap_buttons[0]), "Enable translation snap to grid");
			x = elUINextX( doc->snap_buttons[ 0 ]) + 5;
			
			if( !doc->viewWaypointButton ) {
				doc->viewWaypointButton = ui_ButtonCreateImageOnly( "MapIcons_AreaWaypoint", 0, 0, ugcMapEditorDoToggleViewWaypoints, doc );
				ui_PaneAddChild( doc->header_pane, doc->viewWaypointButton );
			}
			ui_WidgetSetPositionEx( UI_WIDGET( doc->viewWaypointButton ), x, y, 0, 0, UITopRight );
			ui_WidgetSetDimensions( UI_WIDGET( doc->viewWaypointButton ), UGC_ROW_HEIGHT*1.5-12, UGC_ROW_HEIGHT*1.5-12 );
			ui_WidgetSetTooltipString( UI_WIDGET( doc->viewWaypointButton ), "View mission waypoints" );
			ui_ButtonSetImage( doc->viewWaypointButton, (ugcMapEditorGetViewWaypoints() ? "MapIcons_AreaWaypoint" : "MapIcons_AreaWaypoint_Dimmed") );
			ui_ButtonSetImageStretch(doc->viewWaypointButton, true);
			x = elUINextX( doc->viewWaypointButton ) + 5;
		
			if( !doc->searchComponentsButton ) {
				doc->searchComponentsButton = ugcEditorButtonCreate( UGC_ACTION_MAP_SEARCH_COMPONENT, true, false );
				ui_PaneAddChild( doc->header_pane, doc->searchComponentsButton );
			}
			ui_WidgetSetPositionEx( UI_WIDGET( doc->searchComponentsButton ), x, y, 0, 0, UITopRight );
			ui_WidgetSetHeight( UI_WIDGET( doc->searchComponentsButton ), UGC_ROW_HEIGHT*1.5-12 );
			x = elUINextX( doc->searchComponentsButton );

			if( !doc->editBackdropButton ) {
				doc->editBackdropButton = ugcEditorButtonCreate( UGC_ACTION_MAP_EDIT_BACKDROP, false, false );
				ui_PaneAddChild( doc->header_pane, doc->editBackdropButton );
			}
			ui_WidgetSetPositionEx( UI_WIDGET( doc->editBackdropButton ), x, y, 0, 0, UITopRight );
			ui_WidgetSetHeight( UI_WIDGET( doc->editBackdropButton ), UGC_ROW_HEIGHT*1.5-12 );
			x = elUINextX( doc->editBackdropButton );

			ugcMapEditorBackdropPropertiesErrorButtonRefresh( doc, &doc->editBackdropErrorSprite );
			ui_PaneAddChild( doc->header_pane, doc->editBackdropErrorSprite );
			ui_WidgetSetPositionEx( UI_WIDGET( doc->editBackdropErrorSprite ), x, y + 5, 0, 0, UITopRight );
			x = elUINextX( doc->editBackdropErrorSprite );
		}
		else
		{
			ui_WidgetQueueFreeAndNull( &doc->delete_button );
			ui_WidgetQueueFreeAndNull( &doc->duplicate_button );
			ui_WidgetQueueFreeAndNull( &doc->play_button );
			ui_WidgetQueueFreeAndNull( &doc->snap_buttons[ 0 ]);
			ui_WidgetQueueFreeAndNull( &doc->snap_buttons[ 1 ]);
			ui_WidgetQueueFreeAndNull( &doc->viewWaypointButton );
			ui_WidgetQueueFreeAndNull( &doc->searchComponentsButton );
			ui_WidgetQueueFreeAndNull( &doc->editBackdropButton );
		}
	}

	// Content Pane
	if (!doc->layout_pane)
	{
		doc->layout_pane = ui_PaneCreate(0, 0, 1.0f, 1.f, UIUnitPercentage, UIUnitPercentage, 0);
	}
	doc->layout_pane->widget.tickF = ugcLayoutPaneTick;
	doc->layout_pane->widget.userinfo = doc;
	ui_WidgetSetPosition( UI_WIDGET( doc->layout_pane ), 0, 0 );
	ui_WidgetSetDimensionsEx( UI_WIDGET( doc->layout_pane ), 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( UI_WIDGET( doc->layout_pane ), 0, UGC_LIBRARY_PANE_WIDTH, UGC_PANE_TOP_BORDER, 0 );
	ui_WidgetGroupMove( &UI_WIDGET( doc->pRootPane )->children, UI_WIDGET( doc->layout_pane ));

	if( !doc->map_data->pUnitializedMap )
	{
		if (!doc->map_widget)
		{
			doc->map_widget = ugcMapUIMapViewCreate(doc);
			UI_WIDGET(doc->map_widget)->sb->disableScrollWheel = true;
			ui_ScrollAreaSetNoCtrlDraggable(doc->map_widget, true);
			ui_ScrollAreaSetZoomSlider(doc->map_widget, true);
			ui_ScrollAreaSetAutoZoomRate(doc->map_widget, 1.03f);
			doc->map_widget->enableAutoEdgePan = true;
			doc->map_widget->maxZoomScale = 16;
			ui_WidgetSetDimensionsEx(UI_WIDGET(doc->map_widget), 1.0f, 1.0f, UIUnitPercentage, UIUnitPercentage);
			ui_WidgetSetPositionEx(UI_WIDGET(doc->map_widget), 0, 0, 0, 0, UITopLeft);
			ui_PaneAddChild(doc->layout_pane, doc->map_widget);
		}

		if(!doc->map_widget_container)
		{
			doc->map_widget_container = ugcMapEditorWidgetContainerCreate();
			doc->map_widget_container->doc = doc;
			ui_WidgetSetDimensions(UI_WIDGET(doc->map_widget_container),
								   (doc->layout_max_pos[0]-doc->layout_min_pos[0])*doc->layout_grid_size/doc->layout_kit_spacing,
								   (doc->layout_max_pos[1]-doc->layout_min_pos[1])*doc->layout_grid_size/doc->layout_kit_spacing);
			ui_ScrollAreaAddChild( doc->map_widget, doc->map_widget_container );
		}
	}
	else
	{
		ugcMapEditorWidgetRemoveFromGroup( UGC_UI_MAP_EDITOR_WIDGET( doc->backdrop_widget ));
		FOR_EACH_IN_EARRAY( doc->component_widgets, UGCUIMapEditorComponent, component_widget ) {
			ugcMapEditorWidgetRemoveFromGroup( UGC_UI_MAP_EDITOR_WIDGET( component_widget ));
		} FOR_EACH_END;
		eaDestroyEx( &doc->objectiveWaypointWidgets, ugcMapEditorWidgetQueueFree );
		ui_WidgetForceQueueFree( UI_WIDGET( doc->map_widget_container ));
		doc->map_widget_container = NULL;
		ui_WidgetForceQueueFree( UI_WIDGET( doc->map_widget ));
		doc->map_widget = NULL;
	}
	if (doc->map_data->pUnitializedMap ) {
		char buffer[ 512 ];
		UGCMissionMapLink* link = ugcMissionFindLinkByMap( ugcEditorGetProjectData(), doc->map_data->pcName );

		if( ugcDefaultsSingleMissionEnabled() ) {
			sprintf( buffer, "This map is currently used in the story, but it is only partially created." );
		} else {
			sprintf( buffer, "This map is currently used in the story for mission \"%s\", but it is only partially created.",
					 ugcMapMissionLinkName( ugcEditorGetProjectData(), link ));
		}
		MEExpanderRefreshLabel( &doc->unplaced_description, buffer, NULL,
								0, 0, 40, UI_WIDGET( doc->layout_pane ));
		ui_LabelSetWidthNoAutosize( doc->unplaced_description, 1, UIUnitPercentage );
		ui_LabelSetWordWrap( doc->unplaced_description, true );

		if( !doc->unplaced_create_button ) {
			doc->unplaced_create_button = ui_ButtonCreate( "Finish Creation", 0, 0, ugcMapEditorFinishCreateMapCB, doc->map_data );
			ui_PaneAddChild( doc->layout_pane, doc->unplaced_create_button );
		}
		ui_WidgetSetPositionEx( UI_WIDGET( doc->unplaced_create_button ), 0, 0, 0, 0, 0 );
		ui_WidgetSetDimensions( UI_WIDGET( doc->unplaced_create_button ), 250, 64 );
	} else {
		ui_WidgetQueueFreeAndNull( &doc->unplaced_description );
		ui_WidgetQueueFreeAndNull( &doc->unplaced_create_button );
	}

	if (!doc->trash_button)
	{
		doc->trash_button = ui_ButtonCreateImageOnly( "white", 10, 70, NULL, NULL );
		doc->trash_button->widget.priority = 10;
		ui_WidgetSetDimensions(UI_WIDGET(doc->trash_button), 80, 80);
	}

	noMinimapBefore = (SAFE_MEMBER( doc->backdrop_widget, minimap ) == NULL);
	ugcLayoutUIBackdropUpdateUI(doc, false);

	if( doc->map_widget_container )
	{
		FOR_EACH_IN_EARRAY(doc->map_widget_container->eaChildren, UGCUIMapEditorWidget, child)
		{
			ugcMapEditorWidgetRemoveFromGroup(child);
		}
		FOR_EACH_END;
		ugcMapEditorWidgetContainerAddChild( doc->map_widget_container, UGC_UI_MAP_EDITOR_WIDGET( doc->backdrop_widget ));
	}

	if( doc->backdrop_widget->minimap && noMinimapBefore ) {
		Vec2 uiSize;
		ugcLayoutGetUISize( doc, uiSize );
		ui_ScrollAreaScrollToPosition( doc->map_widget, uiSize[ 0 ] / 2, uiSize[ 1 ] / 2 );
		doc->map_widget->autoScrollCenter = true;
		doc->map_widget->scrollToTargetRemaining = doc->map_widget->childScale = 1.0f / 9000; //< It's (one) over NINE THOUSAND!
	}

	// In-backdrop widgets
	{
		char strContextName[ 256 ];
		sprintf( strContextName, "UGCMapEditor_%s_InBackdrop", doc->doc_name );
		if( doc->map_widget ) {
			MEFieldContext* budgetsCtx;
			char* estr = NULL;
			MEFieldContextEntry* entry;
			UIWidget* widget;
			UIPane* pane;
			UIScrollArea* scrollarea;

			MEContextPush( strContextName, NULL, NULL, NULL );
			MEContextSetParent( UI_WIDGET( doc->map_widget ));
			MEContextGetCurrent()->astrOverrideSkinName = allocAddString( "UGCTab_Picker" );

			entry = ugcMEContextAddEditorButton( UGC_ACTION_MAP_SET_DETAIL_MODE, false, false );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			widget->bNoScrollX = true;
			widget->bNoScrollY = true;
			widget->bNoScrollScale = true;
			ui_WidgetSetPosition( widget, 160, 10 );
			ui_WidgetSetWidth( widget, 100 );
			if( doc->mode == UGC_MAP_EDITOR_DETAIL ) {
				SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Picker_Active", widget->hOverrideSkin );
			}
		
			entry = ugcMEContextAddEditorButton( UGC_ACTION_MAP_SET_LAYOUT_MODE, false, false  );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			widget->bNoScrollX = true;
			widget->bNoScrollY = true;
			widget->bNoScrollScale = true;
			ui_WidgetSetPosition( widget, 260, 10 );
			ui_WidgetSetWidth( widget, 100 );
			if( doc->mode == UGC_MAP_EDITOR_LAYOUT ) {
				SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Picker_Active", widget->hOverrideSkin );
			}

			budgetsCtx = MEContextPush( "Budgets", NULL, NULL, NULL );
			budgetsCtx->astrOverrideSkinName = "UGCMapEditor_Budgets";
			{
				entry = MEContextAddLabelMsg( "Header", "UGC_MapEditor.Budgets", NULL );
				widget = UI_WIDGET( ENTRY_LABEL( entry ));
				widget->bNoScrollX = true;
				widget->bNoScrollY = true;
				widget->bNoScrollScale = true;
				ENTRY_LABEL( entry )->textFrom = UITop;
				ui_WidgetSetPositionEx( widget, 10, 110, 0, 0, UIBottomLeft );
				ui_WidgetSetWidth( widget, 200 );

				pane = MEContextPushPaneParent( "Pane" );
				scrollarea = MEContextPushScrollAreaParent( "Scroll" );
				ui_WidgetSetPositionEx( UI_WIDGET( pane ), 10, 10, 0, 0, UIBottomLeft );
				ui_WidgetSetDimensions( UI_WIDGET( pane ), 200, 100 );
				ui_WidgetSetPosition( UI_WIDGET( scrollarea ), 0, 0 );
				ui_WidgetSetDimensionsEx( UI_WIDGET( scrollarea ), 1, 1, UIUnitPercentage, UIUnitPercentage );
				UI_WIDGET( pane )->bNoScrollX = true;
				UI_WIDGET( pane )->bNoScrollY = true;
				UI_WIDGET( pane )->bNoScrollScale = true;
				scrollarea->autosize = true;
				{
					int it;
					for( it = 0; it != ARRAY_SIZE( g_budgetsPriorityDisplay ); ++it ) {
						ugcLayoutUpdateUISingleBudgetLine( doc, g_budgetsPriorityDisplay[ it ]);
					}
					MEContextStepDown();

					qsort( g_budgetsToSortDisplay, ARRAY_SIZE( g_budgetsToSortDisplay ), sizeof( *g_budgetsToSortDisplay ), ugcSortComponentTypeByDisplayName );
					for( it = 0; it != ARRAY_SIZE( g_budgetsToSortDisplay ); ++it ) {
						ugcLayoutUpdateUISingleBudgetLine( doc, g_budgetsToSortDisplay[ it ]);
					}
				}
				MEContextPop( "Scroll" );
				MEContextPop( "Pane" );
			}
			MEContextPop( "Budgets" );

			MEContextPop( strContextName );
			estrDestroy( &estr );
		} else {
			MEContextDestroyByName( strContextName );
		}
	}

	// Library pane
	if (!doc->library_pane)
	{
		doc->library_pane = ui_PaneCreate(0, 0, 1.0f, 1.f, UIUnitPercentage, UIUnitPercentage, 0);
	}
	ui_WidgetSetPositionEx( UI_WIDGET( doc->library_pane ), 0, 0, 0, 0, UITopRight );
	// pane is sized+padded below, after unplaced list gets refreshed
	ui_WidgetGroupMove( &UI_WIDGET( doc->pRootPane )->children, UI_WIDGET( doc->library_pane ));
	ugcMapEditorUpdateAssetLibrary( doc );

	// Unplaced pane
	if (!doc->unplaced_pane)
	{
		doc->unplaced_pane = ui_PaneCreate( 0, 0, 1, 1, UIUnitPercentage, UIUnitPercentage, 0 );
	}
	ui_WidgetSetPositionEx( UI_WIDGET( doc->unplaced_pane ), 0, 0, 0, 0, UIBottomRight );
	// pane is sized+padded below, after unplaced list gets refreshed
	ui_WidgetGroupMove( &UI_WIDGET( doc->pRootPane )->children, UI_WIDGET( doc->unplaced_pane ));
	ugcMapEditorUpdateUnplaced( doc );

	// Resize
	{
		float unplacedComponentsPaneSize = CLAMP( ugcUnplacedListGetContentHeight( doc->unplaced_list )
												  + 8			//< list assembly padding
												  + 10 + 10		//< list top and bottom padding
												  + 24,			//< title height
												  UGC_UNPLACED_PANE_HEIGHT_MIN, UGC_UNPLACED_PANE_HEIGHT_MAX );
		ui_WidgetSetDimensionsEx( UI_WIDGET( doc->library_pane ), UGC_LIBRARY_PANE_WIDTH, 1, UIUnitFixed, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( doc->library_pane ), 0, 0, UGC_PANE_TOP_BORDER, unplacedComponentsPaneSize );
		ui_WidgetSetDimensions( UI_WIDGET( doc->unplaced_pane ), UGC_LIBRARY_PANE_WIDTH, unplacedComponentsPaneSize );
		ui_WidgetSetPaddingEx( UI_WIDGET( doc->unplaced_pane ), 0, 0, 0, 0 );
	}

	ugcLayoutUpdateFlowchart(doc, doc->map_widget_container, false);
	ugcMapEditorPropertiesRefresh( doc );
	ugcMapEditorGlobalPropertiesWindowRefresh( doc );
	ugcMapEditorBackdropPropertiesWindowRefresh( doc );

	ugcMapEditorUpdateZOrder(doc);
	ugcMapEditorUpdateSnap(doc);
}

static void ugcLayoutUnplacedComponentDrag(UGCUnplacedList* pList, UGCMapEditorDoc* doc, U32 componentID)
{
	UGCComponent *component = ugcEditorFindComponentByID( componentID );
	if (component)
	{
		if (ugcLayoutCanCreateComponent(doc->map_data->pcName, component->eType))
		{
			UGCUIMapEditorComponent *component_ui = ugcLayoutUIGetComponent(doc, component);
			if (!component_ui)
				component_ui = ugcLayoutUIComponentCreate(doc, component, false);
			ugcMapUIStartDragComponent(component_ui, 0, 0, UGC_DRAG_MOVE_COMPONENT);
		}
		else
		{
			ugcModalDialogMsg( "UGC_MapEditor.Error_TooManyComponents", "UGC_MapEditor.Error_TooManyComponentsDetails", UIOk );
		}
	}
}

static void ugcMapEditorAssetLibrarySetTab( UIButton* button, UGCMapEditorDoc* doc )
{
	int tabIndex = button->widget.u64;

	doc->activeLibraryTabIndex = tabIndex;
	ugcEditorQueueUIUpdate();
}

void ugcMapEditorUpdateAssetLibrary(UGCMapEditorDoc *doc)
{
	static const char* tab_strings_int[] = {
		"Special", "UGC_MapEditor.AssetLibrary_Special", "UGC_MapEditor.AssetLibrary_Special.Tooltip",
		"Room", "UGC_MapEditor.AssetLibrary_Room", "UGC_MapEditor.AssetLibrary_Room.Tooltip",
		"Detail", "UGC_MapEditor.AssetLibrary_Detail", "UGC_MapEditor.AssetLibrary_Detail.Tooltip",
		"Trap", "UGC_MapEditor.AssetLibrary_Trap", "UGC_MapEditor.AssetLibrary_Trap.Tooltip",
		"Teleporter", "UGC_MapEditor.AssetLibrary_Teleporter", "UGC_MapEditor.AssetLibrary_Teleporter.Tooltip",
		"Costume", "UGC_MapEditor.AssetLibrary_Costume", "UGC_MapEditor.AssetLibrary_Costume.Tooltip",
		"Encounter", "UGC_MapEditor.AssetLibrary_Encounter", "UGC_MapEditor.AssetLibrary_Encounter.Tooltip",
		NULL // End of list
	};
	static const char *tab_strings_prefab_int[] = {
		"Special", "UGC_MapEditor.AssetLibrary_Special", "UGC_MapEditor.AssetLibrary_Special.Tooltip",
		"Detail", "UGC_MapEditor.AssetLibrary_Detail", "UGC_MapEditor.AssetLibrary_Detail.Tooltip",
		"Trap", "UGC_MapEditor.AssetLibrary_Trap", "UGC_MapEditor.AssetLibrary_Trap.Tooltip",
		"Teleporter", "UGC_MapEditor.AssetLibrary_Teleporter", "UGC_MapEditor.AssetLibrary_Teleporter.Tooltip",
		"Costume", "UGC_MapEditor.AssetLibrary_Costume", "UGC_MapEditor.AssetLibrary_Costume.Tooltip",
		"Encounter", "UGC_MapEditor.AssetLibrary_Encounter", "UGC_MapEditor.AssetLibrary_Encounter.Tooltip",
		NULL // End of list
	};
	static const char *tab_strings_ground[] = {
		"Special", "UGC_MapEditor.AssetLibrary_Special", "UGC_MapEditor.AssetLibrary_Special.Tooltip",
		"Detail", "UGC_MapEditor.AssetLibrary_Detail", "UGC_MapEditor.AssetLibrary_Detail.Tooltip",
		"Cluster", "UGC_MapEditor.AssetLibrary_Cluster", "UGC_MapEditor.AssetLibrary_Cluster.Tooltip",
		"Trap", "UGC_MapEditor.AssetLibrary_Trap", "UGC_MapEditor.AssetLibrary_Trap.Tooltip",
		"Teleporter", "UGC_MapEditor.AssetLibrary_Teleporter", "UGC_MapEditor.AssetLibrary_Teleporter.Tooltip",
		"Costume", "UGC_MapEditor.AssetLibrary_Costume", "UGC_MapEditor.AssetLibrary_Costume.Tooltip",
		"Encounter", "UGC_MapEditor.AssetLibrary_Encounter", "UGC_MapEditor.AssetLibrary_Encounter.Tooltip",
		NULL // End of list
	};
	static const char *tab_strings_space[] = {
		"Special", "UGC_MapEditor.AssetLibrary_Special", "UGC_MapEditor.AssetLibrary_Special.Tooltip",
		"Planet", "UGC_MapEditor.AssetLibrary_Planet", "UGC_MapEditor.AssetLibrary_Planet.Tooltip",
		"SpaceDetail", "UGC_MapEditor.AssetLibrary_Detail", "UGC_MapEditor.AssetLibrary_Detail.Tooltip",
		"SpaceCostume", "UGC_MapEditor.AssetLibrary_Costume", "UGC_MapEditor.AssetLibrary_Costume.Tooltip",
		"SpaceEncounter", "UGC_MapEditor.AssetLibrary_Encounter", "UGC_MapEditor.AssetLibrary_Encounter.Tooltip",
		NULL // End of list
	};
	UGCMapType map_type = ugcMapGetType( doc->map_data );
	char strContextName[ 256 ];
	MEFieldContext* pContext = NULL;
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UIPane* pane;
	const char **tab_strings = NULL;
	float y = 0;
	
	sprintf( strContextName, "UGCMapEditor_%s_AssetLibrary", doc->map_data->pcName );
	pContext = MEContextPush( strContextName, NULL, NULL, NULL);
	MEContextSetParent( UI_WIDGET( doc->library_pane ));

	switch (map_type) {
	case UGC_MAP_TYPE_INTERIOR:
		tab_strings = tab_strings_int;
		break;
	case UGC_MAP_TYPE_PREFAB_INTERIOR:
		tab_strings = tab_strings_prefab_int;
		break;
	case UGC_MAP_TYPE_PREFAB_GROUND:
		tab_strings = tab_strings_ground;
		break;
	case UGC_MAP_TYPE_SPACE:
	case UGC_MAP_TYPE_PREFAB_SPACE:
		tab_strings = tab_strings_space;
		break;
	}

	entry = MEContextAddLabelMsg( "Header", "UGC_MapEditor.AssetLibrary", NULL );
	widget = UI_WIDGET( ENTRY_LABEL( entry ));
	ui_WidgetSetFont( widget, "UGC_Header_Alternate" );
	ui_WidgetSetPosition( widget, 4, 2 );
	ui_LabelResize( ENTRY_LABEL( entry ));

	ui_PaneSetTitleHeight( doc->library_pane, 24 );
	y = 24;

	if (tab_strings) {
		int it = 0;
		int tabX = 0;
		float tabY = 0;

		pane = MEContextPushPaneParent( "HeaderPane" );
		ui_PaneSetStyle( pane, "UGC_Pane_Light_Header_Box_Cover", true, false );
		MEContextGetCurrent()->astrOverrideSkinName = allocAddString( "UGCAssetLibrary" );
		while( tab_strings[ it ]) {
			UGCAssetTagType* tagType = RefSystem_ReferentFromString("TagType", tab_strings[it + 0]);
			const char* displayName = tab_strings[it + 1];
			const char* tooltipDisplayName = tab_strings[it + 2];

			if (tagType) {
				entry = MEContextAddButtonIndexMsg( displayName, NULL, ugcMapEditorAssetLibrarySetTab, doc, "TagTypeTab", it, NULL, tooltipDisplayName );
				widget = UI_WIDGET( ENTRY_BUTTON( entry ));
				ENTRY_BUTTON( entry )->textOffsetFrom = UILeft;
				widget->u64 = it;
				if( it == doc->activeLibraryTabIndex ) {
					SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCTab_Picker_Active", widget->hOverrideSkin );
				}

				if( tabX == 0 ) {
					ui_WidgetSetPositionEx( widget, 0, tabY, 0.5, 0, UITopRight );
				} else {
					ui_WidgetSetPositionEx( widget, 0, tabY, 0.5, 0, UITopLeft );
				}
				ui_WidgetSetWidth( widget, 120 );

				++tabX;
				if( tabX >= 2 ) {
					tabX = 0;
					tabY = ui_WidgetGetNextY( widget );
				}
			}
			it += 3;
		}
		if( it % 2 != 0 ) {
			entry = MEContextAddButtonIndex( NULL, NULL, NULL, NULL, "TagTypeTabPlaceholder", it, NULL, NULL );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			ui_SetActive( widget, false );

			ui_WidgetSetPositionEx( widget, 0, tabY, 0.5, 0, UITopLeft );
			ui_WidgetSetWidth( widget, 120 );
			tabY = ui_WidgetGetNextY( widget );
		}
		MEContextPop( "HeaderPane" );
		ui_WidgetSetDimensions( UI_WIDGET( pane ), 260, tabY );

		{
			UGCAssetTagType* activeTagType = RefSystem_ReferentFromString( "TagType", tab_strings[ doc->activeLibraryTabIndex ]);
			if( activeTagType ) {
				if( !doc->libraryEmbeddedPicker ) {
					doc->libraryEmbeddedPicker = ugcAssetLibraryPaneCreate( UGCAssetLibrary_MapEditorEmbedded, true, ugcMapUIStartComponentDrag, NULL, doc );
				}
				ugcAssetLibraryPaneSetTagTypeName( doc->libraryEmbeddedPicker, activeTagType->pcName );
				ugcAssetLibraryPaneRestrictMapType( doc->libraryEmbeddedPicker, map_type );
				ugcAssetLibraryPaneSetHeaderWidget( doc->libraryEmbeddedPicker, UI_WIDGET( pane ));
				widget = UI_WIDGET( ugcAssetLibraryPaneGetUIPane( doc->libraryEmbeddedPicker ));
				ui_WidgetSetPosition( widget, 0, 0 );
				ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
				ui_WidgetSetPaddingEx( widget, 10, 10, y + 5, 10 );
				ui_WidgetGroupMove( &MEContextGetCurrent()->pUIContainer->children, widget );
			}
		}
	}

	MEContextPop( strContextName );
}

void ugcMapEditorUpdateUnplaced( UGCMapEditorDoc* doc )
{
	UIWidget* widget;
	
	MEExpanderRefreshLabel( &doc->unplaced_header, NULL, NULL, 4, 0, 2, UI_WIDGET( doc->unplaced_pane ));
	ui_LabelSetMessage( doc->unplaced_header, "UGC_MapEditor.UnplacedComponents" );
	ui_WidgetSetFont( UI_WIDGET( doc->unplaced_header ), "UGC_Header_Alternate" );
	ui_LabelResize( doc->unplaced_header );
	ui_PaneSetTitleHeight( doc->unplaced_pane, 24 );
	if (!doc->unplaced_list)
	{
		doc->unplaced_list = ugcUnplacedListCreate( UGCUnplacedList_MapEditor, ugcLayoutUnplacedComponentDrag, doc );
	}
	widget = ugcUnplacedListGetUIWidget( doc->unplaced_list );
	ugcUnplacedListSetMap( doc->unplaced_list, doc->map_data->pcName );
	ui_WidgetSetPosition( widget, 0, 24 );
	ui_WidgetSetDimensionsEx( widget, 1.f, 1.f, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( widget, 10, 10, 10, 10 );
	ui_WidgetGroupMove( &doc->unplaced_pane->widget.children, widget );
}

void ugcLayoutOncePerFrame(UGCMapEditorDoc *doc)
{
	if( doc->drag_state ) {
		ugcDragOncePerFrame(doc);
	}
	eaDestroyEx( &doc->events, ugcMapUIDestroyMouseEventInternal );
}

void ugcMapEditorWidgetFreeInternal( UGCUIMapEditorWidget* widget )
{
	ugcMapEditorWidgetRemoveFromGroup( widget );
	free(widget);
}

void ugcMapEditorWidgetContainerAddChild( UGCUIMapEditorWidgetContainer* parent, UGCUIMapEditorWidget* child )
{
	devassertmsg( !child || !child->group || child->group == &parent->eaChildren,
				  "Widget is already in a group, call ui_WidgetGroupMove to change group" );
	if( eaFind( &parent->eaChildren, child ) < 0 ) {
		eaPush( &parent->eaChildren, child );
		child->group = &parent->eaChildren;
	}
}

void ugcMapEditorWidgetQueueFree( UGCUIMapEditorWidget* widget )
{
	if( widget ) {
		ugcMapEditorWidgetRemoveFromGroup( widget );
		eaPushUnique( &g_eaUGCMapEditorWidgetFreeQueue, widget );
	}
}

void ugcMapEditorWidgetRemoveFromGroup( UGCUIMapEditorWidget* widget )
{
	if( widget && widget->group ) {
		UGCUIMapEditorWidget*** group = widget->group;
		eaFindAndRemove( group, widget );
		widget->group = NULL;
	}
}

static void ugcMapEditorWidgetContainerTick( UGCUIMapEditorWidgetContainer* container, UI_PARENT_ARGS )
{
	UI_GET_COORDINATES( container );	
	UGCMapEditorDoc* doc = container->doc;
	int count = eaSize( &container->eaChildren );
	size_t size = sizeof( UGCUIMapEditorWidget* ) * count;
	UGCUIMapEditorWidget** copied = NULL;
	
	devassertmsg( size < 204800, "Way too many widgets in this group." );
	copied = _alloca( size );
	memcpy( copied, container->eaChildren, size );
	{
		int it;
		for( it = 0; it != count; ++it ) {
			UGCUIMapEditorWidget* widget = copied[ it ];
			widget->tickF( widget, UI_PARENT_VALUES );
		}
	}

	// Handle mouse events
	if( doc ) {
		{
			UGCMapUIMouseEvent *event;
			if (event = ugcMapUIDoMouseEventTest(doc, MS_LEFT, &pBox, UGC_MOUSE_EVENT_BACKDROP, ugcLayoutBackdropMouseCB, "BACKDROP LMB"))
			{
				event->drag_type = 0;
				if( inpLevelPeek( INP_SPACE )) {
					event->priority = UGC_MOUSE_EVENT_BACKDROP_HIGH;
					event->is_selected = true;
				}
			}
			if (event = ugcMapUIDoMouseEventTest(doc, MS_RIGHT, &pBox, UGC_MOUSE_EVENT_BACKDROP, ugcLayoutBackdropMouseCB, "BACKDROP RMB"))
			{
				event->drag_type = 0;
			}
		}

		if (!doc->drag_state)
		{
			doc->map_widget->forceAutoEdgePan = false;
			if (eaSize(&doc->events) > 0) {
				int it;
				// Sort by priority
				eaQSort(doc->events, ugcMapUIMouseEventCompare);				
				for( it = 0; it < eaSize( &doc->events ); it++ ) {
					UGCMapUIMouseEvent* event = doc->events[ it ];

					if( event->type == UGC_MOUSE_EVENT_HOVER ) {
						continue;
					}
				
					printf( "%c %s%s - %s\n",
							(it == 0 ? '*' : ' '),
							(event->type == UGC_MOUSE_EVENT_CLICK			? "CLICK" :
							 event->type == UGC_MOUSE_EVENT_DOUBLE_CLICK	? "DBCLK" :
							 event->type == UGC_MOUSE_EVENT_DRAG			? "DRAG " :
							 "?????"),
							event->is_selected ? "(SEL)" : "     ",
							event->debug_label );
				}
				// Only process the first event
				if( doc->events[0]->callback( doc, doc->events[0] )) {
					inpHandled();
				}
			}
		}
	}
}

static void ugcMapEditorWidgetContainerDraw( UGCUIMapEditorWidgetContainer* container, UI_PARENT_ARGS )
{
	int count = eaSize( &container->eaChildren );
	size_t size = sizeof( UGCUIMapEditorWidget* ) * count;
	UGCUIMapEditorWidget** copied = NULL;
	int it;
	UGCUIMapEditorDrawLayer layerIt;
	
	devassertmsg( size < 204800, "Way too many widgets in this group." );
	copied = _alloca( size );
	memcpy( copied, container->eaChildren, size );
	for( layerIt = 0; layerIt != UGC_MAP_LAYER_MAX; ++layerIt ) {
		for( it = count - 1; it >= 0; --it ) {
			UGCUIMapEditorWidget* widget = copied[ it ];
			widget->drawF( widget, layerIt, UI_PARENT_VALUES );
		}
	}
}

static void ugcMapEditorWidgetContainerFreeInternal( UGCUIMapEditorWidgetContainer* container )
{
	int count = eaSize( &container->eaChildren );
	size_t size = sizeof( UGCUIMapEditorWidget* ) * count;
	UGCUIMapEditorWidget** copied = NULL;
	int it;
	
	devassertmsg( size < 204800, "Way too many widgets in this group." );
	copied = _alloca( size );
	memcpy( copied, container->eaChildren, size );
	
	for( it = 0; it != count; ++it ) {
		UGCUIMapEditorWidget* widget = copied[ it ];
		widget->freeF( widget );
	}

	ui_WidgetFreeInternal( UI_WIDGET( container ));
}

UGCUIMapEditorWidgetContainer* ugcMapEditorWidgetContainerCreate( void )
{
	UGCUIMapEditorWidgetContainer* container = calloc( 1, sizeof( *container ));
	ui_WidgetInitialize( UI_WIDGET( container ), ugcMapEditorWidgetContainerTick, ugcMapEditorWidgetContainerDraw, ugcMapEditorWidgetContainerFreeInternal, NULL, NULL );

	return container;
}

static int ugcMapEditorWidgetSortChildWidgets( const UGCUIMapEditorWidget** ppWidget1, const UGCUIMapEditorWidget** ppWidget2 )
{
	return (*ppWidget2)->priority - (*ppWidget1)->priority;
}

void ugcMapEditorWidgetContainerSort( UGCUIMapEditorWidgetContainer* container )
{
	eaQSort( container->eaChildren, ugcMapEditorWidgetSortChildWidgets );
}
