#include "NNOUGCPlayingEditor.h"

#include "EditLibGizmos.h"
#include "EditLibUndo.h"
#include "GameClientLib.h"
#include "GfxCamera.h"
#include "GfxClipper.h"
#include "GfxPrimitive.h"
#include "../GraphicsLib/GfxSky.h"
#include "MultiEditFieldContext.h"
#include "NNOUGCAssetLibrary.h"
#include "NNOUGCCommon.h"
#include "NNOUGCDialogPromptPicker.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCEditorPrivate.h"
#include "NNOUGCMapEditorProperties.h"
#include "NNOUGCMapEditorWidgets.h"
#include "NNOUGCMissionCommon.h"
#include "NNOUGCModalDialog.h"
#include "NNOUGCResource.h"
#include "NNOUGCUnplacedList.h"
#include "NNOUGCZeniPicker.h"
#include "UGCCommon.h"
#include "UGCEditorMain.h"
#include "UIGen.h"
#include "UITextureAssembly.h"
#include "WorldGrid.h"
#include "gclCostumeView.h"
#include "gclEntity.h"
#include "gclPlayerControl.h"
#include "gclReticle.h"
#include "inputKeyBind.h"
#include "inputLib.h"
#include "interaction_common.h"
#include "wlExclusionGrid.h"
#include "AutoGen/GameServerLib_autogen_ServerCmdWrappers.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Editors););

#define ZERO_TO_NEG_ONE(i) ((i)==0?-1:(i))

AUTO_ENUM;
typedef enum UGCCombatMode {
	UGC_COMBAT_NORMAL,			ENAMES("UGC_PlayingEditor.CombatNormal")
	UGC_COMBAT_UNTARGETABLE,	ENAMES("UGC_PlayingEditor.CombatUntargetable")
	UGC_COMBAT_INVULNERABLE,	ENAMES("UGC_PlayingEditor.CombatInvulnerable")
} UGCCombatMode;
StaticDefineInt UGCCombatModeEnum[];

AUTO_ENUM;
typedef enum UGCMovementMode {
	UGC_MOVEMENT_NORMAL,		ENAMES("UGC_PlayingEditor.MovementNormal")
	UGC_MOVEMENT_FREECAM,		ENAMES("UGC_PlayingEditor.MovementFreecam")
} UGCMovementMode;
StaticDefineInt UGCMovementModeEnum[];

typedef enum UGCPlayingEditorMode {
	UGCPLAYEDIT_TRANSLATE,
	UGCPLAYEDIT_ROTATE,
	UGCPLAYEDIT_SLIDE,
} UGCPlayingEditorMode;

AUTO_STRUCT;
typedef struct UGCPlayingEditorDocState {
	UGCCombatMode combatMode;
	UGCMovementMode movementMode;
	float editDistance;
} UGCPlayingEditorDocState;
extern ParseTable parse_UGCPlayingEditorDocState[];
#define TYPE_parse_UGCPlayingEditorDocState UGCPlayingEditorDocState

typedef struct UGCPlayingEditorDoc {
	// Map components on this map that can be edited to their
	// final matrix.
	StashTable componentIDToPlayData;

	// Components newly created on this map.
	int* eaiCreatedComponentIDs;

	// Components edited (and therefore disabled) on this map.
	int* eaiEditedComponentIDs;

	// ID of the currently cut/copied component
	U32 copyComponentID;

	// What mode the copy is (cut vs copy)
	UGCEditorCopyType copyMode;

	// ID of the currently selected component
	U32 editComponentID_USEACCESSOR;

	// If the edit component is the new component
	bool isEditComponentNew_USEACCESSOR;

	// The new component
	UGCComponent newComponent_USEACCESSOR;

	// The edit map for editing the backdrop
	UGCMap *editMap;

	// The map backdrop was edited
	bool bMapEdited;

	// Costumes to draw
	StashTable componentIDToCostumeView;

	// Override matrix for the currently selected component (used
	// mainly when dragging)
	Mat4 editMat4;

	// Editor state
	UGCPlayingEditorDocState state;
	UGCPlayingEditorMode mode;
	bool lastSuspendMouseLookFailed;
	bool queueShowProperties;

	// UIWidgets and gizmos
	TranslateGizmo* translateGizmo;
	RotateGizmo* rotateGizmo;
	SlideGizmo* slideGizmo;
	HeightMapExcludeGrid* excludeGrid;
	UIWidgetWidget* rootWidget;
	CBox rootWidgetBox;
	UIButton* trashButton;
	UIPane* propertiesPane;
	UISprite* propertiesSprite;
	bool propertiesPaneIsDocked;
	UIWidget* editDistanceWidget;

	// The library
	UIPane* libraryPane;
	UGCAssetLibraryPane* libraryEmbeddedPicker;
	int activeLibraryTabIndex;

	// Unplaced Components
	UIPane* unplacedPane;
	UGCUnplacedList* unplacedList;

	// The toolbar
	UIPane* toolbarPane;
} UGCPlayingEditorDoc;

typedef struct UGCCostumeView {
	// a REF_TO a PlayerCostume.  Used so that costumes can get down
	// to the client.
	REF_TO(PlayerCostume) hCostume;

	// The CostumeView being drawn
	CostumeViewCostume* costumeView;

	// The name for the costume in costumeView, if any.
	const char* astrCostumeViewName;
} UGCCostumeView;

static UGCPlayingEditorDoc g_UGCPlayingEditorDoc;

Color ugcPlayingEditorSelectedColor = { 22, 192, 255, 255 };
float g_UGCPlayingEditorCostumeDrawDistance = 300;

static void ugcPlayingEditorRefreshToolbar( void );
static void ugcPlayingEditorRefreshLibrary( void );
static void ugcPlayingEditorRefreshUnplaced( void );
static void ugcPlayingEditorRefreshProperties( void );
static bool ugcPlayingEditorComponentMakeEditable( int componentID, Mat4 outComponentMat4 );
static void ugcPlayingEditorComponentUpdateCostumeView( UGCComponent* component );
static void ugcPlayingEditorComponentPlacementToMat4( UGCMap* map, const UGCComponentPlacement* placement, Mat4 mat4 );
static void ugcPlayingEditorMat4ToComponentPlacement( UGCMap* map, const Mat4 mat4, UGCComponentPlacement* placement );
static void ugcPlayingEditorComponentBoundingBox( UGCComponent* component, Vec3 out_boxMin, Vec3 out_boxMax );
static void ugcPlayingEditorComponentHandleBox( UGCComponent* component, Vec3 out_handleMin, Vec3 out_handleMax );
static bool ugcPlayingEditorComponentHandleIsVisible( UGCComponent* component, Mat4 camMat );
static GroupDef* ugcPlayingEditorComponentTypeEditorOnlyDef( UGCComponentType type );
static void ugcPlayingEditorGizmoStartedCB( const Mat4 matrix, UserData ignored );
static void ugcPlayingEditorGizmoFinishedCB( const Mat4 matrix, UserData ignored );
static void ugcPlayingEditorExclusionGridRefresh( Mat4 camMat );
static void ugcPlayingEditorApplyChanges( void );
static void ugcPlayingEditorCombatModeChangedCB( MEField* pField, bool bFinished, UserData ignored );
static void ugcPlayingEditorMovementModeChangedCB( MEField* pField, bool bFinished, UserData ignored );

static UGCMap* ugcPlayingEditorGetEditMap( void );
static void ugcPlayingEditorClearEditMap( void );
static UGCComponent* ugcPlayingEditorGetEditComponent( void );
static void ugcPlayingEditorClearEditComponent( void );
static void ugcPlayingEditorSetEditComponent( int componentID );
static void ugcPlayingEditorStartDragNewEditComponent( UGCAssetLibraryPane* libraryPane, UserData ignored, UGCAssetLibraryRow* row );

static UGCCostumeView* ugcCostumeViewCreate( void );
static void ugcCostumeViewDestroy( UGCCostumeView* view );
static void ugcCostumeViewSetCostume( UGCCostumeView* view, const char* astrCostumeName );
static void ugcCostumeViewDraw( UGCCostumeView* view, Mat4 mat4, Vec3 color, float alpha );
static void ugcCostumeViewGetBounds( UGCCostumeView* view, Vec3 out_boundsMin, Vec3 out_boundsMax );

static void ugcPlayingEditorSetMode( UGCPlayingEditorMode mode );
static void ugcPlayingEditorRootWidgetTick( UIWidgetWidget* widget, UI_PARENT_ARGS );
static void ugcPlayingEditorRootWidgetDraw( UIWidgetWidget* widget, UI_PARENT_ARGS );
static void ugcPlayingEditorEditBackdrop( void );
bool ugcPlayingEditorIsEnabled( void );
bool ugcPlayingEditorIsSelectionActive( void );
bool ugcPlayingEditorIsClipboardActive( void );
bool ugcPlayingEditorIsGizmoActive( void );
bool ugcPlayingEditorIsGizmoHover( void );
bool ugcPlayingEditorCanUndo( void );
bool ugcPlayingEditorCanRedo( void );
bool ugcPlayingEditorCanDeleteSelection( void );
void ugcPlayingEditorCutSelection( void );
void ugcPlayingEditorCopySelection( void );
void ugcPlayingEditorPaste( void );
void ugcPlayingEditorDeleteSelection( void );
void ugcPlayingEditorToggle( void );
static void ugcPlayingEditorDrawRoomMarkerVolume( UGCComponent* component, Mat4 mat4 );
static void ugcPlayingEditorDrawTrapTargetPath( UGCComponent* component, Mat4 mat4 );
static void ugcPlayingEditorDrawPatrolPath( UGCComponent* component );

static void ugcPlayingEditorHidePropertiesCB( UIWidget* ignored, UserData ignored2 )
{
	MEContextDestroyByName( "UGCPlayingEditor_Properties" );
	ui_WidgetQueueFreeAndNull( &g_UGCPlayingEditorDoc.propertiesPane );
	ui_WidgetQueueFreeAndNull( &g_UGCPlayingEditorDoc.propertiesSprite );
}

static void ugcPlayingEditorPinButtonCB( UIWidget* ignored, UserData ignored2 )
{
	g_UGCPlayingEditorDoc.propertiesPaneIsDocked = !g_UGCPlayingEditorDoc.propertiesPaneIsDocked;
	ugcEditorQueueUIUpdate();
}

static bool ugcPlayingEditorPopupWindowIsAllowed( void )
{
	return (ugcPlayingEditorIsEnabled()
			&& !gclPlayerControl_IsMouseLooking()
			&& !TranslateGizmoIsActive( g_UGCPlayingEditorDoc.translateGizmo )
			&& !RotateGizmoIsActive( g_UGCPlayingEditorDoc.rotateGizmo )
			&& !SlideGizmoIsActive( g_UGCPlayingEditorDoc.slideGizmo )
			&& !ui_GenGetFocus() );
}

void ugcPlayingEditorRefresh( void )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCComponent* editComponent = ugcPlayingEditorGetEditComponent();
	const char* currentMapName = zmapInfoGetPublicName( NULL );

	// Update the edit matrix
	if( editComponent ) {
		ugcPlayingEditorComponentMat4( editComponent, g_UGCPlayingEditorDoc.editMat4 );
		TranslateGizmoSetMatrix( g_UGCPlayingEditorDoc.translateGizmo, g_UGCPlayingEditorDoc.editMat4 );
		RotateGizmoSetMatrix( g_UGCPlayingEditorDoc.rotateGizmo, g_UGCPlayingEditorDoc.editMat4 );
		SlideGizmoSetMatrix( g_UGCPlayingEditorDoc.slideGizmo, g_UGCPlayingEditorDoc.editMat4 );
	}

	// Update created components
	{
		UGCComponent** eaMapComponents = NULL;
		ugcBacklinkTableGetMapComponents( ugcProj, ugcEditorGetBacklinkTable(), currentMapName, &eaMapComponents );

		eaiClear( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs );
		FOR_EACH_IN_EARRAY_FORWARDS( eaMapComponents, UGCComponent, component ) {
			if( stashIntFindElement( g_UGCPlayingEditorDoc.componentIDToPlayData, component->uID, NULL )) {
				continue;
			}

			if(   component->eType == UGC_COMPONENT_TYPE_OBJECT
				  || component->eType == UGC_COMPONENT_TYPE_CLUSTER_PART
				  || component->eType == UGC_COMPONENT_TYPE_TELEPORTER_PART
				  || component->eType == UGC_COMPONENT_TYPE_REWARD_BOX
				  || component->eType == UGC_COMPONENT_TYPE_COMBAT_JOB
				  || component->eType == UGC_COMPONENT_TYPE_SOUND
				  || component->eType == UGC_COMPONENT_TYPE_RESPAWN
				  || component->eType == UGC_COMPONENT_TYPE_TRAP_EMITTER
				  || component->eType == UGC_COMPONENT_TYPE_TRAP_TRIGGER
				  || component->eType == UGC_COMPONENT_TYPE_ROOM_MARKER
				  || component->eType == UGC_COMPONENT_TYPE_TRAP_TARGET
				  || component->eType == UGC_COMPONENT_TYPE_SPAWN
				  || component->eType == UGC_COMPONENT_TYPE_CONTACT
				  || component->eType == UGC_COMPONENT_TYPE_ACTOR
				  || component->eType == UGC_COMPONENT_TYPE_PATROL_POINT ) {
				eaiPush( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs, component->uID );
				eaiPushUnique( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs, component->uID );
			} else if( component->eType == UGC_COMPONENT_TYPE_TRAP ) {
				UGCTrapProperties* pTrapData = ugcTrapGetProperties( objectLibraryGetGroupDef( component->iObjectLibraryId, false ));;
				if( pTrapData && pTrapData->pSelfContained ) {
					eaiPush( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs, component->uID );
					eaiPushUnique( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs, component->uID );
				}
				StructDestroySafe( parse_UGCTrapProperties, &pTrapData );
			}
		} FOR_EACH_END;
	}

	// Update costumes on each edited component
	{
		int it;
		for( it = 0; it != eaiSize( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs ); ++it ) {
			UGCComponent* component = ugcEditorFindComponentByID( g_UGCPlayingEditorDoc.eaiEditedComponentIDs[ it ]);
			if( !component ) {
				continue;
			}
			ugcPlayingEditorComponentUpdateCostumeView( component );
		}
	}

	ugcPlayingEditorRefreshToolbar();
	ugcPlayingEditorRefreshLibrary();
	ugcPlayingEditorRefreshUnplaced();
	ugcPlayingEditorRefreshProperties();

	// Resize the library and unplaced pane
	if( g_UGCPlayingEditorDoc.libraryPane && g_UGCPlayingEditorDoc.unplacedList ) {
		float unplacedComponentsPaneSize = CLAMP( ugcUnplacedListGetContentHeight( g_UGCPlayingEditorDoc.unplacedList )
												  + 8			//< list assembly padding
												  + 10 + 10		//< list top and bottom padding
												  + 24,			//< title height
												  UGC_UNPLACED_PANE_HEIGHT_MIN, UGC_UNPLACED_PANE_HEIGHT_MAX );
		ui_WidgetSetDimensionsEx( UI_WIDGET( g_UGCPlayingEditorDoc.libraryPane ), UGC_LIBRARY_PANE_WIDTH, 1, UIUnitFixed, UIUnitPercentage );
		ui_WidgetSetPaddingEx( UI_WIDGET( g_UGCPlayingEditorDoc.libraryPane ), 0, 0, ui_WidgetGetHeight( UI_WIDGET( g_UGCPlayingEditorDoc.toolbarPane )), unplacedComponentsPaneSize );
		ui_WidgetSetDimensions( UI_WIDGET( g_UGCPlayingEditorDoc.unplacedPane ), UGC_LIBRARY_PANE_WIDTH, unplacedComponentsPaneSize );
		ui_WidgetSetPaddingEx( UI_WIDGET( g_UGCPlayingEditorDoc.unplacedPane ), 0, 0, 0, 0 );
	}

	// Handles the gizmo ticking
	if( ugcPlayingEditorIsEnabled() ) {
		if( !g_UGCPlayingEditorDoc.rootWidget ) {
			g_UGCPlayingEditorDoc.rootWidget = calloc( sizeof( *g_UGCPlayingEditorDoc.rootWidget ), 1 );
			ui_WidgetInitialize( UI_WIDGET( g_UGCPlayingEditorDoc.rootWidget ), ugcPlayingEditorRootWidgetTick, ugcPlayingEditorRootWidgetDraw, ui_WidgetFreeInternal, NULL, NULL );
		}
		UI_WIDGET( g_UGCPlayingEditorDoc.rootWidget )->uClickThrough = true;
		ui_WidgetSetDimensionsEx( UI_WIDGET( g_UGCPlayingEditorDoc.rootWidget ), 1, 1, UIUnitPercentage, UIUnitPercentage );
		UI_WIDGET( g_UGCPlayingEditorDoc.rootWidget )->topPad = ui_WidgetGetHeight( UI_WIDGET( g_UGCPlayingEditorDoc.toolbarPane ));
		UI_WIDGET( g_UGCPlayingEditorDoc.rootWidget )->rightPad = UGC_LIBRARY_PANE_WIDTH;
		ui_WidgetRemoveFromGroup( UI_WIDGET( g_UGCPlayingEditorDoc.rootWidget ));
		ui_WidgetAddToDevice( UI_WIDGET( g_UGCPlayingEditorDoc.rootWidget ), NULL );

		if( !g_UGCPlayingEditorDoc.trashButton ) {
			g_UGCPlayingEditorDoc.trashButton = ui_ButtonCreateImageOnly( "UGC_Icons_Labels_Delete", 10, 70, NULL, NULL );
			g_UGCPlayingEditorDoc.trashButton ->widget.priority = 10;
			ui_WidgetSetDimensions( UI_WIDGET( g_UGCPlayingEditorDoc.trashButton ), 80, 80 );
		}
	} else {
		ui_WidgetQueueFreeAndNull( &g_UGCPlayingEditorDoc.trashButton );
		ui_WidgetQueueFreeAndNull( &g_UGCPlayingEditorDoc.rootWidget );
	}

	if(g_UGCPlayingEditorDoc.bMapEdited)
	{
		SkyInfo *sky;
		const char **ppOverrideList = NULL;

		UGCMap *map = ugcEditorGetMapByName( zmapInfoGetPublicName( NULL ));
		UGCMapType mapType = ugcMapGetType( map );
		UGCGenesisBackdrop* backdrop = NULL;
		if( map->pPrefab ) {
			backdrop = &map->pPrefab->backdrop;
		} else if( map->pSpace ) {
			backdrop = &map->pSpace->backdrop;
		} else {
			assert( 0 );
		}

		eaPush(&ppOverrideList, "default_sky");
		sky = GET_REF(backdrop->hSkyBase);
		if(sky) eaPush(&ppOverrideList, sky->filename_no_path);
		FOR_EACH_IN_EARRAY_FORWARDS(backdrop->eaSkyOverrides, UGCGenesisBackdropSkyOverride, backdrop_override)
		{
			sky = GET_REF(backdrop_override->hSkyOverride);
			if(sky) eaPush(&ppOverrideList, sky->filename_no_path);
		}
		FOR_EACH_END;

		gfxCameraControllerSetSkyGroupOverride(gfxGetActiveCameraController(), ppOverrideList, NULL);
		eaDestroy(&ppOverrideList);
	}
}

bool ugcPlayingEditorQueryAction( UGCActionID action, char** out_estr )
{
	switch( action ) {
		xcase UGC_ACTION_PLAYING_RESET_MAP:
			return true;
		xcase UGC_ACTION_PLAYING_KILL_TARGET:
			return true;
		xcase UGC_ACTION_PLAYING_FULL_HEAL:
			return true;
		xcase UGC_ACTION_MAP_EDIT_BACKDROP:
			return ugcPlayingEditorIsEnabled();
		xcase UGC_ACTION_PLAYING_TOGGLE_EDIT_MODE:
			return resNamespaceIsUGC( zmapInfoGetPublicName( NULL ));
		xcase UGC_ACTION_PLAYING_TRANSLATE_MODE: case UGC_ACTION_PLAYING_ROTATE_MODE:
		case UGC_ACTION_PLAYING_SLIDE_MODE:
			return ugcPlayingEditorIsEnabled();
		xcase UGC_ACTION_CUT: case UGC_ACTION_COPY:
			return ugcPlayingEditorIsSelectionActive();
		xcase UGC_ACTION_PASTE:
			return ugcPlayingEditorIsClipboardActive();
		xcase UGC_ACTION_DELETE:
			return ugcPlayingEditorCanDeleteSelection();
		xcase UGC_ACTION_PLAYING_UNDO:
			return ugcPlayingEditorCanUndo();
		xcase UGC_ACTION_PLAYING_REDO:
			return ugcPlayingEditorCanRedo();
	}

	return false;
}

void ugcPlayingEditorHandleAction( UGCActionID action )
{
	switch( action ) {
		xcase UGC_ACTION_PLAYING_RESET_MAP:
			ugcEditorResetMap();
		xcase UGC_ACTION_PLAYING_KILL_TARGET:
			ServerCmd_ugc_KillTarget();
		xcase UGC_ACTION_PLAYING_FULL_HEAL:
			ServerCmd_gslUGC_RespawnAtFullHealth();
		xcase UGC_ACTION_MAP_EDIT_BACKDROP:
			ugcPlayingEditorEditBackdrop();
		xcase UGC_ACTION_PLAYING_TOGGLE_EDIT_MODE:
			if( ugcPlayingEditorIsEnabled() ) {
				if( ugcModalDialogMsg( "UGC_PlayingEditor.EditModeDisable_Title", "UGC_PlayingEditor.EditModeDisable_Body", UIYes | UINo ) == UIYes ) {
					ugcPlayingEditorToggle();
				}
			} else {
				ugcPlayingEditorToggle();
			}
		xcase UGC_ACTION_PLAYING_TRANSLATE_MODE:
			ugcPlayingEditorSetMode( UGCPLAYEDIT_TRANSLATE );
		xcase UGC_ACTION_PLAYING_ROTATE_MODE:
			ugcPlayingEditorSetMode( UGCPLAYEDIT_ROTATE );
		xcase UGC_ACTION_PLAYING_SLIDE_MODE:
			ugcPlayingEditorSetMode( UGCPLAYEDIT_SLIDE );
		xcase UGC_ACTION_CUT:
			ugcPlayingEditorCutSelection();
		xcase UGC_ACTION_COPY:
			ugcPlayingEditorCopySelection();
		xcase UGC_ACTION_PASTE:
			ugcPlayingEditorPaste();
		xcase UGC_ACTION_DELETE:
			ugcPlayingEditorDeleteSelection();
		xcase UGC_ACTION_PLAYING_UNDO:
			ugcEditorExecuteCommandByID( UGC_ACTION_UNDO );
		xcase UGC_ACTION_PLAYING_REDO:
			ugcEditorExecuteCommandByID( UGC_ACTION_REDO );
	}
}

static void ugcPlayingEditorAssetLibrarySetTab( UIButton* button, UserData ignored )
{
	int tabIndex = UI_WIDGET( button )->u64;
	g_UGCPlayingEditorDoc.activeLibraryTabIndex = tabIndex;
	ugcEditorQueueUIUpdate();
}

void ugcPlayingEditorRefreshToolbar( void )
{
	float x = 0;
	float y = 0;
	float xLeft = 0;
	float maxY = 0;
	MEFieldContext* uiCtx;
	MEFieldContext* combatModeCtx;
	MEFieldContext* movementModeCtx;
	MEFieldContext* editCtx;
	MEFieldContextEntry* entry;
	UIWidget* widget;
	
	if( ugcEditorIsActive() || !ugcEditorGetProjectData() ) {
		MEContextDestroyByName( "UGCPlayingEditor_Toolbar" );
		ui_WidgetQueueFreeAndNull( &g_UGCPlayingEditorDoc.toolbarPane );
		return;
	}

	if( !g_UGCPlayingEditorDoc.toolbarPane ) {
		g_UGCPlayingEditorDoc.toolbarPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	}
	ui_WidgetSetPosition( UI_WIDGET( g_UGCPlayingEditorDoc.toolbarPane ), 0, 0 );
	g_UGCPlayingEditorDoc.toolbarPane->viewportPane = UI_PANE_VP_BOTTOM;
	ui_PaneSetStyle( g_UGCPlayingEditorDoc.toolbarPane, "UGC_Pane_Light_With_Inset", true, false );
	// dimensions are auto-calculated later
	// ui_WidgetSetDimensions()
	ui_WidgetAddToDevice( UI_WIDGET( g_UGCPlayingEditorDoc.toolbarPane ), NULL );

	uiCtx = MEContextPush( "UGCPlayingEditor_Toolbar", &g_UGCPlayingEditorDoc.state, &g_UGCPlayingEditorDoc.state, parse_UGCPlayingEditorDocState );
	MEContextSetParent( UI_WIDGET( g_UGCPlayingEditorDoc.toolbarPane ));
	uiCtx->iXDataStart = uiCtx->iYDataStart = uiCtx->iXPos = 0;

	entry = MEContextAddSprite( "Icons_Foundry_Logo", "Logo", NULL, NULL );
	widget = UI_WIDGET( ENTRY_SPRITE( entry ));
	ui_SpriteResize( ENTRY_SPRITE( entry ));
	ui_WidgetSetPositionEx( widget, x, 0, 0, 0, UILeft );
	x = ui_WidgetGetNextX( widget );
	maxY = MAX( maxY, ui_WidgetGetNextY( widget ));

	x += 14;
	xLeft = x;

	x = xLeft;
	y = 0;
	combatModeCtx = MEContextPush( "CombatMode", &g_UGCPlayingEditorDoc.state, &g_UGCPlayingEditorDoc.state, parse_UGCPlayingEditorDocState );
	combatModeCtx->cbChanged = ugcPlayingEditorCombatModeChangedCB;
	combatModeCtx->pChangedData = NULL;
	combatModeCtx->bDontSortComboEnums = true;
	{
		entry = MEContextAddEnumMsg( kMEFieldType_Combo, UGCCombatModeEnum, "CombatMode", NULL, NULL );
		widget = ENTRY_FIELD( entry )->pUIWidget;
		ui_WidgetSetWidth( widget, 150 );
		ui_WidgetSetPosition( widget, x, y );
		x = ui_WidgetGetNextX( widget );
		maxY = MAX( maxY, ui_WidgetGetNextY( widget ));
	}
	MEContextPop( "CombatMode" );

	movementModeCtx = MEContextPush( "MovementMode", &g_UGCPlayingEditorDoc.state, &g_UGCPlayingEditorDoc.state, parse_UGCPlayingEditorDocState );
	movementModeCtx->cbChanged = ugcPlayingEditorMovementModeChangedCB;
	movementModeCtx->pChangedData = NULL;
	movementModeCtx->bDontSortComboEnums = true;
	{
		entry = MEContextAddEnumMsg( kMEFieldType_Combo, UGCMovementModeEnum, "MovementMode", NULL, NULL );
		widget = ENTRY_FIELD( entry )->pUIWidget;
		ui_WidgetSetWidth( widget, 150 );
		ui_WidgetSetPosition( widget, x, y );
		x = ui_WidgetGetNextX( widget );
		maxY = MAX( maxY, ui_WidgetGetNextY( widget ));
	}
	MEContextPop( "MovementMode" );

	entry = ugcMEContextAddEditorButton( UGC_ACTION_PLAYING_RESET_MAP, false, false );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetPosition( widget, x, y );
	x = ui_WidgetGetNextX( widget );
	maxY = MAX( maxY, ui_WidgetGetNextY( widget ));

	entry = ugcMEContextAddEditorButton( UGC_ACTION_PLAYING_KILL_TARGET, false, false );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetPosition( widget, x, y );
	x = ui_WidgetGetNextX( widget );
	maxY = MAX( maxY, ui_WidgetGetNextY( widget ));

	entry = ugcMEContextAddEditorButton( UGC_ACTION_PLAYING_FULL_HEAL, false, false );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetPosition( widget, x, y );
	x = ui_WidgetGetNextX( widget );
	maxY = MAX( maxY, ui_WidgetGetNextY( widget ));

	entry = ugcMEContextAddEditorButton( UGC_ACTION_MAP_EDIT_BACKDROP, false, false );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetPosition( widget, x, y );
	x = ui_WidgetGetNextX( widget );
	maxY = MAX( maxY, ui_WidgetGetNextY( widget ));

	x = xLeft + 1;
	y += UGC_ROW_HEIGHT;
	entry = ugcMEContextAddEditorButton( UGC_ACTION_PLAYING_TOGGLE_EDIT_MODE, false, false );
	widget = UI_WIDGET( ENTRY_BUTTON( entry ));
	SET_HANDLE_FROM_STRING( g_hUISkinDict,
							(ugcPlayingEditorIsEnabled() ? "UGCButton_CheckActive" : "UGCButton_Check"),
							widget->hOverrideSkin );
	ui_ButtonResize( ENTRY_BUTTON( entry ));
	ui_WidgetSetPosition( widget, x, y + 4 );
	x = ui_WidgetGetNextX( widget );
	maxY = MAX( maxY, ui_WidgetGetNextY( widget ));
	x += 4;

	editCtx = MEContextPush( "Edit", &g_UGCPlayingEditorDoc.state, &g_UGCPlayingEditorDoc.state, parse_UGCPlayingEditorDocState );
	editCtx->bDisabled = editCtx->bLabelsDisabled = !ugcPlayingEditorIsEnabled();
	{
		int modeAndActionIDs[] = { UGCPLAYEDIT_TRANSLATE, UGC_ACTION_PLAYING_TRANSLATE_MODE,
								   UGCPLAYEDIT_ROTATE, UGC_ACTION_PLAYING_ROTATE_MODE,
								   UGCPLAYEDIT_SLIDE, UGC_ACTION_PLAYING_SLIDE_MODE,
								   -1 };
		int it;
		for( it = 0; modeAndActionIDs[ it ] >= 0; it += 2 ) {
			UGCPlayingEditorMode mode = modeAndActionIDs[ it + 0 ];
			UGCActionID actionID = modeAndActionIDs[ it + 1 ];
			entry = ugcMEContextAddEditorButton( actionID, true, true );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			if( mode == g_UGCPlayingEditorDoc.mode ) {
				SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Pressed", widget->hOverrideSkin );
			}
			ui_ButtonResize( ENTRY_BUTTON( entry ));
			ui_WidgetSetPosition( widget, x, y );
			x = ui_WidgetGetNextX( widget );
			maxY = MAX( maxY, ui_WidgetGetNextY( widget ));
		}

		x += 5;
	
		entry = MEContextAddMinMaxMsg( kMEFieldType_Slider, 50, 250, 0, "EditDistance", "UGC_PlayingEditor.EditDistance", "UGC_PlayingEditor.EditDistance.Tooltip" );
		widget = UI_WIDGET( ENTRY_LABEL( entry ));
		ui_LabelResize( ENTRY_LABEL( entry ));
		ui_WidgetSetPosition( widget, x, y + 4 );
		x = ui_WidgetGetNextX( widget );
		maxY = MAX( maxY, ui_WidgetGetNextY( widget ));

		widget = ENTRY_FIELD( entry )->pUIWidget;
		ui_WidgetSetWidth( widget, 100 );
		ui_WidgetSetPosition( widget, x, y );
		g_UGCPlayingEditorDoc.editDistanceWidget = widget;
		x = ui_WidgetGetNextX( widget );
		maxY = MAX( maxY, ui_WidgetGetNextY( widget ));

		x += 2;
		entry = MEContextAddSeparator( "ToolsSeparator" );
		widget = UI_WIDGET( entry->pSeparator );
		entry->pSeparator->orientation = UIVertical;
		ui_SeparatorResize( entry->pSeparator );
		ui_WidgetSetPosition( widget, x, y );
		ui_WidgetSetHeight( widget, maxY - y );
		x = ui_WidgetGetNextX( widget );
		maxY = MAX( maxY, ui_WidgetGetNextY( widget ));
		x += 2;

		entry = ugcMEContextAddEditorButton( UGC_ACTION_CUT, true, true );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetPosition( widget, x, y );
		x = ui_WidgetGetNextX( widget );
		maxY = MAX( maxY, ui_WidgetGetNextY( widget ));

		entry = ugcMEContextAddEditorButton( UGC_ACTION_COPY, true, true );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetPosition( widget, x, y );
		x = ui_WidgetGetNextX( widget );
		maxY = MAX( maxY, ui_WidgetGetNextY( widget ));

		entry = ugcMEContextAddEditorButton( UGC_ACTION_PASTE, true, true );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetPosition( widget, x, y );
		x = ui_WidgetGetNextX( widget );
		maxY = MAX( maxY, ui_WidgetGetNextY( widget ));

		entry = ugcMEContextAddEditorButton( UGC_ACTION_PLAYING_UNDO, true, true );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetPosition( widget, x, y );
		x = ui_WidgetGetNextX( widget );
		maxY = MAX( maxY, ui_WidgetGetNextY( widget ));

		entry = ugcMEContextAddEditorButton( UGC_ACTION_PLAYING_REDO, true, true );
		widget = UI_WIDGET( ENTRY_BUTTON( entry ));
		ui_ButtonResize( ENTRY_BUTTON( entry ));
		ui_WidgetSetPosition( widget, x, y );
		x = ui_WidgetGetNextX( widget );
		maxY = MAX( maxY, ui_WidgetGetNextY( widget ));
	}
	MEContextPop( "Edit" );

	MEContextPop( "UGCPlayingEditor_Toolbar" );
	
	// auto-calculate dimensions
	{
		UITextureAssembly* texas = RefSystem_ReferentFromString( "UITextureAssembly", "UGC_Pane_Light_With_Inset" );
		ui_WidgetSetDimensionsEx( UI_WIDGET( g_UGCPlayingEditorDoc.toolbarPane ), 1, maxY + ui_TextureAssemblyHeight( texas ), UIUnitPercentage, UIUnitFixed );
	}
}

void ugcPlayingEditorRefreshLibrary( void )
{
	static const char* tabStringsInterior[] = {
		"Special", "UGC_MapEditor.AssetLibrary_Special", "UGC_MapEditor.AssetLibrary_Special.Tooltip",
		"Detail", "UGC_MapEditor.AssetLibrary_Detail", "UGC_MapEditor.AssetLibrary_Detail.Tooltip",
		"Trap", "UGC_MapEditor.AssetLibrary_Trap", "UGC_MapEditor.AssetLibrary_Trap.Tooltip",
		"Teleporter", "UGC_MapEditor.AssetLibrary_Teleporter", "UGC_MapEditor.AssetLibrary_Teleporter.Tooltip",
		"Costume", "UGC_MapEditor.AssetLibrary_Costume", "UGC_MapEditor.AssetLibrary_Costume.Tooltip",
		"Encounter", "UGC_MapEditor.AssetLibrary_Encounter", "UGC_MapEditor.AssetLibrary_Encounter.Tooltip",
		NULL // End of list
	};
	static const char* tabStringsGround[] = {
		"Special", "UGC_MapEditor.AssetLibrary_Special", "UGC_MapEditor.AssetLibrary_Special.Tooltip",
		"Detail", "UGC_MapEditor.AssetLibrary_Detail", "UGC_MapEditor.AssetLibrary_Detail.Tooltip",
		"Cluster", "UGC_MapEditor.AssetLibrary_Cluster", "UGC_MapEditor.AssetLibrary_Cluster.Tooltip",
		"Trap", "UGC_MapEditor.AssetLibrary_Trap", "UGC_MapEditor.AssetLibrary_Trap.Tooltip",
		"Teleporter", "UGC_MapEditor.AssetLibrary_Teleporter", "UGC_MapEditor.AssetLibrary_Teleporter.Tooltip",
		"Costume", "UGC_MapEditor.AssetLibrary_Costume", "UGC_MapEditor.AssetLibrary_Costume.Tooltip",
		"Encounter", "UGC_MapEditor.AssetLibrary_Encounter", "UGC_MapEditor.AssetLibrary_Encounter.Tooltip",
		NULL // End of list
	};
	static const char *tabStringsSpace[] = {
		"Special", "UGC_MapEditor.AssetLibrary_Special", "UGC_MapEditor.AssetLibrary_Special.Tooltip",
		"Planet", "UGC_MapEditor.AssetLibrary_Planet", "UGC_MapEditor.AssetLibrary_Planet.Tooltip",
		"SpaceDetail", "UGC_MapEditor.AssetLibrary_Detail", "UGC_MapEditor.AssetLibrary_Detail.Tooltip",
		"SpaceCostume", "UGC_MapEditor.AssetLibrary_Costume", "UGC_MapEditor.AssetLibrary_Costume.Tooltip",
		"SpaceEncounter", "UGC_MapEditor.AssetLibrary_Encounter", "UGC_MapEditor.AssetLibrary_Encounter.Tooltip",
		NULL // End of list
	};

	UGCMap* map = ugcEditorGetMapByName( zmapInfoGetPublicName( NULL ));
	UGCMapType mapType = ugcMapGetType( map );
	const char** tabStrings = NULL; //< not an earray!
	MEFieldContextEntry* entry;
	UIWidget* widget;
	UIPane* pane;
	int y;

	if( !ugcPlayingEditorIsEnabled() || !map ) {
		MEContextDestroyByName( "UGCPlayingEditor_Library" );
		ugcAssetLibraryPaneDestroy( g_UGCPlayingEditorDoc.libraryEmbeddedPicker );
		g_UGCPlayingEditorDoc.libraryEmbeddedPicker = NULL;
		g_UGCPlayingEditorDoc.activeLibraryTabIndex = 0;
		ui_WidgetQueueFreeAndNull( &g_UGCPlayingEditorDoc.libraryPane );
		return;
	}

	// For the purposes of the asset library, every map is a "prefab"
	// map, because you can't place rooms, doors, etc.
	if( mapType == UGC_MAP_TYPE_INTERIOR ) {
		mapType = UGC_MAP_TYPE_PREFAB_INTERIOR;
	} else if( mapType == UGC_MAP_TYPE_GROUND ) {
		mapType = UGC_MAP_TYPE_PREFAB_GROUND;
	} else if( mapType == UGC_MAP_TYPE_SPACE ) {
		mapType = UGC_MAP_TYPE_PREFAB_SPACE;
	}

	if( !g_UGCPlayingEditorDoc.libraryPane ) {
		g_UGCPlayingEditorDoc.libraryPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	}
	ui_WidgetSetPositionEx( UI_WIDGET( g_UGCPlayingEditorDoc.libraryPane ), 0, 0, 0, 0, UITopRight );
	ui_WidgetSetDimensionsEx( UI_WIDGET( g_UGCPlayingEditorDoc.libraryPane ), UGC_LIBRARY_PANE_WIDTH, 1, UIUnitFixed, UIUnitPercentage );
	g_UGCPlayingEditorDoc.libraryPane->viewportPane = UI_PANE_VP_LEFT;
	ui_WidgetAddToDevice( UI_WIDGET( g_UGCPlayingEditorDoc.libraryPane ), NULL );

	MEContextPush( "UGCPlayingEditor_Library", NULL, NULL, NULL );
	MEContextSetParent( UI_WIDGET( g_UGCPlayingEditorDoc.libraryPane ));

	switch( mapType ) {
		xcase UGC_MAP_TYPE_INTERIOR: case UGC_MAP_TYPE_PREFAB_INTERIOR:
			tabStrings = tabStringsInterior;
		xcase UGC_MAP_TYPE_PREFAB_GROUND:
			tabStrings = tabStringsGround;
		xcase UGC_MAP_TYPE_SPACE: case UGC_MAP_TYPE_PREFAB_SPACE:
			tabStrings = tabStringsSpace;
	}

	entry = MEContextAddLabelMsg( "Header", "UGC_MapEditor.AssetLibrary", NULL );
	widget = UI_WIDGET( ENTRY_LABEL( entry ));
	ui_WidgetSetFont( widget, "UGC_Header_Alternate" );
	ui_WidgetSetPosition( widget, 4, 2 );
	ui_LabelResize( ENTRY_LABEL( entry ));

	ui_PaneSetTitleHeight( g_UGCPlayingEditorDoc.libraryPane, 24 );
	y = 24;

	if( tabStrings ) {
		int it = 0;
		int tabX = 0;
		float tabY = 0;

		pane = MEContextPushPaneParent( "HeaderPane" );
		ui_PaneSetStyle( pane, "UGC_Pane_Light_Header_Box_Cover", true, false );
		MEContextGetCurrent()->astrOverrideSkinName = allocAddString( "UGCAssetLibrary" );
		while( tabStrings[ it ]) {
			UGCAssetTagType* tagType = RefSystem_ReferentFromString( "TagType", tabStrings[ it + 0 ]);
			const char* displayName = tabStrings[ it + 1 ];
			const char* tooltipDisplayName = tabStrings[ it + 2 ];

			if( tagType ) {
				entry = MEContextAddButtonIndexMsg( displayName, NULL, ugcPlayingEditorAssetLibrarySetTab, NULL, "TagTypeTab", it, NULL, tooltipDisplayName );
				widget = UI_WIDGET( ENTRY_BUTTON( entry ));
				ENTRY_BUTTON( entry )->textOffsetFrom = UILeft;
				widget->u64 = it;
				if( it == g_UGCPlayingEditorDoc.activeLibraryTabIndex ) {
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
		if( tabX % 2 != 0 ) {
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
			UGCAssetTagType* activeTagType = RefSystem_ReferentFromString( "TagType", tabStrings[ g_UGCPlayingEditorDoc.activeLibraryTabIndex ]);
			if( !g_UGCPlayingEditorDoc.libraryEmbeddedPicker ) {
				g_UGCPlayingEditorDoc.libraryEmbeddedPicker = ugcAssetLibraryPaneCreate( UGCAssetLibrary_PlayingEditorEmbedded, true, ugcPlayingEditorStartDragNewEditComponent, NULL, NULL );
			}
			ugcAssetLibraryPaneSetTagTypeName( g_UGCPlayingEditorDoc.libraryEmbeddedPicker, activeTagType->pcName );
			ugcAssetLibraryPaneRestrictMapType( g_UGCPlayingEditorDoc.libraryEmbeddedPicker, mapType );
			ugcAssetLibraryPaneSetHeaderWidget( g_UGCPlayingEditorDoc.libraryEmbeddedPicker, UI_WIDGET( pane ));
			widget = UI_WIDGET( ugcAssetLibraryPaneGetUIPane( g_UGCPlayingEditorDoc.libraryEmbeddedPicker ));
			ui_WidgetSetPosition( widget, 0, 0 );
			ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
			ui_WidgetSetPaddingEx( widget, 10, 10, y + 5, 10 );
			ui_WidgetGroupMove( &MEContextGetCurrent()->pUIContainer->children, widget );
		}
	}

	MEContextPop( "UGCPlayingEditor_Library" );
}

static void ugcPlayingEditorUnplacedDragCB( UGCUnplacedList* list, UserData ignored, U32 componentID )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCComponent* component = ugcEditorFindComponentByID( componentID );
	UGCMap* map = ugcEditorGetMapByName( zmapInfoGetPublicName( NULL ));

	if( !map || !component ) {
		return;
	}

	ugcComponentOpSetPlacement( ugcEditorGetProjectData(), component, map, component->sPlacement.uRoomID );
	switch( ugcMapGetType( map )) {
		xcase UGC_MAP_TYPE_INTERIOR:
			component->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE;
		xdefault:
			component->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_ABSOLUTE;
	}

	eaiPushUnique( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs, component->uID );
	eaiPushUnique( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs, component->uID );
	ugcPlayingEditorSetEditComponent( component->uID );

	// And start the translate gizmo
	{
		Vec3 rayStart;
		Vec3 rayEnd;
		Vec3 entPos;
		Vec3 camPYR;
		Mat4 mat4;
		editLibCursorRay( rayStart, rayEnd );
		entGetPos( entActivePlayerPtr(), entPos );
		gfxGetActiveCameraYPR( camPYR );

		identityMat3( mat4 );
		yawMat3( camPYR[ 1 ], mat4 );
		if( !findPlaneLineIntersection( entPos, mat4[ 2 ], rayStart, rayEnd, mat4[ 3 ])) {
			copyVec3( entPos, mat4[ 3 ]);
		}

		copyMat4( mat4, g_UGCPlayingEditorDoc.editMat4 );
		SlideGizmoSetMatrix( g_UGCPlayingEditorDoc.slideGizmo, mat4 );
		SlideGizmoForceActive( g_UGCPlayingEditorDoc.slideGizmo, true );

		g_UGCPlayingEditorDoc.mode = UGCPLAYEDIT_SLIDE;
	}

	ugcPlayingEditorComponentUpdateCostumeView( ugcPlayingEditorGetEditComponent() );
}

void ugcPlayingEditorRefreshUnplaced( void )
{
	UGCMap* map = ugcEditorGetMapByName( zmapInfoGetPublicName( NULL ));
	MEFieldContextEntry* entry;
	UIWidget* widget;
	int y;

	if( !ugcPlayingEditorIsEnabled() || !map ) {
		MEContextDestroyByName( "UGCPlayingEditor_Unplaced" );
		ugcUnplacedListDestroy( &g_UGCPlayingEditorDoc.unplacedList );
		ui_WidgetQueueFreeAndNull( &g_UGCPlayingEditorDoc.unplacedPane );
		return;
	}

	if( !g_UGCPlayingEditorDoc.unplacedPane ) {
		g_UGCPlayingEditorDoc.unplacedPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	}
	ui_WidgetSetPositionEx( UI_WIDGET( g_UGCPlayingEditorDoc.unplacedPane ), 0, 0, 0, 0, UIBottomRight );
	ui_WidgetSetDimensionsEx( UI_WIDGET( g_UGCPlayingEditorDoc.unplacedPane ), UGC_LIBRARY_PANE_WIDTH, 1, UIUnitFixed, UIUnitPercentage );
	g_UGCPlayingEditorDoc.libraryPane->viewportPane = UI_PANE_VP_LEFT;
	ui_WidgetAddToDevice( UI_WIDGET( g_UGCPlayingEditorDoc.unplacedPane ), NULL );

	MEContextPush( "UGCPlayingEditor_Unplaced", NULL, NULL, NULL );
	MEContextSetParent( UI_WIDGET( g_UGCPlayingEditorDoc.unplacedPane ));

	entry = MEContextAddLabelMsg( "Header", "UGC_MapEditor.UnplacedComponents", NULL );
	widget = UI_WIDGET( ENTRY_LABEL( entry ));
	ui_WidgetSetFont( widget, "UGC_Header_Alternate" );
	ui_WidgetSetPosition( widget, 4, 2 );
	ui_LabelResize( ENTRY_LABEL( entry ));

	ui_PaneSetTitleHeight( g_UGCPlayingEditorDoc.unplacedPane, 24 );
	y = 24;

	if( !g_UGCPlayingEditorDoc.unplacedList ) {
		g_UGCPlayingEditorDoc.unplacedList = ugcUnplacedListCreate( UGCUnplacedList_PlayingEditor, ugcPlayingEditorUnplacedDragCB, NULL );
	}
	ugcUnplacedListSetMap( g_UGCPlayingEditorDoc.unplacedList, map->pcName );
	widget = ugcUnplacedListGetUIWidget( g_UGCPlayingEditorDoc.unplacedList );
	ui_WidgetSetPosition( widget, 0, 24 );
	ui_WidgetSetDimensionsEx( widget, 1, 1, UIUnitPercentage, UIUnitPercentage );
	ui_WidgetSetPaddingEx( widget, 10, 10, 10, 10 );
	ui_WidgetGroupMove( &g_UGCPlayingEditorDoc.unplacedPane->widget.children, widget );

	MEContextPop( "UGCPlayingEditor_Unplaced" );
}

void ugcPlayingEditorRefreshProperties( void )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCComponent* editComponent = ugcPlayingEditorGetEditComponent();
	UGCMap* editMap = ugcPlayingEditorGetEditMap();
	CBox screenBox = { 0, 0, g_ui_State.screenWidth, g_ui_State.screenHeight };
	CBox componentScreenBox;

	// Since you can only click on things in the middle of the screen, hard code that!
	componentScreenBox.lx = g_ui_State.screenWidth / 2 - 35;
	componentScreenBox.ly = g_ui_State.screenHeight / 2 - 35;
	componentScreenBox.hx = g_ui_State.screenWidth / 2 + 35;
	componentScreenBox.hy = g_ui_State.screenHeight / 2 + 35;

	if( !ugcPlayingEditorPopupWindowIsAllowed() || (!editComponent && !editMap) ) {
		return;
	}
	if( !g_UGCPlayingEditorDoc.propertiesPane && !g_UGCPlayingEditorDoc.queueShowProperties ) {
		return;
	}

	if( !g_UGCPlayingEditorDoc.propertiesPane ) {
		g_UGCPlayingEditorDoc.propertiesPane = ui_PaneCreate( 0, 0, 1, 1, UIUnitFixed, UIUnitFixed, 0 );
	}
	// Intentionally don't set the position here... it is calculated below
	ui_PaneSetStyle( g_UGCPlayingEditorDoc.propertiesPane, "UGC_Details_Popup_Window_Vertical", true, false );
	UI_WIDGET( g_UGCPlayingEditorDoc.propertiesPane )->priority = UI_HIGHEST_PRIORITY;
	ui_WidgetAddToDevice( UI_WIDGET( g_UGCPlayingEditorDoc.propertiesPane ), NULL );

	if( !g_UGCPlayingEditorDoc.propertiesSprite ) {
		g_UGCPlayingEditorDoc.propertiesSprite = ui_SpriteCreate( 0, 0, -1, -1, "white" );
	}
	// Intentionally don't set the position here... it is calculated below
	UI_WIDGET( g_UGCPlayingEditorDoc.propertiesSprite )->priority = UI_HIGHEST_PRIORITY;
	UI_WIDGET( g_UGCPlayingEditorDoc.propertiesSprite )->uClickThrough = true;
	ui_WidgetAddToDevice( UI_WIDGET( g_UGCPlayingEditorDoc.propertiesSprite ), NULL );

	ugcEditorPlacePropertiesWidgetsForBoxes( g_UGCPlayingEditorDoc.propertiesPane, g_UGCPlayingEditorDoc.propertiesSprite, &screenBox, &componentScreenBox, g_UGCPlayingEditorDoc.propertiesPaneIsDocked, false );
	ui_GenSetFocus( NULL );
	ui_SetFocus( g_UGCPlayingEditorDoc.propertiesPane );

	{
		MEFieldContext* uiCtx = NULL;
		UIScrollArea* scrollarea;

		if(editComponent)
			uiCtx = MEContextPush( "UGCPlayingEditor_Properties", editComponent, editComponent, parse_UGCComponent );
		else if(editMap)
			uiCtx = MEContextPush( "UGCPlayingEditor_Properties", editMap, editMap, parse_UGCMap );

		uiCtx->cbChanged = ugcEditorMEFieldChangedCB;
		uiCtx->iEditableMaxLength = UGC_TEXT_SINGLE_LINE_MAX_LENGTH;
		uiCtx->bTextEntryTrimWhitespace = true;
		uiCtx->iXPos = 0;
		uiCtx->iYPos = 0;
		uiCtx->iXLabelStart = 0;
		uiCtx->iYLabelStart = 0;
		uiCtx->bLabelPaddingFromData = false;
		uiCtx->iXDataStart = 20;
		uiCtx->iYDataStart = UGC_ROW_HEIGHT - 10;
		uiCtx->iYStep = UGC_ROW_HEIGHT * 2 - 10;
		MEContextSetErrorIcon( "UGC_icons_Labels_alert", -1, -1 );
		setVec2( uiCtx->iErrorIconOffset, 0, UGC_ROW_HEIGHT - 10 + 3 );
		uiCtx->bErrorIconOffsetFromRight = false;
		uiCtx->iErrorIconSpaceWidth = 0;

		MEContextSetParent( UI_WIDGET( g_UGCPlayingEditorDoc.propertiesPane ));
		{
			char buffer[ 256 ];
			float buttonX;
			MEFieldContextEntry* entry;
			UIWidget* widget;
			UIWidget* buttonWidget = NULL;
			// UIWidget* buttonWidget2 = NULL;

			// Explicitly make sure the header doesn't have any error
			// icons capturing input
			MEContextSetErrorFunction( NULL );

			buttonX = 0;
			entry = MEContextAddButton( NULL, "ugc_icon_window_controls_close", ugcPlayingEditorHidePropertiesCB, NULL, "CloseButton", NULL, NULL );
			widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Light", widget->hOverrideSkin );
			ui_ButtonResize( ENTRY_BUTTON( entry ));
			ui_WidgetSetPositionEx( widget, buttonX, 0, 0, 0, UITopRight );
			ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
			buttonX = ui_WidgetGetNextX( widget );
			buttonWidget = widget;

			// DISABLING PIN -- Reenable this if it is determined to be useful
			// entry = MEContextAddButton( NULL,
			// 							(g_UGCPlayingEditorDoc.propertiesPaneIsDocked ? "UGC_Icon_Window_Controls_Unpin" : "UGC_Icon_Window_Controls_Pin"),
			// 							ugcPlayingEditorPinButtonCB, NULL, "PinButton", NULL, NULL );
			// widget = UI_WIDGET( ENTRY_BUTTON( entry ));
			// SET_HANDLE_FROM_STRING( g_hUISkinDict, "UGCButton_Light", widget->hOverrideSkin );
			// ui_ButtonResize( ENTRY_BUTTON( entry ));
			// ui_WidgetSetPositionEx( widget, buttonX, 0, 0, 0, UITopRight );
			// ui_WidgetSetPaddingEx( widget, 0, 0, 0, 0 );
			// buttonX = ui_WidgetGetNextX( widget );
			// buttonWidget2 = widget;

			if(editComponent)
				ugcComponentGetDisplayNameDefault( buffer, ugcProj, editComponent, false );
			else if(editMap)
				strcpy( buffer, editMap->pcDisplayName );

			entry = MEContextAddLabel( "Title", buffer, NULL );
			widget = UI_WIDGET( ENTRY_LABEL( entry ));
			UI_SET_STYLE_FONT_NAME( widget->hOverrideFont, "UGC_Header_Large" );
			ui_LabelResize( ENTRY_LABEL( entry ));
			ui_LabelSetWidthNoAutosize( ENTRY_LABEL( entry ), 1, UIUnitPercentage );
			ui_WidgetSetPositionEx( widget, 0, 0, 0, 0, UITopLeft );
			ui_WidgetSetWidthEx( widget, 1, UIUnitPercentage );
			ui_WidgetSetPaddingEx( widget, 0, buttonX, 0, 0 );
			uiCtx->iYPos = ui_WidgetGetNextY( widget ) + 5;

			// Set the button's height to be the same as the text
			ui_WidgetSetHeight( buttonWidget, ui_WidgetGetHeight( widget ));
			// ui_WidgetSetHeight( buttonWidget2, ui_WidgetGetHeight( widget ));

			scrollarea = MEContextPushScrollAreaParent( "ScrollArea" );
			ui_WidgetSetDimensionsEx( UI_WIDGET( scrollarea ), 1, 1, UIUnitPercentage, UIUnitPercentage );
			{
				if(editComponent)
					ugcMapEditorPropertiesRefreshComponent( editComponent, /*isPlayingEditor=*/true );
				else if(editMap)
					ugcMapEditorBackdropPropertiesRefresh( editMap, /*isPlayingEditor=*/true );
			}
			MEContextPop( "ScrollArea" );
		}
		MEContextPop( "UGCPlayingEditor_Properties" );
	}
	g_UGCPlayingEditorDoc.queueShowProperties = false;
}

void ugcPlayingEditorOncePerFrame( void )
{
	if( !ugcPlayingEditorPopupWindowIsAllowed() ) {
		ugcPlayingEditorHidePropertiesCB( NULL, NULL );
		if( g_UGCPlayingEditorDoc.libraryEmbeddedPicker ) {
			ugcAssetLibraryPaneClearSelected( g_UGCPlayingEditorDoc.libraryEmbeddedPicker );
		}
		if( g_UGCPlayingEditorDoc.unplacedList ) {
			ugcUnplacedListSetSelectedComponent( g_UGCPlayingEditorDoc.unplacedList, NULL );
		}
	}
	if(   g_UGCPlayingEditorDoc.propertiesPane && g_ui_State.focused
		  && !ugcAssetLibraryPickerWindowOpen() && !ugcZeniPickerWindowOpen()
		  && !ugcDialogPromptPickerWindowOpen()
		  && !ui_IsFocusedOrChildren( g_UGCPlayingEditorDoc.propertiesPane )) {
		ugcPlayingEditorHidePropertiesCB( NULL, NULL );
	}
	{
		UGCComponent* component = ugcPlayingEditorGetEditComponent();
		if(   !ugcPlayingEditorIsGizmoActive() || !component || !component->uID
			  || !ugcLayoutCanDeleteComponent( component )) {
			if( g_UGCPlayingEditorDoc.trashButton ) {
				ui_WidgetRemoveFromGroup( UI_WIDGET( g_UGCPlayingEditorDoc.trashButton ));
				UI_WIDGET( g_UGCPlayingEditorDoc.trashButton )->state &= ~kWidgetModifier_Hovering; //< with the widget removed, it won't lose the hovering state.
			}
		}
	}

	if( !ugcPlayingEditorIsEnabled() ) {
		return;
	}

	if( gclPlayerControl_IsMouseLooking() ) {
		Mat4 camMat;
		ClientReticle reticle = { 0 };
		Vec3 start;
		Vec3 end;
		ExclusionObject* impactObject = NULL;
		float dist;

		gfxGetActiveCameraMatrix( camMat );
		ugcPlayingEditorExclusionGridRefresh( camMat );
		gclReticle_GetReticle( &reticle, false );
		editLibCursorSegment( camMat, reticle.iReticlePosX, reticle.iReticlePosY, g_UGCPlayingEditorDoc.state.editDistance * 1.1, start, end );
		dist = exclusionCollideLine( g_UGCPlayingEditorDoc.excludeGrid, start, end, 0, 0, 1, NULL, &impactObject );
		if( impactObject ) {
			ugcPlayingEditorSetEditComponent( (int)impactObject->userdata );
		} else {
			ugcPlayingEditorClearEditComponent();
		}

		// Force disable all the gizmos, so dragging doesn't stick
		TranslateGizmoForceActive( g_UGCPlayingEditorDoc.translateGizmo, false );
		RotateGizmoForceActive( g_UGCPlayingEditorDoc.rotateGizmo, false );
		SlideGizmoForceActive( g_UGCPlayingEditorDoc.slideGizmo, false );
	}

	if( g_UGCPlayingEditorDoc.copyComponentID || g_UGCPlayingEditorDoc.copyMode ) {
		UGCComponent* component = ugcEditorFindComponentByID( g_UGCPlayingEditorDoc.copyComponentID );
		if( !component ) {
			g_UGCPlayingEditorDoc.copyComponentID = 0;
			g_UGCPlayingEditorDoc.copyMode = UGC_COPY_NONE;
		}
	}

	if( g_UGCPlayingEditorDoc.mode == UGCPLAYEDIT_ROTATE && (ugcPlayingEditorIsGizmoActive() || ugcPlayingEditorIsGizmoHover()) ) {
		ugcEditorSetCursorForRotation( RotateGizmoCursorAngle( g_UGCPlayingEditorDoc.rotateGizmo ));
		ui_CursorLock();
	} else if( ugcPlayingEditorIsGizmoActive() ) {
		ui_SetCursorByName( "UGC_Cursors_Move" );
		ui_CursorLock();
	} else if( ugcPlayingEditorIsGizmoHover() ) {
		ui_SetCursorByName( "UGC_Cursors_Move_Pointer" );
		ui_CursorLock();
	}
}

static void ugcPlayingEditorDrawGhostLowLevel( GroupDef* def, UGCCostumeView* costume, Mat4 mat4, Vec3 color, float alpha )
{
	if( def ) {
		TempGroupParams params = { 0 };
		params.no_culling = true;
		params.tint_color0 = color;
		params.alpha = alpha;
		worldAddTempGroup( def, mat4, &params, true );
	}
	if( costume ) {
		Vec3 camPos;
		gfxGetActiveCameraPos( camPos );
		if(   distance3Squared( mat4[ 3 ], camPos )
			  < g_UGCPlayingEditorCostumeDrawDistance * g_UGCPlayingEditorCostumeDrawDistance ) {
			ugcCostumeViewDraw( costume, mat4, color, alpha );
		}
	}
}

static void ugcPlayingEditorDrawGhostComponent( UGCComponent* component )
{
	Vec3 drawColor = { 1, 1, 1 };
	float drawAlpha = 1;
	GroupDef* componentDef;
	UGCCostumeView* componentCostume;

	if( !component ) {
		return;
	}

	if( ugcPlayingEditorComponentTypeEditorOnlyDef( component->eType ) != NULL ) {
		componentDef = ugcPlayingEditorComponentTypeEditorOnlyDef( component->eType );
		componentCostume = NULL;
	} else {
		if( component->eType == UGC_COMPONENT_TYPE_TRAP_EMITTER ) {
			UGCComponent* parent = ugcEditorFindComponentByID( component->uParentID );
			componentDef = objectLibraryGetGroupDef( SAFE_MEMBER( parent, iObjectLibraryId ), false );
			componentCostume = NULL;
		} else {
			componentDef = objectLibraryGetGroupDef( component->iObjectLibraryId, false );
			stashIntFindPointer( g_UGCPlayingEditorDoc.componentIDToCostumeView, ZERO_TO_NEG_ONE( component->uID ), &componentCostume );
		}
	}
	if( !componentDef && !componentCostume ) {
		return;
	}

	if(   component->uID && component->uID == g_UGCPlayingEditorDoc.copyComponentID
		  && g_UGCPlayingEditorDoc.copyMode == UGC_CUT_COMPONENT ) {
		drawAlpha = 0.5;
	}
	if( component == ugcPlayingEditorGetEditComponent() ) {
		scaleVec3( ugcPlayingEditorSelectedColor.rgb, 1.0 / 255, drawColor );
		drawAlpha *= 0.8;
	}

	if( component == ugcPlayingEditorGetEditComponent() && ugcPlayingEditorIsEnabled() ) {
		Vec3 minPos;
		Vec3 maxPos;
		ugcPlayingEditorComponentBoundingBox( component, minPos, maxPos );

		// The currenttly edited component may not have its
		// position updated if it is currently being dragged.  Use
		// editMat4 instead.
		ugcPlayingEditorDrawGhostLowLevel( componentDef, componentCostume, g_UGCPlayingEditorDoc.editMat4, drawColor, drawAlpha );
		gfxDrawBox3D( minPos, maxPos, g_UGCPlayingEditorDoc.editMat4, colorFromRGBA( 0x16C0FFB6 ), 1 );

		if( !gclPlayerControl_IsMouseLooking() ) {
			if( g_UGCPlayingEditorDoc.mode == UGCPLAYEDIT_TRANSLATE ) {
				TranslateGizmoDraw( g_UGCPlayingEditorDoc.translateGizmo );
			} else if( g_UGCPlayingEditorDoc.mode == UGCPLAYEDIT_ROTATE ) {
				RotateGizmoDraw( g_UGCPlayingEditorDoc.rotateGizmo );
			} else if( g_UGCPlayingEditorDoc.mode == UGCPLAYEDIT_SLIDE ) {
				SlideGizmoDraw( g_UGCPlayingEditorDoc.slideGizmo );
			}
		}

		ugcPlayingEditorDrawRoomMarkerVolume( component, g_UGCPlayingEditorDoc.editMat4 );
		ugcPlayingEditorDrawTrapTargetPath( component, g_UGCPlayingEditorDoc.editMat4 );
	} else {
		Mat4 mat4;
		ugcPlayingEditorComponentMat4( component, mat4 );
		ugcPlayingEditorDrawGhostLowLevel( componentDef, componentCostume, mat4, drawColor, drawAlpha );

		ugcPlayingEditorDrawRoomMarkerVolume( component, mat4 );
		ugcPlayingEditorDrawTrapTargetPath( component, mat4 );
	}
}

static void ugcPlayingEditorDrawGhostComponentHandle( UGCComponent* component, Mat4 camMat )
{
	Vec3 handleMin;
	Vec3 handleMax;

	if( !component || !ugcPlayingEditorComponentHandleIsVisible( component, camMat )) {
		return;
	}

	ugcPlayingEditorComponentHandleBox( component, handleMin, handleMax );

	if( component == ugcPlayingEditorGetEditComponent() ) {
		gfxDrawBox3D( handleMin, handleMax, unitmat, ugcPlayingEditorSelectedColor, 3 );
	} else {
		gfxDrawBox3D( handleMin, handleMax, unitmat, ColorWhite, 3 );
	}
}

void ugcPlayingEditorDrawGhosts( void )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCComponent* editComponent = ugcPlayingEditorGetEditComponent();
	const char* currentMapName = zmapInfoGetPublicName( NULL );
	int it;

	if( !ugcPlayingEditorIsEnabled() ) {
		return;
	}

	gfxSetPrimZTest( false );
	gfxSetPrimIgnoreMax( true );
	for( it = 0; it != eaiSize( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs ); ++it ) {
		UGCComponent* component = ugcEditorFindComponentByID( g_UGCPlayingEditorDoc.eaiEditedComponentIDs[ it ]);
		if( component !=  editComponent ) {
			ugcPlayingEditorDrawGhostComponent( component );
		}
	}
	if( editComponent ) {
		ugcPlayingEditorDrawGhostComponent( editComponent );
	}

	// Draw patrol paths
	{
		UGCComponent** eaMapComponents = NULL;
		ugcBacklinkTableGetMapComponents( ugcProj, ugcEditorGetBacklinkTable(), currentMapName, &eaMapComponents );
		for( it = 0; it != eaSize( &eaMapComponents ); ++it ) {
			UGCComponent* mapComponent = eaMapComponents[ it ];
			if( mapComponent->eType == UGC_COMPONENT_TYPE_KILL ) {
				ugcPlayingEditorDrawPatrolPath( mapComponent );
			}
		}
		eaDestroy( &eaMapComponents );
	}

	// Draw handles
	if( ugcPlayingEditorIsEnabled() && (gclPlayerControl_IsMouseLooking() || ui_IsHovering( g_UGCPlayingEditorDoc.editDistanceWidget )) ) {
		Mat4 camMat;
		gfxGetActiveCameraMatrix( camMat );
		FOR_EACH_IN_STASHTABLE2( g_UGCPlayingEditorDoc.componentIDToPlayData, elem ) {
			UGCComponent* component = ugcEditorFindComponentByID( stashElementGetIntKey( elem ));
			ugcPlayingEditorDrawGhostComponentHandle( component, camMat );
		} FOR_EACH_END;
		FOR_EACH_IN_EARRAY_INT( g_UGCPlayingEditorDoc.eaiCreatedComponentIDs, int, componentID ) {
			UGCComponent* component = ugcEditorFindComponentByID( componentID );
			ugcPlayingEditorDrawGhostComponentHandle( component, camMat );
		} FOR_EACH_END;
	}

	gfxSetPrimIgnoreMax( false );
	gfxSetPrimZTest( true );
}

void ugcPlayingEditorEditModeChanged( bool value )
{
	if( !g_UGCPlayingEditorDoc.translateGizmo ) {
		g_UGCPlayingEditorDoc.translateGizmo = TranslateGizmoCreate();
	}
	TranslateGizmoSetActivateCallback( g_UGCPlayingEditorDoc.translateGizmo, ugcPlayingEditorGizmoStartedCB );
	TranslateGizmoSetDeactivateCallback( g_UGCPlayingEditorDoc.translateGizmo, ugcPlayingEditorGizmoFinishedCB );
	if( !g_UGCPlayingEditorDoc.rotateGizmo ) {
		g_UGCPlayingEditorDoc.rotateGizmo = RotateGizmoCreate();
	}
	RotateGizmoSetActivateCallback( g_UGCPlayingEditorDoc.rotateGizmo, ugcPlayingEditorGizmoStartedCB );
	RotateGizmoSetDeactivateCallback( g_UGCPlayingEditorDoc.rotateGizmo, ugcPlayingEditorGizmoFinishedCB );
	if( !g_UGCPlayingEditorDoc.slideGizmo ) {
		g_UGCPlayingEditorDoc.slideGizmo = SlideGizmoCreate();
	}
	SlideGizmoSetActivateCallback( g_UGCPlayingEditorDoc.slideGizmo, ugcPlayingEditorGizmoStartedCB );
	SlideGizmoSetDeactivateCallback( g_UGCPlayingEditorDoc.slideGizmo, ugcPlayingEditorGizmoFinishedCB );

	if( !value ) {
		g_UGCPlayingEditorDoc.state.combatMode = UGC_COMBAT_NORMAL;
		ugcPlayingEditorCombatModeChangedCB( NULL, true, NULL );
		g_UGCPlayingEditorDoc.state.movementMode = UGC_MOVEMENT_NORMAL;
		ugcPlayingEditorMovementModeChangedCB( NULL, true, NULL );
	} else {
		keybind_PopProfileName( "UGCPlayingEditor" );
		g_UGCPlayingEditorDoc.mode = UGCPLAYEDIT_SLIDE;
		g_UGCPlayingEditorDoc.state.editDistance = 75;
		ugcPlayingEditorClearEditComponent();

		ugcEditorSetActivePlayingEditorMap( NULL );
		ugcEditorQueueApplyUpdate();
	}
}

void ugcPlayingEditorMapChanged( UGCPlayComponentData** eaComponentData )
{
	int it;

	// clear editor state
	MEContextDestroyByName( "UGCPlayingEditor_Library" );
	MEContextDestroyByName( "UGCPlayingEditor_Unplaced" );
	MEContextDestroyByName( "UGCPlayingEditor_Properties" );

	ugcPlayingEditorClearEditComponent();
	ugcPlayingEditorClearEditMap();
	g_UGCPlayingEditorDoc.bMapEdited = false;
	g_UGCPlayingEditorDoc.mode = UGCPLAYEDIT_SLIDE;
	g_UGCPlayingEditorDoc.copyComponentID = 0;
	g_UGCPlayingEditorDoc.copyMode = UGC_COPY_NONE;
	ui_WidgetQueueFreeAndNull( (UIWidgetWidget**)&g_UGCPlayingEditorDoc.rootWidget );
	ugcAssetLibraryPaneDestroy( g_UGCPlayingEditorDoc.libraryEmbeddedPicker );
	g_UGCPlayingEditorDoc.libraryEmbeddedPicker = NULL;
	g_UGCPlayingEditorDoc.activeLibraryTabIndex = 0;
	ui_WidgetQueueFreeAndNull( &g_UGCPlayingEditorDoc.libraryPane );
	ugcUnplacedListDestroy( &g_UGCPlayingEditorDoc.unplacedList );
	ui_WidgetQueueFreeAndNull( &g_UGCPlayingEditorDoc.unplacedPane );
	ui_WidgetQueueFreeAndNull( &g_UGCPlayingEditorDoc.propertiesPane );
	ui_WidgetQueueFreeAndNull( &g_UGCPlayingEditorDoc.propertiesSprite );

	// At the point this is called, a map has been reloaded so sure
	// all the interaction nodes are in their default state.
	eaiClear( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs );
	eaiClear( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs );
	stashTableClearStruct( g_UGCPlayingEditorDoc.componentIDToPlayData, NULL, parse_UGCPlayComponentData );
	stashTableClearEx( g_UGCPlayingEditorDoc.componentIDToCostumeView, NULL, ugcCostumeViewDestroy );

	ugcEditorSetActivePlayingEditorMap( NULL );

	//
	if( !g_UGCPlayingEditorDoc.componentIDToPlayData ) {
		g_UGCPlayingEditorDoc.componentIDToPlayData = stashTableCreateInt( 16 );
	}

	stashTableClearStruct( g_UGCPlayingEditorDoc.componentIDToPlayData, NULL, parse_UGCPlayComponentData );
	for( it = 0; it != eaSize( &eaComponentData ); ++it ) {
		UGCPlayComponentData* componentData = eaComponentData[ it ];
		stashIntAddPointer( g_UGCPlayingEditorDoc.componentIDToPlayData, componentData->componentID, StructClone( parse_UGCPlayComponentData, componentData ), true );
	}

	gfxCameraControllerSetSkyGroupOverride(gfxGetActiveCameraController(), NULL, NULL);
	gfxSkyClearAllVisibleSkies();
}

bool ugcPlayingEditorComponentMakeEditable( int componentID, Mat4 outComponentMat4 )
{
	UGCComponent* component;
	UGCPlayComponentData* componentData = NULL;

	identityMat4( outComponentMat4 );
	component = ugcEditorFindComponentByID( componentID );
	stashIntFindPointerConst( g_UGCPlayingEditorDoc.componentIDToPlayData, componentID, &componentData );

	if( component && eaiFind( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs, componentID ) >= 0 ) {
		UGCMap* map;
		map = ugcEditorGetComponentMap( component );
		if( !map ) {
			return false;
		}

		// Fix for crash if an author makes an object editable and
		// then does not actually edit it, so the snap did not become
		// absolute.
		if(   component->sPlacement.eSnap == COMPONENT_HEIGHT_SNAP_ABSOLUTE
			  || component->sPlacement.eSnap == COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE ) {
			ugcPlayingEditorComponentPlacementToMat4( map, &component->sPlacement, outComponentMat4 );
		} else if( componentData ) {
			copyMat4( componentData->componentMat4, outComponentMat4 );
		}
		return true;
	}

	if( !componentData ) {
		return false;
	}

	ServerCmd_gslUGC_PlayingEditorHideComponent( componentID );
	copyMat4( componentData->componentMat4, outComponentMat4 );
	eaiPushUnique( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs, componentID );

	// Also make a costume
	ugcPlayingEditorComponentUpdateCostumeView( component );

	return true;
}

void ugcPlayingEditorComponentUpdateCostumeView( UGCComponent* component )
{
	UGCCostumeView* costumeView;

	if( !g_UGCPlayingEditorDoc.componentIDToCostumeView ) {
		g_UGCPlayingEditorDoc.componentIDToCostumeView = stashTableCreateInt( 16 );
	}

	if( !stashIntFindPointer( g_UGCPlayingEditorDoc.componentIDToCostumeView, ZERO_TO_NEG_ONE( component->uID ), &costumeView )) {
		costumeView = ugcCostumeViewCreate();
		stashIntAddPointer( g_UGCPlayingEditorDoc.componentIDToCostumeView, ZERO_TO_NEG_ONE( component->uID ), costumeView, true );
	}

	if( component->eType != UGC_COMPONENT_TYPE_CONTACT && component->eType != UGC_COMPONENT_TYPE_ACTOR ) {
		ugcCostumeViewSetCostume( costumeView, NULL );
	} else {
		const char* costumeName = ugcCostumeHandleString( ugcEditorGetProjectData(), component->pcCostumeName );

		// Some encounters have randomness built in to their
		// EncounterTemplate.  Use a "generic" costume for them, since the
		// actual CritterDef used will change from play to play.
		if( !costumeName ) {
			costumeName = "Ugc_Random_Indicator_01";
		}

		ugcCostumeViewSetCostume( costumeView, costumeName );
	}
}

void ugcPlayingEditorComponentPlacementToMat4( UGCMap* map, const UGCComponentPlacement* placement, Mat4 mat4 )
{
	float yOffset;

	if( ugcMapGetType( map ) == UGC_MAP_TYPE_INTERIOR ) {
		yOffset = 0;
	} else {
		if( map->pPrefab ) {
			Vec3 spawnPos;
			ugcGetZoneMapSpawnPoint( map->pPrefab->map_name, spawnPos, NULL );
			yOffset = spawnPos[ 1 ];
		} else {
			yOffset = 0;
		}
	}

	assert( placement->eSnap == COMPONENT_HEIGHT_SNAP_ABSOLUTE
			|| placement->eSnap == COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE );
	identityMat4( mat4 );
	createMat3DegYPR( mat4, placement->vRotPYR );
	copyVec3( placement->vPos, mat4[ 3 ]);
	mat4[ 3 ][ 1 ] += yOffset;
}

void ugcPlayingEditorMat4ToComponentPlacement( UGCMap* map, const Mat4 mat4, UGCComponentPlacement* placement )
{
	float yOffset;

	if( ugcMapGetType( map ) == UGC_MAP_TYPE_INTERIOR ) {
		placement->eSnap = COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE;
		yOffset = 0;
	} else {
		placement->eSnap = COMPONENT_HEIGHT_SNAP_ABSOLUTE;
		if( map->pPrefab ) {
			Vec3 spawnPos;
			ugcGetZoneMapSpawnPoint( map->pPrefab->map_name, spawnPos, NULL );
			yOffset = spawnPos[ 1 ];
		} else {
			yOffset = 0;
		}
	}
	copyVec3( mat4[ 3 ], placement->vPos );
	placement->vPos[ 1 ] -= yOffset;
	getMat3YPR( mat4, placement->vRotPYR );
	DEGVEC3( placement->vRotPYR );
	placement->uRoomID = UGC_TOPLEVEL_ROOM_ID;
}

void ugcPlayingEditorComponentMat4( const UGCComponent* component, Mat4 mat4 )
{
	identityMat4( mat4 );

	if(   component->sPlacement.eSnap == COMPONENT_HEIGHT_SNAP_ABSOLUTE
		  || component->sPlacement.eSnap == COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE ) {
		UGCMap* map = ugcEditorGetComponentMap( component );
		if( !map ) {
			return;
		}

		ugcPlayingEditorComponentPlacementToMat4( map, &component->sPlacement, mat4 );
	} else {
		UGCPlayComponentData* componentData;

		if( !stashIntFindPointerConst( g_UGCPlayingEditorDoc.componentIDToPlayData, component->uID, &componentData )) {
			return;
		}
		copyMat4( componentData->componentMat4, mat4 );
	}
}

void ugcPlayingEditorComponentApplyMat4( UGCComponent* component, const Mat4 mat4 )
{
	ugcPlayingEditorMat4ToComponentPlacement( ugcEditorGetComponentMap( component ), mat4, &component->sPlacement );
}

void ugcPlayingEditorComponentBoundingBox( UGCComponent* component, Vec3 out_boxMin, Vec3 out_boxMax )
{
	zeroVec3( out_boxMin );
	zeroVec3( out_boxMax );

	if( ugcPlayingEditorComponentTypeEditorOnlyDef( component->eType )) {
		float radius = 0;
		GroupDef* def = ugcPlayingEditorComponentTypeEditorOnlyDef( component->eType );
		ugcComponentCalcBoundsForObjLib( def->name_uid, out_boxMin, out_boxMax, &radius );
	} else if( component->iObjectLibraryId ) {
		ugcComponentCalcBounds( ugcEditorGetComponentList(), component, out_boxMin, out_boxMax );
	} else {
		UGCCostumeView* componentCostume = NULL;
		stashIntFindPointer( g_UGCPlayingEditorDoc.componentIDToCostumeView, ZERO_TO_NEG_ONE( component->uID ), &componentCostume );
		ugcCostumeViewGetBounds( componentCostume, out_boxMin, out_boxMax );
	}
}

void ugcPlayingEditorComponentHandleBox( UGCComponent* component, Vec3 out_handleMin, Vec3 out_handleMax )
{
	Mat4 mat;
	Vec3 boundsMin;
	Vec3 boundsMax;

	ugcPlayingEditorComponentMat4( component, mat );

	ugcPlayingEditorComponentBoundingBox( component, boundsMin, boundsMax );

	copyVec3( mat[ 3 ], out_handleMin );
	copyVec3( mat[ 3 ], out_handleMax );

	if( boundsMax[ 0 ] - boundsMin[ 0 ] > 2 || boundsMax[ 0 ] - boundsMin[ 0 ] < 0.01 ) {
		out_handleMin[ 0 ] -= 1;
		out_handleMax[ 0 ] += 1;
	} else {
		out_handleMin[ 0 ] += boundsMin[ 0 ];
		out_handleMax[ 0 ] += boundsMax[ 0 ];
	}
	if( boundsMax[ 1 ] - boundsMin[ 1 ] > 2 || boundsMax[ 1 ] - boundsMin[ 1 ] < 0.01 ) {
		out_handleMin[ 1 ] -= 1;
		out_handleMax[ 1 ] += 1;
	} else {
		out_handleMin[ 1 ] += boundsMin[ 1 ];
		out_handleMax[ 1 ] += boundsMax[ 1 ];
	}
	if( boundsMax[ 2 ] - boundsMin[ 2 ] > 2 || boundsMax[ 2 ] - boundsMin[ 2 ] < 0.01 ) {
		out_handleMin[ 2 ] -= 1;
		out_handleMax[ 2 ] += 1;
	} else {
		out_handleMin[ 2 ] += boundsMin[ 2 ];
		out_handleMax[ 2 ] += boundsMax[ 2 ];
	}
}

bool ugcPlayingEditorComponentHandleIsVisible( UGCComponent* component, Mat4 camMat )
{
	Mat4 mat;
	if( component == ugcPlayingEditorGetEditComponent() ) {
		copyMat4( g_UGCPlayingEditorDoc.editMat4, mat );
	} else {
		ugcPlayingEditorComponentMat4( component, mat );
	}

	return distance3Squared( camMat[ 3 ], mat[ 3 ]) < g_UGCPlayingEditorDoc.state.editDistance * g_UGCPlayingEditorDoc.state.editDistance;
}

GroupDef* ugcPlayingEditorComponentTypeEditorOnlyDef( UGCComponentType type )
{
	switch( type ) {
		xcase UGC_COMPONENT_TYPE_TRAP_TARGET:
			return objectLibraryGetGroupDefByName( "UGC_PlayingEditor_TrapTarget", false );
		xcase UGC_COMPONENT_TYPE_TRAP_TRIGGER:
			return objectLibraryGetGroupDefByName( "UGC_PlayingEditor_TrapTrigger", false );
		xcase UGC_COMPONENT_TYPE_COMBAT_JOB:
			return objectLibraryGetGroupDefByName( "UGC_PlayingEditor_CombatJob", false );
		xcase UGC_COMPONENT_TYPE_SOUND:
			return objectLibraryGetGroupDefByName( "UGC_PlayingEditor_Sound", false );
		xcase UGC_COMPONENT_TYPE_ROOM_MARKER:
			return objectLibraryGetGroupDefByName( "UGC_PlayingEditor_RoomMarker", false );
		xcase UGC_COMPONENT_TYPE_SPAWN:
			return objectLibraryGetGroupDefByName( "UGC_PlayingEditor_Spawn", false );
		xcase UGC_COMPONENT_TYPE_PATROL_POINT:
			return objectLibraryGetGroupDefByName( "UGC_PlayingEditor_PatrolPoint", false );
		xcase UGC_COMPONENT_TYPE_RESPAWN:
			return objectLibraryGetGroupDefByName( "UGC_PlayingEditor_RespawnPoint", false );
	}

	return NULL;
}

void ugcPlayingEditorGizmoStartedCB( const Mat4 matrix, UserData ignored )
{
	UGCComponent* component = ugcPlayingEditorGetEditComponent();

	if(   g_UGCPlayingEditorDoc.mode != UGCPLAYEDIT_ROTATE && component
		  && component->uID && ugcLayoutCanDeleteComponent( component )) {
		ui_WidgetAddChild( UI_WIDGET( g_UGCPlayingEditorDoc.rootWidget ), UI_WIDGET( g_UGCPlayingEditorDoc.trashButton ));
	}
}

void ugcPlayingEditorGizmoFinishedCB( const Mat4 matrix, UserData ignored )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCComponent* component = ugcPlayingEditorGetEditComponent();
	UGCMap* map;

	if( !component ) {
		return;
	}
	map = ugcEditorGetComponentMap( component );
	if( !map ) {
		return;
	}

	// Only actually move the component if the mouse is in a real place.
	if(   !unfilteredMouseCollision( &g_UGCPlayingEditorDoc.rootWidgetBox )
		  && g_UGCPlayingEditorDoc.mode != UGCPLAYEDIT_ROTATE ) {
		ugcPlayingEditorClearEditComponent();
		return;
	}

	if( UI_WIDGET( g_UGCPlayingEditorDoc.trashButton )->state & kWidgetModifier_Hovering ) {
		if( ugcLayoutCanDeleteComponent( component )) {
			if( component->eType == UGC_COMPONENT_TYPE_ACTOR ) {
				if( ugcModalDialogMsg( "UGC_MapEditor.DeleteActorEncounter_Title", "UGC_MapEditor.DeleteActorEncounter_Body", UIYes | UINo ) != UIYes ) {
					return;
				}
			}

			ugcLayoutDeleteComponent( component );
		}
	} else {
		ugcPlayingEditorMat4ToComponentPlacement( map, g_UGCPlayingEditorDoc.editMat4, &component->sPlacement );

		// If it is an edit component, then we need to make it real.
		if( !component->uID ) {
			component = ugcComponentOpClone( ugcProj, component );

			ugcComponentCreateChildrenForInitialDrag( ugcEditorGetProjectData(), component );
			if(	  component->eType == UGC_COMPONENT_TYPE_CLUSTER
				  || component->eType == UGC_COMPONENT_TYPE_TELEPORTER
				  || component->eType == UGC_COMPONENT_TYPE_KILL
				  || component->eType == UGC_COMPONENT_TYPE_TRAP ) {
				int it;
				for( it = 0; it != eaiSize( &component->uChildIDs ); ++it ) {
					UGCComponent* newChild = ugcEditorFindComponentByID( component->uChildIDs[ it ]);
					eaiPushUnique( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs, newChild->uID );
					eaiPushUnique( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs, newChild->uID );
				}

				// "Self contained" traps do not have a separate emitter
				// or trigger.  For that case, we need to mark the trap as
				// editable.
				if( component->eType == UGC_COMPONENT_TYPE_TRAP ) {
					UGCTrapProperties* pTrapData = ugcTrapGetProperties( objectLibraryGetGroupDef( component->iObjectLibraryId, false ));
					if( pTrapData && pTrapData->pSelfContained ) {
						eaiPushUnique( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs, component->uID );
						eaiPushUnique( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs, component->uID );
						ugcPlayingEditorSetEditComponent( component->uID );
					} else {
						ugcPlayingEditorClearEditComponent();
					}
					StructDestroySafe( parse_UGCTrapProperties, &pTrapData );
				} else {
					ugcPlayingEditorClearEditComponent();
				}
			} else {
				eaiPushUnique( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs, component->uID );
				eaiPushUnique( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs, component->uID );
				ugcPlayingEditorSetEditComponent( component->uID );
			}
		}
	}

	ugcEditorQueueApplyUpdate();
}

static void ugcPlayingEditorComponentAddExclusionObject( int componentID, Mat4 camMat )
{
	UGCComponent* component = ugcEditorFindComponentByID( componentID );
	if( component && ugcPlayingEditorComponentHandleIsVisible( component, camMat )) {
		ExclusionObject* object = calloc( 1, sizeof( *object ));
		ExclusionVolume* objectVolume = StructCreate( parse_ExclusionVolume );

		object->max_radius = 1e8;
		object->userdata = (UserData)component->uID;
		identityMat4( object->mat );
		object->volume_group_owned = true;
		object->volume_group = calloc( 1, sizeof( *object->volume_group ));
		identityMat4( object->volume_group->mat_offset );
		eaPush( &object->volume_group->volumes, objectVolume );
		objectVolume->type = EXCLUDE_VOLUME_BOX;
		objectVolume->collides = true;
		identityMat4( objectVolume->mat );
		ugcPlayingEditorComponentHandleBox( component, objectVolume->extents[ 0 ], objectVolume->extents[ 1 ]);

		exclusionGridAddObject( g_UGCPlayingEditorDoc.excludeGrid, object, object->max_radius, false );
	}
}

void ugcPlayingEditorExclusionGridRefresh( Mat4 camMat )
{
	if( !g_UGCPlayingEditorDoc.excludeGrid ) {
		g_UGCPlayingEditorDoc.excludeGrid = exclusionGridCreate( 0, 0, 1, 1 );
	}
	exclusionGridClear( g_UGCPlayingEditorDoc.excludeGrid );

	FOR_EACH_IN_STASHTABLE2( g_UGCPlayingEditorDoc.componentIDToPlayData, elem ) {
		ugcPlayingEditorComponentAddExclusionObject( stashElementGetIntKey( elem ), camMat );
	} FOR_EACH_END;
	{
		int it;
		for( it = 0; it != eaiSize( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs ); ++it ) {
			ugcPlayingEditorComponentAddExclusionObject( g_UGCPlayingEditorDoc.eaiCreatedComponentIDs[ it ], camMat );
		}
	}
}

void ugcPlayingEditorApplyChanges( void )
{
	Entity* ent = entActivePlayerPtr();
	UGCMap* map = ugcEditorGetMapByName( zmapInfoGetPublicName( NULL ));
	Vec3 entPos;
	Vec3 camRot;

	if( !isProductionEditMode() ) {
		return;
	}
	if( !ent ) {
		return;
	}
	if( !map ) {
		return;
	}

	interaction_ClearPlayerInteractState( ent );
	entGetPos( ent, entPos );
	gfxGetActiveCameraYPR( camRot );
	camRot[ 0 ] = 0;
	camRot[ 1 ] = DEG( camRot[ 1 ]) + 180;
	camRot[ 2 ] = 0;

	if( ugcMapGetType( map ) == UGC_MAP_TYPE_INTERIOR ) {
		entPos[ 1 ] -= 0;
	} else {
		if( map->pPrefab ) {
			Vec3 spawnPos;
			ugcGetZoneMapSpawnPoint( map->pPrefab->map_name, spawnPos, NULL );
			entPos[ 1 ] -= spawnPos[ 1 ];
		} else {
			entPos[ 1 ] -= 0;
		}
	}

	ugcEditorPlay( map->pcName, 0, false, entPos, camRot );
}

void ugcPlayingEditorCombatModeChangedCB( MEField* pField, bool bFinished, UserData ignored )
{
	if( !bFinished ) {
		return;
	}
	
	switch( g_UGCPlayingEditorDoc.state.combatMode ) {
		case UGC_COMBAT_NORMAL:
			ServerCmd_gslUGC_GodMode(0);
			ServerCmd_gslUGC_UntargetableMode(0);
		xcase UGC_COMBAT_UNTARGETABLE:
			ServerCmd_gslUGC_GodMode(1);
			ServerCmd_gslUGC_UntargetableMode(1);
		xcase UGC_COMBAT_INVULNERABLE:
			ServerCmd_gslUGC_GodMode(1);
			ServerCmd_gslUGC_UntargetableMode(0);
		xdefault:
			ServerCmd_gslUGC_GodMode(1);
			ServerCmd_gslUGC_UntargetableMode(0);
	}
}

void ugcPlayingEditorMovementModeChangedCB( MEField* pField, bool bFinished, UserData ignored )
{
	if( !bFinished ) {
		return;
	}

	switch( g_UGCPlayingEditorDoc.state.movementMode ) {
		xcase UGC_MOVEMENT_NORMAL:
			ServerCmd_gslUGC_FreecamMode(0);
		xcase UGC_MOVEMENT_FREECAM:
			ServerCmd_gslUGC_FreecamMode(1);
		xdefault:
			ServerCmd_gslUGC_FreecamMode(0);
	}
}

UGCMap* ugcPlayingEditorGetEditMap( void )
{
	return g_UGCPlayingEditorDoc.editMap;
}

void ugcPlayingEditorClearEditMap( void )
{
	g_UGCPlayingEditorDoc.editMap = NULL;
}

UGCComponent* ugcPlayingEditorGetEditComponent( void )
{
	if( g_UGCPlayingEditorDoc.editComponentID_USEACCESSOR ) {
		return ugcEditorFindComponentByID( g_UGCPlayingEditorDoc.editComponentID_USEACCESSOR );
	} else if( g_UGCPlayingEditorDoc.isEditComponentNew_USEACCESSOR ) {
		return &g_UGCPlayingEditorDoc.newComponent_USEACCESSOR;
	} else {
		return NULL;
	}
}

void ugcPlayingEditorClearEditComponent( void )
{
	g_UGCPlayingEditorDoc.editComponentID_USEACCESSOR = 0;
	g_UGCPlayingEditorDoc.isEditComponentNew_USEACCESSOR = false;
}

void ugcPlayingEditorSetEditComponent( int componentID )
{
	UGCComponent* component = ugcEditorFindComponentByID( componentID );
	if( component && ugcPlayingEditorComponentMakeEditable( componentID, g_UGCPlayingEditorDoc.editMat4 )) {
		if( ugcComponentAllow3DRotation( component->eType )) {
			RotateGizmoEnableAxis( g_UGCPlayingEditorDoc.rotateGizmo, true, true, true, true );
		} else {
			RotateGizmoEnableAxis( g_UGCPlayingEditorDoc.rotateGizmo, false, true, false, false );
		}

		g_UGCPlayingEditorDoc.editComponentID_USEACCESSOR = componentID;
		g_UGCPlayingEditorDoc.isEditComponentNew_USEACCESSOR = false;
		TranslateGizmoSetMatrix( g_UGCPlayingEditorDoc.translateGizmo, g_UGCPlayingEditorDoc.editMat4 );
		RotateGizmoSetMatrix( g_UGCPlayingEditorDoc.rotateGizmo, g_UGCPlayingEditorDoc.editMat4 );
		SlideGizmoSetMatrix( g_UGCPlayingEditorDoc.slideGizmo, g_UGCPlayingEditorDoc.editMat4 );
	} else {
		g_UGCPlayingEditorDoc.editComponentID_USEACCESSOR = 0;
		g_UGCPlayingEditorDoc.isEditComponentNew_USEACCESSOR = false;
	}

	ugcPlayingEditorClearEditMap();
}

void ugcPlayingEditorStartDragNewEditComponent( UGCAssetLibraryPane* libraryPane, UserData ignored, UGCAssetLibraryRow* row )
{
	UGCProjectData* ugcProj = ugcEditorGetProjectData();
	UGCMap* map = ugcEditorGetMapByName( zmapInfoGetPublicName( NULL ));
	UGCComponentType type;
	char rowName[ 256 ];
	UGCComponent* component;

	if( !map || !ugcAssetLibraryRowFillComponentTypeAndName( g_UGCPlayingEditorDoc.libraryEmbeddedPicker, row, &type, SAFESTR( rowName ))) {
		return;
	}

	component = &g_UGCPlayingEditorDoc.newComponent_USEACCESSOR;
	// NOTE: This code is duplicated in ugcLayoutCreateTemporaryComponent().
	// It should get combined in the future. (Except the snap setting)
	StructReset( parse_UGCComponent, component );
	component->eType = type;
	ugcComponentOpReset( ugcProj, component, type, false );
	ugcComponentOpSetPlacement( ugcProj, component, NULL, GENESIS_UNPLACED_ID );
	switch( type ) {
		xcase UGC_COMPONENT_TYPE_PLANET: case UGC_COMPONENT_TYPE_KILL:
		case UGC_COMPONENT_TYPE_OBJECT: case UGC_COMPONENT_TYPE_DESTRUCTIBLE:
		case UGC_COMPONENT_TYPE_ROOM: case UGC_COMPONENT_TYPE_ROOM_DOOR:
		case UGC_COMPONENT_TYPE_FAKE_DOOR: case UGC_COMPONENT_TYPE_BUILDING_DEPRECATED:
		case UGC_COMPONENT_TYPE_TELEPORTER: case UGC_COMPONENT_TYPE_CLUSTER:
		case UGC_COMPONENT_TYPE_COMBAT_JOB:
			component->iObjectLibraryId = atoi( rowName );
		xcase UGC_COMPONENT_TYPE_TRAP: {
			int objlibID;
			char powerName[ 256 ];
			sscanf_s( rowName, "%d,%s", &objlibID, SAFESTR( powerName ));
			component->iObjectLibraryId = objlibID;
			StructCopyString( &component->pcTrapPower, powerName );
		}
		xcase UGC_COMPONENT_TYPE_CONTACT:
			StructCopyString( &component->pcCostumeName, rowName );
		xcase UGC_COMPONENT_TYPE_SOUND:
			component->strSoundEvent = allocAddString( rowName );
	}

	ugcComponentOpSetPlacement( ugcEditorGetProjectData(), component, map, component->sPlacement.uRoomID );
	switch( ugcMapGetType( map )) {
		xcase UGC_MAP_TYPE_INTERIOR:
			component->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_ROOM_ABSOLUTE;
		xdefault:
			component->sPlacement.eSnap = COMPONENT_HEIGHT_SNAP_ABSOLUTE;
	}

	// record that we are dragging the edit component
	g_UGCPlayingEditorDoc.editComponentID_USEACCESSOR = 0;
	g_UGCPlayingEditorDoc.isEditComponentNew_USEACCESSOR = true;

	// And start the translate gizmo
	{
		Vec3 rayStart;
		Vec3 rayEnd;
		Vec3 entPos;
		Vec3 camPYR;
		Mat4 mat4;
		editLibCursorRay( rayStart, rayEnd );
		entGetPos( entActivePlayerPtr(), entPos );
		gfxGetActiveCameraYPR( camPYR );

		identityMat3( mat4 );
		yawMat3( camPYR[ 1 ], mat4 );
		if( !findPlaneLineIntersection( entPos, mat4[ 2 ], rayStart, rayEnd, mat4[ 3 ])) {
			copyVec3( entPos, mat4[ 3 ]);
		}

		copyMat4( mat4, g_UGCPlayingEditorDoc.editMat4 );
		SlideGizmoSetMatrix( g_UGCPlayingEditorDoc.slideGizmo, mat4 );
		SlideGizmoForceActive( g_UGCPlayingEditorDoc.slideGizmo, true );

		g_UGCPlayingEditorDoc.mode = UGCPLAYEDIT_SLIDE;
	}

	ugcPlayingEditorComponentUpdateCostumeView( ugcPlayingEditorGetEditComponent() );
}

UGCCostumeView* ugcCostumeViewCreate( void )
{
	UGCCostumeView* accum = calloc( 1, sizeof( *accum ));
	return accum;
}

void ugcCostumeViewDestroy( UGCCostumeView* view )
{
	REMOVE_HANDLE( view->hCostume );
	costumeView_FreeViewCostume( view->costumeView );
	free( view );
}

void ugcCostumeViewSetCostume( UGCCostumeView* view, const char* astrCostumeName )
{
	if( astrCostumeName ) {
		SET_HANDLE_FROM_STRING( "PlayerCostume", astrCostumeName, view->hCostume );
	} else {
		REMOVE_HANDLE( view->hCostume );
	}
}

void ugcCostumeViewDraw( UGCCostumeView* view, Mat4 mat4, Vec3 color, float alpha )
{
	if( REF_STRING_FROM_HANDLE( view->hCostume ) != view->astrCostumeViewName ) {
		if( IS_HANDLE_ACTIVE( view->hCostume )) {
			if( GET_REF( view->hCostume )) {
				if( !view->costumeView ) {
					view->costumeView = costumeView_CreateViewCostume();
				}
				costumeView_RegenViewCostume( view->costumeView, GET_REF( view->hCostume ), NULL, NULL, NULL, NULL, NULL );
				view->astrCostumeViewName = REF_STRING_FROM_HANDLE( view->hCostume );
			}
		} else {
			costumeView_FreeViewCostume( view->costumeView );
			view->costumeView = NULL;
			view->astrCostumeViewName = REF_STRING_FROM_HANDLE( view->hCostume );
		}
	}

	if( view->costumeView && GET_REF( view->costumeView->hWLCostume )) {
		Quat quat;
		mat3ToQuat( mat4, quat );
		costumeView_SetViewCostumePosRot( view->costumeView, mat4[ 3 ], quat );
		costumeView_SetViewCostumeModColor( view->costumeView, color, alpha );
		costumeView_DrawViewCostume( view->costumeView );
		costumeView_DrawGhostsViewCostume( view->costumeView, false );
	}
}

void ugcCostumeViewGetBounds( UGCCostumeView* view, Vec3 out_boundsMin, Vec3 out_boundsMax )
{
	if( SAFE_MEMBER2( view, costumeView, pSkel )) {
		copyVec3( view->costumeView->pSkel->vCurrentGroupExtentsMin, out_boundsMin );
		copyVec3( view->costumeView->pSkel->vCurrentGroupExtentsMax, out_boundsMax );
	} else {
		zeroVec3( out_boundsMin );
		zeroVec3( out_boundsMax );
	}
}

void ugcPlayingEditorSetMode( UGCPlayingEditorMode mode )
{
	g_UGCPlayingEditorDoc.mode = mode;
	TranslateGizmoSetMatrix( g_UGCPlayingEditorDoc.translateGizmo, g_UGCPlayingEditorDoc.editMat4 );
	RotateGizmoSetMatrix( g_UGCPlayingEditorDoc.rotateGizmo, g_UGCPlayingEditorDoc.editMat4 );
	SlideGizmoSetMatrix( g_UGCPlayingEditorDoc.slideGizmo, g_UGCPlayingEditorDoc.editMat4 );
	ugcEditorQueueUIUpdate();
}

void ugcPlayingEditorRootWidgetTick( UIWidgetWidget* widget, UI_PARENT_ARGS )
{
	UI_GET_COORDINATES( widget );

	if( !ugcPlayingEditorGetEditComponent() || !ugcPlayingEditorIsEnabled() ) {
		return;
	}

	UI_TICK_EARLY( widget, true, false );
	if( g_UGCPlayingEditorDoc.mode == UGCPLAYEDIT_TRANSLATE ) {
		TranslateGizmoUpdate( g_UGCPlayingEditorDoc.translateGizmo );
		TranslateGizmoGetMatrix( g_UGCPlayingEditorDoc.translateGizmo, g_UGCPlayingEditorDoc.editMat4 );
	} else if( g_UGCPlayingEditorDoc.mode == UGCPLAYEDIT_ROTATE ) {
		RotateGizmoUpdate( g_UGCPlayingEditorDoc.rotateGizmo );
		RotateGizmoGetMatrix( g_UGCPlayingEditorDoc.rotateGizmo, g_UGCPlayingEditorDoc.editMat4 );
	} else if( g_UGCPlayingEditorDoc.mode == UGCPLAYEDIT_SLIDE ) {
		SlideGizmoUpdate( g_UGCPlayingEditorDoc.slideGizmo );
		SlideGizmoGetMatrix( g_UGCPlayingEditorDoc.slideGizmo, g_UGCPlayingEditorDoc.editMat4 );
	}

	// Don't let left mouse button through, since that would re-select
	// an object
	if( mouseDownHit( MS_LEFT, &box )) {
		inpHandled();
	}

	// Escape clears the selected widget
	if(   inpEdgePeek( INP_ESCAPE ) && !inpIsCaptured( INP_ESCAPE )
		  && (!g_ui_State.focused || !g_ui_State.focused->bConsumesEscape)) {
		ugcPlayingEditorClearEditComponent();
		ugcPlayingEditorHidePropertiesCB( NULL, NULL );
		ugcEditorQueueUIUpdate();
	}

	BuildCBox( &g_UGCPlayingEditorDoc.rootWidgetBox, x, y, w, h );
	UI_TICK_LATE( widget );
}

void ugcPlayingEditorRootWidgetDraw( UIWidgetWidget* widget, UI_PARENT_ARGS )
{
	UI_GET_COORDINATES( widget );

	if( !ugcPlayingEditorGetEditComponent() || !ugcPlayingEditorIsEnabled() ) {
		return;
	}

	UI_DRAW_EARLY( widget );
	UI_DRAW_LATE( widget );
}

void ugcPlayingEditorToggle( void )
{
	if( !isProductionEditMode() ) {
		return;
	}

	if( ugcPlayingEditorIsEnabled() ) {
		keybind_PopProfileName( "UGCPlayingEditor" );
		g_UGCPlayingEditorDoc.mode = UGCPLAYEDIT_SLIDE;
		ugcPlayingEditorClearEditComponent();

		ugcEditorSetActivePlayingEditorMap( NULL );
		ugcPlayingEditorApplyChanges();
	} else {
		keybind_PushProfileName( "UGCPlayingEditor" );
		g_UGCPlayingEditorDoc.mode = UGCPLAYEDIT_SLIDE;

		// Force editable all components with "Edit Only" groups
		FOR_EACH_IN_STASHTABLE2( g_UGCPlayingEditorDoc.componentIDToPlayData, elem ) {
			UGCComponent* component = ugcEditorFindComponentByID( stashElementGetIntKey( elem ));
			if( component && ugcPlayingEditorComponentTypeEditorOnlyDef( component->eType )) {
				Mat4 tempMat4;
				ugcPlayingEditorComponentMakeEditable( component->uID, tempMat4 );
			}
		} FOR_EACH_END;

		ugcEditorSetActivePlayingEditorMap( zmapInfoGetPublicName( NULL ));
		ugcEditorQueueUIUpdate();
		ServerCmd_gslUGC_PlayingEditorSetActive();
	}
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(ugcPlayingEditor_NumEditedComponents);
int ugcPlayingEditorNumEditedComponents( void )
{
	return eaiSize( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs ) + eaiSize( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs );
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(ugcPlayingEditor_IsEnabled);
bool ugcPlayingEditorIsEnabled( void )
{
	return ugcEditorGetActivePlayingEditorMap() != NULL;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME( ugcPlayingEditor_IsSelectionActive );
bool ugcPlayingEditorIsSelectionActive( void )
{
	UGCComponent* editComponent;
	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return false;
	}

	editComponent = ugcPlayingEditorGetEditComponent();
	return editComponent != NULL && editComponent->uID != 0;
}

bool ugcPlayingEditorIsClipboardActive( void )
{
	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return false;
	}
	return g_UGCPlayingEditorDoc.copyComponentID != 0;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME( ugcPlayingEditor_IsPropertiesVisible );
bool ugcPlayingEditorIsPropertiesVisible( void )
{
	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return false;
	}

	return g_UGCPlayingEditorDoc.propertiesPane != NULL;
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME( ugcPlayingEditor_IsGizmoActive );
bool ugcPlayingEditorIsGizmoActive( void )
{
	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return false;
	}

	if( !ugcPlayingEditorGetEditComponent() ) {
		return false;
	}

	return (TranslateGizmoIsActive( g_UGCPlayingEditorDoc.translateGizmo )
			|| RotateGizmoIsActive( g_UGCPlayingEditorDoc.rotateGizmo )
			|| SlideGizmoIsActive( g_UGCPlayingEditorDoc.slideGizmo ));
}

bool ugcPlayingEditorIsGizmoHover( void )
{
	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return false;
	}

	if( !ugcPlayingEditorGetEditComponent() ) {
		return false;
	}

	switch( g_UGCPlayingEditorDoc.mode ) {
		xcase UGCPLAYEDIT_TRANSLATE:
			return TranslateGizmoIsHover( g_UGCPlayingEditorDoc.translateGizmo );
		xcase UGCPLAYEDIT_ROTATE:
			return RotateGizmoIsHover( g_UGCPlayingEditorDoc.rotateGizmo );
		xcase UGCPLAYEDIT_SLIDE:
			return SlideGizmoIsHover( g_UGCPlayingEditorDoc.slideGizmo );
		xdefault:
			return false;
	}
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME( ugcPlayingEditor_LastSuspendMouseLookFailed );
bool ugcPlayingEditorLastSuspendMouseLookFailed( void )
{
	bool value = g_UGCPlayingEditorDoc.lastSuspendMouseLookFailed;
	g_UGCPlayingEditorDoc.lastSuspendMouseLookFailed = false;

	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return false;
	}

	return value;
}

AUTO_COMMAND ACMD_NAME(ugcPlayingEditor_SuspendMouseLookIfSelectionIsActive) ACMD_ACCESSLEVEL(2);
void ugcPlayingEditorSuspendMouseLookIfSelectionIsActive( void )
{
	UGCComponent* editComponent;
	if(   !isProductionEditMode() || !ugcPlayingEditorIsEnabled()
		  || !gclPlayerControl_IsMouseLooking() ) {
		return;
	}

	editComponent = ugcPlayingEditorGetEditComponent();
	if( editComponent ) {
		gclPlayerControl_SuspendMouseLook();
		gclPlayerControl_StopMoving( true );
		ui_GenSetFocus( NULL );
		ugcEditorQueueUIUpdate();
	} else {
		g_UGCPlayingEditorDoc.lastSuspendMouseLookFailed = true;
	}

	inpHandled();
}

AUTO_COMMAND ACMD_NAME(ugcPlayingEditor_ShowPropertiesIfSelectionIsActive) ACMD_ACCESSLEVEL(2);
void ugcPlayingEditorShowPropertiesIfSelectionIsActive( void )
{
	UGCComponent* editComponent;
	UGCMap* editMap;
	if(   !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return;
	}

	editComponent = ugcPlayingEditorGetEditComponent();
	editMap = ugcPlayingEditorGetEditMap();
	if( editComponent || editMap ) {
		gclPlayerControl_SuspendMouseLook();
		gclPlayerControl_StopMoving( true );
		ui_GenSetFocus( NULL );
		g_UGCPlayingEditorDoc.queueShowProperties = true;
		ugcEditorQueueUIUpdate();
	} else {
		g_UGCPlayingEditorDoc.lastSuspendMouseLookFailed = true;
	}

	inpHandled();
}

static void ugcPlayingEditorEditBackdrop( void )
{
	ugcPlayingEditorClearEditComponent();
	g_UGCPlayingEditorDoc.editMap = ugcEditorGetMapByName( zmapInfoGetPublicName( NULL ));
	g_UGCPlayingEditorDoc.bMapEdited = true;
	ugcPlayingEditorShowPropertiesIfSelectionIsActive();
}

bool ugcPlayingEditorCanUndo( void )
{
	char* estr = NULL;
	bool result;
	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return false;
	}

	result = ugcEditorQueryCommandByID( UGC_ACTION_UNDO, &estr );
	estrDestroy( &estr );
	return result;
}

bool ugcPlayingEditorCanRedo( void )
{
	char* estr = NULL;
	bool result;
	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return false;
	}

	result =  ugcEditorQueryCommandByID( UGC_ACTION_REDO, &estr );
	estrDestroy( &estr );
	return result;
}

bool ugcPlayingEditorCanDeleteSelection( void )
{
	UGCComponent* component;
	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return false;
	}

	component = ugcPlayingEditorGetEditComponent();
	if( !component || !ugcLayoutCanDeleteComponent( component )) {
		return false;
	}

	return true;
}

void ugcPlayingEditorCutSelection( void )
{
	UGCComponent* component;
	if( !ugcPlayingEditorIsSelectionActive() ) {
		return;
	}

	component = ugcPlayingEditorGetEditComponent();
	g_UGCPlayingEditorDoc.copyMode = UGC_CUT_COMPONENT;
	g_UGCPlayingEditorDoc.copyComponentID = SAFE_MEMBER( component, uID );
}

void ugcPlayingEditorCopySelection( void )
{
	UGCComponent* component;
	if( !ugcPlayingEditorIsSelectionActive() ) {
		return;
	}

	component = ugcPlayingEditorGetEditComponent();
	g_UGCPlayingEditorDoc.copyMode = UGC_COPY_COMPONENT;
	g_UGCPlayingEditorDoc.copyComponentID = SAFE_MEMBER( component, uID );
}

void ugcPlayingEditorPaste( void )
{
	UGCComponent* component;
	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return;
	}

	component = ugcEditorFindComponentByID( g_UGCPlayingEditorDoc.copyComponentID );
	if( !component ) {
		return;
	}

	if( g_UGCPlayingEditorDoc.copyMode == UGC_COPY_COMPONENT ) {
		UGCComponent* duplicate = ugcComponentOpDuplicate( ugcEditorGetProjectData(), component, component->uParentID );
		if( !duplicate ) {
			return;
		}

		// Set up enough data that we can make this an edit component
		// (since it does not have an entry)
		{
			UGCMap* map = ugcEditorGetComponentMap( duplicate );
			Mat4 entMat4;
			if( !map ) {
				return;
			}

			entGetBodyMat( entActivePlayerPtr(), entMat4 );
			ugcPlayingEditorMat4ToComponentPlacement( map, entMat4, &duplicate->sPlacement );
			eaiPushUnique( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs, duplicate->uID );
			eaiPushUnique( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs, duplicate->uID );
		}
		ugcPlayingEditorSetEditComponent( duplicate->uID );
	} else {
		UGCMap* map = ugcEditorGetComponentMap( component );
		Mat4 entMat4;
		if( !map ) {
			return;
		}

		entGetBodyMat( entActivePlayerPtr(), entMat4 );
		ugcPlayingEditorMat4ToComponentPlacement( map, entMat4, &component->sPlacement );

		g_UGCPlayingEditorDoc.copyMode = UGC_COPY_COMPONENT;
	}

	ugcEditorQueueApplyUpdate();
}

void ugcPlayingEditorDeleteSelection( void )
{
	UGCComponent* component;
	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return;
	}

	component = ugcPlayingEditorGetEditComponent();
	if( !component || !ugcLayoutCanDeleteComponent( component )) {
		return;
	}

	if( component->eType == UGC_COMPONENT_TYPE_ACTOR ) {
		if( ugcModalDialogMsg( "UGC_MapEditor.DeleteActorEncounter_Title", "UGC_MapEditor.DeleteActorEncounter_Body", UIYes | UINo ) != UIYes ) {
			return;
		}
	}

	ugcLayoutDeleteComponent( component );
	// Don't delete from the edited components, in case the component comes back (via undo)

	ugcEditorQueueApplyUpdate();
}

void ugcPlayingEditorDuplicateSelection( void )
{
	UGCComponent* component;
	UGCComponent* duplicate;
	if( !isProductionEditMode() || !ugcPlayingEditorIsEnabled() ) {
		return;
	}

	component = ugcPlayingEditorGetEditComponent();
	if( !component ) {
		return;
	}

	duplicate = ugcComponentOpDuplicate( ugcEditorGetProjectData(), component, component->uParentID );
	if( !duplicate ) {
		return;
	}

	// Set up enough data that we can make this an edit component
	// (since it does not have an entry)
	{
		UGCMap* map = ugcEditorGetComponentMap( duplicate );
		Mat4 entMat4;
		if( !map ) {
			return;
		}

		entGetBodyMat( entActivePlayerPtr(), entMat4 );
		ugcPlayingEditorMat4ToComponentPlacement( map, entMat4, &duplicate->sPlacement );
		eaiPushUnique( &g_UGCPlayingEditorDoc.eaiCreatedComponentIDs, duplicate->uID );
		eaiPushUnique( &g_UGCPlayingEditorDoc.eaiEditedComponentIDs, duplicate->uID );
	}
	ugcPlayingEditorSetEditComponent( duplicate->uID );

	ugcEditorQueueApplyUpdate();
}

void ugcPlayingEditorUndo( void )
{
	if( !isProductionEditMode() ) {
		return;
	}

	ugcEditorExecuteCommandByID( UGC_ACTION_UNDO );
}

void ugcPlayingEditorRedo( void )
{
	if( !isProductionEditMode() ) {
		return;
	}

	ugcEditorExecuteCommandByID( UGC_ACTION_REDO );
}

void ugcPlayingEditorDrawRoomMarkerVolume( UGCComponent* component, Mat4 mat )
{
	if( component->eType != UGC_COMPONENT_TYPE_ROOM_MARKER || component->fVolumeRadius < 0.1 ) {
		return;
	}

	gfxSetPrimZTest( true );
	if( component == ugcPlayingEditorGetEditComponent() ) {
		gfxDrawSphere3D( mat[ 3 ], component->fVolumeRadius, 0, colorFromRGBA( 0xFF0080FF ), 1 );
	} else {
		gfxDrawSphere3D( mat[ 3 ], component->fVolumeRadius, 0, colorFromRGBA( 0xFF008040 ), 0 );
	}
	gfxSetPrimZTest( false );
}

void ugcPlayingEditorDrawTrapTargetPath( UGCComponent* component, Mat4 mat )
{
	UGCComponent* trap = NULL;
	GroupDef* trapDef = NULL;
	UGCTrapProperties* properties = NULL;
	UGCTrapPointData* emitterData = NULL;
	UGCComponent* emitter = NULL;
	Mat4 emitterMat;
	Vec3 emitterPos;
	
	if( component->eType != UGC_COMPONENT_TYPE_TRAP_TARGET ) {
		goto cleanup;
	}
	trap = ugcEditorFindComponentByID( component->uParentID );
	if( !trap ) {
		goto cleanup;
	}
	trapDef = objectLibraryGetGroupDef( trap->iObjectLibraryId, false );
	if( !trapDef ) {
		goto cleanup;
	}
	properties = ugcTrapGetProperties( trapDef );
	if( !properties ) {
		goto cleanup;
	}
	emitterData = eaGet( &properties->eaEmitters, component->iTrapEmitterIndex );
	if( !emitterData ) {
		goto cleanup;
	}
	emitter = ugcTrapFindEmitter( ugcEditorGetProjectData(), trap );
	if( !emitter ) {
		goto cleanup;
	}

	if( emitter == ugcPlayingEditorGetEditComponent() ) {
		copyMat4( g_UGCPlayingEditorDoc.editMat4, emitterMat );
	} else {
		ugcPlayingEditorComponentMat4( emitter, emitterMat );
	}
	mulVecMat4( emitterData->pos, emitterMat, emitterPos );
	gfxDrawLine3DWidthARGB( emitterPos, mat[ 3 ], -1, -1, 2 );

cleanup:
	StructDestroySafe( parse_UGCTrapProperties, &properties );
}

void ugcPlayingEditorDrawPatrolPath( UGCComponent* component )
{
	UGCComponent* editComponent;
	UGCComponentPatrolPath* path = NULL;

	assert( component->eType == UGC_COMPONENT_TYPE_KILL );
	editComponent = ugcPlayingEditorGetEditComponent();
	if( !editComponent ) {
		return;
	}
	switch( editComponent->eType ) {
		xcase UGC_COMPONENT_TYPE_ACTOR: {
			if( eaiFind( &component->uChildIDs, editComponent->uID ) < 0 ) {
				return;
			}
		}
		xcase UGC_COMPONENT_TYPE_PATROL_POINT: {
			if( eaiFind( &component->eaPatrolPoints, editComponent->uID ) < 0 ) {
				return;
			}
		}
		xdefault:
			return;
	}

	// get the patrol path
	{
		UGCComponentPatrolPoint** eaOverrides = NULL;
		UGCComponentPatrolPoint* override = StructCreate( parse_UGCComponentPatrolPoint );
		override->componentID = editComponent->uID;
		copyVec3( g_UGCPlayingEditorDoc.editMat4[ 3 ], override->pos );
		eaPush( &eaOverrides, override );
		path = ugcComponentGetPatrolPath( ugcEditorGetProjectData(), component, eaOverrides );
		eaDestroyStruct( &eaOverrides, parse_UGCComponentPatrolPoint );
	}

	{
		Color4 errorTint = { 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF };
		int it;
		int numPoints = eaSize( &path->points );
		for( it = 0; it != numPoints; ++it ) {
			Vec3 pos1, pos2;
			bool lineError = false;
			U32 lineColor = false;

			if( path->patrolType == PATROL_ONEWAY && it == 0 ) {
				continue;
			}

			if( it == 0 ) {
				copyVec3( path->points[ numPoints - 1 ]->pos, pos1 );
				copyVec3( path->points[ it ]->pos, pos2 );
			} else {
				copyVec3( path->points[ it - 1 ]->pos, pos1 );
				copyVec3( path->points[ it ]->pos, pos2 );
			}
			if( path->points[ it ]->prevConnectionInvalid ) {
				lineError = true;
			}

			lineColor = (lineError ? 0xFF0000FF : 0xFFFFFFFF);
			gfxDrawLine3DWidthARGB( pos1, pos2, lineColor, lineColor, 2 );
		}
	}

	StructDestroy( parse_UGCComponentPatrolPath, path );
}

AUTO_EXPR_FUNC(UIGen) ACMD_NAME(ugc_RespawnAtFullHealth);
void UGCPlayingEditorRespawnAtFullHealth( void )
{
	ServerCmd_gslUGC_RespawnAtFullHealth();
}


#include "NNOUGCPlayingEditor_c_ast.c"
